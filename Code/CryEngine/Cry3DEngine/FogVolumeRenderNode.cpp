// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FogVolumeRenderNode.h"
#include "VisAreas.h"
#include <CryRenderer/RenderElements/CREFogVolume.h>
#include <CryMath/Cry_Geo.h>
#include "ObjMan.h"
#include "ClipVolumeManager.h"

#include <limits>

AABB CFogVolumeRenderNode::s_tracableFogVolumeArea(Vec3(0, 0, 0), Vec3(0, 0, 0));
CFogVolumeRenderNode::CachedFogVolumes CFogVolumeRenderNode::s_cachedFogVolumes;
CFogVolumeRenderNode::GlobalFogVolumeMap CFogVolumeRenderNode::s_globalFogVolumeMap;
bool CFogVolumeRenderNode::s_forceTraceableAreaUpdate(false);

void CFogVolumeRenderNode::StaticReset()
{
	stl::free_container(s_cachedFogVolumes);
}

void CFogVolumeRenderNode::ForceTraceableAreaUpdate()
{
	s_forceTraceableAreaUpdate = true;
}

void CFogVolumeRenderNode::SetTraceableArea(const AABB& traceableArea, const SRenderingPassInfo& passInfo)
{
	// do we bother?
	if (!GetCVars()->e_Fog || !GetCVars()->e_FogVolumes)
		return;

	if (GetCVars()->e_VolumetricFog != 0)
		return;

	// is update of traceable areas necessary
	if (!s_forceTraceableAreaUpdate)
		if ((s_tracableFogVolumeArea.GetCenter() - traceableArea.GetCenter()).GetLengthSquared() < 1e-4f && (s_tracableFogVolumeArea.GetSize() - traceableArea.GetSize()).GetLengthSquared() < 1e-4f)
			return;

	// set new area and reset list of traceable fog volumes
	s_tracableFogVolumeArea = traceableArea;
	s_cachedFogVolumes.resize(0);

	// collect all candidates
	Vec3 traceableAreaCenter(s_tracableFogVolumeArea.GetCenter());
	IVisArea* pVisAreaOfCenter(GetVisAreaManager() ? GetVisAreaManager()->GetVisAreaFromPos(traceableAreaCenter) : NULL);

	GlobalFogVolumeMap::const_iterator itEnd(s_globalFogVolumeMap.end());
	for (GlobalFogVolumeMap::const_iterator it(s_globalFogVolumeMap.begin()); it != itEnd; ++it)
	{
		const CFogVolumeRenderNode* pFogVolume(*it);
		if (pVisAreaOfCenter || (!pVisAreaOfCenter && !pFogVolume->GetEntityVisArea())) // if outside only add fog volumes which are outside as well
			if (Overlap::AABB_AABB(s_tracableFogVolumeArea, pFogVolume->m_WSBBox))        // bb of fog volume overlaps with traceable area
				s_cachedFogVolumes.push_back(SCachedFogVolume(pFogVolume, Vec3(pFogVolume->m_pos - traceableAreaCenter).GetLengthSquared()));
	}

	// sort by distance
	std::sort(s_cachedFogVolumes.begin(), s_cachedFogVolumes.end());

	// reset force-update flags
	s_forceTraceableAreaUpdate = false;
}

void CFogVolumeRenderNode::RegisterFogVolume(const CFogVolumeRenderNode* pFogVolume)
{
	GlobalFogVolumeMap::const_iterator it(s_globalFogVolumeMap.find(pFogVolume));
	assert(it == s_globalFogVolumeMap.end() &&
	       "CFogVolumeRenderNode::RegisterFogVolume() -- Fog volume already registered!");
	if (it == s_globalFogVolumeMap.end())
	{
		s_globalFogVolumeMap.insert(pFogVolume);
		ForceTraceableAreaUpdate();
	}
}

void CFogVolumeRenderNode::UnregisterFogVolume(const CFogVolumeRenderNode* pFogVolume)
{
	GlobalFogVolumeMap::iterator it(s_globalFogVolumeMap.find(pFogVolume));
	assert(it != s_globalFogVolumeMap.end() &&
	       "CFogVolumeRenderNode::UnRegisterFogVolume() -- Fog volume previously not registered!");
	if (it != s_globalFogVolumeMap.end())
	{
		s_globalFogVolumeMap.erase(it);
		ForceTraceableAreaUpdate();
	}
}

CFogVolumeRenderNode::CFogVolumeRenderNode()
	: m_matNodeWS()
	, m_matWS()
	, m_matWSInv()
	, m_volumeType(0)
	, m_pos(0, 0, 0)
	, m_x(1, 0, 0)
	, m_y(0, 1, 0)
	, m_z(0, 0, 1)
	, m_scale(1, 1, 1)
	, m_globalDensity(1)
	, m_densityOffset(0)
	, m_nearCutoff(0)
	, m_fHDRDynamic(0)
	, m_softEdges(1)
	, m_color(1, 1, 1, 1)
	, m_useGlobalFogColor(false)
	, m_affectsThisAreaOnly(false)
	, m_rampParams(0, 1, 0)
	, m_updateFrameID(0)
	, m_windInfluence(1)
	, m_noiseElapsedTime(-5000.0f)
	, m_densityNoiseScale(0)
	, m_densityNoiseOffset(0)
	, m_densityNoiseTimeFrequency(0)
	, m_densityNoiseFrequency(1, 1, 1)
	, m_emission(0, 0, 0)
	, m_heightFallOffDir(0, 0, 1)
	, m_heightFallOffDirScaled(0, 0, 1)
	, m_heightFallOffShift(0, 0, 0)
	, m_heightFallOffBasePoint(0, 0, 0)
	, m_localBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
	, m_globalDensityFader()
	, m_pMatFogVolEllipsoid(0)
	, m_pMatFogVolBox(0)
	, m_WSBBox()
	, m_cachedSoftEdgesLerp(1, 0)
	, m_cachedFogColor(1, 1, 1, 1)
{
	GetInstCount(GetRenderNodeType())++;

	m_matNodeWS.SetIdentity();

	m_matWS.SetIdentity();
	m_matWSInv.SetIdentity();

	m_windOffset.x = cry_random(0.0f, 1000.0f);
	m_windOffset.y = cry_random(0.0f, 1000.0f);
	m_windOffset.z = cry_random(0.0f, 1000.0f);

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		if (GetRenderer())
			m_pFogVolumeRenderElement[i] = (CREFogVolume*)GetRenderer()->EF_CreateRE(eDATA_FogVolume);
		else
			m_pFogVolumeRenderElement[i] = nullptr;
	}

	m_pMatFogVolEllipsoid = Get3DEngine()->m_pMatFogVolEllipsoid;
	m_pMatFogVolBox = Get3DEngine()->m_pMatFogVolBox;

	//Get3DEngine()->RegisterEntity( this );
	RegisterFogVolume(this);
}

CFogVolumeRenderNode::~CFogVolumeRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		if (m_pFogVolumeRenderElement[i])
		{
			m_pFogVolumeRenderElement[i]->Release(false);
			m_pFogVolumeRenderElement[i] = 0;
		}
	}

	UnregisterFogVolume(this);
	Get3DEngine()->FreeRenderNodeState(this);
}

void CFogVolumeRenderNode::UpdateFogVolumeMatrices()
{
	// update matrices used for ray tracing, distance sorting, etc.
	Matrix34 mtx = Matrix34::CreateFromVectors(m_scale.x * m_x, m_scale.y * m_y, m_scale.z * m_z, m_pos);
	m_matWS = mtx;
	m_matWSInv = mtx.GetInverted();
}

void CFogVolumeRenderNode::UpdateWorldSpaceBBox()
{
	// update bounding box in world space used for culling
	m_WSBBox.SetTransformedAABB(m_matNodeWS, m_localBounds);
}

void CFogVolumeRenderNode::UpdateHeightFallOffBasePoint()
{
	m_heightFallOffBasePoint = m_pos + m_heightFallOffShift;
}

void CFogVolumeRenderNode::SetFogVolumeProperties(const SFogVolumeProperties& properties)
{
	m_globalDensityFader.SetInvalid();

	assert(properties.m_size.x > 0 && properties.m_size.y > 0 && properties.m_size.z > 0);
	Vec3 scale(properties.m_size * 0.5f);
	if ((m_scale - scale).GetLengthSquared() > 1e-4)
	{
		m_scale = properties.m_size * 0.5f;
		m_localBounds.min = Vec3(-1, -1, -1).CompMul(m_scale);
		m_localBounds.max = -m_localBounds.min;
		UpdateWorldSpaceBBox();
	}

	m_volumeType = properties.m_volumeType;
	assert(m_volumeType >= 0 && m_volumeType <= 1);
	m_color = properties.m_color;
	assert(properties.m_globalDensity >= 0);
	m_useGlobalFogColor = properties.m_useGlobalFogColor;
	m_globalDensity = properties.m_globalDensity;
	m_densityOffset = properties.m_densityOffset;
	m_nearCutoff = properties.m_nearCutoff;
	m_fHDRDynamic = properties.m_fHDRDynamic;
	assert(properties.m_softEdges >= 0 && properties.m_softEdges <= 1);
	m_softEdges = properties.m_softEdges;

	// IgnoreVisArea and AffectsThisAreaOnly don't work concurrently.
	SetRndFlags(ERF_RENDER_ALWAYS, properties.m_ignoresVisAreas && !properties.m_affectsThisAreaOnly);

	m_affectsThisAreaOnly = properties.m_affectsThisAreaOnly;

	float latiArc(DEG2RAD(90.0f - properties.m_heightFallOffDirLati));
	float longArc(DEG2RAD(properties.m_heightFallOffDirLong));
	float sinLati(sinf(latiArc));
	float cosLati(cosf(latiArc));
	float sinLong(sinf(longArc));
	float cosLong(cosf(longArc));
	m_heightFallOffDir = Vec3(sinLati * cosLong, sinLati * sinLong, cosLati);
	m_heightFallOffShift = m_heightFallOffDir * properties.m_heightFallOffShift;
	m_heightFallOffDirScaled = m_heightFallOffDir * properties.m_heightFallOffScale;
	UpdateHeightFallOffBasePoint();

	m_rampParams = Vec3(properties.m_rampStart, properties.m_rampEnd, properties.m_rampInfluence);

	m_windInfluence = properties.m_windInfluence;
	m_densityNoiseScale = properties.m_densityNoiseScale;
	m_densityNoiseOffset = properties.m_densityNoiseOffset + 1.0f;
	m_densityNoiseTimeFrequency = properties.m_densityNoiseTimeFrequency;
	m_densityNoiseFrequency = properties.m_densityNoiseFrequency * 0.01f;// scale the value to useful range
	m_emission = properties.m_emission;
}

const Matrix34& CFogVolumeRenderNode::GetMatrix() const
{
	return m_matNodeWS;
}

void CFogVolumeRenderNode::GetLocalBounds(AABB& bbox)
{
	bbox = m_localBounds;
};

void CFogVolumeRenderNode::SetMatrix(const Matrix34& mat)
{
	m_matNodeWS = mat;

	// get translation and rotational part of fog volume from entity matrix
	// scale is specified explicitly as fog volumes can be non-uniformly scaled
	m_pos = mat.GetTranslation();
	m_x = mat.GetColumn(0);
	m_y = mat.GetColumn(1);
	m_z = mat.GetColumn(2);

	UpdateFogVolumeMatrices();
	UpdateWorldSpaceBBox();
	UpdateHeightFallOffBasePoint();

	Get3DEngine()->RegisterEntity(this);
	ForceTraceableAreaUpdate();
}

void CFogVolumeRenderNode::FadeGlobalDensity(float fadeTime, float newGlobalDensity)
{
	if (newGlobalDensity >= 0)
	{
		if (fadeTime == 0)
		{
			m_globalDensity = newGlobalDensity;
			m_globalDensityFader.SetInvalid();
		}
		else if (fadeTime > 0)
		{
			float curFrameTime(gEnv->pTimer->GetCurrTime());
			m_globalDensityFader.Set(curFrameTime, curFrameTime + fadeTime, m_globalDensity, newGlobalDensity);
		}
	}
}

const char* CFogVolumeRenderNode::GetEntityClassName() const
{
	return "FogVolume";
}

const char* CFogVolumeRenderNode::GetName() const
{
	return "FogVolume";
}

ColorF CFogVolumeRenderNode::GetFogColor() const
{
	//FUNCTION_PROFILER_3DENGINE
	Vec3 fogColor(m_color.r, m_color.g, m_color.b);

	bool bVolFogEnabled = (GetCVars()->e_VolumetricFog != 0);
	if (bVolFogEnabled)
	{
		if (m_useGlobalFogColor)
		{
			Get3DEngine()->GetGlobalParameter(E3DPARAM_VOLFOG2_COLOR, fogColor);
		}
	}
	else
	{
		if (m_useGlobalFogColor)
		{
			fogColor = Get3DEngine()->GetFogColor();
		}
	}

	bool bHDRModeEnabled = false;
	GetRenderer()->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
	if (bHDRModeEnabled)
		fogColor *= powf(HDRDynamicMultiplier, m_fHDRDynamic);

	return fogColor;
}

Vec2 CFogVolumeRenderNode::GetSoftEdgeLerp(const Vec3& viewerPosOS) const
{
	// Volumetric fog doesn't need special treatment when camera is in the ellipsoid.
	if (GetCVars()->e_VolumetricFog != 0)
	{
		return Vec2(m_softEdges, 1.0f - m_softEdges);
	}

	//FUNCTION_PROFILER_3DENGINE
	// ramp down soft edge factor as soon as camera enters the ellipsoid
	float softEdge(m_softEdges * clamp_tpl((viewerPosOS.GetLength() - 0.95f) * 20.0f, 0.0f, 1.0f));
	return Vec2(softEdge, 1.0f - softEdge);
}

bool CFogVolumeRenderNode::IsViewerInsideVolume(const SRenderingPassInfo& passInfo) const
{
	const CCamera& cam(passInfo.GetCamera());

	// check if fog volumes bounding box intersects the near clipping plane
	const Plane* pNearPlane(cam.GetFrustumPlane(FR_PLANE_NEAR));
	Vec3 pntOnNearPlane(cam.GetPosition() - pNearPlane->DistFromPlane(cam.GetPosition()) * pNearPlane->n);
	Vec3 pntOnNearPlaneOS(m_matWSInv.TransformPoint(pntOnNearPlane));

	Vec3 nearPlaneOS_n(m_matWSInv.TransformVector(pNearPlane->n) /*.GetNormalized()*/);
	f32 nearPlaneOS_d(-nearPlaneOS_n.Dot(pntOnNearPlaneOS));

	// get extreme lengths
	float t(fabsf(nearPlaneOS_n.x) + fabsf(nearPlaneOS_n.y) + fabsf(nearPlaneOS_n.z));
	//float t( 0.0f );
	//if( nearPlaneOS_n.x >= 0 ) t += -nearPlaneOS_n.x; else t += nearPlaneOS_n.x;
	//if( nearPlaneOS_n.y >= 0 ) t += -nearPlaneOS_n.y; else t += nearPlaneOS_n.y;
	//if( nearPlaneOS_n.z >= 0 ) t += -nearPlaneOS_n.z; else t += nearPlaneOS_n.z;

	float t0 = t + nearPlaneOS_d;
	float t1 = -t + nearPlaneOS_d;

	return t0 * t1 < 0.0f;
}

void CFogVolumeRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// anything to render?
	if (passInfo.IsRecursivePass())
		return;

	if (!m_pMatFogVolBox || !m_pMatFogVolEllipsoid || GetCVars()->e_Fog == 0 || GetCVars()->e_FogVolumes == 0)
		return;

	const int32 fillThreadID = passInfo.ThreadID();

	if (!m_pFogVolumeRenderElement[fillThreadID])
		return;

	if (m_globalDensityFader.IsValid())
	{
		float curFrameTime(gEnv->pTimer->GetCurrTime());
		m_globalDensity = m_globalDensityFader.GetValue(curFrameTime);
		if (!m_globalDensityFader.IsTimeInRange(curFrameTime))
			m_globalDensityFader.SetInvalid();
	}

	// transform camera into fog volumes object space (where fog volume is a unit-sphere at (0,0,0))
	const CCamera& cam(passInfo.GetCamera());
	Vec3 viewerPosWS(cam.GetPosition());
	Vec3 viewerPosOS(m_matWSInv * viewerPosWS);

	m_cachedFogColor = GetFogColor();
	m_cachedSoftEdgesLerp = GetSoftEdgeLerp(viewerPosOS);

	bool bVolFog = (GetCVars()->e_VolumetricFog != 0);

	// reset elapsed time for noise when FogVolume stayed out of viewport for 30 frames.
	// this prevents the time from being too large number.
	if ((m_updateFrameID + 30) < passInfo.GetMainFrameID() && m_noiseElapsedTime > 5000.0f)
	{
		m_noiseElapsedTime = -5000.0f;
	}

	if (bVolFog && m_densityNoiseScale > 0.0f && m_updateFrameID != passInfo.GetMainFrameID())
	{
		Vec3 wind = Get3DEngine()->GetGlobalWind(false);
		const float elapsedTime = gEnv->pTimer->GetFrameTime();

		m_windOffset = ((-m_windInfluence * elapsedTime) * wind) + m_windOffset;

		const float windOffsetSpan = 1000.0f;// it should match the constant value in FogVolume.cfx
		m_windOffset.x = m_windOffset.x - floor(m_windOffset.x / windOffsetSpan) * windOffsetSpan;
		m_windOffset.y = m_windOffset.y - floor(m_windOffset.y / windOffsetSpan) * windOffsetSpan;
		m_windOffset.z = m_windOffset.z - floor(m_windOffset.z / windOffsetSpan) * windOffsetSpan;

		m_noiseElapsedTime += m_densityNoiseTimeFrequency * elapsedTime;

		m_updateFrameID = passInfo.GetMainFrameID();
	}

	float densityOffset = bVolFog ? (m_densityOffset * 0.001f) : m_densityOffset;// scale the value to useful range

	// set render element attributes
	m_pFogVolumeRenderElement[fillThreadID]->m_center = m_pos;
	m_pFogVolumeRenderElement[fillThreadID]->m_viewerInsideVolume = IsViewerInsideVolume(passInfo) ? 1 : 0;
	m_pFogVolumeRenderElement[fillThreadID]->m_affectsThisAreaOnly = m_affectsThisAreaOnly ? 1 : 0;
	m_pFogVolumeRenderElement[fillThreadID]->m_stencilRef = rParam.nClipVolumeStencilRef;
	m_pFogVolumeRenderElement[fillThreadID]->m_volumeType = (m_volumeType != 0) ? 1 : 0;
	m_pFogVolumeRenderElement[fillThreadID]->m_localAABB = m_localBounds;
	m_pFogVolumeRenderElement[fillThreadID]->m_matWSInv = m_matWSInv;
	m_pFogVolumeRenderElement[fillThreadID]->m_fogColor = m_cachedFogColor;
	m_pFogVolumeRenderElement[fillThreadID]->m_globalDensity = m_globalDensity;
	m_pFogVolumeRenderElement[fillThreadID]->m_densityOffset = densityOffset;
	m_pFogVolumeRenderElement[fillThreadID]->m_nearCutoff = m_nearCutoff;
	m_pFogVolumeRenderElement[fillThreadID]->m_softEdgesLerp = m_cachedSoftEdgesLerp;
	m_pFogVolumeRenderElement[fillThreadID]->m_heightFallOffDirScaled = m_heightFallOffDirScaled;
	m_pFogVolumeRenderElement[fillThreadID]->m_heightFallOffBasePoint = m_heightFallOffBasePoint;
	m_pFogVolumeRenderElement[fillThreadID]->m_eyePosInWS = viewerPosWS;
	m_pFogVolumeRenderElement[fillThreadID]->m_eyePosInOS = viewerPosOS;
	m_pFogVolumeRenderElement[fillThreadID]->m_rampParams = m_rampParams;
	m_pFogVolumeRenderElement[fillThreadID]->m_windOffset = m_windOffset;
	m_pFogVolumeRenderElement[fillThreadID]->m_noiseScale = m_densityNoiseScale;
	m_pFogVolumeRenderElement[fillThreadID]->m_noiseFreq = m_densityNoiseFrequency;
	m_pFogVolumeRenderElement[fillThreadID]->m_noiseOffset = m_densityNoiseOffset;
	m_pFogVolumeRenderElement[fillThreadID]->m_noiseElapsedTime = m_noiseElapsedTime;
	m_pFogVolumeRenderElement[fillThreadID]->m_emission = m_emission;

	if (bVolFog)
	{
		// add FogVolume to volumetric fog renderer
		GetRenderer()->PushFogVolume(m_pFogVolumeRenderElement[fillThreadID], passInfo);
	}
	else
	{
		IRenderer* pRenderer = GetRenderer();
		CRenderObject* pRenderObject = pRenderer->EF_GetObject_Temp(fillThreadID);

		if (!pRenderObject)
			return;

		// set basic render object properties
		pRenderObject->m_II.m_Matrix = m_matNodeWS;
		pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
		pRenderObject->m_fSort = 0;

		int nAfterWater = GetObjManager()->IsAfterWater(m_pos, passInfo.GetCamera().GetPosition(), passInfo, Get3DEngine()->GetWaterLevel()) ? 1 : 0;

		// TODO: add constant factor to sortID to make fog volumes render before all other alpha transparent geometry (or have separate render list?)
		pRenderObject->m_fSort = WATER_LEVEL_SORTID_OFFSET * 0.5f;

		// get shader item
		SShaderItem& shaderItem(0 != rParam.pMaterial ? rParam.pMaterial->GetShaderItem(0) :
		                        1 == m_volumeType ? m_pMatFogVolBox->GetShaderItem(0) : m_pMatFogVolEllipsoid->GetShaderItem(0));

		// add to renderer
		GetRenderer()->EF_AddEf(m_pFogVolumeRenderElement[fillThreadID], shaderItem, pRenderObject, passInfo, EFSLIST_TRANSP, nAfterWater);
	}
}

IPhysicalEntity* CFogVolumeRenderNode::GetPhysics() const
{
	return 0;
}

void CFogVolumeRenderNode::SetPhysics(IPhysicalEntity*)
{
}

void CFogVolumeRenderNode::SetMaterial(IMaterial* pMat)
{
}

void CFogVolumeRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "FogVolumeNode");
	pSizer->AddObject(this, sizeof(*this));
}

void CFogVolumeRenderNode::OffsetPosition(const Vec3& delta)
{
	if (m_pTempData) m_pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_matNodeWS.SetTranslation(m_matNodeWS.GetTranslation() + delta);
	m_matWS.SetTranslation(m_matWS.GetTranslation() + delta);
	m_matWSInv = m_matWS.GetInverted();
	m_heightFallOffBasePoint += delta;
	m_WSBBox.Move(delta);
}
