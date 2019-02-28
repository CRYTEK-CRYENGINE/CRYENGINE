// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "LoaderCHR.h"

#include <Cry3DEngine/I3DEngine.h>
#include "ModelMesh.h"
#include "ModelSkin.h"
#include "CharacterManager.h"
#include "AttachmentSkin.h"
#include "AttachmentVCloth.h"
#include "AttachmentManager.h"

#ifdef EDITOR_PCDEBUGCODE
typedef std::map<std::pair<string, int>, bool> TValidatedSkeletonMap;
static TValidatedSkeletonMap s_validatedSkeletons;
#endif

namespace CryCHRLoader_LoadNewSKIN_Helpers
{
static string GetLODName(const string& file, const char* szEXT, uint32 LOD)
{
	return LOD ? file + "_lod" + CryStringUtils::toString(LOD) + "." + CRY_SKIN_FILE_EXT : file + "." + CRY_SKIN_FILE_EXT;
}

static bool FindFirstMesh(CMesh*& pMesh, CNodeCGF*& pGFXNode, CContentCGF* pContent, const char* filenameNoExt, int lod)
{
	uint32 numNodes = pContent->GetNodeCount();
	pGFXNode = 0;
	for (uint32 n = 0; n < numNodes; n++)
	{
		CNodeCGF* pNode = pContent->GetNode(n);
		if (pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh)
		{
			pGFXNode = pNode;
			break;
		}
	}
	if (pGFXNode == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. GFXNode not found");
		return false;
	}

	pMesh = pGFXNode->pMesh;
	if (pMesh && pMesh->m_pBoneMapping == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. Skeleton-Initial-Positions are missing");
		return false;
	}
	return true;
}
}

bool CryCHRLoader::BeginLoadSkinRenderMesh(CSkin* pSkin, int nRenderLod, EStreamTaskPriority estp)
{
	using namespace CryCHRLoader_LoadNewSKIN_Helpers;

	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	static_assert(sizeof(TFace) == 6, "Invalid type size!");

	const char* szFilePath = pSkin->GetModelFilePath();
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_CHR, 0, "LoadCharacter %s", szFilePath);

	const char* szExt = PathUtil::GetExt(szFilePath);
	m_strGeomFileNameNoExt.assign(szFilePath, *szExt ? szExt - 1 : szExt);

	if (m_strGeomFileNameNoExt.empty())
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, szFilePath, "Wrong character filename");
		return 0;
	}

	//--------------------------------------------------------------------------------------------

	CModelMesh* pMM = pSkin->GetModelMesh(nRenderLod);

	string fileName = GetLODName(m_strGeomFileNameNoExt, szExt, nRenderLod);
	if (pMM && pMM->m_stream.bHasMeshFile)
		fileName += 'm';

	m_pModelSkin = pSkin;

	// figure out if compute skinning buffers are needed for this skin by iterating through the skin attachments
	{
		CDefaultSkinningReferences* pSkinRef = g_pCharacterManager->GetDefaultSkinningReferences(m_pModelSkin);
		uint32 nSkinInstances = pSkinRef->m_RefByInstances.size();

		bool computeSkinning = false;
		for (int i = 0; i < nSkinInstances; i++)
		{
			CAttachmentSKIN* pAttachmentSkin = pSkinRef->m_RefByInstances[i];
			if (pAttachmentSkin->GetFlags() & FLAGS_ATTACH_COMPUTE_SKINNING)
				computeSkinning = true;
		}
		// since we dont have a facility to trigger an asset reload explicitly,
		// we need to allocate the compute skinning buffers always, because
		// the skinning method could be changed any time in the editor
		if (computeSkinning || gEnv->IsEditor())
		{
			m_pModelSkin->SetNeedsComputeSkinningBuffers();
		}
	}

	StreamReadParams params;
	params.nFlags = 0;//IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
	params.dwUserData = nRenderLod;
	params.ePriority = estp;
	IStreamEngine* pStreamEngine = gEnv->pSystem->GetStreamEngine();
	m_pStream = pStreamEngine->StartRead(eStreamTaskTypeAnimation, fileName.c_str(), this, &params);

	return m_pStream != NULL;
}

void CryCHRLoader::EndStreamSkinAsync(IReadStream* pStream)
{
	using namespace CryCHRLoader_LoadNewSKIN_Helpers;

	int nRenderLod = (int)pStream->GetParams().dwUserData;
	CSkin* pModelSkin = m_pModelSkin;

	const char* lodName = pStream->GetName();

	SmartContentCGF pCGF = g_pI3DEngine->CreateChunkfileContent(lodName);
	bool bLoaded = g_pI3DEngine->LoadChunkFileContentFromMem(pCGF, pStream->GetBuffer(), pStream->GetBytesRead(), IStatObj::ELoadingFlagsJustGeometry, false, false);

	if (!bLoaded || !pCGF)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, lodName, "%s: The Chunk-Loader failed to load the file.", __FUNCTION__);
		return;
	}

	CMesh* pMesh = 0;
	CNodeCGF* pMeshNode = 0;
	if (!FindFirstMesh(pMesh, pMeshNode, pCGF, m_strGeomFileNameNoExt.c_str(), nRenderLod))
		return;

	CModelMesh& modelMesh = pModelSkin->m_arrModelMeshes[nRenderLod];
	m_pNewRenderMesh = modelMesh.InitRenderMeshAsync(pMesh, m_pModelSkin->GetModelFilePath(), nRenderLod, m_arrNewRenderChunks, m_pModelSkin->NeedsComputeSkinningBuffers());
}

void CryCHRLoader::EndStreamSkinSync(IReadStream* pStream)
{
	CDefaultSkinningReferences* pSkinRef = g_pCharacterManager->GetDefaultSkinningReferences(m_pModelSkin);
	if (pSkinRef == 0)
		return;

	uint32 nSkinInstances = pSkinRef->m_RefByInstances.size();
	uint32 nVClothInstances = pSkinRef->m_RefByInstancesVCloth.size();

	int nRenderLod = (int)pStream->GetParams().dwUserData;
	if (CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(nRenderLod))
	{
		pModelMesh->InitRenderMeshSync(m_arrNewRenderChunks, m_pNewRenderMesh);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (pRenderMesh)
		{
			for (int i = 0; i < nSkinInstances; i++)
			{
				CAttachmentSKIN* pAttachmentSkin = pSkinRef->m_RefByInstances[i];
				int guid = pAttachmentSkin->GetGuid();

				if (guid > 0)
				{
					pRenderMesh->CreateRemappedBoneIndicesPair(pAttachmentSkin->m_arrRemapTable, guid, pAttachmentSkin);
				}

				pAttachmentSkin->m_pAttachmentManager->RequestMergeCharacterAttachments();
			}

			for (int i = 0; i < nVClothInstances; i++)
			{
				CAttachmentVCLOTH* pAttachmentVCloth = (CAttachmentVCLOTH*)pSkinRef->m_RefByInstancesVCloth[i];
				int guid = pAttachmentVCloth->GetGuid();

				if (guid > 0)
				{
					if (pAttachmentVCloth->m_pSimSkin)
					{
						IRenderMesh* pVCSimRenderMesh = pAttachmentVCloth->m_pSimSkin->GetIRenderMesh(0);
						if (pVCSimRenderMesh && (pVCSimRenderMesh == pRenderMesh))
						{
							pVCSimRenderMesh->CreateRemappedBoneIndicesPair(pAttachmentVCloth->m_arrSimRemapTable, guid, pAttachmentVCloth);
						}
					}

					if (pAttachmentVCloth->m_pRenderSkin)
					{
						IRenderMesh* pVCRenderMesh = pAttachmentVCloth->m_pRenderSkin->GetIRenderMesh(0);
						if (pVCRenderMesh && (pVCRenderMesh == pRenderMesh))
						{
							pVCRenderMesh->CreateRemappedBoneIndicesPair(pAttachmentVCloth->m_arrRemapTable, guid, pAttachmentVCloth);
						}
					}

					pAttachmentVCloth->m_pAttachmentManager->RequestMergeCharacterAttachments();
				}
			}
		}
	}

	m_pNewRenderMesh = NULL;
	m_arrNewRenderChunks.clear();
}

void CryCHRLoader::ClearCachedLodSkeletonResults()
{
#ifdef EDITOR_PCDEBUGCODE
	s_validatedSkeletons.clear();
#endif
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----              this belongs into another file (i.e.CSkin)   -------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

namespace SkinLoader_Helpers
{

struct LevelOfDetail
{
	CContentCGF* m_arrContentCGF[g_nMaxGeomLodLevels * 2];
	CContentCGF* m_arrContentMeshCGF[g_nMaxGeomLodLevels * 2];
	string       m_cgfNames[g_nMaxGeomLodLevels * 2];

	LevelOfDetail()
	{
		memset(m_arrContentCGF, 0, sizeof(m_arrContentCGF));
		memset(m_arrContentMeshCGF, 0, sizeof(m_arrContentMeshCGF));
	}

	~LevelOfDetail()
	{
		uint32 num = sizeof(m_arrContentCGF) / sizeof(CContentCGF*);
		for (uint32 i = 0; i < num; i++)
		{
			if (m_arrContentCGF[i])
				g_pI3DEngine->ReleaseChunkfileContent(m_arrContentCGF[i]), m_arrContentCGF[i] = 0;
			if (m_arrContentMeshCGF[i])
				g_pI3DEngine->ReleaseChunkfileContent(m_arrContentMeshCGF[i]), m_arrContentMeshCGF[i] = 0;
		}
	}
};

static string GetLODName(const string& file, const char* szEXT, uint32 LOD)
{
	return LOD ? file + "_lod" + CryStringUtils::toString(LOD) + "." + CRY_SKIN_FILE_EXT : file + "." + CRY_SKIN_FILE_EXT;
}

#ifdef EDITOR_PCDEBUGCODE
static bool ValidateLodSkeleton(CSkinningInfo* pSkinningInfo0, CSkinningInfo* pSkinningInfo1, int lod, const char* filename)
{
	std::pair<string, int> nameAndLodNum = std::make_pair(filename, lod);
	TValidatedSkeletonMap::const_iterator alreadyValidated = s_validatedSkeletons.find(nameAndLodNum);
	bool bOK = true;

	if (alreadyValidated == s_validatedSkeletons.end())
	{
		const DynArray<CryBoneDescData>& arrBones0 = pSkinningInfo0->m_arrBonesDesc;
		const DynArray<CryBoneDescData>& arrBones1 = pSkinningInfo1->m_arrBonesDesc;

		const uint32 numBones0 = arrBones0.size();
		const uint32 numBones1 = arrBones1.size();

		bOK = (numBones0 == numBones1);

		for (uint32 g = 0; bOK && g < numBones0; g++)
		{
			bOK = (0 == stricmp(arrBones0[g].m_arrBoneName, arrBones1[g].m_arrBoneName));
		}

		if (!bOK)
		{
			const uint32 maxBones = max(numBones0, numBones1);

			for (uint32 bn = 0; bn < maxBones; bn++)
			{
				const char* boneNameInInfo0 = (bn < numBones0) ? arrBones0[bn].m_arrBoneName : "N/A";
				const char* boneNameInInfo1 = (bn < numBones1) ? arrBones1[bn].m_arrBoneName : "N/A";

				if (stricmp(boneNameInInfo0, boneNameInInfo1))
				{
					CryLog("File '%s' joint ID #%02d: LOD0=\"%s\" vs. LOD%d=\"%s\"", filename, bn, boneNameInInfo0, lod, boneNameInInfo1);
				}
			}
		}

		// Never cache findings when running the editor so that user can edit and reload skeletons
		if (!gEnv->IsEditor())
		{
			s_validatedSkeletons[nameAndLodNum] = bOK;
		}
	}
	else
	{
		bOK = alreadyValidated->second;
	}

	if (!bOK)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filename, "Failed to load '%s' - LOD0 and LOD%i have different bones (see log for details)", filename, lod);
	}

	return bOK;
}
#endif

//! Validates a CryBoneDescData range.
//! \param begin Beginning of the range to check. Must be a ForwardIterator.
//! \param end End of the range. Must be a ForwardIterator.
//! \return A c-string explaining the reason of the validation failure in a human readable fashion, or nullptr in case of a validation success.
// TODO: Can be reused in a more general context (and ideally a little higher up in the asset loading stack).
// TODO: Replace the const char* return type with a general-purpose validation result class.
template<typename TIn>
static const char* ValidateBoneDescriptors(TIn begin, TIn end)
{
	const auto rangeSize = std::distance(begin, end);

	if (rangeSize >= MAX_JOINT_AMOUNT)
	{
		return "Skeleton definition contains too many joints. Current joint limit is set to " STRINGIFY(MAX_JOINT_AMOUNT) ".";
	}

	if (rangeSize == 0)
	{
		return "Skeleton definition contains no joints.";
	}

	if (!std::all_of(begin, end, [](const CryBoneDescData& boneDesc) { return boneDesc.m_DefaultB2W.IsOrthonormalRH(); }))
	{
		return "Skeleton definition contains joints with invalid (non-orthonormal) transformations in their default pose.";
	}

	std::vector<uint32> crc32Ids(rangeSize);
	std::transform(begin, end, crc32Ids.begin(), [](const CryBoneDescData& boneDesc) { return CCrc32::ComputeLowercase(boneDesc.m_arrBoneName); });
	std::sort(crc32Ids.begin(), crc32Ids.end());
	if (std::adjacent_find(crc32Ids.begin(), crc32Ids.end()) != crc32Ids.end())
	{
		return "Skeleton definition contains joint names with colliding crc32 identifiers.";
	}

	return nullptr;
}

static bool InitializeBones(CSkin* pSkeleton, const CSkinningInfo* pSkinningInfo, const char* filename)
{
	const auto& boneDescriptors = pSkinningInfo->m_arrBonesDesc;

	const char* const validationError = ValidateBoneDescriptors(boneDescriptors.begin(), boneDescriptors.end());
	if (validationError == nullptr)
	{
		pSkeleton->m_arrModelJoints.resize(boneDescriptors.size());

		for (uint32 i = 0, limit = boneDescriptors.size(); i < limit; ++i)
		{
			pSkeleton->m_arrModelJoints[i].m_idxParent = int32(i) + boneDescriptors[i].m_nOffsetParent;
			pSkeleton->m_arrModelJoints[i].m_nJointCRC32Lower = CCrc32::ComputeLowercase(boneDescriptors[i].m_arrBoneName);
			pSkeleton->m_arrModelJoints[i].m_DefaultAbsolute = QuatT(boneDescriptors[i].m_DefaultB2W);
			pSkeleton->m_arrModelJoints[i].m_DefaultAbsolute.q.Normalize();
			pSkeleton->m_arrModelJoints[i].m_NameModelSkin = boneDescriptors[i].m_arrBoneName;
		}

		// First joint is always assumed to be the root. Ensure that its parent index is set to -1 (SJointInfo invariant).
		pSkeleton->m_arrModelJoints[0].m_idxParent = -1;

		return true;
	}
	else
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, filename, "Failed to load skin geometry. Reason: %s", validationError);
		return false;
	}
}

static bool FindFirstMesh(CMesh*& pMesh, CNodeCGF*& pGFXNode, CContentCGF* pContent, const char* filenameNoExt, int lod)
{
	uint32 numNodes = pContent->GetNodeCount();
	pGFXNode = 0;
	for (uint32 n = 0; n < numNodes; n++)
	{
		CNodeCGF* pNode = pContent->GetNode(n);
		if (pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh)
		{
			pGFXNode = pNode;
			break;
		}
	}
	if (pGFXNode == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. GFXNode not found");
		return false;
	}

	pMesh = pGFXNode->pMesh;
	if (pMesh && pMesh->m_pBoneMapping == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. Skeleton-Initial-Positions are missing");
		return false;
	}

	return true;
}

static bool FindFirstMeshAndMaterial(CMesh*& pMesh, CNodeCGF*& pGFXNode, _smart_ptr<IMaterial>& pMaterial, CContentCGF* pContent, const char* filenameNoExt, int lod)
{
	uint32 numNodes = pContent->GetNodeCount();
	pGFXNode = 0;
	for (uint32 n = 0; n < numNodes; n++)
	{
		CNodeCGF* pNode = pContent->GetNode(n);
		if (pNode->type == CNodeCGF::NODE_MESH)
		{
			pGFXNode = pNode;
			break;
		}
	}
	if (pGFXNode == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. GFXNode not found");
		return false;
	}

	pMesh = pGFXNode->pMesh;
	if (pMesh && pMesh->m_pBoneMapping == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filenameNoExt, "Failed to Load Character file. Skeleton-Initial-Positions are missing");
		return false;
	}

	if (lod == 0 || pMaterial == NULL)
	{
		if (pGFXNode->pMaterial)
			pMaterial = g_pI3DEngine->GetMaterialManager()->LoadCGFMaterial(pGFXNode->pMaterial->name, PathUtil::GetPathWithoutFilename(filenameNoExt).c_str());
		else
			pMaterial = g_pI3DEngine->GetMaterialManager()->GetDefaultMaterial();
	}
	return true;
}

}

bool CSkin::LoadNewSKIN(const char* szFilePath, uint32 nLoadingFlags)
{
	if (g_pI3DEngine == 0)
		return 0;

	using namespace SkinLoader_Helpers;

	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	static_assert(sizeof(TFace) == 6, "Invalid type size!");

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_CHR, 0, "LoadCharacter %s", szFilePath);

	CRY_DEFINE_ASSET_SCOPE(CRY_SKEL_FILE_EXT, szFilePath);

	const char* szExt = PathUtil::GetExt(szFilePath);

	stack_string strGeomFileNameNoExt;
	strGeomFileNameNoExt.assign(szFilePath, *szExt ? szExt - 1 : szExt);
	if (strGeomFileNameNoExt.empty())
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, szFilePath, "CryAnimation: Failed to load SKIN: Wrong character filename");
		return 0;
	}
	stack_string szFileName = stack_string(strGeomFileNameNoExt.c_str()) + "." + szExt;

	//----------------------------------------------------------------------------------------------------------------------
	//--- select the BaseLOD (on consoles we can omit LODs with high poly-count to save memory and speedup rendering)
	//----------------------------------------------------------------------------------------------------------------------
	int32 baseLOD = 0;
	static ICVar* p_e_lod_min = gEnv->pConsole->GetCVar("e_CharLodMin");
	if (p_e_lod_min)
		baseLOD = max(0, p_e_lod_min->GetIVal()); //use this as the new LOD0 (but only if possible)
	//if (baseLOD)
	//	CryFatalError("CryAnimation: m_nBaseLOD should be 0");

	//----------------------------------------------------------------------------------------------------------------------
	//--- find all available LODs for this model
	//----------------------------------------------------------------------------------------------------------------------
	uint32 lodCount = 0;
	LevelOfDetail cgfs;                                     //in case of early exit we call the destructor automatically and delete all CContentCGFs
	for (int32 ml = g_nMaxGeomLodLevels - 1; ml >= 0; --ml) //load the smallest meshes first
	{
		string lodName = GetLODName(strGeomFileNameNoExt, szExt, ml);

		const bool lodExists = gEnv->pCryPak->IsFileExist(lodName);
		if (!lodExists)
			continue; //continue until we find the first valid LOD

		CContentCGF* pChunkFile = g_pI3DEngine->CreateChunkfileContent(lodName);

		if (pChunkFile == 0)
			CryFatalError("CryAnimation error: issue in Chunk-Loader");

		cgfs.m_arrContentCGF[ml] = pChunkFile; //cleanup automatically when we leave the function-scope
		cgfs.m_cgfNames[ml] = lodName;

		const bool bLoaded = g_pI3DEngine->LoadChunkFileContent(pChunkFile, lodName, 0);

		if (!bLoaded)
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, szFilePath, "CryAnimation (%s): Failed to load SKIN (Invalid File Contents) '%s'", __FUNCTION__, lodName.c_str());
			return 0;
		}

		if (!Console::GetInst().ca_StreamCHR)
		{
			string lodmName = lodName + "m";
			if (gEnv->pCryPak->IsFileExist(lodmName.c_str()))
			{
				cgfs.m_arrContentMeshCGF[ml] = g_pI3DEngine->CreateChunkfileContent(lodmName);
				const bool bLoaded = g_pI3DEngine->LoadChunkFileContent(cgfs.m_arrContentMeshCGF[ml], lodmName.c_str(), false);
				if (!bLoaded)
				{
					g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, szFilePath, "CryAnimation (%s): Failed to load SKIN (Invalid file contents) '%s'", __FUNCTION__, lodmName.c_str());
					return 0;
				}
			}
		}

		lodCount++;                                   //count all available and valid LODs
		if (baseLOD >= ml)  { baseLOD = ml; break;  } //this is the real base lod
	}

	if (lodCount > 0)
	{
		bool isVCloth = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_vertices.size() > 0;

		// fill vcloth data
		if (isVCloth)
		{
			m_VClothData.m_nndc.resize(cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_vertices.size());
			m_VClothData.m_listBendTriangles.resize(cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_triangles.size());
			m_VClothData.m_listBendTrianglePairs.resize(cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_trianglePairs.size());
			m_VClothData.m_nndcNotAttachedOrderedIdx.resize(cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_nndcNotAttachedOrderedIdx.size());

			{
				auto itNndc = m_VClothData.m_nndc.begin();
				for (auto it = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_vertices.begin(); it != cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_vertices.end(); it++, itNndc++)
				{
					itNndc->nndcDist = it->attributes.nndcDist;
					itNndc->nndcIdx = it->attributes.nndcIdx;
					itNndc->nndcNextParent = it->attributes.nndcNextParent;
				}
			}

			{
				auto itLBT = m_VClothData.m_listBendTriangles.begin();
				for (auto it = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_triangles.begin(); it != cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_triangles.end(); it++, itLBT++)
				{
					itLBT->p0 = it->p0;
					itLBT->p1 = it->p1;
					itLBT->p2 = it->p2;
				}
			}

			{
				auto itLBTP = m_VClothData.m_listBendTrianglePairs.begin();
				for (auto it = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_trianglePairs.begin(); it != cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_trianglePairs.end(); it++, itLBTP++)
				{
					itLBTP->phi0 = it->angle;
					itLBTP->p0 = it->p0;
					itLBTP->p1 = it->p1;
					itLBTP->p2 = it->p2;
					itLBTP->p3 = it->p3;
					itLBTP->idxNormal0 = it->idxNormal0;
					itLBTP->idxNormal1 = it->idxNormal1;
				}
			}

			{
				auto itL = m_VClothData.m_nndcNotAttachedOrderedIdx.begin();
				for (auto it = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_nndcNotAttachedOrderedIdx.begin(); it != cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_nndcNotAttachedOrderedIdx.end(); it++, itL++)
				{
					*itL = it->nndcNotAttachedOrderedIdx;
				}
			}

			// Links
			{
				for (int i = 0; i < eVClothLink_COUNT; i++)
				{
					m_VClothData.m_links[i].resize(cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_links[i].size());
					auto itL = m_VClothData.m_links[i].begin();
					for (auto it = cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_links[i].begin(); it != cgfs.m_arrContentCGF[0]->GetVClothInfo()->m_links[i].end(); it++, itL++)
					{
						itL->i1 = it->i1;
						itL->i2 = it->i2;
						itL->lenSqr = it->lenSqr;
					}
				}
			}
		}
	}

	if (lodCount == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, szFileName, "CryAnimation (%s): Failed to load SKIN: No LODs found.", __FUNCTION__);
		return 0;
	}
	uint32 num = sizeof(cgfs.m_arrContentCGF) / sizeof(CContentCGF*) - baseLOD;
	for (uint32 l = 0; l < num; l++)
	{
		cgfs.m_arrContentCGF[l] = cgfs.m_arrContentCGF[l + baseLOD];         //internally baseLOD is the new LOD0
		cgfs.m_arrContentMeshCGF[l] = cgfs.m_arrContentMeshCGF[l + baseLOD]; //internally baseLOD is the new LOD0
		cgfs.m_cgfNames[l] = cgfs.m_cgfNames[l + baseLOD];
	}

	//----------------------------------------------------------------------------------------------------------------------
	//--- initialize all available LODs
	//----------------------------------------------------------------------------------------------------------------------

	m_arrModelMeshes.resize(lodCount, CModelMesh());
	CContentCGF* pBaseContent = cgfs.m_arrContentCGF[0];
	for (uint32 lod = 0; lod < lodCount; lod++)
	{
		CContentCGF* pLodContent = cgfs.m_arrContentCGF[lod];
		CSkinningInfo* pSkinningInfo = pLodContent->GetSkinningInfo();
		if (pSkinningInfo == 0)
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, szFilePath, "Skinning info is missing in lod %d", lod);
			return 0;
		}

		if (lod == 0)
		{
			uint32 result = InitializeBones(this, pSkinningInfo, szFileName);
			if (result == 0)
				return 0;
		}

#ifdef EDITOR_PCDEBUGCODE
		if (lod > 0)
		{
			//make sure that all skeletons in LOD1-5 are identical to LOD0 (or we might see weird skinning issues)
			CSkinningInfo* pBaseSkinning = pBaseContent->GetSkinningInfo();
			CSkinningInfo* pLodSkinning = pLodContent->GetSkinningInfo();
			if (!ValidateLodSkeleton(pBaseSkinning, pLodSkinning, lod, szFileName))
				return 0;
		}
#endif

		//-------------------------------------------------------------------------------------------------
		//---  initialize ModelMesh and RenderMesh (this is purely cosmetic stuff and therefore optional)
		//-------------------------------------------------------------------------------------------------
		CModelMesh* pModelMesh = GetModelMesh(lod);
		if (pModelMesh == 0)
			continue;  //meshes are optional
		CMesh* pMesh = 0;
		CNodeCGF* pMeshNode = 0;
		_smart_ptr<IMaterial> pMaterial;
		FindFirstMeshAndMaterial(pMesh, pMeshNode, pMaterial, pLodContent, strGeomFileNameNoExt.c_str(), lod);

		if (cgfs.m_arrContentMeshCGF[lod])
		{
			if (!FindFirstMesh(pMesh, pMeshNode, cgfs.m_arrContentMeshCGF[lod], strGeomFileNameNoExt.c_str(), lod))
				return 0;
		}

		uint32 success = pModelMesh->InitMesh(pMesh, pMeshNode, pMaterial, pSkinningInfo, m_strFilePath.c_str(), lod);
		if (success == 0)
			return 0;

		string meshFilename = cgfs.m_cgfNames[lod] + 'm';
		pModelMesh->m_stream.bHasMeshFile = gEnv->pCryPak->IsFileExist(meshFilename.c_str());
	} //loop over all LODs

	return 1; //success
}
