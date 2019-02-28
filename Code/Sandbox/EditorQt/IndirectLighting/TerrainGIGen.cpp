// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <set>

#include "TerrainGIGen.h"
#include "Vegetation/VegetationMap.h"
#include "Vegetation/VegetationObject.h"
#include <CrySystem/File/ICryPak.h>
#include "Util/PakFile.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include "Terrain/Sky Accessibility/HeightmapAccessibility.h"
#include "Terrain/Heightmap.h"
#include "Objects/EntityObject.h"
#include "Objects/Group.h"
#include "Objects/BrushObject.h"
#include "Material/Material.h"
#include "Raster.h"
#include <CryRenderer/IShader.h>
#include "QT/Widgets/QWaitProgress.h"

//defines for debugging to enable features
#define USE_BRUSHS
#define USE_PREFABS
//#define USE_ENTITIES
#define USE_VEGETATION

//#define GEN_ACCESS_TEST_TGA
//#define DEBUG_RASTER_TABLES

const float CTerrainGIGen::scMinBBoxBrushHeight = 1.5f;
const float CTerrainGIGen::scMinBBoxVegHeight = 2.f;
const float CTerrainGIGen::scMaxDistToGroundRC = 10.0f;
const float CTerrainGIGen::scMaxDistToGroundNoRayCast = 30.0f;
const float CTerrainGIGen::scMaxObjectSampleExtension = 15.f;
const float CTerrainGIGen::scMinLargeEntityXYExtension = 10.0f;
const float CTerrainGIGen::scMaxLargeEntityXYExtension = 60.f;
const float CTerrainGIGen::scMaxObjExt = 256.f;

static const ColorF gscFrozenCol(0.776f, 0.819f, 0.882f, 1.f);

namespace
{
inline void GenConvCoeffs(const CTerrainGIGen::TCoeffType& crOrigCoeffs, CTerrainGIGen::TCoeffType& rConvCoeffs)
{
	rConvCoeffs[0] = crOrigCoeffs[0];
	for (int i = 1; i < 4; ++i)
		rConvCoeffs[i] = CHemisphereSink_SH::cA1 * crOrigCoeffs[i];
	for (int i = 4; i < 9; ++i)
		rConvCoeffs[i] = CHemisphereSink_SH::cA2 * crOrigCoeffs[i];
}

// Description:
//		assigns the sh coeffs to a real spherical harmonic
inline void AssignSHData(const float cSource[9], CTerrainGIGen::TCoeffType& rDestCoeffs)
{
	assert(rDestCoeffs.size() < 9);
	rDestCoeffs.ReSize(NSH::SDescriptor(3));  //initialize it
	memcpy(&rDestCoeffs[0], cSource, 9 * sizeof(float));
}
}

const bool CTerrainGIGen::PrepareHeightmap(const uint32 cWidth, CFloatImage& rFloatImage, float& rHeightScale) const
{
	//cWidth is a size of a texture where every pixel represents one grid point
	rHeightScale = 0.f;
	if (!m_pHeightMap)
		return false;
	//get down sampled version of heightmap
	if (!rFloatImage.Allocate(cWidth, cWidth))
		return false;
	CRect fullRect(0, 0, m_pHeightMap->GetWidth(), m_pHeightMap->GetHeight());
	m_pHeightMap->GetData(fullRect, rFloatImage.GetWidth(), CPoint(0, 0), rFloatImage, false, false);    // no smooth, no noise
	rHeightScale = cWidth * m_pHeightMap->CalcHeightScale();
	return true;
}

void CTerrainGIGen::GetMaterialColor(IMaterial* pMat, std::vector<ColorF>& rSubMats, ColorF& rLastCol, const bool cFillVec) const
{
	const ColorF cDefColour = ColorF(0.5f, 0.5f, 0.5f, 1.f);
	if (pMat)
	{
		const uint32 cSubMatCount = pMat->GetSubMtlCount();
		if (cSubMatCount > 0)
		{
			ColorF matCol(0.f, 0.f, 0.f, 0.f);
			uint32 validMatCount = 0;
			for (int i = 0; i < cSubMatCount; ++i)
			{
				IMaterial* pSubMat = pMat->GetSubMtl(i);
				if (pSubMat && ((pSubMat->GetFlags() & MTL_FLAG_NODRAW) == 0))//might be proxy or obstruct material
				{
					GetMaterialColor(pSubMat, rSubMats, rLastCol, false);
					matCol += rLastCol;
					validMatCount++;
					if (cFillVec)
						rSubMats.push_back(rLastCol);
				}
				else
				{
					if (cFillVec)
						rSubMats.push_back(pSubMat ? ColorF(1.f, 1.f, 1.f, 0.f) : ColorF(1.f, 1.f, 1.f, 1.f));
				}
			}
			if (!cFillVec)
				rLastCol = validMatCount ? (matCol * (1.f / validMatCount)) : cDefColour;
		}
		else
		{
			if (((pMat->GetFlags() & MTL_FLAG_NODRAW) == 0))
			{
				const SShaderItem& shaderItem = pMat->GetShaderItem();
				const SEfResTexture* pMtlDiffuseTex = shaderItem.m_pShaderResources ? shaderItem.m_pShaderResources->GetTexture(EFTT_DIFFUSE) : NULL;
				//				SInputShaderResources& rRes = pMat->GetShaderResources();
				//				ColorF matCol(rRes.m_LMaterial.m_Front.m_Diffuse);
				ColorF matCol = shaderItem.m_pShaderResources->GetColorValue(EFTT_DIFFUSE);
				matCol.a = 1.0f;
				if (cFillVec)
					rSubMats.push_back(matCol);
				else
					rLastCol = matCol;
			}
			else
				rSubMats.push_back(ColorF(1.f, 1.f, 1.f, 0.f));
		}
	}
	else
	{
		if (cFillVec)
			rSubMats.push_back(cDefColour);
		else
			rLastCol = cDefColour;
	}
}

const bool CTerrainGIGen::ProcessMesh
(
  IStatObj* pStatObj,
  const AABB& crBBox,
  const Matrix34& crWorldMat,
  const std::vector<std::vector<int>>& crMatIndices,
  const std::vector<ColorF>& crMatColours,
  std::vector<STempBBoxData>& rBBoxVec,
  const bool cIsVegetation,
  const float* cpHeightMap,
  const float cWorldHMapScale,
  const uint32 cHeigthMapRes,
  SObjectBBoxes& rObjectBox,
  const bool cCheckAgainstHeightMap,
  AABB& rMinMaxCalculated,
  IRenderMesh* pReplacementMesh
)
{
	if (!pStatObj && !pReplacementMesh)
		return false;
	//set outer extensions
	rObjectBox.minOuter.x = crBBox.min.x;
	rObjectBox.minOuter.y = crBBox.min.y;
	rObjectBox.maxOuter.x = crBBox.max.x;
	rObjectBox.maxOuter.y = crBBox.max.y;
	const Vec3 cCenter((crBBox.min + crBBox.max) * 0.5f);
	//not correct if it really has an inner box not sharing middle point
	rObjectBox.minInner.x = rObjectBox.maxInner.x = cCenter.x;
	rObjectBox.minInner.y = rObjectBox.maxInner.y = cCenter.y;
	const Vec3 cExt = crBBox.GetSize();
	rObjectBox.maxAxisExtension = std::max(cExt.x, std::max(cExt.y, cExt.z));
	rObjectBox.zExt = cExt.z;
	rObjectBox.distToGround = GetDistToGround(cpHeightMap, crBBox, cWorldHMapScale, cHeigthMapRes);

	//make sure object pos is above ground
	const float cInvUnitSize = 1.f / m_UnitSize;
	const uint32 cWidth = m_pHeightMap->GetWidth();
	const uint32 cWidthUnits = m_UnitSize * cWidth;
	const uint32 cObjectMul = std::max(m_Config.vegResMultiplier, m_Config.brushResMultiplier);
	const uint32 cHeightmapHighResWidth = cWidthUnits / m_Config.baseGranularity * cObjectMul;
	int32 imageToTerrainShift = 0;
	while ((cHeightmapHighResWidth << imageToTerrainShift) != m_TerrainMapRes)
		++imageToTerrainShift;

	//triangle validators for CBBoxRaster, static for triangle rasterization MP
	//	static ITriangleValidator defaultVal;
	static SHeightmapTriangleValidator heigthMapVal(*this);

	//WORLD_HMAP_COORDINATE_EXCHANGE
	const float cHeigthUnderPos = GetFilteredHeight
	                              (
	  rObjectBox.pos.y * cInvUnitSize,
	  rObjectBox.pos.x * cInvUnitSize,
	  cHeightmapHighResWidth,
	  m_TerrainMapRes,
	  cpHeightMap,
	  imageToTerrainShift
	                              );
	rObjectBox.pos.z = std::max(cHeigthUnderPos + 0.3f /*little offset*/, rObjectBox.pos.z);//make sure its above ground

	//set to -1.f to indicate that this object is not big enough for a useful vis progression
	rObjectBox.maxZ = (cExt.z > CHemisphereSink_SH::scSampleHeightOffset) ? crBBox.max.z : -1.f;

	const uint32 cMatCount = crMatColours.size();
	assert(cMatCount > 0);

	//double buffered chunk vectors, needed to rasterize in parallel
	m_ChunkVecs[m_ActiveChunk].clear();
	m_ChunkVecs[m_ActiveChunk].resize(cMatCount);
	std::vector<std::vector<SMatChunk>>& chunkVec = m_ChunkVecs[m_ActiveChunk];
	m_ActiveChunk = ((m_ActiveChunk + 1) & 0x1);

	struct MeshLock
	{
		MeshLock() : mesh(0) {}
		~MeshLock()
		{
			if (mesh)
			{
				mesh->UnlockStream(VSF_GENERAL);
				mesh->UnlockIndexStream();
				mesh->UnLockForThreadAccess();
			}
		}
		void Set(IRenderMesh* m)
		{
			assert(!mesh);
			(mesh = m)->LockForThreadAccess();
		}
	private:
		IRenderMesh* mesh;
	};

	MeshLock meshLock;

	uint32 objectsAdded = 0;
	if (!pStatObj)
	{
		assert(cMatCount == 1);
		int posStride = 0;
		TRenderChunkArray* chunkList = &pReplacementMesh->GetChunks();

		meshLock.Set(pReplacementMesh);

		vtx_idx* meshIndices = pReplacementMesh->GetIndexPtr(FSL_READ);
		uint8* meshPositions = pReplacementMesh->GetPosPtr(posStride, FSL_READ);

		for (int chunkId = 0; chunkId < chunkList->size(); ++chunkId)
		{
			CRenderChunk* pChunk = &chunkList->at(chunkId);
			if (!pChunk)
				continue;
			SMatChunk matChunk;
			matChunk.pIndices = meshIndices;
			matChunk.pPositions = meshPositions;
			if (!matChunk.pIndices || !matChunk.pPositions)
				continue;
			matChunk.num = pChunk->nNumIndices;
			if (matChunk.num <= 0)
				continue;
			matChunk.matIndex = 0;
			matChunk.startIndex = pChunk->nFirstIndexId;
			matChunk.posStride = posStride;
			chunkVec[0].push_back(matChunk);
			++objectsAdded;
		}
	}
	else
	{
		const int cSubObjCnt = pStatObj ? pStatObj->GetSubObjectCount() : 0;
		int curObjIndex = 0;
		IStatObj* pParentStatObj = pStatObj;//follow only one indirection
		uint32 matIndex = 0;//index into crMatIndices
		Matrix34* pLocalTM = NULL;

		while (pStatObj)
		{
			IIndexedMesh* pIMesh = pStatObj->GetIndexedMesh();
			if (pIMesh)
			{
				CMesh* pMesh = pIMesh->GetMesh();
				assert(pMesh);
				const uint32 cSubMatCount = crMatIndices[matIndex].size();
				for (int i = 0; i < (int)pMesh->m_subsets.size(); i++)
				{
					const SMeshSubset& crSubset = pMesh->m_subsets[i];
					assert(matIndex < crMatIndices.size());
					SMatChunk matChunk;
					matChunk.pIndices = pMesh->m_pIndices;
					matChunk.pPositions = (uint8*)pMesh->m_pPositions;
					assert(pMesh->m_pPositionsF16 == 0);
					if (!matChunk.pIndices || !matChunk.pPositions)
						continue;
					matChunk.num = crSubset.nNumIndices;
					if (matChunk.num <= 0 || crSubset.nMatID >= cSubMatCount)
						continue;
					if (pLocalTM)
						matChunk.localTM = *pLocalTM;
					matChunk.matIndex = crMatIndices[matIndex][std::min(crSubset.nMatID, (int)(cSubMatCount - 1))];
					assert(matChunk.matIndex >= 0 && matChunk.matIndex < cMatCount);
					matChunk.startIndex = crSubset.nFirstIndexId;
					chunkVec[matChunk.matIndex].push_back(matChunk);
					++objectsAdded;
				}
			}
			else
			{
				IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh();
				if (pRenderMesh)
				{
					meshLock.Set(pRenderMesh);

					int indexCount = pRenderMesh->GetIndicesCount();
					int posStride = 0;

					vtx_idx* meshIndices = pRenderMesh->GetIndexPtr(FSL_READ);
					uint8* meshPositions = pRenderMesh->GetPosPtr(posStride, FSL_READ);

					const uint32 cVertexCount = pRenderMesh->GetVerticesCount();

					const uint32 cSubMatCount = crMatIndices[matIndex].size();
					TRenderChunkArray* chunkList = &pRenderMesh->GetChunks();
					for (int chunkId = 0; chunkId < chunkList->size(); ++chunkId)
					{
						CRenderChunk* pChunk = &chunkList->at(chunkId);
						if (!pChunk || pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
							continue;
						if (pChunk->nNumVerts > cVertexCount)
							continue;
						assert(matIndex < crMatIndices.size());
						SMatChunk matChunk;
						if (pLocalTM)
							matChunk.localTM = *pLocalTM;

						matChunk.pIndices = meshIndices;
						matChunk.pPositions = meshPositions;
						if (!matChunk.pIndices || !matChunk.pPositions)
							continue;
						if (pChunk->nFirstIndexId + pChunk->nNumIndices > indexCount || pChunk->nNumVerts < 3)
							continue;
						matChunk.num = pChunk->nNumIndices;
						if (matChunk.num <= 0 || pChunk->m_nMatID >= cSubMatCount)
							continue;
						matChunk.matIndex = crMatIndices[matIndex][MIN(pChunk->m_nMatID, (int)(cSubMatCount - 1))];
						assert(matChunk.matIndex >= 0 && matChunk.matIndex < cMatCount);
						matChunk.startIndex = pChunk->nFirstIndexId;
						matChunk.posStride = posStride;
						chunkVec[matChunk.matIndex].push_back(matChunk);
						++objectsAdded;
					}
				}
			}
			bool updated = false;
			while (!updated)
			{
				pLocalTM = NULL;
				pStatObj = (curObjIndex < cSubObjCnt) ? pParentStatObj->GetSubObject(curObjIndex++)->pStatObj : NULL;
				if (pStatObj)
				{
					pLocalTM = &(pParentStatObj->GetSubObject(curObjIndex - 1)->localTM);
					updated = true;
				}
				if (curObjIndex >= cSubObjCnt)
					updated = true;
				assert(matIndex < crMatIndices.size());
				if (updated)
					++matIndex;
			}
		}
		pStatObj = NULL;
	}

	bool onGround = false;
	rBBoxVec.reserve(std::max((uint32)1, objectsAdded));

	if (objectsAdded == 0)
	{
		onGround = (rObjectBox.distToGround <= CHemisphereSink_SH::scSampleHeightOffset);
		if (onGround)
		{
			rObjectBox.minInner.x = crBBox.min.x;
			rObjectBox.minInner.y = crBBox.min.y;
			rObjectBox.maxInner.x = crBBox.max.x;
			rObjectBox.maxInner.y = crBBox.max.y;
		}
		rBBoxVec.push_back(STempBBoxData(crBBox, ColorF(0.5f, 0.5f, 0.5f, 1.f), NULL, onGround));
		return onGround;
	}

	uint32 addedMatCntr = 0;//counts all rasterized mats

	//compute better averaging bbox
	//use indices and positions, assume 3 indices per face
	//first compute full triangle area and average center
	//transform into object space where points can be tested
	const std::vector<std::vector<SMatChunk>>::const_iterator cChunkEnd = chunkVec.end();
	for (std::vector<std::vector<SMatChunk>>::const_iterator chunkIter = chunkVec.begin(); chunkIter != cChunkEnd; ++chunkIter)
	{
		const std::vector<SMatChunk>& crChunks = *chunkIter;
		if (crChunks.empty())
			continue;
		assert(crChunks[0].matIndex >= 0 && crChunks[0].matIndex < crMatColours.size());
		ColorF newCol = crMatColours[crChunks[0].matIndex];//sorted by materials
		if (newCol.a <= 0.000001f)
			continue;//proxy mat

		float area = 0;
		Vec3 center(0, 0, 0);//center mean

		Vec3 max(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		Vec3 min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

		Vec3 locMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		Vec3 locMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

		const std::vector<SMatChunk>::const_iterator cSubsetEnd = crChunks.end();
		for (std::vector<SMatChunk>::const_iterator iter = crChunks.begin(); iter != cSubsetEnd; ++iter)
		{
			const SMatChunk& crSubset = *iter;

			vtx_idx* pIndices = crSubset.pIndices;
			uint8* pPositions = crSubset.pPositions;
			assert(pIndices && pPositions);

			const Matrix34& crLocalTM = crSubset.localTM;

			for (int i = crSubset.startIndex; i < crSubset.startIndex + crSubset.num; i += 3)
			{
				//only consider if entire triangle inside new bbox
				const Vec3 cLocA = crLocalTM.TransformPoint(*(Vec3*)(&pPositions[crSubset.posStride * pIndices[i]]));
				const Vec3 cLocB = crLocalTM.TransformPoint(*(Vec3*)(&pPositions[crSubset.posStride * pIndices[i + 1]]));
				const Vec3 cLocC = crLocalTM.TransformPoint(*(Vec3*)(&pPositions[crSubset.posStride * pIndices[i + 2]]));

				const Vec3 cA = crWorldMat.TransformPoint(cLocA);
				const Vec3 cB = crWorldMat.TransformPoint(cLocB);
				const Vec3 cC = crWorldMat.TransformPoint(cLocC);

				if (cCheckAgainstHeightMap)
				{
					//do only consider if above heightmap (voxels replace terrain in its sector)
					if (!IsTriangleAboveTerrain(cA, cB, cC))
						continue;
				}

				locMax.x = std::max(locMax.x, cA.x);
				locMax.x = std::max(locMax.x, cB.x);
				locMax.x = std::max(locMax.x, cC.x);
				locMax.y = std::max(locMax.y, cA.y);
				locMax.y = std::max(locMax.y, cB.y);
				locMax.y = std::max(locMax.y, cC.y);
				locMax.z = std::max(locMax.z, cA.z);
				locMax.z = std::max(locMax.z, cB.z);
				locMax.z = std::max(locMax.z, cC.z);

				locMin.x = std::min(locMin.x, cA.x);
				locMin.x = std::min(locMin.x, cB.x);
				locMin.x = std::min(locMin.x, cC.x);
				locMin.y = std::min(locMin.y, cA.y);
				locMin.y = std::min(locMin.y, cB.y);
				locMin.y = std::min(locMin.y, cC.y);
				locMin.z = std::min(locMin.z, cA.z);
				locMin.z = std::min(locMin.z, cB.z);
				locMin.z = std::min(locMin.z, cC.z);

				max.x = std::max(max.x, cA.x);
				max.x = std::max(max.x, cB.x);
				max.x = std::max(max.x, cC.x);
				max.y = std::max(max.y, cA.y);
				max.y = std::max(max.y, cB.y);
				max.y = std::max(max.y, cC.y);
				max.z = std::max(max.z, cA.z);
				max.z = std::max(max.z, cB.z);
				max.z = std::max(max.z, cC.z);

				min.x = std::min(min.x, cA.x);
				min.x = std::min(min.x, cB.x);
				min.x = std::min(min.x, cC.x);
				min.y = std::min(min.y, cA.y);
				min.y = std::min(min.y, cB.y);
				min.y = std::min(min.y, cC.y);
				min.z = std::min(min.z, cA.z);
				min.z = std::min(min.z, cB.z);
				min.z = std::min(min.z, cC.z);
			}
		}//chunkVec loop
		 //adjust bbox alpha according to center mean and center diff
		AABB bbox(min, max);
		bool isPlane = false;
		const Vec3 cLocMaxMin = (locMax - locMin) * 0.5f;
		if (cLocMaxMin.x <= 0.1f || cLocMaxMin.y <= 0.1f || cLocMaxMin.z <= 0.1f)
			isPlane = true;//will not get a CBBoxRaster
		Vec3 centerDiff;
		float weight = 1.f;
		rMinMaxCalculated = bbox;
		const Vec3 cCalcCenter((min + max) * 0.5f);
		const float cXYExtensionScale = std::min(1.f, 1.f / 200.f * (bbox.GetSize().x * bbox.GetSize().x + bbox.GetSize().y * bbox.GetSize().y));//xy scale for alpha weighting
		const bool cIsOnGround = (GetDistToGround(cpHeightMap, bbox, cWorldHMapScale, cHeigthMapRes) <= CHemisphereSink_SH::scSampleHeightOffset);
		if (cIsOnGround)
			onGround = true;
		if (cIsVegetation && newCol.a < 0.9f && !cIsOnGround)
		{
			//take back lighting into account
			static const float scBackLightingMultiplier = 1.4f;
			newCol.g = std::min(0.9f, newCol.g * scBackLightingMultiplier);
		}

		//create bounding box
		if (newCol.a > 0.08f)//material matters
		{
			CBBoxRaster* pBBoxRaster = NULL;
			if (!isPlane)
			{
				pBBoxRaster = new CBBoxRaster();
				if (cCheckAgainstHeightMap)
					m_RasterMPMan.PushProjectTrianglesOS(pBBoxRaster, min, max, crWorldMat, crChunks, cIsVegetation, &heigthMapVal);
				else
					m_RasterMPMan.PushProjectTrianglesOS(pBBoxRaster, min, max, crWorldMat, crChunks, cIsVegetation, NULL);
#if defined(DEBUG_RASTER_TABLES)
				static int test = 0;
				char buf[255];
				itoa(test, buf, 10);
				string baseDir("e:/downloads/test");
				baseDir += buf;
				//synchronize before accessing raster data
				m_RasterMPMan.StartProjectTrianglesOS();
				m_RasterMPMan.SynchronizeProjectTrianglesOS();//synchronizes any previous calls to ProjectTrianglesOS
				pBBoxRaster->SaveTGA(baseDir.c_str());
				++test;
#endif   //DEBUG_RASTER_TABLES
				m_BBoxRasterPtrs.push_back(pBBoxRaster);
			}
			rBBoxVec.push_back(STempBBoxData(bbox, newCol, pBBoxRaster, cIsOnGround));
			if (cIsOnGround)//update inner box
			{
				rObjectBox.minInner.x = std::min(rObjectBox.minInner.x, bbox.min.x);
				rObjectBox.minInner.y = std::min(rObjectBox.minInner.y, bbox.min.y);
				rObjectBox.maxInner.x = std::max(rObjectBox.maxInner.x, bbox.max.x);
				rObjectBox.maxInner.y = std::max(rObjectBox.maxInner.y, bbox.max.y);
			}
			if (++addedMatCntr >= CRasterMPMan::scMaxMatChunks)//too many mats already
				break;
		}
	}
	//start bbox rasterization
	m_RasterMPMan.StartProjectTrianglesOS();

	return onGround;
}

const float CTerrainGIGen::GetDistToGround(const float* cpHeightMap, const AABB& crBBox, const float cWorldHMapScale, const uint32 cHeigthMapRes) const
{
	//sample dist to ground in center and on the 2 extensions (reason to do this: consider a bridge where center is far off ground but edges not)
	//if minimum z is below the sample height above ground, mark as on the ground
	const Vec3 cPos[3] = { (crBBox.max + crBBox.min) * 0.5f, Vec3(crBBox.max.x, crBBox.max.y, crBBox.min.z), crBBox.min };
	float minDistToGround = std::numeric_limits<float>::max();
	for (int i = 0; i < 3; ++i)
	{
		//ensure terrain constraints
		const uint32 cX = std::max(0, std::min((int32)(cWorldHMapScale * cPos[i].x), (int32)(cHeigthMapRes - 1)));
		const uint32 cY = std::max(0, std::min((int32)(cWorldHMapScale * cPos[i].y), (int32)(cHeigthMapRes - 1)));
		//WORLD_HMAP_COORDINATE_EXCHANGE
		const float cHeightMapZ = cpHeightMap[cX * cHeigthMapRes + cY];
		const float cDistToGround = std::max(0.f, (crBBox.min.z - cpHeightMap[cX * cHeigthMapRes + cY]));
		minDistToGround = std::min(minDistToGround, cDistToGround);
	}
	return minDistToGround;
}

const float CTerrainGIGen::GetAverageMaterialAlpha(const std::vector<ColorF>& crSubMat) const
{
	int validMatCnt = 0;
	float aveAlpha = 0.f;
	const std::vector<ColorF>::const_iterator cEnd = crSubMat.end();
	for (std::vector<ColorF>::const_iterator iter = crSubMat.begin(); iter != cEnd; ++iter)
	{
		const ColorF& crCol = *iter;
		if (crCol.r > 0.f || crCol.g > 0.f || crCol.b > 0.f)//no proxy mat
		{
			aveAlpha += crCol.a;
			++validMatCnt;
		}
	}
	aveAlpha *= 1.f / (float)validMatCnt;
	return aveAlpha;
}

const bool CTerrainGIGen::PrepareBrush
(
  CBaseObject* pBaseObj,
  const float cWaterLevel,
  CTerrainObjectMan& rTerrainMan,
  const float* cpHeightMap,
  const uint32 cWidthUnits,
  const uint32 cHeigthMapRes,
  const float cWorldHMapScale,
  AABB& rBoxObject
)
{
	if (!pBaseObj->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		return false;
	}
	CBrushObject* pObj = (CBrushObject*)pBaseObj;
	assert(pObj);

	//	if(pObj->IsHidden())
	//		return false;

	if (pObj->IncludeForGI() && pObj->ApplyIndirLighting())
	{
		IRenderNode* pRenderNode = pObj->GetEngineNode();
		if (!pRenderNode)
		{
			return true;
		}
		//check vis area
		if (pRenderNode->GetEntityVisArea() == NULL /* && !(pRenderNode->GetRndFlags() & ERF_HIDDEN)*/)
		{
			//get pos, material(colour) and bounding box
			TTempTerrainObject obj;
			const Vec3& crObjPos = pRenderNode->GetPos();
			//WORLD_HMAP_COORDINATE_EXCHANGE
			SetSectorUsed(crObjPos.y, crObjPos.x, scSectorUsedByNonVeg);
			if (crObjPos.z < cWaterLevel)
			{
				return true;
			}
			if (crObjPos.x < 0.f || crObjPos.y < 0.f || crObjPos.x >= (float)cWidthUnits || crObjPos.y >= (float)cWidthUnits)
			{
				return true;//off terrain
			}
			IStatObj* pStatObj = pObj->GetIStatObj();
			assert(pStatObj);
			//hack: catch reference planes, no other way to ensure it right now
			{
				IMaterial* pMat = pRenderNode ? pRenderNode->GetMaterial() : NULL;
				if (pMat)
				{
					const string cMatName(pMat->GetName());
					if (cMatName.find("reference") != string::npos || cMatName.find("Reference") != string::npos || cMatName.find("_plane") != string::npos)
					{
						return true;
					}
				}
			}
			AABB bbox = pRenderNode->GetBBox();
			SObjectBBoxes object;
			const Vec3 cExt = bbox.GetSize();
			if (std::max(cExt.x, cExt.y) > scMaxObjExt)
			{
				return true;//too large
			}
			if (bbox.max.x >= cWidthUnits || bbox.max.y >= cWidthUnits || bbox.min.x < 0.f || bbox.min.y < 0.f)
			{
				return true;//invalid coords
			}
			std::vector<std::vector<int>> matVec; //vector containing indices into materials, for each sub object one
			std::vector<ColorF> materials;
			GenMaterialVec(pStatObj, pRenderNode, matVec, materials);

			//was:  object.pos = crObjPos;
			object.pos.x = (bbox.max.x + bbox.min.x) * 0.5f;
			object.pos.y = (bbox.max.y + bbox.min.y) * 0.5f;

			//WORLD_HMAP_COORDINATE_EXCHANGE
			SetSectorUsed(object.pos.y, object.pos.x, scSectorUsedByNonVeg);

			TVec2 intPos = (TVec2)(TVec2F(object.pos.x, object.pos.y));
			obj.posX = intPos.x;
			obj.posY = intPos.y;

			object.pos.z = std::max(crObjPos.z, (bbox.max.z + bbox.min.z) * 0.5f);
			object.centerZ = crObjPos.z;
			object.terrainObjManId = 0xFFFFFFFF;
			AABB newMinMax;
			const bool cIsOnGround = ProcessMesh
			                         (
			  pStatObj,
			  bbox,
			  pObj->GetWorldTM(),
			  matVec,
			  materials,
			  obj.bboxCol,
			  false,
			  cpHeightMap,
			  cWorldHMapScale,
			  cHeigthMapRes,
			  object,
			  false,
			  newMinMax
			                         );
			if (object.distToGround - object.maxAxisExtension > scMaxDistToGroundNoRayCast)
				return true;//when a not too big object is too far off ground, ignore it completely
			//offset sample when bounding box is on ground and has little height
			if (!obj.bboxCol.empty() && rTerrainMan.AddObject
			    (
			      bbox.max.z - bbox.min.z,
			      obj,
			      object.terrainObjManId,
			      false,
			      (cIsOnGround && bbox.GetSize().z <= CHemisphereSink_SH::scSampleHeightOffset
			      )))
			{
				m_ObjectBBoxesXYBrushes.push_back(object);
				rBoxObject = bbox;
			}
		}
	}
	return true;
}

// Interface to indirect lighting engine (not used)
struct IIndirectLighting
{
	struct SVoxelInfo
	{
		IRenderNode* pRenderNode;
		IRenderMesh* pRenderMesh;
		Matrix34     mat34;
		SVoxelInfo() : pRenderNode(NULL), pRenderMesh(NULL)
		{
			mat34.SetIdentity();
		}
	};

	// Description:
	//		 retrieves the required information about terrain voxels
	//		 pass NULL to retrieve just the count
	virtual void GetTerrainVoxelInfo(SVoxelInfo* ppVoxels, uint32& rCount) = 0;

	// Description:
	//		 retrieves existing terrain accessibility data
	virtual void RetrieveTerrainAcc(uint8* pDst, const uint32 cWidth, const uint32 cHeight) = 0;
};

void CTerrainGIGen::AddTerrainVoxels
(
  const float cWaterLevel,
  CTerrainObjectMan& rTerrainMan,
  const float* cpHeightMap,
  const uint32 cHeigthMapRes,
  const float cWorldHMapScale
)
{
	//get all terrain voxel render nodes
	IIndirectLighting* pIL = NULL;//gEnv->p3DEngine->GetIIndirectLighting();
	if (!pIL)
		return;
	uint32 nodeCount = 0;
	pIL->GetTerrainVoxelInfo(NULL, nodeCount);
	if (nodeCount == 0)
		return;
	std::vector<IIndirectLighting::SVoxelInfo> voxelRenderNodes(nodeCount);
	nodeCount = 0;
	pIL->GetTerrainVoxelInfo(&voxelRenderNodes[0], nodeCount);
	//nodeCount is now the actual count -> nodeCount <= voxelRenderNodes.size()
	for (int i = 0; i < nodeCount; ++i)
	{
		IIndirectLighting::SVoxelInfo& rVoxelInfo = voxelRenderNodes[i];
		IRenderNode* pRenderNode = rVoxelInfo.pRenderNode;
		if (!rVoxelInfo.pRenderMesh || !pRenderNode || pRenderNode->GetEntityVisArea() != NULL)
			continue;
		//get pos, material(colour) and bounding box
		TTempTerrainObject obj;
		AABB bbox = pRenderNode->GetBBox();
		//use object position from center of bounding box
		const Vec3 cObjPos = (bbox.max + bbox.min) * 0.5f;
		std::vector<std::vector<int>> matVec; //vector containing indices into materials -> not used here
		std::vector<ColorF> materials(1);
		//WORLD_HMAP_COORDINATE_EXCHANGE
		materials[0] = ColorF(1.f, 1.f, 1.f, 1.f);//will be set later on
		//for entities follow another fallback to gain default material
		const Vec3 cExt = bbox.GetSize();
		const float cMaxExtXY = std::max(cExt.x, cExt.y);
		const float cMaxExt = std::max(cMaxExtXY, cExt.z);
		SObjectBBoxes object;
		object.pos = cObjPos;
		AABB newMinMax;
		ProcessMesh
		(
		  NULL,
		  bbox,
		  rVoxelInfo.mat34,
		  matVec,
		  materials,
		  obj.bboxCol,
		  false,
		  cpHeightMap,
		  cWorldHMapScale,
		  cHeigthMapRes,
		  object,
		  true,
		  newMinMax,
		  rVoxelInfo.pRenderMesh
		);
		object.centerZ = cObjPos.z;
		object.pos = (newMinMax.min + newMinMax.max) * 0.5f;//set center to the center of the valid bounding box
		if (object.pos.z < cWaterLevel)
			continue;

		TVec2 intPos = (TVec2)(TVec2F(object.pos.x, object.pos.y));
		obj.posX = intPos.x;
		obj.posY = intPos.y;

		//adjust inner and outer bounding boxes for samples to be placed around
		object.minInner.x = object.maxInner.x = object.pos.x;
		object.minInner.y = object.maxInner.y = object.pos.y;
		object.minOuter.x = newMinMax.min.x;
		object.minOuter.y = newMinMax.min.y;
		object.maxOuter.x = newMinMax.max.x;
		object.maxOuter.y = newMinMax.max.y;

		//WORLD_HMAP_COORDINATE_EXCHANGE
		SetSectorUsed(object.pos.y, object.pos.x, scSectorUsedByNonVeg);

		object.terrainObjManId = 0xFFFFFFFF;
		if (!obj.bboxCol.empty())
		{
			//adjust color to the retrieved voxel center
			obj.bboxCol[0].col.a = 1.f;
			rTerrainMan.AddObject(bbox.max.z - bbox.min.z, obj, object.terrainObjManId, false, false);
			m_ObjectBBoxesXYVoxels.push_back(object);
		}
	}
}

const bool CTerrainGIGen::PrepareEntity
(
  CBaseObject* pBaseObj,
  const float cWaterLevel,
  CTerrainObjectMan& rTerrainMan,
  const float* cpHeightMap,
  const uint32 cWidthUnits,
  const uint32 cHeigthMapRes,
  const float cWorldHMapScale,
  const bool cIsPrefab
)
{
	if (!pBaseObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		return false;
	CEntityObject* pEntity = (CEntityObject*)pBaseObj;
	assert(pEntity);
	//	if(pEntity->IsHidden())
	//		return true;
	//	CEntityScript *pEntityScript = pEntity->GetScript();
	//	if(!pEntityScript || !pEntityScript->GetFile() || pEntityScript->GetFile().MakeLower().Find("vehiclepool") == -1)//if no vehicle
	{
		IRenderNode* pRenderNode = NULL;
		if (!pEntity->GetIEntity())
			return true;
		const Vec3& crEntityPos = pEntity->GetPos();
		if (crEntityPos.x < 0.f || crEntityPos.y < 0.f || crEntityPos.x >= (float)cWidthUnits || crEntityPos.y >= (float)cWidthUnits)
			return true;//off terrain
		//WORLD_HMAP_COORDINATE_EXCHANGE
		SetSectorUsed(crEntityPos.y, crEntityPos.x, scSectorUsedByNonVeg);
#if !defined(USE_ENTITIES)
		return true;
#endif
		IEntityRender* pIEntityRender = pEntity->GetIEntity()->GetRenderInterface();
		if (!pIEntityRender || !(pRenderNode = pIEntityRender->GetRenderNode()))
			return true;
		//check vis area
		if (pRenderNode->GetEntityVisArea() == NULL /* && !(pRenderNode->GetRndFlags() & ERF_HIDDEN)*/)
		{
			//get pos, material(colour) and bounding box
			TTempTerrainObject obj;
			const Vec3& crObjPos = pRenderNode->GetPos();
			if (crObjPos.z < cWaterLevel)
				return true;
			if (crObjPos.x < 0.f || crObjPos.y < 0.f || crObjPos.x >= (float)cWidthUnits || crObjPos.y >= (float)cWidthUnits)
				return true;//off terrain
			const TVec2 intPos = (TVec2)(TVec2F(crObjPos.x, crObjPos.y));
			obj.posX = intPos.x;
			obj.posY = intPos.y;

			IStatObj* pStatObj = pRenderNode->GetEntityStatObj();

			std::vector<std::vector<int>> matVec; //vector containing indices into materials, for each sub object one
			std::vector<ColorF> materials;
			ColorF defCol(0.5f, 0.5f, 0.5f, 1.f);
			//for entities follow another fallback to gain default material
			IMaterial* pMat = NULL;
			if (pEntity->GetRenderMaterial())
				pMat = pEntity->GetRenderMaterial()->GetMatInfo();
			if (pMat)
			{
				std::vector<ColorF> mats;
				GetMaterialColor(pMat, mats, defCol, true);
			}
			GenMaterialVec(pStatObj, pRenderNode, matVec, materials, defCol);
			AABB bbox = pRenderNode->GetBBox();
			const Vec3 cExt = bbox.GetSize();
			const float cMaxExtXY = std::max(cExt.x, cExt.y);
			const float cMaxExt = std::max(cMaxExtXY, cExt.z);
			if (!cIsPrefab && (cMaxExt < scMinLargeEntityXYExtension || cMaxExtXY > scMaxLargeEntityXYExtension))
				return true;//consider only large or prefab entities
			SObjectBBoxes object;
			object.pos = crObjPos;
			object.pos.z = std::max(crObjPos.z, (bbox.max.z + bbox.min.z) * 0.5f);
			AABB newMinMax;
			const bool cIsOnGround = ProcessMesh
			                         (
			  pStatObj,
			  bbox,
			  pEntity->GetWorldTM(),
			  matVec,
			  materials,
			  obj.bboxCol,
			  false,
			  cpHeightMap,
			  cWorldHMapScale,
			  cHeigthMapRes,
			  object,
			  false,
			  newMinMax
			                         );
			if (object.distToGround - object.maxAxisExtension > scMaxDistToGroundNoRayCast)
				return true;//when a not too big object is too far off ground, ignore it completely
			object.centerZ = crObjPos.z;
			object.terrainObjManId = 0xFFFFFFFF;
			if
			(
			  !obj.bboxCol.empty() &&
			  rTerrainMan.AddObject
			  (
			    bbox.max.z - bbox.min.z,
			    obj,
			    object.terrainObjManId,
			    false,
			    (cIsOnGround && bbox.GetSize().z <= CHemisphereSink_SH::scSampleHeightOffset
			    ))
			)
				m_ObjectBBoxesXYLargeEntities.push_back(object);
		}
	}
	return true;
}

void CTerrainGIGen::GenMaterialVec
(
  IStatObj* pStatObj,
  IRenderNode* pRenderNode,
  std::vector<std::vector<int>>& rMats, //each sub object contains sub materials which are indices
  std::vector<ColorF>& rMaterials,      //the real colours
  const ColorF cDefMat
)
{
	//rMats vector containes indices into materials, for each sub object one
	//generate for each sub object a material, will be quantized later
	std::vector<std::vector<ColorF>> matCols;
	matCols.resize(0);
	rMaterials.resize(0);
	const int cSubObjCnt = pStatObj ? pStatObj->GetSubObjectCount() : 0;
	int curObjIndex = 0;
	IStatObj* pParentStatObj = pStatObj;//follow only one indirection
	//first
	if (pStatObj)
	{
		while (pStatObj)
		{
			IMaterial* pMat = pStatObj->GetMaterial();
			std::vector<ColorF> matVec;
			if (pMat)
			{
				ColorF col;
				GetMaterialColor(pMat, matVec, col, true);
			}
			else
				matVec.push_back(cDefMat);
			matCols.push_back(matVec);
			bool updated = false;
			while (!updated)
			{
				pStatObj = (curObjIndex < cSubObjCnt) ? pParentStatObj->GetSubObject(curObjIndex++)->pStatObj : NULL;
				if (pStatObj)
					updated = true;
				if (curObjIndex >= cSubObjCnt)
					updated = true;
			}
		}
	}
	else
	{
		//get material from render node and add only one
		IMaterial* pMat = pRenderNode ? pRenderNode->GetMaterial() : NULL;
		std::vector<ColorF> matVec;
		if (!pMat)
		{
			matVec.push_back(cDefMat);
			matCols.push_back(matVec);
		}
		else
		{
			ColorF col;
			GetMaterialColor(pMat, matVec, col, true);
		}
		matCols.push_back(matVec);
	}

	//now quantize material colours and generate index plus colour vector
	const uint32 cSubMatCount = matCols.size();
	rMats.resize(cSubMatCount);
	for (uint32 i = 0; i < cSubMatCount; ++i)
	{
		const uint32 cSubSubCount = matCols[i].size();
		rMats[i].resize(cSubSubCount);
		for (int j = 0; j < cSubSubCount; ++j)
		{
			//quantize into rMaterials
			const ColorF& crCol = matCols[i][j];
			const uint32 cCurMaxIndex = rMaterials.size();
			uint32 colIndex;
			for (colIndex = 0; colIndex < cCurMaxIndex; ++colIndex)
			{
				const ColorF& crTestCol = rMaterials[colIndex];
				static const float scMaxDiff = 0.2f;
				if (fabs(crTestCol.r - crCol.r) + fabs(crTestCol.g - crCol.g) + fabs(crTestCol.b - crCol.b) + fabs(crTestCol.a - crCol.a) < scMaxDiff)
				{
					//found colour match
					rMats[i][j] = colIndex;
					break;
				}
			}
			if (colIndex >= cCurMaxIndex)
			{
				//add new entry
				rMats[i][j] = cCurMaxIndex;
				rMaterials.push_back(crCol);
			}
		}
	}
}

void CTerrainGIGen::PrepareObjects
(
  const float cWaterLevel,
  CTerrainObjectMan& rTerrainMan,
  const float* cpHeightMap,
  const uint32 cWidthUnits,
  const uint32 cHeigthMapRes
)
{
	//pos and bounding boxes are divided by the heightmap unit scale since we are working in the heightmap texture space itself
	static const ColorF scTruncCol(80.f / 255.f, 50.f / 255.f, 20.f / 255.f);
	static const ColorF scLeavesCol(140.f / 255.f, 200.f / 255.f, 110.f / 255.f, 0.5f);

	std::set<void*> addedObjects;//will track the already added objects to never include an object twice (prefabs contain objects refered to elsewhere too)

	const float cWorldHMapScale = (float)cHeigthMapRes / (float)cWidthUnits;

	//add all brushes and vegetation objects on the terrain and insert it with type TTempTerrainObject
	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	std::vector<CVegetationInstance*> vegVec;
	pVegetationMap->GetObjectInstances(0, 0, m_pHeightMap->GetWidth() * m_UnitSize, m_pHeightMap->GetHeight() * m_UnitSize, vegVec);//get all veg instances

	IObjectManager* pObjManager = GetIEditorImpl()->GetObjectManager();
	CBaseObjectsArray objects;
	pObjManager->GetObjects(objects);

	const uint32 cObjectNum = vegVec.size() + objects.size();

	m_BBoxRasterPtrs.reserve(cObjectNum << 1);//2 per valid material

	uint32 curObj = 0;
	const bool cUseWait = cObjectNum > 50;
	CWaitProgress progress("Prepare Indirect Lighting...", false);
	if (cUseWait)
		progress.Start();

	const int cObjectsSize = objects.size();
	//we need a corresponding vector of bounding boxes for each brush
	std::vector<AABB> brushBoxes;
	brushBoxes.reserve(cObjectsSize);
	//add the brushes
	for (int i = 0; i < cObjectsSize; ++i)
	{
		if (objects[i]->GetType() == OBJTYPE_BRUSH || objects[i]->GetType() == OBJTYPE_PREFAB)
		{
			if (cUseWait)
				progress.Step((100 * curObj++) / cObjectNum);
			AABB box;
			if (objects[i]->GetType() == OBJTYPE_PREFAB)
			{
#if defined(USE_PREFABS)
				CGroup* pGroup = (CGroup*)objects[i];
				for (int o = 0; o < pGroup->GetChildCount(); ++o)
				{
					CBaseObject* pBaseObj = pGroup->GetChild(o);
					if (pBaseObj)
					{
						if (addedObjects.find(pBaseObj) == addedObjects.end())
						{
							const size_t cSizeBefore = m_ObjectBBoxesXYBrushes.size();
							if (PrepareBrush(pBaseObj, cWaterLevel, rTerrainMan, cpHeightMap, cWidthUnits, cHeigthMapRes, cWorldHMapScale, box))
								addedObjects.insert(pBaseObj);
							if (m_ObjectBBoxesXYBrushes.size() > cSizeBefore)//object has been added
								brushBoxes.push_back(box);
						}
					}
				}
#endif
			}
			else
			{
#if defined(USE_BRUSHS)
				const size_t cSizeBefore = m_ObjectBBoxesXYBrushes.size();
				if (addedObjects.find(objects[i]) == addedObjects.end())
				{
					if (PrepareBrush(objects[i], cWaterLevel, rTerrainMan, cpHeightMap, cWidthUnits, cHeigthMapRes, cWorldHMapScale, box))
						addedObjects.insert(objects[i]);
					if (m_ObjectBBoxesXYBrushes.size() > cSizeBefore)//object has been added
						brushBoxes.push_back(box);
				}
#endif  //USE_BRUSHS
			}
		}
	}
	//now add the entities
	for (int i = 0; i < cObjectsSize; ++i)
	{
		if (objects[i]->GetType() == OBJTYPE_ENTITY || objects[i]->GetType() == OBJTYPE_PREFAB)
		{
			if (objects[i]->GetType() == OBJTYPE_PREFAB)
			{
#if defined(USE_PREFABS)
				CGroup* pGroup = (CGroup*)objects[i];
				for (int o = 0; o < pGroup->GetChildCount(); ++o)
				{
					CBaseObject* pBaseObj = pGroup->GetChild(o);
					if (pBaseObj)
					{
						if (addedObjects.find(pBaseObj) == addedObjects.end())
						{
							if (PrepareEntity(pBaseObj, cWaterLevel, rTerrainMan, cpHeightMap, cWidthUnits, cHeigthMapRes, cWorldHMapScale, true))
								addedObjects.insert(pBaseObj);
						}
					}
				}
#endif
			}
			else
			{
				if (addedObjects.find(objects[i]) == addedObjects.end())
				{
					if (PrepareEntity(objects[i], cWaterLevel, rTerrainMan, cpHeightMap, cWidthUnits, cHeigthMapRes, cWorldHMapScale, false))
						addedObjects.insert(objects[i]);
				}
			}
			if (cUseWait)
				progress.Step((100 * curObj++) / cObjectNum);
		}
	}

	curObj = cObjectsSize;
#if defined(USE_VEGETATION)
	const std::vector<CVegetationInstance*>::const_iterator cEnd = vegVec.end();
	for (std::vector<CVegetationInstance*>::const_iterator iter = vegVec.begin(); iter != cEnd; ++iter)
	{
		if (cUseWait)
			progress.Step((100 * curObj++) / cObjectNum);
		const CVegetationInstance& crVeg = *(*iter);
		IRenderNode* pRenderNode = crVeg.pRenderNode;
		if (!pRenderNode || crVeg.pos.z < cWaterLevel)
			continue;
		//neglect veg rotation since it does not matter much here
		TTempTerrainObject obj;
		assert(crVeg.pos.x >= 0.f);
		assert(crVeg.pos.y >= 0.f);
		TVec2 intPos = (TVec2)(TVec2F(crVeg.pos.x, crVeg.pos.y));
		obj.posX = intPos.x;
		obj.posY = intPos.y;
		CVegetationObject* pVegObject = crVeg.object;
		assert(pVegObject);
		IStatObj* pStatObj = pVegObject->GetObject();
		assert(pStatObj);
		AABB bbox = pRenderNode->GetBBox();

		SObjectBBoxes object;
		object.pos = crVeg.pos;

		//WORLD_HMAP_COORDINATE_EXCHANGE
		SetSectorUsed(object.pos.y, object.pos.x, scSectorUsedByVeg);

		const AABB cHeightBox = crVeg.object->GetObject()->GetAABB();
		const float cEncodeHeight = cHeightBox.max.z - cHeightBox.min.z;

		//if vegetation is too small, do not consider it (storage waster, procedural path does the job)
		const float cHeightDiff = bbox.max.z - bbox.min.z;
		if (cHeightDiff < scMinBBoxVegHeight)
			continue;

		if (!pVegObject->IsCastShadow())
			continue;

		Matrix34 instMat;
		instMat.SetRotationZ(crVeg.angle);
		instMat.SetScale(Vec3(crVeg.scale, crVeg.scale, crVeg.scale));
		instMat.SetTranslation(crVeg.pos);

		std::vector<std::vector<int>> matVec; //vector containing indices into materials, for each sub object one
		std::vector<ColorF> materials;
		const bool cIsFrozen = (pVegObject->GetFrozen() == true);
		GenMaterialVec(pStatObj, pRenderNode, matVec, materials);

		//place not too far off ground
		object.pos.z = std::min(crVeg.pos.z + 1.5f, (bbox.max.z + bbox.min.z) * 0.5f);
		AABB newMinMax;
		const bool cIsOnGround = ProcessMesh
		                         (
		  pStatObj,
		  bbox,
		  instMat,
		  matVec,
		  materials,
		  obj.bboxCol,
		  true,
		  cpHeightMap,
		  cWorldHMapScale,
		  cHeigthMapRes,
		  object,
		  false,
		  newMinMax
		                         );
		object.centerZ = crVeg.pos.z;
		object.terrainObjManId = 0xFFFFFFFF;
		//offset sample when all materials are opaque (base of plants)
		if
		(
		  !obj.bboxCol.empty() &&
		  rTerrainMan.AddObject
		  (
		    cHeightDiff,
		    obj,
		    object.terrainObjManId,
		    true,
		    GetAverageMaterialAlpha(materials) == 1.f
		  )
		)
			m_ObjectBBoxesXYVegetation.push_back(object);
	}
#endif //USE_VEGETATION
	AddTerrainVoxels(cWaterLevel, rTerrainMan, cpHeightMap, cHeigthMapRes, cWorldHMapScale);

	if (cUseWait)
		progress.Step(100);

	if (cUseWait)
		progress.Stop();

	m_RasterMPMan.SynchronizeProjectTrianglesOS();//synchronizes any previous calls to ProjectTrianglesOS
}

const bool CTerrainGIGen::InterpolateGridPoint
(
  int pClosestNodes[2],
  const uint32 cLeafCount,
  const TQuadTree::TInitialInterpolRetrievalType cpNodes,
  TCoeffType& rInterpolatedCoeffs
) const
{
	pClosestNodes[0] = pClosestNodes[1] = -1;
	if (cLeafCount == 0)
		return false;
	//weight nodes linear with squared distance
	//first sum linear distances
	float distSum = 0.f;
	float dist[TQuadTree::scInitalSliceGranularity];
	assert(cLeafCount < TQuadTree::scInitalSliceGranularity);
	int nonZeroCount = 0;

	float curClosestDist[2] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	for (int i = 0; i < cLeafCount; ++i)
	{
		dist[i] = 0.f;
		const TQuadTree::SInterpolRetrievalType& crCurNode = cpNodes[i];
		assert(crCurNode.cpCont);
		if (crCurNode.distSq < curClosestDist[0])
		{
			pClosestNodes[0] = i;
			curClosestDist[0] = crCurNode.distSq;
		}
		else
		{
			if (crCurNode.distSq < curClosestDist[1])
			{
				pClosestNodes[1] = i;
				curClosestDist[1] = crCurNode.distSq;
			}
		}
		dist[i] = 1.f / std::max(0.01f, crCurNode.distSq);//keep some min distance
		distSum += dist[i];
	}
	//inverse weighting values
	const float cInverseSum = 1.f / distSum;
	for (int i = 0; i < cLeafCount; ++i)
		dist[i] *= cInverseSum;
	for (int i = 0; i < 9; ++i)
		rInterpolatedCoeffs[i][0] = 0.f;//reset coeffs
	//add according to inverse euclidean distance
	for (int i = 0; i < cLeafCount; ++i)
	{
		assert(cpNodes[i].cpCont);
		const float cWeight = dist[i];
		assert(*(cpNodes[i].cpCont) < m_GridPoints.size());
		const TEditorGridPoint& crGridPoint = m_GridPoints[*(cpNodes[i].cpCont)];
		assert(m_GridPoints[*(cpNodes[i].cpCont)].GetFlags() & scValid);
		rInterpolatedCoeffs += crGridPoint.shCoeffs * cWeight;
	}
	return true;
}

void CTerrainGIGen::BuildSectorTable(const uint32 cWidthUnits, const CTerrainObjectMan& crTerrainObjMan)
{
	//now mark each sector as used if at least one neighbor sector is used by the terrain object manager
	std::vector<uint8> sectorTableCopied(m_SectorTable);
	memset(&m_SectorTable[0], scSectorUnused, m_SectorRowNum * m_SectorRowNum);
	for (int i = 0; i < m_SectorRowNum; ++i)
		for (int j = 0; j < m_SectorRowNum; ++j)
		{
			m_SectorTable[j * m_SectorRowNum + i] = sectorTableCopied[j * m_SectorRowNum + i] |
			                                        sectorTableCopied[std::max(j - 1, 0) * m_SectorRowNum + i] |
			                                        sectorTableCopied[std::min(j + 1, m_SectorRowNum - 1) * m_SectorRowNum + i] |
			                                        sectorTableCopied[std::max(j - 1, 0) * m_SectorRowNum + std::max(i - 1, 0)] |
			                                        sectorTableCopied[j * m_SectorRowNum + std::max(i - 1, 0)] |
			                                        sectorTableCopied[std::min(j + 1, m_SectorRowNum - 1) * m_SectorRowNum + std::max(i - 1, 0)] |
			                                        sectorTableCopied[std::max(j - 1, 0) * m_SectorRowNum + std::min(i + 1, m_SectorRowNum - 1)] |
			                                        sectorTableCopied[j * m_SectorRowNum + std::min(i + 1, m_SectorRowNum - 1)] |
			                                        sectorTableCopied[std::min(j + 1, m_SectorRowNum - 1) * m_SectorRowNum + std::min(i + 1, m_SectorRowNum - 1)];
		}
}

void CTerrainGIGen::AddSamplesAroundObjects
(
  CTerrainObjectMan& rTerrainObjMan,
  THeightmapAccess& rHMapCalc,
  TObjectVec& rObjectVec,
  const uint8 cResMultiplier,
  const float* cpHeightMap,
  const uint8 cMaxMultiplier,
  const EObjectType cObjectType
)
{
	//important note for additional samples around objects: it detects the demanded position and if there is
	//an available sample pos (grid has higher resolution than the coarse sample grid), it adds that pos
	//if higher precision is required, it add a sample by invoking AddSample on the SHHeightmapAcc calc.
	const uint32 cWidth = m_pHeightMap->GetWidth();
	const uint32 cWidthUnits = m_UnitSize * cWidth;
	const uint32 cObjectMul = std::max(m_Config.vegResMultiplier, m_Config.brushResMultiplier);
	const uint32 cHeightmapHighResWidth = cWidthUnits / m_Config.baseGranularity * cObjectMul;
	const uint32 cWorldToHMapMul = cWidthUnits / cHeightmapHighResWidth;
	uint32 worldToHMapMulShift = 0;
	while ((cHeightmapHighResWidth << worldToHMapMulShift) != cWidthUnits)
		++worldToHMapMulShift;
	assert((cHeightmapHighResWidth << worldToHMapMulShift) == cWidthUnits);

	const uint32 cConvertFactor = m_Config.baseGranularity / cResMultiplier;
	uint32 convertFactorShift = 0;
	while ((cResMultiplier << convertFactorShift) != m_Config.baseGranularity)
		++convertFactorShift;
	assert((cResMultiplier << convertFactorShift) == m_Config.baseGranularity);

	uint32 obstructionIndex = 0xFFFFFFFF;//dummy
	float obstructionAmount = 1.f;//dummy
	const uint32 cCalcWidth = rHMapCalc.m_Sink.GetGridResolution();

	//sample refinement constants
	const float cInvUnitSize = 1.f / m_UnitSize;

	int32 baseGranularityShift = 0;//replace / m_Config.baseGranularity by >> baseGranularityShift
	while ((1 << baseGranularityShift) != m_Config.baseGranularity)
		++baseGranularityShift;

	const int32 cMaxGranularity = m_Config.baseGranularity / cMaxMultiplier;
	int32 maxGranularityShift = 0;//replace / m_Config.baseGranularity by >> baseGranularityShift
	while ((1 << maxGranularityShift) != cMaxGranularity)
		++maxGranularityShift;

	const int32 cMaxInnerGranularity = (cMaxGranularity >> 1) + 1;

	assert(m_TerrainMapRes >= cHeightmapHighResWidth);
	int32 imageToTerrainShift = 0;
	while ((cHeightmapHighResWidth << imageToTerrainShift) != m_TerrainMapRes)
		++imageToTerrainShift;
	assert((cHeightmapHighResWidth << imageToTerrainShift) == m_TerrainMapRes);

	//iterate all bounding boxes and add according to higher resolution within m_Config.baseGranularity distance to bbox border, more samples
	//(at resolution grid points)
	//idea: place samples some rows around object and outside the objects on the ground (basically around the trunc for vegetation and outside the whole tree)
	static const int32 scEnlargementUnits = 1;//refinement units (determined by size)
	const TObjectVec::const_iterator cObjectEnd = rObjectVec.end();
	for (TObjectVec::iterator iter = rObjectVec.begin(); iter != cObjectEnd; ++iter)
	{
		SObjectBBoxes& rObject = *iter;
		if ((uint16)rObject.pos.x >= (uint16)cWidthUnits || (uint16)rObject.pos.y >= (uint16)cWidthUnits)
			continue;//dont place samples around too small objects
		//determine if non veg objects are in the sector as well
		//WORLD_HMAP_COORDINATE_EXCHANGE
		const bool cNonVegPresent = IsSectorUsedByNonVeg(rObject.pos.y, rObject.pos.x);
		assert((cObjectType == eOT_Veg) || cNonVegPresent);
		const int32 cEnlargementUnits = (rObject.zExt > 8.f) ? (scEnlargementUnits + 1) : scEnlargementUnits;//with 8 m height one unit more
		const TVec2F& crMin(rObject.minOuter);
		const TVec2F& crMax(rObject.maxOuter);
		TVec2F minInner(rObject.minInner);
		TVec2F maxInner(rObject.maxInner);
		//do ray casting if object type is not entity, if brushes are not too large or far off ground
		bool doObjectRayCasting = (cObjectType != eOT_Entity) && ((cObjectType == eOT_Veg) ||
		                                                          ((rObject.maxAxisExtension < scMaxObjectSampleExtension) && (rObject.distToGround < scMaxDistToGroundRC)));
		//add object link computation (find sample most close to center and set object link to it)
		const TVec2F cCenterPos((crMin + crMax) * 0.5f);
		uint32 cGridPosX = ((uint32)cCenterPos.x >> convertFactorShift) << convertFactorShift;
		uint32 cGridPosY = ((uint32)cCenterPos.y >> convertFactorShift) << convertFactorShift;
		cGridPosX += (cCenterPos.x - cGridPosX) >= (cConvertFactor >> 1) ? cConvertFactor : 0;
		cGridPosY += (cCenterPos.y - cGridPosY) >= (cConvertFactor >> 1) ? cConvertFactor : 0;
		cGridPosX = std::min(cGridPosX, cWidthUnits - 1);//ensure boundaries
		cGridPosY = std::min(cGridPosY, cWidthUnits - 1);//ensure boundaries
		//WORLD_HMAP_COORDINATE_EXCHANGE
		const unsigned int cGridIndexI = cGridPosY >> worldToHMapMulShift;
		const unsigned int cGridIndexJ = cGridPosX >> worldToHMapMulShift;
		SSHSample& rSample = rHMapCalc.GetSampleAt(cGridIndexI, cGridIndexJ);
		if (cObjectType != eOT_Voxel)
		{
			//now ensure object sample does not stuck in one of the bounding boxes of the neighbor objects
			Vec3 objectPos(rObject.pos);
			const TVec2 cObjectPos = (TVec2)(TVec2F(objectPos.y, objectPos.x));
			if (cObjectType == eOT_Veg && rObject.maxZ <= 0.f)
				continue;//dont place samples around completely obstructed objects or low vegetation objects
			//add sample on top of object to get vis progression
			//make sure it gets inserted after the actual sample
			if ((cObjectType == eOT_Brush) && rObject.maxAxisExtension < scMinBBoxBrushHeight)
				continue;//dont place samples around too small objects
			//get bbox from raster
			const TTerrainObject& crTerrainObj = rTerrainObjMan.GetTerrainObjectByID(rObject.terrainObjManId);
			//get bbox from raster
			AABB xyBBox(Vec3(minInner.x, minInner.y, 0.f), Vec3(maxInner.x, maxInner.y, 0.f));
			const float cMiddleHeight = CHemisphereSink_SH::scSampleHeightOffset +
			                            GetFilteredHeight
			                            (
			  (uint16)(crMax.y + crMin.y) * 0.5f * cInvUnitSize,
			  (uint16)(crMax.x + crMin.x) * 0.5f * cInvUnitSize,
			  cHeightmapHighResWidth,
			  m_TerrainMapRes,
			  cpHeightMap,
			  imageToTerrainShift
			                            );
			const uint32 cBoxCnt = crTerrainObj.bboxCol.size();
			bool firstTime = true;
			for (int b = 0; b < cBoxCnt; ++b)
			{
				const SBBoxData& crBoxData = crTerrainObj.bboxCol[b];
				const float cCenterHeight = crBoxData.bbox.min.z + (crBoxData.bbox.max.z - crBoxData.bbox.min.z) * 0.5f;
				if (crBoxData.onGround && crBoxData.pBBoxRaster)
				{
					AABB xyBBoxTemp(xyBBox);
					if (crBoxData.pBBoxRaster->RetrieveXYBBox(xyBBoxTemp, cMiddleHeight - cCenterHeight))
					{
						if (firstTime)
						{
							xyBBox.min.x = xyBBoxTemp.min.x;
							xyBBox.min.y = xyBBoxTemp.min.y;
							xyBBox.max.x = xyBBoxTemp.max.x;
							xyBBox.max.y = xyBBoxTemp.max.y;
							firstTime = false;
						}
						else
						{
							xyBBox.min.x = std::min(xyBBox.min.x, xyBBoxTemp.min.x);
							xyBBox.min.y = std::min(xyBBox.min.y, xyBBoxTemp.min.y);
							xyBBox.max.x = std::max(xyBBox.max.x, xyBBoxTemp.max.x);
							xyBBox.max.y = std::max(xyBBox.max.y, xyBBoxTemp.max.y);
						}
					}
				}
			}
			minInner.x = xyBBox.min.x;
			minInner.y = xyBBox.min.y;
			maxInner.x = xyBBox.max.x;
			maxInner.y = xyBBox.max.y;
		}//cObjectType == eOT_Voxel
		 //now add grid points being within the large bounding box and outside the object bounding box itself, use specific higher	resolution
		 //quadtree will automatically take care of duplicated positions, so dont treat here any already inserted positions, just check return value
		 //only add when above water level
		 //compute start and end values in x and y, convert to grid pos and add exactly cEnlargementUnits sample rows in minus and plus x/y
		 //world space coords, clamp
		int32 cStartX = (int32)std::max(0.f, crMin.x);
		int32 cTempStartX = (cStartX >> convertFactorShift) << convertFactorShift;
		if (cTempStartX == cStartX)//starts at grid pos, will be dropped by inner object test
			cStartX = std::max((int32)0, cTempStartX - (cEnlargementUnits << convertFactorShift));
		else
			cStartX = std::max((int32)0, cTempStartX - ((cEnlargementUnits - 1) << convertFactorShift));
		int32 cStartY = (int32)std::max(0.f, crMin.y);
		int32 cTempStartY = (cStartY >> convertFactorShift) << convertFactorShift;
		if (cTempStartY == cStartY)//starts at grid pos, will be dropped by inner object test
			cStartY = std::max((int32)0, cTempStartY - (cEnlargementUnits << convertFactorShift));
		else
			cStartY = std::max((int32)0, cTempStartY - ((cEnlargementUnits - 1) << convertFactorShift));
		const int32 cEndX = std::max((int32)0, std::min((int32)(cWidthUnits - 1), (((int32)crMax.x >> convertFactorShift) << convertFactorShift) + (cEnlargementUnits << convertFactorShift)));
		const int32 cEndY = std::max((int32)0, std::min((int32)(cWidthUnits - 1), (((int32)crMax.y >> convertFactorShift) << convertFactorShift) + (cEnlargementUnits << convertFactorShift)));
		//remember world coords and heightmap coords are flipped
		//idea: in the inner ring, place a sample within 1 unit of distance
		//otherwise all 2 units, check if it is on an existing grid pos, if not add an additional sample
		//apply sample refinement by placing additional samples
		const int32 cMaxOuterRing = cMaxGranularity * (cEnlargementUnits / 2);

		//check if it is worth to have a sample each unit
		const bool c1UnitSample = (cObjectType != eOT_Voxel) && cNonVegPresent;

		for (int32 x = cStartX; x <= cEndX; ++x)
		{
			for (int32 y = cStartY; y <= cEndY; ++y)
			{
				const TVec2F cPos(x, y);
				//WORLD_HMAP_COORDINATE_EXCHANGE
				const unsigned int cHighResIndexI = (y >> worldToHMapMulShift);
				const unsigned int cHighResIndexJ = (x >> worldToHMapMulShift);
				SSHSample& rSample = rHMapCalc.GetSampleAt(cHighResIndexI, cHighResIndexJ);
				//test if it is a corner sample
				Vec3 samplePos(cPos.x, cPos.y, 0.f);
				//check if is part of the coarse grid (therefore already existing)
				if (((x >> baseGranularityShift) << baseGranularityShift) == x && ((y >> baseGranularityShift) << baseGranularityShift) == y)
					continue;
				//check if it is part of the refinement grid, use m_AdditionalSamples in  this case
				if (((x >> maxGranularityShift) << maxGranularityShift) == x && ((y >> maxGranularityShift) << maxGranularityShift) == y)
				{
					if (!rSample.IsToBeIgnored() && !rSample.IsOffseted() && !rSample.IsMapped())//not obstructed and not offsetted
					{
						m_AdditionalSamples.push_back(TVec2(cHighResIndexI, cHighResIndexJ));//optimizeable
						rSample.SetMapped();
					}
					continue;
				}
				if (cObjectType == eOT_Voxel)
					continue;
				//check if within the enlargement field
				if ((x > maxInner.x + cMaxOuterRing || y > maxInner.y + cMaxOuterRing ||
				     x < minInner.x - cMaxOuterRing || y < minInner.y - cMaxOuterRing))
					continue;
				const float cHeight = CHemisphereSink_SH::scSampleHeightOffset +
				                      GetFilteredHeight
				                      (
				  (uint16)((float)y * cInvUnitSize),
				  (uint16)((float)x * cInvUnitSize),
				  cHeightmapHighResWidth,
				  m_TerrainMapRes,
				  cpHeightMap,
				  imageToTerrainShift
				                      );
				//for triangle intersections a sample all 2 units is sufficient
				//check if it is in the inner ring (where to place a sample all 1 unit if not vegetation)
				//not for triangle intersections
				if (c1UnitSample && x <= maxInner.x + cMaxInnerGranularity && y <= maxInner.y + cMaxInnerGranularity &&
				    x >= minInner.x - cMaxInnerGranularity && y >= minInner.y - cMaxInnerGranularity)
				{
					//within the first 2 units
					//WORLD_HMAP_COORDINATE_EXCHANGE
					float obstructionAmount = 0.f;
					uint32 obstructionObjectIndex = 0xFFFFFFFF;
					rTerrainObjMan.IsObstructed(y, x, cWidthUnits, obstructionObjectIndex, obstructionAmount, true);
					if (obstructionAmount == 1.f)
						continue;
					samplePos.z = cHeight;
					rHMapCalc.m_Sink.AddSample(rSample, samplePos, true, false);
					continue;
				}
				//check if it is on the 2 unit scale of the outer ring
				if (!cNonVegPresent || ((x >> 1) << 1) != x || ((y >> 1) << 1) != y)
					continue;

				//or the outer ring (where to place a sample all 2 units)
				//WORLD_HMAP_COORDINATE_EXCHANGE
				float obstructionAmount = 0.f;
				uint32 obstructionObjectIndex = 0xFFFFFFFF;
				rTerrainObjMan.IsObstructed(y, x, cWidthUnits, obstructionObjectIndex, obstructionAmount, true);
				if (obstructionAmount == 1.f)
					continue;
				samplePos.z = cHeight;
				rHMapCalc.m_Sink.AddSample(rSample, samplePos, true, false);
			}
		}
	}
}

void CTerrainGIGen::SetDataForAdditionalSamples(const THeightmapAccess& crHMapCalc)
{
	const uint32 cWidth = m_pHeightMap->GetWidth();
	const uint32 cWidthUnits = m_UnitSize * cWidth;
	const uint32 cObjectMul = std::max(m_Config.vegResMultiplier, m_Config.brushResMultiplier);
	const uint32 cHeightmapHighResWidth = cWidthUnits / m_Config.baseGranularity * cObjectMul;
	const uint32 cWorldToHMapMul = cWidthUnits / cHeightmapHighResWidth;
	uint32 curCount = m_GridPoints.size();
	const std::vector<TVec2>::const_iterator cEnd = m_AdditionalSamples.end();
	for (std::vector<TVec2>::const_iterator iter = m_AdditionalSamples.begin(); iter != cEnd; ++iter)
	{
		const SSHSample& crSample = crHMapCalc.GetSampleAt(iter->x, iter->y);
		if (crSample.IsToBeIgnored())//need additional check here since obstructed samples have been added too
			continue;
		TEditorGridPoint gridPoint;
		//WORLD_HMAP_COORDINATE_EXCHANGE
		gridPoint.x = crSample.posY;
		gridPoint.y = crSample.posX;
		AssignSHData(crSample.pSHData->shCoeffs, gridPoint.shCoeffs);
		if (curCount == m_GridMaxCount)
		{
			CLogFile::WriteLine("Maximum number of samples reached, drop remaining");
			return;
		}
		//WORLD_HMAP_COORDINATE_EXCHANGE
		const NQT::EInsertionResult cRes = m_QuadTree.InsertLeaf(TVec2(gridPoint.x, gridPoint.y), m_GridPoints.size());
		assert(NQT::SUCCESS == cRes || NQT::POS_EQUAL_TO_EXISTING_LEAF == cRes);
		if (NQT::SUCCESS == cRes)
		{
			gridPoint.SetFlag(scValid | scOptimizeable);
			m_GridPoints.push_back(gridPoint);
			++curCount;
		}
		else if (NQT::TOO_MANY_LEAVES == cRes)
		{
			CLogFile::WriteLine("Maximum number of quadtree leaves reached, drop remaining");
			return;
		}
	}
}

void CTerrainGIGen::SetCoarseGridValues
(
  const uint32 cWidthUnits,
  const uint32 cHighResWidth,
  const THeightmapAccess& crCalc
)
{
	const uint32 cWorldToGridCoordsMul = cWidthUnits / cHighResWidth;
	uint32 worldToGridCoordsMulShift = 0;
	while ((cHighResWidth << worldToGridCoordsMulShift) != cWidthUnits)
		++worldToGridCoordsMulShift;
	assert((cHighResWidth << worldToGridCoordsMulShift) == cWidthUnits);

	const TGridVec::const_iterator cEnd = m_GridPoints.end();
	for (TGridVec::iterator iter = m_GridPoints.begin(); iter != cEnd; ++iter)
	{
		TEditorGridPoint& rGridPoint = *iter;
		assert(rGridPoint.GetFlags() & scValid);
		const SSHSample& crSample =
		  crCalc.GetSampleAt(rGridPoint.x >> worldToGridCoordsMulShift, rGridPoint.y >> worldToGridCoordsMulShift);
		if (crSample.IsToBeIgnored())
			rGridPoint.UnSetFlag(scValid);
		else
			AssignSHData(crSample.pSHData->shCoeffs, rGridPoint.shCoeffs);
	}
}

void CTerrainGIGen::AddCoarseGridPoints
(
  const float cWaterLevel,
  const uint32 cGridWidth,
  const uint32 cHighResWidth,
  const float* const cpHeightMap,
  const CTerrainObjectMan& crTerrainMan
)
{
	//reserve with 64 MB, any resize will fail anyway
	m_GridMaxCount = 64 * 1024 * 1024 / sizeof(TEditorGridPoint);
	m_GridPoints.reserve(m_GridMaxCount);//reserve max possible size for a single vector
	assert(cGridWidth < (2 << (sizeof(int16) * 8 - 2)));
	const uint32 cHeightmapMul = cHighResWidth / cGridWidth;
	uint32 dummy;
	float obstructionAmount;
	//leave one sample within a range of 2 base units if it is in a sector where at least one object is in a neighbor sector
	bool optimizeable = false;
	bool xStage = false;
	for (uint16 x = 0; x < cGridWidth; ++x)
	{
		optimizeable = xStage;
		xStage = !xStage;
		for (uint16 y = 0; y < cGridWidth; ++y)
		{
			TEditorGridPoint gridPoint;
			gridPoint.x = x * m_Config.baseGranularity;
			gridPoint.y = y * m_Config.baseGranularity;
			//do not add anything for unused sectors
			if (!IsSectorUsed(gridPoint.x, gridPoint.y))
				continue;
			//add only leafs above water level
			if (cpHeightMap[y * cHighResWidth * cHeightmapMul + x * cHeightmapMul] <= cWaterLevel)
				continue;
			uint8 initialFlag = optimizeable ? (scValid | scOptimizeable) : scValid;
			optimizeable = !optimizeable;
			//also add only if not inside an object
			if (crTerrainMan.IsObstructed(x, y, cGridWidth, dummy, obstructionAmount, false) == CTerrainObjectMan::scObstructed)
			{
				//can safely be continued since samples around objects will be placed and translated automatically
				continue;
			}
			const NQT::EInsertionResult cRes = m_QuadTree.InsertLeaf(TVec2(gridPoint.x, gridPoint.y), m_GridPoints.size());
			assert(NQT::SUCCESS == cRes);
			if (NQT::SUCCESS == cRes)
			{
				gridPoint.SetFlag(initialFlag);
				m_GridPoints.push_back(gridPoint);//cannot exceed 64 MB
			}
			else if (NQT::TOO_MANY_LEAVES == cRes)
			{
				CLogFile::WriteLine("Maximum number of quadtree leaves reached, drop remaining");
				return;
			}
		}
	}
}

void CTerrainGIGen::MarkSamples
(
  CTerrainObjectMan& rTerrainObjMan,
  const uint32 cRes,
  THeightmapAccess& rCalc,
  const uint8 cMaxMultiplier,
  const float* cpHeightMap,
  const uint32 cWidthUnits
)
{
	//mark samples outside any ray casting range and samples to be ignored
	//first mark all samples with the double res as needed, the others as not
	const uint32 cMarkStep = cMaxMultiplier / 2;
	//now compute and mark the samples around objects with the corresponding res
	AddSamplesAroundObjects(rTerrainObjMan, rCalc, m_ObjectBBoxesXYVegetation, m_Config.vegResMultiplier, cpHeightMap, cMaxMultiplier, eOT_Veg);
	AddSamplesAroundObjects(rTerrainObjMan, rCalc, m_ObjectBBoxesXYBrushes, m_Config.brushResMultiplier, cpHeightMap, cMaxMultiplier, eOT_Brush);
#if defined(USE_ENTITIES)
	AddSamplesAroundObjects(rTerrainObjMan, rCalc, m_ObjectBBoxesXYLargeEntities, m_Config.brushResMultiplier, cpHeightMap, cMaxMultiplier, eOT_Entity);
#endif
	AddSamplesAroundObjects(rTerrainObjMan, rCalc, m_ObjectBBoxesXYVoxels, m_Config.brushResMultiplier, cpHeightMap, cMaxMultiplier, eOT_Voxel);

	if (cMaxMultiplier > 2)
	{
		for (int i = 0; i < cRes; ++i)
			for (int j = 0; j < cRes; ++j)
			{
				if (i % cMarkStep != 0 || j % cMarkStep != 0)
				{
					SSHSample& rSample = rCalc.GetSampleAt(i, j);
					rSample.SetToBeIgnored(true);
				}
			}
	}
	const std::vector<TVec2>::const_iterator cEnd = m_AdditionalSamples.end();
	for (std::vector<TVec2>::const_iterator iter = m_AdditionalSamples.begin(); iter != cEnd; ++iter)
	{
		SSHSample& rSample = rCalc.GetSampleAt(iter->x, iter->y);
		rSample.SetToBeIgnored(false);//if it was obstructed before, it will not have been recorded in m_AdditionalSamples
	}
}

void CTerrainGIGen::GenerateAddSampleVec
(
  const CTerrainObjectMan& crTerrainObjMan,
  const CHemisphereSink_SH& crCalcSink
)
{
	bool messaged = false;
	uint32 curCount = m_GridPoints.size();
	const float cAngleThreshold = gf_PI2 * 40.f / 360.f;//if angle exceeds some angle, do not use vis progression
	const uint8 cAngleThresholdQuant = (uint8)(cAngleThreshold * 256.f / (gf_PI / 2));//quantize 0..PI/2 into 256 values
	//generate some directions for lookup, slidely looking upward and 90 degree separated in xy
	static const uint32 scDirectionCnt = 16;
	static const NSH::TSample scDirections[scDirectionCnt] =
	{
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 0.f)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 0.25f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 0.5f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 0.75f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 1.25f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 1.5f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.45f, 1.75f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 0.f)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 0.25f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 0.5f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 0.75f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 1.25f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 1.5f * gf_PI)),
		NSH::TSample(NSH::SDescriptor(), NSH::TPolarCoord(1.05f, 1.75f * gf_PI))
	};
	//check for each element if the following one has the same id, this is used to calculate a vis progression across the object (in z)
	const std::vector<SSHSample>::const_iterator cEnd = crCalcSink.GetAddSampleIterEnd();
	for (std::vector<SSHSample>::const_iterator iter = crCalcSink.GetAddSampleIterBegin(); iter != cEnd; ++iter)
	{
		const SSHSample& crAddSample = *iter;
		if (crAddSample.IsToBeIgnored() || crAddSample.IsObstructed())
			continue;

		//usual sample
		TEditorGridPoint addSample;
		AssignSHData(crAddSample.pSHData->shCoeffs, addSample.shCoeffs);
		//WORLD_HMAP_COORDINATE_EXCHANGE
		addSample.x = crAddSample.posY;
		addSample.y = crAddSample.posX;
		if (curCount == m_GridMaxCount && !messaged)
		{
			messaged = true;
			CLogFile::WriteLine("Maximum number of samples reached, drop remaining");
			continue;
		}

		const NQT::EInsertionResult cRes = m_QuadTree.InsertLeaf(TVec2((TIndexType)addSample.x, (TIndexType)addSample.y), m_GridPoints.size());
		assert(NQT::SUCCESS == cRes || NQT::POS_EQUAL_TO_EXISTING_LEAF == cRes);
		if (NQT::SUCCESS == cRes)
		{
			addSample.SetFlag(scValid | scOptimizeable);
			m_GridPoints.push_back(addSample);
			++curCount;
		}
		else if (NQT::TOO_MANY_LEAVES == cRes)
		{
			messaged = true;
			CLogFile::WriteLine("Maximum number of quadtree leaves reached, drop remaining");
			continue;
		}
	}
}

void CTerrainGIGen::BlurTerrainAccMap(uint8* pAccMap, const uint32 cWidth, const uint8 cMinValue)
{
	//just weight center pixel
	uint8* pTempMap = m_pTempBlurArea;
	memcpy(pTempMap, pAccMap, cWidth * cWidth);

	const float cCenterWeight = 0.15f;         //0.35f;//center weight
	const float cWeight = 1.f - cCenterWeight; //remaining weight
	const float cRemainingWeightSum = 4.f / sqrtf(2.f) + 4 * 1.f;

	const float cFullPixelWeight = cWeight / cRemainingWeightSum;
	const float cCornerDistWeight = cFullPixelWeight / sqrtf(2.f);

	const float cCornerPixelWeight = cWeight / 3.f;
	const float cEdgePixelWeight = cWeight / 5.f;

	for (int x = 0; x < cWidth; ++x)
		for (int y = 0; y < cWidth; ++y)
		{
			const uint8 cCurVal = pTempMap[y * cWidth + x];
			float newCol = cCenterWeight * (float)cCurVal;//weight center pixel by 0.5f
			//center pixels
			if (x > 0 && y > 0 && x < cWidth - 1 && y < cWidth - 1)
			{
				//blur all pixels
				newCol += cCornerDistWeight * (float)pTempMap[(y - 1) * cWidth + x - 1];
				newCol += cCornerDistWeight * (float)pTempMap[(y + 1) * cWidth + x - 1];
				newCol += cCornerDistWeight * (float)pTempMap[(y - 1) * cWidth + x + 1];
				newCol += cCornerDistWeight * (float)pTempMap[(y + 1) * cWidth + x + 1];

				newCol += cFullPixelWeight * (float)pTempMap[(y - 1) * cWidth + x];
				newCol += cFullPixelWeight * (float)pTempMap[(y + 1) * cWidth + x];
				newCol += cFullPixelWeight * (float)pTempMap[y * cWidth + x - 1];
				newCol += cFullPixelWeight * (float)pTempMap[y * cWidth + x + 1];
			}
			else//get the 4 corner pixels
			if (x == 0 && y == 0)
			{
				newCol += cCornerPixelWeight * (float)pTempMap[cWidth];
				newCol += cCornerPixelWeight * (float)pTempMap[1];
				newCol += cCornerPixelWeight * (float)pTempMap[cWidth + 1];
			}
			else if (x == cWidth - 1 && y == 0)
			{
				newCol += cCornerPixelWeight * (float)pTempMap[cWidth + cWidth - 1];
				newCol += cCornerPixelWeight * (float)pTempMap[cWidth - 2];
				newCol += cCornerPixelWeight * (float)pTempMap[cWidth + cWidth - 2];
			}
			else if (x == cWidth - 1 && y == cWidth - 1)
			{
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 1) * cWidth + cWidth - 2];
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 2) * cWidth + cWidth - 2];
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 2) * cWidth + cWidth - 1];
			}
			else if (x == 0 && y == cWidth - 1)
			{
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 1) * cWidth + 1];
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 2) * cWidth];
				newCol += cCornerPixelWeight * (float)pTempMap[(cWidth - 2) * cWidth + 1];
			}
			else if (y == 0)//upper edge
			{
				for (int i = -1; i <= 1; ++i)
					newCol += cEdgePixelWeight * (float)pTempMap[cWidth + x + i];
				newCol += cEdgePixelWeight * (float)pTempMap[x - 1];
				newCol += cEdgePixelWeight * (float)pTempMap[x + 1];
			}
			else if (y == 0)//lower edge
			{
				for (int i = -1; i <= 1; ++i)
					newCol += cEdgePixelWeight * (float)pTempMap[(cWidth - 2) * cWidth + x + i];
				newCol += cEdgePixelWeight * (float)pTempMap[(cWidth - 1) * cWidth + x - 1];
				newCol += cEdgePixelWeight * (float)pTempMap[(cWidth - 1) * cWidth + x + 1];
			}
			else if (x == 0)//left edge
			{
				for (int i = -1; i <= 1; ++i)
					newCol += cEdgePixelWeight * (float)pTempMap[(y + i) * cWidth + 1];
				newCol += cEdgePixelWeight * (float)pTempMap[(y - 1) * cWidth];
				newCol += cEdgePixelWeight * (float)pTempMap[(y + 1) * cWidth];
			}
			else if (x == cWidth - 1)//right edge
			{
				for (int i = -1; i <= 1; ++i)
					newCol += cEdgePixelWeight * (float)pTempMap[(y + i) * cWidth + cWidth - 2];
				newCol += cEdgePixelWeight * (float)pTempMap[(y - 1) * cWidth + cWidth - 1];
				newCol += cEdgePixelWeight * (float)pTempMap[(y + 1) * cWidth + cWidth - 1];
			}
			assert(newCol < 255.5f);
			pAccMap[y * cWidth + x] = max(cMinValue, (uint8)(newCol + 0.5f));
		}
}

const float CTerrainGIGen::GetHeightFromWorldPosFullRes(const Vec3& crWorldPos) const
{
	//use a filtered version
	const float cUnitSize = m_pHeightMap->GetUnitSize();
	const float cInvUnitSize = 1.f / cUnitSize;
	const uint32 cWidth = m_pHeightMap->GetWidth();
	//WORLD_HMAP_COORDINATE_EXCHANGE
	const float cXPosFloat = crWorldPos.y * cInvUnitSize;
	const float cYPosFloat = crWorldPos.x * cInvUnitSize;
	const uint32 cX = (uint32)cXPosFloat;
	const uint32 cY = (uint32)cYPosFloat;
	const uint32 cX1 = std::min(cWidth - 1, cX + 1);
	const uint32 cY1 = std::min(cWidth - 1, cY + 1);

	const float* pHMapData = m_pHeightMap->GetData();
	const float c00Height = pHMapData[cY * cWidth + cX];
	const float c01Height = pHMapData[cY * cWidth + cX1];
	const float c10Height = pHMapData[cY1 * cWidth + cX];
	const float c11Height = pHMapData[cY1 * cWidth + cX1];

	//now apply filter
	const float cXFrac = cXPosFloat - (float)cX;
	const float cYFrac = cYPosFloat - (float)cY;
	return (1.f - cYFrac) * (c00Height * (1.f - cXFrac) + cXFrac * c01Height) +
	       cYFrac * (c10Height * (1.f - cXFrac) + cXFrac * c11Height);
}

const float CTerrainGIGen::GetFilteredHeight
(
  const uint16 cXPos,
  const uint16 cYPos,
  const uint32 cRGBImageRes,
  const uint32 cXYRes,
  const float* const cpHeightMap,
  const uint32 cRGBToXYResShift
) const
{
	const uint32 cX = cXPos >> cRGBToXYResShift;
	const uint32 cY = cYPos >> cRGBToXYResShift;
	const uint32 cX1 = std::min(cRGBImageRes - 1, cX + 1);
	const uint32 cY1 = std::min(cRGBImageRes - 1, cY + 1);

	const float c00Height = cpHeightMap[cY * cRGBImageRes + cX];
	const float c01Height = cpHeightMap[cY * cRGBImageRes + cX1];
	const float c10Height = cpHeightMap[cY1 * cRGBImageRes + cX];
	const float c11Height = cpHeightMap[cY1 * cRGBImageRes + cX1];

	//now apply filter
	const float cPixelFrac = (float)cRGBImageRes / (float)m_TerrainMapRes;
	assert(cPixelFrac <= 1.f);
	const float cXFrac = ((float)cXPos - (float)((cXPos >> cRGBToXYResShift) << cRGBToXYResShift)) * cPixelFrac;
	const float cYFrac = ((float)cYPos - (float)((cYPos >> cRGBToXYResShift) << cRGBToXYResShift)) * cPixelFrac;
	assert(cXFrac >= 0.f && cXFrac <= 1.f);
	assert(cYFrac >= 0.f && cYFrac <= 1.f);
	return (1.f - cYFrac) * (c00Height * (1.f - cXFrac) + cXFrac * c01Height) +
	       cYFrac * (c10Height * (1.f - cXFrac) + cXFrac * c11Height);
}

void CTerrainGIGen::PostProcessSample
(
  const uint8 cScale,
  const TCoeffType& pSHCoeffs,
  uint8* pHeightMapAccMap,
  const uint32 cWidth,
  const uint32 cX,
  const uint32 cY,
  const NSH::TSample& crLookupVec
)
{
	//now perform the upper hemisphere convolution around the heightmap normal and discretize into 8 bit
	//calculate upper average vis
	NSH::SCoeffList_tpl<NSH::SScalarCoeff_tpl<float>> convCoeffs;
	GenConvCoeffs(pSHCoeffs, convCoeffs);
	float upperHSAveVis = std::min(1.f, std::max(0.f, (float)(convCoeffs.PerformLookup(crLookupVec, NSH::ALL_COEFFS))));
	//amplify effect a bit
	//we should not amplify anything below 0.35: 0.35 = power factor 1.0  -  1.0: power factor 2
	const float cPowerFactor = 1.f + ((std::max(0.f, upperHSAveVis - 0.35f) * 1.f / 0.65f) /* * 1.f*/);
	upperHSAveVis = powf(upperHSAveVis, cPowerFactor);
	//lerp according to cScale(0 = 255, 1=cBrightness)
	const float cScaleFactor = (float)cScale * (1.f / 255.f);
	upperHSAveVis = cScaleFactor * upperHSAveVis + (1.f - cScaleFactor);
	const uint32 cBrightness = (uint32)(255.f * upperHSAveVis + 0.5f);
	assert(cBrightness < 256);
	pHeightMapAccMap[cY * cWidth + cX] = (uint8)cBrightness;
}

void CTerrainGIGen::CalcTerrainOcclusion
(
  CPakFile& rLevelPakFile,
  const uint32 cRGBImageRes,
  const uint64 cWidthUnits,
  CTerrainObjectMan& rTerrainMan,
  const float* const cpHeightMap,
  const CHemisphereSink_SH& crCalcSink,
  uint8*& rpTerrainAccMap,
  uint32& rTerrainAccMapRes,
  const uint32 cApplySS,
  uint8* pHeightMapAccMap
)
{
	if (!pHeightMapAccMap)
	{
		rpTerrainAccMap = NULL;
		return;
	}
	int dummy[2];
	//for each pos on the heightmap, check if has some hits to objects and if so
	//initialize the temp map heightmap data to be written
	CLogFile::WriteLine("Calculate Terrain Accessibility...");

	CWaitProgress progress("Calculate Terrain Accessibility...", false);
	progress.Start();

	const uint64 cTerrainOcclRes = cWidthUnits;
	const float cHMapToWorldMulSS = 0.5f /*super sampling*/;

	//sample at unit resolution, super sampling is applied at the double res
	uint32 hmapUnitShift = 0;
	while ((m_TerrainMapRes << hmapUnitShift) != cTerrainOcclRes)
		++hmapUnitShift;

	uint32 imageToTerrainShift = 0;
	while ((cRGBImageRes << imageToTerrainShift) != cTerrainOcclRes)
		++imageToTerrainShift;
	assert((cRGBImageRes << imageToTerrainShift) == cTerrainOcclRes);

	const uint32 cFullRes = cTerrainOcclRes << 1 /*super sampling*/;
	const uint64 cTerrainMapEntries = cTerrainOcclRes * cTerrainOcclRes;
	uint64 curUpdated = 0;
	//sample in double resolution if super sampling is active
	for (uint32 tempX = 0; tempX < cTerrainOcclRes; ++tempX)
	{
		for (uint32 tempY = 0; tempY < cTerrainOcclRes; ++tempY)
		{
			progress.Step((uint32)(100.f * ((float)curUpdated / (float)cTerrainMapEntries)));
			++curUpdated;
			const uint8 cTerrainMarkVal = m_TerrainMapMarks[(tempY >> hmapUnitShift) * m_TerrainMapRes + (tempX >> hmapUnitShift)];
			if (cTerrainMarkVal == 0)
				continue;
			float height = 1.f;
			//generate heightmap normal
			const Vec3 cNormal = ComputeHeightmapNormal
			                     (
			  tempX >> imageToTerrainShift,
			  tempY >> imageToTerrainShift,
			  cpHeightMap,
			  cRGBImageRes
			                     );
			NSH::SCartesianCoord_tpl<float> cartCoord(cNormal.x, cNormal.y, cNormal.z);
			NSH::SPolarCoord_tpl<float> polarCoord;
			NSH::ConvertUnitToPolar(polarCoord, cartCoord);
			const NSH::TSample cLookupVec(NSH::SDescriptor(), polarCoord);
			//gather objects around
			//WORLD_HMAP_COORDINATE_EXCHANGE
			//since special bilinear filtering for terrain occlusion map is performed, negative offset positions
			static const uint32 cXOffsetsSS[4] = { 0, 1, 0, 1 };
			static const uint32 cYOffsetsSS[4] = { 0, 0, 1, 1 };
			//to calculate the offset here is actually not right
			//depends on final max. sector resolution since there is the 0.5 offset applied
			static const float cTexelOffset = -0.5f;
			if (cApplySS)
			{
				//perform super sampling
				for (int ss = 0; ss < 4; ++ss)//super sampling loop
				{
					uint32 x = tempX * 2 + cXOffsetsSS[ss];
					uint32 y = tempY * 2 + cYOffsetsSS[ss];
					//so it potentially has some objects nearby
					//WORLD_HMAP_COORDINATE_EXCHANGE
					//add half pixel offset
					TQuadTree::TInitialInterpolRetrievalType nodes;
					const TVec2F cGridPos((float)x * cHMapToWorldMulSS + cTexelOffset, (float)y * cHMapToWorldMulSS + cTexelOffset);
					const uint32 cLeafCount = m_QuadTree.GetInterpolationNodes(nodes, cGridPos, 32.05f * 32.05f);
					TCoeffType interpolatedCoeffs;
					if (cLeafCount != 0)
					{
						InterpolateGridPoint(dummy, cLeafCount, nodes, interpolatedCoeffs);
						PostProcessSample(cTerrainMarkVal, interpolatedCoeffs, pHeightMapAccMap, cFullRes, x, y, cLookupVec);
					}
				}
			}
			else
			{
				//so it potentially has some objects nearby
				TQuadTree::TInitialInterpolRetrievalType nodes;
				//WORLD_HMAP_COORDINATE_EXCHANGE
				const TVec2F cGridPos((float)tempX + cTexelOffset, (float)tempY + cTexelOffset);
				const uint32 cLeafCount = m_QuadTree.GetInterpolationNodes(nodes, cGridPos, 32.05f * 32.05f);
				TCoeffType interpolatedCoeffs;
				if (cLeafCount != 0)
				{
					InterpolateGridPoint(dummy, cLeafCount, nodes, interpolatedCoeffs);
					PostProcessSample(cTerrainMarkVal, interpolatedCoeffs, pHeightMapAccMap, cTerrainOcclRes, tempX, tempY, cLookupVec);
				}
			}
		}
	}

	//if super sampling was active, blur and down sample now
	if (cApplySS)
	{
		for (int i = 0; i < cTerrainOcclRes; ++i)
			for (int j = 0; j < cTerrainOcclRes; ++j)
			{
				float val = (float)pHeightMapAccMap[2 * j * cFullRes + 2 * i];
				val += (float)pHeightMapAccMap[2 * j * cFullRes + 2 * i + 1];
				val += (float)pHeightMapAccMap[(2 * j + 1) * cFullRes + 2 * i + 1];
				val += (float)pHeightMapAccMap[(2 * j + 1) * cFullRes + 2 * i];
				val *= 0.25f;
				const uint32 cBrightness = (uint32)(val + 0.5f);
				m_pSmallHeightMapAccMap[j * cTerrainOcclRes + i] = (uint8)std::min((uint32)255, cBrightness);
			}
		delete[] pHeightMapAccMap;
		pHeightMapAccMap = m_pSmallHeightMapAccMap;
	}
	//now blur it
	if (!cApplySS)
		BlurTerrainAccMap(pHeightMapAccMap, cTerrainOcclRes);
	BlurTerrainAccMap(pHeightMapAccMap, cTerrainOcclRes, 50);

#if defined(GEN_ACCESS_TEST_TGA)
	uint32* pBitmap = new uint32[cTerrainOcclRes * cTerrainOcclRes];
	assert(pBitmap);
	memset(pBitmap, 0, cTerrainOcclRes * cTerrainOcclRes * sizeof(uint32));
	for (int i = 0; i < cTerrainOcclRes; ++i)
		for (int j = 0; j < cTerrainOcclRes; ++j)
		{
			const uint32 cCol = pHeightMapAccMap[j * cTerrainOcclRes + i];
			pBitmap[j * cTerrainOcclRes + i] = 0xFF000000 | cCol | (cCol << 8) | (cCol << 16);
		}
	NQT::SaveTGA32("e:/downloads/TerrainAccess.tga", pBitmap, cTerrainOcclRes);
	delete[] pBitmap;
#endif //GEN_ACCESS_TEST_TGA

	rpTerrainAccMap = pHeightMapAccMap;
	rTerrainAccMapRes = cTerrainOcclRes;

	progress.Step(100);
	progress.Stop();

	if (m_pTempBlurArea) delete[] m_pTempBlurArea;
}

const Vec3 CTerrainGIGen::ComputeHeightmapNormal
(
  const uint16 cHPosX, const uint16 cHPosY,
  const float* const cpHeightMap,
  const uint32 cHighResWidth
) const
{
	const int32 cX = cHPosX;
	const int32 cXm1 = std::max(0, cX - 1);
	const int32 cXp1 = std::min(cHighResWidth - 1, (uint32)(cX + 1));

	const int32 cY = cHPosY;
	const int32 cYm1 = std::max(0, cY - 1);
	const int32 cYp1 = std::min(cHighResWidth - 1, (uint32)(cY + 1));

	const float cXm1Height = cpHeightMap[cY * cHighResWidth + cXm1];
	const float cXp1Height = cpHeightMap[cY * cHighResWidth + cXp1];
	const float cYm1Height = cpHeightMap[cYm1 * cHighResWidth + cX];
	const float cYp1Height = cpHeightMap[cYp1 * cHighResWidth + cX];

	const Vec3 cDX(2.f * m_UnitSize, 0.f, cXm1Height - cXp1Height);
	const Vec3 cDY(0.f, 2.f * m_UnitSize, cYm1Height - cYp1Height);
	return cDX.Cross(cDY).GetNormalized();
}

void CTerrainGIGen::AdaptObstructedSamplePos
(
  const uint32 cFullWidth,
  const CTerrainObjectMan& crTerrainObjMan,
  const THeightmapAccess& crCalc,
  SSHSample& rSample,
  SSHSample& rSampleLeft,
  SSHSample& rSampleRight,
  SSHSample& rSampleUpper,
  SSHSample& rSampleLower
)
{
	//generate pointers to samples to work in loops, pay attention to work in the same space
	//all in heightmap space
	SSHSample* pNeighbourSamples[4] = { &rSampleLeft, &rSampleRight, &rSampleUpper, &rSampleLower };
	const uint32 cObstrMapRes = crTerrainObjMan.GetObstructionMapRes();
	const uint32 cGridToObstrMapMultiplier = cObstrMapRes / crCalc.m_Sink.GetGridResolution();
	assert(cObstrMapRes > crCalc.m_Sink.GetGridResolution());
	const uint32 cObstrMapToWorldMultiplier = cFullWidth / cObstrMapRes;
	assert(cGridToObstrMapMultiplier * crCalc.m_Sink.GetGridResolution() == cObstrMapRes);
	//first calculate the next non obstructed uint16 position for each grid direction
	//work in heightmap space the samples are in too
	uint16 hmapX, hmapY;
	crCalc.m_Sink.WorldToGridPos(hmapX, hmapY, (float)rSample.posX, (float)rSample.posY);
	hmapX *= cGridToObstrMapMultiplier;
	hmapY *= cGridToObstrMapMultiplier;
	uint32 dummy;
	float obstructionAmount = 1.f;
	const std::pair<uint16, uint16> cDirOffsets[4] = { std::make_pair(-1, 0), std::make_pair(1, 0), std::make_pair(0, -1), std::make_pair(0, 1) };
	std::pair<uint16, uint16> nextNonObstructedPos[4];
	for (int i = 0; i < 4; ++i)
	{
		nextNonObstructedPos[i].first = hmapX + cDirOffsets[i].first;
		nextNonObstructedPos[i].second = hmapY + cDirOffsets[i].second;
		while (CTerrainObjectMan::scObstructedForOffset ==
		       crTerrainObjMan.IsObstructed(
		         nextNonObstructedPos[i].first, nextNonObstructedPos[i].second, cObstrMapRes, dummy, obstructionAmount, false))
		{
			nextNonObstructedPos[i].first += cDirOffsets[i].first;
			nextNonObstructedPos[i].second += cDirOffsets[i].second;
		}
	}
	//determine in which direction it is the shortest to move
	int shortestIndex = 0;
	unsigned int shortestDistance = std::numeric_limits<unsigned int>::max();
	for (int i = 0; i < 4; ++i)
	{
		const unsigned int cDist = abs(nextNonObstructedPos[i].first - hmapX) + abs(nextNonObstructedPos[i].second - hmapY);
		if (cDist < shortestDistance)
		{
			shortestDistance = cDist;
			shortestIndex = i;
		}
	}
	assert(shortestDistance > 0);
	//move sample there if distance is less than the next sample
	if (shortestDistance >= crCalc.m_Sink.GetWorldSampleDist())
	{
		rSample.SetToBeIgnored(true);
		return;
	}
	//set new sample pos
	//WORLD_HMAP_COORDINATE_EXCHANGE
	rSample.posX = nextNonObstructedPos[shortestIndex].second * cObstrMapToWorldMultiplier;
	rSample.posY = nextNonObstructedPos[shortestIndex].first * cObstrMapToWorldMultiplier;
	rSample.SetIsAlreadyDisplaced(true);//mark as translated to not do it twice
	//now move other samples if they have not been translated, offseted or in the direction of the move
	for (int i = 0; i < 4; ++i)
	{
		if (i != shortestIndex &&
		    !pNeighbourSamples[i]->IsObstructed() &&
		    !pNeighbourSamples[i]->IsAlreadyDisplaced() &&
		    !pNeighbourSamples[i]->IsToBeIgnored() &&
		    !pNeighbourSamples[i]->IsOffseted())
		{
			//WORLD_HMAP_COORDINATE_EXCHANGE
			pNeighbourSamples[i]->posX = nextNonObstructedPos[i].second * cObstrMapToWorldMultiplier;
			pNeighbourSamples[i]->posY = nextNonObstructedPos[i].first * cObstrMapToWorldMultiplier;
			pNeighbourSamples[i]->SetIsAlreadyDisplaced(true);
		}
	}
}

void CTerrainGIGen::OnBeforeProcessing(const uint32 cFullWidth, CTerrainObjectMan& rTerrainObjMan, const uint32 cHighResWidth, THeightmapAccess& rCalc)
{
	//determine number of elements we need
	uint32 num = rCalc.m_Sink.GetAddSampleCount();

	//sample at unit resolution, super sampling is applied at the double res
	uint32 hmapUnitShift = 0;
	assert(cHighResWidth <= m_TerrainMapRes);
	while ((cHighResWidth << hmapUnitShift) != m_TerrainMapRes)
		++hmapUnitShift;

	for (int i = 0; i < cHighResWidth; ++i)
		for (int j = 0; j < cHighResWidth; ++j)
		{
			SSHSample& rSample = rCalc.GetSampleAt(i, j);
			//do only ray cast if it is within a certain distance of objects
			const uint8 cTerrainMarkVal = m_TerrainMapMarks[(j << hmapUnitShift) * m_TerrainMapRes + (i << hmapUnitShift)];
			if (cTerrainMarkVal == 0)
				rSample.SetDoRayCasting(false);
			if (!rSample.IsToBeIgnored())
			{
				//check if it is in a unused sector
				//WORLD_HMAP_COORDINATE_EXCHANGE
				if (!IsSectorUsed(rSample.posY, rSample.posX))
				{
					rSample.SetToBeIgnored();
					continue;
				}
				if (rSample.IsObstructed())//if sample is obstructed by a brush, apply special treatment for translating them
				{
					//if all neighbor samples are also marked as to obstructed or ignored, set it to be ignored too
					if (i == cHighResWidth - 1 || j == cHighResWidth - 1 || i == 0 || j == 0)
					{
						rSample.SetToBeIgnored(true);//dont treat border cases for simplicity
						continue;
					}
					//if sample is obstructed by a large building, mark it as fully obstructed and set the respective colours
					uint32 reflCol = 0;
					//get 4 neighbor samples
					SSHSample& rSampleLeft = rCalc.GetSampleAt(i - 1, j);
					SSHSample& rSampleRight = rCalc.GetSampleAt(i + 1, j);
					SSHSample& rSampleUpper = rCalc.GetSampleAt(i, j - 1);
					SSHSample& rSampleLower = rCalc.GetSampleAt(i, j + 1);

					if ((rSampleLeft.IsToBeIgnored() || rSampleLeft.IsObstructed()) &&
					    (rSampleRight.IsToBeIgnored() || rSampleRight.IsObstructed()) &&
					    (rSampleUpper.IsToBeIgnored() || rSampleUpper.IsObstructed()) &&
					    (rSampleLower.IsToBeIgnored() || rSampleLower.IsObstructed()))
					{
						rSample.SetToBeIgnored(true);//all neighbors are obstructed too
						continue;
					}
					AdaptObstructedSamplePos(cFullWidth, rTerrainObjMan, rCalc, rSample, rSampleLeft, rSampleRight, rSampleUpper, rSampleLower);
					if (rSample.IsToBeIgnored())
						continue;
				}
				++num;  //count number of valid samples to allocate
			}
		}
	assert(m_ContiguousSampleData.empty());
	if (num == 0)
		return;

	//allocate in parts of 4 MB
	static const uint32 cSingleAllocationSize = 4096 * 1024;
	const uint32 cSingleCount = std::min((uint32)num, (uint32)(cSingleAllocationSize / sizeof(SSHSampleOnDemand)));
#if defined(_DEBUG)
	SSHSampleOnDemand* pLastPtr = NULL;
#endif
	uint32 parts = 1;
	if (num > cSingleCount)
	{
		parts = num / cSingleCount;
		parts += (parts * cSingleCount == num) ? 0 : 1;
	}
	m_ContiguousSampleData.resize(parts);
	uint32 remainingCount = num;
	for (int i = 0; i < parts; ++i)
	{
		m_ContiguousSampleData[i] = new SSHSampleOnDemand[std::min(cSingleCount, remainingCount)];
#if defined(_DEBUG)
		pLastPtr = &m_ContiguousSampleData[i][std::min(cSingleCount, remainingCount) - 1] + 1;
#endif
		remainingCount -= cSingleCount;
	}
	//initialize
	remainingCount = num;
	for (int i = 0; i < parts; ++i)
	{
		for (int j = 0; j < std::min(cSingleCount, remainingCount); ++j)
		{
			SSHSampleOnDemand& rSampleData = m_ContiguousSampleData[i][j];
			for (int i = 0; i < 9; ++i)
				rSampleData.shCoeffs[i] = 0.f;
		}
		remainingCount -= cSingleCount;
	}
	//assign data
	//first to object data
	SSHSampleOnDemand* pCurSHPtr = &m_ContiguousSampleData[0][0];
	remainingCount = cSingleCount;
	uint32 curIndex = 0;
	const std::vector<SSHSample>::const_iterator cEnd = rCalc.m_Sink.GetAddSampleIterEnd();
	for (std::vector<SSHSample>::iterator iter = rCalc.m_Sink.GetAddSampleIterBegin(); iter != cEnd; ++iter)
	{
		SSHSample& rObjectSample = *iter;
		assert(rObjectSample.pSHData == NULL);
		rObjectSample.pSHData = pCurSHPtr++;
		--remainingCount;
		if (remainingCount == 0)
		{
			pCurSHPtr = &m_ContiguousSampleData[++curIndex][0];
			remainingCount = cSingleCount;
		}
	}
	//now assign to grid samples
	for (int i = 0; i < cHighResWidth; ++i)
		for (int j = 0; j < cHighResWidth; ++j)
		{
			SSHSample& rSample = rCalc.GetSampleAt(i, j);
			if (!rSample.IsToBeIgnored())
			{
				assert(rSample.pSHData == NULL);
				rSample.pSHData = pCurSHPtr++;
				--remainingCount;
				if (remainingCount == 0 && curIndex < parts - 1)
				{
					pCurSHPtr = &m_ContiguousSampleData[++curIndex][0];
					remainingCount = cSingleCount;
				}
			}
		}
#if defined(_DEBUG)
	assert(pCurSHPtr == pLastPtr);
#endif
}

const bool CTerrainGIGen::Generate
(
  CPakFile& rLevelPakFile,
  uint8*& rpTerrainAccMap,
  uint32& rTerrainAccMapRes,
  const uint32 cApplySS,
  const SConfigProp& crConfigProps
)
{
	CLogFile::WriteLine("Prepare Indirect Lighting...");

	m_pHeightMap = GetIEditorImpl()->GetHeightmap();
	if (!m_pHeightMap)
	{
		CLogFile::WriteLine("Failed to get Heightmap...aborting inderect lightning calculation...");
		return false;
	}

	m_Config = crConfigProps;

	uint64 cWidth = m_pHeightMap->GetWidth();
	m_UnitSize = m_pHeightMap->GetUnitSize();
	uint64 cWidthUnits = m_UnitSize * cWidth;
	m_TerrainMapRes = cWidth;

	uint32 cObjectMul = std::max(m_Config.vegResMultiplier, m_Config.brushResMultiplier);
	uint32 cHeightmapHighResWidth = cWidthUnits / m_Config.baseGranularity * cObjectMul;

	if (cHeightmapHighResWidth > m_TerrainMapRes)
	{
		CLogFile::WriteLine("[WARNING] Increasing indirect lighting resolution: settings under the minimum required.");
		while (cHeightmapHighResWidth > m_TerrainMapRes)
		{
			m_Config.baseGranularity <<= 1;
			cHeightmapHighResWidth = cWidthUnits / m_Config.baseGranularity * cObjectMul;
		}
	}

	uint32 cGridWidth = cWidthUnits / m_Config.baseGranularity;//width of targeted resolution (pixel size for sh accessibility)

	uint32 cHighResWidth = cGridWidth * cObjectMul;

	//init multiprocessing for rasterization
	m_ActiveChunk = 0;
	m_RasterMPMan.InitMP();

	//allocate additional samples
	m_AdditionalSamples.reserve(4 * 1024 * 1024 / sizeof(TVec2));//avoid late allocations (4 MB)

	if (m_FirstTime)
		m_QuadTree.Reset();
	assert(m_Config.brushResMultiplier >= 1 && m_Config.vegResMultiplier >= 1);
	assert(m_Config.wedges >= 16);
	assert(m_Config.brushResMultiplier <= m_Config.baseGranularity);
	assert(m_Config.vegResMultiplier <= m_Config.baseGranularity);
	//determine max number of subdivision levels

	uint32 maxSubDivLevel = 2;
	uint32 size = 1;
	while ((size <<= 1) < cWidthUnits)
		++maxSubDivLevel;
	m_QuadTree.Init(cWidthUnits, maxSubDivLevel, TVec2(cWidthUnits >> 1, cWidthUnits >> 1));//quadtree has world space dimensions
	m_FirstTime = false;

	//RGB image from RGBLayer
	CLogFile::WriteLine("Prepare Image-layers for Indirect Lighting...");

	//prepare heightmap for sky accessibility
	float heightScale;
	CFloatImage floatImage;
	if (!PrepareHeightmap(cHighResWidth, floatImage, heightScale))
		return false;

	float* pHeightMap = floatImage.GetData();
	if (!pHeightMap)
		return false;

	const float cWaterLevel = m_pHeightMap->GetWaterLevel();//cache water level

	//create bitset for terrain indirect lighting samples
	m_TerrainMapRes = cWidth;
	m_TerrainMapMarks.resize(m_TerrainMapRes * m_TerrainMapRes);
	for (int i = 0; i < m_TerrainMapRes * m_TerrainMapRes; ++i)
		m_TerrainMapMarks[i] = 0;//initially set to no scale
	static const unsigned int scTerrainMapUpdateRadius = 12;//update radius around objects radius for ray casting updates

	//create terrain object manager
	//object manager for the ray casting objects (basically a simple bbox manager)
	//works in world space
	CTerrainObjectMan terrainObjMan(cWidthUnits);
	//init sector table
	m_SectorRowNum = (cWidthUnits >> CObstructionAccessManager::scSectorSizeShift);
	m_SectorTable.resize(m_SectorRowNum * m_SectorRowNum);
	memset(&m_SectorTable[0], scSectorUnused, m_SectorRowNum * m_SectorRowNum);

	CLogFile::WriteLine("Prepare objects for Indirect Lighting...");

	PrepareObjects(cWaterLevel, terrainObjMan, pHeightMap, cWidthUnits, cHighResWidth);
	if (!terrainObjMan.ConvertAndGenObstructionMap
	    (
	      cWidthUnits,
	      m_TerrainMapMarks,
	      m_TerrainMapRes,
	      scTerrainMapUpdateRadius
	    )) //generate in full resolution
		return false;

	BuildSectorTable(cWidthUnits, terrainObjMan);//build sector table and use information from terrainObjMan

	//allocate buffer for terrain occlusion now so that we dont crash too late on allocations
	uint8* pHeightMapAccMap = NULL;
	if (cApplySS)
	{
		pHeightMapAccMap = new uint8[(cWidthUnits * cWidthUnits) << 2 /*super sampling*/];
		m_pSmallHeightMapAccMap = new uint8[cWidthUnits * cWidthUnits];//down sampled pointer
		memset(pHeightMapAccMap, 255, (cWidthUnits * cWidthUnits) << 2 /*super sampling*/);//set to full vis by default
	}
	else
	{
		m_pSmallHeightMapAccMap = NULL;
		pHeightMapAccMap = new uint8[cWidthUnits * cWidthUnits];
		memset(pHeightMapAccMap, 255, cWidthUnits * cWidthUnits);//set to full vis by default
	}
	m_pTempBlurArea = new uint8[cWidthUnits * cWidthUnits];//temporary blur area

	CLogFile::WriteLine("Initializing Indirect Lighting...");
	//insert grid points, TLeafContent is index into vector containing grid points
	//reserve space or one grid point every (m_Config.baseGranularity x m_Config.baseGranularity) area
	AddCoarseGridPoints(cWaterLevel, cGridWidth, cHighResWidth, pHeightMap, terrainObjMan);
	{
		//set up heightmap accessibility and start calculation, sample type is a spherical harmonics
		THeightmapAccess calc(cHighResWidth, cHighResWidth, m_Config.wedges);
		{
			calc.m_Sink.Init(&terrainObjMan, pHeightMap, cWaterLevel, heightScale, cWidthUnits / cHighResWidth /*world space multiplier*/);
			calc.InitSamples();
			MarkSamples(terrainObjMan, cHighResWidth, calc, cObjectMul, pHeightMap, cWidthUnits);

			OnBeforeProcessing(cWidthUnits, terrainObjMan, cHighResWidth, calc);//allocate data for sh coeffs for the samples we really need

			const uint32 cThreadCount = calc.m_Sink.GetThreadCount();
			string headLine;
			if (cThreadCount > 1)
			{
				headLine = ("Calculate Indirect Lighting (");
				char buf[3];
				_itoa(cThreadCount, buf, 10);
				headLine += buf;
				headLine += " Threads)...";
			}
			else
				headLine = ("Calculate Indirect Lighting (1 Thread)...");

			CLogFile::WriteLine(headLine.c_str());

			calc.Calc(pHeightMap, heightScale, headLine.c_str());

			CLogFile::WriteLine("Post Process Indirect Lighting...");

			//free some stuff from calc sink
			calc.m_Sink.SignalCalcEnd();

			//free some memory from terrain object manager
			terrainObjMan.FreeObstructionAmount();
		}
		//now get sample data, shcoeffs are already rotated into world space
		SetCoarseGridValues(cWidthUnits, cHighResWidth, calc);
		GenerateAddSampleVec(terrainObjMan, calc.m_Sink);

		//save memory, dont needed it anymore
		m_ObjectBBoxesXYVegetation.clear();
		m_ObjectBBoxesXYBrushes.clear();
		m_ObjectBBoxesXYLargeEntities.clear();
		m_ObjectBBoxesXYVoxels.clear();

		terrainObjMan.FreeObstructionIndices();
		//now add samples around brushes and vegetation
		SetDataForAdditionalSamples(calc);

		CalcTerrainOcclusion
		(
		  rLevelPakFile,
		  cHighResWidth,
		  cWidthUnits,
		  terrainObjMan,
		  pHeightMap,
		  calc.m_Sink,
		  rpTerrainAccMap,
		  rTerrainAccMapRes,
		  cApplySS,
		  pHeightMapAccMap
		);

		DeAllocateContiguousSampleData();

		calc.m_Sink.FreeWedgeSampleVec();
	}

	const std::vector<CBBoxRaster*>::const_iterator cRasterEnd = m_BBoxRasterPtrs.end();
	for (std::vector<CBBoxRaster*>::const_iterator iter = m_BBoxRasterPtrs.begin(); iter != cRasterEnd; ++iter)
		delete *iter;

	//init multiprocessing for rasterization
	m_RasterMPMan.CleanupMP();

	return true;
}

