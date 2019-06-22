// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>

#include <../../Cry3DEngine/StdAfx.h>

#include <ParticleSystem/ParticleSystem.h>
#include <ParticleSystem/ParticleContainer.h>


using namespace pfx2;

#define arraysize(array) (sizeof(array) / sizeof(array[0]))

void ParticleContainerRemoveTest(size_t containerSz, TParticleId* toRemove, size_t toRemoveSz)
{
	// create particles with SpawnId
	TParticleHeap heap;
	pfx2::CParticleContainer container;
	PUseData useData = NewUseData();
	useData->AddData(EPDT_SpawnId);
	container.SetUsedData(useData, EDD_Particle);
	container.BeginSpawn();
	container.AddElements(containerSz);
	container.EndSpawn();
	IOPidStream spawnIds = container.IOStream(EPDT_SpawnId);
	for (uint i = 0; i < containerSz; ++i)
	{
		spawnIds.Store(i, i);
	}

	// remove particles
	TParticleIdArray toRemoveMem(heap);
	toRemoveMem.reserve(toRemoveSz);
	for (size_t i = 0; i < toRemoveSz; ++i)
	{
		toRemoveMem.push_back(toRemove[i]);
	}
	TParticleIdArray swapIds(heap, container.Size());
	container.MakeSwapIds(toRemoveMem, swapIds);
	container.RemoveElements(toRemoveMem);

	// check if none of the particles are in the toRemove list
	for (uint i = 0; i < containerSz - toRemoveSz; ++i)
	{
		for (uint j = 0; j < toRemoveSz; ++j)
		{
			REQUIRE(spawnIds.Load(i) != toRemove[j]);
		}
	}

	// check that no ids are repeated
	for (uint i = 0; i < containerSz - toRemoveSz; ++i)
	{
		for (uint j = 0; j < containerSz - toRemoveSz; ++j)
		{
			if (i == j)
			{
				continue;
			}
			REQUIRE(spawnIds.Load(i) != spawnIds.Load(j));
		}
	}

	// check if SwapIds is consistent with the actual final locations of the particles
	TParticleIdArray expected(heap, containerSz);
	for (uint i = 0; i < containerSz - toRemoveSz; ++i)
	{
		expected[spawnIds.Load(i)] = i;
	}
	for (uint i = 0; i < toRemoveSz; ++i)
	{
		expected[toRemove[i]] = gInvalidId;
	}
	REQUIRE(memcmp(swapIds.data(), expected.data(), sizeof(TParticleId) * containerSz) == 0);
}

TEST(CParticleContainerTest, Remove)
{
	gEnv->startProfilingSection = [](SProfilingSection*) { return false; };
	gEnv->recordProfilingMarker = [](SProfilingMarker*) {};

	{
		TParticleId toRemove[] = { 3, 4 };
		ParticleContainerRemoveTest(5, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 8, 12 };
		ParticleContainerRemoveTest(13, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 0, 4 };
		ParticleContainerRemoveTest(5, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 4, 7, 9 };
		ParticleContainerRemoveTest(10, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 0, 1, 2, 4, 5, 6 };
		ParticleContainerRemoveTest(7, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 0 };
		ParticleContainerRemoveTest(1, toRemove, arraysize(toRemove));
	}

	{
		TParticleId toRemove[] = { 9, 11 };
		ParticleContainerRemoveTest(21, toRemove, arraysize(toRemove));
	}
}

#if CRY_PFX2_GROUP_STRIDE == 4

class CParticleContainerSpawnTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		static_assert(CRY_PFX2_GROUP_STRIDE == 4, "This unit test is assuming vectorization of 4 particles");
		pContainer = std::unique_ptr<pfx2::CParticleContainer>(new pfx2::CParticleContainer());
		useData = NewUseData();
		useData->AddData(EPDT_ParentId);
		pContainer->SetUsedData(useData, EDD_Particle);
	}

	virtual void TearDown() override
	{
		pContainer.reset();
		useData.reset();
	};

	void AddParticles(uint32 count)
	{
		ResetSpawnedParticles();
		pContainer->AddElements(count);
	}

	void ResetSpawnedParticles()
	{
		pContainer->EndSpawn();
	}

	const pfx2::CParticleContainer& GetContainer() const
	{
		return *pContainer;
	}

	void ResetAll()
	{
		pContainer->Clear();
	}

private:
	PUseData useData;
	std::unique_ptr<pfx2::CParticleContainer> pContainer;
};

TEST_F(CParticleContainerSpawnTest, VectorizeSpawn)
{
	// PFX2_TODO : right now new born vectorization is a bit too simple. Should be profiled and tested later
	// particle memory should be segmented.

	const pfx2::CParticleContainer& container = GetContainer();
	AddParticles(6);

	AddParticles(3);
	REQUIRE(container.RealSize() == 9);
	REQUIRE(container.SpawnedRange().m_begin == 8);
	REQUIRE(container.NumSpawned() == 3);
	REQUIRE(container.Capacity() > 11);

	ResetSpawnedParticles();
	REQUIRE(container.Size() == 9);
	REQUIRE(container.SpawnedRange().m_begin == 6);
	REQUIRE(container.NumSpawned() == 3);
	REQUIRE(container.Capacity() > 9);
}

TEST_F(CParticleContainerSpawnTest, RealId)
{
	const pfx2::CParticleContainer& container = GetContainer();

	AddParticles(1);
	REQUIRE(container.RealId(0) == 0);
	ResetSpawnedParticles();

	ResetAll();
	AddParticles(6);
	AddParticles(3);
	REQUIRE(container.RealId(3) == 3);
	REQUIRE(container.RealId(8) == 8);
	REQUIRE(container.RealId(10) == 7);
	ResetSpawnedParticles();
	REQUIRE(container.RealId(3) == 3);
	REQUIRE(container.RealId(8) == 8);
	REQUIRE(container.RealId(6) == 6);
}

class ParticleAttributesTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		gEnv->pNameTable = &m_nameTable;
	}

	CNameTable m_nameTable;
};

TEST_F(ParticleAttributesTest, All)
{
	auto pTable = std::make_shared<CAttributeTable>();
	CAttributeTable& table = *pTable.get();

	SAttributeDesc attr;
	attr.m_name = "Float";
	attr.m_defaultValue = -3.0f;
	attr.m_maxFloat = 1.0f;
	table.AddAttribute(attr);

	attr.m_name = "Bool";
	attr.m_defaultValue = false;
	table.AddAttribute(attr);

	attr.m_name = "Color";
	attr.m_defaultValue = ColorB(50, 100, 150, 255);
	table.AddAttribute(attr);

	REQUIRE(table.GetAttribute(1).GetType() == IParticleAttributes::ET_Boolean);

	CAttributeInstance inst;
	inst.Reset(pTable);

	REQUIRE(inst.GetNumAttributes() == 3);
	uint id = inst.FindAttributeIdByName("Color");
	REQUIRE(inst.GetAttributeType(id) == IParticleAttributes::ET_Color);
	REQUIRE(inst.GetAsBoolean(id, false) == true);
	REQUIRE(inst.GetAsInteger(id, 0) == 91);
	inst.SetAsFloat(id, 0.5f);
	REQUIRE(IsEquivalent(inst.GetAsFloat(id), 0.5f, 0.002f));

	id = inst.FindAttributeIdByName("Float");
	REQUIRE(inst.GetAsInteger(id, 0) == -3);

	inst.SetAsInteger(id, 2);
	REQUIRE(inst.GetAsFloat(id, 0.0f) == 1.0f);
	inst.ResetValue(id);
	REQUIRE(inst.GetAsFloat(id, 0.0f) == -3.0f);

	IParticleAttributes::TValue value;
	value.SetType(IParticleAttributes::ET_Color);
	REQUIRE(value.Type() == IParticleAttributes::ET_Color);

	auto type = table.GetAttribute(0).GetType();
	DO_FOR_ATTRIBUTE_TYPE(type, T, value = T());
};

#endif // #if CRY_PFX2_GROUP_STRIDE == 4

#ifdef CRY_PFX2_USE_SSE

TEST(ParticleSSE, IndexedLoad4)
{
	using namespace pfx2::detail;
	CRY_ALIGN(16) float stream[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
	uint32v index;
	floatv expected, got;

	index = _mm_set1_epi32(2);
	expected = _mm_set_ps(2.0f, 2.0f, 2.0f, 2.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));

	index = _mm_set_epi32(7, 6, 5, 4);
	expected = _mm_set_ps(7.0f, 6.0f, 5.0f, 4.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));

	index = _mm_set_epi32(0, 1, 2, 0);
	expected = _mm_set_ps(0.0f, 1.0f, 2.0f, 0.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));

	index = _mm_set_epi32(0, -1, 2, -1);
	expected = _mm_set_ps(0.0f, -1.0f, 2.0f, -1.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));

	index = _mm_set_epi32(-1, -1, -1, -1);
	expected = _mm_set_ps(-1.0f, -1.0f, -1.0f, -1.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));

	index = _mm_set_epi32(9, 9, 2, 2);
	expected = _mm_set_ps(9.0f, 9.0f, 2.0f, 2.0f);
	got = LoadIndexed4(stream, index, -1.0f);
	REQUIRE(All(got == expected));
}


TEST(CryVectorTest, ChaosKeyTest)
{
	CRndGen randomGenerator;
	randomGenerator.SetState(time(0));

	for (int rep = 0; rep < 8; ++rep)
	{
		uint32 key = randomGenerator.GetRandom(0U, 99999U);
		SChaosKey ch(key);
		SChaosKeyV chv(convert<uint32v>(key));

		uint32 r = ch.Rand();
		uint32v rv = chv.Rand();
		REQUIRE(All(convert<uint32v>(r) == rv));

		float ss = ch.RandSNorm();
		floatv ssv = chv.RandSNorm();
		REQUIRE(All(ToFloatv(ss) == ssv));
	}
}

#endif // #ifdef CRY_PFX2_USE_SSE
