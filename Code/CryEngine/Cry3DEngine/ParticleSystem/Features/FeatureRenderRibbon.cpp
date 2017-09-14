// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     04/05/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem/ParticleRender.h"
#include "ParticleSystem/ParticleEmitter.h"
#include <CryMath/RadixSort.h>

CRY_PFX2_DBG

namespace pfx2
{


EParticleDataType PDT(EPDT_RibbonId, TParticleId);

extern EParticleDataType EPDT_Alpha, EPDT_Color;


SERIALIZATION_DECLARE_ENUM(ERibbonMode,
                           Camera,
                           Free
                           )

SERIALIZATION_DECLARE_ENUM(ERibbonStreamSource,
                           Age,
                           Spawn
                           )


EParticleDataType DataType(ERibbonStreamSource source)
{
	return source == ERibbonStreamSource::Spawn ? EPDT_SpawnFraction : EPDT_NormalAge;
}

class CFeatureRenderRibbon : public CParticleRenderBase
{
private:
	typedef SParticleAxes (CFeatureRenderRibbon::* AxesFn)(CParticleComponentRuntime* pComponentRuntime, TParticleId particleId, const CCamera& camera, Vec3 movingPositions[3]);

	struct SRibbon
	{
		uint m_firstIdx;
		uint m_lastIdx;
	};

public:
	typedef CParticleRenderBase           BaseClass;
	typedef TParticleHeap::Array<SRibbon> TRibbons;

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderRibbon();

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void ComputeVertices(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;

private:
	void             MakeRibbons(CParticleComponentRuntime* pComponentRuntime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices);
	void             WriteToGPUMem(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, const TParticleIdArray& pSortEntries, const TRibbons& pRibbons, uint numVertices);
	template<typename TAxesSampler>
	void             WriteToGPUMem(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, const TParticleIdArray& pSortEntries, const TRibbons& pRibbons, const TAxesSampler& axesSampler, uint numVertices);

	SParticleColorST VertexColorST(CParticleComponentRuntime* pComponentRuntime, TParticleId particleId, uint frameId, float animPos);

	SFloat              m_sortBias;
	ERibbonMode         m_ribbonMode;
	ERibbonStreamSource m_streamSource;
	UFloat10            m_frequency;
	UFloat10            m_offset;
	bool                m_connectToOrigin;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderRibbon, "Render", "Ribbon", colorRender);

CFeatureRenderRibbon::CFeatureRenderRibbon()
	: m_ribbonMode(ERibbonMode::Camera)
	, m_streamSource(ERibbonStreamSource::Age)
	, m_connectToOrigin(false)
	, m_frequency(1.0f)
	, m_sortBias(0.0f)
	, m_offset(0.0f)
{
}

void CFeatureRenderRibbon::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	BaseClass::AddToComponent(pComponent, pParams);
	CParticleComponent* pParent = pComponent->GetParentComponent();
	pComponent->AddToUpdateList(EUL_InitUpdate, this);
	pComponent->AddParticleData(EPDT_ParentId);
	pComponent->AddParticleData(EPDT_SpawnId);
	if (pParent)
		pParent->AddParticleData(EPDT_SpawnId);
	pComponent->AddParticleData(EPDT_RibbonId);
	pComponent->AddParticleData(EPDT_Size);
	pComponent->AddParticleData(EPDT_NormalAge);
	pParams->m_shaderData.m_expansion[1] = 0.0f;
	pParams->m_particleObjFlags |= CREParticle::ePOF_USE_VERTEX_PULL_MODEL;
	pParams->m_renderObjectSortBias = m_sortBias;
	if (m_ribbonMode == ERibbonMode::Free)
		pComponent->AddParticleData(EPQF_Orientation);
	pComponent->AddParticleData(DataType(m_streamSource));
	pParams->m_shaderData.m_textureFrequency = m_frequency;
}

void CFeatureRenderRibbon::Serialize(Serialization::IArchive& ar)
{
	BaseClass::Serialize(ar);
	ar(m_ribbonMode, "RibbonMode", "Ribbon Mode");
	ar(m_streamSource, "StreamSource", "Stream Source");
	ar(m_sortBias, "SortBias", "Sort Bias");
	ar(m_connectToOrigin, "ConnectToOrigin", "Connect To Origin");
	ar(m_frequency, "TextureFrequency", "Texture Frequency");
	ar(m_offset, "Offset", "Offset");
}

void CFeatureRenderRibbon::InitParticles(const SUpdateContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	CParticleContainer& container = context.m_container;
	CParticleContainer& parentContainer = context.m_parentContainer;
	IOUintStream ribbonIds = container.GetIOUintStream(EPDT_RibbonId);
	IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
	IUintStream parentSpawnIds = parentContainer.GetIUintStream(EPDT_SpawnId, 0);

	for (auto particleId : context.GetSpawnedRange())
	{
		const TParticleId parentId = parentIds.Load(particleId);
		const uint32 parentSpawnId = parentSpawnIds.SafeLoad(parentId);
		ribbonIds.Store(particleId, parentSpawnId);
	}
}

void CFeatureRenderRibbon::ComputeVertices(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	const uint32 threadId = JobManager::GetWorkerThreadId();
	auto& memHep = GetPSystem()->GetMemHeap(threadId);
	const CParticleContainer& container = pComponentRuntime->GetContainer();
	const TParticleId lastParticleId = container.GetLastParticleId();

	if (lastParticleId == 0)
		return;

	uint numVertices;
	TParticleIdArray sortEntries(memHep, lastParticleId);
	TRibbons ribbons(memHep);
	MakeRibbons(pComponentRuntime, &sortEntries, &ribbons, &numVertices);
	WriteToGPUMem(pComponentRuntime, camInfo, pRE, sortEntries, ribbons, numVertices);

	int totalRenderedParticles = 0;
	for (uint ribbonId = 0; ribbonId < uint(ribbons.size()); ++ribbonId)
		totalRenderedParticles += ribbons[ribbonId].m_lastIdx - ribbons[ribbonId].m_firstIdx;
	pComponentRuntime->GetEmitter()->AddDrawCallCounts(totalRenderedParticles, 0);
	GetPSystem()->GetProfiler().AddEntry(pComponentRuntime, EPS_RendereredParticles, uint(totalRenderedParticles));
}

void CFeatureRenderRibbon::MakeRibbons(CParticleComponentRuntime* pComponentRuntime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices)
{
	CRY_PFX2_PROFILE_DETAIL;

	const uint32 threadId = JobManager::GetWorkerThreadId();
	auto& memHep = GetPSystem()->GetMemHeap(threadId);
	const SUpdateContext context = SUpdateContext(pComponentRuntime);
	const CParticleContainer& container = pComponentRuntime->GetContainer();
	const IUintStream ribbonIds = container.GetIUintStream(EPDT_RibbonId);
	const IUintStream spawnIds = container.GetIUintStream(EPDT_SpawnId);
	const TIStream<uint8> states = container.GetTIStream<uint8>(EPDT_State);
	const TParticleId lastParticleId = container.GetLastParticleId();
	uint numValidParticles = 0;

	{
		const uint64 noKey = uint64(-1);
		THeapArray<uint64> sortEntries(memHep, lastParticleId);
		for (auto particleId : context.GetUpdateRange())
		{
			const TParticleId ribbonId = ribbonIds.Load(particleId);
			const uint32 spawnId = spawnIds.Load(particleId);
			const uint64 key = (uint64(ribbonId) << 32) | uint64(spawnId);
			const uint8 state = states.Load(particleId);
			const bool valid = (state & ESB_Dead) == 0;
			numValidParticles += uint(valid);
			sortEntries[particleId] = valid ? key : noKey;
		}
		RadixSort(
		  pSortEntries->begin(), pSortEntries->end(),
		  sortEntries.begin(), sortEntries.end(), memHep);
	}

	uint ribbonCount = 0;
	uint lastRibbonId = ribbonIds.Load((*pSortEntries)[0]);
	SRibbon ribbon;
	ribbon.m_firstIdx = 0;
	ribbon.m_lastIdx = 0;
	for (; ribbon.m_lastIdx < numValidParticles; ++ribbon.m_lastIdx)
	{
		const TParticleId particleId = (*pSortEntries)[ribbon.m_lastIdx];
		const TParticleId ribbonId = ribbonIds.Load(particleId);
		if (ribbonId != lastRibbonId)
		{
			if ((ribbon.m_lastIdx - ribbon.m_firstIdx) >= 2)
				++ribbonCount;
			ribbon.m_firstIdx = ribbon.m_lastIdx;
		}
		lastRibbonId = ribbonId;
	}
	if (ribbon.m_lastIdx - ribbon.m_firstIdx >= 2)
		++ribbonCount;
	pRibbons->reserve(ribbonCount);
	ribbon.m_firstIdx = 0;
	ribbon.m_lastIdx = 0;
	for (; ribbon.m_lastIdx < numValidParticles; ++ribbon.m_lastIdx)
	{
		const TParticleId particleId = (*pSortEntries)[ribbon.m_lastIdx];
		const TParticleId ribbonId = ribbonIds.Load(particleId);
		if (ribbonId != lastRibbonId)
		{
			if ((ribbon.m_lastIdx - ribbon.m_firstIdx) >= 2)
				pRibbons->push_back(ribbon);
			ribbon.m_firstIdx = ribbon.m_lastIdx;
		}
		lastRibbonId = ribbonId;
	}
	if (ribbon.m_lastIdx - ribbon.m_firstIdx >= 2)
		pRibbons->push_back(ribbon);

	*pNumVertices = 0;
	for (uint ribbonId = 0; ribbonId < uint(pRibbons->size()); ++ribbonId)
	{
		const SRibbon ribbon = (*pRibbons)[ribbonId];
		const uint numSegments = ribbon.m_lastIdx - ribbon.m_firstIdx;
		*pNumVertices += numSegments + 1;
		if (m_connectToOrigin)
			++*pNumVertices;
	}
}

class CRibbonAxesCamera
{
public:
	CRibbonAxesCamera(const CParticleContainer& container, const CCamera& camera)
		: m_sizes(container.GetIFStream(EPDT_Size))
		, m_cameraPosition(camera.GetPosition()) {}

	ILINE SParticleAxes Sample(TParticleId particleId, Vec3 movingPositions[3]) const
	{
		SParticleAxes axes;
		const float size = m_sizes.Load(particleId);
		const Vec3 front = movingPositions[1] - m_cameraPosition;
		const Vec3 dir0 = movingPositions[0] - movingPositions[1];
		const Vec3 dir1 = movingPositions[1] - movingPositions[2];
		const Vec3 up0 = dir0.Cross(front);
		const Vec3 up1 = dir1.Cross(front);
		const Vec3 up = (up0 + up1).GetNormalized();
		const Vec3 right = up.Cross(front).GetNormalized();

		axes.xAxis = up * size;
		axes.yAxis = right * size;

		return axes;
	}

private:
	const IFStream m_sizes;
	Vec3           m_cameraPosition;
};

class CRibbonAxesFree
{
public:
	CRibbonAxesFree(const CParticleContainer& container)
		: m_orientations(container.GetIQuatStream(EPQF_Orientation))
		, m_sizes(container.GetIFStream(EPDT_Size)) {}

	ILINE SParticleAxes Sample(TParticleId particleId, Vec3 movingPositions[3]) const
	{
		SParticleAxes axes;
		const float size = m_sizes.Load(particleId);
		const Quat orientation = m_orientations.Load(particleId);
		const Vec3 dir0 = movingPositions[0] - movingPositions[1];
		const Vec3 dir1 = movingPositions[1] - movingPositions[2];
		const Vec3 up = -orientation.GetColumn0();
		const Vec3 right = (dir0 + dir1).GetNormalized();

		axes.xAxis = up * size;
		axes.yAxis = right * size;

		return axes;
	}

private:
	const IQuatStream m_orientations;
	const IFStream    m_sizes;
};

class CRibbonColorSTs
{
public:
	CRibbonColorSTs(const CParticleContainer& container, ERibbonStreamSource streamSource)
		: m_colors(container.GetIColorStream(EPDT_Color))
		, m_stream(container.GetIFStream(DataType(streamSource)))
		, m_sizes(container.GetIFStream(EPDT_Size))
		, m_alphas(container.GetIFStream(EPDT_Alpha, 1.0f))
		, m_hasColors(container.HasData(EPDT_Color)) {}

	ILINE SParticleColorST Sample(TParticleId particleId, uint frameId, float animPos) const
	{
		SParticleColorST colorST;
		const float alpha = m_alphas.SafeLoad(particleId);
		const float stream = m_stream.Load(particleId);

		colorST.color.dcolor = ~0;
		if (m_hasColors)
			colorST.color = m_colors.Load(particleId);
		colorST.color.a = FloatToUFrac8Saturate(alpha);

		colorST.st.dcolor = 0;
		colorST.st.x = 0;
		colorST.st.y = FloatToUFrac8Saturate(stream);
		colorST.st.z = frameId;
		colorST.st.w = FloatToUFrac8Saturate(animPos - int(animPos));

		return colorST;
	}

private:
	const IColorStream m_colors;
	const IFStream     m_sizes;
	const IFStream     m_stream;
	const IFStream     m_alphas;
	const bool         m_hasColors;
};

void CFeatureRenderRibbon::WriteToGPUMem(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, const TParticleIdArray& sortEntries, const TRibbons& ribbons, uint numVertices)
{
	CRY_PFX2_PROFILE_DETAIL;

	const CParticleContainer& container = pComponentRuntime->GetContainer();
	const CCamera& camera = *camInfo.pCamera;

	if (m_ribbonMode == ERibbonMode::Camera)
		WriteToGPUMem<CRibbonAxesCamera>(pComponentRuntime, camInfo, pRE, sortEntries, ribbons, CRibbonAxesCamera(container, camera), numVertices);
	else
		WriteToGPUMem<CRibbonAxesFree>(pComponentRuntime, camInfo, pRE, sortEntries, ribbons, CRibbonAxesFree(container), numVertices);
}

template<typename TAxesSampler>
ILINE void CFeatureRenderRibbon::WriteToGPUMem(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, const TParticleIdArray& sortEntries, const TRibbons& ribbons, const TAxesSampler& axesSampler, uint numVertices)
{
	const SComponentParams& params = pComponentRuntime->GetComponentParams();
	const CParticleContainer& container = pComponentRuntime->GetContainer();
	const CParticleContainer& parentContainer = pComponentRuntime->GetParentContainer();

	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
	const IFStream parentAges = parentContainer.GetIFStream(EPDT_NormalAge);
	const uint extraVertices = m_connectToOrigin ? 2 : 1;
	const CRibbonColorSTs colorSTsSampler = CRibbonColorSTs(container, m_streamSource);
	const bool hasOffset = m_offset != 0.0f;

	SRenderVertices* pRenderVertices = pRE->AllocPullVertices(numVertices);
	CWriteCombinedBuffer<Vec3, vertexBufferSize, vertexChunckSize> localPositions(pRenderVertices->aPositions);
	CWriteCombinedBuffer<SParticleAxes, vertexBufferSize, vertexChunckSize> localAxes(pRenderVertices->aAxes);
	CWriteCombinedBuffer<SParticleColorST, vertexBufferSize, vertexChunckSize> localColorSTs(pRenderVertices->aColorSTs);

	for (uint ribbonId = 0; ribbonId < uint(ribbons.size()); ++ribbonId)
	{
		const SRibbon ribbon = ribbons[ribbonId];
		const uint numSegments = ribbon.m_lastIdx - ribbon.m_firstIdx;
		const uint numVertices = numSegments + extraVertices;
		if (!localPositions.CheckAvailable(numVertices) || !localAxes.CheckAvailable(numVertices) || !localColorSTs.CheckAvailable(numVertices))
			break;

		Vec3 movingPositions[3] =
		{
			positions.Load(sortEntries[ribbon.m_firstIdx]),
			movingPositions[0],
			positions.Load(sortEntries[ribbon.m_firstIdx + 1])
		};

		const TParticleId ribbonParentId = parentIds.Load(sortEntries[ribbon.m_firstIdx]);
		const float parentAge = ribbonParentId != gInvalidId ? parentAges.Load(ribbonParentId) : 1.0f;
		const float animPos = parentAge * float(params.m_shaderData.m_frameCount);
		const uint frameId = int(min(animPos, params.m_shaderData.m_frameCount - 1));

		Vec3 position;
		SParticleAxes axes;
		SParticleColorST colorST;

		for (uint i = ribbon.m_firstIdx; i < ribbon.m_lastIdx; ++i)
		{
			const TParticleId particleId = sortEntries[i];

			position = movingPositions[1];

			axes = axesSampler.Sample(particleId, movingPositions);
			colorST = colorSTsSampler.Sample(particleId, frameId, animPos);

			if (hasOffset)
			{
				const Vec3 zAxis = axes.xAxis.Cross(axes.yAxis).GetNormalized();
				position -= zAxis * m_offset;
			}

			localPositions.Array().push_back(position);
			localAxes.Array().push_back(axes);
			localColorSTs.Array().push_back(colorST);

			movingPositions[0] = movingPositions[1];
			movingPositions[1] = movingPositions[2];
			if (i + 2 < ribbon.m_lastIdx)
				movingPositions[2] = positions.Load(sortEntries[i + 2]);
		}

		if (m_connectToOrigin)
		{
			if (ribbonParentId != gInvalidId)
				position = parentPositions.Load(ribbonParentId);
			colorST.st.y = 0;
			colorST.st.w = 0;
			localPositions.Array().push_back(position);
			localAxes.Array().push_back(axes);
			localColorSTs.Array().push_back(colorST);
		}

		colorST.st.x = 255;
		localPositions.Array().push_back(position);
		localAxes.Array().push_back(axes);
		localColorSTs.Array().push_back(colorST);
	}
}

}
