// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IFogVolumeRenderNode;
class CDeviceCommandList;
namespace render_element
{
namespace fogvolume
{
struct SCompiledFogVolume;
}
}

class CREFogVolume : public CRenderElement
{
public:
	CREFogVolume();
	virtual ~CREFogVolume();


	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	virtual bool Compile(CRenderObject* pObj, CRenderView *pRenderView, bool updateInstanceDataOnly) override;
	virtual void DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList) override;

public:
	std::unique_ptr<render_element::fogvolume::SCompiledFogVolume> m_pCompiledObject;

	Vec3     m_center;
	uint32   m_viewerInsideVolume  : 1;
	uint32   m_affectsThisAreaOnly : 1;
	uint32   m_stencilRef          : 8;
	uint32   m_volumeType          : 1;
	uint32   m_reserved            : 21;
	AABB     m_localAABB;
	Matrix34 m_matWSInv;
	float    m_globalDensity;
	float    m_densityOffset;
	float    m_nearCutoff;
	Vec2     m_softEdgesLerp;
	ColorF   m_fogColor;              //!< Color already combined with fHDRDynamic.
	Vec3     m_heightFallOffDirScaled;
	Vec3     m_heightFallOffBasePoint;
	Vec3     m_eyePosInWS;
	Vec3     m_eyePosInOS;
	Vec3     m_rampParams;
	Vec3     m_windOffset;
	float    m_noiseScale;
	Vec3     m_noiseFreq;
	float    m_noiseOffset;
	float    m_noiseElapsedTime;
	Vec3     m_emission;

private:
	void PrepareForUse(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj, bool bInstanceOnly, CDeviceCommandList& RESTRICT_REFERENCE commandList) const;
	void UpdatePerDrawCB(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj, const CRenderObject& renderObj) const;
	bool UpdateVertex(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj);

private:
	stream_handle_t m_handleVertexBuffer;
	stream_handle_t m_handleIndexBuffer;
	AABB            m_cachedLocalAABB;

};
