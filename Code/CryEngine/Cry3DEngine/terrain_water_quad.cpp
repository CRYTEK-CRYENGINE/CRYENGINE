// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_water_quad.cpp
//  Version:     v1.00
//  Created:     28/8/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Create and draw terrain water geometry (screen space grid, cycle buffers)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain_water.h"
#include "PolygonClipContext.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "VisAreas.h"

namespace
{
static const float OCEAN_FOG_DENSITY_MINIMUM = 0.0001f;

std::tuple<uint32, uint32> GenerateOceanSurfaceVertices(
  SVF_P3F_C4B_T2F* pDestVertices,
  vtx_idx* pDestIndices,
  int32 nScrGridSizeY,
  float fRcpScrGridSizeY,
  int32 nScrGridSizeX,
  float fRcpScrGridSizeX,
  bool bUseTessHW,
  int32 swathWidth)
{
	if (!pDestVertices || !pDestIndices)
	{
		return std::make_tuple(0u, 0u);
	}

	SVF_P3F_C4B_T2F* pVertices = pDestVertices;
	vtx_idx* pIndices = pDestIndices;
	uint32 verticesCount = 0;
	uint32 indicesCount = 0;

	SVF_P3F_C4B_T2F tmp;
	Vec3 vv;
	vv.z = 0;

	// Grid vertex generation
	for (int32 y(0); y < nScrGridSizeY; ++y)
	{
		vv.y = (float)y * fRcpScrGridSizeY;// + fRcpScrGridSize;

		for (int32 x(0); x < nScrGridSizeX; ++x)
		{
			// vert 1
			vv.x = (float)x * fRcpScrGridSizeX;// + fRcpScrGridSize;

			// store in z edges information
			float fx = fabs((vv.x) * 2.0f - 1.0f);
			float fy = fabs((vv.y) * 2.0f - 1.0f);
			//float fEdgeDisplace = sqrt_tpl(fx*fx + fy * fy);//max(fx, fy);
			float fEdgeDisplace = max(fx, fy);
			//sqrt_tpl(fx*fx + fy * fy);
			vv.z = fEdgeDisplace; //!((y==0 ||y == nScrGridSize-1) || (x==0 || x == nScrGridSize-1));

			tmp.xyz = vv;
			*pVertices = tmp;
			++pVertices;
			++verticesCount;
		}
	}

	auto AddIndex = [&pIndices, &indicesCount](vtx_idx index)
	{
		*pIndices = index;
		++pIndices;
		++indicesCount;
	};

	if (bUseTessHW)
	{
		// Normal approach
		int32 nIndex = 0;
		for (int32 y(0); y < nScrGridSizeY - 1; ++y)
		{
			for (int32 x(0); x < nScrGridSizeX - 1; ++x, ++nIndex)
			{
				AddIndex(nScrGridSizeX * y + x);
				AddIndex(nScrGridSizeX * y + x + 1);
				AddIndex(nScrGridSizeX * (y + 1) + x);

				AddIndex(nScrGridSizeX * (y + 1) + x);
				AddIndex(nScrGridSizeX * y + x + 1);
				AddIndex(nScrGridSizeX * (y + 1) + x + 1);

				//m_pMeshIndices.Add( nIndex );
				//m_pMeshIndices.Add( nIndex + 1);
				//m_pMeshIndices.Add( nIndex + nScrGridSizeX);

				//m_pMeshIndices.Add( nIndex + nScrGridSizeX);
				//m_pMeshIndices.Add( nIndex + 1);
				//m_pMeshIndices.Add( nIndex + nScrGridSizeX + 1);
			}
		}
	}
	else
	{
		// Grid index generation

		if (swathWidth <= 0)
		{
			// Normal approach
			int32 nIndex = 0;
			for (int32 y(0); y < nScrGridSizeY - 1; ++y)
			{
				for (int32 x(0); x < nScrGridSizeX; ++x, ++nIndex)
				{
					AddIndex(nIndex);
					AddIndex(nIndex + nScrGridSizeX);
				}

				if (nScrGridSizeY - 2 > y)
				{
					AddIndex(nIndex + nScrGridSizeY - 1);
					AddIndex(nIndex);
				}
			}
		}
		else
		{
			// Boustrophedonic walk
			//
			//  0  1  2  3  4
			//  5  6  7  8  9
			// 10 11 12 13 14
			// 15 16 17 18 19
			//
			// Should generate the following indices
			// 0 5 1 6 2 7 3 8 4 9 9 14 14 9 13 8 12 7 11 6 10 5 5 10 10 15 11 16 12 17 13 18 14 19
			//

			int32 startX = 0, endX = swathWidth - 1;

			do
			{

				for (int32 y(0); y < nScrGridSizeY - 1; y += 2)
				{
					// Forward
					for (int32 x(startX); x <= endX; ++x)
					{
						AddIndex(y * nScrGridSizeX + x);
						AddIndex((y + 1) * nScrGridSizeX + x);
					}

					// Can we go backwards?
					if (y + 2 < nScrGridSizeY)
					{
						// Restart strip by duplicating last and first of next strip
						AddIndex((y + 1) * nScrGridSizeX + endX);
						AddIndex((y + 2) * nScrGridSizeX + endX);

						//Backward
						for (int32 x(endX); x >= startX; --x)
						{
							AddIndex((y + 2) * nScrGridSizeX + x);
							AddIndex((y + 1) * nScrGridSizeX + x);
						}

						// Restart strip
						if (y + 2 == nScrGridSizeY - 1 && endX < nScrGridSizeX - 1)
						{
							if (endX < nScrGridSizeX - 1)
							{
								// Need to restart at the top of the next column
								AddIndex((nScrGridSizeY - 1) * nScrGridSizeX + startX);
								AddIndex(endX);
							}
						}
						else
						{
							AddIndex((y + 1) * nScrGridSizeX + startX);
							AddIndex((y + 2) * nScrGridSizeX + startX);
						}
					}
					else
					{
						// We can restart to next column
						if (endX < nScrGridSizeX - 1)
						{
							// Restart strip for next swath
							AddIndex((nScrGridSizeY - 1) * nScrGridSizeX + endX);
							AddIndex(endX);
						}
					}
				}

				startX = endX;
				endX = startX + swathWidth - 1;

				if (endX >= nScrGridSizeX) endX = nScrGridSizeX - 1;

			}
			while (startX < nScrGridSizeX - 1);

		}
	}

	return std::make_tuple(verticesCount, indicesCount);
}
}

ITimer* COcean::m_pOceanTimer = 0;
CREWaterOcean* COcean::m_pOceanRE = 0;
uint32 COcean::m_nVisiblePixelsCount = ~0;

COcean::COcean(IMaterial* pMat)
{
	m_pBottomCapRenderMesh = 0;

	memset(m_fRECustomData, 0, sizeof(m_fRECustomData));
	memset(m_fREOceanBottomCustomData, 0, sizeof(m_fREOceanBottomCustomData));

	m_pMaterial = pMat;
	m_fLastFov = 0;
	m_fLastVisibleFrameTime = 0.0f;

	m_pShaderOcclusionQuery = GetRenderer() ?
	                          GetRenderer()->EF_LoadShader("OcclusionTest", 0) : nullptr;
	memset(m_pREOcclusionQueries, 0, sizeof(m_pREOcclusionQueries));

	m_nLastVisibleFrameId = 0;

	m_pBottomCapMaterial = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Water/WaterOceanBottom", false);
	m_pFogIntoMat = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/OceanInto", false);
	m_pFogOutofMat = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/OceanOutof", false);
	m_pFogIntoMatLowSpec = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/OceanIntoLowSpec", false);
	m_pFogOutofMatLowSpec = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/OceanOutofLowSpec", false);

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		if (GetRenderer())
		{
			m_pWVRE[i] = static_cast<CREWaterVolume*>(GetRenderer()->EF_CreateRE(eDATA_WaterVolume));
			if (m_pWVRE[i])
			{
				m_pWVRE[i]->m_drawWaterSurface = false;
				m_pWVRE[i]->m_pParams = &m_wvParams[i];
				m_pWVRE[i]->m_pOceanParams = &m_wvoParams[i];
			}
		}
		else
		{
			m_pWVRE[i] = nullptr;
		}
	}

	m_pOceanRE = GetRenderer() ?
	             static_cast<CREWaterOcean*>(GetRenderer()->EF_CreateRE(eDATA_WaterOcean)) : nullptr;

	m_nVertsCount = 0;
	m_nIndicesCount = 0;

	m_bOceanFFT = false;
}

COcean::~COcean()
{
	for (int32 x = 0; x < CYCLE_BUFFERS_NUM; ++x)
	{
		if (m_pREOcclusionQueries[x])
			m_pREOcclusionQueries[x]->Release(true);
	}

	m_pBottomCapRenderMesh = NULL;

	SAFE_RELEASE(m_pOceanRE);
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		SAFE_RELEASE(m_pWVRE[i]);
}

int32 COcean::GetMemoryUsage()
{
	int32 nSize = 0;

	nSize += sizeofVector(m_pBottomCapVerts);
	nSize += sizeofVector(m_pBottomCapIndices);

	return nSize;
}

void COcean::Update(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	C3DEngine* p3DEngine = (C3DEngine*)Get3DEngine();
	IRenderer* pRenderer = GetRenderer();
	if (passInfo.IsRecursivePass() || !passInfo.RenderWaterOcean() || !m_pMaterial)
		return;

	const CCamera& rCamera = passInfo.GetCamera();
	int32 nFillThreadID = passInfo.ThreadID();
	uint32 nBufID = passInfo.GetFrameID() % CYCLE_BUFFERS_NUM;

	Vec3 vCamPos = rCamera.GetPosition();
	float fWaterLevel = p3DEngine->GetWaterLevel();

	// No hardware FFT support
	m_bOceanFFT = false;
	if (GetCVars()->e_WaterOceanFFT && pRenderer->EF_GetShaderQuality(eST_Water) >= eSQ_High)
	{
		m_bOceanFFT = true;
	}

	if (vCamPos.z < fWaterLevel)
	{
		// if camera is in indoors and lower than ocean level
		// and exit portals are higher than ocean level - skip ocean rendering
		CVisArea* pVisArea = (CVisArea*)p3DEngine->GetVisAreaFromPos(vCamPos);
		if (pVisArea && !pVisArea->IsPortal())
		{
			for (int32 i = 0; i < pVisArea->m_lstConnections.Count(); i++)
			{
				if (pVisArea->m_lstConnections[i]->IsConnectedToOutdoor() && pVisArea->m_lstConnections[i]->m_boxArea.min.z < fWaterLevel)
					break; // there is portal making ocean visible

				if (i == pVisArea->m_lstConnections.Count())
					return; // ocean surface is not visible
			}
		}
	}

	bool bWaterVisible = IsVisible(passInfo);
	float _fWaterPlaneSize = rCamera.GetFarPlane();

	// Check if water surface occluded
	if (fabs(m_fLastFov - rCamera.GetFov()) < 0.01f && GetCVars()->e_HwOcclusionCullingWater && passInfo.IsGeneralPass())
	{
		AABB boxOcean(Vec3(vCamPos.x - _fWaterPlaneSize, vCamPos.y - _fWaterPlaneSize, fWaterLevel),
		              Vec3(vCamPos.x + _fWaterPlaneSize, vCamPos.y + _fWaterPlaneSize, fWaterLevel));

		if ((!GetVisAreaManager()->IsOceanVisible() && rCamera.IsAABBVisible_EM(boxOcean)) ||
		    (GetVisAreaManager()->IsOceanVisible() && rCamera.IsAABBVisible_E(boxOcean)))
		{
			// make element if not ready
			if (!m_pREOcclusionQueries[nBufID])
			{
				m_pREOcclusionQueries[nBufID] = (CREOcclusionQuery*)GetRenderer()->EF_CreateRE(eDATA_OcclusionQuery);
				m_pREOcclusionQueries[nBufID]->m_pRMBox = (CRenderMesh*)GetObjManager()->GetRenderMeshBox();
			}

			// get last test result
			//	if((m_pREOcclusionQueries[nFillThreadID][nBufID]->m_nCheckFrame - passInfo.GetFrameID())<2)
			{
				COcean::m_nVisiblePixelsCount = m_pREOcclusionQueries[nBufID]->m_nVisSamples;
				if (COcean::m_nVisiblePixelsCount > 16)
				{
					m_nLastVisibleFrameId = passInfo.GetFrameID();
					bWaterVisible = true;
				}
			}

			// request new test
			m_pREOcclusionQueries[nBufID]->m_vBoxMin(boxOcean.min.x, boxOcean.min.y, boxOcean.min.z - 1.0f);
			m_pREOcclusionQueries[nBufID]->m_vBoxMax(boxOcean.max.x, boxOcean.max.y, boxOcean.max.z);

			m_pREOcclusionQueries[nBufID]->mfReadResult_Try(COcean::m_nVisiblePixelsCount);
			if (!m_pREOcclusionQueries[nBufID]->m_nDrawFrame || m_pREOcclusionQueries[nBufID]->HasSucceeded())
			{
				SShaderItem shItem(m_pShaderOcclusionQuery);
				CRenderObject* pObj = GetIdentityCRenderObject(passInfo);
				if (!pObj)
					return;
				passInfo.GetIRenderView()->AddRenderObject(m_pREOcclusionQueries[nBufID], shItem, pObj, passInfo, EFSLIST_WATER_VOLUMES, 0);
			}
		}
	}
	else
	{
		m_nLastVisibleFrameId = passInfo.GetFrameID();
		bWaterVisible = true;
	}

	if (bWaterVisible || vCamPos.z < fWaterLevel)
	{
		m_p3DEngine->SetOceanRenderFlags(OCR_OCEANVOLUME_VISIBLE);

		// lazy mesh creation
		if (bWaterVisible)
		{
			Create(passInfo);
		}
	}
}

void COcean::Create(const SRenderingPassInfo& passInfo)
{
	// Calculate water geometry and update vertex buffers
	int32 nScrGridSizeX = 20 * GetCVars()->e_WaterTessellationAmount;

	nScrGridSizeX = (GetCVars()->e_WaterTessellationAmountX && GetCVars()->e_WaterTessellationAmountY) ? GetCVars()->e_WaterTessellationAmountX : nScrGridSizeX;
	int32 nScrGridSizeY = (GetCVars()->e_WaterTessellationAmountX && GetCVars()->e_WaterTessellationAmountY) ? GetCVars()->e_WaterTessellationAmountY : nScrGridSizeX;

	// Store permanently the swath width
	static int32 swathWidth = 0;
	static bool bUsingFFT = false;
	static bool bUseTessHW = false;
	bool bUseWaterTessHW;
	GetRenderer()->EF_Query(EFQ_WaterTessellation, bUseWaterTessHW);

	if (!bUseWaterTessHW && m_bOceanFFT)
		nScrGridSizeX = nScrGridSizeY = 20 * 10; // for hi/very specs - use maximum tessellation

	// swath width must be equal or shorter than nScrGridSizeX, otherwise it causes corrupted indices.
	const int32 currentSwathWidth = min(nScrGridSizeX, GetCVars()->e_WaterTessellationSwathWidth);

	// Generate screen space grid
	if ((m_bOceanFFT && bUsingFFT != m_bOceanFFT) || bUseTessHW != bUseWaterTessHW || swathWidth != currentSwathWidth || !m_nVertsCount || !m_nIndicesCount || nScrGridSizeX * nScrGridSizeY != m_nPrevGridDim)
	{
		const CCamera& pCam = passInfo.GetCamera();

		m_nPrevGridDim = nScrGridSizeX * nScrGridSizeY;
		m_nVertsCount = 0;
		m_nIndicesCount = 0;

		bUsingFFT = m_bOceanFFT;
		bUseTessHW = bUseWaterTessHW;
		// Update the swath width
		swathWidth = currentSwathWidth;

		// Render ocean with screen space tessellation
		const int32 nScreenY = pCam.GetViewSurfaceX();
		const int32 nScreenX = pCam.GetViewSurfaceZ();
		if (!nScreenY || !nScreenX)
			return;

		const float fRcpScrGridSizeX = 1.0f / ((float) nScrGridSizeX - 1);
		const float fRcpScrGridSizeY = 1.0f / ((float) nScrGridSizeY - 1);

		SVF_P3F_C4B_T2F* pReqVertices = nullptr;
		vtx_idx* pReqIndices = nullptr;
		const uint32 reqVerticesCount = nScrGridSizeX * nScrGridSizeY;
		const uint32 reqIndicesCount  = nScrGridSizeX * nScrGridSizeY * 6;
		const bool result = m_pOceanRE->RequestVerticesBuffer(&pReqVertices, (uint8**)&pReqIndices, reqVerticesCount, reqIndicesCount, sizeof(vtx_idx));

		if (!result)
		{
			return;
		}

		auto counts = GenerateOceanSurfaceVertices(pReqVertices, pReqIndices, nScrGridSizeY, fRcpScrGridSizeY, nScrGridSizeX, fRcpScrGridSizeX, bUseTessHW, swathWidth);

		m_nVertsCount = std::get<0>(counts);
		m_nIndicesCount = std::get<1>(counts);
		CRY_ASSERT(m_nVertsCount <= reqVerticesCount);
		CRY_ASSERT(m_nIndicesCount <= reqIndicesCount);

		m_pOceanRE->SubmitVerticesBuffer(m_nVertsCount, m_nIndicesCount, sizeof(vtx_idx), pReqVertices, (uint8*)pReqIndices);
	}
}

void COcean::Render(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// if reaches render stage - means ocean is visible

	C3DEngine* p3DEngine = (C3DEngine*)Get3DEngine();
	IRenderer* pRenderer(GetRenderer());

	int32 nBufID = (passInfo.GetFrameID() & 1);
	Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	float fWaterLevel = p3DEngine->GetWaterLevel();

	const int fillThreadID = passInfo.ThreadID();

	CRenderObject* pObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObject)
		return;
	pObject->SetMatrix(Matrix34::CreateIdentity(), passInfo);
	pObject->m_pRenderNode = this;

	m_fLastFov = passInfo.GetCamera().GetFov();

	// test for multiple lights and shadows support

	SRenderObjData* pOD = pObject->GetObjData();

	m_Camera = passInfo.GetCamera();
	pObject->m_fAlpha = 1.f;//m_fWaterTranspRatio;

	m_fRECustomData[0] = p3DEngine->m_oceanWindDirection;
	m_fRECustomData[1] = p3DEngine->m_oceanWindSpeed;
	m_fRECustomData[2] = 0.0f; // used to be m_oceanWavesSpeed
	m_fRECustomData[3] = p3DEngine->m_oceanWavesAmount;
	m_fRECustomData[4] = p3DEngine->m_oceanWavesSize;

	sincos_tpl(p3DEngine->m_oceanWindDirection, &m_fRECustomData[6], &m_fRECustomData[5]);
	m_fRECustomData[7] = fWaterLevel;
	m_fRECustomData[8] = m_fRECustomData[9] = m_fRECustomData[10] = m_fRECustomData[11] = 0.0f;

	bool isFastpath = GetCVars()->e_WaterOcean == 2;
	bool bUsingMergedFog = false;

	{
		Vec3 camPos(passInfo.GetCamera().GetPosition());

		// Check if we outside water volume - we can enable fast path with merged fog version
		if (camPos.z - fWaterLevel >= p3DEngine->m_oceanWavesSize)
		{
			Vec3 cFinalFogColor = gEnv->p3DEngine->GetSunColor().CompMul(m_p3DEngine->m_oceanFogColor);
			Vec4 vFogParams = Vec4(cFinalFogColor, max(OCEAN_FOG_DENSITY_MINIMUM, m_p3DEngine->m_oceanFogDensity) * 1.44269502f);// log2(e) = 1.44269502

			m_fRECustomData[8] = vFogParams.x;
			m_fRECustomData[9] = vFogParams.y;
			m_fRECustomData[10] = vFogParams.z;
			m_fRECustomData[11] = vFogParams.w;
			if (isFastpath)
				bUsingMergedFog = true;
		}
	}

	{
		CMatInfo* pMatInfo = (CMatInfo*)(IMaterial*)m_pMaterial;
		float fInstanceDistance = GetTerrain()->GetDistanceToSectorWithWater();
		pMatInfo->PrecacheMaterial(fInstanceDistance, 0, false);
	}

	if (!GetCVars()->e_WaterOceanFFT || !m_bOceanFFT)
	{
		m_pOceanRE->m_oceanParam[fillThreadID].bWaterOceanFFT = false;
	}
	else
	{
		m_pOceanRE->m_oceanParam[fillThreadID].bWaterOceanFFT = m_bOceanFFT;
	}

	pObject->m_pCurrMaterial = m_pMaterial;
	SShaderItem& shaderItem(m_pMaterial->GetShaderItem(0));
	m_pOceanRE->m_CustomData = &m_fRECustomData[0];
	passInfo.GetIRenderView()->AddRenderObject(m_pOceanRE, shaderItem, pObject, passInfo, EFSLIST_WATER, 0);

	if (GetCVars()->e_WaterOceanBottom)
		RenderBottomCap(passInfo);

	if (!bUsingMergedFog)
		RenderFog(passInfo);
}

void COcean::RenderBottomCap(const SRenderingPassInfo& passInfo)
{
	C3DEngine* p3DEngine = (C3DEngine*)Get3DEngine();

	const CCamera& pCam = passInfo.GetCamera();
	Vec3 vCamPos = pCam.GetPosition();

	// Render ocean with screen space tessellation
	const int32 nScreenY = pCam.GetViewSurfaceX();
	const int32 nScreenX = pCam.GetViewSurfaceZ();
	if (!nScreenY || !nScreenX)
		return;

	// Calculate water geometry and update vertex buffers
	int32 nScrGridSize = 5;
	float fRcpScrGridSize = 1.0f / (float) nScrGridSize;

	if (!m_pBottomCapVerts.Count() || !m_pBottomCapIndices.Count() || nScrGridSize * nScrGridSize != m_pBottomCapVerts.Count())
	{

		m_pBottomCapVerts.Clear();
		m_pBottomCapIndices.Clear();

		SVF_P3F_C4B_T2F tmp;
		Vec3 vv;
		vv.z = 0;

		// Grid vertex generation
		for (int32 y(0); y < nScrGridSize; ++y)
		{
			vv.y = (float) y * fRcpScrGridSize + fRcpScrGridSize;
			for (int32 x(0); x < nScrGridSize; ++x)
			{
				vv.x = (float) x * fRcpScrGridSize + fRcpScrGridSize;
				tmp.xyz = vv;
				m_pBottomCapVerts.Add(tmp);
			}
		}

		// Normal approach
		int32 nIndex = 0;
		for (int32 y(0); y < nScrGridSize - 1; ++y)
		{
			for (int32 x(0); x < nScrGridSize; ++x, ++nIndex)
			{
				m_pBottomCapIndices.Add(nIndex);
				m_pBottomCapIndices.Add(nIndex + nScrGridSize);
			}

			if (nScrGridSize - 2 > y)
			{
				m_pBottomCapIndices.Add(nIndex + nScrGridSize - 1);
				m_pBottomCapIndices.Add(nIndex);
			}
		}

		m_pBottomCapRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
		  m_pBottomCapVerts.GetElements(),
		  m_pBottomCapVerts.Count(),
		  EDefaultInputLayouts::P3F_C4B_T2F,
		  m_pBottomCapIndices.GetElements(),
		  m_pBottomCapIndices.Count(),
		  prtTriangleStrip,
		  "OceanBottomGrid", "OceanBottomGrid",
		  eRMT_Static);

		m_pBottomCapRenderMesh->SetChunk(m_pBottomCapMaterial, 0, m_pBottomCapVerts.Count(), 0, m_pBottomCapIndices.Count(), 1.0f);
	}

	CRenderObject* pObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObject)
		return;
	pObject->SetMatrix(Matrix34::CreateIdentity(), passInfo);
	pObject->m_pRenderNode = this;

	// make distance to water level near to zero
	m_pBottomCapRenderMesh->SetBBox(vCamPos, vCamPos);

	m_Camera = passInfo.GetCamera();
	pObject->m_fAlpha = 1.f;

	m_pBottomCapRenderMesh->AddRenderElements(m_pBottomCapMaterial, pObject, passInfo, EFSLIST_GENERAL, 0);
}

void COcean::RenderFog(const SRenderingPassInfo& passInfo)
{
	if (!GetCVars()->e_Fog || !GetCVars()->e_FogVolumes)
		return;

	IRenderer* pRenderer(GetRenderer());
	C3DEngine* p3DEngine(Get3DEngine());

	const int fillThreadID = passInfo.ThreadID();

	CRenderObject* pROVol(passInfo.GetIRenderView()->AllocateTemporaryRenderObject());
	if (!pROVol)
		return;

	bool isFastpath = GetCVars()->e_WaterOcean == 2;
	bool isLowSpec(GetCVars()->e_ObjQuality == CONFIG_LOW_SPEC || isFastpath);
	if (pROVol && m_pWVRE[fillThreadID] && ((!isLowSpec && m_pFogIntoMat && m_pFogOutofMat) ||
	                                        (isLowSpec && m_pFogIntoMatLowSpec && m_pFogOutofMatLowSpec)))
	{
		Vec3 camPos(passInfo.GetCamera().GetPosition());
		float waterLevel(p3DEngine->GetWaterLevel());
		Vec3 planeOrigin(camPos.x, camPos.y, waterLevel);

		// fill water volume param structure
		m_wvParams[fillThreadID].m_center = planeOrigin;
		m_wvParams[fillThreadID].m_fogPlane.Set(Vec3(0, 0, 1), -waterLevel);

		float distCamToFogPlane(camPos.z + m_wvParams[fillThreadID].m_fogPlane.d);
		m_wvParams[fillThreadID].m_viewerCloseToWaterPlane = (distCamToFogPlane) < 0.5f;
		m_wvParams[fillThreadID].m_viewerInsideVolume = distCamToFogPlane < 0.00f;
		m_wvParams[fillThreadID].m_viewerCloseToWaterVolume = true;

		if (!isFastpath || (distCamToFogPlane < p3DEngine->m_oceanWavesSize))
		{
			if (isLowSpec)
			{
				m_wvParams[fillThreadID].m_fogColor = m_p3DEngine->m_oceanFogColor;
				m_wvParams[fillThreadID].m_fogDensity = m_p3DEngine->m_oceanFogDensity;

				m_wvoParams[fillThreadID].m_fogColor = Vec3(0, 0, 0);        // not needed for low spec
				m_wvoParams[fillThreadID].m_fogColorShallow = Vec3(0, 0, 0); // not needed for low spec
				m_wvoParams[fillThreadID].m_fogDensity = 0;                  // not needed for low spec

				m_pWVRE[fillThreadID]->m_pOceanParams = 0;
			}
			else
			{
				m_wvParams[fillThreadID].m_fogColor = Vec3(0, 0, 0); // not needed, we set ocean specific params below
				m_wvParams[fillThreadID].m_fogDensity = 0;           // not needed, we set ocean specific params below

				m_wvoParams[fillThreadID].m_fogColor = m_p3DEngine->m_oceanFogColor;
				m_wvoParams[fillThreadID].m_fogColorShallow = m_p3DEngine->m_oceanFogColorShallow;
				m_wvoParams[fillThreadID].m_fogDensity = max(OCEAN_FOG_DENSITY_MINIMUM, m_p3DEngine->m_oceanFogDensity);

				m_pWVRE[fillThreadID]->m_pOceanParams = &m_wvoParams[fillThreadID];
			}

			// tessellate plane
			float planeSize(2.0f * passInfo.GetCamera().GetFarPlane());
			size_t subDivSize(min(64, 1 + (int32) (planeSize / 512.0f)));
			if (isFastpath)
				subDivSize = 4;

			size_t numSubDivVerts((subDivSize + 1) * (subDivSize + 1));

			if (m_wvVertices[fillThreadID].size() != numSubDivVerts)
			{
				m_wvVertices[fillThreadID].resize(numSubDivVerts);
				m_wvParams[fillThreadID].m_pVertices = &m_wvVertices[fillThreadID][0];
				m_wvParams[fillThreadID].m_numVertices = m_wvVertices[fillThreadID].size();

				m_wvIndices[fillThreadID].resize(subDivSize * subDivSize * 6);
				m_wvParams[fillThreadID].m_pIndices = &m_wvIndices[fillThreadID][0];
				m_wvParams[fillThreadID].m_numIndices = m_wvIndices[fillThreadID].size();

				size_t ind(0);
				for (uint32 y(0); y < subDivSize; ++y)
				{
					for (uint32 x(0); x < subDivSize; ++x, ind += 6)
					{
						m_wvIndices[fillThreadID][ind + 0] = static_cast<uint16>((y) * (subDivSize + 1) + (x));
						m_wvIndices[fillThreadID][ind + 1] = static_cast<uint16>((y) * (subDivSize + 1) + (x + 1));
						m_wvIndices[fillThreadID][ind + 2] = static_cast<uint16>((y + 1) * (subDivSize + 1) + (x + 1));

						m_wvIndices[fillThreadID][ind + 3] = static_cast<uint16>((y) * (subDivSize + 1) + (x));
						m_wvIndices[fillThreadID][ind + 4] = static_cast<uint16>((y + 1) * (subDivSize + 1) + (x + 1));
						m_wvIndices[fillThreadID][ind + 5] = static_cast<uint16>((y + 1) * (subDivSize + 1) + (x));
					}
				}
			}
			{
				float xyDelta(2.0f * planeSize / (float) subDivSize);
				float zDelta(waterLevel - camPos.z);

				size_t ind(0);
				float yd(-planeSize);
				for (uint32 y(0); y <= subDivSize; ++y, yd += xyDelta)
				{
					float xd(-planeSize);
					for (uint32 x(0); x <= subDivSize; ++x, xd += xyDelta, ++ind)
					{
						m_wvVertices[fillThreadID][ind].xyz = Vec3(xd, yd, zDelta);
						m_wvVertices[fillThreadID][ind].st = Vec2(0, 0);
					}
				}
			}

			// fill in data for render object
			pROVol->SetMatrix(Matrix34::CreateIdentity(), passInfo);
			pROVol->m_fSort = 0;

			auto pMaterial =
			  m_wvParams[fillThreadID].m_viewerInsideVolume
			  ? (isLowSpec ? m_pFogOutofMatLowSpec.get() : m_pFogOutofMat.get())
			  : (isLowSpec ? m_pFogIntoMatLowSpec.get() : m_pFogIntoMat.get());

			pROVol->m_pCurrMaterial = pMaterial;

			// get shader item
			SShaderItem& shaderItem(pMaterial->GetShaderItem(0));

			// add to renderer
			passInfo.GetIRenderView()->AddRenderObject(m_pWVRE[fillThreadID], shaderItem, pROVol, passInfo, EFSLIST_WATER_VOLUMES, distCamToFogPlane < -0.1f);
		}
	}
}

bool COcean::IsVisible(const SRenderingPassInfo& passInfo)
{
	if (abs(m_nLastVisibleFrameId - passInfo.GetFrameID()) <= 2)
		m_fLastVisibleFrameTime = 0.0f;

	ITimer* pTimer(gEnv->pTimer);
	m_fLastVisibleFrameTime += gEnv->pTimer->GetFrameTime();

	if (m_fLastVisibleFrameTime > 2.0f)                                 // at least 2 seconds
		return (abs(m_nLastVisibleFrameId - passInfo.GetFrameID()) < 64); // and at least 64 frames

	return true; // keep water visible for a couple frames - or at least 1 second - minimizes popping during fast camera movement
}

void COcean::SetTimer(ITimer* pTimer)
{
	assert(pTimer);
	m_pOceanTimer = pTimer;
}

float COcean::GetWave(const Vec3& pPos, int32 nFrameID)
{
	// todo: optimize...

	IRenderer* pRenderer(GetRenderer());
	if (!pRenderer)
		return 0.0f;

	EShaderQuality nShaderQuality = pRenderer->EF_GetShaderQuality(eST_Water);

	if (!m_pOceanTimer || nShaderQuality < eSQ_High)
		return 0.0f;

	// Return height - matching computation on GPU

	C3DEngine* p3DEngine(Get3DEngine());

	bool bOceanFFT = false;
	if (GetCVars()->e_WaterOceanFFT && nShaderQuality >= eSQ_High)
		bOceanFFT = true;

	if (bOceanFFT)
	{
		Vec4 pDispPos = Vec4(0, 0, 0, 0);

		if (m_pOceanRE)
		{
			// Get height from FFT grid

			Vec4* pGridFFT = m_pOceanRE->GetDisplaceGrid();
			if (!pGridFFT)
				return 0.0f;

			// match scales used in shader
			float fScaleX = pPos.x * 0.0125f * p3DEngine->m_oceanWavesAmount * 1.25f;
			float fScaleY = pPos.y * 0.0125f * p3DEngine->m_oceanWavesAmount * 1.25f;

			float fu = fScaleX * 64.0f;
			float fv = fScaleY * 64.0f;
			int32 u1 = ((int32)fu) & 63;
			int32 v1 = ((int32)fv) & 63;
			int32 u2 = (u1 + 1) & 63;
			int32 v2 = (v1 + 1) & 63;

			// Fractional parts
			float fracu = fu - floorf(fu);
			float fracv = fv - floorf(fv);

			// Get weights
			float w1 = (1 - fracu) * (1 - fracv);
			float w2 = fracu * (1 - fracv);
			float w3 = (1 - fracu) * fracv;
			float w4 = fracu * fracv;

			Vec4 h1 = pGridFFT[u1 + v1 * 64];
			Vec4 h2 = pGridFFT[u2 + v1 * 64];
			Vec4 h3 = pGridFFT[u1 + v2 * 64];
			Vec4 h4 = pGridFFT[u2 + v2 * 64];

			// scale and sum the four heights
			pDispPos = h1 * w1 + h2 * w2 + h3 * w3 + h4 * w4;
		}

		// match scales used in shader
		return pDispPos.z * 0.06f * p3DEngine->m_oceanWavesSize;
	}

	// constant to scale down values a bit
	const float fAnimAmplitudeScale = 1.0f / 5.0f;

	static int32 s_nFrameID = 0;
	static Vec3 vFlowDir = Vec3(0, 0, 0);
	static Vec4 vFrequencies = Vec4(0, 0, 0, 0);
	static Vec4 vPhases = Vec4(0, 0, 0, 0);
	static Vec4 vAmplitudes = Vec4(0, 0, 0, 0);

	// Update per-frame data
	if (s_nFrameID != nFrameID)
	{
		sincos_tpl(p3DEngine->m_oceanWindDirection, &vFlowDir.y, &vFlowDir.x);
		vFrequencies = Vec4(0.233f, 0.455f, 0.6135f, -0.1467f) * 5.0f;
		vPhases = Vec4(0.1f, 0.159f, 0.557f, 0.2199f) * p3DEngine->m_oceanWavesAmount;
		vAmplitudes = Vec4(1.0f, 0.5f, 0.25f, 0.5f) * p3DEngine->m_oceanWavesSize;

		s_nFrameID = nFrameID;
	}

	float fPhase = sqrt_tpl(pPos.x * pPos.x + pPos.y * pPos.y);
	Vec4 vCosPhase = vPhases * (fPhase + pPos.x);

	Vec4 vWaveFreq = vFrequencies * m_pOceanTimer->GetCurrTime();

	Vec4 vCosWave = Vec4(cos_tpl(vWaveFreq.x * vFlowDir.x + vCosPhase.x),
	                     cos_tpl(vWaveFreq.y * vFlowDir.x + vCosPhase.y),
	                     cos_tpl(vWaveFreq.z * vFlowDir.x + vCosPhase.z),
	                     cos_tpl(vWaveFreq.w * vFlowDir.x + vCosPhase.w));

	Vec4 vSinPhase = vPhases * (fPhase + pPos.y);
	Vec4 vSinWave = Vec4(sin_tpl(vWaveFreq.x * vFlowDir.y + vSinPhase.x),
	                     sin_tpl(vWaveFreq.y * vFlowDir.y + vSinPhase.y),
	                     sin_tpl(vWaveFreq.z * vFlowDir.y + vSinPhase.z),
	                     sin_tpl(vWaveFreq.w * vFlowDir.y + vSinPhase.w));

	return (vCosWave.Dot(vAmplitudes) + vSinWave.Dot(vAmplitudes)) * fAnimAmplitudeScale;
}

uint32 COcean::GetVisiblePixelsCount()
{
	return m_nVisiblePixelsCount;
}

void COcean::OffsetPosition(const Vec3& delta)
{
}

void COcean::FillBBox(AABB& aabb)
{
	aabb = COcean::GetBBox();
}

EERType COcean::GetRenderNodeType()
{
	return eERType_WaterVolume;
}

Vec3 COcean::GetPos(bool) const
{
	return Vec3(0, 0, 0);
}

IMaterial* COcean::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
