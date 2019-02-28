// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   decals.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw, create decals on the world
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "DecalManager.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "Vegetation.h"
#include "terrain.h"

IGeometry* CDecal::s_pSphere = 0;

void CDecal::ResetStaticData()
{
	SAFE_RELEASE(s_pSphere);
}

int CDecal::Update(bool& active, const float fFrameTime)
{
	// process life time and disable decal when needed
	m_fLifeTime -= fFrameTime;

	if (m_fLifeTime < 0)
	{
		active = 0;
		FreeRenderData();
	}
	else if (m_ownerInfo.pRenderNode && m_ownerInfo.pRenderNode->m_nInternalFlags & IRenderNode::UPDATE_DECALS)
	{
		active = false;
		return 1;
	}
	return 0;
}

Vec3 CDecal::GetWorldPosition()
{
	Vec3 vPos = m_vPos;

	if (m_ownerInfo.pRenderNode)
	{
		if (m_eDecalType == eDecalType_OS_SimpleQuad || m_eDecalType == eDecalType_OS_OwnersVerticesUsed)
		{
			assert(m_ownerInfo.pRenderNode);
			if (m_ownerInfo.pRenderNode)
			{
				Matrix34A objMat;
				if (IStatObj* pEntObject = m_ownerInfo.GetOwner(objMat))
					vPos = objMat.TransformPoint(vPos);
			}
		}
	}

	return vPos;
}

void CDecal::Render(const float fCurTime, int nAfterWater, float fDistanceFading, float fDistance, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pMaterial || !m_pMaterial->GetShaderItem().m_pShader || m_pMaterial->GetShaderItem().m_pShader->GetShaderType() != eST_General)
		return; // shader not supported for decals

	// Get decal alpha from life time
	float fAlpha = m_fLifeTime * 2;

	if (fAlpha > 1.f)
		fAlpha = 1.f;
	else if (fAlpha < 0)
		return;

	fAlpha *= fDistanceFading;

	float fSizeK;
	if (m_fGrowTime)
		fSizeK = min(1.f, sqrt_tpl((fCurTime - m_fLifeBeginTime) / m_fGrowTime));
	else
		fSizeK = 1.f;

	float fSizeAlphaK;
	if (m_fGrowTimeAlpha)
		fSizeAlphaK = min(1.f, sqrt_tpl((fCurTime - m_fLifeBeginTime) / m_fGrowTimeAlpha));
	else
		fSizeAlphaK = 1.f;

	if (m_bDeferred)
	{
		SDeferredDecal newItem;
		newItem.fAlpha = fAlpha;
		newItem.pMaterial = m_pMaterial;
		newItem.nSortOrder = m_sortPrio;
		newItem.nFlags = 0;

		Vec3 vRight, vUp, vNorm;
		Matrix34A objMat;

		if (IStatObj* pEntObject = m_ownerInfo.GetOwner(objMat))
		{
			vRight = objMat.TransformVector(m_vRight * m_fSize);
			vUp = objMat.TransformVector(m_vUp * m_fSize);
			vNorm = objMat.TransformVector((Vec3(m_vRight).Cross(m_vUp)) * m_fSize);
		}
		else
		{
			vRight = (m_vRight * m_fSize);
			vUp = (m_vUp * m_fSize);
			vNorm = ((Vec3(m_vRight).Cross(m_vUp)) * m_fSize);
		}

		Matrix33 matRotation;
		matRotation.SetColumn(0, vRight);
		matRotation.SetColumn(1, vUp);
		matRotation.SetColumn(2, vNorm * GetFloatCVar(e_DecalsDeferredDynamicDepthScale));
		newItem.projMatrix.SetRotation33(matRotation);
		newItem.projMatrix.SetTranslation(m_vWSPos + vNorm * .1f * m_fWSSize);

		if (m_fGrowTimeAlpha)
			newItem.fGrowAlphaRef = max(.02f, 1.f - fSizeAlphaK);
		else
			newItem.fGrowAlphaRef = 0;

		GetRenderer()->EF_AddDeferredDecal(newItem, passInfo);
		return;
	}

	switch (m_eDecalType)
	{
	case eDecalType_WS_Merged:
	case eDecalType_OS_OwnersVerticesUsed:
		{
			// check if owner mesh was deleted
			if (m_pRenderMesh && (m_pRenderMesh->GetVertexContainer() == m_pRenderMesh) && m_pRenderMesh->GetVerticesCount() < 3)
				FreeRenderData();

			if (!m_pRenderMesh)
				break;

			// setup transformation
			CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
			if (!pObj)
				return;
			pObj->m_fSort = 0;
			pObj->m_RState = 0;

			Matrix34A objMat;
			if (m_ownerInfo.pRenderNode && !m_ownerInfo.GetOwner(objMat))
			{
				assert(0);
				return;
			}
			else if (!m_ownerInfo.pRenderNode)
			{
				objMat.SetIdentity();
				if (m_eDecalType == eDecalType_WS_Merged)
				{
					objMat.SetTranslation(m_vPos);
					pObj->m_ObjFlags |= FOB_TRANS_MASK;
				}
			}

			pObj->SetMatrix(objMat, passInfo);
			if (m_ownerInfo.pRenderNode)
				pObj->m_ObjFlags |= FOB_TRANS_MASK;

			pObj->m_nSort = m_sortPrio;

			// somehow it's need's to be twice bigger to be same as simple decals
			float fSize2 = m_fSize * fSizeK * 2.f;///m_ownerInfo.pRenderNode->GetScale();
			if (fSize2 < 0.0001f)
				return;

			// setup texgen
			// S component
			float correctScale(-1);
			m_arrBigDecalRMCustomData[0] = correctScale * m_vUp.x / fSize2;
			m_arrBigDecalRMCustomData[1] = correctScale * m_vUp.y / fSize2;
			m_arrBigDecalRMCustomData[2] = correctScale * m_vUp.z / fSize2;

			Vec3 vPosDecS = m_vPos;
			if (m_eDecalType == eDecalType_WS_Merged)
				vPosDecS.zero();

			float D0 =
			  m_arrBigDecalRMCustomData[0] * vPosDecS.x +
			  m_arrBigDecalRMCustomData[1] * vPosDecS.y +
			  m_arrBigDecalRMCustomData[2] * vPosDecS.z;

			m_arrBigDecalRMCustomData[3] = -D0 + 0.5f;

			// T component
			m_arrBigDecalRMCustomData[4] = m_vRight.x / fSize2;
			m_arrBigDecalRMCustomData[5] = m_vRight.y / fSize2;
			m_arrBigDecalRMCustomData[6] = m_vRight.z / fSize2;

			float D1 =
			  m_arrBigDecalRMCustomData[4] * vPosDecS.x +
			  m_arrBigDecalRMCustomData[5] * vPosDecS.y +
			  m_arrBigDecalRMCustomData[6] * vPosDecS.z;

			m_arrBigDecalRMCustomData[7] = -D1 + 0.5f;

			// pass attenuation info
			m_arrBigDecalRMCustomData[8] = vPosDecS.x;
			m_arrBigDecalRMCustomData[9] = vPosDecS.y;
			m_arrBigDecalRMCustomData[10] = vPosDecS.z;
			m_arrBigDecalRMCustomData[11] = m_fSize;

			// N component
			Vec3 vNormal(Vec3(correctScale * m_vUp).Cross(m_vRight).GetNormalized());
			m_arrBigDecalRMCustomData[12] = vNormal.x * (m_fSize / m_fWSSize);
			m_arrBigDecalRMCustomData[13] = vNormal.y * (m_fSize / m_fWSSize);
			m_arrBigDecalRMCustomData[14] = vNormal.z * (m_fSize / m_fWSSize);
			m_arrBigDecalRMCustomData[15] = 0;

			CStatObj* pBody = NULL;
			bool bUseBending = GetCVars()->e_VegetationBending != 0;
			if (m_ownerInfo.pRenderNode && m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation)
			{
				CObjManager* pObjManager = GetObjManager();
				CVegetation* pVegetation = (CVegetation*)m_ownerInfo.pRenderNode;
				pBody = pVegetation->GetStatObj();
				assert(pObjManager && pVegetation && pBody);

				if (pVegetation && pBody && bUseBending)
				{
					pVegetation->FillBendingData(pObj, passInfo);
				}
				IMaterial* pMat = m_ownerInfo.pRenderNode->GetMaterial();
				pMat = pMat->GetSubMtl(m_ownerInfo.nMatID);
				if (pMat)
				{
					// Support for public parameters from owner (deformations)
					SShaderItem& SH = pMat->GetShaderItem();
					if (SH.m_pShaderResources)
					{
						IMaterial* pMatDecal = m_pMaterial;
						SShaderItem& SHDecal = pMatDecal->GetShaderItem();
						if (bUseBending)
						{
							pObj->m_nMDV = SH.m_pShader->GetVertexModificator();
							//if (SHDecal.m_pShaderResources && pObj->m_nMDV)
							//SH.m_pShaderResources->ExportModificators(SHDecal.m_pShaderResources, pObj);
						}
					}
				}
				if (m_eDecalType == eDecalType_OS_OwnersVerticesUsed)
					pObj->m_ObjFlags |= FOB_OWNER_GEOMETRY;
				IFoliage* pFol = pVegetation->GetFoliage();
				if (pFol)
				{
					SRenderObjData* pOD = pObj->GetObjData();
					if (pOD)
					{
						pOD->m_pSkinningData = pFol->GetSkinningData(objMat, passInfo);
					}
				}
				if (pBody && pBody->m_pRenderMesh)
				{
					m_pRenderMesh->SetVertexContainer(pBody->m_pRenderMesh);
				}
			}

			// draw complex decal using new indices and original object vertices
			pObj->m_fAlpha = fAlpha;
			pObj->m_ObjFlags |= FOB_DECAL | FOB_DECAL_TEXGEN_2D | FOB_INSHADOW;
			pObj->SetAmbientColor(m_vAmbient, passInfo);

			m_pRenderMesh->SetREUserData(m_arrBigDecalRMCustomData, 0, fAlpha);
			m_pRenderMesh->AddRenderElements(m_pMaterial, pObj, passInfo, EFSLIST_GENERAL, nAfterWater);
		}
		break;

	case eDecalType_OS_SimpleQuad:
		{
			assert(m_ownerInfo.pRenderNode);
			if (!m_ownerInfo.pRenderNode)
				break;

			// transform decal in software from owner space into world space and render as quad
			Matrix34A objMat;
			IStatObj* pEntObject = m_ownerInfo.GetOwner(objMat);
			if (!pEntObject)
				break;

			Vec3 vPos = objMat.TransformPoint(m_vPos);
			Vec3 vRight = objMat.TransformVector(m_vRight * m_fSize);
			Vec3 vUp = objMat.TransformVector(m_vUp * m_fSize);
			UCol uCol;

			uCol.dcolor = 0xffffffff;
			uCol.bcolor[3] = fastround_positive(fAlpha * 255);

			AddDecalToRenderView(fDistance, m_pMaterial, m_sortPrio, vRight * fSizeK, vUp * fSizeK, uCol,
			                     OS_ALPHA_BLEND, m_vAmbient, vPos, nAfterWater,
			                     m_ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation ? (CVegetation*) m_ownerInfo.pRenderNode : nullptr, passInfo);
		}
		break;

	case eDecalType_WS_SimpleQuad:
		{
			// draw small world space decal untransformed
			UCol uCol;
			uCol.dcolor = 0;
			uCol.bcolor[3] = fastround_positive(fAlpha * 255);
			AddDecalToRenderView(fDistance, m_pMaterial, m_sortPrio, m_vRight * m_fSize * fSizeK,
			                     m_vUp * m_fSize * fSizeK, uCol, OS_ALPHA_BLEND, m_vAmbient, m_vPos, nAfterWater, nullptr, passInfo);
		}
		break;

	case eDecalType_WS_OnTheGround:
		{
			RenderBigDecalOnTerrain(fAlpha, fSizeK, passInfo);
		}
		break;
	}
}

void CDecal::FreeRenderData()
{
	// delete render mesh
	m_pRenderMesh = NULL;

	m_ownerInfo.pRenderNode = 0;
}

void CDecal::RenderBigDecalOnTerrain(float fAlpha, float fScale, const SRenderingPassInfo& passInfo)
{
	float fRadius = m_fSize * fScale;

	// check terrain bounds
	if (m_vPos.x < -fRadius || m_vPos.y < -fRadius)
		return;
	if (m_vPos.x >= CTerrain::GetTerrainSize() + fRadius || m_vPos.y >= CTerrain::GetTerrainSize() + fRadius)
		return;

	fRadius += CTerrain::GetHeightMapUnitSize();

	if (fabs(m_vPos.z - Get3DEngine()->GetTerrainZ((m_vPos.x), (m_vPos.y))) > fRadius)
		return; // too far from ground surface

	// setup texgen
	float fSize2 = m_fSize * fScale * 2.f;
	if (fSize2 < 0.05f)
		return;

	// S component
	float correctScale(-1);
	m_arrBigDecalRMCustomData[0] = correctScale * m_vUp.x / fSize2;
	m_arrBigDecalRMCustomData[1] = correctScale * m_vUp.y / fSize2;
	m_arrBigDecalRMCustomData[2] = correctScale * m_vUp.z / fSize2;

	float D0 = 0;

	m_arrBigDecalRMCustomData[3] = -D0 + 0.5f;

	// T component
	m_arrBigDecalRMCustomData[4] = m_vRight.x / fSize2;
	m_arrBigDecalRMCustomData[5] = m_vRight.y / fSize2;
	m_arrBigDecalRMCustomData[6] = m_vRight.z / fSize2;

	float D1 = 0;

	m_arrBigDecalRMCustomData[7] = -D1 + 0.5f;

	// pass attenuation info
	m_arrBigDecalRMCustomData[8] = 0;
	m_arrBigDecalRMCustomData[9] = 0;
	m_arrBigDecalRMCustomData[10] = 0;
	m_arrBigDecalRMCustomData[11] = fSize2;

	Vec3 vNormal(Vec3(correctScale * m_vUp).Cross(m_vRight).GetNormalized());
	m_arrBigDecalRMCustomData[12] = vNormal.x;
	m_arrBigDecalRMCustomData[13] = vNormal.y;
	m_arrBigDecalRMCustomData[14] = vNormal.z;
	m_arrBigDecalRMCustomData[15] = 0;

	CRenderObject* pObj = GetIdentityCRenderObject(passInfo);
	if (!pObj)
		return;
	pObj->SetMatrix(Matrix34::CreateTranslationMat(m_vPos), passInfo);
	pObj->m_ObjFlags |= FOB_TRANS_TRANSLATE;

	pObj->m_fAlpha = fAlpha;
	pObj->m_ObjFlags |= FOB_DECAL | FOB_DECAL_TEXGEN_2D | FOB_INSHADOW;
	pObj->SetAmbientColor(m_vAmbient, passInfo);

	pObj->m_nSort = m_sortPrio;

	Plane planes[4];
	planes[0].SetPlane(m_vRight, m_vRight * m_fSize + m_vPos);
	planes[1].SetPlane(-m_vRight, -m_vRight * m_fSize + m_vPos);
	planes[2].SetPlane(m_vUp, m_vUp * m_fSize + m_vPos);
	planes[3].SetPlane(-m_vUp, -m_vUp * m_fSize + m_vPos);

	// m_pRenderMesh might get updated by the following function
	GetTerrain()->RenderArea(m_vPos, fRadius, m_pRenderMesh,
	                         pObj, m_pMaterial, "BigDecalOnTerrain", m_arrBigDecalRMCustomData, GetCVars()->e_DecalsClip ? planes : NULL, passInfo);
}

//////////////////////////////////////////////////////////////////////////
void CDecal::AddDecalToRenderView(float fDistance,
                                  IMaterial* pMaterial,
                                  const uint8 sortPrio,
                                  Vec3 right,
                                  Vec3 up,
                                  const UCol& ucResCol,
                                  const uint8 uBlendType,
                                  const Vec3& vAmbientColor,
                                  Vec3 vPos,
                                  const int nAfterWater,
                                  CVegetation* pVegetation,
                                  const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "AddDecalToRenderer");

	CRenderObject* pRenderObject(GetIdentityCRenderObject(passInfo));
	if (!pRenderObject)
		return;

	// prepare render object
	pRenderObject->m_fDistance = fDistance;
	pRenderObject->m_fAlpha = (float)ucResCol.bcolor[3] / 255.f;
	pRenderObject->SetAmbientColor(vAmbientColor, passInfo);
	pRenderObject->m_fSort = 0;
	pRenderObject->m_ObjFlags |= FOB_DECAL | FOB_INSHADOW;
	pRenderObject->m_nSort = sortPrio;

	bool bBending = pVegetation && pVegetation->IsBending();

	if (bBending)
	{
		// transfer decal into object space
		Matrix34A objMat;
		IStatObj* pEntObject = pVegetation->GetEntityStatObj(0, &objMat);
		pRenderObject->SetMatrix(objMat, passInfo);
		pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
		pVegetation->FillBendingData(pRenderObject, passInfo);
		assert(pEntObject);
		if (pEntObject)
		{
			objMat.Invert();
			vPos = objMat.TransformPoint(vPos);
			right = objMat.TransformVector(right);
			up = objMat.TransformVector(up);
		}
	}

	SVF_P3F_C4B_T2F pVerts[4];
	uint16 pIndices[6];

	// TODO: determine whether this is a decal on opaque or transparent geometry
	// (put it in the respective renderlist for correct shadowing)
	// fill general vertex data
	pVerts[0].xyz = (-right - up) + vPos;
	pVerts[0].st = Vec2(0, 1);
	pVerts[0].color.dcolor = ~0;

	pVerts[1].xyz = (right - up) + vPos;
	pVerts[1].st = Vec2(1, 1);
	pVerts[1].color.dcolor = ~0;

	pVerts[2].xyz = (right + up) + vPos;
	pVerts[2].st = Vec2(1, 0);
	pVerts[2].color.dcolor = ~0;

	pVerts[3].xyz = (-right + up) + vPos;
	pVerts[3].st = Vec2(0, 0);
	pVerts[3].color.dcolor = ~0;

	// prepare tangent space (tangent, bitangent) and fill it in
	Vec3 rightUnit(right.GetNormalized());
	Vec3 upUnit(up.GetNormalized());

	SPipTangents pTangents[4];

	pTangents[0] = SPipTangents(rightUnit, -upUnit, -1);
	pTangents[1] = pTangents[0];
	pTangents[2] = pTangents[0];
	pTangents[3] = pTangents[0];

	// fill decals topology (two triangles)
	pIndices[0] = 0;
	pIndices[1] = 1;
	pIndices[2] = 2;

	pIndices[3] = 0;
	pIndices[4] = 2;
	pIndices[5] = 3;

	SRenderPolygonDescription poly(pRenderObject, pMaterial->GetShaderItem(), 4, pVerts, pTangents, pIndices, 6, EFSLIST_DECAL, nAfterWater);
	passInfo.GetIRenderView()->AddPolygon(poly, passInfo);
}
