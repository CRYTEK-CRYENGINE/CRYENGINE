// Copyright 2015-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "ParticleSystem/ParticleRender.h"
#include <CryMath/RadixSort.h>

namespace pfx2
{


MakeDataType(EPDT_RibbonId, TParticleId);  // Corresponds to ParentId, preserved after parent death

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

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(CParticleComponentRuntime& runtime) override;
	virtual void ComputeVertices(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;

private:
	void MakeRibbons(const CParticleComponentRuntime& runtime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices);
	void WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& cameraMain, const CCamera& cameraRender, CREParticle* pRE, const TParticleIdArray& pSortEntries, const TRibbons& pRibbons, uint numVertices, float fMaxPixels);
	void CullRibbonAreas(const CParticleComponentRuntime& runtime, const CCamera& camera, float fMaxPixels, TConstArray<uint> sortEntries, TConstArray<SRibbon> ribbons, TVarArray<float> ribbonAlphas, float& pixels, float &pixelsDrawn);

	ERibbonMode         m_ribbonMode       = ERibbonMode::Camera;
	ERibbonStreamSource m_streamSource     = ERibbonStreamSource::Age;
	bool                m_connectToParent  = false;
	bool                m_tessellated      = false;
	UFloat10            m_textureFrequency = 1.0f;
	UFloat10            m_offset           = 0.0f;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderRibbon, "Render", "Ribbon", colorRender);

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
	if (m_ribbonMode == ERibbonMode::Free)
		pComponent->AddParticleData(EPQF_Orientation);
	pComponent->AddParticleData(DataType(m_streamSource));
	pParams->m_shaderData.m_textureFrequency = m_textureFrequency;
	if (m_tessellated)
		pParams->m_renderObjectFlags |= FOB_ALLOW_TESSELLATION;
}

void CFeatureRenderRibbon::Serialize(Serialization::IArchive& ar)
{
	BaseClass::Serialize(ar);
	SERIALIZE_VAR(ar, m_ribbonMode);
	SERIALIZE_VAR(ar, m_streamSource);
	SERIALIZE_VAR(ar, m_connectToParent);
	SERIALIZE_VAR(ar, m_tessellated);
	SERIALIZE_VAR(ar, m_textureFrequency);
	SERIALIZE_VAR(ar, m_offset);

	SERIALIZE_VAR(ar, m_sortBias);
	SERIALIZE_VAR(ar, m_fillCost);

	if (ar.isInput() && GetVersion(ar) < 14)
		ar(m_connectToParent, "ConnectToOrigin", "");
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
		WriteToGPUMem(runtime, *camInfo.pMainCamera, *camInfo.pCamera, pRE, sortEntries, ribbons, numVertices, fMaxPixels);
}

void CFeatureRenderRibbon::MakeRibbons(const CParticleComponentRuntime& runtime, TParticleIdArray* pSortEntries, TRibbons* pRibbons, uint* pNumVertices)
{
	CRY_PFX2_PROFILE_DETAIL;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	const CParticleContainer& container = runtime.GetContainer();
	const IUintStream ribbonIds = container.IStream(EPDT_RibbonId);
	const IUintStream parentIds = container.IStream(EPDT_ParentId);
	const IPidStream spawnIds = container.IStream(EPDT_SpawnId);
	const uint32 numParticles = container.Size();

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
	for (; ribbon.m_lastIdx < numParticles; ++ribbon.m_lastIdx)
	{
		const TParticleId particleId = (*pSortEntries)[ribbon.m_lastIdx];
		const TParticleId ribbonId = ribbonIds.Load(particleId);
		if (ribbonId != lastRibbonId)
		{
			if (ribbon.m_lastIdx - ribbon.m_firstIdx + m_connectToParent >= 2)
				++ribbonCount;
			ribbon.m_firstIdx = ribbon.m_lastIdx;
			lastRibbonId = ribbonId;
		}
	}
	++ribbonCount;
	pRibbons->reserve(ribbonCount);

	*pNumVertices = 0;
	ribbon.m_firstIdx = 0;
	ribbon.m_lastIdx = 0;
	for (; ribbon.m_lastIdx <= numParticles; ++ribbon.m_lastIdx)
	{
		const TParticleId ribbonId = ribbon.m_lastIdx < numParticles ? ribbonIds.Load((*pSortEntries)[ribbon.m_lastIdx]) : gInvalidId;
		if (ribbonId != lastRibbonId)
		{
			const bool connectParent = m_connectToParent && ribbon.m_lastIdx > 0 && parentIds.Load((*pSortEntries)[ribbon.m_lastIdx - 1]) != gInvalidId;
			const uint numVertices = ribbon.m_lastIdx - ribbon.m_firstIdx + connectParent + 1;
			if (numVertices >= 3)
			{
				pRibbons->push_back(ribbon);
				*pNumVertices += numVertices;
			}
			ribbon.m_firstIdx = ribbon.m_lastIdx;
			lastRibbonId = ribbonId;
		}
	}
}

static const float minParticleSize = 1e-6f;

ILINE float NormalizeSafe(Vec3& out, const Vec3& v, float size)
{
	float lensqr = v.len2();
	if (lensqr > sqr(FLT_EPSILON))
	{
		out = v * (rsqrt(lensqr) * size);
		return size;
	}
	else
	{
		out.zero();
		return 0.0f;
	}
}

class CRibbonAxes
{
public:
	CRibbonAxes(ERibbonMode facing, const CParticleContainer& container, const CCamera& camera)
		: m_facing(facing)
		, m_sizes(container.GetIFStream(EPDT_Size))
		, m_orientations(container.GetIQuatStream(EPQF_Orientation))
		, m_cameraPosition(camera.GetPosition()) {}

	ILINE bool Sample(SParticleAxes& axes, TParticleId particleId, const Vec3 movingPositions[3], float axisScale) const
	{
		axes.yAxis = (movingPositions[2] - movingPositions[0]) * axisScale;
		if (axes.yAxis.IsZero(minParticleSize))
			return false;
		const float size = max(m_sizes.Load(particleId), minParticleSize);
		if (m_facing == ERibbonMode::Camera)
		{
			const Vec3 front = movingPositions[1] - m_cameraPosition;
			return NormalizeSafe(axes.xAxis, axes.yAxis ^ front, size) > minParticleSize;
		}
		else
		{
			const Quat orientation = m_orientations.Load(particleId);
			axes.xAxis = orientation.GetColumn0() * -size;
			return true;
		}
	}

private:
	ERibbonMode       m_facing;
	const IFStream    m_sizes;
	const IQuatStream m_orientations;
	Vec3              m_cameraPosition;
};

class CRibbonColorSTs
{
public:
	CRibbonColorSTs(const CParticleContainer& container, ERibbonStreamSource streamSource)
		: m_colors(container.GetIColorStream(EPDT_Color, UCol{{~0u}}))
		, m_stream(container.GetIFStream(DataType(streamSource))) {}

	ILINE void Init(SParticleColorST& colorST, uint frameId, float animPos) const
	{
		colorST.color.dcolor = ~0;
		colorST.st.dcolor = 0;
		colorST.st.z = frameId;
		colorST.st.w = FloatToUFrac8Saturate(animPos - int(animPos));
	}

	ILINE void Sample(SParticleColorST& colorST, TParticleId particleId, float alpha) const
	{
		colorST.color = m_colors.SafeLoad(particleId);
		colorST.color.a = FloatToUFrac8Saturate(alpha);
		const float stream = m_stream.Load(particleId);
		colorST.st.y = FloatToUFrac8Saturate(stream);
	}

private:
	const IColorStream m_colors;
	const IFStream     m_stream;
};

void CFeatureRenderRibbon::WriteToGPUMem(const CParticleComponentRuntime& runtime, const CCamera& cameraMain, const CCamera& cameraRender, CREParticle* pRE, const TParticleIdArray& sortEntries, const TRibbons& ribbons, uint numVertices, float fMaxPixels)
{
	CRY_PFX2_PROFILE_DETAIL;

	const CParticleContainer& container = runtime.GetContainer();
	const SComponentParams& params = runtime.ComponentParams();
	const CParticleContainer& parentContainer = runtime.GetParentContainer();

	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IFStream alphas = container.GetIFStream(EPDT_Alpha, 1.0f);
	const IPidStream parentIds = container.IStream(EPDT_ParentId);
	const IFStream parentAges = parentContainer.GetIFStream(EPDT_NormalAge);
	const CRibbonAxes axesSampler {m_ribbonMode, container, cameraRender};
	const CRibbonColorSTs colorSTsSampler {container, m_streamSource};
	const bool hasOffset = m_offset != 0.0f;

	SRenderVertices* pRenderVertices = pRE->AllocPullVertices(numVertices);
	CWriteCombinedBuffer<Vec3, vertexBufferSize, vertexChunckSize> localPositions(pRenderVertices->aPositions);
	CWriteCombinedBuffer<SParticleAxes, vertexBufferSize, vertexChunckSize> localAxes(pRenderVertices->aAxes);
	CWriteCombinedBuffer<SParticleColorST, vertexBufferSize, vertexChunckSize> localColorSTs(pRenderVertices->aColorSTs);

	uint totalRenderedParticles = 0;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	THeapArray<float> ribbonAlphas(memHeap, ribbons.size());

	float pixels = 0.0f, pixelsDrawn = 0.0f;
	CullRibbonAreas(runtime, cameraMain, fMaxPixels, sortEntries, ribbons, ribbonAlphas, pixels, pixelsDrawn);

	for (uint ribbonId = 0; ribbonId < ribbons.size(); ++ribbonId)
	{
		if (ribbonAlphas[ribbonId] <= 0.0f)
			continue;

		const SRibbon ribbon = ribbons[ribbonId];

		const TParticleId ribbonParentId = parentIds.Load(sortEntries[ribbon.m_lastIdx - 1]);
		const bool connectToParent = m_connectToParent && ribbonParentId != gInvalidId;
		const uint numVertices = ribbon.m_lastIdx - ribbon.m_firstIdx + connectToParent + 1;
		if (!localPositions.CheckAvailable(numVertices) || !localAxes.CheckAvailable(numVertices) || !localColorSTs.CheckAvailable(numVertices))
			break;

		Vec3 movingPositions[3] =
		{
			positions.Load(sortEntries[ribbon.m_firstIdx]),
			movingPositions[0],
			movingPositions[0]
		};

		const float parentAge = ribbonParentId != gInvalidId ? parentAges.Load(ribbonParentId) : 1.0f;
		const float animPos = parentAge * float(params.m_shaderData.m_frameCount);
		const uint frameId = int(min(animPos, params.m_shaderData.m_frameCount - 1));

		Vec3 position;
		SParticleAxes axes;
		SParticleColorST colorST;
		colorSTsSampler.Init(colorST, frameId, animPos);
		float axisScale = 1.0f;

		const uint lastIdx = ribbon.m_lastIdx + connectToParent;
		for (uint i = ribbon.m_firstIdx; i < lastIdx; ++i)
		{
			const TParticleId particleId = sortEntries[min(i, ribbon.m_lastIdx - 1)];

			movingPositions[0] = movingPositions[1];
			movingPositions[1] = movingPositions[2];
			if (i < lastIdx - 1)
			{
				if (i + 1 < ribbon.m_lastIdx)
					movingPositions[2] = positions.Load(sortEntries[i + 1]);
				else if (connectToParent)
					movingPositions[2] = parentPositions.Load(ribbonParentId);
			}
			else
				axisScale = 1.0f;

			position = movingPositions[1];

			if (axesSampler.Sample(axes, particleId, movingPositions, axisScale))
			{
				if (i < ribbon.m_lastIdx)
				{
					const float alpha = alphas.SafeLoad(particleId) * ribbonAlphas[ribbonId];
					colorSTsSampler.Sample(colorST, particleId, alpha);
				}
				else
				{
					colorST.st.y = 0;
				}

				if (hasOffset)
				{
					const Vec3 zAxis = axes.xAxis.Cross(axes.yAxis).GetNormalized();
					position -= zAxis * m_offset;
				}

				localPositions.Array().push_back(position);
				localAxes.Array().push_back(axes);
				localColorSTs.Array().push_back(colorST);
			}
			axisScale = 0.5f;
		}

		// Add degenerate vertex between ribbons
		if (ribbonId < ribbons.size() - 1)
		{
			colorST.st.x = 255;
			localPositions.Array().push_back(position);
			localAxes.Array().push_back(axes);
			localColorSTs.Array().push_back(colorST);
		}

		totalRenderedParticles += numVertices;
	}

	pRenderVertices->fPixels = pixels;

	auto& stats = GetPSystem()->GetThreadData().statsCPU;
	stats.pixels.updated += pos_round(pixels);
	stats.pixels.rendered += pos_round(pixelsDrawn);
	stats.particles.rendered += totalRenderedParticles;
	stats.spawners.rendered += ribbons.size();
	stats.components.rendered ++;

	GetPSystem()->GetProfiler().AddEntry(runtime, EPS_RendereredParticles, totalRenderedParticles);
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
	const float maxArea = container.GetNumParticles() *	div_min(maxSize, sqr(nearDist), screenArea) * m_fillCost;
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
			m_connectToParent && parentIds.Load(rIds[2]) != gInvalidId ?
				parentPositions.Load(parentIds.Load(rIds[2])) :
				positions.Load(rIds[2])
		};

		// Compute area for each segment
		const float ribbonArea =
			(frustumTest.VisibleArea(rPositions[0], rSizes[0], rPositions[1], rSizes[1], screenArea)
			+ frustumTest.VisibleArea(rPositions[2], rSizes[2], rPositions[1], rSizes[1], screenArea))
			* m_fillCost;
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
