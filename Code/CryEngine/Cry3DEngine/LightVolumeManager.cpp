// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LightVolumeManager.h"
#include "ClipVolumeManager.h"

void CLightVolumesMgr::Init()
{
	m_bUpdateLightVolumes = false;
	m_pLightVolsInfo.reserve(LV_MAX_COUNT);
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		m_pLightVolumes[i].reserve(LV_MAX_COUNT);
	memset(m_nWorldCells, 0, sizeof(m_nWorldCells));
	memset(m_pWorldLightCells, 0, sizeof(m_pWorldLightCells));
}

void CLightVolumesMgr::Reset()
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		stl::free_container(m_pLightVolumes[i]);

	m_bUpdateLightVolumes = false;
	memset(m_nWorldCells, 0, sizeof(m_nWorldCells));
	memset(m_pWorldLightCells, 0, sizeof(m_pWorldLightCells));
}

uint16 CLightVolumesMgr::RegisterVolume(const Vec3& vPos, f32 fRadius, uint8 nClipVolumeRef, const SRenderingPassInfo& passInfo)
{
	IF ((m_bUpdateLightVolumes & (m_pLightVolsInfo.size() < LV_MAX_COUNT)) && fRadius < 256.0f, 1)
	{
		FUNCTION_PROFILER_3DENGINE;

		int32 nPosx = (int32)(floorf(vPos.x * LV_CELL_RSIZEX));
		int32 nPosy = (int32)(floorf(vPos.y * LV_CELL_RSIZEY));
		int32 nPosz = (int32)(floorf(vPos.z * LV_CELL_RSIZEZ));

		// Check if world cell has any light volume, else add new one
		uint16 nHashIndex = GetWorldHashBucketKey(nPosx, nPosy, nPosz);
		uint16* pCurrentVolumeID = &m_nWorldCells[nHashIndex];

		while (*pCurrentVolumeID != 0)
		{
			SLightVolInfo& sVolInfo = m_pLightVolsInfo[*pCurrentVolumeID - 1];

			int32 nVolumePosx = (int32)(floorf(sVolInfo.vVolume.x * LV_CELL_RSIZEX));
			int32 nVolumePosy = (int32)(floorf(sVolInfo.vVolume.y * LV_CELL_RSIZEY));
			int32 nVolumePosz = (int32)(floorf(sVolInfo.vVolume.z * LV_CELL_RSIZEZ));

			if (nPosx == nVolumePosx &&
			    nPosy == nVolumePosy &&
			    nPosz == nVolumePosz &&
			    nClipVolumeRef == sVolInfo.nClipVolumeID)
			{
				return (uint16) * pCurrentVolumeID;
			}

			pCurrentVolumeID = &sVolInfo.nNextVolume;
		}

		// create new volume
		size_t nIndex = ~0;
		SLightVolInfo* pLightVolInfo = ::new(m_pLightVolsInfo.push_back_new(nIndex))SLightVolInfo(vPos, fRadius, nClipVolumeRef);
		*pCurrentVolumeID = static_cast<uint16>(nIndex + 1);

		return *pCurrentVolumeID;
	}

	return 0;
}

void CLightVolumesMgr::RegisterLight(const SRenderLight& pDL, uint32 nLightID, const SRenderingPassInfo& passInfo)
{
	IF (nLightID < LV_MAX_LIGHTS && (m_bUpdateLightVolumes & !(pDL.m_Flags & LV_DLF_LIGHTVOLUMES_MASK)), 1)
	{
		FUNCTION_PROFILER_3DENGINE;

		int32 nMiny = (int32)(floorf((pDL.m_Origin.y - pDL.m_fRadius) * LV_LIGHT_CELL_R_SIZE));
		int32 nMaxy = (int32)(floorf((pDL.m_Origin.y + pDL.m_fRadius) * LV_LIGHT_CELL_R_SIZE));
		int32 nMinx = (int32)(floorf((pDL.m_Origin.x - pDL.m_fRadius) * LV_LIGHT_CELL_R_SIZE));
		int32 nMaxx = (int32)(floorf((pDL.m_Origin.x + pDL.m_fRadius) * LV_LIGHT_CELL_R_SIZE));

		// Register light into all cells touched by light radius
		for (int32 y = nMiny, ymax = nMaxy; y <= ymax; ++y)
		{
			for (int32 x = nMinx, xmax = nMaxx; x <= xmax; ++x)
			{
				SLightCell& lightCell = m_pWorldLightCells[GetWorldHashBucketKey(x, y, 1, LV_LIGHTS_WORLD_BUCKET_SIZE)];
				CryPrefetch(&lightCell);
				lightCell.nLightID[lightCell.nLightCount] = nLightID;
				lightCell.nLightCount = (lightCell.nLightCount + 1) & (LV_CELL_MAX_LIGHTS - 1);
			}
		}
	}
}

void CLightVolumesMgr::AddLight(const SRenderLight& pLight, const SLightVolInfo* __restrict pVolInfo, SLightVolume& pVolume)
{
	// Check for clip volume
	if (pLight.m_nStencilRef[0] == pVolInfo->nClipVolumeID || pLight.m_nStencilRef[1] == pVolInfo->nClipVolumeID ||
	    pLight.m_nStencilRef[0] == CClipVolumeManager::AffectsEverythingStencilRef)
	{
		const Vec3 position = pLight.m_Origin;
		const float radius = pLight.m_fRadius;
		const Vec4& vVolume = pVolInfo->vVolume;
		const f32 fDimCheck = (f32) __fsel(radius - vVolume.w * 0.1f, 1.0f, 0.0f);  //light radius not more than 10x smaller than volume radius
		const f32 fOverlapCheck = (f32) __fsel(sqr(vVolume.x - position.x) + sqr(vVolume.y - position.y) + sqr(vVolume.z - position.z) - sqr(vVolume.w + radius), 0.0f, 1.0f);// touches volumes
		if (fDimCheck * fOverlapCheck)
		{
			float fAttenuationBulbSize = pLight.m_fAttenuationBulbSize;
			Vec3 lightColor = pLight.m_Color.toVec3();

			// Adjust light intensity so that the intended brightness is reached 1 meter from the light's surface
			IF (!(pLight.m_Flags & (DLF_AREA_LIGHT | DLF_AMBIENT)), 1)
			{
				fAttenuationBulbSize = max(fAttenuationBulbSize, 0.001f);

				// Solve I * 1 / (1 + d/lightsize)^2 = 1
				float intensityMul = 1.0f + 1.0f / fAttenuationBulbSize;
				intensityMul *= intensityMul;
				lightColor *= intensityMul;
			}

			pVolume.pData.push_back();
			SLightVolume::SLightData& lightData = pVolume.pData[pVolume.pData.size() - 1];
			lightData.position = position;
			lightData.radius = radius;
			lightData.color = lightColor;
			lightData.buldRadius = fAttenuationBulbSize;
			if (pLight.m_Flags & DLF_PROJECT)
			{
				lightData.projectorDirection = pLight.m_ObjMatrix.GetColumn0();
				lightData.projectorCosAngle = cos_tpl(DEG2RAD(pLight.m_fLightFrustumAngle));
			}
			else
			{
				lightData.projectorDirection = Vec3(ZERO);
				lightData.projectorCosAngle = 0.0f;
			}
		}
	}
}

void CLightVolumesMgr::Update(const SRenderingPassInfo& passInfo)
{
	if (!m_bUpdateLightVolumes || m_pLightVolsInfo.empty() || !passInfo.IsGeneralPass())
		return;

	FUNCTION_PROFILER_3DENGINE;
	m_pLightVolsInfo.CoalesceMemory();
	const uint32 nLightCount = passInfo.GetIRenderView()->GetLightsCount(eDLT_DeferredLight);

	uint32 nLightVols = m_pLightVolsInfo.size();
	LightVolumeVector& lightVols = m_pLightVolumes[passInfo.ThreadID()];
	lightVols.resize(nLightVols);

	if (!nLightCount)
	{
		for (uint32 v = 0; v < nLightVols; ++v)
			lightVols[v].pData.resize(0);

		return;
	}

	uint8 nLightProcessed[LV_MAX_LIGHTS] = { 0 };

	for (uint32 v = 0; v < nLightVols; ++v)
	{
		const Vec4* __restrict vBVol = &m_pLightVolsInfo[v].vVolume;
		int32 nMiny = (int32)(floorf((vBVol->y - vBVol->w) * LV_LIGHT_CELL_R_SIZE));
		int32 nMaxy = (int32)(floorf((vBVol->y + vBVol->w) * LV_LIGHT_CELL_R_SIZE));
		int32 nMinx = (int32)(floorf((vBVol->x - vBVol->w) * LV_LIGHT_CELL_R_SIZE));
		int32 nMaxx = (int32)(floorf((vBVol->x + vBVol->w) * LV_LIGHT_CELL_R_SIZE));

		lightVols[v].pData.resize(0);

		// Loop through active light cells touching bounding volume (~avg 2 cells)
		for (int32 y = nMiny, ymax = nMaxy; y <= ymax; ++y)
		{
			for (int32 x = nMinx, xmax = nMaxx; x <= xmax; ++x)
			{
				const SLightCell& lightCell = m_pWorldLightCells[GetWorldHashBucketKey(x, y, 1, LV_LIGHTS_WORLD_BUCKET_SIZE)];
				CryPrefetch(&lightCell);

				const SRenderLight& pFirstDL = passInfo.GetIRenderView()->GetLight(eDLT_DeferredLight, lightCell.nLightID[0]);
				CryPrefetch(&pFirstDL);
				CryPrefetch(&pFirstDL.m_ObjMatrix);
				for (uint32 l = 0; (l < lightCell.nLightCount) & (lightVols[v].pData.size() < LIGHTVOLUME_MAXLIGHTS); ++l)
				{
					const int32 nLightId = lightCell.nLightID[l];
					const SRenderLight& pDL = passInfo.GetIRenderView()->GetLight(eDLT_DeferredLight, nLightId);
					const int32 nNextLightId = lightCell.nLightID[(l + 1) & (LIGHTVOLUME_MAXLIGHTS - 1)];
					const SRenderLight& pNextDL = passInfo.GetIRenderView()->GetLight(eDLT_DeferredLight, nNextLightId);
					CryPrefetch(&pNextDL);
					CryPrefetch(&pNextDL.m_ObjMatrix);

					IF (nLightProcessed[nLightId] != v + 1, 1)
					{
						nLightProcessed[nLightId] = v + 1;
						AddLight(pDL, &m_pLightVolsInfo[v], lightVols[v]);
					}
				}
			}
		}
	}
}

void CLightVolumesMgr::Clear(const SRenderingPassInfo& passInfo)
{
	m_bUpdateLightVolumes = false;
	if (GetCVars()->e_LightVolumes && passInfo.IsGeneralPass() && GetCVars()->e_DynamicLights)
	{
		memset(m_nWorldCells, 0, sizeof(m_nWorldCells));
		memset(m_pWorldLightCells, 0, sizeof(m_pWorldLightCells));
		m_pLightVolsInfo.clear();
		m_bUpdateLightVolumes = (GetCVars()->e_LightVolumes == 1) ? true : false;
	}
}

void CLightVolumesMgr::GetLightVolumes(threadID nThreadID, SLightVolume*& pLightVols, uint32& nNumVols)
{
	pLightVols = 0;
	nNumVols = 0;
	if (GetCVars()->e_LightVolumes == 1 && GetCVars()->e_DynamicLights && !m_pLightVolumes[nThreadID].empty())
	{
		pLightVols = &m_pLightVolumes[nThreadID][0];
		nNumVols = m_pLightVolumes[nThreadID].size();
	}
}

void C3DEngine::GetLightVolumes(threadID nThreadID, SLightVolume*& pLightVols, uint32& nNumVols)
{
	m_LightVolumesMgr.GetLightVolumes(nThreadID, pLightVols, nNumVols);
}

void CLightVolumesMgr::DrawDebug(const SRenderingPassInfo& passInfo)
{
#ifndef _RELEASE
	IRenderer* pRenderer = GetRenderer();
	IRenderAuxGeom* pAuxGeom = GetRenderer()->GetIRenderAuxGeom();
	if (!pAuxGeom || !passInfo.IsGeneralPass())
		return;

	ColorF cWhite = ColorF(1, 1, 1, 1);
	ColorF cBad = ColorF(1.0f, 0.0, 0.0f, 1.0f);
	ColorF cWarning = ColorF(1.0f, 1.0, 0.0f, 1.0f);
	ColorF cGood = ColorF(0.0f, 0.5, 1.0f, 1.0f);
	ColorF cSingleCell = ColorF(0.0f, 1.0, 0.0f, 1.0f);

	const uint32 nLightVols = m_pLightVolsInfo.size();
	LightVolumeVector& lightVols = m_pLightVolumes[passInfo.ThreadID()];
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	float fYLine = 8.0f, fYStep = 20.0f;
	IRenderAuxText::Draw2dLabel(8.0f, fYLine += fYStep, 2.0f, (float*)&cWhite.r, false, "Light Volumes Info (count %d)", nLightVols);

	for (uint32 v = 0; v < nLightVols; ++v)  // draw each light volume
	{
		SLightVolume& lv = lightVols[v];
		SLightVolInfo& lvInfo = m_pLightVolsInfo[v];

		ColorF& cCol = (lv.pData.size() >= 10) ? cBad : ((lv.pData.size() >= 5) ? cWarning : cGood);
		const Vec3 vPos = Vec3(lvInfo.vVolume.x, lvInfo.vVolume.y, lvInfo.vVolume.z);
		const float fCamDistSq = (vPos - vCamPos).len2();
		cCol.a = max(0.25f, min(1.0f, 1024.0f / (fCamDistSq + 1e-6f)));

		IRenderAuxText::DrawLabelExF(vPos, 1.3f, (float*)&cCol.r, true, true, "Id: %d\nPos: %.2f %.2f %.2f\nRadius: %.2f\nLights: %d\nOutLights: %d",
		                       v, vPos.x, vPos.y, vPos.z, lvInfo.vVolume.w, lv.pData.size(), (*(int32*)&lvInfo.vVolume.w) & (1 << 31) ? 1 : 0);

		if (GetCVars()->e_LightVolumesDebug == 2)
		{
			const float fSideSize = 0.707f * sqrtf(lvInfo.vVolume.w * lvInfo.vVolume.w * 2);
			pAuxGeom->DrawAABB(AABB(vPos - Vec3(fSideSize), vPos + Vec3(fSideSize)), false, cCol, eBBD_Faceted);
		}

		if (GetCVars()->e_LightVolumesDebug == 3)
		{
			cBad.a = 1.0f;
			const Vec3 vCellPos = Vec3(floorf((lvInfo.vVolume.x) * LV_CELL_RSIZEX) * LV_CELL_SIZEX,
			                           floorf((lvInfo.vVolume.y) * LV_CELL_RSIZEY) * LV_CELL_SIZEY,
			                           floorf((lvInfo.vVolume.z) * LV_CELL_RSIZEZ) * LV_CELL_SIZEZ);

			const Vec3 vMin = vCellPos;
			const Vec3 vMax = vMin + Vec3(LV_CELL_SIZEX, LV_CELL_SIZEY, LV_CELL_SIZEZ);
			pAuxGeom->DrawAABB(AABB(vMin, vMax), false, cBad, eBBD_Faceted);
		}
	}
#endif
}
