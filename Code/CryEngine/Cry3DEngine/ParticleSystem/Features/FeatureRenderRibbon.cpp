// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     04/05/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "ParticleSystem/ParticleRender.h"
#include <CryMath/RadixSort.h>

namespace pfx2
{


MakeDataType(EPDT_RibbonId, TParticleId);

extern TDataType<float> EPDT_Alpha;
extern TDataType<UCol>  EPDT_Color;


SERIALIZATION_DECLARE_ENUM(ERibbonMode,
                           Camera,
                           Free
                           )

SERIALIZATION_DECLARE_ENUM(ERibbonStreamSource,
                           Age,
                           Spawn
                           )


TDataType<float> DataType(ERibbonStreamSource source)
{
	return source == ERibbonStreamSource::Spawn ? EPDT_SpawnFraction : EPDT_NormalAge;
}

class CFeatureRenderRibbon : public CParticleRenderBase
{
private:
	typedef SParticleAxes (CFeatureRenderRibbon::* AxesFn)(CParticleComponentRuntime& runtime, TParticleId particleId, const CCamera& camera, Vec3 movingPositions[3]);

	struct SRibbon
	{
		uint m_firstIdx;
		uint m_lastIdx;
	};

public:
	typedef CParticleRenderBase BaseClass;
	typedef THeapArray<SRibbon> TRibbons;

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderRibbon();

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(CParticleComponentRuntime& runtime) override;
	virtual void ComputeVertices(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;

private:
	void             MakeRibbons(const CParticleComponentRuntime& runtime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices);
	void             WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& camera, CREParticle* pRE, const TParticleIdArray& pSortEntries, const TRibbons& pRibbons, uint numVertices, float fMaxPixels);
	template<typename TAxesSampler>
	void             WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& camera, CREParticle* pRE, const TParticleIdArray& pSortEntries, const TRibbons& pRibbons, const TAxesSampler& axesSampler, uint numVertices, float fMaxPixels);

	void CullRibbonAreas(const CParticleComponentRuntime& runtime, const CCamera& camera, float fMaxPixels, TConstArray<uint> sortEntries, TConstArray<SRibbon> ribbons, TVarArray<float> ribbonAlphas, float& pixels, float &pixelsDrawn);

	SParticleColorST VertexColorST(const CParticleComponentRuntime& runtime, TParticleId particleId, uint frameId, float animPos);

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
	pComponent->InitParticles.add(this);
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

void CFeatureRenderRibbon::InitParticles(CParticleComponentRuntime& runtime)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	CParticleContainer& container = runtime.GetContainer();
	CParticleContainer& parentContainer = runtime.GetParentContainer();
	IOUintStream ribbonIds = container.IOStream(EPDT_RibbonId);
	IPidStream parentIds = container.IStream(EPDT_ParentId);
	IPidStream parentSpawnIds = parentContainer.IStream(EPDT_SpawnId);

	for (auto particleId : runtime.SpawnedRange())
	{
		const TParticleId parentId = parentIds.Load(particleId);
		const uint32 parentSpawnId = parentSpawnIds.SafeLoad(parentId);
		ribbonIds.Store(particleId, parentSpawnId);
	}
}

void CFeatureRenderRibbon::ComputeVertices(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)
{
	const CParticleContainer& container = runtime.GetContainer();
	const uint32 numParticles = container.GetNumParticles();

	if (numParticles == 0)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	uint numVertices;
	TParticleIdArray sortEntries(memHeap, numParticles);
	TRibbons ribbons(memHeap);
	MakeRibbons(runtime, &sortEntries, &ribbons, &numVertices);
	if (ribbons.size())
		WriteToGPUMem(runtime, *camInfo.pCamera, pRE, sortEntries, ribbons, numVertices, fMaxPixels);
}

void CFeatureRenderRibbon::MakeRibbons(const CParticleComponentRuntime& runtime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices)
{
	CRY_PFX2_PROFILE_DETAIL;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	const CParticleContainer& container = runtime.GetContainer();
	const IUintStream ribbonIds = container.IStream(EPDT_RibbonId);
	const IPidStream spawnIds = container.IStream(EPDT_SpawnId);
	const uint32 numParticles = container.GetNumParticles();
	uint numValidParticles = runtime.FullRange().size();

	{
		THeapArray<uint64> sortEntries(memHeap, numParticles);
		for (auto particleId : runtime.FullRange())
		{
			const TParticleId ribbonId = ribbonIds.Load(particleId);
			const uint32 spawnId = spawnIds.Load(particleId);
			const uint64 key = (uint64(ribbonId) << 32) | uint64(spawnId);
			sortEntries[particleId] = key;
		}
		RadixSort(
		  pSortEntries->begin(), pSortEntries->end(),
		  sortEntries.begin(), sortEntries.end(), memHeap);
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

inline void NormalizeSafe(Vec3& v, const Vec3& safe, float size)
{
	float lensqr = v.len2();
	if (lensqr > FLT_EPSILON)
		v *= rsqrt(lensqr) * size;
	else
		v = safe * size;
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
		axes.xAxis = (movingPositions[0] - movingPositions[2]) ^ front;
		NormalizeSafe(axes.xAxis, Vec3(0, 0, 1), size);
		axes.yAxis = axes.xAxis ^ front;
		axes.yAxis *= size * axes.yAxis.GetInvLength();
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
		axes.xAxis = orientation.GetColumn0() * -size;
		axes.yAxis = movingPositions[0] - movingPositions[2];
		NormalizeSafe(axes.yAxis, Vec3(0, 0, 1), size);
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
		, m_hasColors(container.HasData(EPDT_Color)) {}

	ILINE SParticleColorST Sample(TParticleId particleId, uint frameId, float animPos, float alpha) const
	{
		SParticleColorST colorST;
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
	const bool         m_hasColors;
};

void CFeatureRenderRibbon::WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& camera, CREParticle* pRE, const TParticleIdArray& sortEntries, const TRibbons& ribbons, uint numVertices, float fMaxPixels)
{
	CRY_PFX2_PROFILE_DETAIL;

	const CParticleContainer& container = runtime.GetContainer();

	if (m_ribbonMode == ERibbonMode::Camera)
		WriteToGPUMem<CRibbonAxesCamera>(runtime, camera, pRE, sortEntries, ribbons, CRibbonAxesCamera(container, camera), numVertices, fMaxPixels);
	else
		WriteToGPUMem<CRibbonAxesFree>(runtime, camera, pRE, sortEntries, ribbons, CRibbonAxesFree(container), numVertices, fMaxPixels);
}

template<typename TAxesSampler>
void CFeatureRenderRibbon::WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& camera, CREParticle* pRE, const TParticleIdArray& sortEntries, const TRibbons& ribbons, const TAxesSampler& axesSampler, uint numVertices, float fMaxPixels)
{
	const SComponentParams& params = runtime.ComponentParams();
	const CParticleContainer& container = runtime.GetContainer();
	const CParticleContainer& parentContainer = runtime.GetParentContainer();

	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IFStream sizes = container.GetIFStream(EPDT_Size);
	const IFStream alphas = container.GetIFStream(EPDT_Alpha);
	const IPidStream parentIds = container.IStream(EPDT_ParentId);
	const IFStream parentAges = parentContainer.GetIFStream(EPDT_NormalAge);
	const uint extraVertices = m_connectToOrigin ? 2 : 1;
	const CRibbonColorSTs colorSTsSampler = CRibbonColorSTs(container, m_streamSource);
	const bool hasOffset = m_offset != 0.0f;

	SRenderVertices* pRenderVertices = pRE->AllocPullVertices(numVertices);
	CWriteCombinedBuffer<Vec3, vertexBufferSize, vertexChunckSize> localPositions(pRenderVertices->aPositions);
	CWriteCombinedBuffer<SParticleAxes, vertexBufferSize, vertexChunckSize> localAxes(pRenderVertices->aAxes);
	CWriteCombinedBuffer<SParticleColorST, vertexBufferSize, vertexChunckSize> localColorSTs(pRenderVertices->aColorSTs);

	const Vec3 camPos = camera.GetPosition();

	uint totalRenderedParticles = 0;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	THeapArray<float> ribbonAlphas(memHeap, ribbons.size());

	float pixels = 0.0f, pixelsDrawn = 0.0f;
	CullRibbonAreas(runtime, camera, fMaxPixels, sortEntries, ribbons, ribbonAlphas, pixels, pixelsDrawn);

	for (uint ribbonId = 0; ribbonId < uint(ribbons.size()); ++ribbonId)
	{
		if (ribbonAlphas[ribbonId] <= 0.0f)
			continue;

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

		const TParticleId ribbonParentId = parentIds.Load(sortEntries[ribbon.m_lastIdx - 1]);

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

			const float alpha = alphas.Load(particleId) * ribbonAlphas[ribbonId];
			axes = axesSampler.Sample(particleId, movingPositions);
			colorST = colorSTsSampler.Sample(particleId, frameId, animPos, alpha);

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
		totalRenderedParticles += numVertices;
	}

	pRenderVertices->fPixels = pixels;

	auto& stats = GetPSystem()->GetThreadData().statsCPU;
	stats.pixels.updated += pos_round(pixels);
	stats.pixels.rendered += pos_round(pixelsDrawn);
	stats.particles.rendered += totalRenderedParticles;
	stats.components.rendered ++;

	GetPSystem()->GetProfiler().AddEntry(runtime, EPS_RendereredParticles, uint(totalRenderedParticles));
}

void CFeatureRenderRibbon::CullRibbonAreas(const CParticleComponentRuntime& runtime, const CCamera& camera, float fMaxPixels, TConstArray<uint> sortEntries, TConstArray<SRibbon> ribbons, TVarArray<float> ribbonAlphas, float& pixels, float &pixelsDrawn)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const CParticleContainer& container = runtime.GetContainer();
	const Vec3 camPos = camera.GetPosition();
	const float areaToPixels = sqr(camera.GetAngularResolution());
	const float screenArea = camera.GetFov() * camera.GetHorizontalFov();
	static const float ribbonAreaMultiple = 2.0f; // Estimate ribbon area as 2x sprite area
	const float maxSize = sqr(runtime.ComponentParams().m_maxParticleSize * 2.0f) * ribbonAreaMultiple;
	const float nearDist = runtime.GetBounds().GetDistance(camPos);
	const float maxArea = container.GetNumParticles() *	div_min(maxSize, sqr(nearDist), screenArea);
	const float areaLimit = fMaxPixels / areaToPixels;
	const bool cullArea = maxArea > areaLimit;
	const bool sumArea = cullArea || maxArea > 1.0f / 256.0f;

	ribbonAlphas.fill(1.0f);

	if (!sumArea)
	{
		pixels = pixelsDrawn = maxArea * areaToPixels;
		return;
	}

	IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	IFStream sizes = container.GetIFStream(EPDT_Size);
	IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);

	const CParticleContainer& parentContainer = runtime.GetParentContainer();
	IVec3Stream parentPositions = parentContainer .GetIVec3Stream(EPVF_Position);

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	THeapArray<uint> ids(memHeap);
	THeapArray<float> areas(memHeap);
	if (cullArea)
	{
		ids.resize(ribbons.size());
		areas.resize(ribbons.size());
	}

	SFrustumTest frustumTest(camera, runtime.GetBounds());

	float area = 0.0f;
	for (uint ribbonId = 0; ribbonId < ribbons.size(); ++ribbonId)
	{
		// Estimate ribbon area by sampling first, last, and mid points
		const SRibbon ribbon = ribbons[ribbonId];

		uint rIds[3] = 
		{
			sortEntries[ribbon.m_firstIdx],
			sortEntries[(ribbon.m_firstIdx + ribbon.m_lastIdx) / 2],
			sortEntries[ribbon.m_lastIdx - 1]
		};
		float rSizes[3] =
		{
			sizes.Load(rIds[0]), sizes.Load(rIds[1]), sizes.Load(rIds[2])
		};
		Vec3 rPositions[3] =
		{
			positions.Load(rIds[0]),
			positions.Load(rIds[1]),
			m_connectToOrigin && parentIds.Load(rIds[2]) != gInvalidId ?
				parentPositions.Load(parentIds.Load(rIds[2])) :
				positions.Load(rIds[2])
		};

		// Compute area for each segment
		const float ribbonArea =
			frustumTest.VisibleArea(rPositions[0], rSizes[0], rPositions[1], rSizes[1], screenArea) +
			frustumTest.VisibleArea(rPositions[2], rSizes[2], rPositions[1], rSizes[1], screenArea);
		area += ribbonArea;

		if (cullArea)
		{
			ids[ribbonId] = ribbonId;
			areas[ribbonId] = ribbonArea;
		}
	}

	const float areaDrawn = CullArea(area, areaLimit, ids, ribbonAlphas, areas);

	pixels = area * areaToPixels;
	pixelsDrawn = areaDrawn * areaToPixels;
}

}
