// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     06/04/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySystem/CryUnitTest.h>
#include "ParticleSystem.h"
#include "ParticleContainer.h"
#include "../ParticleEffect.h"
#include "ParticleEffect.h"
#include <CryMath/SNoise.h>
#include <CryMath/RadixSort.h>

#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/Color.h>

CRY_UNIT_TEST_SUITE(CryParticleSystemTest)
{

	using namespace pfx2;

#define arraysize(array) (sizeof(array) / sizeof(array[0]))

#define CRY_PFX2_UNIT_TEST_ASSERT(cond) \
  CRY_UNIT_TEST_ASSERT(cond)

	CRY_UNIT_TEST_FIXTURE(CParticleContainerRemoveTests)
	{
		void Test(size_t containerSz, TParticleId* toRemove, size_t toRemoveSz)
		{
			// create particles with SpawnId
			TParticleHeap heap;
			pfx2::CParticleContainer container;
			container.AddParticleData(EPDT_SpawnId);
			pfx2::SSpawnEntry spawn;
			spawn.m_count = containerSz;
			spawn.m_parentId = 0;
			container.AddParticles({&spawn, 1});
			container.ResetSpawnedParticles();
			IOPidStream spawnIds = container.GetIOPidStream(EPDT_SpawnId);
			for (uint i = 0; i < containerSz; ++i)
				spawnIds.Store(i, i);

			// remove particles
			TParticleIdArray toRemoveMem(heap);
			toRemoveMem.reserve(toRemoveSz);
			for (size_t i = 0; i < toRemoveSz; ++i)
				toRemoveMem.push_back(toRemove[i]);
			TParticleIdArray swapIds(heap, container.GetNumParticles());
			container.RemoveParticles(toRemoveMem, swapIds);

			// check if none of the particles are in the toRemove list
			for (uint i = 0; i < containerSz - toRemoveSz; ++i)
			{
				for (uint j = 0; j < toRemoveSz; ++j)
				{
					CRY_PFX2_UNIT_TEST_ASSERT(spawnIds.Load(i) != toRemove[j]);
				}
			}

			// check that no ids are repeated
			for (uint i = 0; i < containerSz - toRemoveSz; ++i)
			{
				for (uint j = 0; j < containerSz - toRemoveSz; ++j)
				{
					if (i == j)
						continue;
					CRY_PFX2_UNIT_TEST_ASSERT(spawnIds.Load(i) != spawnIds.Load(j));
				}
			}

			// check if SwapIds is consistent with the actual final locations of the particles
			TParticleIdArray expected(heap, containerSz);
			for (uint i = 0; i < containerSz - toRemoveSz; ++i)
				expected[spawnIds.Load(i)] = i;
			for (uint i = 0; i < toRemoveSz; ++i)
				expected[toRemove[i]] = gInvalidId;
			CRY_PFX2_UNIT_TEST_ASSERT(memcmp(swapIds.data(), expected.data(), sizeof(TParticleId) * containerSz) == 0);
		}
	};

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case1, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 3, 4 };
		Test(5, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case2, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 8, 12 };
		Test(13, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case3, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 0, 4 };
		Test(5, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case4, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 4, 7, 9 };
		Test(10, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case5, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 0, 1, 2, 4, 5, 6 };
		Test(7, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case6, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 0 };
		Test(1, toRemove, arraysize(toRemove));
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_Remove_Case7, CParticleContainerRemoveTests)
	{
		TParticleId toRemove[] = { 9, 11 };
		Test(21, toRemove, arraysize(toRemove));
	}

#if CRY_PFX2_PARTICLESGROUP_STRIDE == 4

	CRY_UNIT_TEST_FIXTURE(CParticleContainerSpawn)
	{
	public:
		virtual void Init()
		{
			static_assert(CRY_PFX2_PARTICLESGROUP_STRIDE == 4, "This unit test is assuming vectorization of 4 particles");
			pContainer = std::unique_ptr<pfx2::CParticleContainer>(new pfx2::CParticleContainer());
			pContainer->AddParticleData(EPDT_ParentId);
		};

		virtual void Done()
		{
			pContainer.reset();
		};

		void AddParticles(uint32 count)
		{
			ResetSpawnedParticles();
			pfx2::SSpawnEntry spawn;
			spawn.m_count = count;
			spawn.m_parentId = 0;
			pContainer->AddParticles({&spawn, 1});
		}

		void ResetSpawnedParticles()
		{
			pContainer->ResetSpawnedParticles();
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
		std::unique_ptr<pfx2::CParticleContainer> pContainer;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_VectorizeSpawn, CParticleContainerSpawn)
	{
		// PFX2_TODO : right now new born vectorization is a bit too simple. Should be profiled and tested later
		// particle memory should be segmented.

		const pfx2::CParticleContainer& container = GetContainer();
		AddParticles(6);

		AddParticles(3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealNumParticles() == 9);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetSpawnedRange().m_begin == 8);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetNumSpawnedParticles() == 3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetMaxParticles() > 11);

		ResetSpawnedParticles();
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetNumParticles() == 9);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetSpawnedRange().m_begin == 6);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetNumSpawnedParticles() == 3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetMaxParticles() > 9);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleContainer_RealId, CParticleContainerSpawn)
	{
		const pfx2::CParticleContainer& container = GetContainer();

		AddParticles(1);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(0) == 0);
		ResetSpawnedParticles();

		ResetAll();
		AddParticles(6);
		AddParticles(3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(3) == 3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(8) == 8);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(10) == 7);
		ResetSpawnedParticles();
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(3) == 3);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(8) == 8);
		CRY_PFX2_UNIT_TEST_ASSERT(container.GetRealId(6) == 6);
	}

	CRY_UNIT_TEST_FIXTURE(CParticleEffectTests)
	{
	public:
		virtual void Init() override
		{
			m_pEffect = std::unique_ptr<pfx2::CParticleEffect>(new pfx2::CParticleEffect());
		};

		virtual void Done() override
		{
			m_pEffect.reset();
		}

		pfx2::CParticleEffect& GetEffect()
		{
			return *m_pEffect;
		}

		void LoadSimpleEffect()
		{
			const char* jsonCode =
			  "{ \"Version\": 3,\n"
			  "	\"Components\": [\n"
			  "		{ \"Component\": { \"Name\": \"Test01\" }},\n"
			  "		{ \"Component\": { \"Name\": \"Test02\" }}]}\n";
			Serialization::LoadJsonBuffer(GetEffect(), jsonCode, strlen(jsonCode));
		}

		void LoadDefaultComponent(uint componentId)
		{
			const char* jsonCode =
			  "{ \"Name\": \"Default\", \"Features\": [\n"
			  "	{ \"RenderSprites\": { \"SortMode\": \"None\" } },\n"
			  "	{ \"SpawnRate\": {\n"
			  "		\"Amount\": { \"value\": 1.0, \"modifiers\": [ ] },\n"
			  "		\"Delay\": { \"State\": false, \"Value\": { \"value\": 0.0, \"modifiers\": [ ] } },\n"
			  "		\"Duration\": { \"State\": false, \"Value\": { \"value\": 0.0, \"modifiers\": [ ] } },\n"
			  "		\"Mode\": \"ParticlesPerSecond\"} },\n"
			  "	{ \"LifeTime\": { \"LifeTime\": { \"value\": 1, \"var\": 0 } } },\n"
			  "	{ \"FieldSize\": { \"value\": { \"value\": 1, \"modifiers\": [ ] } } } ] }\n";
			Serialization::LoadJsonBuffer(*GetEffect().GetComponent(componentId), jsonCode, strlen(jsonCode));
		}

	private:
		std::unique_ptr<pfx2::CParticleEffect> m_pEffect;
	};

	CRY_UNIT_TEST(ParticleAttributesTest)
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

		CRY_PFX2_UNIT_TEST_ASSERT(table.GetAttribute(1).GetType() == IParticleAttributes::ET_Boolean);

		CAttributeInstance inst;
		inst.Reset(pTable);

		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetNumAttributes() == 3);
		uint id = inst.FindAttributeIdByName("Color");
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAttributeType(id) == IParticleAttributes::ET_Color);
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAsBoolean(id, false) == true);
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAsInteger(id, 0) == 91);
		inst.SetAsFloat(id, 0.5f);
		CRY_PFX2_UNIT_TEST_ASSERT(IsEquivalent(inst.GetAsFloat(id), 0.5f, 0.002f));

		id = inst.FindAttributeIdByName("Float");
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAsInteger(id, 0) == -3);

		inst.SetAsInteger(id, 2);
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAsFloat(id, 0.0f) == 1.0f);
		inst.ResetValue(id);
		CRY_PFX2_UNIT_TEST_ASSERT(inst.GetAsFloat(id, 0.0f) == -3.0f);

		IParticleAttributes::TValue value;
		value.SetType(IParticleAttributes::ET_Color);
		CRY_PFX2_UNIT_TEST_ASSERT(value.Type() == IParticleAttributes::ET_Color);

		auto type = table.GetAttribute(0).GetType();
		DO_FOR_ATTRIBUTE_TYPE(type, T, value = T());
	};


	CRY_UNIT_TEST_WITH_FIXTURE(CParticleSystem_USimpleEffect, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		LoadSimpleEffect();
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Test01") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Test02") == 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleSystem_UniqueAutoName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		effect.AddComponent();
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Component01") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Component02") == 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleSystem_UniqueCustomName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		effect.GetComponent(0)->SetName("Test");
		effect.AddComponent();
		effect.GetComponent(1)->SetName("Test");
		effect.AddComponent();
		effect.GetComponent(2)->SetName("Test");
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Test") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Test1") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(2)->GetName(), "Test2") == 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(CParticleSystem_UniqueLoadedName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		LoadDefaultComponent(0);
		effect.AddComponent();
		LoadDefaultComponent(1);
		effect.AddComponent();
		LoadDefaultComponent(2);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Default") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Default1") == 0);
		CRY_PFX2_UNIT_TEST_ASSERT(strcmp(effect.GetComponent(2)->GetName(), "Default2") == 0);
	}

#endif

#ifdef CRY_PFX2_USE_SSE

	CRY_UNIT_TEST(ParticleSSE_IndexedLoad4)
	{
		using namespace pfx2::detail;
		CRY_ALIGN(16) float stream[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
		uint32v index;
		floatv expected, got;

		index = _mm_set1_epi32(2);
		expected = _mm_set_ps(2.0f, 2.0f, 2.0f, 2.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));

		index = _mm_set_epi32(7, 6, 5, 4);
		expected = _mm_set_ps(7.0f, 6.0f, 5.0f, 4.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));

		index = _mm_set_epi32(0, 1, 2, 0);
		expected = _mm_set_ps(0.0f, 1.0f, 2.0f, 0.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));

		index = _mm_set_epi32(0, -1, 2, -1);
		expected = _mm_set_ps(0.0f, -1.0f, 2.0f, -1.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));

		index = _mm_set_epi32(-1, -1, -1, -1);
		expected = _mm_set_ps(-1.0f, -1.0f, -1.0f, -1.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));

		index = _mm_set_epi32(9, 9, 2, 2);
		expected = _mm_set_ps(9.0f, 9.0f, 2.0f, 2.0f);
		got = LoadIndexed4(stream, index, -1.0f);
		CRY_PFX2_UNIT_TEST_ASSERT(All(got == expected));
	}

#endif

}

CRY_UNIT_TEST_SUITE(CryVectorTest)
{

#if defined(CRY_PFX2_USE_SSE)

	using namespace pfx2;

	template<typename Real>
	NO_INLINE Real SNoiseNoInline(Vec4_tpl<Real> v)
	{
		return SNoise(v);
	}

	template<typename Real>
	void SnoiseTest()
	{
		using namespace crydetail;
		const Vec4_tpl<Real> s0 = ToVec4(convert<Real>(0.0f));
		const Vec4_tpl<Real> s1 = Vec4_tpl<Real>(convert<Real>(104.2f), convert<Real>(504.85f), convert<Real>(32.0f), convert<Real>(10.25f));
		const Vec4_tpl<Real> s2 = Vec4_tpl<Real>(convert<Real>(-104.2f), convert<Real>(504.85f), convert<Real>(-32.0f), convert<Real>(1.25f));
		const Vec4_tpl<Real> s3 = Vec4_tpl<Real>(convert<Real>(-257.07f), convert<Real>(-25.85f), convert<Real>(-0.5f), convert<Real>(105.5f));
		const Vec4_tpl<Real> s4 = Vec4_tpl<Real>(convert<Real>(104.2f), convert<Real>(1504.85f), convert<Real>(78.57f), convert<Real>(-0.75f));
		Real p0 = SNoiseNoInline(s0);
		Real p1 = SNoiseNoInline(s1);
		Real p2 = SNoiseNoInline(s2);
		Real p3 = SNoiseNoInline(s3);
		Real p4 = SNoiseNoInline(s4);
		// #PFX2_TODO : Orbis has a slight difference in precision
		#if CRY_COMPILER_MSVC
		CRY_PFX2_UNIT_TEST_ASSERT(All(p0 == convert<Real>(0.0f)));
		CRY_PFX2_UNIT_TEST_ASSERT(All(p1 == convert<Real>(-0.291425288f)));
		CRY_PFX2_UNIT_TEST_ASSERT(All(p2 == convert<Real>(-0.295406163f)));
		CRY_PFX2_UNIT_TEST_ASSERT(All(p3 == convert<Real>(-0.127176195f)));
		CRY_PFX2_UNIT_TEST_ASSERT(All(p4 == convert<Real>(-0.0293087773f)));
		#endif
	}

	CRY_UNIT_TEST(SNoiseTest)
	{
		SnoiseTest<float>();
		SnoiseTest<floatv>();
	}

	CRY_UNIT_TEST(ChaosKeyTest)
	{
		for (int rep = 0; rep < 8; ++rep)
		{
			uint32 key = cry_random(0U, 99999U);
			SChaosKey ch(key);
			SChaosKeyV chv(ToUint32v(key));

			uint32 r = ch.Rand();
			uint32v rv = chv.Rand();
			CRY_PFX2_UNIT_TEST_ASSERT(All(ToUint32v(r) == rv));

			float ss = ch.RandSNorm();
			floatv ssv = chv.RandSNorm();
			CRY_PFX2_UNIT_TEST_ASSERT(All(ToFloatv(ss) == ssv));
		}
	}

	CRY_UNIT_TEST(RadixSortTest)
	{
		TParticleHeap heap;

		const uint64 data[] = { 15923216, 450445401561561, 5061954, 5491494, 56494109840, 500120, 520025710, 58974, 45842669, 3226, 995422665 };
		const uint sz = sizeof(data) / sizeof(data[0]);
		const uint32 expectedIndices[sz] = { 9, 7, 5, 2, 3, 0, 8, 6, 10, 4, 1 };
		uint32 indices[sz];

		RadixSort(indices, indices + sz, data, data + sz, heap);

		CRY_PFX2_UNIT_TEST_ASSERT(memcmp(expectedIndices, indices, sz * 4) == 0);
	}

	template<typename T>
	void SpecialTest(T z = 0)
	{
		typedef std::numeric_limits<T> lim;

		auto has_denorm = lim::has_denorm;
		auto denorm_loss = lim::has_denorm_loss;
		T infinity = lim::infinity();
		T max = lim::max();
		T epsilon = lim::epsilon();
		T pmin = lim::min();
		T dmin = lim::denorm_min();
		T lmin = lim::lowest();

		T dz2 = infinity + infinity;
		T dzsq = infinity * infinity;
		T dzm = infinity - T(1000);
		T zz = T(1) / infinity;

		T dd = dmin + dmin;
		T dnz = dmin - dmin;
		T dp = dmin * T(1000000);
	}

	CRY_UNIT_TEST(SpecialTests)
	{
		SpecialTest<float>();
		SpecialTest<double>();
	}

#endif

}
