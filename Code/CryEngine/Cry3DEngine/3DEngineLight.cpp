// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3denginelight.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Light sources manager
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include <CryMath/AABBSV.h>
#include "terrain.h"
#include "LightEntity.h"
#include "ObjectsTree.h"
#include "Brush.h"
#include "ClipVolumeManager.h"

void C3DEngine::RegisterLightSourceInSectors(SRenderLight* pDynLight, const SRenderingPassInfo& passInfo)
{
	// AntonK: this hack for colored shadow maps is temporary, since we will render it another way in the future
	if (pDynLight->m_Flags & DLF_SUN || !m_pTerrain || !pDynLight->m_pOwner)
		return;

	if (GetRenderer()->EF_IsFakeDLight(pDynLight))
		return;

	// pass to outdoor only outdoor lights and some indoor projectors
	IVisArea* pLightArea = pDynLight->m_pOwner->GetEntityVisArea();
	if (!pLightArea || ((pDynLight->m_Flags & DLF_PROJECT) && pLightArea->IsConnectedToOutdoor()))
	{
		if (m_pObjectsTree)
			m_pObjectsTree->AddLightSource(pDynLight, passInfo);
	}

	if (m_pVisAreaManager)
		m_pVisAreaManager->AddLightSource(pDynLight, passInfo);
}

ILightSource* C3DEngine::CreateLightSource()
{
	// construct new object
	CLightEntity* pLightEntity = new CLightEntity();

	m_lstStaticLights.Add(pLightEntity);

	return pLightEntity;
}

void C3DEngine::DeleteLightSource(ILightSource* pLightSource)
{
	if (m_lstStaticLights.Delete((CLightEntity*)pLightSource) || pLightSource == m_pSun)
	{
		if (pLightSource == m_pSun)
			m_pSun = NULL;

		delete pLightSource;
	}
	else
		assert(!"Light object not found");
}

void CLightEntity::Release(bool)
{
	Get3DEngine()->UnRegisterEntityDirect(this);
	Get3DEngine()->DeleteLightSource(this);
}

void CLightEntity::SetLightProperties(const SRenderLight& light)
{
	C3DEngine* engine = Get3DEngine();

	m_light = light;

	m_bShadowCaster = (m_light.m_Flags & DLF_CASTSHADOW_MAPS) != 0;

	m_light.m_fLightFrustumAngle = CLAMP(m_light.m_fLightFrustumAngle, 0.f, (LIGHT_PROJECTOR_MAX_FOV / 2.f));

	if (!(m_light.m_Flags & (DLF_PROJECT | DLF_AREA_LIGHT)))
		m_light.m_fLightFrustumAngle = 90.f / 2.f;

	m_light.m_pOwner = this;

	if (m_light.m_Flags & DLF_ATTACH_TO_SUN)
	{
		m_dwRndFlags |= ERF_RENDER_ALWAYS | ERF_HUD;
	}

	engine->GetLightEntities()->Delete((ILightSource*)this);

	PodArray<ILightSource*>& lightEntities = *engine->GetLightEntities();

	//on consoles we force all lights (except sun) to be deferred
	if (GetCVars()->e_DynamicLightsForceDeferred && !(m_light.m_Flags & (DLF_SUN | DLF_POST_3D_RENDERER)))
		m_light.m_Flags |= DLF_DEFERRED_LIGHT;

	if (light.m_Flags & DLF_DEFERRED_LIGHT)
		lightEntities.Add((ILightSource*)this);
	else
		lightEntities.InsertBefore((ILightSource*)this, 0);
}

const PodArray<SRenderLight*>* C3DEngine::GetStaticLightSources()
{
	m_tmpLstLights.Clear();

	for (ILightSource* pStaticLight : m_lstStaticLights)
	{
		m_tmpLstLights.Add(&pStaticLight->GetLightProperties());
	}

	return &m_tmpLstLights;
}

void C3DEngine::FindPotentialLightSources(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;
#ifndef _RELEASE
	static ICVar* pCV_r_wireframe = GetConsole()->GetCVar("r_wireframe");
	if (pCV_r_wireframe && pCV_r_wireframe->GetIVal() == R_WIREFRAME_MODE)
		return;
#endif
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	for (int i = 0; i < m_lstStaticLights.Count(); i++)
	{
		CLightEntity* pLightEntity = (CLightEntity*)m_lstStaticLights[i];
		SRenderLight* pLight = &pLightEntity->GetLightProperties();

		if (pLight->m_Flags & DLF_DEFERRED_LIGHT)
			break; // process deferred lights in CLightEntity::Render(), deferred lights are stored in the end of this array

		int nRenderNodeMinSpec = (pLightEntity->m_dwRndFlags & ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
		if (!CheckMinSpec(nRenderNodeMinSpec))
			continue;

		const bool bIsPost3dRendererLight = (pLight->m_Flags & DLF_POST_3D_RENDERER) ? true : false;
		const Vec3 vFinalCamPos = (!bIsPost3dRendererLight) ? vCamPos : ZERO; // Post 3d renderer camera is at origin
		float fEntDistance = sqrt(Distance::Point_AABBSq(vFinalCamPos, pLightEntity->GetBBox())) * passInfo.GetZoomFactor();
		if (fEntDistance > pLightEntity->m_fWSMaxViewDist && !bIsPost3dRendererLight)
			continue;

		IMaterial* pMat = pLightEntity->GetMaterial();
		if (pMat)
			pLight->m_Shader = pMat->GetShaderItem();

		bool bIsVisible = false;
		if (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)
		{
			OBB obb(OBB::CreateOBBfromAABB(Matrix33(pLight->m_ObjMatrix), AABB(-pLight->m_ProbeExtents, pLight->m_ProbeExtents)));
			bIsVisible = passInfo.GetCamera().IsOBBVisible_F(pLight->m_Origin, obb);
		}
		else if (pLight->m_Flags & DLF_AREA_LIGHT)
		{
			// OBB test for area lights.
			Vec3 vBoxMax(pLight->m_fRadius, pLight->m_fRadius + pLight->m_fAreaWidth, pLight->m_fRadius + pLight->m_fAreaHeight);
			Vec3 vBoxMin(-0.1f, -(pLight->m_fRadius + pLight->m_fAreaWidth), -(pLight->m_fRadius + pLight->m_fAreaHeight));

			OBB obb(OBB::CreateOBBfromAABB(Matrix33(pLight->m_ObjMatrix), AABB(vBoxMin, vBoxMax)));
			bIsVisible = passInfo.GetCamera().IsOBBVisible_F(pLight->m_Origin, obb);
		}
		else
		{
			if (!bIsPost3dRendererLight)
			{
				bIsVisible = passInfo.GetCamera().IsSphereVisible_F(Sphere(pLight->m_Origin, pLight->m_fRadius));
			}
			else
			{
				bIsVisible = true;
			}
		}

		if (bIsVisible)
		{
			if ((pLight->m_Flags & DLF_PROJECT) && (pLight->m_fLightFrustumAngle < 90.f) && (pLight->m_pLightImage || pLight->m_pLightDynTexSource))
			{
				CCamera lightCam = passInfo.GetCamera();
				lightCam.SetPositionNoUpdate(pLight->m_Origin);
				Matrix34 entMat = ((ILightSource*)(pLight->m_pOwner))->GetMatrix();
				Matrix33 matRot = Matrix33::CreateRotationVDir(entMat.GetColumn(0));
				lightCam.SetMatrixNoUpdate(Matrix34(matRot, pLight->m_Origin));
				lightCam.SetFrustum(1, 1, (pLight->m_fLightFrustumAngle * 2) / 180.0f * gf_PI, 0.1f, pLight->m_fRadius);
				if (!CLightEntity::FrustumIntersection(passInfo.GetCamera(), lightCam) && !bIsPost3dRendererLight)
					continue;
			}

			if (pLight->m_pOwner)
				((CLightEntity*)pLight->m_pOwner)->UpdateCastShadowFlag(fEntDistance, passInfo);

			AddDynamicLightSource(*pLight, pLight->m_pOwner, 1, pLightEntity->m_fWSMaxViewDist > 0.f ? fEntDistance / pLightEntity->m_fWSMaxViewDist : 0.f, passInfo);
		}
	}
}

void C3DEngine::ResetCasterCombinationsCache()
{
	for (int nSunInUse = 0; nSunInUse < 2; nSunInUse++)
	{
		// clear user counters
		for (ShadowFrustumListsCacheUsers::iterator it = m_FrustumsCacheUsers[nSunInUse].begin(); it != m_FrustumsCacheUsers[nSunInUse].end(); ++it)
			it->second = 0;
	}
}

void C3DEngine::AddDynamicLightSource(const SRenderLight& LSource, ILightSource* pEnt, int nEntityLightId, float fFadeout, const SRenderingPassInfo& passInfo)
{
	assert(pEnt && _finite(LSource.m_Origin.x) && _finite(LSource.m_Origin.y) && _finite(LSource.m_fRadius));
	assert(LSource.IsOk());

	if ((LSource.m_Flags & DLF_DISABLED) || (!GetCVars()->e_DynamicLights))
	{
		for (int i = 0; i < m_lstDynLights.Count(); i++)
		{
			if (m_lstDynLights[i]->m_pOwner == pEnt)
			{
				FreeLightSourceComponents(m_lstDynLights[i]);
				m_lstDynLights.Delete(i);
			}
		}

		return;
	}

	if (m_lstDynLights.Count() == GetCVars()->e_DynamicLightsMaxCount)
		Warning("C3DEngine::AddDynamicLightSource: more than %d dynamic light sources created", GetCVars()->e_DynamicLightsMaxCount);
	else if (m_lstDynLights.Count() > GetCVars()->e_DynamicLightsMaxCount)
		return;

	if (!(LSource.m_Flags & DLF_POST_3D_RENDERER))
	{
		if (LSource.m_Flags & DLF_SUN && !(GetCVars()->e_CoverageBuffer == 2))
		{
			// sun
			IF (LSource.m_Color.Max() <= 0.0f || !GetCVars()->e_Sun, 0)
				return; // sun disabled
		}
		else if (GetCVars()->e_DynamicLightsFrameIdVisTest > 1 && (GetCVars()->e_CoverageBuffer != 2))
		{
			if (GetCVars()->e_DynamicLightsConsistentSortOrder)
				if (passInfo.GetFrameID() - pEnt->GetDrawFrame(0) > MAX_FRAME_ID_STEP_PER_FRAME)
					return;
		}

		if (GetCVars()->e_DynamicLightsFrameIdVisTest)
		{
			int nMaxReqursion = (LSource.m_Flags & DLF_THIS_AREA_ONLY) ? 2 : 3;
			if (!m_pObjManager || !m_pVisAreaManager || !m_pVisAreaManager->IsEntityVisAreaVisible(pEnt, nMaxReqursion, &LSource, passInfo))
			{
				if (LSource.m_Flags & DLF_SUN && m_pVisAreaManager && m_pVisAreaManager->m_bSunIsNeeded)
				{
					// sun may be used in indoor even if outdoor is not visible
				}
				else
					return;
			}
		}
	}

	// distance fading
	const float fFadingRange = 0.25f;
	fFadeout -= (1.f - fFadingRange);
	fFadeout = fFadeout < 0.f ? 0.f : fFadeout;
	fFadeout = 1.f - fFadeout / fFadingRange;
	assert(fFadeout > 0); // should be culled earlier

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Try to update present lsource
	////////////////////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < m_lstDynLights.Count(); i++)
	{
		if (m_lstDynLights[i]->m_pOwner == pEnt)
		{
			// copy lsource (keep old CRenderObject)
			CRenderObject* pObj[MAX_RECURSION_LEVELS];
			memcpy(&pObj[0], &(m_lstDynLights[i]->m_pObject[0]), sizeof(pObj));
			*m_lstDynLights[i] = LSource;

			//reset DLF_CASTSHADOW_MAPS
			if (!passInfo.RenderShadows())
				m_lstDynLights[i]->m_Flags &= ~DLF_CASTSHADOW_MAPS;

			memcpy(&m_lstDynLights[i]->m_pObject[0], &pObj[0], sizeof(pObj));

			// !HACK: Needs to decrement reference counter of shader because m_lstDynLights never release light sources
			if (LSource.m_Shader.m_pShader)
				LSource.m_Shader.m_pShader->Release();

			m_lstDynLights[i]->m_pOwner = pEnt;

			// set base params
			//m_lstDynLights[i]->m_BaseOrigin = m_lstDynLights[i]->m_Origin;

			ColorF cNewColor;
			IF_LIKELY ((m_lstDynLights[i]->m_Flags & DLF_DEFERRED_CUBEMAPS) == 0)
				cNewColor = ColorF(LSource.m_Color.r * fFadeout, LSource.m_Color.g * fFadeout, LSource.m_Color.b * fFadeout, LSource.m_Color.a);
			else
				cNewColor = ColorF(LSource.m_Color.r, LSource.m_Color.g, LSource.m_Color.b, clamp_tpl(fFadeout, 0.f, 1.f)); // use separate
			m_lstDynLights[i]->SetLightColor(cNewColor);

			m_lstDynLights[i]->m_n3DEngineUpdateFrameID = passInfo.GetMainFrameID();

			return;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Add new lsource into list and set some parameters
	////////////////////////////////////////////////////////////////////////////////////////////////

	m_lstDynLights.Add(new SRenderLight);
	*m_lstDynLights.Last() = LSource;

	// add ref to avoid shader deleting
	if (m_lstDynLights.Last()->m_Shader.m_pShader)
		m_lstDynLights.Last()->m_Shader.m_pShader->AddRef();

	m_lstDynLights.Last()->m_pOwner = pEnt;

	m_lstDynLights.Last()->SetLightColor(ColorF(LSource.m_Color.r * fFadeout, LSource.m_Color.g * fFadeout, LSource.m_Color.b * fFadeout, LSource.m_Color.a));

	m_lstDynLights.Last()->m_n3DEngineUpdateFrameID = passInfo.GetMainFrameID();
}

void C3DEngine::PrepareLightSourcesForRendering_0(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	const CCamera& rCamera = passInfo.GetCamera();

	m_lstDynLightsNoLight.Clear();

	bool bWarningPrinted = false;

	// update lmasks in terrain sectors
	if (passInfo.IsRecursivePass())
	{
		// do not delete lsources during recursion, becasue hmap lpasses are shared between levels
		for (int i = 0; i < m_nRealLightsNum && i < m_lstDynLights.Count(); i++)
		{
			m_lstDynLights[i]->m_Id = -1;
		}
	}
	else
	{
		IVisArea* pCameraVisArea = GetVisAreaFromPos(rCamera.GetPosition());

		for (int i = 0; i < m_lstDynLights.Count(); i++)
		{
			m_lstDynLights[i]->m_Id = -1;

			if (m_lstDynLights[i]->m_pOwner && m_lstDynLights[i]->m_pOwner->GetEntityVisArea())
			{
				// vis area lsource

				bool bIsVisible = false;
				if (m_lstDynLights[i]->m_Flags & DLF_DEFERRED_CUBEMAPS)
				{
					OBB obb(OBB::CreateOBBfromAABB(Matrix33(m_lstDynLights[i]->m_ObjMatrix), AABB(-m_lstDynLights[i]->m_ProbeExtents, m_lstDynLights[i]->m_ProbeExtents)));
					bIsVisible = passInfo.GetCamera().IsOBBVisible_F(m_lstDynLights[i]->m_Origin, obb);
				}
				else if (m_lstDynLights[i]->m_Flags & DLF_AREA_LIGHT)
				{
					// OBB test for area lights.
					Vec3 vBoxMax(m_lstDynLights[i]->m_fRadius, m_lstDynLights[i]->m_fRadius + m_lstDynLights[i]->m_fAreaWidth, m_lstDynLights[i]->m_fRadius + m_lstDynLights[i]->m_fAreaHeight);
					Vec3 vBoxMin(-0.1f, -(m_lstDynLights[i]->m_fRadius + m_lstDynLights[i]->m_fAreaWidth), -(m_lstDynLights[i]->m_fRadius + m_lstDynLights[i]->m_fAreaHeight));

					OBB obb(OBB::CreateOBBfromAABB(Matrix33(m_lstDynLights[i]->m_ObjMatrix), AABB(vBoxMin, vBoxMax)));
					bIsVisible = passInfo.GetCamera().IsOBBVisible_F(m_lstDynLights[i]->m_Origin, obb);
				}
				else
					bIsVisible = passInfo.GetCamera().IsSphereVisible_F(Sphere(m_lstDynLights[i]->m_Origin, m_lstDynLights[i]->m_fRadius));

				if (!bIsVisible)
				{
					FreeLightSourceComponents(m_lstDynLights[i]);
					m_lstDynLights.Delete(i);
					i--;
					continue; // invisible
				}

				// check if light is visible thru light area portal cameras
				if (CVisArea* pArea = (CVisArea*)m_lstDynLights[i]->m_pOwner->GetEntityVisArea())
					if (pArea->m_nRndFrameId == passInfo.GetFrameID() && pArea != (CVisArea*)pCameraVisArea)
					{
						int nCam = 0;
						for (; nCam < pArea->m_lstCurCamerasLen; nCam++)
							if (CVisArea::s_tmpCameras[pArea->m_lstCurCamerasIdx + nCam].IsSphereVisible_F(Sphere(m_lstDynLights[i]->m_Origin, m_lstDynLights[i]->m_fRadius)))
								break;

						if (nCam == pArea->m_lstCurCamerasLen)
						{
							FreeLightSourceComponents(m_lstDynLights[i]);
							m_lstDynLights.Delete(i);
							i--;
							continue; // invisible
						}
					}

				// check if lsource is in visible area
				ILightSource* pEnt = m_lstDynLights[i]->m_pOwner;
				if (!pEnt->IsLightAreasVisible() && pCameraVisArea != pEnt->GetEntityVisArea())
				{
					if (m_lstDynLights[i]->m_Flags & DLF_THIS_AREA_ONLY)
					{
						if (pEnt->GetEntityVisArea())
						{
							if (passInfo.GetFrameID() - pEnt->GetDrawFrame(0) > MAX_FRAME_ID_STEP_PER_FRAME)
							{
								FreeLightSourceComponents(m_lstDynLights[i]);
								m_lstDynLights.Delete(i);
								i--;
								continue;   // area invisible
							}
						}
						else
						{
							FreeLightSourceComponents(m_lstDynLights[i]);
							m_lstDynLights.Delete(i);
							i--;
							continue;   // area invisible
						}
					}
				}
			}
			else
			{
				// outdoor lsource
				if (!(m_lstDynLights[i]->m_Flags & DLF_DIRECTIONAL) && !m_lstDynLights[i]->m_pOwner->IsLightAreasVisible())
				{
					FreeLightSourceComponents(m_lstDynLights[i]);
					m_lstDynLights.Delete(i);
					i--;
					continue; // outdoor invisible
				}
			}

			if (m_lstDynLights[i]->m_Flags & DLF_DEFERRED_LIGHT)
			{
				SRenderLight* pLight = m_lstDynLights[i];
				CLightEntity* pLightEntity = (CLightEntity*)pLight->m_pOwner;

				if (passInfo.RenderShadows() && (pLight->m_Flags & DLF_CASTSHADOW_MAPS) && pLight->m_Id >= 0)
				{
					pLightEntity->UpdateGSMLightSourceShadowFrustum(passInfo);

					if (pLightEntity->GetShadowMapInfo())
					{
						pLight->m_pShadowMapFrustums = reinterpret_cast<ShadowMapFrustum**>(pLightEntity->GetShadowMapInfo()->pGSM);
					}
				}

				if (GetCVars()->e_DynamicLights)
				{
					AddLightToRenderer(*m_lstDynLights[i], 1.f, passInfo);
				}

				FreeLightSourceComponents(m_lstDynLights[i]);
				m_lstDynLights.Delete(i);
				i--;
				continue;
			}

			if (GetRenderer()->EF_IsFakeDLight(m_lstDynLights[i]))
			{
				// ignored by renderer
				m_lstDynLightsNoLight.Add(m_lstDynLights[i]);
				m_lstDynLights.Delete(i);
				i--;
				continue;
			}

			const int32 nMaxLightsNum = max((int)GetCVars()->e_DynamicLightsMaxCount, MAX_LIGHTS_NUM);
			if (i >= nMaxLightsNum)
			{
				// ignored by renderer

				assert(i >= nMaxLightsNum);

				if (i >= nMaxLightsNum && !bWarningPrinted && (passInfo.GetMainFrameID() & 7) == 7)
				{
					// no more sources can be accepted by renderer
					Warning("C3DEngine::PrepareLightSourcesForRendering: No more than %d real lsources allowed on the screen", (int)nMaxLightsNum);
					bWarningPrinted = true;
				}

				FreeLightSourceComponents(m_lstDynLights[i]);
				m_lstDynLights.Delete(i);
				i--;
				continue;
			}

			if (m_lstDynLights[i]->m_Flags & DLF_PROJECT
			    && m_lstDynLights[i]->m_fLightFrustumAngle < 90 // actual fov is twice bigger
			    && (m_lstDynLights[i]->m_pLightImage || m_lstDynLights[i]->m_pLightDynTexSource)
			    /*&& (m_lstDynLights[i]->m_pLightImage->GetFlags() & FT_FORCE_CUBEMAP)*/)
			{
				// prepare projector camera for frustum test
				m_arrLightProjFrustums.PreAllocate(i + 1, i + 1);
				CCamera& cam = m_arrLightProjFrustums[i];
				cam.SetPosition(m_lstDynLights[i]->m_Origin);
				//				Vec3 Angles(m_lstDynLights[i]->m_ProjAngles[1], 0, m_lstDynLights[i]->m_ProjAngles[2]+90.0f);
				//			cam.SetAngle(Angles);
				Matrix34 entMat = ((ILightSource*)(m_lstDynLights[i]->m_pOwner))->GetMatrix();
				Matrix33 matRot = Matrix33::CreateRotationVDir(entMat.GetColumn(0));
				cam.SetMatrixNoUpdate(Matrix34(matRot, m_lstDynLights[i]->m_Origin));
				cam.SetFrustum(1, 1, (m_lstDynLights[i]->m_fLightFrustumAngle * 2) / 180.0f * gf_PI, 0.1f, m_lstDynLights[i]->m_fRadius);

			}

		}

		m_nRealLightsNum = m_lstDynLights.Count();
		m_lstDynLights.AddList(m_lstDynLightsNoLight);

		m_lstDynLightsNoLight.Clear();
	}
}

void C3DEngine::PrepareLightSourcesForRendering_1(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// reset lists of lsource pointers in sectors
	if (passInfo.IsRecursivePass())
	{
		// do not delete lsources during recursion, becasue hmap lpasses are shared between levels
		for (int i = 0; i < m_nRealLightsNum && i < m_lstDynLights.Count(); i++)
		{
			m_lstDynLights[i]->m_Id = -1;
			GetRenderer()->EF_ADDDlight(m_lstDynLights[i], passInfo);
			assert(m_lstDynLights[i]->m_Id == i);
			if (m_lstDynLights[i]->m_Id != -1)
			{
				RegisterLightSourceInSectors(m_lstDynLights[i], passInfo);
			}
		}
	}
	else
	{
		for (int i = 0; i < m_nRealLightsNum && i < m_lstDynLights.Count(); i++)
		{
			m_lstDynLights[i]->m_Id = -1;

			if (m_lstDynLights[i]->m_Flags & DLF_SUN || GetCVars()->e_DynamicLightsConsistentSortOrder)
				CheckAddLight(m_lstDynLights[i], passInfo);

			if (m_lstDynLights[i]->m_fRadius >= 0.5f)
			{
				assert(m_lstDynLights[i]->m_fRadius >= 0.5f && !(m_lstDynLights[i]->m_Flags & DLF_FAKE));
				RegisterLightSourceInSectors(m_lstDynLights[i], passInfo);
			}
		}

		for (int i = m_nRealLightsNum; i < m_lstDynLights.Count(); i++)
		{
			// ignored by renderer
			m_lstDynLights[i]->m_Id = -1;
			GetRenderer()->EF_ADDDlight(m_lstDynLights[i], passInfo);
			assert(m_lstDynLights[i]->m_Id == -1);
		}
	}

	if (GetCVars()->e_DynamicLights == 2)
	{
		for (int i = 0; i < m_lstDynLights.Count(); i++)
		{
			SRenderLight* pL = m_lstDynLights[i];
			float fSize = 0.05f * (sinf(GetCurTimeSec() * 10.f) + 2.0f);
			DrawSphere(pL->m_Origin, fSize, pL->m_Color);
			IRenderAuxText::DrawLabelF(pL->m_Origin, 1.3f, "id=%d, rad=%.1f, vdr=%d", pL->m_Id, pL->m_fRadius, (int)(pL->m_pOwner ? pL->m_pOwner->m_ucViewDistRatio : 0));
		}
	}
}

void C3DEngine::InitShadowFrustums(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	for (int i = 0; i < m_nRealLightsNum && i < m_lstDynLights.Count(); i++)
	{
		SRenderLight* pLight = m_lstDynLights[i];
		CLightEntity* pLightEntity = (CLightEntity*)pLight->m_pOwner;

		if (passInfo.RenderShadows() && (pLight->m_Flags & DLF_CASTSHADOW_MAPS) && pLight->m_Id >= 0)
		{
			pLightEntity->UpdateGSMLightSourceShadowFrustum(passInfo);

			if (pLightEntity->GetShadowMapInfo())
			{
				pLight->m_pShadowMapFrustums = reinterpret_cast<ShadowMapFrustum**>(pLightEntity->GetShadowMapInfo()->pGSM);
			}
		}

		IMaterial* pMat = pLightEntity->GetMaterial();
		if (pMat)
			pLight->m_Shader = pMat->GetShaderItem();

		// update copy of light ion the renderer
		if (pLight->m_Id >= 0)
		{
			SRenderLight& light = passInfo.GetIRenderView()->GetDynamicLight(pLight->m_Id);

			assert(pLight->m_Id == light.m_Id);
			light.m_pShadowMapFrustums = pLight->m_pShadowMapFrustums;
			light.m_Shader = pLight->m_Shader;
			light.m_Flags = pLight->m_Flags;
		}
	}

	// add per object shadow frustums
	m_nCustomShadowFrustumCount = 0;
	if (passInfo.RenderShadows() && GetCVars()->e_ShadowsPerObject > 0)
	{
		ILightSource* pSun = GetSunEntity();
		if (pSun)
		{
			const uint nFrustumCount = m_lstPerObjectShadows.size();
			if (nFrustumCount > m_lstCustomShadowFrustums.size())
				m_lstCustomShadowFrustums.resize(nFrustumCount, nullptr);

			for (uint i = 0; i < nFrustumCount; ++i)
			{
				if (m_lstPerObjectShadows[i].pCaster)
				{
					ShadowMapFrustum*& pFr = m_lstCustomShadowFrustums[i];
					if (!pFr)
					{
						pFr = new ShadowMapFrustum();
						pFr->AddRef();
					}

					CLightEntity::ProcessPerObjectFrustum(pFr, &m_lstPerObjectShadows[i], pSun, passInfo);
					++m_nCustomShadowFrustumCount;
				}
			}
		}
	}

	//shadows frustums intersection test
	if (GetCVars()->e_ShadowsDebug == 4)
	{
		for (int i = 0; i < m_nRealLightsNum && i < m_lstDynLights.Count(); i++)
		{
			for (int j = (m_nRealLightsNum - 1); j >= (i + 1); j--)
			{

				SRenderLight* pLight = m_lstDynLights[i];
				CLightEntity* pLightEntity1 = (CLightEntity*)pLight->m_pOwner;

				pLight = m_lstDynLights[j];
				CLightEntity* pLightEntity2 = (CLightEntity*)pLight->m_pOwner;

				pLightEntity1->CheckFrustumsIntersect(pLightEntity2);
			}
		}
	}

	if (passInfo.RenderShadows())
		ResetCasterCombinationsCache();
}

void C3DEngine::AddPerObjectShadow(IShadowCaster* pCaster, float fConstBias, float fSlopeBias, float fJitter, const Vec3& vBBoxScale, uint nTexSize)
{
	SPerObjectShadow* pOS = GetPerObjectShadow(pCaster);
	if (!pOS)
		pOS = &m_lstPerObjectShadows.AddNew();

	const bool bRequiresObjTreeUpdate = pOS->pCaster != pCaster;

	pOS->pCaster = pCaster;
	pOS->fConstBias = fConstBias;
	pOS->fSlopeBias = fSlopeBias;
	pOS->fJitter = fJitter;
	pOS->vBBoxScale = vBBoxScale;
	pOS->nTexSize = nTexSize;

	if (bRequiresObjTreeUpdate)
	{
		CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);

		if (static_cast<IRenderNode*>(pCaster)->m_pOcNode)
		{
			static_cast<COctreeNode*>(static_cast<IRenderNode*>(pCaster)->m_pOcNode)->SetCompiled(IRenderNode::GetRenderNodeListId(pCaster->GetRenderNodeType()), false);
		}
	}
}

void C3DEngine::RemovePerObjectShadow(IShadowCaster* pCaster)
{
	SPerObjectShadow* pOS = GetPerObjectShadow(pCaster);
	if (pOS)
	{
		CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);

		size_t nIndex = (size_t)(pOS - m_lstPerObjectShadows.begin());
		m_lstPerObjectShadows.Delete(nIndex);

		if (static_cast<IRenderNode*>(pCaster)->m_pOcNode)
		{
			static_cast<COctreeNode*>(static_cast<IRenderNode*>(pCaster)->m_pOcNode)->SetCompiled(IRenderNode::GetRenderNodeListId(pCaster->GetRenderNodeType()), false);
		}
	}
}

struct SPerObjectShadow* C3DEngine::GetPerObjectShadow(IShadowCaster* pCaster)
{
	for (int i = 0; i < m_lstPerObjectShadows.Count(); ++i)
		if (m_lstPerObjectShadows[i].pCaster == pCaster)
			return &m_lstPerObjectShadows[i];

	return NULL;
}

void C3DEngine::GetCustomShadowMapFrustums(ShadowMapFrustum**& arrFrustums, int& nFrustumCount)
{
	if (!m_lstCustomShadowFrustums.empty())
	{
		arrFrustums = reinterpret_cast<ShadowMapFrustum**>(&m_lstCustomShadowFrustums[0]);
		nFrustumCount = m_nCustomShadowFrustumCount;
	}
	else
	{
		arrFrustums = nullptr;
		nFrustumCount = 0;
	}
}

void C3DEngine::FreeLightSourceComponents(SRenderLight* pLight, bool bDeleteLight)
{
	FUNCTION_PROFILER_3DENGINE;

	//  delete pLight->m_pProjCamera;
	//pLight->m_pProjCamera=0;

	//if(pLight->m_pShader)
	//		SAFE_RELEASE(pLight->m_pShader);

	if (bDeleteLight)
		SAFE_DELETE(pLight);
}

namespace
{
static inline bool CmpCastShadowFlag(const SRenderLight* p1, const SRenderLight* p2)
{
	// move sun first
	if ((p1->m_Flags & DLF_SUN) > (p2->m_Flags & DLF_SUN))
		return true;
	else if ((p1->m_Flags & DLF_SUN) < (p2->m_Flags & DLF_SUN))
		return false;

	// move shadow casters first
	if ((p1->m_Flags & DLF_CASTSHADOW_MAPS) > (p2->m_Flags & DLF_CASTSHADOW_MAPS))
		return true;
	else if ((p1->m_Flags & DLF_CASTSHADOW_MAPS) < (p2->m_Flags & DLF_CASTSHADOW_MAPS))
		return false;

	// get some sorting consistency for shadow casters
	if (p1->m_pOwner > p2->m_pOwner)
		return true;
	else if (p1->m_pOwner < p2->m_pOwner)
		return false;

	return false;
}
}

//////////////////////////////////////////////////////////////////////////

void C3DEngine::UpdateLightSources(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	m_LightVolumesMgr.Clear(passInfo);

	// sort by cast shadow flag
	if (passInfo.IsGeneralPass())
		std::sort(m_lstDynLights.begin(), m_lstDynLights.end(), CmpCastShadowFlag);

	// process lsources
	for (int i = 0; i < m_lstDynLights.Count(); i++)
	{
		SRenderLight* pLight = m_lstDynLights[i];

		bool bDeleteNow = GetRenderer()->EF_UpdateDLight(pLight);

		if (pLight->m_n3DEngineUpdateFrameID < (passInfo.GetMainFrameID() - 2) || pLight->m_Flags & DLF_DISABLED)
			bDeleteNow = true; // light is too old

		if (bDeleteNow)
		{
			// time is come to delete this lsource
			FreeLightSourceComponents(pLight);
			m_lstDynLights.Delete(i);
			i--;
			continue;
		}

		if (passInfo.IsGeneralPass())
			SetupLightScissors(m_lstDynLights[i], passInfo);
	}
}

//////////////////////////////////////////////////////////////////////////

bool IsSphereInsideHull(const SPlaneObject* pHullPlanes, int nPlanesNum, const Sphere& objSphere);

void C3DEngine::SetupLightScissors(SRenderLight* pLight, const SRenderingPassInfo& passInfo)
{
	const CCamera& pCam = passInfo.GetCamera();
	Vec3 vViewVec = pLight->m_Origin - pCam.GetPosition();
	float fDistToLS = vViewVec.GetLength();

	// Use max of width/height for area lights.
	float fMaxRadius = pLight->m_fRadius;
	if (pLight->m_Flags & DLF_AREA_LIGHT) // Use max for area lights.
		fMaxRadius += max(pLight->m_fAreaWidth, pLight->m_fAreaHeight);
	else if (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)
		fMaxRadius = pLight->m_ProbeExtents.len();  // This is not optimal for a box

	ITexture* pLightTexture = pLight->m_pLightImage ? pLight->m_pLightImage : pLight->m_pLightDynTexSource ? pLight->m_pLightDynTexSource->GetTexture() : NULL;
	bool bProjectiveLight = (pLight->m_Flags & DLF_PROJECT) && pLightTexture && !(pLightTexture->GetFlags() & FT_REPLICATE_TO_ALL_SIDES);
	bool bInsideLightVolume = fDistToLS <= fMaxRadius;
	if (bInsideLightVolume && !bProjectiveLight)
	{
		//optimization when we are inside light frustum
		pLight->m_sX = 0;
		pLight->m_sY = 0;
		pLight->m_sWidth = pCam.GetViewSurfaceX();
		pLight->m_sHeight = pCam.GetViewSurfaceZ();

		return;
	}

	Matrix44 mProj, mView;
	mProj = pCam.GetRenderProjectionMatrix();
	mView = pCam.GetRenderViewMatrix();

	Vec3 vCenter = pLight->m_Origin;
	float fRadius = fMaxRadius;

	const int nMaxVertsToProject = 10;
	int nVertsToProject = 4;
	Vec3 pBRectVertices[nMaxVertsToProject];

	Vec4 vCenterVS = Vec4(vCenter, 1) * mView;

	if (!bInsideLightVolume)
	{
		// Compute tangent planes
		float r = fRadius;
		float sq_r = r * r;

		Vec3 vLPosVS = Vec3(vCenterVS.x, vCenterVS.y, vCenterVS.z);
		float lx = vLPosVS.x;
		float ly = vLPosVS.y;
		float lz = vLPosVS.z;
		float sq_lx = lx * lx;
		float sq_ly = ly * ly;
		float sq_lz = lz * lz;

		// Compute left and right tangent planes to light sphere
		float sqrt_d = sqrt_tpl(max(sq_r * sq_lx - (sq_lx + sq_lz) * (sq_r - sq_lz), 0.0f));
		float nx = iszero(sq_lx + sq_lz) ? 1.0f : (r * lx + sqrt_d) / (sq_lx + sq_lz);
		float nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;

		Vec3 vTanLeft = Vec3(nx, 0, nz).normalized();

		nx = iszero(sq_lx + sq_lz) ? 1.0f : (r * lx - sqrt_d) / (sq_lx + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;
		Vec3 vTanRight = Vec3(nx, 0, nz).normalized();

		pBRectVertices[0] = vLPosVS - r * vTanLeft;
		pBRectVertices[1] = vLPosVS - r * vTanRight;

		// Compute top and bottom tangent planes to light sphere
		sqrt_d = sqrt_tpl(max(sq_r * sq_ly - (sq_ly + sq_lz) * (sq_r - sq_lz), 0.0f));
		float ny = iszero(sq_ly + sq_lz) ? 1.0f : (r * ly - sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanBottom = Vec3(0, ny, nz).normalized();

		ny = iszero(sq_ly + sq_lz) ? 1.0f : (r * ly + sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanTop = Vec3(0, ny, nz).normalized();

		pBRectVertices[2] = vLPosVS - r * vTanTop;
		pBRectVertices[3] = vLPosVS - r * vTanBottom;
	}

	if (bProjectiveLight)
	{

		// todo: improve/simplify projective case

		Vec3 vRight = pLight->m_ObjMatrix.GetColumn2();
		Vec3 vUp = -pLight->m_ObjMatrix.GetColumn1();
		Vec3 pDirFront = pLight->m_ObjMatrix.GetColumn0();
		pDirFront.NormalizeFast();

		// Cone radius
		float fConeAngleThreshold = 0.0f;
		float fConeRadiusScale = /*min(1.0f,*/ tan_tpl((pLight->m_fLightFrustumAngle + fConeAngleThreshold) * (gf_PI / 180.0f));  //);
		float fConeRadius = fRadius * fConeRadiusScale;

		Vec3 pDiagA = (vUp + vRight);
		float fDiagLen = 1.0f / pDiagA.GetLengthFast();
		pDiagA *= fDiagLen;

		Vec3 pDiagB = (vUp - vRight);
		pDiagB *= fDiagLen;

		float fPyramidBase = sqrt_tpl(fConeRadius * fConeRadius * 2.0f);
		pDirFront *= fRadius;

		Vec3 pEdgeA = (pDirFront + pDiagA * fPyramidBase);
		Vec3 pEdgeA2 = (pDirFront - pDiagA * fPyramidBase);
		Vec3 pEdgeB = (pDirFront + pDiagB * fPyramidBase);
		Vec3 pEdgeB2 = (pDirFront - pDiagB * fPyramidBase);

		uint32 nOffset = 4;

		// Check whether the camera is inside the extended bounding sphere that contains pyramid

#if 0 // Don't early out, is possible that sphere test results in reasonable bounds
		if (fDistToLS * fDistToLS <= fRadius * fRadius + fPyramidBase * fPyramidBase)
		{
			const uint32 nPlanes = 5;
			SPlaneObject planes[nPlanes];

			Vec3 NormA = pEdgeA.Cross(pEdgeB);
			Vec3 NormB = pEdgeB.Cross(pEdgeA2);
			Vec3 NormC = pEdgeA2.Cross(pEdgeB2);
			Vec3 NormD = pEdgeB2.Cross(pEdgeA);

			planes[0].plane = Plane::CreatePlane(NormA, vCenter);
			planes[1].plane = Plane::CreatePlane(NormB, vCenter);
			planes[2].plane = Plane::CreatePlane(NormC, vCenter);
			planes[3].plane = Plane::CreatePlane(NormD, vCenter);
			planes[4].plane = Plane::CreatePlane(-pDirFront, vCenter + pEdgeA);

			Sphere CamPosSphere(pCam.GetPosition(), 0.001f);

			if (IsSphereInsideHull(planes, nPlanes, CamPosSphere))
			{
				// we are inside light frustum
				pLight->m_sX = 0;
				pLight->m_sY = 0;
				pLight->m_sWidth = GetRenderer()->GetWidth();
				pLight->m_sHeight = GetRenderer()->GetHeight();

				return;
			}
		}
#endif

		// Put all pyramid vertices in view space
		Vec4 pPosVS = Vec4(vCenter, 1) * mView;
		pBRectVertices[nOffset++] = Vec3(pPosVS.x, pPosVS.y, pPosVS.z);
		pPosVS = Vec4(vCenter + pEdgeA, 1) * mView;
		pBRectVertices[nOffset++] = Vec3(pPosVS.x, pPosVS.y, pPosVS.z);
		pPosVS = Vec4(vCenter + pEdgeB, 1) * mView;
		pBRectVertices[nOffset++] = Vec3(pPosVS.x, pPosVS.y, pPosVS.z);
		pPosVS = Vec4(vCenter + pEdgeA2, 1) * mView;
		pBRectVertices[nOffset++] = Vec3(pPosVS.x, pPosVS.y, pPosVS.z);
		pPosVS = Vec4(vCenter + pEdgeB2, 1) * mView;
		pBRectVertices[nOffset++] = Vec3(pPosVS.x, pPosVS.y, pPosVS.z);

		nVertsToProject = nOffset;
	}

	Vec3 vPMin = Vec3(1, 1, 999999.0f);
	Vec2 vPMax = Vec2(0, 0);
	Vec2 vMin = Vec2(1, 1);
	Vec2 vMax = Vec2(0, 0);

	int nStart = 0;

	if (bInsideLightVolume)
	{
		nStart = 4;
		vMin = Vec2(0, 0);
		vMax = Vec2(1, 1);
	}

	if (GetCVars()->e_ScissorDebug)
		mView.Invert();

	// Project all vertices
	for (int i = nStart; i < nVertsToProject; i++)
	{
		if (GetCVars()->e_ScissorDebug)
		{
			if (GetRenderer()->GetIRenderAuxGeom() != NULL)
			{
				Vec4 pVertWS = Vec4(pBRectVertices[i], 1) * mView;
				Vec3 v = Vec3(pVertWS.x, pVertWS.y, pVertWS.z);
				GetRenderer()->GetIRenderAuxGeom()->DrawPoint(v, RGBA8(0xff, 0xff, 0xff, 0xff), 10);

				int32 nPrevVert = (i - 1) < 0 ? nVertsToProject - 1 : (i - 1);
				pVertWS = Vec4(pBRectVertices[nPrevVert], 1) * mView;
				Vec3 v2 = Vec3(pVertWS.x, pVertWS.y, pVertWS.z);
				GetRenderer()->GetIRenderAuxGeom()->DrawLine(v, RGBA8(0xff, 0xff, 0x0, 0xff), v2, RGBA8(0xff, 0xff, 0x0, 0xff), 3.0f);
			}
		}

		Vec4 vScreenPoint = Vec4(pBRectVertices[i], 1.0) * mProj;

		// convert to NDC
		vScreenPoint.w = max(vScreenPoint.w, 0.00000000000001f);
		vScreenPoint /= vScreenPoint.w;
		vScreenPoint.x = max(-1.0f, min(1.0f, vScreenPoint.x));
		vScreenPoint.y = max(-1.0f, min(1.0f, vScreenPoint.y));

		//output coords
		//generate viewport (x=0,y=0,height=1,width=1)
		Vec2 vWin;
		vWin.x = (1.0f + vScreenPoint.x) * 0.5f;
		vWin.y = (1.0f + vScreenPoint.y) * 0.5f;   //flip coords for y axis

		assert(vWin.x >= 0.0f && vWin.x <= 1.0f);
		assert(vWin.y >= 0.0f && vWin.y <= 1.0f);

		if (bProjectiveLight && i >= 4)
		{
			// Get light pyramid screen bounds
			vPMin.x = min(vPMin.x, vWin.x);
			vPMin.y = min(vPMin.y, vWin.y);
			vPMax.x = max(vPMax.x, vWin.x);
			vPMax.y = max(vPMax.y, vWin.y);

			vPMin.z = min(vPMin.z, vScreenPoint.z); // if pyramid intersects the nearplane, the test is unreliable. (requires proper clipping)
		}
		else
		{
			// Get light sphere screen bounds
			vMin.x = min(vMin.x, vWin.x);
			vMin.y = min(vMin.y, vWin.y);
			vMax.x = max(vMax.x, vWin.x);
			vMax.y = max(vMax.y, vWin.y);
		}
	}

	int iWidth = pCam.GetViewSurfaceX();
	int iHeight = pCam.GetViewSurfaceZ();
	float fWidth = (float)iWidth;
	float fHeight = (float)iHeight;

	if (bProjectiveLight)
	{
		vPMin.x = (float)__fsel(vPMin.z, vPMin.x, vMin.x); // Use sphere bounds if pyramid bounds are unreliable
		vPMin.y = (float)__fsel(vPMin.z, vPMin.y, vMin.y);
		vPMax.x = (float)__fsel(vPMin.z, vPMax.x, vMax.x);
		vPMax.y = (float)__fsel(vPMin.z, vPMax.y, vMax.y);

		// Clamp light pyramid bounds to light sphere screen bounds
		vMin.x = clamp_tpl<float>(vPMin.x, vMin.x, vMax.x);
		vMin.y = clamp_tpl<float>(vPMin.y, vMin.y, vMax.y);
		vMax.x = clamp_tpl<float>(vPMax.x, vMin.x, vMax.x);
		vMax.y = clamp_tpl<float>(vPMax.y, vMin.y, vMax.y);
	}

	pLight->m_sX = (short)(vMin.x * fWidth);
	pLight->m_sY = (short)((1.0f - vMax.y) * fHeight);
	pLight->m_sWidth = (short)ceilf((vMax.x - vMin.x) * fWidth);
	pLight->m_sHeight = (short)ceilf((vMax.y - vMin.y) * fHeight);

	// make sure we don't create a scissor rect out of bound (D3DError)
	pLight->m_sWidth = (pLight->m_sX + pLight->m_sWidth) > iWidth ? iWidth - pLight->m_sX : pLight->m_sWidth;
	pLight->m_sHeight = (pLight->m_sY + pLight->m_sHeight) > iHeight ? iHeight - pLight->m_sY : pLight->m_sHeight;

#if !defined(RELEASE)
	if (GetCVars()->e_ScissorDebug == 2)
	{
		// Render 2d areas additively on screen
		IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
		if (pAuxRenderer)
		{
			SAuxGeomRenderFlags oldRenderFlags = pAuxRenderer->GetRenderFlags();

			SAuxGeomRenderFlags newRenderFlags;
			newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
			newRenderFlags.SetAlphaBlendMode(e_AlphaAdditive);
			newRenderFlags.SetMode2D3DFlag(e_Mode2D);
			pAuxRenderer->SetRenderFlags(newRenderFlags);

			const float screenWidth = float(pAuxRenderer->GetCamera().GetViewSurfaceX());
			const float screenHeight = float(pAuxRenderer->GetCamera().GetViewSurfaceZ());

			// Calc resolve area
			const float left = pLight->m_sX / screenWidth;
			const float top = pLight->m_sY / screenHeight;
			const float right = (pLight->m_sX + pLight->m_sWidth) / screenWidth;
			const float bottom = (pLight->m_sY + pLight->m_sHeight) / screenHeight;

			// Render resolve area
			ColorB areaColor(50, 0, 50, 255);

			if (vPMin.z < 0.0f)
			{
				areaColor = ColorB(0, 100, 0, 255);
			}

			const uint vertexCount = 6;
			const Vec3 vert[vertexCount] = {
				Vec3(left,  top,    0.0f),
				Vec3(left,  bottom, 0.0f),
				Vec3(right, top,    0.0f),
				Vec3(left,  bottom, 0.0f),
				Vec3(right, bottom, 0.0f),
				Vec3(right, top,    0.0f)
			};
			pAuxRenderer->DrawTriangles(vert, vertexCount, areaColor);

			// Set previous Aux render flags back again
			pAuxRenderer->SetRenderFlags(oldRenderFlags);
		}
	}
#endif
}

void C3DEngine::RemoveEntityLightSources(IRenderNode* pEntity)
{
	for (int i = 0; i < m_lstDynLights.Count(); i++)
	{
		if (m_lstDynLights[i]->m_pOwner == pEntity)
		{
			FreeLightSourceComponents(m_lstDynLights[i]);
			m_lstDynLights.Delete(i);
			i--;
		}
	}

	for (int i = 0; i < m_lstStaticLights.Count(); i++)
	{
		if (m_lstStaticLights[i] == pEntity)
		{
			m_lstStaticLights.Delete(i);
			if (pEntity == m_pSun)
				m_pSun = NULL;
			i--;
		}
	}
}

float C3DEngine::GetLightAmountInRange(const Vec3& pPos, float fRange, bool bAccurate)
{
	static Vec3 pPrevPos(0, 0, 0);
	static float fPrevLightAmount = 0.0f, fPrevRange = 0.0f;

	CVisAreaManager* pVisAreaMan(GetVisAreaManager());
	if (!pVisAreaMan)
	{
		return 0.0f;
	}

	// Return cached result instead ( should also check if lights position/range changes - but how todo efficiently.. ? )
	if (fPrevLightAmount != 0.0f && fPrevRange == fRange && pPrevPos.GetSquaredDistance(pPos) < 0.25f)
	{
		return fPrevLightAmount;
	}

	CVisArea* pCurrVisArea(static_cast<CVisArea*>(Get3DEngine()->GetVisAreaFromPos(pPos)));

	Vec3 pAmb;
	if (pCurrVisArea)
		pAmb = pCurrVisArea->GetFinalAmbientColor();
	else
		pAmb = Get3DEngine()->GetSkyColor();

	// Take into account ambient lighting first
	float fAmbLength(pAmb.len2());
	if (fAmbLength > 1.0f)
	{
		// Normalize color (for consistently working with HDR/LDR)
		pAmb /= sqrt_tpl(fAmbLength);
	}

	float fLightAmount((pAmb.x + pAmb.y + pAmb.z) / 3.0f);

	for (int l(0); l < m_lstStaticLights.Count(); ++l)
	{
		SRenderLight& pLight = m_lstStaticLights[l]->GetLightProperties();

		if ((pLight.m_Flags & (DLF_FAKE | DLF_VOLUMETRIC_FOG_ONLY)))
		{
			// Fake and volumetric fog only light source not needed for light amount
			continue;
		}

		CVisArea* pLightVisArea(static_cast<CVisArea*>(m_lstStaticLights[l]->GetEntityVisArea()));

		if (pCurrVisArea != pLightVisArea && (pLight.m_Flags & DLF_THIS_AREA_ONLY))
		{
			// Not same vis area, skip
			continue;
		}

		float fAtten = 1.0f;
		float fDist = 0.0f;

		// Check if light comes from sun (only directional light source..), if true then no attenuation is required
		if (!(pLight.m_Flags & DLF_DIRECTIONAL))
		{
			fDist = pLight.m_Origin.GetSquaredDistance(pPos);
			fAtten = (1.0f - (fDist - fRange * fRange) / (pLight.m_fRadius * pLight.m_fRadius));

			// No need to proceed if attenuation coefficient is too small
			if (fAtten < 0.01f)
			{
				continue;
			}

			fAtten = clamp_tpl<float>(fAtten, 0.0f, 1.0f);
		}

		Vec3 pLightColor(pLight.m_Color.r, pLight.m_Color.g, pLight.m_Color.b);
		float fLightLength(pLightColor.len2());
		if (fLightLength > 1.0f)
		{
			// Normalize color (for consistently working with HDR/LDR)
			pLightColor /= sqrt_tpl(fLightLength);
		}

		float fCurrAmount(((pLightColor.x + pLightColor.y + pLightColor.z) / 3.0f) * fAtten);
		if (fCurrAmount < 0.01f)
		{
			// no need to proceed if current lighting amount is too small
			continue;
		}

		if (bAccurate && (pLight.m_Flags & DLF_CASTSHADOW_MAPS))
		{
			// Ray-cast to each of the 6 'edges' of a sphere, get an average of visibility and use it
			// to attenuate current light amount

			float fHitAtten(0);
			ray_hit pHit;

			const int nSamples = 6;
			Vec3 pOffsets[nSamples] =
			{
				Vec3(fRange,  0,       0),
				Vec3(-fRange, 0,       0),
				Vec3(0,       fRange,  0),
				Vec3(0,       -fRange, 0),
				Vec3(0,       0,       fRange),
				Vec3(0,       0,       -fRange),
			};

			const uint32 nEntQueryFlags(rwi_any_hit | rwi_stop_at_pierceable);
			const int32 nObjectTypes(ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid);

			for (int s(0); s < nSamples; ++s)
			{
				Vec3 pCurrPos(pPos - pOffsets[s]);
				Vec3 pDir(pLight.m_Origin - pCurrPos);

				if (GetPhysicalWorld()->RayWorldIntersection(pPos, pDir, nObjectTypes, nEntQueryFlags, &pHit, 1))
				{
					fHitAtten += 1.0f;
				}
			}

			// Average the n edges hits and attenuate light amount factor
			fHitAtten = 1.0f - (fHitAtten / (float) nSamples);
			fCurrAmount *= fHitAtten;
		}

		// Sum up results
		fLightAmount += fCurrAmount;
	}

	fLightAmount = clamp_tpl<float>(fLightAmount, 0.0f, 1.0f);

	// Cache results
	pPrevPos = pPos;
	fPrevLightAmount = fLightAmount;
	fPrevRange = fRange;

	return fLightAmount;
}

ILightSource* C3DEngine::GetSunEntity()
{
	return m_pSun;
}

PodArray<struct ILightSource*>* C3DEngine::GetAffectingLights(const AABB& bbox, bool bAllowSun, const SRenderingPassInfo& passInfo)
{
	PodArray<struct ILightSource*>& lstAffectingLights = m_tmpLstAffectingLights;
	lstAffectingLights.Clear();

	// fill temporary list
	for (int i = 0; i < m_lstStaticLights.Count(); i++)
	{
		CLightEntity* pLightEntity = (CLightEntity*)m_lstStaticLights[i];
		SRenderLight* pLight = &pLightEntity->GetLightProperties();
		if (pLight->m_Flags & DLF_SUN)
			continue;

		ILightSource* p = (ILightSource*)pLightEntity;
		lstAffectingLights.Add(p);
	}

	// search for same cached list
	for (int nListId = 0; nListId < m_lstAffectingLightsCombinations.Count(); nListId++)
	{
		PodArray<struct ILightSource*>* pCachedList = m_lstAffectingLightsCombinations[nListId];
		if (pCachedList->Count() == lstAffectingLights.Count() &&
		    !memcmp(pCachedList->GetElements(), lstAffectingLights.GetElements(), lstAffectingLights.Count() * sizeof(ILightSource*)))
			return pCachedList;
	}

	// if not found in cache - create new
	PodArray<struct ILightSource*>* pNewList = new PodArray<struct ILightSource*>;
	pNewList->AddList(lstAffectingLights);
	m_lstAffectingLightsCombinations.Add(pNewList);

	return pNewList;
}

void C3DEngine::UregisterLightFromAccessabilityCache(ILightSource* pLight)
{
	// search for same cached list
	for (int nListId = 0; nListId < m_lstAffectingLightsCombinations.Count(); nListId++)
	{
		PodArray<struct ILightSource*>* pCachedList = m_lstAffectingLightsCombinations[nListId];
		pCachedList->Delete(pLight);
	}
}

void C3DEngine::OnCasterDeleted(IShadowCaster* pCaster)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	{
		// make sure pointer to object will not be used somewhere in the renderer
		PodArray<SRenderLight*>* pLigts = GetDynamicLightSources();
		for (int i = 0; i < pLigts->Count(); i++)
		{
			CLightEntity* pLigt = (CLightEntity*)(pLigts->GetAt(i)->m_pOwner);
			if (pLigt && (pLigt != m_pSun))
				pLigt->OnCasterDeleted(pCaster);
		}

		if (m_pSun)
			m_pSun->OnCasterDeleted(pCaster);

		// remove from per object shadows list
		RemovePerObjectShadow(pCaster);
	}
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::CheckAddLight(SRenderLight* pLight, const SRenderingPassInfo& passInfo)
{
	if (pLight->m_Id < 0)
	{
		GetRenderer()->EF_ADDDlight(pLight, passInfo);
		assert(pLight->m_Id >= 0);
	}
}

///////////////////////////////////////////////////////////////////////////////
float C3DEngine::GetLightAmount(SRenderLight* pLight, const AABB& objBox)
{
	// find amount of light
	float fDist = sqrt_tpl(Distance::Point_AABBSq(pLight->m_Origin, objBox));
	float fLightAttenuation = (pLight->m_Flags & DLF_DIRECTIONAL) ? 1.f : 1.f - (fDist) / (pLight->m_fRadius);
	if (fLightAttenuation < 0)
		fLightAttenuation = 0;

	float fLightAmount =
	  (pLight->m_Color.r + pLight->m_Color.g + pLight->m_Color.b) * 0.233f +
	  (pLight->GetSpecularMult()) * 0.1f;

	return fLightAmount * fLightAttenuation;
}
