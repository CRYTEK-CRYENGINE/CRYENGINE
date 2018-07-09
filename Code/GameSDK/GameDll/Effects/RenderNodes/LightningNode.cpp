// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LightningNode.h"
#include "Effects/GameEffects/LightningGameEffect.h"
#include "Utility/Hermite.h"
#include <CryRenderer/IRenderAuxGeom.h>

void CLightningRenderNode::CTriStrip::Reset()
{
	m_vertices.clear();
	m_indices.clear();
}

void CLightningRenderNode::CTriStrip::PushVertex(SLightningVertex v)
{
	int numTris = m_vertices.size() - m_firstVertex - 2;
	if (numTris < 0)
		numTris = 0;
	int vidx = m_vertices.size();
	if (numTris > 0)
	{
		if (numTris % 2)
		{
			m_indices.push_back(vidx - 1);
			m_indices.push_back(vidx - 2);
		}
		else
		{
			m_indices.push_back(vidx - 2);
			m_indices.push_back(vidx - 1);
		}
	}
	m_vertices.push_back(v);
	m_indices.push_back(vidx);
}

void CLightningRenderNode::CTriStrip::Branch()
{
	m_firstVertex = m_vertices.size();
}

void CLightningRenderNode::CTriStrip::Draw(const SRendParams& renderParams, const SRenderingPassInfo& passInfo, IRenderer* pRenderer, CRenderObject* pRenderObject, IMaterial* pMaterial, float distanceToCamera)
{
	if (m_vertices.empty())
		return;

	bool nAfterWater = true;

	pRenderObject->SetMatrix(Matrix34::CreateIdentity(), passInfo);
	pRenderObject->m_ObjFlags |= FOB_NO_FOG;
	pRenderObject->m_ObjFlags &= ~FOB_ALLOW_TESSELLATION;
	pRenderObject->m_nSort = fastround_positive(distanceToCamera * 2.0f);
	pRenderObject->m_fDistance = renderParams.fDistance;
	pRenderObject->m_pCurrMaterial = pMaterial;
	pRenderObject->m_pRenderNode = (IRenderNode*)this;  // Incompatible type!

	SRenderPolygonDescription poly(
	  pRenderObject,
	  pMaterial->GetShaderItem(),
	  m_vertices.size(), &m_vertices[0], nullptr,
	  &m_indices[0], m_indices.size(),
	  EFSLIST_DECAL, nAfterWater);

	passInfo.GetIRenderView()->AddPolygon(poly, passInfo);
}

void CLightningRenderNode::CTriStrip::Clear()
{
	m_indices.clear();
	m_vertices.clear();
}

void CLightningRenderNode::CTriStrip::AddStats(SLightningStats* pStats) const
{
	pStats->m_memory.Increment(m_vertices.capacity() * sizeof(SLightningVertex));
	pStats->m_memory.Increment(m_indices.capacity() * sizeof(uint16));
	pStats->m_triCount.Increment(m_indices.size() / 3);
}

void CLightningRenderNode::CSegment::Create(const SLightningParams& desc, SPointData* m_pointData, int _parentSegmentIdx, int _parentPointIdx, Vec3 _origin, Vec3 _destany, float _duration, float _intensity)
{
	m_firstPoint = m_pointData->m_points.size();
	m_firstFuzzyPoint = m_pointData->m_fuzzyPoints.size();
	m_origin = _origin;
	m_destany = _destany;
	m_intensity = _intensity;
	m_duration = _duration;
	m_parentSegmentIdx = _parentSegmentIdx;
	m_parentPointIdx = _parentPointIdx;
	m_time = 0.0f;

	const int numSegs = desc.m_strikeNumSegments;
	const int numSubSegs = desc.m_strikeNumPoints;
	const int numFuzzySegs = numSegs * numSubSegs;

	m_pointData->m_points.push_back(Vec3(ZERO));
	m_pointData->m_velocity.push_back(Vec3(ZERO));
	for (int i = 1; i < numSegs; ++i)
	{
		m_pointData->m_points.push_back(cry_random_unit_vector<Vec3>());
		m_pointData->m_velocity.push_back(cry_random_unit_vector<Vec3>() + Vec3(0.0f, 0.0f, 1.0f));
	}
	m_pointData->m_points.push_back(Vec3(ZERO));
	m_pointData->m_velocity.push_back(Vec3(ZERO));

	m_pointData->m_fuzzyPoints.push_back(Vec3(ZERO));
	for (int i = 1; i < numFuzzySegs; ++i)
	{
		m_pointData->m_fuzzyPoints.push_back(cry_random_unit_vector<Vec3>());
	}
	m_pointData->m_fuzzyPoints.push_back(Vec3(ZERO));

	m_numPoints = m_pointData->m_points.size() - m_firstPoint;
	m_numFuzzyPoints = m_pointData->m_fuzzyPoints.size() - m_firstFuzzyPoint;
}

void CLightningRenderNode::CSegment::Update(const SLightningParams& desc)
{
	m_time += gEnv->pTimer->GetFrameTime();
}

void CLightningRenderNode::CSegment::Draw(const SLightningParams& desc, const SPointData& pointData, CTriStrip* strip, Vec3 cameraPosition, float deviationMult)
{
	float fade = 1.0f;
	if (m_time > m_duration)
		fade = 1.0f - (m_time - m_duration) / (desc.m_strikeFadeOut + 0.001f);
	fade *= m_intensity;
	float halfSize = fade * desc.m_beamSize;
	UCol white;
	white.dcolor = ~0;

	const float frameHeight = 1.0f / desc.m_beamTexFrames;
	const float minY = fmod_tpl(floor_tpl(m_time * desc.m_beamTexFPS), desc.m_beamTexFrames) * frameHeight;
	const float maxY = minY + frameHeight;
	float distanceTraveled = 0.0f;

	strip->Branch();

	Vec3 p0 = GetPoint(desc, pointData, 0, deviationMult);
	Vec3 p1 = GetPoint(desc, pointData, 1, deviationMult);
	Vec3 p2 = GetPoint(desc, pointData, 2, deviationMult);

	{
		Vec3 front = p0 - cameraPosition;
		Vec3 dir1 = p0 - p1;
		Vec3 up = dir1.Cross(front).GetNormalized();

		SLightningVertex v;
		v.color = white;
		v.xyz = p0 - up * halfSize;
		v.st.x = -m_time * desc.m_beamTexShift;
		v.st.y = minY;
		strip->PushVertex(v);
		v.xyz = p0 + up * halfSize;
		v.st.y = maxY;
		strip->PushVertex(v);
	}

	for (int i = 1; i < m_numFuzzyPoints - 1; ++i)
	{
		p0 = p1;
		p1 = p2;
		p2 = GetPoint(desc, pointData, i + 1, deviationMult);
		distanceTraveled += p0.GetDistance(p1);

		Vec3 front = p1 - cameraPosition;
		Vec3 dir0 = p0 - p1;
		Vec3 dir1 = p1 - p2;
		Vec3 up0 = dir0.Cross(front);
		Vec3 up1 = dir1.Cross(front);
		Vec3 up = (up0 + up1).GetNormalized();
		float t = i / float(m_numFuzzyPoints - 1);

		SLightningVertex v;
		v.color = white;
		v.xyz = p1 - up * halfSize;
		v.st.x = desc.m_beamTexTiling * distanceTraveled - m_time * desc.m_beamTexShift;
		v.st.y = minY;
		strip->PushVertex(v);
		v.xyz = p1 + up * halfSize;
		v.st.y = maxY;
		strip->PushVertex(v);
	}

	{
		p0 = p1;
		p1 = p2;
		distanceTraveled += p0.GetDistance(p1);

		Vec3 front = p1 - cameraPosition;
		Vec3 dir0 = p0 - p1;
		Vec3 up = dir0.Cross(front).GetNormalized();

		SLightningVertex v;
		v.color = white;
		v.xyz = p1 - up * halfSize;
		v.st.x = desc.m_beamTexTiling * distanceTraveled - m_time * desc.m_beamTexShift;
		v.st.y = minY;
		strip->PushVertex(v);
		v.xyz = p1 + up * halfSize;
		v.st.y = maxY;
		strip->PushVertex(v);
	}
}

bool CLightningRenderNode::CSegment::IsDone(const SLightningParams& desc)
{
	return m_time > (m_duration + desc.m_strikeFadeOut);
}

void CLightningRenderNode::CSegment::SetOrigin(Vec3 _origin)
{
	m_origin = _origin;
}

void CLightningRenderNode::CSegment::SetDestany(Vec3 _destany)
{
	m_destany = _destany;
}

// cppcheck-suppress uninitMemberVar
CLightningRenderNode::CLightningRenderNode()
	: m_pMaterial(0)
	, m_pLightningDesc(0)
	, m_dirtyBBox(true)
	, m_deviationMult(1.0f)
{
}

CLightningRenderNode::~CLightningRenderNode()
{
}

const char* CLightningRenderNode::GetEntityClassName() const
{
	return "Lightning";
}

const char* CLightningRenderNode::GetName() const
{
	return "Lightning";
}

void CLightningRenderNode::Render(const struct SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	if (!m_pMaterial)
		return;

	IRenderer* pRenderer = gEnv->pRenderer;
	const CCamera& camera = GetISystem()->GetViewCamera();
	CRenderObject* pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	Vec3 cameraPosition = camera.GetPosition();
	float distanceToCamera = sqrt_tpl(Distance::Point_AABBSq(cameraPosition, GetBBox())) * passInfo.GetZoomFactor();

	m_triStrip.Reset();
	Update();
	Draw(&m_triStrip, cameraPosition);

	m_triStrip.Draw(rParam, passInfo, pRenderer, pRenderObject, m_pMaterial, distanceToCamera);
}

IPhysicalEntity* CLightningRenderNode::GetPhysics() const
{
	return 0;
}

void CLightningRenderNode::SetPhysics(IPhysicalEntity*)
{
}

void CLightningRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}

IMaterial* CLightningRenderNode::GetMaterialOverride()
{
	return m_pMaterial;
}

void CLightningRenderNode::Precache()
{
}

void CLightningRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
}

void CLightningRenderNode::SetBBox(const AABB& WSBBox)
{
	m_dirtyBBox = false;
	m_aabb = WSBBox;
}

bool CLightningRenderNode::IsAllocatedOutsideOf3DEngineDLL()
{
	return true;
}

void CLightningRenderNode::SetEmiterPosition(Vec3 emiterPosition)
{
	m_emmitterPosition = emiterPosition;
	m_dirtyBBox = true;
}

void CLightningRenderNode::SetReceiverPosition(Vec3 receiverPosition)
{
	m_receiverPosition = receiverPosition;
	m_dirtyBBox = true;
}

void CLightningRenderNode::SetSparkDeviationMult(float deviationMult)
{
	m_deviationMult = max(0.0f, deviationMult);
}

void CLightningRenderNode::AddStats(SLightningStats* pStats) const
{
	m_triStrip.AddStats(pStats);
	int lightningMem = 0;
	lightningMem += sizeof(CLightningRenderNode);
	lightningMem += m_pointData.m_points.capacity() * sizeof(Vec3);
	lightningMem += m_pointData.m_fuzzyPoints.capacity() * sizeof(Vec3);
	lightningMem += m_pointData.m_velocity.capacity() * sizeof(Vec3);
	lightningMem += m_segments.capacity() * sizeof(CSegment);
	pStats->m_memory.Increment(lightningMem);
	pStats->m_branches.Increment(m_segments.size());
}

void CLightningRenderNode::Reset()
{
	m_dirtyBBox = true;
	m_segments.clear();
	m_pointData.m_fuzzyPoints.clear();
	m_pointData.m_points.clear();
	m_pointData.m_velocity.clear();
	m_deviationMult = 1.0f;
}

float CLightningRenderNode::TriggerSpark()
{
	m_dirtyBBox = true;
	m_deviationMult = 1.0f;

	float lightningTime = cry_random(
	  m_pLightningDesc->m_strikeTimeMin,
	  m_pLightningDesc->m_strikeTimeMax);

	CreateSegment(m_emmitterPosition, -1, -1, lightningTime, 0);

	return lightningTime + m_pLightningDesc->m_strikeFadeOut;
}

void CLightningRenderNode::SetLightningParams(const SLightningParams* pDescriptor)
{
	m_pLightningDesc = pDescriptor;
}

void CLightningRenderNode::Update()
{
	m_dirtyBBox = true;

	for (TSegments::iterator it = m_segments.begin(); it != m_segments.end(); ++it)
		it->Update(*m_pLightningDesc);

	for (TSegments::iterator it = m_segments.begin(); it != m_segments.end(); ++it)
	{
		if (it->GetParentSegmentIdx() != -1)
			it->SetOrigin(m_segments[it->GetParentSegmentIdx()].GetPoint(*m_pLightningDesc, m_pointData, it->GetParentPointIdx(), m_deviationMult));
		else
			it->SetOrigin(m_emmitterPosition);
		it->SetDestany(m_receiverPosition);
	}

	while (!m_segments.empty() && m_segments[0].IsDone(*m_pLightningDesc))
		PopSegment();
}

void CLightningRenderNode::Draw(CTriStrip* strip, Vec3 cameraPosition)
{
	for (TSegments::iterator it = m_segments.begin(); it != m_segments.end(); ++it)
		it->Draw(*m_pLightningDesc, m_pointData, strip, cameraPosition, m_deviationMult);
}

void CLightningRenderNode::CreateSegment(Vec3 originPosition, int parentIdx, int parentPointIdx, float duration, int level)
{
	if (level > m_pLightningDesc->m_branchMaxLevel)
		return;
	if (m_segments.size() == m_pLightningDesc->m_maxNumStrikes)
		return;
	int segmentIdx = m_segments.size();

	CSegment segmentData;
	segmentData.Create(*m_pLightningDesc, &m_pointData, parentIdx, parentPointIdx, originPosition, m_receiverPosition, duration, 1.0f / (level + 1));
	m_segments.push_back(segmentData);

	float randVal = cry_random(
	  0.0f,
	  max(1.0f, m_pLightningDesc->m_branchProbability));
	float prob = m_pLightningDesc->m_branchProbability;
	int numBranches = int(randVal);
	prob -= std::floor(prob);
	if (randVal <= prob)
		numBranches++;

	for (int i = 0; i < numBranches; ++i)
	{
		int point = cry_random(0, m_pLightningDesc->m_strikeNumSegments * m_pLightningDesc->m_strikeNumPoints - 1);
		CreateSegment(
		  segmentData.GetPoint(*m_pLightningDesc, m_pointData, point, m_deviationMult),
		  segmentIdx, point, duration, level + 1);
	}

}

void CLightningRenderNode::PopSegment()
{
	m_segments.erase(m_segments.begin());
	for (TSegments::iterator it = m_segments.begin(); it != m_segments.end(); ++it)
	{
		if (it->GetParentSegmentIdx() != -1)
			it->DecrementParentIdx();
	}
}

void CLightningRenderNode::OffsetPosition(const Vec3& delta)
{
	m_aabb.Move(delta);
	m_emmitterPosition += delta;
	m_receiverPosition += delta;
}
