// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   WaterWavesRenderNode.cpp
//  Version:     v1.00
//  Created:     20/09/2005 by Tiago Sousa.
//  Compilers:   Visual Studio.NET 2003
//  Description: Draws/Creates waves in the world
// -------------------------------------------------------------------------
//  History:
//
//  Todo:
//   - physicalization / lod
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WaterWaveRenderNode.h"
#include "VisAreas.h"
#include "MatMan.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryMath/Cry_Geo.h>

#define MAX_WAVE_DIST_UPDATE 5000.0f

namespace
{
// Some wave utilities

Vec3 VecMin(const Vec3& v1, const Vec3& v2)
{
	return Vec3(std::min(v1.x, v2.x), std::min(v1.y, v2.y), std::min(v1.z, v2.z));
}

Vec3 VecMax(const Vec3& v1, const Vec3& v2)
{
	return Vec3(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z));
}

float sfrand()
{
	return cry_random(-1.0f, 1.0f);
}

};

//////////////////////////////////////////////////////////////////////////
CWaterWaveRenderNode::CWaterWaveRenderNode() :
	m_nID(~0),
	m_pMaterial(0),
	m_pRenderMesh(0),
	m_pMin(ZERO),
	m_pMax(ZERO),
	m_pSerializeParams(0),
	m_pOrigPos(ZERO),
	m_fWaveKey(-1.0f)
{
	GetInstCount(GetRenderNodeType())++;

	memset(m_fRECustomData, 0, sizeof(m_fRECustomData));
	m_fCurrTerrainDepth = 0;

	// put default material on
	m_pMaterial = GetMatMan()->LoadMaterial("Materials/Ocean/water_wave", false);
}

//////////////////////////////////////////////////////////////////////////
CWaterWaveRenderNode::~CWaterWaveRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	SAFE_DELETE(m_pSerializeParams);

	GetWaterWaveManager()->Unregister(this);

	Get3DEngine()->FreeRenderNodeState(this);
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::ComputeMinMax(const Vec3* pVertices, unsigned int nVertexCount)
{

	m_fWaveKey = 0.0f;
	m_pMin = Vec3(0);
	m_pMax = Vec3(0);

	Matrix34 pInvWorldTM(m_pWorldTM);
	pInvWorldTM.Invert();

	// Each sector has 4 vertices
	for (uint32 i(0); i < nVertexCount; i += 4)
	{
		// Set edges (in object space)
		Vec3 p0(pVertices[i + 0]);
		p0 = pInvWorldTM.TransformPoint(p0);

		Vec3 p1(pVertices[i + 1]);
		p1 = pInvWorldTM.TransformPoint(p1);

		Vec3 p2(pVertices[i + 2]);
		p2 = pInvWorldTM.TransformPoint(p2);

		Vec3 p3(pVertices[i + 3]);
		p3 = pInvWorldTM.TransformPoint(p3);

		const float fKeyMultiplier = 1000.0f / 3.0f;

		// Compute wave key, for faster instantiation
		m_fWaveKey += ceilf((p0.x + p0.y + p0.z) * fKeyMultiplier);
		m_fWaveKey += ceilf((p1.x + p1.y + p1.z) * fKeyMultiplier);
		m_fWaveKey += ceilf((p2.x + p2.y + p2.z) * fKeyMultiplier);
		m_fWaveKey += ceilf((p3.x + p3.y + p3.z) * fKeyMultiplier);

		// Store min/max (for faster bounding box computation)

		// Get min/max
		m_pMin = VecMin(m_pMin, p0);
		m_pMax = VecMax(m_pMax, p0);
		m_pMin = VecMin(m_pMin, p1);
		m_pMax = VecMax(m_pMax, p1);
		m_pMin = VecMin(m_pMin, p2);
		m_pMax = VecMax(m_pMax, p2);
		m_pMin = VecMin(m_pMin, p3);
		m_pMax = VecMax(m_pMax, p3);
	}

	const int nSectors = nVertexCount / 4;
	m_fWaveKey /= (float) nSectors;

	// Update bounding info
	UpdateBoundingBox();

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::UpdateBoundingBox()
{

	m_WSBBox.max = m_pMax;
	m_WSBBox.min = m_pMin;

	// Transform back to world space
	m_WSBBox.SetTransformedAABB(m_pWorldTM, m_WSBBox);

	m_pParams.m_pPos = m_WSBBox.GetCenter();

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::Create(uint64 nID, const Vec3* pVertices, unsigned int nVertexCount, const Vec2& pUVScale, const Matrix34& pWorldTM)
{

	if (nVertexCount < 4)
	{
		return;
	}

	// Copy serialization parameters
	CopySerializationParams(nID, pVertices, nVertexCount, pUVScale, pWorldTM);

	m_nID = nID;
	m_pWorldTM = pWorldTM;

	// Remove form 3d engine
	GetWaterWaveManager()->Unregister(this);
	Get3DEngine()->UnRegisterEntityAsJob(this);

	// Get min/max boundings
	ComputeMinMax(pVertices, nVertexCount);

	// Store original position
	m_pOrigPos = m_pWorldTM.GetTranslation();

	// Add to 3d engine
	GetWaterWaveManager()->Register(this);
	Get3DEngine()->RegisterEntity(this);

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::CopySerializationParams(uint64 nID, const Vec3* pVertices, unsigned int nVertexCount, const Vec2& pUVScale, const Matrix34& pWorldTM)
{

	SAFE_DELETE(m_pSerializeParams);

	m_pSerializeParams = new SWaterWaveSerialize;

	m_pSerializeParams->m_nID = nID;
	m_pSerializeParams->m_pMaterial = m_pMaterial;

	m_pSerializeParams->m_fUScale = pUVScale.x;
	m_pSerializeParams->m_fVScale = pUVScale.y;

	m_pSerializeParams->m_nVertexCount = nVertexCount;

	// Copy vertices
	m_pSerializeParams->m_pVertices.resize(nVertexCount);
	for (uint32 v(0); v < nVertexCount; ++v)
	{
		m_pSerializeParams->m_pVertices[v] = pVertices[v];
	}

	m_pSerializeParams->m_pWorldTM = pWorldTM;

	m_pSerializeParams->m_pParams = m_pParams;

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::SetParams(const SWaterWaveParams& pParams)
{
	m_pParams = pParams;
}

//////////////////////////////////////////////////////////////////////////
const SWaterWaveParams& CWaterWaveRenderNode::GetParams() const
{
	return m_pParams;
}

//////////////////////////////////////////////////////////////////////////
const char* CWaterWaveRenderNode::GetEntityClassName() const
{
	return "CWaterWaveRenderNode";
}

//////////////////////////////////////////////////////////////////////////
const char* CWaterWaveRenderNode::GetName() const
{
	return "WaterWave";
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::Spawn()
{

	m_pWorldTM.SetTranslation(m_pOrigPos + Vec3(sfrand(), sfrand(), sfrand()) * m_pParams.m_fPosVar);
	m_pParams.m_fCurrLifetime = max(m_pParams.m_fLifetime + m_pParams.m_fLifetimeVar * sfrand(), 0.5f);
	m_pParams.m_fCurrSpeed = max(m_pParams.m_fSpeed + m_pParams.m_fSpeedVar * sfrand(), 1.0f);
	m_pParams.m_fCurrHeight = max(m_pParams.m_fHeight + m_pParams.m_fHeightVar * sfrand(), 0.0f);
	m_pParams.m_fCurrFrameLifetime = m_pParams.m_fCurrLifetime;

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::Update(float fDistanceToCamera)
{

	// Check distance to terrain
	Vec3 pCenterPos = m_pParams.m_pPos;
	float fTerrainZ(GetTerrain()->GetZApr(pCenterPos.x, pCenterPos.y));
	m_fCurrTerrainDepth = max(pCenterPos.z - fTerrainZ, 0.0f);

	float fDepthAttenuation = clamp_tpl<float>(m_fCurrTerrainDepth * 0.2f, 0.0f, 1.0f);

	// Slide wave along transformation orientation

	Vec3 pDirection(m_pWorldTM.GetColumn1());
	Vec3 pPos(m_pWorldTM.GetTranslation());

	float fDelta(m_pParams.m_fCurrSpeed * GetTimer()->GetFrameTime());
	pPos += pDirection * fDelta * fDepthAttenuation;

	// Always set Z to water level
	float fWaterLevel(Get3DEngine()->GetWaterLevel());
	pPos.z = fWaterLevel - 0.5f;

	// Update position and bounding box

	m_pWorldTM.SetTranslation(pPos);
	UpdateBoundingBox();

	// Update lifetime
	m_pParams.m_fCurrFrameLifetime -= GetTimer()->GetFrameTime();

	// Spawn new wave
	if (m_pParams.m_fCurrFrameLifetime <= 0.0f || m_fCurrTerrainDepth == 0.0f)
		Spawn();

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Set custom render data
	m_fRECustomData[8] = clamp_tpl<float>(m_pParams.m_fCurrFrameLifetime, 0.0f, 1.0f);
	m_fRECustomData[8] *= m_fRECustomData[8] * (3.0f - 2.0f * m_fRECustomData[8]);

	m_fRECustomData[9] = clamp_tpl<float>(m_pParams.m_fCurrFrameLifetime / m_pParams.m_fCurrLifetime, 0.0f, 1.0f);

	m_fRECustomData[10] = min(m_fCurrTerrainDepth, 1.0f);
	m_fRECustomData[11] = min(1.0f, 1.0f - (fDistanceToCamera / GetMaxViewDist()));
	m_fRECustomData[12] = m_pParams.m_fCurrHeight * (fDepthAttenuation);

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pRenderMesh == 0)
		return; //  false;

	if (!m_pMaterial || !passInfo.RenderWaterWaves() || !passInfo.RenderWaterOcean())
		return; // false;

	C3DEngine* p3DEngine(Get3DEngine());
	IRenderer* pRenderer(GetRenderer());

	// get render objects
	CRenderObject* pRenderObj(passInfo.GetIRenderView()->AllocateTemporaryRenderObject());
	if (!pRenderObj)
		return; // false;

	float fWaterLevel(Get3DEngine()->GetWaterLevel());
	Vec3 pCamPos(passInfo.GetCamera().GetPosition());

	// don't render if bellow water level
	if (max((pCamPos.z - fWaterLevel) * 100.0f, 0.0f) == 0.0f)
		return; //false;

	// Fill in data for render object

	pRenderObj->SetMatrix(m_pWorldTM, passInfo);
	pRenderObj->m_fSort = 0;
	pRenderObj->m_ObjFlags |= FOB_TRANS_MASK;
	SRenderObjData* pOD = pRenderObj->GetObjData();
	pOD->m_fTempVars[0] = 1.0f;

	m_fRECustomData[0] = p3DEngine->m_oceanWindDirection;
	m_fRECustomData[1] = p3DEngine->m_oceanWindSpeed;
	m_fRECustomData[2] = 0.0f; // used to be m_oceanWavesSpeed
	m_fRECustomData[3] = p3DEngine->m_oceanWavesAmount;
	m_fRECustomData[4] = p3DEngine->m_oceanWavesSize;

	sincos_tpl(p3DEngine->m_oceanWindDirection, &m_fRECustomData[6], &m_fRECustomData[5]);
	m_fRECustomData[7] = fWaterLevel;

	//pOD->m_CustomData = &m_fRECustomData;
	m_pRenderMesh->SetREUserData(&m_fRECustomData[0]);

	// Submit wave surface
	bool bAboveWaterLevel(max(pCamPos.z - fWaterLevel, 0.0f) > 0.0f);

	m_pRenderMesh->AddRenderElements(m_pMaterial, pRenderObj, passInfo, EFSLIST_WATER_VOLUMES, bAboveWaterLevel);

	//  return true;
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CWaterWaveRenderNode::GetPhysics() const
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::SetPhysics(IPhysicalEntity*)
{
	// make me
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "WaterWaveNode");
	size_t dynSize(sizeof(*this));

	pSizer->AddObject(this, dynSize);
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::Precache()
{

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::SetRenderMesh(IRenderMesh* pRenderMesh)
{
	m_pRenderMesh = pRenderMesh;
}

void CWaterWaveRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_pOrigPos += delta;
	m_pWorldTM.SetTranslation(m_pWorldTM.GetTranslation() + delta);
	m_pMin += delta;
	m_pMax += delta;
	m_WSBBox.Move(delta);
}

//////////////////////////////////////////////////////////////////////////
CWaterWaveManager::CWaterWaveManager()
{

}

//////////////////////////////////////////////////////////////////////////
CWaterWaveManager::~CWaterWaveManager()
{

	Release();

}

//////////////////////////////////////////////////////////////////////////
IRenderMesh* CWaterWaveManager::CreateRenderMeshInstance(CWaterWaveRenderNode* pWave)
{
	// todo: put tessellation and indices generation outside

	f32 fWaveKey(pWave->GetWaveKey());
	InstancedWavesMap::iterator pItor(m_pInstancedWaves.find(fWaveKey));

	if (pItor == m_pInstancedWaves.end())
	{
		// Create render mesh and store it
		const SWaterWaveSerialize* pSerParam = pWave->GetSerializationParams();

		Matrix34 pInvWorldTM(pSerParam->m_pWorldTM);
		pInvWorldTM.Invert();

		int32 nVertexCount(pSerParam->m_nVertexCount);
		float fUScale(pSerParam->m_fUScale);
		float fVScale(pSerParam->m_fVScale);

		_smart_ptr<IRenderMesh> pRenderMesh = NULL;
		std::vector<Vec3> pVertices(pSerParam->m_pVertices);
		PodArray<SVF_P3F_C4B_T2F> pWaveVertices;
		PodArray<vtx_idx> pWaveIndices;

		const int nGridRes = GetCVars()->e_WaterWavesTessellationAmount;
		const float fGridStep = 1.0f / ((float) nGridRes - 1);

		// Generate vertices
		pWaveVertices.resize(0);
		pWaveIndices.resize(0);

		pWaveVertices.reserve(nVertexCount);
		pWaveIndices.reserve(nVertexCount);

		// Tesselate sectors
		Vec3 pTmpPos;
		int i = 0;

		const int nSectors = nVertexCount / 4;
		const int nGridResTotal = nGridRes * nSectors - nSectors;
		const float fGridResTotalStep = 1.0f / ((float)nGridResTotal);
		int nCurrVertex = 0;

		// Each sector has 4 vertices
		for (i = 0; i < nVertexCount; i += 4)
		{
			// Set edges (in object space)
			Vec3 p0(pVertices[i + 0]);
			p0 = pInvWorldTM.TransformPoint(p0);

			Vec3 p1(pVertices[i + 1]);
			p1 = pInvWorldTM.TransformPoint(p1);

			Vec3 p2(pVertices[i + 2]);
			p2 = pInvWorldTM.TransformPoint(p2);

			Vec3 p3(pVertices[i + 3]);
			p3 = pInvWorldTM.TransformPoint(p3);

			float fBeginTC = 0;
			float fEndTC = 1;

			for (int x(0); x < nGridRes; ++x)
			{
				// Find min/max edges
				float _fCurrStep = (float) x * fGridStep;
				Vec3 pMin = p0 + (p2 - p0) * _fCurrStep;
				Vec3 pMax = p1 + (p3 - p1) * _fCurrStep;

				float fTexCoordU = fBeginTC + (fEndTC - fBeginTC) * _fCurrStep;
				fTexCoordU *= fUScale;

				float fXStep = (float) nCurrVertex * fGridResTotalStep;
				if (x < nGridRes - 1)
				{
					nCurrVertex++;
				}

				for (int y(0); y < nGridRes; ++y)
				{
					// Get final position
					float fCurrStep = (float) y * fGridStep;

					Vec3 pFinal = pMin + (pMax - pMin) * fCurrStep;

					SVF_P3F_C4B_T2F pCurr;

					pCurr.xyz = pFinal;

					float fTexCoordV = fBeginTC + (fEndTC - fBeginTC) * fCurrStep;
					fTexCoordV *= fVScale;

					pCurr.st = Vec2(fTexCoordU, fTexCoordV);

					// Store some geometry info in vertex colors
					float fEdgeX = 1 - abs(fXStep * 2.0f - 1.0f);     // horizontal lerp
					float fEdgeY = 1 - abs(fCurrStep * 2.0f - 1.0f);  // vertical lerp

					float fEdge = fEdgeX * fEdgeY;              // horizontal/vertical merged
					uint8 nEdge = (uint8)(fEdge * 255);

					pCurr.color.bcolor[0] = (uint8) (fXStep * 255.0f);
					pCurr.color.bcolor[1] = (uint8) (fCurrStep * 255.0f);
					pCurr.color.bcolor[2] = 0;
					pCurr.color.bcolor[3] = nEdge;

					pWaveVertices.push_back(pCurr);
				}
			}
		}

		// Generate indices
		int nOffset = 0;
		for (i = 0; i < nVertexCount; i += 4)
		{
			int nIndex(0);

			for (int y(0); y < nGridRes - 1; ++y)
			{
				for (int x(0); x < nGridRes; ++x, ++nIndex)
				{
					pWaveIndices.push_back(nOffset + nIndex);
					pWaveIndices.push_back(nOffset + nIndex + nGridRes);
				}

				if (nGridRes - 2 >= y)
				{
					// close strip row
					pWaveIndices.push_back(nOffset + nIndex + nGridRes - 1);
					pWaveIndices.push_back(nOffset + nIndex);
				}
			}

			// close sector strip
			pWaveIndices.push_back(nOffset + nIndex + nGridRes - 1);
			pWaveIndices.push_back(nOffset + nIndex);

			// Set new offset
			nOffset = nOffset + nIndex + nGridRes;
		}

		// Finally, make render mesh
		pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(&pWaveVertices[0],
		                                                         pWaveVertices.size(),
		                                                         EDefaultInputLayouts::P3F_C4B_T2F,
		                                                         &pWaveIndices[0],
		                                                         pWaveIndices.size(),
		                                                         prtTriangleStrip,
		                                                         "WaterWave",
		                                                         "WaterWave",
		                                                         eRMT_Static);

		float texelAreaDensity = 1.0f;
		{
			const size_t indexCount = pWaveIndices.size();
			const size_t vertexCount = pWaveVertices.size();

			if ((indexCount > 0) && (vertexCount > 0))
			{
				float posArea;
				float texArea;
				const char* errorText = "";

				const bool ok = CMeshHelpers::ComputeTexMappingAreas(
				  indexCount, &pWaveIndices[0],
				  vertexCount,
				  &pWaveVertices[0].xyz, sizeof(pWaveVertices[0]),
				  &pWaveVertices[0].st, sizeof(pWaveVertices[0]),
				  posArea, texArea, errorText);

				if (ok)
				{
					texelAreaDensity = texArea / posArea;
				}
				else
				{
					gEnv->pLog->LogError("Failed to compute texture mapping density for mesh '%s': %s", "WaterWave", errorText);
				}
			}
		}

		pRenderMesh->SetChunk(pWave->GetMaterial(0), 0, pWaveVertices.size(), 0, pWaveIndices.size(), texelAreaDensity);

		// And add it to out instanced list
		m_pInstancedWaves.insert(InstancedWavesMap::value_type(fWaveKey, pRenderMesh));

		return pRenderMesh;
	}

	return pItor->second;
}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveManager::Register(CWaterWaveRenderNode* pWave)
{

	// Check first if exists already
	GlobalWavesMap::iterator pItor(m_pWaves.find(pWave->GetID()));

	if (pItor == m_pWaves.end())
	{
		m_pWaves.insert(GlobalWavesMap::value_type(pWave->GetID(), pWave));
	}

	// Make wave geometry instance and set wave render mesh
	_smart_ptr<IRenderMesh> pWaveRenderMesh(CreateRenderMeshInstance(pWave));
	pWave->SetRenderMesh(pWaveRenderMesh);

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveManager::Unregister(CWaterWaveRenderNode* pWave)
{

	f32 fCurrWaveKey(pWave->GetWaveKey());

	{
		// Remove from map
		GlobalWavesMapIt pItor(m_pWaves.find(pWave->GetID()));

		if (pItor != m_pWaves.end())
		{
			m_pWaves.erase(pItor);
		}
	}

	{

		GlobalWavesMapIt _pItor(m_pWaves.begin());
		GlobalWavesMapIt pEnd(m_pWaves.end());
		uint32 nInstances(0);

		// todo: this is unneficient, find better solution
		for (; _pItor != pEnd; ++_pItor)
		{
			f32 fWaveKey = _pItor->second->GetWaveKey();
			if (fWaveKey == fCurrWaveKey)
			{
				nInstances++;
				break;
			}
		}

		// If no instances left, remove render mesh
		if (!nInstances)
		{
			InstancedWavesMapIt pItor(m_pInstancedWaves.find(fCurrWaveKey));
			if (pItor != m_pInstancedWaves.end())
				m_pInstancedWaves.erase(pItor);
		}

	}

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveManager::Release()
{
	// Free all resources

	m_pWaves.clear();
	m_pInstancedWaves.clear();

}

//////////////////////////////////////////////////////////////////////////
void CWaterWaveManager::Update(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// Update each wave
	GlobalWavesMapIt pItor(m_pWaves.begin());
	GlobalWavesMapIt pEnd(m_pWaves.end());

	Vec3 pCamPos = passInfo.GetCamera().GetPosition();

	for (; pItor != pEnd; ++pItor)
	{
		// Verify distance to camera and visibility
		CWaterWaveRenderNode* pCurr(pItor->second);
		bool IsVisible(abs(pCurr->GetDrawFrame(passInfo.GetRecursiveLevel()) - passInfo.GetFrameID()) <= 2);
		float fDistanceSqr(pCamPos.GetSquaredDistance(pCurr->GetPos()));

		// if wave is visible, or nearby camera update it
		if (IsVisible || fDistanceSqr < MAX_WAVE_DIST_UPDATE)
		{
			pCurr->Update(sqrt_tpl(fDistanceSqr));
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
void CWaterWaveRenderNode::FillBBox(AABB& aabb)
{
	aabb = CWaterWaveRenderNode::GetBBox();
}

///////////////////////////////////////////////////////////////////////////////
EERType CWaterWaveRenderNode::GetRenderNodeType()
{
	return eERType_WaterWave;
}

///////////////////////////////////////////////////////////////////////////////
float CWaterWaveRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, CWaterWaveRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, CWaterWaveRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CWaterWaveRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pParams.m_pPos;
}

///////////////////////////////////////////////////////////////////////////////
IMaterial* CWaterWaveRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
