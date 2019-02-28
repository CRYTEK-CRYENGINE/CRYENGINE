// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CGFLoader.cpp
//  Version:     v1.00
//  Created:     6/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CGFLoader.h"
#include <CryCore/TypeInfo_impl.h>
#include <CryCore/Common_TypeInfo2.h>
#include <CryString/StringUtils.h>
#include <CrySystem/InplaceFactory.h>
#include <CryString/CryPath.h>
#include "ReadOnlyChunkFile.h"
#include <CryCore/CryCustomTypes.h>

#include <Cry3DEngine/IStatObj.h>

// #define DEBUG_DUMP_RBATCHES
#define VERTEX_SCALE 0.01f
enum { MAX_NUMBER_OF_BONES = 65534 };

#if !defined(FUNCTION_PROFILER_3DENGINE)
	#define FUNCTION_PROFILER_3DENGINE
#endif

namespace
{
// Templated construct helper function using an inplace factory
//
// Called from the templated New<T, Expr> function below. Returns a typed
// pointer to the inplace constructed object.
template<typename T, typename InPlaceFactory>
T* Construct(
  const InPlaceFactory& factory,
  void* (*pAllocFnc)(size_t))
{
	return reinterpret_cast<T*>(factory.template apply<T>(pAllocFnc(sizeof(T))));
}

// Templated construct helper function using an inplace factory
//
// Called from the templated New<T, Expr> function below. Returns a typed
// pointer to the inplace constructed object.
template<typename T>
T* Construct(void* (*pAllocFnc)(size_t))
{
	return new(pAllocFnc(sizeof(T)))T;
}

// Templated destruct helper function
//
// Calls the object's destructor and returns a void pointer to the storage
template<typename T>
void Destruct(T* obj, void (* pDestructFnc)(void*))
{
	obj->~T();
	pDestructFnc(reinterpret_cast<void*>(obj));
}
}

//////////////////////////////////////////////////////////////////////////
CLoaderCGF::CLoaderCGF(
  AllocFncPtr pAllocFnc,
  DestructFncPtr pDestructFnc,
  bool bAllowStreamSharing)
	: m_pAllocFnc(pAllocFnc)
	, m_pDestructFnc(pDestructFnc)
{
	m_pChunkFile = NULL;

	//////////////////////////////////////////////////////////////////////////
	// To find really used materials
	memset(MatIdToSubset, 0, sizeof(MatIdToSubset));
	nMaterialsUsed = 0;
	//////////////////////////////////////////////////////////////////////////

	m_bUseReadOnlyMesh = false;

	m_bAllowStreamSharing = bAllowStreamSharing;

	m_pCompiledCGF = 0;

	m_maxWeightsPerVertex = 4;
}

//////////////////////////////////////////////////////////////////////////
CLoaderCGF::~CLoaderCGF()
{
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CLoaderCGF::LoadCGF(const char* filename, IChunkFile& chunkFile, ILoaderCGFListener* pListener, unsigned long nLoadingFlags)
{
	CContentCGF* const pContentCGF = new CContentCGF(filename);
	if (!pContentCGF)
	{
		return NULL;
	}
	if (!LoadCGF(pContentCGF, filename, chunkFile, pListener, nLoadingFlags))
	{
		delete pContentCGF;
		return NULL;
	}
	return pContentCGF;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadCGF(CContentCGF* pContentCGF, const char* filename, IChunkFile& chunkFile, ILoaderCGFListener* pListener, unsigned long nLoadingFlags)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!chunkFile.IsLoaded())
	{
		if (!chunkFile.Read(filename))
		{
			m_LastError = chunkFile.GetLastError();
			return false;
		}
	}

	if (gEnv)
	{
		//Update loading screen and important tick functions
		SYNCHRONOUS_LOADING_TICK();
	}

	return LoadCGF_Int(pContentCGF, filename, chunkFile, pListener, nLoadingFlags);
}

bool CLoaderCGF::LoadCGFFromMem(CContentCGF* pContentCGF, const void* pData, size_t nDataLen, IChunkFile& chunkFile, ILoaderCGFListener* pListener, unsigned long nLoadingFlags)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!chunkFile.IsLoaded())
	{
		if (!chunkFile.ReadFromMemory(pData, (int)nDataLen))
		{
			m_LastError = chunkFile.GetLastError();
			return false;
		}
	}

	if (gEnv)
	{
		//Update loading screen and important tick functions
		SYNCHRONOUS_LOADING_TICK();
	}

	return LoadCGF_Int(pContentCGF, pContentCGF->GetFilename(), chunkFile, pListener, nLoadingFlags);
}

bool CLoaderCGF::LoadCGF_Int(CContentCGF* pContentCGF, const char* filename, IChunkFile& chunkFile, ILoaderCGFListener* pListener, unsigned long nLoadingFlags)
{
	m_pListener = pListener;
	m_bUseReadOnlyMesh = chunkFile.IsReadOnly();

	cry_strcpy(m_filename, filename);
	m_pChunkFile = &chunkFile;

	// Load mesh from chunk file.

	if (!pContentCGF)
	{
		m_LastError.Format("no valid CContentCGF instance for cgf file: %s", m_filename);
		return false;
	}
	m_pCGF = pContentCGF;

	{
		const char* const pExt = PathUtil::GetExt(filename);
		m_IsCHR =
		  !stricmp(pExt, "chr") ||
		  !stricmp(pExt, "chrp") ||
		  !stricmp(pExt, "chrm") ||
		  !stricmp(pExt, "skin") ||
		  !stricmp(pExt, "skinp") ||
		  !stricmp(pExt, "skinm") ||
		  !stricmp(pExt, "skel");
	}

	const bool bJustGeometry = (nLoadingFlags& IStatObj::ELoadingFlagsJustGeometry) != 0;

	if (!LoadChunks(bJustGeometry))
	{
		return false;
	}

	for (int i = 0; i < m_pChunkFile->NumChunks(); ++i)
	{
		if (m_pChunkFile->GetChunk(i)->bSwapEndian)
		{
			m_pCGF->m_bConsoleFormat = true;
			break;
		}
	}

	if (!bJustGeometry)
	{
		if (m_pCGF->GetMaterialCount() > 0)
		{
			m_pCGF->SetCommonMaterial(m_pCGF->GetMaterial(0));
		}
	}

	// Set node parents.
	for (int i = 0; i < m_pCGF->GetNodeCount(); ++i)
	{
		if (m_pCGF->GetNode(i)->nParentChunkId > 0)
		{
			for (int j = 0; j < m_pCGF->GetNodeCount(); ++j)
			{
				if (m_pCGF->GetNode(i)->nParentChunkId == m_pCGF->GetNode(j)->nChunkId)
				{
					m_pCGF->GetNode(i)->pParent = m_pCGF->GetNode(j);
					break;
				}
			}
		}
	}

	// Calculate node *world* matrices.
	for (int i = 0; i < m_pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = m_pCGF->GetNode(i);
		Matrix34 tm = pNode->localTM;
		for (CNodeCGF* pCurNode = pNode->pParent; pCurNode; pCurNode = pCurNode->pParent)
		{
			tm = pCurNode->localTM * tm;
		}
		pNode->worldTM = tm;
		pNode->bIdentityMatrix = pNode->worldTM.IsIdentity();
	}

	// Setup mesh subsets.
	for (int i = 0; i < m_pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = m_pCGF->GetNode(i);
		if (pNode->pMesh)
		{
			SetupMeshSubsets(*pNode->pMesh, pNode->pMaterial);
		}
	}

	if (gEnv)
	{
		SYNCHRONOUS_LOADING_TICK();
	}

	if (!bJustGeometry)
	{
		if (!ProcessSkinning())
		{
			return false;
		}
	}

	if (pListener && pListener->IsValidationEnabled())
	{
		const char* pErrorDescription = 0;
		if (!m_pCGF->ValidateMeshes(&pErrorDescription))
		{
			Warning(
			  "!Invalid meshes (%s) found in file: %s\n"
			  "The file is corrupt (possibly generated with old/buggy RC) -- please re-export it with newest RC",
			  pErrorDescription, m_filename);
		}
	}

	m_pListener = 0;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadChunks(bool bJustGeometry)
{
	FUNCTION_PROFILER_3DENGINE;

	m_CompiledBones = false;
	m_CompiledMesh = 0;
	m_CompiledBonesBoxes = false;

	m_numBonenameList = 0;
	m_numBoneInitialPos = 0;
	m_numMorphTargets = 0;
	m_numBoneHierarchy = 0;

	// Load Nodes.
	const uint32 numChunk = m_pChunkFile->NumChunks();
	m_pCGF->GetSkinningInfo()->m_arrPhyBoneMeshes.clear();
	m_pCGF->GetSkinningInfo()->m_numChunks = numChunk;

	for (uint32 i = 0; i < numChunk; i++)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = m_pChunkFile->GetChunk(i);

		if (!bJustGeometry)
		{
			if (m_IsCHR)
			{
				switch (pChunkDesc->chunkType)
				{

				case ChunkType_BoneNameList:
					m_numBonenameList++;
					if (!ReadBoneNameList(pChunkDesc))
						return false;
					break;

				case ChunkType_BoneInitialPos:
					m_numBoneInitialPos++;
					if (!ReadBoneInitialPos(pChunkDesc))
						return false;
					break;

				case ChunkType_BoneAnim:
					m_numBoneHierarchy++;
					if (!ReadBoneHierarchy(pChunkDesc))
						return false;
					break;

				case ChunkType_BoneMesh:
					if (!ReadBoneMesh(pChunkDesc))
						return false;
					break;

				case ChunkType_MeshMorphTarget:
					m_numMorphTargets++;
					if (!ReadMorphTargets(pChunkDesc))
						return false;
					break;

				//---------------------------------------------------------
				//---       chunks for compiled characters             ----
				//---------------------------------------------------------

				case ChunkType_CompiledBones:
					if (!ReadCompiledBones(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledPhysicalBones:
					if (!ReadCompiledPhysicalBones(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledPhysicalProxies:
					if (!ReadCompiledPhysicalProxies(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledMorphTargets:
					if (!ReadCompiledMorphTargets(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledIntFaces:
					if (!ReadCompiledIntFaces(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledIntSkinVertices:
					if (!ReadCompiledIntSkinVertice(pChunkDesc))
						return false;
					break;

				case ChunkType_CompiledExt2IntMap:
					if (!ReadCompiledExt2IntMap(pChunkDesc))
						return false;
					break;

				case ChunkType_BonesBoxes:
					if (!ReadCompiledBonesBoxes(pChunkDesc))
						return false;
					break;

				}
			}

			switch (pChunkDesc->chunkType)
			{
			//---------------------------------------------------------
			//---       chunks for CGA-objects  -----------------------
			//---------------------------------------------------------

			case ChunkType_ExportFlags:
				if (!LoadExportFlagsChunk(pChunkDesc))
					return false;
				break;

			case ChunkType_Node:
				if (!LoadNodeChunk(pChunkDesc, false))
					return false;
				break;

			case ChunkType_MtlName:
				if (!LoadMaterialFromChunk(pChunkDesc->chunkId))
					return false;
				break;

			case ChunkType_BreakablePhysics:
				if (!ReadCompiledBreakablePhysics(pChunkDesc))
					return false;
				break;

			case ChunkType_FoliageInfo:
				if (!LoadFoliageInfoChunk(pChunkDesc))
					return false;
				break;

			case ChunkType_VCloth:
				if (!LoadVClothChunk(pChunkDesc))
					return false;
				break;
			}
		}
		else
		{
			switch (pChunkDesc->chunkType)
			{
			case ChunkType_Node:
				if (!LoadNodeChunk(pChunkDesc, true))
					return false;
				break;
			}
		}
	}

	return true;
}

bool CLoaderCGF::ReadBoneInitialPos(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkVersion != BONEINITIALPOS_CHUNK_DESC_0001::VERSION)
	{
		m_LastError.Format("BoneInitialPos chunk is of unknown version %d", pChunkDesc->chunkVersion);
		return false;
	}

	BONEINITIALPOS_CHUNK_DESC_0001* const pBIPChunk = (BONEINITIALPOS_CHUNK_DESC_0001*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pBIPChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	const uint32 numBones = pBIPChunk->numBones;

	SBoneInitPosMatrix* const pDefMatrix = (SBoneInitPosMatrix*)(pBIPChunk + 1);
	SwapEndian(pDefMatrix, numBones, bSwapEndianness);

	m_arrInitPose34.resize(numBones, IDENTITY);
	for (uint32 nBone = 0; nBone < numBones; ++nBone)
	{
		m_arrInitPose34[nBone].m00 = pDefMatrix[nBone].mx[0][0];
		m_arrInitPose34[nBone].m01 = pDefMatrix[nBone].mx[1][0];
		m_arrInitPose34[nBone].m02 = pDefMatrix[nBone].mx[2][0];
		m_arrInitPose34[nBone].m03 = pDefMatrix[nBone].mx[3][0] * VERTEX_SCALE;
		m_arrInitPose34[nBone].m10 = pDefMatrix[nBone].mx[0][1];
		m_arrInitPose34[nBone].m11 = pDefMatrix[nBone].mx[1][1];
		m_arrInitPose34[nBone].m12 = pDefMatrix[nBone].mx[2][1];
		m_arrInitPose34[nBone].m13 = pDefMatrix[nBone].mx[3][1] * VERTEX_SCALE;
		m_arrInitPose34[nBone].m20 = pDefMatrix[nBone].mx[0][2];
		m_arrInitPose34[nBone].m21 = pDefMatrix[nBone].mx[1][2];
		m_arrInitPose34[nBone].m22 = pDefMatrix[nBone].mx[2][2];
		m_arrInitPose34[nBone].m23 = pDefMatrix[nBone].mx[3][2] * VERTEX_SCALE;
		m_arrInitPose34[nBone].OrthonormalizeFast(); // for some reason Max supplies unnormalized matrices.
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CLoaderCGF::ReadMorphTargets(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

#if !defined(RESOURCE_COMPILER)
	m_LastError.Format("%s tried to load a noncompiled MeshMorphTarget chunk %i", __FUNCTION__, (int)pChunkDesc->chunkId);
	return false;
#else
	if (pChunkDesc->chunkVersion != MESHMORPHTARGET_CHUNK_DESC_0001::VERSION)
	{
		m_LastError.Format("MeshMorphTarget chunk %i is of unknown version %i", (int)pChunkDesc->chunkId, (int)pChunkDesc->chunkVersion);
		return false;
	}

	// the chunk must at least contain its header and the name (min 2 bytes)
	if (pChunkDesc->size <= sizeof(MESHMORPHTARGET_CHUNK_DESC_0001))
	{
		m_LastError.Format("MeshMorphTarget chunk %i: Bad size: %i", (int)pChunkDesc->chunkId, (int)pChunkDesc->size);
		return false;
	}

	if (m_vertexOldToNew.empty())
	{
		m_LastError.Format("MeshMorphTarget chunk %i: main mesh was not loaded yet or its type is not Skin.", (int)pChunkDesc->chunkId);
		return false;
	}

	MESHMORPHTARGET_CHUNK_DESC_0001* const pMeshMorphTarget = (MESHMORPHTARGET_CHUNK_DESC_0001*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pMeshMorphTarget, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	const uint32 oldVertexCount = pMeshMorphTarget->numMorphVertices;
	if (oldVertexCount <= 0)
	{
		m_LastError.Format("MeshMorphTarget chunk %i: Bad # of vertices: %i", (int)pChunkDesc->chunkId, (int)oldVertexCount);
		return false;
	}

	SMeshMorphTargetVertex* const pSrcVertices = (SMeshMorphTargetVertex*)(pMeshMorphTarget + 1);
	SwapEndian(pSrcVertices, (size_t)oldVertexCount, bSwapEndianness);

	// Remap stored vertex indices to match main mesh's remapping. We also delete entries that
	// reference vertices which do not exist (were removed) in main mesh.

	uint32 newVertexCount = 0;

	for (uint32 i = 0; i < oldVertexCount; ++i)
	{
		const unsigned oldIdx = pSrcVertices[i].nVertexId;
		if (oldIdx >= m_vertexOldToNew.size())
		{
			m_LastError.Format("MeshMorphTarget chunk %i: bad vertex index (%u) at element %u", (int)pChunkDesc->chunkId, oldIdx, (unsigned)i);
			return false;
		}

		const int newIdx = m_vertexOldToNew[oldIdx];
		// Negative index means that the vertex was not used and so it was removed from the main mesh
		if (newIdx >= 0)
		{
			pSrcVertices[i].nVertexId = (unsigned)newIdx;
			pSrcVertices[newVertexCount++] = pSrcVertices[i];
		}
	}

	// Get rid of duplicate entries (referencing same main mesh vertex), and also sort entries
	// in ascending order of vertex indices
	{
		struct CompareByVertexIndex
		{
			static bool less(const SMeshMorphTargetVertex& left, const SMeshMorphTargetVertex& right)
			{
				return left.nVertexId < right.nVertexId;
			}
		};

		std::sort(&pSrcVertices[0], &pSrcVertices[newVertexCount], CompareByVertexIndex::less);

		const uint32 n = newVertexCount;
		newVertexCount = 0;
		for (uint32 i = 0; i < n; ++i)
		{
			if (i == 0 || CompareByVertexIndex::less(pSrcVertices[i - 1], pSrcVertices[i]))
			{
				pSrcVertices[newVertexCount++] = pSrcVertices[i];
			}
		}
	}

	// Form results

	MorphTargets* const pMT = new MorphTargets;

	// Store the mesh ID
	pMT->MeshID = pMeshMorphTarget->nChunkIdMesh;
	// Store the name of morph-target
	const char* const pName = (const char*)(pSrcVertices + oldVertexCount);
	pMT->m_strName = "#" + string(pName);
	// Store the vertices of morph-target
	pMT->m_arrIntMorph.resize(newVertexCount);
	memcpy(pMT->m_arrIntMorph.data(), pSrcVertices, sizeof(*pMT->m_arrIntMorph.data()) * newVertexCount);

	CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
	pSkinningInfo->m_arrMorphTargets.push_back(pMT);

	return true;
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadBoneNameList(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkVersion != BONENAMELIST_CHUNK_DESC_0745::VERSION)
	{
		m_LastError.Format("Unknown version of bone name list chunk");
		return false;
	}

	// the chunk contains variable-length packed strings following tightly each other
	BONENAMELIST_CHUNK_DESC_0745* pNameChunk = (BONENAMELIST_CHUNK_DESC_0745*)(pChunkDesc->data);
	SwapEndian(*pNameChunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	const uint32 nGeomBones = pNameChunk->numEntities;

	// we know how many strings there are there; note that the whole bunch
	// of strings may be followed by padding zeros
	m_arrBoneNameTable.resize(nGeomBones, "");

	// scan through all the strings (strings are zero-terminated)
	const char* const pNameListEnd = ((const char*)pNameChunk) + pChunkDesc->size;
	const char* pName = (const char*)(pNameChunk + 1);
	uint32 numNames = 0;
	while (*pName && pName < pNameListEnd && numNames < nGeomBones)
	{
		m_arrBoneNameTable[numNames] = pName;
		pName += m_arrBoneNameTable[numNames].size() + 1;
		numNames++;
	}
	if (numNames < nGeomBones)
	{
		m_LastError.Format("inconsistent bone name list chunk: only %d out of %d bone names have been read.", numNames, nGeomBones);
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// loads the root bone (and the hierarchy) and returns true if loaded successfully
// this gets called only for LOD 0
/////////////////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadBoneHierarchy(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (pChunkDesc->chunkVersion != BONEANIM_CHUNK_DESC_0290::VERSION)
	{
		m_LastError.Format("Unknown version of bone hierarchy chunk");
		return false;
	}

	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	BONEANIM_CHUNK_DESC_0290* pChunk = (BONEANIM_CHUNK_DESC_0290*)(pChunkDesc->data);
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	m_pBoneAnimRawData = 0;

	if (pChunk->nBones <= 0)
	{
		m_LastError = "There must be at least one bone.";
		return false;
	}

	if (pChunkDesc->size < sizeof(*pChunk) ||
	    pChunk->nBones != (pChunkDesc->size - sizeof(*pChunk)) / sizeof(BONE_ENTITY))
	{
		m_LastError = "Corrupted bone hierarchy chunk data.";
	}

	m_pBoneAnimRawData = pChunk + 1;
	m_pBoneAnimRawDataEnd = ((const char*)pChunk) + pChunkDesc->size;

	BONE_ENTITY* const pBones = (BONE_ENTITY*)m_pBoneAnimRawData;

	for (int i = 0; i < pChunk->nBones; ++i)
	{
		SwapEndian(pBones[i], bSwapEndianness);
	}

	if (pBones[0].ParentID != -1)
	{
		m_LastError = "The first bone in the hierarchy has a parent, but the first none expected to be the root bone.";
		return false;
	}

	/*
	   for (int i = 1; i < pChunk->nBones; ++i)
	   {
	   if (pBones[i].ParentID == -1)
	   {
	   m_LastError = "The skeleton has multiple roots. Only single-rooted skeletons are supported in this version.");
	   return false;
	   }
	   }
	 */

	pSkinningInfo->m_arrBonesDesc.resize(pChunk->nBones);
	CryBoneDescData zeroBone;
	memset(&zeroBone, 0, sizeof(zeroBone));
	std::fill(pSkinningInfo->m_arrBonesDesc.begin(), pSkinningInfo->m_arrBonesDesc.end(), zeroBone);
	m_arrIndexToId.resize(pChunk->nBones, ~0);
	m_arrIdToIndex.resize(pChunk->nBones, ~0);

	m_nNextBone = 1;
	assert(m_nNextBone <= pChunk->nBones);

	m_numBones = 0;

	for (int i = 0; i < pChunk->nBones; ++i)
	{
		if (pBones[i].ParentID == -1)
		{
			const int nRootBoneIndex = i;
			m_nNextBone = nRootBoneIndex + 1;
			RecursiveBoneLoader(nRootBoneIndex, nRootBoneIndex);
		}
	}
	assert(pChunk->nBones == m_numBones);

	//-----------------------------------------------------------------------------
	//---                  read physical information                            ---
	//-----------------------------------------------------------------------------
	pSkinningInfo->m_arrBoneEntities.resize(pChunk->nBones);

	int32 test = 0;
	for (int i = 0; i < pChunk->nBones; ++i)
	{
		pSkinningInfo->m_arrBoneEntities[i] = pBones[i];
		test |= pBones[i].phys.nPhysGeom;
	}
	return true;
}

// loads the whole hierarchy of bones, using the state machine
// when this function is called, the bone is already allocated
uint32 CLoaderCGF::RecursiveBoneLoader(int nBoneParentIndex, int nBoneIndex)
{
	m_numBones++;
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	BONE_ENTITY* pEntity = (BONE_ENTITY*)m_pBoneAnimRawData;
	SwapEndian(*pEntity);
	m_pBoneAnimRawData = ((uint8*)m_pBoneAnimRawData) + sizeof(BONE_ENTITY);

	// initialize the next bone
	CryBoneDescData& rBoneDesc = pSkinningInfo->m_arrBonesDesc[nBoneIndex];
	//read  bone info

	CopyPhysInfo(rBoneDesc.m_PhysInfo[0], pEntity->phys);
	int nFlags = 0;
	if (pEntity->prop[0])
	{
		nFlags = joint_no_gravity | joint_isolated_accelerations;
	}
	else
	{
		if (!CryStringUtils::strnstr(pEntity->prop, "gravity", sizeof(pEntity->prop)))
			nFlags |= joint_no_gravity;
		if (!CryStringUtils::strnstr(pEntity->prop, "active_phys", sizeof(pEntity->prop)))
			nFlags |= joint_isolated_accelerations;
	}
	(rBoneDesc.m_PhysInfo[0].flags &= ~(joint_no_gravity | joint_isolated_accelerations)) |= nFlags;

	//get bone info
	rBoneDesc.m_nControllerID = pEntity->ControllerID;

	// set the mapping entries
	m_arrIndexToId[nBoneIndex] = pEntity->BoneID;
	m_arrIdToIndex[pEntity->BoneID] = nBoneIndex;

	rBoneDesc.m_nOffsetParent = nBoneParentIndex - nBoneIndex;

	// load children
	if (pEntity->nChildren)
	{
		int nChildrenIndexBase = m_nNextBone;
		m_nNextBone += pEntity->nChildren;
		if (nChildrenIndexBase < 0)
			return 0;
		// load the children
		rBoneDesc.m_numChildren = pEntity->nChildren;
		rBoneDesc.m_nOffsetChildren = nChildrenIndexBase - nBoneIndex;
		for (int nChild = 0; nChild < pEntity->nChildren; ++nChild)
		{
			if (!RecursiveBoneLoader(nBoneIndex, nChildrenIndexBase + nChild))
				return 0;
		}
	}
	else
	{
		rBoneDesc.m_numChildren = 0;
		rBoneDesc.m_nOffsetChildren = 0;
	}
	return m_numBones;
}

namespace
{
struct BoneVertex
{
	Vec3 pos;
	int  matId;
	int  faceIndex;
	int  cornerIndex;
};

struct BoneVertexComparator
{
	bool operator()(const BoneVertex& v0, const BoneVertex& v1) const
	{
		int res = memcmp(&v0.pos, &v1.pos, sizeof(v0.pos));
		// The 'if' block below prevents sharing vertices between faces with different materials.
		// Note: you can comment the block out to enable sharing and decreases # of resulting
		// vertices in case of multi-material geometry.
		if (res == 0)
		{
			res = v0.matId - v1.matId;
		}
		return res < 0;
	}
};
}

static bool CompactBoneVertices(
  DynArray<Vec3>& outArrPositions,
  DynArray<char>& outArrMaterials,
  DynArray<uint16>& outArrIndices,
  const int inVertexCount,
  const CryVertex* const inVertices,
  const int inFaceCount,
  const CryFace* const inFaces)
{
	outArrPositions.clear();
	outArrMaterials.clear();
	outArrIndices.clear();

	DynArray<BoneVertex> verts;
	verts.reserve(inFaceCount * 3);

	outArrMaterials.reserve(inFaceCount);

	for (int i = 0; i < inFaceCount; ++i)
	{
		outArrMaterials.push_back(inFaces[i].MatID);

		for (int j = 0; j < 3; ++j)
		{
			const int vIdx = inFaces[i][j];
			if (vIdx < 0 || vIdx >= inVertexCount)
			{
				return false;
			}
			BoneVertex v;
			v.pos = inVertices[vIdx].p;
			v.matId = inFaces[i].MatID;
			v.faceIndex = i;
			v.cornerIndex = j;
			verts.push_back(v);
		}
	}

	std::sort(verts.begin(), verts.end(), BoneVertexComparator());

	outArrPositions.reserve(inVertexCount);   // preallocating by using a good enough guess
	outArrIndices.resize(3 * inFaceCount, -1);

	int outVertexCount = 0;
	for (int i = 0; i < verts.size(); ++i)
	{
		if (i == 0 || BoneVertexComparator()(verts[i - 1], verts[i]))
		{
			outArrPositions.push_back(verts[i].pos);
			++outVertexCount;
			if (outVertexCount > (1 << 16))
			{
				return false;
			}
		}
		outArrIndices[verts[i].faceIndex * 3 + verts[i].cornerIndex] = outVertexCount - 1;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadBoneMesh(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (pChunkDesc->chunkVersion != MESH_CHUNK_DESC_0745::VERSION &&
	    pChunkDesc->chunkVersion != MESH_CHUNK_DESC_0745::COMPATIBLE_OLD_VERSION)
	{
		m_LastError.Format(
		  "Unknown version (0x%x) of BoneMesh chunk. The only supported versions are 0x%x and 0x%x.",
		  (uint)pChunkDesc->chunkVersion,
		  (uint)MESH_CHUNK_DESC_0745::VERSION,
		  (uint)MESH_CHUNK_DESC_0745::COMPATIBLE_OLD_VERSION);
		return false;
	}

	if (pChunkDesc->size < sizeof(MESH_CHUNK_DESC_0745))
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Bad chunk size");
		return false;
	}

	MESH_CHUNK_DESC_0745* const pMeshChunk = (MESH_CHUNK_DESC_0745*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pMeshChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	if (pMeshChunk->nVerts <= 0)
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Bad vertex count (%d)", pMeshChunk->nVerts);
		return false;
	}
	if (pMeshChunk->nFaces <= 0)
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Bad face count (%d)", pMeshChunk->nFaces);
		return false;
	}
	if (pMeshChunk->nTVerts != 0)
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Texture coordinates found (%d)", pMeshChunk->nTVerts);
		return false;
	}
	if (pMeshChunk->flags1 != 0 || pMeshChunk->flags2 != 0)
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Flags are not 0 (0x%x, 0x%x)", (int)pMeshChunk->flags1, (int)pMeshChunk->flags2);
		return false;
	}

	// prepare vertices
	void* pRawData = pMeshChunk + 1;
	CryVertex* const pSrcVertices = (CryVertex*)pRawData;
	SwapEndian(pSrcVertices, pMeshChunk->nVerts, bSwapEndianness);
	if ((const char*)(pSrcVertices + pMeshChunk->nVerts) > (const char*)pRawData + (pChunkDesc->size - sizeof(*pMeshChunk)))
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Vertex data are truncated");
		return false;
	}
	pRawData = pSrcVertices + pMeshChunk->nVerts;

	// prepare indices and MatIDs
	CryFace* const pSrcFaces = (CryFace*)pRawData;
	SwapEndian(pSrcFaces, pMeshChunk->nFaces, bSwapEndianness);
	if ((const char*)(pSrcFaces + pMeshChunk->nFaces) > (const char*)pRawData + (pChunkDesc->size - sizeof(*pMeshChunk)))
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Vertex data are truncated");
		return false;
	}

	PhysicalProxy pbm;

	pbm.ChunkID = pChunkDesc->chunkId;

	// Bone meshes may contain many vertices sharing positions, so we
	// call CompactBoneVertices() to get vertices with unique positions only
	if (!CompactBoneVertices(
	      pbm.m_arrPoints,
	      pbm.m_arrMaterials,
	      pbm.m_arrIndices,
	      pMeshChunk->nVerts,
	      pSrcVertices,
	      pMeshChunk->nFaces,
	      pSrcFaces))
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Bad geometry (indices are out range or too many vertices in mesh)");
		return false;
	}

	// SS: I have no idea why we used this magic number of vertices (60000). I just keep it as is.
	if (pbm.m_arrPoints.size() > 60000)
	{
		m_LastError.Format("CLoaderCGF::ReadBoneMesh: Bad vertex count (%d)", pbm.m_arrPoints.size());
		return false;
	}

	for (int i = 0, n = pbm.m_arrPoints.size(); i < n; ++i)
	{
		pbm.m_arrPoints[i] *= VERTEX_SCALE;
	}

	CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
	pSkinningInfo->m_arrPhyBoneMeshes.push_back(pbm);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBones(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	COMPILED_BONE_CHUNK_DESC_0800* const pBIPChunk = (COMPILED_BONE_CHUNK_DESC_0800*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pBIPChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == pBIPChunk->VERSION)
	{
		CryBoneDescData_Comp* const pSrcVertices = (CryBoneDescData_Comp*)(pBIPChunk + 1);
		const uint32 nDataSize = pChunkDesc->size - sizeof(COMPILED_BONE_CHUNK_DESC_0800);
		const uint32 numBones = nDataSize / sizeof(CryBoneDescData_Comp);
		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrBonesDesc.resize(numBones);
		SwapEndian(pSrcVertices, (size_t)numBones, bSwapEndianness);

		for (uint32 i = 0; i < numBones; i++)
		{
			pSkinningInfo->m_arrBonesDesc[i].m_nControllerID = pSrcVertices[i].m_nControllerID;
			memcpy(
			  &pSkinningInfo->m_arrBonesDesc[i].m_fMass,
			  &pSrcVertices[i].m_fMass,
			  (INT_PTR)&pSrcVertices[i + 1] - (INT_PTR)&pSrcVertices[i].m_fMass);
			for (uint32 j = 0; j < 2; j++)
			{
				pSkinningInfo->m_arrBonesDesc[i].m_PhysInfo[j].pPhysGeom = (phys_geometry*)(INT_PTR)pSrcVertices[i].m_PhysInfo[j].nPhysGeom;
				memcpy(
				  &pSkinningInfo->m_arrBonesDesc[i].m_PhysInfo[j].flags,
				  &pSrcVertices[i].m_PhysInfo[j].flags,
				  (INT_PTR)&pSrcVertices[i].m_PhysInfo[j + 1] - (INT_PTR)&pSrcVertices[i].m_PhysInfo[j].flags);
			}
		}

		m_CompiledBones = true;
		return true;
	}

	m_LastError.Format("Unknown version of compiled bone chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledPhysicalBones(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	COMPILED_PHYSICALBONE_CHUNK_DESC_0800* const pChunk = (COMPILED_PHYSICALBONE_CHUNK_DESC_0800*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == pChunk->VERSION)
	{
		BONE_ENTITY* pSrcVertices = (BONE_ENTITY*)(pChunk + 1);
		const uint32 nDataSize = pChunkDesc->size - sizeof(COMPILED_PHYSICALBONE_CHUNK_DESC_0800);
		const uint32 numBones = nDataSize / sizeof(BONE_ENTITY);
		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrBoneEntities.resize(numBones);
		SwapEndian(pSrcVertices, (size_t)numBones, bSwapEndianness);
		for (uint32 i = 0; i < numBones; i++)
		{
			pSkinningInfo->m_arrBoneEntities[i] = pSrcVertices[i];
		}

		m_CompiledBones = true;
		return true;
	}

	m_LastError.Format("Unknown version of compiled physical bone chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledPhysicalProxies(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	// Note that this chunk type often contains non-aligned data. Because of that
	// we use chunk's data only after memcpy()'ing them.

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;

	COMPILED_PHYSICALPROXY_CHUNK_DESC_0800 chunk;
	memcpy(&chunk, pChunkDesc->data, sizeof(chunk));
	SwapEndian(chunk, bSwapEndianness);

	// Fixing bug of an old format: numPhysicalProxies was stored in little-endian
	// format when chunk header had 'big-endian' flag set.
	if (chunk.numPhysicalProxies > 0xffff)
	{
		SwapEndianBase(&chunk.numPhysicalProxies, 1);
	}

	if (pChunkDesc->chunkVersion == chunk.VERSION)
	{
		const uint8* rawdata = ((const uint8*)pChunkDesc->data) + sizeof(chunk);
		const uint32 numPhysicalProxies = chunk.numPhysicalProxies;

		for (uint32 i = 0; i < numPhysicalProxies; ++i)
		{
			SMeshPhysicalProxyHeader header;

			memcpy(&header, rawdata, sizeof(header));
			SwapEndian(header, bSwapEndianness);
			rawdata += sizeof(header);

			if (header.ChunkID > 0xffff)
			{
				SwapEndianBase(&header.ChunkID, 1);
			}

			PhysicalProxy sm;

			sm.ChunkID = header.ChunkID;

			//store the vertices
			static_assert(sizeof(sm.m_arrPoints[0]) == sizeof(Vec3), "Invalid type size!");
			sm.m_arrPoints.resize(header.numPoints);
			size_t size = sizeof(sm.m_arrPoints[0]) * header.numPoints;
			if (size > 0)
			{
				memcpy(sm.m_arrPoints.begin(), rawdata, size);
				SwapEndian(sm.m_arrPoints.begin(), (size_t)header.numPoints, bSwapEndianness);
				rawdata += size;
			}

			//store the indices
			static_assert(sizeof(sm.m_arrIndices[0]) == sizeof(uint16), "Invalid type size!");
			sm.m_arrIndices.resize(header.numIndices);
			size = sizeof(sm.m_arrIndices[0]) * header.numIndices;
			if (size > 0)
			{
				memcpy(sm.m_arrIndices.begin(), rawdata, size);
				SwapEndian(sm.m_arrIndices.begin(), (size_t)header.numIndices, bSwapEndianness);
				rawdata += size;
			}

			//store the materials
			static_assert(sizeof(sm.m_arrMaterials[0]) == sizeof(uint8), "Invalid type size!");
			sm.m_arrMaterials.resize(header.numMaterials);
			size = sizeof(sm.m_arrMaterials[0]) * header.numMaterials;
			if (size > 0)
			{
				memcpy(sm.m_arrMaterials.begin(), rawdata, size);
				SwapEndian(sm.m_arrMaterials.begin(), (size_t)header.numMaterials, bSwapEndianness);
				rawdata += size;
			}

			m_pCGF->GetSkinningInfo()->m_arrPhyBoneMeshes.push_back(sm);
		}
		return true;
	}

	m_LastError.Format("Unknown version of compiled physical proxies chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledMorphTargets(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	// Note that this chunk type often contains non-aligned data. Because of that
	// we use chunk's data only after memcpy()'ing them.

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;

	COMPILED_MORPHTARGETS_CHUNK_DESC_0800 chunk;
	memcpy(&chunk, pChunkDesc->data, sizeof(chunk));
	SwapEndian(chunk, bSwapEndianness);

	if (pChunkDesc->chunkVersion == chunk.VERSION || pChunkDesc->chunkVersion == chunk.VERSION1)
	{
		const uint8* rawdata = ((const uint8*)pChunkDesc->data) + sizeof(chunk);

		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		for (uint32 i = 0; i < chunk.numMorphTargets; ++i)
		{
			MorphTargetsPtr pSm = new MorphTargets;

			SMeshMorphTargetHeader header;

			memcpy(&header, rawdata, sizeof(header));
			SwapEndian(header, bSwapEndianness);
			rawdata += sizeof(header);

			pSm->MeshID = header.MeshID;

			pSm->m_strName = string((const char*)rawdata);
			rawdata += header.NameLength;

			// store the internal vertices&indices of morph-target
			static_assert(sizeof(pSm->m_arrIntMorph[0]) == sizeof(SMeshMorphTargetVertex), "Invalid type size!");
			pSm->m_arrIntMorph.resize(header.numIntVertices);
			size_t size = sizeof(pSm->m_arrIntMorph[0]) * header.numIntVertices;
			if (size > 0)
			{
				memcpy(pSm->m_arrIntMorph.begin(), rawdata, size);
				SwapEndian(pSm->m_arrIntMorph.begin(), (size_t)header.numIntVertices, bSwapEndianness);
				rawdata += size;
			}

			// store the external vertices&indices of morph-target
			static_assert(sizeof(pSm->m_arrExtMorph[0]) == sizeof(SMeshMorphTargetVertex), "Invalid type size!");
			pSm->m_arrExtMorph.resize(header.numExtVertices);
			size = sizeof(pSm->m_arrExtMorph[0]) * header.numExtVertices;
			if (size > 0)
			{
				memcpy(pSm->m_arrExtMorph.begin(), rawdata, size);
				SwapEndian(pSm->m_arrExtMorph.begin(), (size_t)header.numExtVertices, bSwapEndianness);
				rawdata += size;
			}

			pSkinningInfo->m_arrMorphTargets.push_back(pSm);
		}
		return true;
	}

	m_LastError.Format("Unknown version of compiled morph targets chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledIntFaces(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == COMPILED_INTFACES_CHUNK_DESC_0800::VERSION)
	{
		TFace* const pSrc = (TFace*)pChunkDesc->data;
		const uint32 numIntFaces = pChunkDesc->size / sizeof(pSrc[0]);
		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrIntFaces.resize(numIntFaces);
		SwapEndian(pSrc, (size_t)numIntFaces, bSwapEndianness);
		for (uint32 i = 0; i < numIntFaces; ++i)
		{
			pSkinningInfo->m_arrIntFaces[i] = pSrc[i];
		}
		m_CompiledMesh |= 2;
		return true;
	}

	m_LastError.Format("Unknown version of compiled int faces chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledIntSkinVertice(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	COMPILED_INTSKINVERTICES_CHUNK_DESC_0800* const pBIPChunk = (COMPILED_INTSKINVERTICES_CHUNK_DESC_0800*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pBIPChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == pBIPChunk->VERSION)
	{
		IntSkinVertex* const pSrcVertices = (IntSkinVertex*)(pBIPChunk + 1);
		const uint32 nDataSize = pChunkDesc->size - sizeof(COMPILED_INTSKINVERTICES_CHUNK_DESC_0800);
		const uint32 numIntVertices = nDataSize / sizeof(pSrcVertices[0]);
		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrIntVertices.resize(numIntVertices);
		SwapEndian(pSrcVertices, (size_t)numIntVertices, bSwapEndianness);
		for (uint32 i = 0; i < numIntVertices; ++i)
		{
			pSkinningInfo->m_arrIntVertices[i] = pSrcVertices[i];
		}
		m_CompiledMesh |= 1;
		return true;
	}

	m_LastError.Format("Unknown version of compiled skin vertices chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBonesBoxes(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == COMPILED_BONEBOXES_CHUNK_DESC_0800::VERSION ||
	    pChunkDesc->chunkVersion == COMPILED_BONEBOXES_CHUNK_DESC_0800::VERSION1)
	{
		char* pSrc = (char*)pChunkDesc->data;

		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrCollisions.push_back(MeshCollisionInfo());
		MeshCollisionInfo& info = pSkinningInfo->m_arrCollisions[pSkinningInfo->m_arrCollisions.size() - 1];

		static_assert(sizeof(info.m_iBoneId) == sizeof(int32), "Invalid type size!");
		SwapEndian((int32*)pSrc, 1, bSwapEndianness);
		memcpy(&info.m_iBoneId, pSrc, sizeof(info.m_iBoneId));
		pSrc += sizeof(info.m_iBoneId);

		static_assert(sizeof(info.m_aABB) == sizeof(AABB), "Invalid type size!");
		SwapEndian((AABB*)pSrc, 1, bSwapEndianness);
		memcpy(&info.m_aABB, pSrc, sizeof(info.m_aABB));
		pSrc += sizeof(info.m_aABB);

		int32 size;
		static_assert(sizeof(size) == sizeof(int32), "Invalid type size!");
		SwapEndian((int32*)pSrc, 1, bSwapEndianness);
		memcpy(&size, pSrc, sizeof(size));
		pSrc += sizeof(size);

		if (size > 0)
		{
			static_assert(sizeof(info.m_arrIndexes[0]) == sizeof(int16), "Invalid type size!");
			SwapEndian((int16*)pSrc, size, bSwapEndianness);
			info.m_arrIndexes.resize(size);
			memcpy(info.m_arrIndexes.begin(), pSrc, size * sizeof(info.m_arrIndexes[0]));
		}

		m_CompiledBonesBoxes = true;
		return true;
	}

	m_LastError.Format("Unknown version of compiled bone boxes chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledExt2IntMap(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	pChunkDesc->bSwapEndian = false;

	if (pChunkDesc->chunkVersion == COMPILED_EXT2INTMAP_CHUNK_DESC_0800::VERSION)
	{
		uint16* const pSrc = (uint16*)pChunkDesc->data;
		const uint32 numExtVertices = pChunkDesc->size / sizeof(pSrc[0]);
		CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();
		pSkinningInfo->m_arrExt2IntMap.resize(numExtVertices);
		SwapEndian(pSrc, (size_t)numExtVertices, bSwapEndianness);
		for (uint32 i = 0; i < numExtVertices; ++i)
		{
			assert(pSrc[i] != 0xffff);
			pSkinningInfo->m_arrExt2IntMap[i] = pSrc[i];
		}
		m_CompiledMesh |= 4;
		return true;
	}
	m_LastError.Format("Unknown version of compiled Ext2Int map chunk");
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBreakablePhysics(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkVersion != BREAKABLE_PHYSICS_CHUNK_DESC::VERSION)
	{
		m_LastError.Format("Unknown version of breakable physics chunk");
		return false;
	}

	BREAKABLE_PHYSICS_CHUNK_DESC* const pChunk = (BREAKABLE_PHYSICS_CHUNK_DESC*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(*pChunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	CPhysicalizeInfoCGF* const pPi = m_pCGF->GetPhysicalizeInfo();

	pPi->nGranularity = pChunk->granularity;
	pPi->nMode = pChunk->nMode;
	pPi->pRetVtx = new Vec3[pPi->nRetVtx = pChunk->nRetVtx];
	pPi->pRetTets = new int[(pPi->nRetTets = pChunk->nRetTets) * 4];
	if (pPi->nRetVtx > 0)
	{
		char* pData = ((char*)pChunk) + sizeof(BREAKABLE_PHYSICS_CHUNK_DESC);
		SwapEndian((Vec3*)pData, pPi->nRetVtx, bSwapEndianness);
		memcpy(pPi->pRetVtx, pData, pPi->nRetVtx * sizeof(Vec3));
	}
	if (pPi->nRetTets > 0)
	{
		char* pData = ((char*)pChunk) + sizeof(BREAKABLE_PHYSICS_CHUNK_DESC) + pPi->nRetVtx * sizeof(Vec3);
		SwapEndian((int*)pData, pPi->nRetTets * 4, bSwapEndianness);
		memcpy(pPi->pRetTets, pData, pPi->nRetTets * sizeof(int) * 4);
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

#if defined(RESOURCE_COMPILER)

namespace ProcessSkinningHelpers
{

struct RBatch
{
	uint32 startFace;
	uint32 numFaces;
	uint32 MaterialID;
};

bool SplitIntoRBatches(
  std::vector<SMeshSubset>& arrSubsets,
  std::vector<TFace>& arrExtFaces,
  string& lastError,
  const CMesh* const pMesh)
{
	arrSubsets.clear();
	arrExtFaces.clear();

	const int numSubsets = pMesh->m_subsets.size();

	//--------------------------------------------------------------------------
	//---         sort render-batches for hardware skinning                  ---
	//--------------------------------------------------------------------------

	std::vector<RBatch> arrBatches;
	for (uint32 m = 0; m < numSubsets; m++)
	{
		const int numFacesTotal = pMesh->GetIndexCount() / 3;
		const int firstFace = pMesh->m_subsets[m].nFirstIndexId / 3;
		const int numFaces = pMesh->m_subsets[m].nNumIndices / 3;
		if (firstFace >= numFacesTotal)
		{
			lastError.Format("Wrong first face id index (%i out of %i)", (int)firstFace, (int)numFacesTotal);
			return false;
		}
		if (numFaces <= 0 || firstFace + numFaces > numFacesTotal)
		{
			lastError.Format("Bad # of faces (%i)", (int)numFaces);
			return false;
		}

		{
			RBatch rbatch;
			rbatch.MaterialID = pMesh->m_subsets[m].nMatID;
			rbatch.startFace = arrExtFaces.size();
			rbatch.numFaces = numFaces;

			arrBatches.push_back(rbatch);
		}

		vtx_idx* const pIndices = &pMesh->m_pIndices[firstFace * 3];

		for (int i = 0; i < numFaces * 3; i += 3)
		{
			arrExtFaces.push_back(TFace(pIndices[i + 0], pIndices[i + 1], pIndices[i + 2]));
		}
	}

	//-------------------------------------------------------------------------
	//---                 check if material batches overlap                 ---
	//-------------------------------------------------------------------------
	{
		for (uint32 m = 0; m < numSubsets; m++)
		{
			uint32 vmin = 0xffffffff;
			uint32 vmax = 0x00000000;

			uint32 nFirstFaceId = pMesh->m_subsets[m].nFirstIndexId / 3;
			uint32 numFaces = pMesh->m_subsets[m].nNumIndices / 3;
			for (uint32 f = 0; f < numFaces; f++)
			{
				uint32 i0 = arrExtFaces[nFirstFaceId + f].i0;
				uint32 i1 = arrExtFaces[nFirstFaceId + f].i1;
				uint32 i2 = arrExtFaces[nFirstFaceId + f].i2;
				if (vmin > i0) vmin = i0;
				if (vmin > i1) vmin = i1;
				if (vmin > i2) vmin = i2;
				if (vmax < i0) vmax = i0;
				if (vmax < i1) vmax = i1;
				if (vmax < i2) vmax = i2;
			}
			uint32 a = pMesh->m_subsets[m].nFirstVertId;
			uint32 b = pMesh->m_subsets[m].nNumVerts;
			if (pMesh->m_subsets[m].nFirstVertId != vmin ||
			    pMesh->m_subsets[m].nNumVerts != vmax - vmin + 1)
			{
				lastError = "Overlapping material batches";
				return false;
			}
		}

		for (uint32 a = 0; a < numSubsets; a++)
		{
			for (uint32 b = 0; b < numSubsets; b++)
			{
				if (a == b) continue;
				uint32 amin = pMesh->m_subsets[a].nFirstVertId;
				uint32 amax = pMesh->m_subsets[a].nNumVerts + amin - 1;
				uint32 bmin = pMesh->m_subsets[b].nFirstVertId;
				uint32 bmax = pMesh->m_subsets[b].nNumVerts + bmin - 1;
				if (amax >= bmin && amin <= bmax)
				{
					lastError = "Overlapping material batches";
					return false;
				}
			}
		}
	}

	arrSubsets.resize(arrBatches.size());           //subsets of the mesh as they appear in video-mem
	for (uint32 m = 0; m < arrBatches.size(); m++)
	{
		uint32 mat = arrBatches[m].MaterialID;
		//	assert(mat<testcheck);
		uint32 r = 0;
		bool found = false;
		const uint32 numSubsets = pMesh->m_subsets.size();
		for (r = 0; r < numSubsets; r++)
		{
			if (mat == pMesh->m_subsets[r].nMatID)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			lastError.Format("Mesh subset for material %i was not found.", (int)mat);
			return false;
		}

		arrSubsets[m] = pMesh->m_subsets[r];
		arrSubsets[m].nMatID = arrBatches[m].MaterialID;
		arrSubsets[m].nFirstIndexId = arrBatches[m].startFace * 3;
		arrSubsets[m].nNumIndices = arrBatches[m].numFaces * 3;

		//Make sure the all vertices are in range if indices. This is important for ATI-kards
		uint32 sml_index = 0xffffffff;
		uint32 big_index = 0x00000000;
		uint32 sface = arrBatches[m].startFace;
		uint32 eface = arrBatches[m].numFaces + sface;
		for (uint32 i = sface; i < eface; i++)
		{
			uint32 i0 = arrExtFaces[i].i0;
			uint32 i1 = arrExtFaces[i].i1;
			uint32 i2 = arrExtFaces[i].i2;
			if (sml_index > i0) sml_index = i0;
			if (sml_index > i1) sml_index = i1;
			if (sml_index > i2) sml_index = i2;
			if (big_index < i0) big_index = i0;
			if (big_index < i1) big_index = i1;
			if (big_index < i2) big_index = i2;
		}
		arrSubsets[m].nFirstVertId = sml_index;
		arrSubsets[m].nNumVerts = big_index - sml_index + 1;
	}

	return true;
}

}
#endif

bool CLoaderCGF::ProcessSkinning()
{
	FUNCTION_PROFILER_3DENGINE;

	CSkinningInfo* const pSkinningInfo = m_pCGF->GetSkinningInfo();

	const uint32 numBones = pSkinningInfo->m_arrBonesDesc.size();

	//do we have an initialized skeleton?
	if (numBones == 0)
	{
		return true;
	}

	if (numBones > MAX_NUMBER_OF_BONES)
	{
		m_LastError.Format("Too many bones: %i. Reached limit of %i bones.", int(numBones), int(MAX_NUMBER_OF_BONES));
		return false;
	}

#if !defined(RESOURCE_COMPILER)

	if (m_CompiledMesh != 7 && m_CompiledBones != 1)
	{
		CryFatalError("%s tried to load a noncompiled mesh: %s", __FUNCTION__, m_filename);
		m_LastError.Format("noncompiled mesh");
		return false;
	}
	return true;

#else
	using namespace ProcessSkinningHelpers;

	if (m_CompiledBones == 0)
	{
		//process raw bone-data
		assert(m_numBonenameList < 2);
		assert(m_numBoneInitialPos < 2);
		assert(m_numBoneHierarchy < 2);

		const uint32 numIPos = m_arrInitPose34.size();
		if (numBones != numIPos)
		{
			m_LastError.Format("Skeleton-Initial-Positions are missing.");
			return false;
		}

		const uint32 numBonenames = m_arrBoneNameTable.size();
		if (numBones != numBonenames)
		{
			m_LastError.Format("Number of bones does not match in the bone hierarchy chunk (%d) and the bone name chunk (%d)", numBones, numBonenames);
			return false;
		}

		//------------------------------------------------------------------------------------------
		//----     use the old chunks to initialize the bone structure  ----------------------------
		//------------------------------------------------------------------------------------------
		static const char* const arrLimbNames[] =
		{
			"L UpperArm",
			"R UpperArm",
			"L Thigh",
			"R Thigh"
		};
		static const int limbNamesCount = CRY_ARRAY_COUNT(arrLimbNames);
		const uint32 numBonesDesc = pSkinningInfo->m_arrBonesDesc.size();
		for (uint32 nBone = 0; nBone < numBonesDesc; ++nBone)
		{
			uint32 nBoneID = m_arrIndexToId[nBone];
			if (nBoneID == ~0)
				continue;

			pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultW2B = m_arrInitPose34[nBoneID].GetInvertedFast();
			pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultB2W = m_arrInitPose34[nBoneID];

			memset(pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName, 0, sizeof(pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName));
			cry_strcpy(pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName, m_arrBoneNameTable[nBoneID].c_str());

			//fill in names of the bones and the limb IDs
			uint32 nBoneId = m_arrIndexToId[nBone];
			pSkinningInfo->m_arrBonesDesc[nBone].m_nLimbId = -1;
			for (int j = 0; j < limbNamesCount; ++j)
			{
				if (strstr(m_arrBoneNameTable[nBoneId], arrLimbNames[j]))
				{
					pSkinningInfo->m_arrBonesDesc[nBone].m_nLimbId = j;
					break;
				}
			}
		}
	}

	if (m_CompiledMesh != 0 && m_CompiledMesh != 7)
	{
		m_LastError.Format("Found mix of new and old chunks");
		return false;
	}

	if (m_CompiledMesh)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------
	//----     get the mesh    -----------------------------------------------------------------
	//------------------------------------------------------------------------------------------

	CNodeCGF* pNode = 0;
	for (int i = 0; i < m_pCGF->GetNodeCount(); ++i)
	{
		if (m_pCGF->GetNode(i)->type == CNodeCGF::NODE_MESH && m_pCGF->GetNode(i)->pMesh)
		{
			pNode = m_pCGF->GetNode(i);
			break;
		}
	}
	if (!pNode)
	{
		return true;
	}

	CMesh* const pMesh = pNode->pMesh;

	if (!pMesh)
	{
		m_LastError = "No mesh found";
		return false;
	}

	if (pMesh->m_pPositionsF16)
	{
		m_LastError.Format("Unexpected format of vertex positions: f16");
		return false;
	}

	const uint32 numIntVertices = pMesh->GetVertexCount();

	/* -----------------------------------------------------------------------------
	   // NOTE: Related to commented out code below!
	   // Color0 is copied into two distinct streams in the Engine:
	   // as Color in the Base stream and as indices in the Shape
	   // Deformation stream. The latter needs for the values to be
	   // snapped while the former doesn't.
	   // Snapping used to happen here at compiling/loading time, however
	   // this will destroy the Color information that might be needed in
	   // the Base stream.
	   // We are now snapping the values when copying this data into the
	   // Shape Deformation stream, while leaving the data copied in the
	   // Base stream untouched. A properer solution would be to create an
	   // additional chunk in the file and completely decouple the two
	   // datasets.

	   if (pMesh->m_pColor0)
	   {
	   //-----------------------------------------------------------------
	   //--- do a color-snap on all vcolors and calculate index        ---
	   //-----------------------------------------------------------------
	   for (uint32 i=0; i<numIntVertices; ++i)
	   {
	   pMesh->m_pColor0[i].r = (pMesh->m_pColor0[i].r>0x80) ? 0xff : 0x00;
	   pMesh->m_pColor0[i].g = (pMesh->m_pColor0[i].g>0x80) ? 0xff : 0x00;
	   pMesh->m_pColor0[i].b = (pMesh->m_pColor0[i].b>0x80) ? 0xff : 0x00;
	   pMesh->m_pColor0[i].a = 0;
	   //calculate the index (this is the only value we need in the vertex-shader)
	   if (pMesh->m_pColor0[i].r) pMesh->m_pColor0[i].a|=1;
	   if (pMesh->m_pColor0[i].g) pMesh->m_pColor0[i].a|=2;
	   if (pMesh->m_pColor0[i].b) pMesh->m_pColor0[i].a|=4;
	   }
	   }
	   ----------------------------------------------------------------------------- */

	//--------------------------------------------------------------
	//---        copy the links into geometry info               ---
	//--------------------------------------------------------------
	{
		const uint32 numLinks = m_arrLinksTmp.size();
		if (numIntVertices != numLinks)
		{
			m_LastError.Format("Different number of vertices (%i) and vertex links (%i)", (int)numIntVertices, (int)numLinks);
			return false;
		}
		assert(!m_arrIdToIndex.empty());

		for (uint32 i = 0; i < numIntVertices; ++i)
		{
			MeshUtils::VertexLinks& links = m_arrLinksTmp[i];

			for (int j = 0; j < (int)links.links.size(); ++j)
			{
				MeshUtils::VertexLinks::Link& cl = links.links[j];
				if (cl.boneId >= 0 && cl.boneId < (int)m_arrIdToIndex.size())
				{
					cl.boneId = m_arrIdToIndex[cl.boneId];
				}
				else
				{
					// bone index is out of range
					// if you get this assert, most probably there is desynchronization between different LODs of the same model
					// - all of them must be exported with exactly the same skeletons.
					assert(0);
					cl.boneId = 0;
				}
			}

			const char* const err = links.Normalize(MeshUtils::VertexLinks::eSort_ByWeight, 0.0f, (int)links.links.size());
			if (err)
			{
				m_LastError.Format("Internal error in skin compiler: %s", err);
				return false;
			}

			// Paranoid checks
			{
				float w = 0;
				for (int j = 0; j < (int)links.links.size(); ++j)
				{
					w += links.links[j].weight;
				}
				if (fabsf(w - 1.0f) > 0.005f)
				{
					m_LastError.Format("Internal error in skin compiler: %s", "sum of weights is not 1.0");
					return false;
				}

				for (int j = 1; j < (int)links.links.size(); ++j)
				{
					if (links.links[j - 1].weight < links.links[j].weight)
					{
						m_LastError.Format("Internal error in skin compiler: %s", "links are not sorted by weight");
						return false;
					}
				}
			}
		}
	}

	//---------------------------------------------------------------------
	// create internal SkinBuffer
	//---------------------------------------------------------------------
	bool bHasExtraBoneMappings = false;
	pSkinningInfo->m_arrIntVertices.resize(numIntVertices);
	for (uint32 nVert = 0; nVert < numIntVertices; ++nVert)
	{
		const MeshUtils::VertexLinks& rLinks_base = m_arrLinksTmp[nVert];

		const int numVertexLinks = (int)rLinks_base.links.size();
		assert(numVertexLinks > 0 && numVertexLinks <= 8);

		bHasExtraBoneMappings = bHasExtraBoneMappings || numVertexLinks > 4;

		IntSkinVertex v;

		if (pMesh->m_pColor0)
		{
			v.color = pMesh->m_pColor0[nVert].GetRGBA();
		}
		else
		{
			v.color = ColorB(0xff, 0xff, 0xff, 1 | 2 | 4);
		}

		v.__obsolete0 = Vec3(ZERO);
		v.__obsolete2 = Vec3(ZERO);

		const int n = std::min(numVertexLinks, (int)CRY_ARRAY_COUNT(v.weights));
		for (int j = 0; j < n; ++j)
		{
			v.boneIDs[j] = rLinks_base.links[j].boneId;
			v.weights[j] = rLinks_base.links[j].weight;
		}
		for (int j = n; j < CRY_ARRAY_COUNT(v.weights); ++j)
		{
			v.boneIDs[j] = 0;
			v.weights[j] = 0;
		}

		// transform position from bone-space to world-space
		v.pos = Vec3(ZERO);
		for (int j = 0; j < numVertexLinks; ++j)
		{
			v.pos +=
			  pSkinningInfo->m_arrBonesDesc[rLinks_base.links[j].boneId].m_DefaultB2W *
			  rLinks_base.links[j].offset *
			  rLinks_base.links[j].weight;
		}

		pSkinningInfo->m_arrIntVertices[nVert] = v;
	}

	//--------------------------------------------------------------------------
	// sort faces by subsets
	//--------------------------------------------------------------------------
	// for each subset, construct an array of faces and keep the faces (in the original order) in there
	typedef std::map<uint8, std::vector<TFace>> SubsetFacesMap;
	SubsetFacesMap mapSubsetFaces;

	if (numIntVertices > (1 << 16))
	{
		m_LastError.Format("Too many vertices in skin geometry: %i (max possible is %i)", numIntVertices, (1 << 16));
		return false;
	}

	// put each face into its subset group;
	// the corresponding groups will be created by the map automatically
	// upon the first request of that group
	const uint32 numIntFaces = pMesh->GetFaceCount();

	for (uint32 i = 0; i < numIntFaces; ++i)
	{
		const int subsetIdx = pMesh->m_pFaces[i].nSubset;
		if (subsetIdx < 0 || subsetIdx > pMesh->m_subsets.size())
		{
			m_LastError.Format("Invalid subset index detected: %i (# of subsets: %i)", subsetIdx, (int)pMesh->m_subsets.size());
			return false;
		}
		if (pMesh->m_subsets[subsetIdx].nMatID >= MAX_SUB_MATERIALS)
		{
			m_LastError.Format("Maximum number of submaterials reached (%i)", (int)MAX_SUB_MATERIALS);
			return false;
		}

		int vIdx[3];
		for (int j = 0; j < 3; ++j)
		{
			vIdx[j] = pMesh->m_pFaces[i].v[j];
			if (vIdx[j] < 0)
			{
				m_LastError.Format("Internal vertex index %i is negative (# of vertices is %i)", vIdx[j], numIntVertices);
				return false;
			}
			if (vIdx[j] >= numIntVertices)
			{
				m_LastError.Format("Internal vertex index %i is out of range (# of vertices is %i)", vIdx[j], numIntVertices);
				return false;
			}
		}
		mapSubsetFaces[subsetIdx].push_back(TFace(vIdx[0], vIdx[1], vIdx[2]));
	}

	if (pMesh->GetSubSetCount() != mapSubsetFaces.size())
	{
		m_LastError.Format("Number of referenced subsets (%d) is not equal to number of stored subsets (%i)", (int)mapSubsetFaces.size(), pMesh->GetSubSetCount());
		return false;
	}

	//--------------------------------------------------------------------------
	// create array with internal faces (sorted by subsets)
	//--------------------------------------------------------------------------
	{
		pSkinningInfo->m_arrIntFaces.resize(numIntFaces);

		int newFaceCount = 0;
		for (SubsetFacesMap::iterator itMtl = mapSubsetFaces.begin(); itMtl != mapSubsetFaces.end(); ++itMtl)
		{
			const size_t faceCount = itMtl->second.size();
			for (size_t f = 0; f < faceCount; ++f, ++newFaceCount)
			{
				pSkinningInfo->m_arrIntFaces[newFaceCount] = itMtl->second[f];
			}
		}
		assert(newFaceCount == numIntFaces);
	}

	// Compile contents.
	// These map from internal (original) to external (optimized) indices/vertices
	std::vector<int> arrVRemapping;
	std::vector<int> arrIRemapping;

	m_pCompiledCGF = MakeCompiledSkinCGF(m_pCGF, &arrVRemapping, &arrIRemapping);
	if (!m_pCompiledCGF)
	{
		return false;
	}
	const uint32 numVRemapping = arrVRemapping.size();
	if (numVRemapping == 0)
	{
		m_LastError = "Empty vertex remapping";
		return false;
	}
	if (arrIRemapping.size() != numIntFaces * 3)
	{
		m_LastError.Format("Wrong # of indices for remapping");
		return false;
	}

	//allocates the external to internal map entries
	// (m_arrExt2IntMap[]: for each final vertex contains its index in initial vertex buffer)
	// TODO: in theory arrVRemapping.size() is not necessarily equals to the number of final vertices.
	// It could be bigger if a face is not referenced from any of the subsets.
	pSkinningInfo->m_arrExt2IntMap.resize(numVRemapping, ~0);
	for (uint32 i = 0; i < numIntFaces; ++i)
	{
		const uint32 idx0 = arrVRemapping[arrIRemapping[i * 3 + 0]];
		const uint32 idx1 = arrVRemapping[arrIRemapping[i * 3 + 1]];
		const uint32 idx2 = arrVRemapping[arrIRemapping[i * 3 + 2]];
		if (idx0 >= numVRemapping || idx1 >= numVRemapping || idx2 >= numVRemapping)
		{
			m_LastError.Format("Indices out of range");
			return false;
		}
		pSkinningInfo->m_arrExt2IntMap[idx0] = pSkinningInfo->m_arrIntFaces[i].i0;
		pSkinningInfo->m_arrExt2IntMap[idx1] = pSkinningInfo->m_arrIntFaces[i].i1;
		pSkinningInfo->m_arrExt2IntMap[idx2] = pSkinningInfo->m_arrIntFaces[i].i2;
	}

	{
		int brokenCount = 0;
		for (uint32 i = 0; i < numVRemapping; i++)
		{
			if (pSkinningInfo->m_arrExt2IntMap[i] >= numIntVertices)
			{
				++brokenCount;
				// "Fixing" mapping allows us to comment out "return false" below (in case of an urgent need)
				pSkinningInfo->m_arrExt2IntMap[i] = 0;
			}
		}
		if (brokenCount > 0)
		{
			m_LastError.Format("Remapping-table is broken. %i of %i vertices are not remapped", brokenCount, numVRemapping);
			return false;
		}
	}

	//-------------------------------------------------------------------------

	std::vector<SMeshSubset> arrSubsets;
	std::vector<TFace> arrExtFaces;

	if (!SplitIntoRBatches(arrSubsets, arrExtFaces, m_LastError, pMesh))
	{
		return false;
	}

	//--------------------------------------------------------------------------
	//---              copy compiled-data back into CMesh                    ---
	//--------------------------------------------------------------------------
	for (size_t f = 0, n = arrExtFaces.size(); f < n; ++f)
	{
		pMesh->m_pIndices[f * 3 + 0] = arrExtFaces[f].i0;
		pMesh->m_pIndices[f * 3 + 1] = arrExtFaces[f].i1;
		pMesh->m_pIndices[f * 3 + 2] = arrExtFaces[f].i2;
	}

	pMesh->m_subsets.clear();
	pMesh->m_subsets.reserve(arrSubsets.size());
	for (size_t i = 0; i < arrSubsets.size(); ++i)
	{
		pMesh->m_subsets.push_back(arrSubsets[i]);
	}

	//////////////////////////////////////////////////////////////////////////
	// Create and fill bone-mapping streams.
	//////////////////////////////////////////////////////////////////////////
	{
		pMesh->ReallocStream(CMesh::BONEMAPPING, numVRemapping);
		if (bHasExtraBoneMappings)
		{
			pMesh->ReallocStream(CMesh::EXTRABONEMAPPING, numVRemapping);
		}

		for (uint32 i = 0; i < numVRemapping; ++i)
		{
			const uint32 index = pSkinningInfo->m_arrExt2IntMap[i];
			const MeshUtils::VertexLinks& links = m_arrLinksTmp[index];
			const int linkCount = (int)links.links.size();

			// Convert floating point weights to integer [0;255] weights
			int w[8];
			{
				assert(linkCount <= 8);

				int wSum = 0;
				for (int j = 0; j < linkCount; ++j)
				{
					w[j] = (int)(links.links[j].weight * 255.0f + 0.5f);
					wSum += w[j];
				}

				// Ensure that the sum of weights is exactly 255.
				// Note that the code below preserves sorting by
				// weight in descending order.
				if (wSum < 255)
				{
					w[0] += 255 - wSum;
				}
				else if (wSum > 255)
				{
					for (int j = 0;; ++j)
					{
						if (j >= linkCount - 1 || w[j] > w[j + 1])
						{
							--w[j];
							if (--wSum == 255)
							{
								break;
							}
							j = std::max(j - 1, 0) - 1;
						}
					}
				}

				// TODO: quantization to integer values might produce zero weight links, so
				// it might be a good idea to delete such links. Warning: m_arrIntVertices[]
				// also stores (up to) four links, so we should delete matching zero-weight
				// links in m_arrIntVertices[] as well.
			}

			// Fill CMesh::BONEMAPPING stream
			{
				const int n = std::min(linkCount, 4);
				for (int j = 0; j < n; ++j)
				{
					pMesh->m_pBoneMapping[i].boneIds[j] = links.links[j].boneId;
					pMesh->m_pBoneMapping[i].weights[j] = (uint8)w[j];
				}
				for (int j = n; j < 4; ++j)
				{
					pMesh->m_pBoneMapping[i].boneIds[j] = 0;
					pMesh->m_pBoneMapping[i].weights[j] = 0;
				}
			}

			// Fill CMesh::EXTRABONEMAPPING stream
			if (bHasExtraBoneMappings)
			{
				const int n = std::max(linkCount - 4, 0);
				for (int j = 0; j < n; ++j)
				{
					pMesh->m_pExtraBoneMapping[i].boneIds[j] = links.links[4 + j].boneId;
					pMesh->m_pExtraBoneMapping[i].weights[j] = (uint8)w[4 + j];
				}
				for (int j = n; j < 4; ++j)
				{
					pMesh->m_pExtraBoneMapping[i].boneIds[j] = 0;
					pMesh->m_pExtraBoneMapping[i].weights[j] = 0;
				}
			}
		}
	}

	// Keep original transform for morph targets
	Matrix34 mat34 = pNode->localTM * Diag33(VERTEX_SCALE, VERTEX_SCALE, VERTEX_SCALE);

	//////////////////////////////////////////////////////////////////////////
	// Copy shape-deformation and positions.
	//////////////////////////////////////////////////////////////////////////
	{
		// Modify orientation, but keep translation.
		// We need to keep translation to be able to use pivot of the node.
		// It allows us to control coordinate origin for FP16 meshes.
		// The translation is applied later before skinning.
		const Matrix34 oldWorldTM = pNode->worldTM;
		const Vec3 translation = oldWorldTM.GetTranslation();
		pNode->worldTM = Matrix34(Matrix33(IDENTITY), translation);

		// Reconstruct localTM out of new worldTM
		if (pNode->pParent)
		{
			Matrix34 parentWorldInverted = pNode->pParent->worldTM;
			parentWorldInverted.Invert();
			pNode->localTM = parentWorldInverted * pNode->worldTM;
		}
		else
		{
			pNode->localTM = pNode->worldTM;
		}

		for (uint32 e = 0; e < numVRemapping; ++e)
		{
			const uint32 i = pSkinningInfo->m_arrExt2IntMap[e];
			const IntSkinVertex& intVertex = pSkinningInfo->m_arrIntVertices[i];
			pMesh->m_pPositions[e] = intVertex.pos - translation;
		}

		// The exporting pipeline is expected to produce pNode->worldTM
		// with identity orientation only, but let's be paranoid and handle
		// non-identity orientations properly as well.

		const float eps = 0.001f;
		const bool bIdentity =
		  oldWorldTM.GetColumn0().IsEquivalent(Vec3(1, 0, 0), eps) &&
		  oldWorldTM.GetColumn1().IsEquivalent(Vec3(0, 1, 0), eps) &&
		  oldWorldTM.GetColumn2().IsEquivalent(Vec3(0, 0, 1), eps);

		if (!bIdentity)
		{
			for (uint32 e = 0; e < numVRemapping; ++e)
			{
				const uint32 i = pSkinningInfo->m_arrExt2IntMap[e];

				pMesh->m_pNorms[e].RotateSafelyBy(oldWorldTM);
				pMesh->m_pTangents[i].RotateSafelyBy(oldWorldTM);
			}
		}
	}

	//--------------------------------------------------------------------------
	//---              prepare morph-targets                                 ---
	//--------------------------------------------------------------------------
	uint32 numMorphTargets = pSkinningInfo->m_arrMorphTargets.size();
	for (uint32 it = 0; it < numMorphTargets; ++it)
	{
		//init internal morph-targets
		MorphTargets* pMorphtarget = pSkinningInfo->m_arrMorphTargets[it];
		uint32 numMorphVerts = pMorphtarget->m_arrIntMorph.size();
		uint32 intVertexCount = pSkinningInfo->m_arrIntVertices.size();
		for (uint32 i = 0; i < numMorphVerts; i++)
		{
			uint32 idx = pMorphtarget->m_arrIntMorph[i].nVertexId;
			assert(idx < intVertexCount);
			Vec3 mvertex = (mat34 * pMorphtarget->m_arrIntMorph[i].ptVertex) - pSkinningInfo->m_arrIntVertices[idx].pos;
			pMorphtarget->m_arrIntMorph[i].ptVertex = mvertex;
		}

		//init external morph-targets
		for (uint32 v = 0; v < numMorphVerts; v++)
		{
			uint32 idx = pMorphtarget->m_arrIntMorph[v].nVertexId;
			Vec3 mvertex = pMorphtarget->m_arrIntMorph[v].ptVertex;

			const uint16* pExtToIntMap = &pSkinningInfo->m_arrExt2IntMap[0];
			uint32 numExtVertices = numVRemapping;
			assert(numExtVertices);
			for (uint32 i = 0; i < numExtVertices; ++i)
			{
				uint32 index = pExtToIntMap[i];
				if (index == idx)
				{
					SMeshMorphTargetVertex mp;
					mp.nVertexId = i;
					mp.ptVertex = mvertex;
					pMorphtarget->m_arrExtMorph.push_back(mp);
				}
			}
		}
	}

	pMesh->m_bbox.Reset();
	for (size_t v = 0; v < numVRemapping; ++v)
	{
		pMesh->m_bbox.Add(pMesh->m_pPositions[v]);
	}

	return true;
#endif
}

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
#if defined(RESOURCE_COMPILER)

CContentCGF* CLoaderCGF::MakeCompiledSkinCGF(
  CContentCGF* pCGF, std::vector<int>* pVertexRemapping, std::vector<int>* pIndexRemapping) PREFAST_SUPPRESS_WARNING(6262) //function uses > 32k stack space
{
	CContentCGF* const pCompiledCGF = new CContentCGF(pCGF->GetFilename());
	*pCompiledCGF->GetExportInfo() = *pCGF->GetExportInfo(); // Copy export info.

	// Compile mesh.
	// Note that this function cannot fill/return mapping arrays properly in case of
	// multiple meshes (because mapping is per-mesh), so we will treat multiple
	// meshes as error.
	bool bMeshFound = false;
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNodeCGF = pCGF->GetNode(i);
		if (!pNodeCGF->pMesh || pNodeCGF->type != CNodeCGF::NODE_MESH || pNodeCGF->bPhysicsProxy)
		{
			continue;
		}

		if (bMeshFound)
		{
			m_LastError.Format("Failed to compile skinned geometry file %s - %s", pCGF->GetFilename(), "*multiple* mesh nodes aren't supported");
			delete pCompiledCGF;
			return 0;
		}

		bMeshFound = true;

		mesh_compiler::CMeshCompiler meshCompiler;

		meshCompiler.SetIndexRemapping(pIndexRemapping);
		meshCompiler.SetVertexRemapping(pVertexRemapping);

		int flags = mesh_compiler::MESH_COMPILE_TANGENTS | mesh_compiler::MESH_COMPILE_OPTIMIZE;
		if (pCompiledCGF->GetExportInfo()->bUseCustomNormals)
		{
			flags |= mesh_compiler::MESH_COMPILE_USE_CUSTOM_NORMALS;
		}

		if (!meshCompiler.Compile(*pNodeCGF->pMesh, flags))
		{
			m_LastError.Format("Failed to compile skinned geometry file %s - %s", pCGF->GetFilename(), meshCompiler.GetLastError());
			delete pCompiledCGF;
			return 0;
		}

		pCompiledCGF->AddNode(pNodeCGF);

		// We continue scanning just to detect if we have multiple mesh nodes (to throw an error)
	}

	// Compile physics proxy nodes.
	if (pCGF->GetExportInfo()->bHavePhysicsProxy)
	{
		for (int i = 0; i < pCGF->GetNodeCount(); ++i)
		{
			CNodeCGF* pNodeCGF = pCGF->GetNode(i);
			if (pNodeCGF->pMesh && pNodeCGF->bPhysicsProxy)
			{
				// Compile physics proxy mesh.
				mesh_compiler::CMeshCompiler meshCompiler;
				if (!meshCompiler.Compile(*pNodeCGF->pMesh, mesh_compiler::MESH_COMPILE_OPTIMIZE))
				{
					m_LastError.Format("Failed to compile skinned geometry in node %s in file %s - %s", pNodeCGF->name, pCGF->GetFilename(), meshCompiler.GetLastError());
					delete pCompiledCGF;
					return 0;
				}
			}
			pCompiledCGF->AddNode(pNodeCGF);
		}
	}

	return pCompiledCGF;
}

#endif

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadExportFlagsChunk(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (pChunkDesc->chunkVersion != EXPORT_FLAGS_CHUNK_DESC::VERSION)
	{
		m_LastError.Format("Unknown version of export flags chunk");
		return false;
	}

	EXPORT_FLAGS_CHUNK_DESC& chunk = *(EXPORT_FLAGS_CHUNK_DESC*)pChunkDesc->data;
	SwapEndian(chunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	CExportInfoCGF* pExportInfo = m_pCGF->GetExportInfo();
	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::MERGE_ALL_NODES)
		pExportInfo->bMergeAllNodes = true;
	else
		pExportInfo->bMergeAllNodes = false;

	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::HAVE_AUTO_LODS)
		pExportInfo->bHaveAutoLods = true;
	else
		pExportInfo->bHaveAutoLods = false;

	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::USE_CUSTOM_NORMALS)
		pExportInfo->bUseCustomNormals = true;
	else
		pExportInfo->bUseCustomNormals = false;

	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::WANT_F32_VERTICES)
		pExportInfo->bWantF32Vertices = true;
	else
		pExportInfo->bWantF32Vertices = false;

	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::EIGHT_WEIGHTS_PER_VERTEX)
		pExportInfo->b8WeightsPerVertex = true;
	else
		pExportInfo->b8WeightsPerVertex = false;

	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::MAKE_VCLOTH)
		pExportInfo->bMakeVCloth = true;
	else
		pExportInfo->bMakeVCloth = false;

	return true;
}

inline const char* stristr2(const char* szString, const char* szSubstring)
{
	int nSuperstringLength = (int)strlen(szString);
	int nSubstringLength = (int)strlen(szSubstring);

	for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
	{
		if (strnicmp(szString + nSubstringPos, szSubstring, nSubstringLength) == 0)
			return szString + nSubstringPos;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadNodeChunk(IChunkFile::ChunkDesc* pChunkDesc, bool bJustGeometry)
{
	if (pChunkDesc->chunkVersion != NODE_CHUNK_DESC_0824::VERSION &&
	    pChunkDesc->chunkVersion != NODE_CHUNK_DESC_0824::COMPATIBLE_OLD_VERSION)
	{
		m_LastError.Format(
		  "Unknown version (0x%x) of Node chunk. The only supported versions are 0x%x and 0x%x.",
		  (uint)pChunkDesc->chunkVersion,
		  (uint)NODE_CHUNK_DESC_0824::VERSION,
		  (uint)NODE_CHUNK_DESC_0824::COMPATIBLE_OLD_VERSION);
		return false;
	}

	NODE_CHUNK_DESC_0824* nodeChunk = (NODE_CHUNK_DESC_0824*)pChunkDesc->data;
	assert(nodeChunk);
	SwapEndian(*nodeChunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	CNodeCGF* pNodeCGF = Construct<CNodeCGF>(InplaceFactory(m_pDestructFnc), m_pAllocFnc);
	m_pCGF->AddNode(pNodeCGF);

	cry_strcpy(pNodeCGF->name, nodeChunk->name);

	// Fill node object.
	pNodeCGF->nChunkId = pChunkDesc->chunkId;
	pNodeCGF->nParentChunkId = nodeChunk->ParentID;
	pNodeCGF->nObjectChunkId = nodeChunk->ObjectID;
	pNodeCGF->pParent = 0;
	pNodeCGF->pMesh = 0;

	pNodeCGF->pos_cont_id = nodeChunk->pos_cont_id;
	pNodeCGF->rot_cont_id = nodeChunk->rot_cont_id;
	pNodeCGF->scl_cont_id = nodeChunk->scl_cont_id;

	pNodeCGF->pMaterial = 0;
	if (nodeChunk->MatID > 0)
	{
		pNodeCGF->pMaterial = LoadMaterialFromChunk(nodeChunk->MatID);
		if (!pNodeCGF->pMaterial)
		{
			return false;
		}
	}

	{
		const float* const pMat = &nodeChunk->tm[0][0];

		pNodeCGF->localTM.SetFromVectors(
		  Vec3(pMat[0], pMat[1], pMat[2]),
		  Vec3(pMat[4], pMat[5], pMat[6]),
		  Vec3(pMat[8], pMat[9], pMat[10]),
		  Vec3(pMat[12] * VERTEX_SCALE, pMat[13] * VERTEX_SCALE, pMat[14] * VERTEX_SCALE));
	}

	if (nodeChunk->PropStrLen > 0)
	{
		pNodeCGF->properties.Format("%.*s", nodeChunk->PropStrLen, ((const char*)nodeChunk) + sizeof(*nodeChunk));
	}

	if (stristr2(nodeChunk->name, CGF_NODE_NAME_PHYSICS_PROXY0) ||
	    stristr2(nodeChunk->name, CGF_NODE_NAME_PHYSICS_PROXY1) ||
	    stristr2(nodeChunk->name, CGF_NODE_NAME_PHYSICS_PROXY2))
	{
		pNodeCGF->type = CNodeCGF::NODE_HELPER;
		pNodeCGF->bPhysicsProxy = true;
		m_pCGF->GetExportInfo()->bHavePhysicsProxy = true;
	}
	else if (nodeChunk->name[0] == '$')
	{
		pNodeCGF->type = CNodeCGF::NODE_HELPER;
		pNodeCGF->bPhysicsProxy = false;
	}
	else
	{
		pNodeCGF->type = CNodeCGF::NODE_MESH;
		pNodeCGF->bPhysicsProxy = false;
	}

	// Check if valid object node.
	if (nodeChunk->ObjectID > 0)
	{
		IChunkFile::ChunkDesc* const pObjChunkDesc = m_pChunkFile->FindChunkById(nodeChunk->ObjectID);
		if (!pObjChunkDesc)
		{
			assert(pObjChunkDesc);
			m_LastError.Format("Failed to find chunk with id %d", nodeChunk->ObjectID);
			return false;
		}
		if (pObjChunkDesc->chunkType == ChunkType_Mesh)
		{
			if (pNodeCGF->type == CNodeCGF::NODE_HELPER)
				pNodeCGF->helperType = HP_GEOMETRY;
			if (!LoadGeomChunk(pNodeCGF, pObjChunkDesc))
				return false;
		}
		else if (!bJustGeometry)
		{
			if (pObjChunkDesc->chunkType == ChunkType_Helper)
			{
				pNodeCGF->type = CNodeCGF::NODE_HELPER;
				if (!LoadHelperChunk(pNodeCGF, pObjChunkDesc))
					return false;
			}
		}
	}
	else
	{
		//	pNodeCGF->type = CNodeCGF::NODE_HELPER;
		//	pNodeCGF->helperType = HP_POINT;
		//	pNodeCGF->helperSize = Vec3(0.01f,0.01f,0.01f);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadHelperChunk(CNodeCGF* pNode, IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkVersion != HELPER_CHUNK_DESC::VERSION)
	{
		m_LastError.Format("Unknown version of Helper chunk");
		return false;
	}

	HELPER_CHUNK_DESC& chunk = *(HELPER_CHUNK_DESC*)pChunkDesc->data;
	assert(&chunk);

	SwapEndian(chunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	// Fill node object.
	pNode->helperType = chunk.type;
	pNode->helperSize = chunk.size;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLoaderCGF::SetupMeshSubsets(CMesh& mesh, CMaterialCGF* pMaterialCGF)
{
	if (!m_pCGF->GetExportInfo()->bCompiledCGF && mesh.m_subsets.empty())
	{
		const DynArray<int>& usedMaterialIds = m_pCGF->GetUsedMaterialIDs();
		for (int i = 0; i < usedMaterialIds.size(); ++i)
		{
			SMeshSubset meshSubset;
			meshSubset.nMatID = usedMaterialIds[i];
			meshSubset.nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			mesh.m_subsets.push_back(meshSubset);
		}
	}

	// Copy physicalization type from material to subsets (and fix subsets' matId if needed)
	if (pMaterialCGF)
	{
		for (int i = 0; i < mesh.m_subsets.size(); ++i)
		{
			SMeshSubset& meshSubset = mesh.m_subsets[i];
			if (pMaterialCGF->subMaterials.size() > 0)
			{
				int id = meshSubset.nMatID;
				if (id >= (int)pMaterialCGF->subMaterials.size())
				{
					// Let's use 3dsMax's approach of handling material ids out of range
					id %= (int)pMaterialCGF->subMaterials.size();
				}

				if (id >= 0 && pMaterialCGF->subMaterials[id] != nullptr)
				{
					meshSubset.nPhysicalizeType = pMaterialCGF->subMaterials[id]->nPhysicalizeType;
				}
				else
				{
					Warning("Submaterial %d is not available for subset %d in %s", meshSubset.nMatID, i, m_filename);
				}
			}
			else
			{
				meshSubset.nPhysicalizeType = pMaterialCGF->nPhysicalizeType;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadGeomChunk(CNodeCGF* pNode, IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	// First check if this geometry chunk was already loaded by some node.
	int nNumNodes = m_pCGF->GetNodeCount();
	for (int i = 0; i < nNumNodes; i++)
	{
		CNodeCGF* pOldNode = m_pCGF->GetNode(i);
		if (pOldNode != pNode && pOldNode->nObjectChunkId == pChunkDesc->chunkId)
		{
			pNode->pMesh = pOldNode->pMesh;
			pNode->pSharedMesh = pOldNode;
			return true;
		}
	}

	assert(pChunkDesc && pChunkDesc->chunkType == ChunkType_Mesh);

	if (pChunkDesc->chunkVersion == MESH_CHUNK_DESC_0801::VERSION ||
	    pChunkDesc->chunkVersion == MESH_CHUNK_DESC_0801::COMPATIBLE_OLD_VERSION)
	{
		m_pCGF->GetExportInfo()->bCompiledCGF = true;
		return LoadCompiledMeshChunk(pNode, pChunkDesc);
	}

	// Uncompiled format
	if (pChunkDesc->chunkVersion == MESH_CHUNK_DESC_0745::VERSION ||
	    pChunkDesc->chunkVersion == MESH_CHUNK_DESC_0745::COMPATIBLE_OLD_VERSION)
	{
#if !defined(RESOURCE_COMPILER)
		m_LastError.Format("%s: non-compiled geometry chunk in %s", __FUNCTION__, m_filename);
		return false;
#else
		m_pCGF->GetExportInfo()->bCompiledCGF = false;

		const int maxLinkCount =
		  m_pCGF->GetExportInfo()->b8WeightsPerVertex
		  ? 8
		  : ((m_maxWeightsPerVertex <= 8) ? m_maxWeightsPerVertex : 8);      // CMesh doesn't support more than 8 weights

		const bool bSwapEndianness = pChunkDesc->bSwapEndian;
		pChunkDesc->bSwapEndian = false;

		uint8* pMeshChunkData = (uint8*)pChunkDesc->data;

		MESH_CHUNK_DESC_0745* chunk;
		StepData(chunk, pMeshChunkData, 1, bSwapEndianness);

		if (!(chunk->flags2 & MESH_CHUNK_DESC_0745::FLAG2_HAS_TOPOLOGY_IDS))
		{
			m_LastError.Format("%s: obsolete non-compiled geometry chunk format in %s", __FUNCTION__, m_filename);
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		// Preparing source mesh data (may contain duplicate vertices)
		//////////////////////////////////////////////////////////////////////////

		MeshUtils::Mesh mesh;
		const char* err = 0;

		if (chunk->nVerts <= 0)
		{
			m_LastError.Format("%s: missing vertices in %s", __FUNCTION__, m_filename);
			return false;
		}
		if (chunk->nFaces <= 0)
		{
			m_LastError.Format("%s: missing faces in %s", __FUNCTION__, m_filename);
			return false;
		}
		if (chunk->nTVerts != 0 && chunk->nTVerts != chunk->nVerts)
		{
			m_LastError.Format("%s: Number of texture coordinates doesn't match number of vertices", __FUNCTION__);
			return false;
		}

		// Preparing positions and normals
		{
			CryVertex* p;
			StepData(p, pMeshChunkData, chunk->nVerts, bSwapEndianness);

			err = mesh.SetPositions(&p->p.x, chunk->nVerts, sizeof(*p), VERTEX_SCALE);       // VERTEX_SCALE - to convert from centimeters to meters
			if (!err)
			{
				err = mesh.SetNormals(&p->n.x, chunk->nVerts, sizeof(*p));
			}

			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		// Preparing faces and face material IDs
		{
			CryFace* p;
			StepData(p, pMeshChunkData, chunk->nFaces, bSwapEndianness);
			err = mesh.SetFaces(&p->v0, chunk->nFaces, sizeof(*p));
			if (!err)
			{
				err = mesh.SetFaceMatIds(&p->MatID, chunk->nFaces, sizeof(*p), MAX_SUB_MATERIALS - 1);
			}
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
			mesh.RemoveDegradedFaces();
		}

		// Preparing topology IDs
		{
			int* p;
			StepData(p, pMeshChunkData, chunk->nVerts, bSwapEndianness);
			err = mesh.SetTopologyIds(p, chunk->nVerts, sizeof(*p));
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		// Preparing texture coordinates
		if (chunk->nTVerts > 0)
		{
			CryUV* p;
			StepData(p, pMeshChunkData, chunk->nVerts, bSwapEndianness);
			err = mesh.SetTexCoords(&p->u, chunk->nVerts, sizeof(*p), true);
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		// Preparing vertex-bone links
		if (chunk->flags1 & MESH_CHUNK_DESC_0745::FLAG1_BONE_INFO)
		{
			mesh.m_links.resize(chunk->nVerts);

			for (int i = 0; i < chunk->nVerts; ++i)
			{
				MeshUtils::VertexLinks& linksDst = mesh.m_links[i];

				int32* pNumLinks;
				StepData(pNumLinks, pMeshChunkData, 1, bSwapEndianness);
				if (pNumLinks <= 0)
				{
					m_LastError.Format("%s: Number of links for vertex is invalid: %i", __FUNCTION__, pNumLinks);
					return false;
				}

				linksDst.links.resize(*pNumLinks);

				CryLink* pLinksSrc;
				StepData(pLinksSrc, pMeshChunkData, *pNumLinks, bSwapEndianness);
				for (int j = 0; j < *pNumLinks; ++j)
				{
					linksDst.links[j].boneId = pLinksSrc[j].BoneID;
					linksDst.links[j].weight = pLinksSrc[j].Blending;
					linksDst.links[j].offset = pLinksSrc[j].offset * VERTEX_SCALE;
				}

				err = linksDst.Normalize(MeshUtils::VertexLinks::eSort_ByWeight, 0.0f, maxLinkCount);

				if (err)
				{
					m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
					return false;
				}
			}
		}

		// Preparing colors
		if (chunk->flags2 & MESH_CHUNK_DESC_0745::FLAG2_HAS_VERTEX_COLOR)
		{
			CryIRGB* p;
			StepData(p, pMeshChunkData, chunk->nVerts, bSwapEndianness);
			assert(&p->r < &p->b);
			err = mesh.SetColors(&p->r, chunk->nVerts, sizeof(*p));
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		// Preparing alphas
		if (chunk->flags2 & MESH_CHUNK_DESC_0745::FLAG2_HAS_VERTEX_ALPHA)
		{
			uint8* p;
			StepData(p, pMeshChunkData, chunk->nVerts, bSwapEndianness);
			err = mesh.SetAlphas(p, chunk->nVerts, sizeof(*p));
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		// Prevent sharing materials by vertices (this call might create new vertices)
		mesh.SetVertexMaterialIdsFromFaceMaterialIds();

		// Validation
		{
			err = mesh.Validate();
			if (err)
			{
				m_LastError.Format("%s: Failed: %s", __FUNCTION__, err);
				return false;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Compute vertex remapping
		//////////////////////////////////////////////////////////////////////////

		mesh.ComputeVertexRemapping();

		//////////////////////////////////////////////////////////////////////////
		// Creating resulting mesh
		//////////////////////////////////////////////////////////////////////////

		CMesh* const pMesh = new CMesh();

		const int nVertsNew = mesh.m_vertexNewToOld.size();

		// Filling positions, normals, topology IDs, texture coordinates, colors
		{
			pMesh->SetVertexCount(nVertsNew);
			pMesh->ReallocStream(CMesh::TOPOLOGY_IDS, nVertsNew);
			pMesh->ReallocStream(CMesh::TEXCOORDS, nVertsNew);
			if (!mesh.m_colors.empty() || !mesh.m_alphas.empty())
			{
				pMesh->ReallocStream(CMesh::COLORS_0, nVertsNew);
			}

			for (int i = 0; i < nVertsNew; ++i)
			{
				const int origVertex = mesh.m_vertexNewToOld[i];

				pMesh->m_pPositions[i] = mesh.m_positions[origVertex];

				pMesh->m_pNorms[i] = SMeshNormal(mesh.m_normals[origVertex]);

				pMesh->m_pTopologyIds[i] = mesh.m_topologyIds[origVertex];

				if (!mesh.m_texCoords.empty())
				{
					pMesh->m_pTexCoord[i] = SMeshTexCoord(mesh.m_texCoords[origVertex].x, mesh.m_texCoords[origVertex].y);
				}

				if (pMesh->m_pColor0)
				{
					uint8 r = 0xFF;
					uint8 g = 0xFF;
					uint8 b = 0xFF;
					uint8 a = 0xFF;
					if (!mesh.m_colors.empty())
					{
						r = mesh.m_colors[origVertex].r;
						g = mesh.m_colors[origVertex].g;
						b = mesh.m_colors[origVertex].b;
					}
					if (!mesh.m_alphas.empty())
					{
						a = mesh.m_alphas[origVertex];
					}

					pMesh->m_pColor0[i] = SMeshColor(r, g, b, a);
				}
			}
		}

		// Filling vertex-bone links
		{
			m_arrLinksTmp.clear();
			if (!mesh.m_links.empty())
			{
				m_arrLinksTmp.reserve(nVertsNew);
				for (int i = 0; i < nVertsNew; ++i)
				{
					const int origVertex = mesh.m_vertexNewToOld[i];
					m_arrLinksTmp.push_back(mesh.m_links[origVertex]);
				}

				// Remember the mapping table for future re-mapping of uncompiled Morph Target vertices (if any)
				m_vertexOldToNew = mesh.m_vertexOldToNew;
			}
		}

		// Filling faces
		{
			const int nFaces = mesh.GetFaceCount();
			pMesh->SetFaceCount(nFaces);

			DynArray<int>& usedMaterialIds = m_pCGF->GetUsedMaterialIDs();

			for (int i = 0; i < nFaces; ++i)
			{
				const MeshUtils::Face& cf = mesh.m_faces[i];
				const int matId = mesh.m_faceMatIds[i];

				SMeshFace& face = pMesh->m_pFaces[i];

				face.v[0] = mesh.m_vertexOldToNew[cf.vertexIndex[0]];
				face.v[1] = mesh.m_vertexOldToNew[cf.vertexIndex[1]];
				face.v[2] = mesh.m_vertexOldToNew[cf.vertexIndex[2]];

				// Map material ID to index of subset
				if (!MatIdToSubset[matId])
				{
					MatIdToSubset[matId] = 1 + nMaterialsUsed++;
					// Order of material ids in usedMaterialIds correspond to the indices of chunks.
					usedMaterialIds.push_back(matId);
				}
				face.nSubset = MatIdToSubset[matId] - 1;
			}
		}

		// Computing AABB
		pMesh->m_bbox.Reset();
		for (int i = 0; i < nVertsNew; ++i)
		{
			pMesh->m_bbox.Add(pMesh->m_pPositions[i]);
		}

		// Saving results
		pNode->pMesh = pMesh;

		return true;
#endif
	}

	m_LastError.Format("%s: unknown geometry chunk version in %s", __FUNCTION__, m_filename);
	return false;
}

template<class T>
bool CLoaderCGF::LoadStreamChunk(CMesh& mesh, const MESH_CHUNK_DESC_0801& chunk, ECgfStreamType Type, CMesh::EStream MStream)
{
	if (chunk.nStreamChunkID[Type] <= 0)
	{
		return true;
	}

	void* pStreamData;
	int nStreamType;
	int nElemCount;
	int nElemSize;
	bool bSwapEndianness;
	if (!LoadStreamDataChunk(chunk.nStreamChunkID[Type], pStreamData, nStreamType, nElemCount, nElemSize, bSwapEndianness))
	{
		return false;
	}
	if (nStreamType != Type)
	{
		m_LastError.Format("Mesh stream %d has unknown type (%d instead of %d)", (int)MStream, (int)nStreamType, (int)Type);
		return false;
	}
	if (nElemSize != sizeof(T))
	{
		m_LastError.Format("Mesh stream %d has damaged data (elemSize:%u)", (int)MStream, (uint)nElemSize);
		return false;
	}

	SwapEndian((T*)pStreamData, nElemCount, bSwapEndianness);

	void* pMeshElements = 0;
	{
		const bool bSourceAligned = (((UINT_PTR)pStreamData & 0x3) == 0);
		const bool bShare = (m_bUseReadOnlyMesh && bSourceAligned && m_bAllowStreamSharing);

		if (bShare)
		{
			mesh.SetSharedStream(MStream, pStreamData, nElemCount);
		}
		else
		{
			mesh.ReallocStream(MStream, nElemCount);
		}

		int nMeshElemSize = 0;
		mesh.GetStreamInfo(MStream, pMeshElements, nMeshElemSize);
		if (nMeshElemSize != nElemSize || nMeshElemSize != sizeof(T))
		{
			m_LastError.Format("Mesh stream %d has damaged data (elemCount:%u, elemSize:%u)",
			                   (int)MStream, (uint)nElemCount, (uint)nMeshElemSize);

			return false;
		}

		if (!bShare)
		{
			memcpy(pMeshElements, pStreamData, nElemCount * nElemSize);
		}
	}

	return true;
}

template<class TA, class TB>
bool CLoaderCGF::LoadStreamChunk(CMesh& mesh, const MESH_CHUNK_DESC_0801& chunk, ECgfStreamType Type, CMesh::EStream MStreamA, CMesh::EStream MStreamB)
{
	if (chunk.nStreamChunkID[Type] <= 0)
	{
		return true;
	}

	void* pStreamData;
	int nStreamType;
	int nElemCount;
	int nElemSize;
	bool bSwapEndianness;
	if (!LoadStreamDataChunk(chunk.nStreamChunkID[Type], pStreamData, nStreamType, nElemCount, nElemSize, bSwapEndianness))
	{
		return false;
	}
	if (nStreamType != Type)
	{
		m_LastError.Format("Mesh stream %d/%d has unknown type (%d instead of %d)", (int)MStreamA, (int)MStreamB, (int)nStreamType, (int)Type);
		return false;
	}
	if ((nElemSize != sizeof(TA) && nElemSize != sizeof(TB)))
	{
		m_LastError.Format("Mesh stream %d/%d has unsupported element size (%u instead of %u or %u)", (int)MStreamA, (int)MStreamB, (uint)nElemSize, (uint)sizeof(TA), (uint)sizeof(TB));
		return false;
	}

	const bool bUseA = (nElemSize == sizeof(TA));

	if (bUseA)
	{
		SwapEndian((TA*)pStreamData, nElemCount, bSwapEndianness);
	}
	else
	{
		SwapEndian((TB*)pStreamData, nElemCount, bSwapEndianness);
	}

	void* pMeshElements = 0;
	{
		const bool bSourceAligned = (((UINT_PTR)pStreamData & 0x3) == 0);
		const bool bShare = (m_bUseReadOnlyMesh && bSourceAligned && m_bAllowStreamSharing);

		if (bShare)
		{
			mesh.SetSharedStream((bUseA ? MStreamA : MStreamB), pStreamData, nElemCount);
		}
		else
		{
			mesh.ReallocStream((bUseA ? MStreamA : MStreamB), nElemCount);
		}

		int nMeshElemSize = 0;
		mesh.GetStreamInfo((bUseA ? MStreamA : MStreamB), pMeshElements, nMeshElemSize);
		if (nMeshElemSize != nElemSize || nMeshElemSize != (bUseA ? sizeof(TA) : sizeof(TB)))
		{
			m_LastError.Format("Mesh stream %d has damaged data (elemCount:%u, elemSize:%u)",
			                   (int)(bUseA ? MStreamA : MStreamB), (uint)nElemCount, (uint)nMeshElemSize);
			return false;
		}

		if (!bShare)
		{
			memcpy(pMeshElements, pStreamData, nElemCount * nElemSize);
		}
	}

	return true;
}

bool CLoaderCGF::LoadBoneMappingStreamChunk(CMesh& mesh, const MESH_CHUNK_DESC_0801& chunk, const std::vector<std::vector<uint16>>& globalBonesPerSubset)
{
	const ECgfStreamType Type = CGF_STREAM_BONEMAPPING;
	CMesh::EStream MStream = CMesh::BONEMAPPING;

	if (chunk.nStreamChunkID[Type] <= 0)
	{
		return true;
	}

	void* pStreamData;
	int nStreamType;
	int nElemCount;
	int nStreamElemSize;
	bool bSwapEndianness;
	if (!LoadStreamDataChunk(chunk.nStreamChunkID[Type], pStreamData, nStreamType, nElemCount, nStreamElemSize, bSwapEndianness))
	{
		return false;
	}
	if (nStreamType != Type)
	{
		m_LastError.Format("Bone mapping stream %d has unknown type (%d instead of %d)", (int)MStream, (int)nStreamType, (int)Type);
		return false;
	}

	if (nElemCount != mesh.GetVertexCount() && nElemCount != 2 * mesh.GetVertexCount())
	{
		m_LastError.Format("Bone mapping stream %d has wrong # vertices %d (expected %d or %d)", (int)MStream, nElemCount, mesh.GetVertexCount(), 2 * mesh.GetVertexCount());
		return false;
	}

	static_assert(sizeof(mesh.m_pBoneMapping[0]) == sizeof(SMeshBoneMapping_uint16), "Invalid type size!");

	if (nStreamElemSize == sizeof(SMeshBoneMapping_uint8))
	{
		// Obsolete format. We support it just because many existing asset files use it.

		if (globalBonesPerSubset.size() != mesh.m_subsets.size())
		{
			m_LastError.Format("Bad or missing bone remapping tables. Contact an RC programmer.");
			return false;
		}

		SwapEndian((SMeshBoneMapping_uint8*)pStreamData, nElemCount, bSwapEndianness);

		// Converting local (per-preset) uint8 bone indices to global bone uint16 indices

		mesh.ReallocStream(MStream, nElemCount);

		SMeshBoneMapping_uint16* const pMeshElements = mesh.GetStreamPtr<SMeshBoneMapping_uint16>(MStream);
		if (!pMeshElements)
		{
			// Should never happen because we did check it by static_assert above.
			m_LastError.Format("Bone mapping has invalid size. Contact an RC programmer.");
			return false;
		}

		// Filling bone indices with 0xFFFF allows us to perform input data
		// validation (see code with '== 0xFFFF' below)
		memset(pMeshElements, 0xFF, nElemCount * sizeof(pMeshElements[0]));

		const SMeshBoneMapping_uint8* const pSrcBoneMapping = (const SMeshBoneMapping_uint8*)pStreamData;

		const int vertexCount = mesh.GetVertexCount();
		const int indexCount = mesh.GetIndexCount();

		for (int subset = 0; subset < mesh.m_subsets.size(); ++subset)
		{
			SMeshSubset& meshSubset = mesh.m_subsets[subset];

			const std::vector<uint16>& globalBones = globalBonesPerSubset[subset];

			if (meshSubset.nNumIndices == 0)
			{
				continue;
			}

			for (int extra = 0; extra < nElemCount; extra += vertexCount)
			{
				for (int j = meshSubset.nFirstIndexId; j < meshSubset.nFirstIndexId + meshSubset.nNumIndices; ++j)
				{
					const int vIdx = mesh.m_pIndices[j];

					if (vIdx < meshSubset.nFirstVertId || vIdx >= meshSubset.nFirstVertId + meshSubset.nNumVerts)
					{
						m_LastError.Format("Index stream contains invalid vertex index.");
						return false;
					}

					for (int k = 0; k < 4; ++k)
					{
						const uint8 weight = pSrcBoneMapping[vIdx + extra].weights[k];
						if (weight <= 0)
						{
							if (pMeshElements[vIdx + extra].boneIds[k] == 0xFFFF)
							{
								pMeshElements[vIdx + extra].weights[k] = 0;
								pMeshElements[vIdx + extra].boneIds[k] = 0;
							}
							else if (pMeshElements[vIdx + extra].weights[k] != 0 ||
							         pMeshElements[vIdx + extra].boneIds[k] != 0)
							{
								m_LastError.Format("Conflicting vertex-bone references.");
								return false;
							}
							continue;
						}

						const uint8 boneIdx = pSrcBoneMapping[vIdx + extra].boneIds[k];
						if (boneIdx < 0 || (size_t)boneIdx >= globalBones.size())
						{
							m_LastError.Format(
							  "Bad bone mapping found in subset %d, vertex %d: boneIdx %d, # bones in subset %d.",
							  subset, vIdx, boneIdx, (int)globalBones.size());
							return false;
						}

						const uint16 globalBoneIdx = globalBones[boneIdx];

						if (pMeshElements[vIdx + extra].boneIds[k] == 0xFFFF)
						{
							pMeshElements[vIdx + extra].weights[k] = weight;
							pMeshElements[vIdx + extra].boneIds[k] = globalBoneIdx;
						}
						else if (pMeshElements[vIdx + extra].weights[k] != weight ||
						         pMeshElements[vIdx + extra].boneIds[k] != globalBoneIdx)
						{
							m_LastError.Format("Conflicting vertex-bone references.");
							return false;
						}
					}
				}
			}
		}

		int orphanVertexCount = 0;
		for (int i = 0; i < nElemCount; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				const uint boneIdx = pMeshElements[i].boneIds[k];
				if (boneIdx >= 0xFFFF)
				{
					++orphanVertexCount;
					pMeshElements[i].weights[k] = 0;
					pMeshElements[i].boneIds[k] = 0;
				}
			}
		}

		if (orphanVertexCount)
		{
			orphanVertexCount /= 4 * (nElemCount / vertexCount);
			CryWarning(
			  VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING,
			  "Found %d orphan vertices", orphanVertexCount);
		}
	}
	else if (nStreamElemSize == sizeof(SMeshBoneMapping_uint16))
	{
		SwapEndian((SMeshBoneMapping_uint16*)pStreamData, nElemCount, bSwapEndianness);

		const bool bSourceAligned = (((UINT_PTR)pStreamData & 0x3) == 0);
		const bool bShare = (m_bUseReadOnlyMesh && bSourceAligned && m_bAllowStreamSharing);

		if (bShare)
		{
			mesh.SetSharedStream(MStream, pStreamData, nElemCount);
		}
		else
		{
			mesh.ReallocStream(MStream, nElemCount);
		}

		SMeshBoneMapping_uint16* const pMeshElements = mesh.GetStreamPtr<SMeshBoneMapping_uint16>(MStream);
		if (!pMeshElements)
		{
			// Should never happen because we did check it by static_assert above.
			m_LastError.Format("Bone mapping has invalid size. Contact an RC programmer.");
			return false;
		}

		if (!bShare)
		{
			memcpy(pMeshElements, pStreamData, nElemCount * sizeof(pMeshElements[0]));
		}
	}
	else
	{
		m_LastError.Format("Bone mapping stream %d has damaged data (elemSize:%u)", (int)MStream, (uint)nStreamElemSize);
		return false;
	}

	// Validation
	{
		SMeshBoneMapping_uint16* const pMeshElements = mesh.GetStreamPtr<SMeshBoneMapping_uint16>(MStream);

		for (int i = 0; i < nElemCount; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				const uint boneIdx = pMeshElements[i].boneIds[k];
				if (boneIdx >= MAX_NUMBER_OF_BONES)
				{
					m_LastError.Format("Bad bone index detected: %u.", (uint)boneIdx);
					return false;
				}
			}
		}
	}

	return true;
}

bool CLoaderCGF::LoadIndexStreamChunk(CMesh& mesh, const MESH_CHUNK_DESC_0801& chunk)
{
	const ECgfStreamType Type = CGF_STREAM_INDICES;
	CMesh::EStream MStream = CMesh::INDICES;

	if (chunk.nStreamChunkID[Type] <= 0)
	{
		return true;
	}

	void* pStreamData;
	int nStreamType;
	int nElemCount;
	int nStreamElemSize;
	bool bSwapEndianness;
	if (!LoadStreamDataChunk(chunk.nStreamChunkID[Type], pStreamData, nStreamType, nElemCount, nStreamElemSize, bSwapEndianness))
	{
		return false;
	}
	if (nStreamType != Type)
	{
		m_LastError.Format("Index stream %d has unknown type (%d instead of %d)", (int)MStream, (int)nStreamType, (int)Type);
		return false;
	}

	if (nStreamElemSize == sizeof(uint16))
	{
		SwapEndian((uint16*)pStreamData, nElemCount, bSwapEndianness);
	}
	else if (nStreamElemSize == sizeof(uint32))
	{
		SwapEndian((uint32*)pStreamData, nElemCount, bSwapEndianness);
	}
	else
	{
		m_LastError.Format("Index stream %d has damaged data (elemSize:%u)", (int)MStream, (uint)nStreamElemSize);
		return false;
	}

	const bool bSourceAligned = (((UINT_PTR)pStreamData & 0x3) == 0);
	const bool bShare = (m_bUseReadOnlyMesh && bSourceAligned && m_bAllowStreamSharing);

	static_assert(sizeof(mesh.m_pIndices[0]) == sizeof(vtx_idx), "Invalid type size!");
	static_assert(sizeof(vtx_idx) == 2 || sizeof(vtx_idx) == 4, "Invalid type size!");

	if (nStreamElemSize == sizeof(vtx_idx))
	{
		if (bShare)
		{
			mesh.SetSharedStream(MStream, pStreamData, nElemCount);
		}
		else
		{
			mesh.ReallocStream(MStream, nElemCount);
		}

		void* pMeshIndices = 0;
		int nMeshIndexSize = 0;
		mesh.GetStreamInfo(MStream, pMeshIndices, nMeshIndexSize);
		if (nMeshIndexSize != sizeof(vtx_idx))
		{
			// Should never happen - we already did static_assert(sizeof(mesh.m_pIndices[0]) == sizeof(vtx_idx))
			m_LastError.Format("Vertex index has invalid size. Contact an RC programmer.");
			return false;
		}

		if (!bShare)
		{
			memcpy(pMeshIndices, pStreamData, nElemCount * nStreamElemSize);
		}
	}
	else
	{
		// Converting index format uint16 <--> uint32

#if !defined(RESOURCE_COMPILER)
	#if 0 // Sokov: commented out the warning because it was confusing users. We will uncomment it after implementing converting files to proper index format during asset exporting.
		CryWarning(
		  VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING,
		  "%s: This asset contains vertex indices in incorrect format (%u-bit instead of %u-bit). "
		  "Probably it didn't go through build process yet. "
		  "Disregard this message if this asset was just exported.",
		  m_filename, (uint)nStreamElemSize * 8, (uint)sizeof(vtx_idx) * 8);
	#endif
#endif

		mesh.ReallocStream(MStream, nElemCount);

		void* pMeshIndices = 0;
		int nMeshIndexSizeCheck = 0;
		mesh.GetStreamInfo(MStream, pMeshIndices, nMeshIndexSizeCheck);
		if (nMeshIndexSizeCheck != sizeof(vtx_idx))
		{
			// Should never happen - we already did static_assert(sizeof(mesh.m_pIndices[0]) == sizeof(vtx_idx))
			m_LastError.Format("Vertex index has invalid size. Contact an RC programmer.");
			return false;
		}

		if (nStreamElemSize == sizeof(uint16))
		{
			const uint16* const pSrc = (const uint16*)pStreamData;
			for (int i = 0; i < nElemCount; ++i)
			{
				((uint32*)pMeshIndices)[i] = (uint32)pSrc[i];
			}
		}
		else
		{
			const uint32* const pSrc = (const uint32*)pStreamData;
			for (int i = 0; i < nElemCount; ++i)
			{
				const uint32 idx = pSrc[i];
				if (idx >= 0xffff)   // ">=": index 0xffff is reserved (the engine uses it to mark invalid indices etc)
				{
					m_LastError.Format("Cannot convert index stream %d from %u-bit to %u-bit format because it contains index %u", (int)MStream, (uint)nStreamElemSize * 8, (uint)sizeof(vtx_idx) * 8, (uint)pSrc[i]);
					return false;
				}
				((uint16*)pMeshIndices)[i] = (uint16)idx;
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadCompiledMeshChunk(CNodeCGF* pNode, IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!pChunkDesc || pChunkDesc->chunkType != ChunkType_Mesh)
	{
		m_LastError.Format("Corrupted compiled mesh chunk");
		return false;
	}

	if (pChunkDesc->chunkVersion != MESH_CHUNK_DESC_0801::VERSION &&
	    pChunkDesc->chunkVersion != MESH_CHUNK_DESC_0801::COMPATIBLE_OLD_VERSION)
	{
		m_LastError.Format("Unknown version of compiled mesh chunk");
		return false;
	}

	MESH_CHUNK_DESC_0801& chunk = *(MESH_CHUNK_DESC_0801*)pChunkDesc->data;

	if (pChunkDesc->bSwapEndian)
	{
		SwapEndian(chunk, true);
		pChunkDesc->bSwapEndian = false;
	}

	Vec3 bboxMin, bboxMax;
	memcpy(&bboxMin, &chunk.bboxMin, sizeof(bboxMin));
	memcpy(&bboxMax, &chunk.bboxMax, sizeof(bboxMax));

	pNode->meshInfo.nVerts = chunk.nVerts;
	pNode->meshInfo.nIndices = chunk.nIndices;
	pNode->meshInfo.nSubsets = chunk.nSubsets;
	pNode->meshInfo.bboxMin = bboxMin;
	pNode->meshInfo.bboxMax = bboxMax;
	pNode->meshInfo.fGeometricMean = chunk.geometricMeanFaceArea;
	pNode->nPhysicalizeFlags = chunk.nFlags2;

	for (int nPhysGeomType = 0; nPhysGeomType < 4; nPhysGeomType++)
	{
		if (chunk.nPhysicsDataChunkId[nPhysGeomType] > 0)
		{
			LoadPhysicsDataChunk(pNode, nPhysGeomType, chunk.nPhysicsDataChunkId[nPhysGeomType]);
		}
	}

	if (chunk.nFlags & MESH_CHUNK_DESC_0801::MESH_IS_EMPTY)
	{
		// This is an empty mesh.
		if (pNode->type == CNodeCGF::NODE_MESH)
		{
			// Do not create CMesh for it.
			m_pCGF->GetExportInfo()->bNoMesh = true;
		}
		return true;
	}

	std::unique_ptr<CMesh> pMesh(new CMesh());
	CMesh& mesh = *(pMesh.get());

	if (!m_bUseReadOnlyMesh)
	{
		mesh.SetVertexCount(chunk.nVerts);
		mesh.SetIndexCount(chunk.nIndices);

		if (chunk.nStreamChunkID[CGF_STREAM_TEXCOORDS] > 0)
		{
			mesh.ReallocStream(CMesh::TEXCOORDS, chunk.nVerts);
		}
	}

	mesh.m_bbox = AABB(bboxMin, bboxMax);

	std::vector<std::vector<uint16>> globalBonesPerSubset;

	if (chunk.nSubsets > 0 && chunk.nSubsetsChunkId > 0)
	{
		IChunkFile::ChunkDesc* pSubsetChunkDesc = m_pChunkFile->FindChunkById(chunk.nSubsetsChunkId);
		if (!pSubsetChunkDesc || pSubsetChunkDesc->chunkType != ChunkType_MeshSubsets)
		{
			m_LastError.Format("MeshSubsets Chunk not found in CGF file %s", m_filename);
			return false;
		}
		if (!LoadMeshSubsetsChunk(mesh, pSubsetChunkDesc, globalBonesPerSubset))
		{
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Read streams
	//////////////////////////////////////////////////////////////////////////

	static_assert(sizeof(Vec3f16) == 8, "Invalid type size!");

	bool ok = true;

	// Read position stream.
	ok = ok && LoadStreamChunk<Vec3, Vec3f16>(mesh, chunk, CGF_STREAM_POSITIONS, CMesh::POSITIONS, CMesh::POSITIONSF16);
	if (mesh.m_streamSize[CMesh::POSITIONSF16] > 0 && !m_bUseReadOnlyMesh)
	{
		const int count = mesh.m_streamSize[CMesh::POSITIONSF16];
		mesh.ReallocStream(CMesh::POSITIONS, count);

		void* pSrc = 0;
		int nSrcElementSize = 0;
		mesh.GetStreamInfo(CMesh::POSITIONSF16, pSrc, nSrcElementSize);
		assert(pSrc);

		void* pDst = 0;
		int nDstElementSize = 0;
		mesh.GetStreamInfo(CMesh::POSITIONS, pDst, nDstElementSize);

		if (pDst)
		{
			const Vec3f16* const pS = (const Vec3f16*)pSrc;
			Vec3* const pD = (Vec3*)pDst;
			for (int i = 0; i < count; ++i)
			{
				pD[i] = pS[i].ToVec3();
			}
			mesh.ReallocStream(CMesh::POSITIONSF16, 0);
		}
	}

	// Read normals stream.
	ok = ok && LoadStreamChunk<Vec3>(mesh, chunk, CGF_STREAM_NORMALS, CMesh::NORMALS);

	// Read Texture coordinates stream.
	ok = ok && LoadStreamChunk<SMeshTexCoord>(mesh, chunk, CGF_STREAM_TEXCOORDS, CMesh::TEXCOORDS);

	// Read indices stream.
	ok = ok && LoadIndexStreamChunk(mesh, chunk);

	// Read colors stream.
	ok = ok && LoadStreamChunk<SMeshColor>(mesh, chunk, CGF_STREAM_COLORS, CMesh::COLORS_0);
	ok = ok && LoadStreamChunk<SMeshColor>(mesh, chunk, CGF_STREAM_COLORS2, CMesh::COLORS_1);

	// Read Vertex Mapping.
	ok = ok && LoadStreamChunk<int>(mesh, chunk, CGF_STREAM_VERT_MATS, CMesh::VERT_MATS);

	// Read Tangent Streams.
	ok = ok && LoadStreamChunk<SMeshTangents>(mesh, chunk, CGF_STREAM_TANGENTS, CMesh::TANGENTS);
	ok = ok && LoadStreamChunk<SMeshQTangents>(mesh, chunk, CGF_STREAM_QTANGENTS, CMesh::QTANGENTS);

	// Read interleaved stream.
	ok = ok && LoadStreamChunk<SVF_P3S_C4B_T2S>(mesh, chunk, CGF_STREAM_P3S_C4B_T2S, CMesh::P3S_C4B_T2S);

	ok = ok && LoadBoneMappingStreamChunk(mesh, chunk, globalBonesPerSubset);

	if (!ok)
	{
		return false;
	}

	if (chunk.nFlags & MESH_CHUNK_DESC_0801::HAS_EXTRA_WEIGHTS)
	{
		// The memory being used by the extraWeight array has been allocated in the LoadBoneMappingStreamChunk.
		mesh.m_pExtraBoneMapping = &mesh.m_pBoneMapping[mesh.GetVertexCount()];
	}

	if (chunk.nFlags & MESH_CHUNK_DESC_0801::HAS_TEX_MAPPING_DENSITY)
	{
		mesh.m_texMappingDensity = chunk.texMappingDensity;
	}
	else
	{
		mesh.RecomputeTexMappingDensity();
	}

	if (chunk.nFlags & MESH_CHUNK_DESC_0801::HAS_FACE_AREA)
	{
		mesh.m_geometricMeanFaceArea = chunk.geometricMeanFaceArea;
	}

	if (mesh.m_geometricMeanFaceArea <= 0.0f)
	{
		mesh.RecomputeGeometricMeanFaceArea();
	}

	assert(mesh.m_geometricMeanFaceArea > 0.0f || mesh.GetFaceCount() == 0);

	pNode->pMesh = pMesh.release();

	if (chunk.nStreamChunkID[CGF_STREAM_SKINDATA] > 0)
	{
		int nStreamType, nStreamCount, nElemSize;
		void* pStreamData;
		bool bSwapEndianness;
		if (!LoadStreamDataChunk(chunk.nStreamChunkID[CGF_STREAM_SKINDATA], pStreamData, nStreamType, nStreamCount, nElemSize, bSwapEndianness))
		{
			return false;
		}
		SwapEndian((CrySkinVtx*)pStreamData, nStreamCount, bSwapEndianness);
		memcpy(pNode->pSkinInfo = new CrySkinVtx[nStreamCount], pStreamData, nStreamCount * nElemSize);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadMeshSubsetsChunk(CMesh& mesh, IChunkFile::ChunkDesc* pChunkDesc, std::vector<std::vector<uint16>>& globalBonesPerSubset)
{
	FUNCTION_PROFILER_3DENGINE;

	globalBonesPerSubset.clear();

	if (pChunkDesc->chunkType != ChunkType_MeshSubsets)
	{
		m_LastError.Format("Unknown type in mesh subset chunk");
		return false;
	}

	if (pChunkDesc->chunkVersion != MESH_SUBSETS_CHUNK_DESC_0800::VERSION)
	{
		m_LastError.Format("Unknown version of mesh subset chunk");
		return false;
	}

	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	pChunkDesc->bSwapEndian = false;

	uint8* pCurDataLoc = (uint8*)pChunkDesc->data;

	MESH_SUBSETS_CHUNK_DESC_0800& chunk = *StepData<MESH_SUBSETS_CHUNK_DESC_0800>(pCurDataLoc, bSwapEndianness);

	const bool cbBoneIDs = (chunk.nFlags & MESH_SUBSETS_CHUNK_DESC_0800::BONEINDICES) != 0;
	const bool cbSubsetTexelDensities = (chunk.nFlags & MESH_SUBSETS_CHUNK_DESC_0800::HAS_SUBSET_TEXEL_DENSITY) != 0;

	for (int i = 0; i < chunk.nCount; i++)
	{
		MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset& meshSubset = *StepData<MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset>(pCurDataLoc, bSwapEndianness);

		SMeshSubset subset;

		subset.nFirstIndexId = meshSubset.nFirstIndexId;
		subset.nNumIndices = meshSubset.nNumIndices;
		subset.nFirstVertId = meshSubset.nFirstVertId;
		subset.nNumVerts = meshSubset.nNumVerts;
		subset.nMatID = meshSubset.nMatID;
		subset.fRadius = meshSubset.fRadius;
		subset.vCenter = meshSubset.vCenter;
		mesh.m_subsets.push_back(subset);
	}

	//------------------------------------------------------------------
	if (cbBoneIDs)
	{
		globalBonesPerSubset.resize(chunk.nCount);

		for (int i = 0; i < chunk.nCount; i++)
		{
			MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs& meshSubset = *StepData<MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs>(pCurDataLoc, bSwapEndianness);

			globalBonesPerSubset[i].resize(meshSubset.numBoneIDs);

			for (uint32 b = 0; b < meshSubset.numBoneIDs; ++b)
			{
				globalBonesPerSubset[i][b] = meshSubset.arrBoneIDs[b];
			}
		}
	}

	if (cbSubsetTexelDensities)
	{
		for (int i = 0; i < chunk.nCount; i++)
		{
			MESH_SUBSETS_CHUNK_DESC_0800::MeshSubsetTexelDensity& meshSubset = *StepData<MESH_SUBSETS_CHUNK_DESC_0800::MeshSubsetTexelDensity>(pCurDataLoc, bSwapEndianness);
			mesh.m_subsets[i].fTexelDensity = meshSubset.texelDensity;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadStreamDataChunk(int nChunkId, void*& pStreamData, int& nStreamType, int& nCount, int& nElemSize, bool& bSwapEndianness)
{
	IChunkFile::ChunkDesc* pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
	{
		m_LastError.Format("Failed to find chunk with id %d", nChunkId);
		return false;
	}

	if (pChunkDesc->chunkType != ChunkType_DataStream)
	{
		m_LastError.Format("Unknown type of stream data chunk");
		return false;
	}

	if (pChunkDesc->chunkVersion != STREAM_DATA_CHUNK_DESC_0800::VERSION)
	{
		m_LastError.Format("Unknown version of stream data chunk");
		return false;
	}

	FUNCTION_PROFILER_3DENGINE;

	STREAM_DATA_CHUNK_DESC_0800& chunk = *(STREAM_DATA_CHUNK_DESC_0800*)pChunkDesc->data;
	bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(chunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	nStreamType = chunk.nStreamType;
	nCount = chunk.nCount;
	nElemSize = chunk.nElementSize;
	pStreamData = (char*)pChunkDesc->data + sizeof(chunk);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadPhysicsDataChunk(CNodeCGF* pNode, int nPhysGeomType, int nChunkId)
{
	IChunkFile::ChunkDesc* const pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
	{
		return false;
	}

	if (pChunkDesc->chunkType != ChunkType_MeshPhysicsData)
	{
		return false;
	}

	if (pChunkDesc->chunkVersion != MESH_PHYSICS_DATA_CHUNK_DESC_0800::VERSION)
	{
		return false;
	}

	FUNCTION_PROFILER_3DENGINE;

	MESH_PHYSICS_DATA_CHUNK_DESC_0800& chunk = *(MESH_PHYSICS_DATA_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(chunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	assert(nPhysGeomType >= 0 && nPhysGeomType < 4);

	pNode->physicalGeomData[nPhysGeomType].resize(chunk.nDataSize);
	void* const pDst = &(pNode->physicalGeomData[nPhysGeomType][0]);
	const void* const pSrc = (char*)pChunkDesc->data + sizeof(chunk);
	memcpy(pDst, pSrc, chunk.nDataSize);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadFoliageInfoChunk(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (pChunkDesc->chunkVersion != FOLIAGE_INFO_CHUNK_DESC::VERSION)
	{
		m_LastError.Format("Unknown version of FoliageInfo chunk");
		return false;
	}

	FOLIAGE_INFO_CHUNK_DESC& chunk = *(FOLIAGE_INFO_CHUNK_DESC*)pChunkDesc->data;
	const bool bSwapEndianness = pChunkDesc->bSwapEndian;
	SwapEndian(chunk, bSwapEndianness);
	pChunkDesc->bSwapEndian = false;

	SFoliageInfoCGF& fi = *m_pCGF->GetFoliageInfo();
	if (fi.nSpines = chunk.nSpines)
	{
		fi.nSkinnedVtx = chunk.nSkinnedVtx;

		FOLIAGE_SPINE_SUB_CHUNK* const pSpineSrc = (FOLIAGE_SPINE_SUB_CHUNK*)(&chunk + 1);
		Vec3* const pSpineVtxSrc = (Vec3*)(&pSpineSrc[chunk.nSpines]);
		Vec4* const pSpineSegDimSrc = (Vec4*)(&pSpineVtxSrc[chunk.nSpineVtx]);
		SMeshBoneMapping_uint8* const pBoneMappingSrc = (SMeshBoneMapping_uint8*)(&pSpineSegDimSrc[chunk.nSpineVtx]);
		uint16* const pBoneIdsSrc = (uint16*)(&pBoneMappingSrc[chunk.nSkinnedVtx]);

		if (bSwapEndianness)
		{
			SwapEndian(pSpineSrc, chunk.nSpines, true);
			SwapEndian(pSpineVtxSrc, chunk.nSpineVtx, true);
			SwapEndian(pSpineSegDimSrc, chunk.nSpineVtx, true);
			SwapEndian(pBoneMappingSrc, chunk.nSkinnedVtx, true);
			SwapEndian(pBoneIdsSrc, chunk.nBoneIds, true);
		}

		Vec3* const pSpineVtx = new Vec3[chunk.nSpineVtx];
		Vec4* const pSpineSegDim = new Vec4[chunk.nSpineVtx];
		fi.pBoneMapping = new SMeshBoneMapping_uint8[chunk.nSkinnedVtx];
		fi.chunkBoneIds.resize(chunk.nBoneIds);
		fi.pSpines = new SSpineRC[chunk.nSpines];

		memcpy(pSpineVtx, pSpineVtxSrc, sizeof(pSpineVtx[0]) * chunk.nSpineVtx);
		memcpy(pSpineSegDim, pSpineSegDimSrc, sizeof(pSpineSegDim[0]) * chunk.nSpineVtx);
		memcpy(fi.pBoneMapping, pBoneMappingSrc, sizeof(pBoneMappingSrc[0]) * chunk.nSkinnedVtx);
		static_assert(sizeof(fi.chunkBoneIds[0]) == sizeof(pBoneIdsSrc[0]), "Invalid type size!");
		memcpy(&fi.chunkBoneIds[0], pBoneIdsSrc, sizeof(fi.chunkBoneIds[0]) * chunk.nBoneIds);

		int i, j;
		for (i = j = 0; i < chunk.nSpines; j += fi.pSpines[i++].nVtx)
		{
			fi.pSpines[i].nVtx = pSpineSrc[i].nVtx;
			fi.pSpines[i].len = pSpineSrc[i].len;
			fi.pSpines[i].navg = pSpineSrc[i].navg;
			fi.pSpines[i].iAttachSpine = pSpineSrc[i].iAttachSpine - 1;
			fi.pSpines[i].iAttachSeg = pSpineSrc[i].iAttachSeg - 1;
			fi.pSpines[i].pVtx = pSpineVtx + j;
			fi.pSpines[i].pSegDim = pSpineSegDim + j;
		}
	}

	return true;
}

bool CLoaderCGF::LoadVClothChunk(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (pChunkDesc->chunkVersion != VCLOTH_CHUNK::VERSION)
	{
		m_LastError.Format("Unknown version of FoliageInfo chunk");
		return false;
	}

	SVClothInfoCGF* const pVClothInfo = m_pCGF->GetVClothInfo();

	const char* pCursor = (const char*)pChunkDesc->data;

	VCLOTH_CHUNK* const pChunk = (VCLOTH_CHUNK*)pCursor;
	const bool bSwapEndian = pChunkDesc->bSwapEndian;
	SwapEndian(*pChunk, bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	// Read vertices.
	{
		pCursor += sizeof(VCLOTH_CHUNK);
		SVClothChunkVertex* const pChunkVertices = (SVClothChunkVertex*)pCursor;
		SwapEndian(pChunkVertices, pChunk->vertexCount, bSwapEndian);

		pVClothInfo->m_vertices.resize(pChunk->vertexCount);
		for (uint32 vid = 0; vid < pChunk->vertexCount; ++vid)
		{
			const SVClothChunkVertex& chunkVertex = pChunkVertices[vid];
			SVClothVertex& vertex = pVClothInfo->m_vertices[vid];

			vertex.attributes = chunkVertex.attributes;
		}
	}

	// Read triangles.
	{
		pCursor += sizeof(SVClothChunkVertex) * pChunk->vertexCount;
		SVClothBendTriangle* const pTriangles = (SVClothBendTriangle*)pCursor;
		SwapEndian(pTriangles, pChunk->bendTriangleCount, bSwapEndian);

		pVClothInfo->m_triangles.assign(pTriangles, pTriangles + pChunk->bendTriangleCount);
	}

	// Read triangle pairs.
	{
		pCursor += sizeof(SVClothBendTriangle) * pChunk->bendTriangleCount;
		SVClothBendTrianglePair* const pTrianglePairs = (SVClothBendTrianglePair*)pCursor;
		SwapEndian(pTrianglePairs, pChunk->bendTrianglePairCount, bSwapEndian);

		pVClothInfo->m_trianglePairs.assign(pTrianglePairs, pTrianglePairs + pChunk->bendTrianglePairCount);
	}

	// Read nndcNotAttachedOrderedIdx.
	{
		pCursor += sizeof(SVClothBendTrianglePair) * pChunk->bendTrianglePairCount;
		SVClothNndcNotAttachedOrderedIdx* const pNndcNotAttachedOrderedIdx = (SVClothNndcNotAttachedOrderedIdx*)pCursor;
		SwapEndian(pNndcNotAttachedOrderedIdx, pChunk->nndcNotAttachedOrderedIdxCount, bSwapEndian);

		pVClothInfo->m_nndcNotAttachedOrderedIdx.assign(pNndcNotAttachedOrderedIdx, pNndcNotAttachedOrderedIdx + pChunk->nndcNotAttachedOrderedIdxCount);
	}

	// Read links.
	{
		pCursor += sizeof(SVClothNndcNotAttachedOrderedIdx) * pChunk->nndcNotAttachedOrderedIdxCount;

		for (int lid = 0; lid < eVClothLink_COUNT; ++lid)
		{
			SVClothLink* const pLinks = (SVClothLink*)pCursor;
			SwapEndian(pLinks, pChunk->linkCount[lid], bSwapEndian);

			pVClothInfo->m_links[lid].assign(pLinks, pLinks + pChunk->linkCount[lid]);

			pCursor += sizeof(SVClothLink) * pChunk->linkCount[lid];
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CMaterialCGF* CLoaderCGF::LoadMaterialFromChunk(int nChunkId)
{
	for (int i = 0; i < m_pCGF->GetMaterialCount(); i++)
	{
		if (m_pCGF->GetMaterial(i)->nChunkId == nChunkId)
		{
			return m_pCGF->GetMaterial(i);
		}
	}

	IChunkFile::ChunkDesc* const pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
	{
		m_LastError.Format("Can't find material chunk with id %d in file %s", nChunkId, m_filename);
		return 0;
	}

	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkType != ChunkType_MtlName)
	{
		m_LastError.Format(
		  "Invalid chunk type (0x%08x instead of expected material chunk type 0x%08x) in chunk %d in file %s",
		  (int)pChunkDesc->chunkType, (int)ChunkType_MtlName, nChunkId, m_filename);
		return 0;
	}

	return LoadMaterialNameChunk(pChunkDesc);
}

//////////////////////////////////////////////////////////////////////////
static const char* GetNextAsciizString(const char*& pCurrent, const char* const pEnd)
{
	const char* const result = pCurrent;

	while (pCurrent < pEnd && *pCurrent >= ' ')
	{
		++pCurrent;
	}

	if (pCurrent >= pEnd || *pCurrent != 0)
	{
		// If we found that the data are damaged (*pCurrent != 0) then it's
		// better to stop using the data. We move pCurrent to the end
		// for that - it guarantees that all future calls return "".
		pCurrent = pEnd;
		return "";
	}

	++pCurrent;

	return result;
}

//////////////////////////////////////////////////////////////////////////
CMaterialCGF* CLoaderCGF::LoadMaterialNameChunk(IChunkFile::ChunkDesc* pChunkDesc)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pChunkDesc->chunkVersion == MTL_NAME_CHUNK_DESC_0802::VERSION)
	{
		MTL_NAME_CHUNK_DESC_0802 chunk;

		const bool bSwapEndianness = pChunkDesc->bSwapEndian;

		memcpy(&chunk, pChunkDesc->data, sizeof(chunk));
		SwapEndian(chunk, bSwapEndianness);

		CMaterialCGF* pMtlCGF = Construct<CMaterialCGF>(InplaceFactory(m_pDestructFnc), m_pAllocFnc);
		pMtlCGF->nChunkId = pChunkDesc->chunkId;
		m_pCGF->AddMaterial(pMtlCGF);

		for (size_t i = 0; i < sizeof(chunk.name); ++i)
		{
			if (chunk.name[i] == '\\')
			{
				chunk.name[i] = '/';
			}
		}
		cry_strcpy(pMtlCGF->name, chunk.name);

		const int32 slotCount = (chunk.nSubMaterials <= 0) ? 1 : chunk.nSubMaterials;
		const int nPhysicalizeTypeMaxCount = (pChunkDesc->size - sizeof(MTL_NAME_CHUNK_DESC_0802)) / sizeof(int32);

		if (slotCount > nPhysicalizeTypeMaxCount)
		{
			m_LastError.Format("Corrupted MTL_NAME_CHUNK_DESC_0802 chunk");
			return NULL;
		}

		const int32* const pPhysicalizeTypes = (const int32*)((const char*)pChunkDesc->data + sizeof(MTL_NAME_CHUNK_DESC_0802));

		if (chunk.nSubMaterials <= 0)
		{
			int nPhysicalizeType = pPhysicalizeTypes[0];
			SwapEndian(nPhysicalizeType, bSwapEndianness);
			pMtlCGF->nPhysicalizeType = nPhysicalizeType;
		}
		else if (chunk.nSubMaterials <= MAX_SUB_MATERIALS)
		{
			const char* pNames = (const char*)&pPhysicalizeTypes[slotCount];
			const char* const pNamesEnd = (const char*)pChunkDesc->data + pChunkDesc->size;

			for (int i = 0; i < chunk.nSubMaterials; ++i)
			{
				CMaterialCGF* const pSubMaterial = new CMaterialCGF;

				cry_strcpy(pSubMaterial->name, GetNextAsciizString(pNames, pNamesEnd));

				int nPhysicalizeType = pPhysicalizeTypes[i];
				SwapEndian(nPhysicalizeType, bSwapEndianness);

				if (nPhysicalizeType != PHYS_GEOM_TYPE_NONE &&
				    (nPhysicalizeType < PHYS_GEOM_TYPE_DEFAULT || nPhysicalizeType > PHYS_GEOM_TYPE_DEFAULT_PROXY))
				{
					m_LastError.Format("Invalid physicalize type in material name chunk (0x%08x) in %s, %s", nPhysicalizeType, pMtlCGF->name, m_filename);
					return NULL;
				}

				pSubMaterial->nPhysicalizeType = nPhysicalizeType;
				pMtlCGF->subMaterials.push_back(pSubMaterial);
				m_pCGF->AddMaterial(pSubMaterial);
			}
		}
		else
		{
			m_LastError.Format("Material name chunk: too many submaterials (0x%08x) in %s, %s", chunk.nSubMaterials, pMtlCGF->name, m_filename);
			return NULL;
		}

		return pMtlCGF;
	}

	if (pChunkDesc->chunkVersion == MTL_NAME_CHUNK_DESC_0800::VERSION)
	{
		if (pChunkDesc->size > sizeof(MTL_NAME_CHUNK_DESC_0800))
		{
			m_LastError.Format("Illegal material name chunk size %s (%d should be %d)", m_filename, (int)pChunkDesc->size, (int)sizeof(MTL_NAME_CHUNK_DESC_0800));
			return NULL;
		}

		MTL_NAME_CHUNK_DESC_0800& chunk = *(MTL_NAME_CHUNK_DESC_0800*)pChunkDesc->data;
		SwapEndian(chunk, pChunkDesc->bSwapEndian);
		pChunkDesc->bSwapEndian = false;

		for (size_t i = 0; i < sizeof(chunk.name); ++i)
		{
			if (chunk.name[i] == '\\')
			{
				chunk.name[i] = '/';
			}
		}

		CMaterialCGF* pMtlCGF = Construct<CMaterialCGF>(InplaceFactory(m_pDestructFnc), m_pAllocFnc);
		pMtlCGF->nChunkId = pChunkDesc->chunkId;
		m_pCGF->AddMaterial(pMtlCGF);
		cry_strcpy(pMtlCGF->name, chunk.name);

		// hack for old broken assets
		if (size_t(chunk.nSubMaterials) > 0xffff || chunk.nPhysicalizeType > 0xffff)
		{
			Warning("Fixing material name chunk with wrong endianness: %s, %s", pMtlCGF->name, m_filename);
			Warning(" nSubMaterials=0x%08x, nPhysicalizeType=0x%08x, nFlags=0x%08x",
			        int(chunk.nSubMaterials), int(chunk.nPhysicalizeType), int(chunk.nFlags));
			SwapEndian(chunk, true);
		}

		pMtlCGF->nPhysicalizeType = chunk.nPhysicalizeType;
		if ((unsigned int)pMtlCGF->nPhysicalizeType <= (PHYS_GEOM_TYPE_DEFAULT_PROXY - PHYS_GEOM_TYPE_DEFAULT))
		{
			pMtlCGF->nPhysicalizeType += PHYS_GEOM_TYPE_DEFAULT;   // fixup if was exported with PHYS_GEOM_TYPE_DEFAULT==0
		}

		if (pMtlCGF->nPhysicalizeType != PHYS_GEOM_TYPE_NONE &&
		    (pMtlCGF->nPhysicalizeType < PHYS_GEOM_TYPE_DEFAULT ||
		     pMtlCGF->nPhysicalizeType > PHYS_GEOM_TYPE_DEFAULT_PROXY))
		{
			m_LastError.Format("Invalid physicalize type in material name chunk (0x%08x) in %s, %s", pMtlCGF->nPhysicalizeType, pMtlCGF->name, m_filename);
			return NULL;
		}

		if (size_t(chunk.nSubMaterials) <= MTL_NAME_CHUNK_DESC_0800_MAX_SUB_MATERIALS)
		{
			pMtlCGF->subMaterials.resize(chunk.nSubMaterials);
			for (int i = 0; i < chunk.nSubMaterials; i++)
			{
				if (chunk.nSubMatChunkId[i] > 0)
				{
					CMaterialCGF* const pMtl = LoadMaterialFromChunk(chunk.nSubMatChunkId[i]);
					if (!pMtl)
					{
						return NULL;
					}
					if (!pMtl->subMaterials.empty())
					{
						m_LastError.Format("Multi-material used as sub-material from file %s", m_filename);
						return NULL;
					}
					pMtlCGF->subMaterials[i] = pMtl;
				}
			}
		}
		else
		{
			m_LastError.Format("Material name chunk: too many submaterials (0x%08x) in %s, %s", chunk.nSubMaterials, pMtlCGF->name, m_filename);
			return NULL;
		}
		return pMtlCGF;
	}

	m_LastError.Format("Illegal material name chunk %s", m_filename);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CLoaderCGF::Warning(const char* szFormat, ...)
{
	if (m_pListener)
	{
		char szBuffer[1024];
		va_list args;
		va_start(args, szFormat);
		cry_vsprintf(szBuffer, szFormat, args);
		m_pListener->Warning(szBuffer);
		va_end(args);
	}
}
