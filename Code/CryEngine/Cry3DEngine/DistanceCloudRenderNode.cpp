// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DistanceCloudRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "terrain.h"

CDistanceCloudRenderNode::CDistanceCloudRenderNode()
	: m_pos(0, 0, 0)
	, m_sizeX(1)
	, m_sizeY(1)
	, m_rotationZ(0)
	, m_pMaterial(NULL)
{
	GetInstCount(GetRenderNodeType())++;

	SetRndFlags(ERF_OUTDOORONLY, true); // should achieve faster octree update
}

CDistanceCloudRenderNode::~CDistanceCloudRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	Get3DEngine()->FreeRenderNodeState(this);
}

SDistanceCloudProperties CDistanceCloudRenderNode::GetProperties() const
{
	SDistanceCloudProperties properties;

	properties.m_sizeX = m_sizeX;
	properties.m_sizeY = m_sizeY;
	properties.m_rotationZ = m_rotationZ;
	properties.m_pos = m_pos;
	properties.m_pMaterialName = 0; // query materialID instead!

	return properties;
}

void CDistanceCloudRenderNode::SetProperties(const SDistanceCloudProperties& properties)
{
	// register material
	m_pMaterial = GetMatMan()->LoadMaterial(properties.m_pMaterialName, false);

	// copy distance cloud properties
	m_sizeX = properties.m_sizeX;
	m_sizeY = properties.m_sizeY;
	m_rotationZ = properties.m_rotationZ;
	m_pos = properties.m_pos;
}

void CDistanceCloudRenderNode::SetMatrix(const Matrix34& mat)
{
	if (m_pos == mat.GetTranslation())
		return;

	Get3DEngine()->UnRegisterEntityAsJob(this);

	m_pos = mat.GetTranslation();

	m_WSBBox.SetTransformedAABB(mat, AABB(-Vec3(1, 1, 1e-4f), Vec3(1, 1, 1e-4f)));

	Get3DEngine()->RegisterEntity(this);
}

const char* CDistanceCloudRenderNode::GetEntityClassName() const
{
	return "DistanceCloud";
}

const char* CDistanceCloudRenderNode::GetName() const
{
	return "DistanceCloud";
}

static inline uint16 HalfFlip(uint16 h)
{
	uint16 mask = -int16(h >> 15) | 0x8000;
	return h ^ mask;
}

void CDistanceCloudRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	IMaterial* pMaterial(GetMaterial());

	if (!passInfo.RenderClouds() || !pMaterial)
		return; // false;

	const CCamera& cam(passInfo.GetCamera());
	float zDist = cam.GetPosition().z - m_pos.z;
	if (cam.GetViewdir().z < 0)
		zDist = -zDist;

	CRenderObject* pRenderObject(passInfo.GetIRenderView()->AllocateTemporaryRenderObject());
	if (!pRenderObject)
		return;
	pRenderObject->m_nSort = HalfFlip(CryConvertFloatToHalf(zDist));

	// fill general vertex data
	f32 sinZ(0), cosZ(1);
	sincos_tpl(DEG2RAD(m_rotationZ), &sinZ, &cosZ);
	Vec3 right(m_sizeX * cosZ, m_sizeY * sinZ, 0);
	Vec3 up(-m_sizeX * sinZ, m_sizeY * cosZ, 0);

	SVF_P3F_C4B_T2F pVerts[4];
	pVerts[0].xyz = (-right - up) + m_pos;
	pVerts[0].st = Vec2(0, 1);
	pVerts[0].color.dcolor = ~0;

	pVerts[1].xyz = (right - up) + m_pos;
	pVerts[1].st = Vec2(1, 1);
	pVerts[1].color.dcolor = ~0;

	pVerts[2].xyz = (right + up) + m_pos;
	pVerts[2].st = Vec2(1, 0);
	pVerts[2].color.dcolor = ~0;

	pVerts[3].xyz = (-right + up) + m_pos;
	pVerts[3].st = Vec2(0, 0);
	pVerts[3].color.dcolor = ~0;

	// prepare tangent space (tangent, bitangent) and fill it in
	Vec3 rightUnit(cosZ, sinZ, 0);
	Vec3 upUnit(-sinZ, cosZ, 0);

	SPipTangents pTangents[4];

	pTangents[0] = SPipTangents(rightUnit, -upUnit, 1);
	pTangents[1] = pTangents[0];
	pTangents[2] = pTangents[0];
	pTangents[3] = pTangents[0];

	// prepare indices
	uint16 pIndices[6];
	pIndices[0] = 0;
	pIndices[1] = 1;
	pIndices[2] = 2;

	pIndices[3] = 0;
	pIndices[4] = 2;
	pIndices[5] = 3;

	int afterWater(GetObjManager()->IsAfterWater(m_pos, passInfo.GetCamera().GetPosition(), passInfo, Get3DEngine()->GetWaterLevel()) ? 1 : 0);
	SRenderPolygonDescription poly(pRenderObject, pMaterial->GetShaderItem(), 4, pVerts, pTangents, pIndices, 6, EFSLIST_SKY, afterWater);
	passInfo.GetIRenderView()->AddPolygon(poly, passInfo);
}

IPhysicalEntity* CDistanceCloudRenderNode::GetPhysics() const
{
	return 0;
}

void CDistanceCloudRenderNode::SetPhysics(IPhysicalEntity*)
{
}

void CDistanceCloudRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}

void CDistanceCloudRenderNode::Precache()
{
}

void CDistanceCloudRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "DistanceCloudNode");
	pSizer->AddObject(this, sizeof(*this));
}

void CDistanceCloudRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_WSBBox.Move(delta);
}

void CDistanceCloudRenderNode::FillBBox(AABB& aabb)
{
	aabb = CDistanceCloudRenderNode::GetBBox();
}

EERType CDistanceCloudRenderNode::GetRenderNodeType()
{
	return eERType_DistanceCloud;
}

float CDistanceCloudRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, CDistanceCloudRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, CDistanceCloudRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

Vec3 CDistanceCloudRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

IMaterial* CDistanceCloudRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
