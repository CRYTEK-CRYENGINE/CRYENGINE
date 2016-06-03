// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CloudRenderNode.h
//  Version:     v1.00
//  Created:     15/2/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "CloudRenderNode.h"
#include "CloudsManager.h"
#include "VisAreas.h"
#include "ObjMan.h"

//////////////////////////////////////////////////////////////////////////
CCloudRenderNode::CCloudRenderNode()
{
	GetInstCount(GetRenderNodeType())++;

	m_bounds.min = Vec3(-1, -1, -1);
	m_bounds.max = Vec3(1, 1, 1);
	m_fScale = 1.0f;
	m_offsetedMatrix.SetIdentity();
	m_matrix.SetIdentity();
	m_vOffset.Set(0, 0, 0);
	m_alpha = 1.f;

	m_pCloudRenderElement = (CREBaseCloud*)GetRenderer()->EF_CreateRE(eDATA_Cloud);
	m_pREImposter = (CREImposter*) GetRenderer()->EF_CreateRE(eDATA_Imposter);

	GetCloudsManager()->AddCloudRenderNode(this);

	m_origin = Vec3(0, 0, 0);
	m_moveProps.m_autoMove = false;
	m_moveProps.m_speed = Vec3(0, 0, 0);
	m_moveProps.m_spaceLoopBox = Vec3(2000.0f, 2000.0f, 2000.0f);
	m_moveProps.m_fadeDistance = 0;
}

//////////////////////////////////////////////////////////////////////////
CCloudRenderNode::~CCloudRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	GetCloudsManager()->RemoveCloudRenderNode(this);
	m_pCloudRenderElement->Release(false);
	m_pREImposter->Release(false);

	Get3DEngine()->FreeRenderNodeState(this);
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::LoadCloudFromXml(XmlNodeRef root)
{
	m_pCloudDesc = new SCloudDescription;
	GetCloudsManager()->ParseCloudFromXml(root, m_pCloudDesc);

	SetCloudDesc(m_pCloudDesc);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::LoadCloud(const char* sCloudFilename)
{
	m_bounds.min = Vec3(-1, -1, -1);
	m_bounds.max = Vec3(1, 1, 1);

	SetCloudDesc(GetCloudsManager()->LoadCloud(sCloudFilename));

	return m_pCloudDesc != 0;
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMovementProperties(const SCloudMovementProperties& properties)
{
	m_moveProps = properties;
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetCloudDesc(SCloudDescription* pCloud)
{
	m_pCloudDesc = pCloud;
	if (m_pCloudDesc != NULL && m_pCloudDesc->m_particles.size() > 0)
	{
		m_vOffset = m_pCloudDesc->m_offset;
		m_bounds.min = m_pCloudDesc->m_bounds.min - m_pCloudDesc->m_offset;
		m_bounds.max = m_pCloudDesc->m_bounds.max - m_pCloudDesc->m_offset;
		if (m_pCloudDesc->m_pMaterial)
			m_pMaterial = m_pCloudDesc->m_pMaterial;
		m_pCloudRenderElement->SetParticles(&m_pCloudDesc->m_particles[0], m_pCloudDesc->m_particles.size());

		m_WSBBox.SetTransformedAABB(m_matrix, m_bounds);
		m_fScale = m_matrix.GetColumn(0).GetLength();
		// Offset matrix by the cloud bounds offset.
		m_offsetedMatrix = m_matrix * Matrix34::CreateTranslationMat(-m_vOffset);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMatrix(const Matrix34& mat)
{
	SetMatrixInternal(mat, true);
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMatrixInternal(const Matrix34& mat, bool updateOrigin)
{
	m_dwRndFlags |= ERF_OUTDOORONLY;

	if (updateOrigin)
		m_origin = mat.GetTranslation();

	m_matrix = mat;
	m_pos = mat.GetTranslation();
	//	m_WSBBox.SetTransformedAABB( m_matrix,m_bounds );
	m_fScale = mat.GetColumn(0).GetLength();
	m_WSBBox.SetTransformedAABB(Matrix34::CreateTranslationMat(m_pos), AABB(m_bounds.min * m_fScale, m_bounds.max * m_fScale));

	// Offset matrix by the cloud bounds offset.
	//	m_offsetedMatrix = m_matrix * Matrix34::CreateTranslationMat(-m_vOffset);
	m_offsetedMatrix = Matrix34::CreateTranslationMat(m_pos - m_vOffset * m_fScale);
	m_offsetedMatrix.ScaleColumn(Vec3(m_fScale, m_fScale, m_fScale));
	Get3DEngine()->RegisterEntity(this);
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::MoveCloud()
{
	FUNCTION_PROFILER_3DENGINE;

	Vec3 pos(m_matrix.GetTranslation());

	ITimer* pTimer(gEnv->pTimer);
	if (m_moveProps.m_autoMove)
	{
		// update position
		float deltaTime = pTimer->GetFrameTime();

		assert(deltaTime >= 0);

		pos += deltaTime * m_moveProps.m_speed;

		// constrain movement to specified loop box
		Vec3 loopBoxMin(m_origin - m_moveProps.m_spaceLoopBox);
		Vec3 loopBoxMax(m_origin + m_moveProps.m_spaceLoopBox);

		if (pos.x < loopBoxMin.x)
			pos.x = loopBoxMax.x;
		if (pos.y < loopBoxMin.y)
			pos.y = loopBoxMax.y;
		if (pos.z < loopBoxMin.z)
			pos.z = loopBoxMax.z;

		if (pos.x > loopBoxMax.x)
			pos.x = loopBoxMin.x;
		if (pos.y > loopBoxMax.y)
			pos.y = loopBoxMin.y;
		if (pos.z > loopBoxMax.z)
			pos.z = loopBoxMin.z;

		// set new position
		Matrix34 mat(m_matrix);
		mat.SetTranslation(pos);
		SetMatrixInternal(mat, false);

		// fade out clouds at the borders of the loop box
		if (m_moveProps.m_fadeDistance > 0)
		{
			Vec3 fade(max(m_moveProps.m_spaceLoopBox.x, m_moveProps.m_fadeDistance),
			          max(m_moveProps.m_spaceLoopBox.y, m_moveProps.m_fadeDistance),
			          max(m_moveProps.m_spaceLoopBox.z, m_moveProps.m_fadeDistance));

			fade -= Vec3(fabs(pos.x - m_origin.x), fabs(pos.y - m_origin.y), fabs(pos.z - m_origin.z));

			m_alpha = clamp_tpl(min(min(fade.x, fade.y), fade.z) / m_moveProps.m_fadeDistance, 0.0f, 1.0f);
		}
	}
	else
	{
		if ((m_origin - pos).GetLengthSquared() > 1e-4f)
		{
			Matrix34 mat(m_matrix);
			mat.SetTranslation(m_origin);
			SetMatrixInternal(mat, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::Render(const SRendParams& rParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pMaterial || !passInfo.RenderClouds())
		return;

	IRenderer* pRenderer(GetRenderer());

	// get render objects
	CRenderObject* pRO = pRenderer->EF_GetObject_Temp(passInfo.ThreadID());
	if (!pRO)
		return;

	SShaderItem& shaderItem = (rParams.pMaterial) ? rParams.pMaterial->GetShaderItem(0) : m_pMaterial->GetShaderItem(0);

	pRO->m_II.m_Matrix = m_offsetedMatrix;
	pRO->m_ObjFlags |= FOB_TRANS_MASK;
	SRenderObjData* pOD = pRO->GetObjData();
	pRO->m_pRE = m_pREImposter;
	pOD->m_fTempVars[0] = m_fScale;
	pRO->m_fSort = 0;
	pRO->m_fDistance = rParams.fDistance;
	int nAfterWater = CObjManager::IsAfterWater(m_offsetedMatrix.GetTranslation(), passInfo.GetCamera().GetPosition(), passInfo, Get3DEngine()->GetWaterLevel()) ? 1 : 0;
	pRO->m_II.m_AmbColor = rParams.AmbientColor;
	pRO->m_fAlpha = rParams.fAlpha * m_alpha;

	float mvd(GetMaxViewDist());
	float d((passInfo.GetCamera().GetPosition() - m_offsetedMatrix.GetTranslation()).GetLength());
	if (d > 0.9f * mvd)
	{
		float s(clamp_tpl(1.0f - (d - 0.9f * mvd) / (0.1f * mvd), 0.0f, 1.0f));
		pRO->m_fAlpha *= s;
	}

	pRenderer->EF_AddEf(m_pCloudRenderElement, shaderItem, pRO, passInfo, EFSLIST_TRANSP, nAfterWater);
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::CheckIntersection(const Vec3& p1, const Vec3& p2)
{
	if (p1 == p2)
		return false;
	if (m_pCloudDesc && m_pCloudDesc->m_pCloudTree)
	{
		Vec3 outp;
		if (Intersect::Lineseg_AABB(Lineseg(p1, p2), m_WSBBox, outp))
		{
			Matrix34 pInv = m_offsetedMatrix.GetInverted();
			return m_pCloudDesc->m_pCloudTree->CheckIntersection(pInv * p1, pInv * p2);
		}
	}
	return false;
}

void CCloudRenderNode::OffsetPosition(const Vec3& delta)
{
	if (m_pTempData) m_pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_origin += delta;
	m_matrix.SetTranslation(m_matrix.GetTranslation() + delta);
	m_offsetedMatrix.SetTranslation(m_offsetedMatrix.GetTranslation() + delta);
	m_WSBBox.Move(delta);
}

void CCloudRenderNode::FillBBox(AABB& aabb)
{
	aabb = CCloudRenderNode::GetBBox();
}

EERType CCloudRenderNode::GetRenderNodeType()
{
	return eERType_Cloud;
}

float CCloudRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, CCloudRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, CCloudRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

Vec3 CCloudRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

IMaterial* CCloudRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
