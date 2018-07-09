// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AttachmentBase.h"
#include "CharacterManager.h"
#include "CharacterInstance.h"
#include "Vertex/VertexData.h"
#include "Vertex/VertexCommand.h"
#include "Vertex/VertexCommandBuffer.h"
#include "AttachmentManager.h"
#include "AttachmentVCloth.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IShader.h>
#include "ModelMesh.h"
#include <CryMath/QTangent.h>
#include <CryThreading/IJobManager_JobDelegator.h>

void CAttachmentVCLOTH::ReleaseSoftwareRenderMeshes()
{
	m_pRenderMeshsSW[0] = NULL;
	m_pRenderMeshsSW[1] = NULL;
}

uint32 CAttachmentVCLOTH::Immediate_AddBinding(IAttachmentObject* pIAttachmentObject, ISkin* pISkinRender, uint32 nLoadingFlags)
{
	if (pIAttachmentObject == 0)
		return 0; //no attachment objects 

	if (pISkinRender == 0)
		CryFatalError("CryAnimation: if you create the binding for a Skin-Attachment, then you have to pass the pointer to an ISkin as well");

	uint32 nLogWarnings = (nLoadingFlags&CA_DisableLogWarnings) == 0;
	CSkin* pCSkinRenderModel = (CSkin*)pISkinRender;

	//only the SKIN-Instance is allowed to keep a smart-ptr CSkin object 
	//this should prevent ptr-hijacking in the future
	if (m_pRenderSkin != pCSkinRenderModel)
	{
		ReleaseRenderSkin();
		g_pCharacterManager->RegisterInstanceVCloth(pCSkinRenderModel, this); //register this CAttachmentVCLOTH instance in CSkin. 
		m_pRenderSkin = pCSkinRenderModel;            //increase the Ref-Counter 		
	}

	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pSkelFilePath = pDefaultSkeleton->GetModelFilePath();
	const char* pSkinFilePath = m_pRenderSkin->GetModelFilePath();

	uint32 numJointsSkel = pDefaultSkeleton->m_arrModelJoints.size();
	uint32 numJointsSkin = m_pRenderSkin->m_arrModelJoints.size();

	uint32 NotMatchingNames = 0;
	m_arrRemapTable.resize(numJointsSkin, 0);
	for (uint32 js = 0; js < numJointsSkin; js++)
	{
		const int32 nID = pDefaultSkeleton->GetJointIDByCRC32(m_pRenderSkin->m_arrModelJoints[js].m_nJointCRC32Lower);
		if (nID >= 0)
			m_arrRemapTable[js] = nID;
#ifdef EDITOR_PCDEBUGCODE
		else
			NotMatchingNames++, g_pILog->LogError("The joint-name (%s) of SKIN (%s) was not found in SKEL:  %s", m_pRenderSkin->m_arrModelJoints[js].m_NameModelSkin.c_str(), pSkinFilePath, pSkelFilePath);
#endif
	} //for loop

	if (NotMatchingNames)
	{
		if (pInstanceSkel->m_CharEditMode)
		{
			//for now limited to CharEdit
			RecreateDefaultSkeleton(pInstanceSkel, nLoadingFlags);
		}
		else
		{
			if (nLogWarnings)
			{
				CryLogAlways("SKEL: %s", pDefaultSkeleton->GetModelFilePath());
				CryLogAlways("SKIN: %s", m_pRenderSkin->GetModelFilePath());
				uint32 numJointCount = pDefaultSkeleton->GetJointCount();
				for (uint32 i = 0; i < numJointCount; i++)
				{
					const char* pJointName = pDefaultSkeleton->GetJointNameByID(i);
					CryLogAlways("%03d JointName: %s", i, pJointName);
				}
			}

			// Free the new attachment as we cannot use it
			SAFE_RELEASE(pIAttachmentObject);
			return 0; //critical! incompatible skeletons. cant create skin-attachment
		}
	}

	// Patch the remapping 
	for (size_t i = 0; i < m_pRenderSkin->GetNumLODs(); i++)
	{
		CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(i);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (!pRenderMesh)
			continue;
		pRenderMesh->CreateRemappedBoneIndicesPair(m_arrRemapTable, pDefaultSkeleton->GetGuid(), this);
	}

	SAFE_RELEASE(m_pIAttachmentObject);
	m_pIAttachmentObject = pIAttachmentObject;
	return 1;
}

void CAttachmentVCLOTH::ComputeClothCacheKey()
{
	uint32 keySimMesh = CCrc32::ComputeLowercase(m_pSimSkin->GetModelFilePath());
	uint32 keyRenderMesh = CCrc32::ComputeLowercase(m_pRenderSkin->GetModelFilePath());
	m_clothCacheKey = (((uint64)keySimMesh) << 32) | keyRenderMesh;
}

void CAttachmentVCLOTH::AddClothParams(const SVClothParams& clothParams)
{
	m_clothPiece.SetClothParams(clothParams);
}

bool CAttachmentVCLOTH::InitializeCloth()
{
	return m_clothPiece.Initialize(this);
}

const SVClothParams& CAttachmentVCLOTH::GetClothParams()
{
	return m_clothPiece.GetClothParams();
}

uint32 CAttachmentVCLOTH::AddSimBinding(const ISkin& pISkinRender, uint32 nLoadingFlags)
{
	uint32 nLogWarnings = (nLoadingFlags&CA_DisableLogWarnings) == 0;
	CSkin* pCSkinRenderModel = (CSkin*)&pISkinRender;

	//only the VCLOTH-Instance is allowed to keep a smart-ptr CSkin object 
	//this should prevent ptr-hijacking in the future
	if (m_pSimSkin != pCSkinRenderModel)
	{
		ReleaseSimSkin();
		g_pCharacterManager->RegisterInstanceVCloth(pCSkinRenderModel, this); //register this CAttachmentVCLOTH instance in CSkin. 
		m_pSimSkin = pCSkinRenderModel;            //increase the Ref-Counter 		
	}

	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pSkelFilePath = pDefaultSkeleton->GetModelFilePath();
	const char* pSkinFilePath = m_pSimSkin->GetModelFilePath();

	uint32 numJointsSkel = pDefaultSkeleton->m_arrModelJoints.size();
	uint32 numJointsSkin = m_pSimSkin->m_arrModelJoints.size();

	uint32 NotMatchingNames = 0;
	m_arrSimRemapTable.resize(numJointsSkin, 0);
	for (uint32 js = 0; js < numJointsSkin; js++)
	{
		const int32 nID = pDefaultSkeleton->GetJointIDByCRC32(m_pSimSkin->m_arrModelJoints[js].m_nJointCRC32Lower);
		if (nID >= 0)
			m_arrSimRemapTable[js] = nID;
#ifdef EDITOR_PCDEBUGCODE
		else
			NotMatchingNames++, g_pILog->LogError("The joint-name (%s) of SKIN (%s) was not found in SKEL:  %s", m_pSimSkin->m_arrModelJoints[js].m_NameModelSkin.c_str(), pSkinFilePath, pSkelFilePath);
#endif
	} //for loop

	if (NotMatchingNames)
	{
		CryFatalError("CryAnimation: the simulation-attachment is supposed to have the same skeleton as the render-attachment");
		return 0; //critical! incompatible skeletons. cant create skin-attachment
	}

	return 1;
}

void CAttachmentVCLOTH::Immediate_ClearBinding(uint32 nLoadingFlags)
{
	if (m_pIAttachmentObject)
	{
		m_pIAttachmentObject->Release();
		m_pIAttachmentObject = 0;

		ReleaseRenderSkin();
		ReleaseSimSkin();

		if (nLoadingFlags&CA_SkipSkelRecreation)
			return;
		//for now limited to CharEdit
//		CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
//		if (pInstanceSkel->GetCharEditMode())
//			RecreateDefaultSkeleton(pInstanceSkel,CA_CharEditModel|nLoadingFlags); 
	}
};

void CAttachmentVCLOTH::RecreateDefaultSkeleton(CCharInstance* pInstanceSkel, uint32 nLoadingFlags)
{
	CDefaultSkeleton* const pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pOriginalFilePath = pDefaultSkeleton->GetModelFilePath();
	if (pDefaultSkeleton->GetModelFilePathCRC64() && pOriginalFilePath[0] == '_')
	{
		pOriginalFilePath++; // All extended skeletons have an '_' in front of the filepath to not confuse them with regular skeletons.
	}

	CDefaultSkeleton* pOrigDefaultSkeleton = g_pCharacterManager->CheckIfModelSKELLoaded(pOriginalFilePath, nLoadingFlags);
	if (!pOrigDefaultSkeleton)
	{
		return;
	}

	pOrigDefaultSkeleton->SetKeepInMemory(true);

	std::vector<const char*> mismatchingSkins;
	uint64 nExtendedCRC64 = CCrc32::ComputeLowercase(pOriginalFilePath);
	for (auto && pAttachment : m_pAttachmentManager->m_arrAttachments)
	{
		if (pAttachment->GetType() != CA_VCLOTH)
		{
			continue;
		}

		const CSkin* const pSkin = static_cast<CAttachmentVCLOTH*>(pAttachment.get())->m_pRenderSkin;
		if (!pSkin)
		{
			continue;
		}

		const char* const pSkinFilename = pSkin->GetModelFilePath();
		mismatchingSkins.push_back(pSkinFilename);
		nExtendedCRC64 += CCrc32::ComputeLowercase(pSkinFilename);
	}

	CDefaultSkeleton* const pExtDefaultSkeleton = g_pCharacterManager->CreateExtendedSkel(pInstanceSkel, pOrigDefaultSkeleton, nExtendedCRC64, mismatchingSkins, nLoadingFlags);
	if (pExtDefaultSkeleton)
	{
		pInstanceSkel->RuntimeInit(pExtDefaultSkeleton);
		m_pAttachmentManager->m_TypeSortingRequired++;
		m_pAttachmentManager->UpdateAllRemapTables();
	}
}

void CAttachmentVCLOTH::UpdateRemapTable()
{
	if (m_pRenderSkin == 0)
		return;

	ReleaseRenderRemapTablePair();
	ReleaseSimRemapTablePair();
	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pSkelFilePath = pDefaultSkeleton->GetModelFilePath();
	const char* pSkinFilePath = m_pRenderSkin->GetModelFilePath();

	uint32 numJointsSkel = pDefaultSkeleton->m_arrModelJoints.size();
	uint32 numJointsSkin = m_pRenderSkin->m_arrModelJoints.size();

	m_arrRemapTable.resize(numJointsSkin, 0);
	for (uint32 js = 0; js < numJointsSkin; js++)
	{
		const int32 nID = pDefaultSkeleton->GetJointIDByCRC32(m_pRenderSkin->m_arrModelJoints[js].m_nJointCRC32Lower);
		if (nID >= 0)
			m_arrRemapTable[js] = nID;
		else
			CryFatalError("ModelError: data-corruption when executing UpdateRemapTable for SKEL (%s) and SKIN (%s) ", pSkelFilePath, pSkinFilePath); //a fail in this case is virtually impossible, because the SKINs are alread attached 
	}

	// Patch the remapping 
	for (size_t i = 0; i < m_pRenderSkin->GetNumLODs(); i++)
	{
		CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(i);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (!pRenderMesh)
			continue;
		pRenderMesh->CreateRemappedBoneIndicesPair(m_arrRemapTable, pDefaultSkeleton->GetGuid(), this);
	}

	CModelMesh* pSimModelMesh = m_pSimSkin->GetModelMesh(0);
	IRenderMesh* pSimRenderMesh = pSimModelMesh->m_pIRenderMesh;
	if (pSimRenderMesh)
		pSimRenderMesh->CreateRemappedBoneIndicesPair(m_arrSimRemapTable, pDefaultSkeleton->GetGuid(), this);
}

bool CAttachmentVCLOTH::EnsureRemapTableIsValid()
{
	if (m_arrSimRemapTable.size() == 0 || m_arrRemapTable.size() == 0)
	{
		UpdateRemapTable();
	}
	return m_arrSimRemapTable.size() > 0 && m_arrRemapTable.size() > 0;
}

void CAttachmentVCLOTH::ReleaseSimRemapTablePair()
{
	if (!m_pSimSkin)
		return;

	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pModelSkel = pInstanceSkel->m_pDefaultSkeleton;
	const uint skeletonGuid = pModelSkel->GetGuid();

	CModelMesh* pModelSimMesh = m_pSimSkin->GetModelMesh(0);
	IRenderMesh* pSimRenderMesh = pModelSimMesh->m_pIRenderMesh;
	if (pSimRenderMesh)
		pSimRenderMesh->ReleaseRemappedBoneIndicesPair(skeletonGuid);
}

void CAttachmentVCLOTH::ReleaseRenderRemapTablePair()
{
	if (!m_pRenderSkin)
		return;

	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pModelSkel = pInstanceSkel->m_pDefaultSkeleton;
	const uint skeletonGuid = pModelSkel->GetGuid();
	for (size_t i = 0; i < m_pRenderSkin->GetNumLODs(); i++)
	{
		CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(i);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (!pRenderMesh)
			continue;
		pRenderMesh->ReleaseRemappedBoneIndicesPair(skeletonGuid);
	}
}

uint32 CAttachmentVCLOTH::Immediate_SwapBinding(IAttachment* pNewAttachment)
{
	CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] CAttachmentVCLOTH::SwapBinding attempting to swap skin attachment bindings this is not supported");
	return 0;
}

float CAttachmentVCLOTH::GetExtent(EGeomForm eForm)
{
	if (m_pRenderSkin)
	{
		int nLOD = m_pRenderSkin->SelectNearestLoadedLOD(0);
		if (IRenderMesh* pMesh = m_pRenderSkin->GetIRenderMesh(nLOD))
			return pMesh->GetExtent(eForm);
	}
	return 0.f;
}

void CAttachmentVCLOTH::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	int nLOD = m_pRenderSkin->SelectNearestLoadedLOD(0);
	IRenderMesh* pMesh = m_pRenderSkin->GetIRenderMesh(nLOD);
	if (!pMesh)
	{
		points.fill(ZERO);
		return;
	}

	SSkinningData* pSkinningData = NULL;
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	for (int n = 0; n < 3; n++)
	{
		int nList = (nFrameID - n) % 3;
		if (m_arrSkinningRendererData[nList].nFrameID == nFrameID - n)
		{
			pSkinningData = m_arrSkinningRendererData[nList].pSkinningData;
			break;
		}
	}

	pMesh->GetRandomPoints(points, seed, eForm, pSkinningData);
}

const QuatTS CAttachmentVCLOTH::GetAttWorldAbsolute() const
{
	QuatTS rPhysLocation = m_pAttachmentManager->m_pSkelInstance->m_location;
	return rPhysLocation;
};

void CAttachmentVCLOTH::UpdateAttModelRelative()
{
}

int CAttachmentVCLOTH::GetGuid() const
{
	return m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetGuid();
}

void CAttachmentVCLOTH::ReleaseRenderSkin()
{
	if (m_pRenderSkin)
	{
		g_pCharacterManager->UnregisterInstanceVCloth(m_pRenderSkin, this);
		ReleaseRenderRemapTablePair();
		m_pRenderSkin = 0;
	}
}

void CAttachmentVCLOTH::ReleaseSimSkin()
{
	if (m_pSimSkin)
	{
		g_pCharacterManager->UnregisterInstanceVCloth(m_pSimSkin, this);
		ReleaseSimRemapTablePair();
		m_pSimSkin = 0;
	}
}

CAttachmentVCLOTH::~CAttachmentVCLOTH()
{
	const int nFrameID = gEnv->pRenderer ? gEnv->pRenderer->EF_GetSkinningPoolID() : 0;
	const int nList = nFrameID % 3;
	if (m_arrSkinningRendererData[nList].nFrameID == nFrameID && m_arrSkinningRendererData[nList].pSkinningData)
	{
		if (m_arrSkinningRendererData[nList].pSkinningData->pAsyncJobs)
			gEnv->pJobManager->WaitForJob(*m_arrSkinningRendererData[nList].pSkinningData->pAsyncJobs);
	}

	m_vertexAnimation.SetClothData(NULL);
	ReleaseRenderSkin();
	ReleaseSimSkin();

	CVertexAnimation::RemoveSoftwareRenderMesh(this);
	for (uint32 j = 0; j < 2; ++j)
		m_pRenderMeshsSW[j] = NULL;
}

void CAttachmentVCLOTH::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CAttachmentVCLOTH");

		bool bHideInMainPass;

		if (ser.IsWriting())
		{
			bHideInMainPass = ((m_AttFlags & FLAGS_ATTACH_HIDE_MAIN_PASS) == FLAGS_ATTACH_HIDE_MAIN_PASS);
		}

		ser.Value("HideInMainPass", bHideInMainPass);

		if (ser.IsReading())
		{
			HideAttachment(bHideInMainPass);
		}

		ser.EndGroup();
	}
}

size_t CAttachmentVCLOTH::SizeOfThis() const
{
	size_t nSize = sizeof(CAttachmentVCLOTH) + sizeofVector(m_strSocketName);
	nSize += m_arrRemapTable.get_alloc_size();
	return nSize;
}

void CAttachmentVCLOTH::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_strSocketName);
	pSizer->AddObject(m_arrRemapTable);
}

_smart_ptr<IRenderMesh> CAttachmentVCLOTH::CreateVertexAnimationRenderMesh(uint lod, uint id)
{
	m_pRenderMeshsSW[id] = NULL; // smart pointer release

	if (m_pRenderSkin == 0)
		return _smart_ptr<IRenderMesh>(NULL);

	uint32 numModelMeshes = m_pRenderSkin->m_arrModelMeshes.size();
	if (lod >= numModelMeshes)
		return _smart_ptr<IRenderMesh>(NULL);

	_smart_ptr<IRenderMesh> pIStaticRenderMesh = m_pRenderSkin->m_arrModelMeshes[lod].m_pIRenderMesh;
	if (pIStaticRenderMesh == 0)
		return _smart_ptr<IRenderMesh>(NULL);
	;

	CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(lod);
	uint32 success = pModelMesh->InitSWSkinBuffer();
	if (success == 0)
		return _smart_ptr<IRenderMesh>(NULL);
	;

	if (m_sSoftwareMeshName.empty() && pIStaticRenderMesh->GetSourceName() != NULL)
	{
		m_sSoftwareMeshName.reserve(strlen(pIStaticRenderMesh->GetSourceName()) + 3);
		m_sSoftwareMeshName = pIStaticRenderMesh->GetSourceName();
		m_sSoftwareMeshName += "_SW";
	}

	m_pRenderMeshsSW[id] = g_pIRenderer->CreateRenderMeshInitialized(
		NULL
		, pIStaticRenderMesh->GetVerticesCount()
		, EDefaultInputLayouts::P3F_C4B_T2F
		, NULL
		, pIStaticRenderMesh->GetIndicesCount()
		, prtTriangleList
		, "Character"
		, m_sSoftwareMeshName.c_str()
		, eRMT_Transient);

	m_pRenderMeshsSW[id]->SetMeshLod(lod);

	TRenderChunkArray& chunks = pIStaticRenderMesh->GetChunks();
	TRenderChunkArray  nchunks;
	nchunks.resize(chunks.size());
	for (size_t i = 0; i < size_t(chunks.size()); ++i)
	{
		nchunks[i].m_texelAreaDensity = chunks[i].m_texelAreaDensity;
		nchunks[i].nFirstIndexId = chunks[i].nFirstIndexId;
		nchunks[i].nNumIndices = chunks[i].nNumIndices;
		nchunks[i].nFirstVertId = chunks[i].nFirstVertId;
		nchunks[i].nNumVerts = chunks[i].nNumVerts;
#ifdef SUBDIVISION_ACC_ENGINE
		nchunks[i].nFirstFaceId = chunks[i].nFirstFaceId;
		nchunks[i].nNumFaces = chunks[i].nNumFaces;
		nchunks[i].nPrimitiveType = chunks[i].nPrimitiveType;
#endif
		nchunks[i].m_nMatFlags = chunks[i].m_nMatFlags;
		nchunks[i].m_nMatID = chunks[i].m_nMatID;
		nchunks[i].nSubObjectIndex = chunks[i].nSubObjectIndex;
	}
	m_pRenderMeshsSW[id]->SetRenderChunks(&nchunks[0], nchunks.size(), false);

#ifndef _RELEASE
	m_vertexAnimation.m_vertexAnimationStats.sCharInstanceName = m_pAttachmentManager->m_pSkelInstance->GetFilePath();
	m_vertexAnimation.m_vertexAnimationStats.sAttachmentName = "";//m_Name; TODO fix
	m_vertexAnimation.m_vertexAnimationStats.pCharInstance = m_pAttachmentManager->m_pSkelInstance;
#endif
	return m_pRenderMeshsSW[id];
}

void CAttachmentVCLOTH::DrawAttachment(SRendParams& RendParams, const SRenderingPassInfo &passInfo, const Matrix34& rWorldMat34, f32 fZoomFactor)
{
	if (!m_clothPiece.GetSimulator().IsVisible() || Console::GetInst().ca_VClothMode == 0) return;

	bool bNeedSWskinning = (m_pAttachmentManager->m_pSkelInstance->m_CharEditMode&CA_CharacterTool); // in character tool always use software skinning
	if (!bNeedSWskinning)
	{
		m_clothPiece.GetSimulator().HandleCameraDistance();
		bNeedSWskinning = !m_clothPiece.GetSimulator().IsGpuSkinning();
	}

	//-----------------------------------------------------------------------------
	//---              map logical LOD to render LOD                            ---
	//-----------------------------------------------------------------------------
	int32 numLODs = m_pRenderSkin->m_arrModelMeshes.size();
	if (numLODs == 0)
		return;
	int nDesiredRenderLOD = RendParams.lodValue.LodA();
	if (nDesiredRenderLOD == -1)
		nDesiredRenderLOD = RendParams.lodValue.LodB();
	if (nDesiredRenderLOD >= numLODs)
	{
		if (m_AttFlags&FLAGS_ATTACH_RENDER_ONLY_EXISTING_LOD)
			return;  //early exit, if LOD-file doesn't exist
		nDesiredRenderLOD = numLODs - 1; //render the last existing LOD-file
	}
	int nRenderLOD = m_pRenderSkin->SelectNearestLoadedLOD(nDesiredRenderLOD);  //we can render only loaded LODs

	m_pRenderSkin->m_arrModelMeshes[nRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();
	m_pSimSkin->m_arrModelMeshes[nRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();

	Matrix34 FinalMat34 = rWorldMat34;
	RendParams.pMatrix = &FinalMat34;
	RendParams.pInstance = this;
	RendParams.pMaterial = (IMaterial*)m_pIAttachmentObject->GetReplacementMaterial(nRenderLOD); //the Replacement-Material has priority
	if (RendParams.pMaterial == 0)
		RendParams.pMaterial = (IMaterial*)m_pRenderSkin->GetIMaterial(nRenderLOD); //as a fall back take the Base-Material from the model

	bool bNewFrame = false;
	if (m_vertexAnimation.m_skinningPoolID != gEnv->pRenderer->EF_GetSkinningPoolID())
	{
		m_vertexAnimation.m_skinningPoolID = gEnv->pRenderer->EF_GetSkinningPoolID();
		++m_vertexAnimation.m_RenderMeshId;
		bNewFrame = true;
	}

	if (bNeedSWskinning && bNewFrame && nRenderLOD < m_clothPiece.GetNumLods())
	{
		bool visible = m_clothPiece.IsAlwaysVisible();
		//  			|| g_pCharacterManager->GetClothManager()->QueryVisibility((uint64)m_pAttachmentManager, center, passInfo.IsGeneralPass());
		if (m_clothPiece.PrepareCloth(m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose, rWorldMat34, visible, nRenderLOD))
		{
			m_AttFlags |= FLAGS_ATTACH_SW_SKINNING;
			m_vertexAnimation.SetClothData(&m_clothPiece);
		}
		else
		{
			m_AttFlags &= ~FLAGS_ATTACH_SW_SKINNING;
			m_vertexAnimation.SetClothData(NULL);

			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] Character cloth buffer exceeds maximum (results in flickering between skinning & simulation). Increase buffer size with global 'ca_ClothMaxChars'.");
		}
	}

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObj)
		return;

	pObj->m_pRenderNode = RendParams.pRenderNode;
	uint64 uLocalObjFlags = pObj->m_ObjFlags;

	//check if it should be drawn close to the player
	CCharInstance* pMaster = m_pAttachmentManager->m_pSkelInstance;
	if (pMaster->m_rpFlags & CS_FLAG_DRAW_NEAR)
	{
		uLocalObjFlags |= FOB_NEAREST;
	}
	else
	{
		uLocalObjFlags &= ~FOB_NEAREST;
	}

	pObj->m_fAlpha = RendParams.fAlpha;
	pObj->m_fDistance = RendParams.fDistance;
	pObj->SetAmbientColor(RendParams.AmbientColor, passInfo);

	uLocalObjFlags |= RendParams.dwFObjFlags;

	SRenderObjData* pD = pObj->GetObjData();

	// copy the shaderparams into the render object data from the render params
	if (RendParams.pShaderParams && RendParams.pShaderParams->size() > 0)
	{
		pD->SetShaderParams(RendParams.pShaderParams);
	}

	pD->m_uniqueObjectId = reinterpret_cast<uintptr_t>(RendParams.pInstance);

	bool bCheckMotion = pMaster->MotionBlurMotionCheck(pObj->m_ObjFlags);
	if (bCheckMotion)
		uLocalObjFlags |= FOB_MOTION_BLUR;

	assert(RendParams.pMatrix);
	Matrix34 RenderMat34 = (*RendParams.pMatrix);
	pObj->SetMatrix(RenderMat34, passInfo);
	pObj->m_nClipVolumeStencilRef = RendParams.nClipVolumeStencilRef;
	pObj->m_nTextureID = RendParams.nTextureID;

	pObj->m_nMaterialLayers = RendParams.nMaterialLayersBlend;

	pD->m_nVisionParams = RendParams.nVisionParams;
	pD->m_nHUDSilhouetteParams = RendParams.nHUDSilhouettesParams;

	pD->m_nCustomData = RendParams.nCustomData;
	pD->m_nCustomFlags = RendParams.nCustomFlags;
	if (RendParams.nCustomFlags & COB_CLOAK_HIGHLIGHT)
	{
		pD->m_fTempVars[5] = RendParams.fCustomData[0];
	}
	else if (RendParams.nCustomFlags & COB_POST_3D_RENDER)
	{
		memcpy(&pD->m_fTempVars[5], &RendParams.fCustomData[0], sizeof(float) * 4);
		pObj->m_fAlpha = 1.0f; // Use the alpha in the post effect instead of here
		pD->m_fTempVars[9] = RendParams.fAlpha;
	}
	pObj->m_DissolveRef = RendParams.nDissolveRef;

	pObj->m_ObjFlags = uLocalObjFlags | FOB_INSHADOW | FOB_TRANS_MASK;

	if (g_pI3DEngine->IsTessellationAllowed(pObj, passInfo))
	{
		// Allow this RO to be tessellated, however actual tessellation will be applied if enabled in material
		pObj->m_ObjFlags |= FOB_ALLOW_TESSELLATION;
	}

	pObj->m_nSort = fastround_positive(RendParams.fDistance * 2.0f);

	const float SORT_BIAS_AMOUNT = 1.f;
	if (pMaster->m_rpFlags & CS_FLAG_BIAS_SKIN_SORT_DIST)
	{
		pObj->m_fSort = SORT_BIAS_AMOUNT;
	}

	// get skinning data		
	const int numClothLods = m_clothPiece.GetNumLods();
	const bool swSkin = bNeedSWskinning && (nRenderLOD == 0) && (Console::GetInst().ca_vaEnable != 0) && (nRenderLOD < numClothLods);

	IF(!swSkin, 1)
		pObj->m_ObjFlags |= FOB_SKINNED;

	pD->m_pSkinningData = GetVertexTransformationData(swSkin, nRenderLOD, passInfo);
	m_pRenderSkin->m_arrModelMeshes[nRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();
	m_pSimSkin->m_arrModelMeshes[nRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();

	Vec3 skinOffset = m_pRenderSkin->m_arrModelMeshes[nRenderLOD].m_vRenderMeshOffset;
	pD->m_pSkinningData->vecAdditionalOffset = skinOffset;

	SVertexAnimationJob *pVertexAnimation = static_cast<SVertexAnimationJob*>(pD->m_pSkinningData->pCustomData);

	IRenderMesh* pRenderMesh = m_pRenderSkin->GetIRenderMesh(nRenderLOD);
	if (swSkin && pRenderMesh)
	{
		uint iCurrentRenderMeshID = m_vertexAnimation.m_RenderMeshId & 1;

		if (bNewFrame)
		{
			CreateVertexAnimationRenderMesh(nRenderLOD, iCurrentRenderMeshID);
			CVertexAnimation::RegisterSoftwareRenderMesh((CAttachmentVCLOTH*)this);
		}

		pRenderMesh = m_pRenderMeshsSW[iCurrentRenderMeshID];

		IF(pRenderMesh && bNewFrame, 1)
		{
			CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(nRenderLOD);
			CSoftwareMesh& geometry = pModelMesh->m_softwareMesh;

			SVertexSkinData vertexSkinData = SVertexSkinData();
			vertexSkinData.pTransformations = pD->m_pSkinningData->pBoneQuatsS;
			vertexSkinData.pTransformationRemapTable = pD->m_pSkinningData->pRemapTable;
			vertexSkinData.transformationCount = pD->m_pSkinningData->nNumBones;
			vertexSkinData.pVertexPositions = geometry.GetPositions();
			vertexSkinData.pVertexColors = geometry.GetColors();
			vertexSkinData.pVertexCoords = geometry.GetCoords();
			vertexSkinData.pVertexTransformIndices = geometry.GetBlendIndices();
			vertexSkinData.pVertexTransformWeights = geometry.GetBlendWeights();
			vertexSkinData.vertexTransformCount = geometry.GetBlendCount();
			vertexSkinData.pIndices = geometry.GetIndices();
			vertexSkinData.indexCount = geometry.GetIndexCount();
			CRY_ASSERT(pRenderMesh->GetVerticesCount() == geometry.GetVertexCount());

			// also update tangents & vertexCount to fix problems in skinning
			vertexSkinData.pVertexQTangents = geometry.GetTangents();
			vertexSkinData.pTangentUpdateTriangles = geometry.GetTangentUpdateData();
			vertexSkinData.tangetUpdateTriCount = geometry.GetTangentUpdateDataCount();
			vertexSkinData.pTangentUpdateVertIds = geometry.GetTangentUpdateVertIds();
			vertexSkinData.tangetUpdateVertIdsCount = geometry.GetTangentUpdateTriIdsCount();
			vertexSkinData.vertexCount = geometry.GetVertexCount();

#if CRY_PLATFORM_DURANGO
			const uint fslCreate = FSL_VIDEO_CREATE;
			const uint fslRead = FSL_READ | FSL_VIDEO;
#else
			const uint fslCreate = FSL_SYSTEM_CREATE;
			const uint fslRead = FSL_READ;
#endif

			vertexSkinData.pVertexPositionsPrevious = strided_pointer<const Vec3>(NULL);
			if (pD->m_pSkinningData->pPreviousSkinningRenderData)
				gEnv->pJobManager->WaitForJob(*pD->m_pSkinningData->pPreviousSkinningRenderData->pAsyncJobs);
			_smart_ptr<IRenderMesh>& pRenderMeshPrevious = m_pRenderMeshsSW[1 - iCurrentRenderMeshID];
			if (pRenderMeshPrevious != NULL)
			{
				pRenderMeshPrevious->LockForThreadAccess();
				Vec3* pPrevPositions = (Vec3*)pRenderMeshPrevious->GetPosPtrNoCache(vertexSkinData.pVertexPositionsPrevious.iStride, fslRead);
				if (pPrevPositions)
				{
					vertexSkinData.pVertexPositionsPrevious.data = pPrevPositions;
					pVertexAnimation->m_previousRenderMesh = pRenderMeshPrevious;
				}
				else
				{
					pRenderMeshPrevious->UnlockStream(VSF_GENERAL);
					pRenderMeshPrevious->UnLockForThreadAccess();
				}
			}
			m_vertexAnimation.SetSkinData(vertexSkinData);

			pVertexAnimation->vertexData.m_vertexCount = pRenderMesh->GetVerticesCount();

			pRenderMesh->LockForThreadAccess();

			SVF_P3F_C4B_T2F *pGeneral = (SVF_P3F_C4B_T2F*)pRenderMesh->GetPosPtrNoCache(pVertexAnimation->vertexData.pPositions.iStride, fslCreate);

			pVertexAnimation->vertexData.pPositions.data = (Vec3*)(&pGeneral[0].xyz);
			pVertexAnimation->vertexData.pPositions.iStride = sizeof(pGeneral[0]);
			pVertexAnimation->vertexData.pColors.data = (uint32*)(&pGeneral[0].color);
			pVertexAnimation->vertexData.pColors.iStride = sizeof(pGeneral[0]);
			pVertexAnimation->vertexData.pCoords.data = (Vec2*)(&pGeneral[0].st);
			pVertexAnimation->vertexData.pCoords.iStride = sizeof(pGeneral[0]);
			pVertexAnimation->vertexData.pVelocities.data = (Vec3*)pRenderMesh->GetVelocityPtr(pVertexAnimation->vertexData.pVelocities.iStride, fslCreate);
			pVertexAnimation->vertexData.pTangents.data = (SPipTangents*)pRenderMesh->GetTangentPtr(pVertexAnimation->vertexData.pTangents.iStride, fslCreate);
			pVertexAnimation->vertexData.pIndices = pRenderMesh->GetIndexPtr(fslCreate);
			pVertexAnimation->vertexData.m_indexCount = geometry.GetIndexCount();

			if (!pVertexAnimation->vertexData.pPositions ||
				!pVertexAnimation->vertexData.pVelocities ||
				!pVertexAnimation->vertexData.pTangents)
			{
				pRenderMesh->UnlockStream(VSF_GENERAL);
				pRenderMesh->UnlockStream(VSF_TANGENTS);
				pRenderMesh->UnlockStream(VSF_VERTEX_VELOCITY);
#if ENABLE_NORMALSTREAM_SUPPORT
				pRenderMesh->UnlockStream(VSF_NORMALS);
#endif
				pRenderMesh->UnLockForThreadAccess();
				return;
			}

			// If memory was pre-allocated for the command buffer, recompile into it.
			if (pVertexAnimation->commandBufferLength)
			{
				CVertexCommandBufferAllocatorStatic commandBufferAllocator(
					(uint8*)pD->m_pSkinningData->pCustomData + sizeof(SVertexAnimationJob),
					pVertexAnimation->commandBufferLength);
				pVertexAnimation->commandBuffer.Initialize(commandBufferAllocator);
				m_vertexAnimation.CompileCommands(pVertexAnimation->commandBuffer);
			}

			pVertexAnimation->pRenderMeshSyncVariable = pRenderMesh->SetAsyncUpdateState();

			SSkinningData *pCurrentJobSkinningData = *pD->m_pSkinningData->pMasterSkinningDataList;
			if (pCurrentJobSkinningData == NULL)
			{
				pVertexAnimation->Begin(pD->m_pSkinningData->pAsyncJobs);
			}
			else
			{
				// try to append to list
				pD->m_pSkinningData->pNextSkinningData = pCurrentJobSkinningData;
				void *pUpdatedJobSkinningData = CryInterlockedCompareExchangePointer(alias_cast<void *volatile *>(pD->m_pSkinningData->pMasterSkinningDataList), pD->m_pSkinningData, pCurrentJobSkinningData);

				// in case we failed (job has finished in the meantime), we need to start the job from the main thread
				if (pUpdatedJobSkinningData == NULL)
				{
					pVertexAnimation->Begin(pD->m_pSkinningData->pAsyncJobs);
				}
			}

			if ((Console::GetInst().ca_DebugSWSkinning > 0) || (pMaster->m_CharEditMode & CA_CharacterTool))
			{
				m_vertexAnimation.DrawVertexDebug(pRenderMesh, QuatT(RenderMat34), pVertexAnimation);
			}

			pRenderMesh->UnLockForThreadAccess();
			if (m_clothPiece.NeedsDebugDraw())
				m_clothPiece.DrawDebug(pVertexAnimation);
			if (!(Console::GetInst().ca_DrawCloth & 1))
			{
				return;
			}
		}
	}

	//	CryFatalError("CryAnimation: pMaster is zero");
	if (pRenderMesh)
	{
		//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, ColorF(0,1,0,1), false,"VCloth name: %s",GetName()),g_YLine+=16.0f;

		IMaterial* pMaterial = RendParams.pMaterial;
		if (pMaterial == 0)
			pMaterial = m_pRenderSkin->GetIMaterial(0);
		pObj->m_pCurrMaterial = pMaterial;
#ifndef _RELEASE
		CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(nRenderLOD);
		static ICVar *p_e_debug_draw = gEnv->pConsole->GetCVar("e_DebugDraw");
		if (p_e_debug_draw && p_e_debug_draw->GetIVal() > 0)
			pModelMesh->DrawDebugInfo(pMaster->m_pDefaultSkeleton, nRenderLOD, RenderMat34, p_e_debug_draw->GetIVal(), pMaterial, pObj, RendParams, passInfo.IsGeneralPass(), (IRenderNode*)RendParams.pRenderNode, m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose.GetAABB(),passInfo);
#endif
		pRenderMesh->Render(pObj, passInfo);
	}
}

//-----------------------------------------------------------------------------
//---     trigger streaming of desired LODs (only need in CharEdit)         ---
//-----------------------------------------------------------------------------
void CAttachmentVCLOTH::TriggerMeshStreaming(uint32 nDesiredRenderLOD, const SRenderingPassInfo &passInfo)
{
	if (!m_pRenderSkin)
		return;

	uint32 numLODs = m_pRenderSkin->m_arrModelMeshes.size();
	if (numLODs == 0)
		return;
	if (m_AttFlags&FLAGS_ATTACH_HIDE_ATTACHMENT)
		return;  //mesh not visible
	if (nDesiredRenderLOD >= numLODs)
	{
		if (m_AttFlags&FLAGS_ATTACH_RENDER_ONLY_EXISTING_LOD)
			return;  //early exit, if LOD-file doesn't exist
		nDesiredRenderLOD = numLODs - 1; //render the last existing LOD-file
	}
	m_pRenderSkin->m_arrModelMeshes[nDesiredRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();
	m_pSimSkin->m_arrModelMeshes[nDesiredRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();
}

SSkinningData* CAttachmentVCLOTH::GetVertexTransformationData(bool bVertexAnimation, uint8 nRenderLOD, const SRenderingPassInfo& passInfo)
{
	DEFINE_PROFILER_FUNCTION();
	CCharInstance* pMaster = m_pAttachmentManager->m_pSkelInstance;
	if (pMaster == 0)
	{
		CryFatalError("CryAnimation: pMaster is zero");
		return NULL;
	}

	uint32 skinningQuatCount = m_arrRemapTable.size();
	uint32 skinningQuatCountMax = pMaster->GetSkinningTransformationCount();
	uint32 nNumBones = min(skinningQuatCount, skinningQuatCountMax);

	// get data to fill
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	int nList = nFrameID % 3;
	int nPrevList = (nFrameID - 1) % 3;
	// before allocating new skinning date, check if we already have for this frame
	if (m_arrSkinningRendererData[nList].nFrameID == nFrameID && m_arrSkinningRendererData[nList].pSkinningData)
	{
		return m_arrSkinningRendererData[nList].pSkinningData;
	}

	if (pMaster->arrSkinningRendererData[nList].nFrameID != nFrameID)
	{
		pMaster->GetSkinningData(passInfo); // force master to compute skinning data if not available
	}

	uint32 nCustomDataSize = 0;
	uint commandBufferLength = 0;
	if (bVertexAnimation)
	{
		// Make sure the software skinning command gets compiled.
		SVertexSkinData vertexSkinData = SVertexSkinData();
		vertexSkinData.transformationCount = 1;
		vertexSkinData.vertexTransformCount = 4;
		vertexSkinData.tangetUpdateTriCount = 1;
		m_vertexAnimation.SetSkinData(vertexSkinData);

		// Compile the command buffer without allocating the commands to
		// compute the buffer length.
		CVertexCommandBufferAllocationCounter commandBufferAllocationCounter;
		CVertexCommandBuffer commandBuffer;
		commandBuffer.Initialize(commandBufferAllocationCounter);
		m_vertexAnimation.CompileCommands(commandBuffer);
		commandBufferLength = commandBufferAllocationCounter.GetCount();

		nCustomDataSize = sizeof(SVertexAnimationJob) + commandBufferLength;
	}

	SSkinningData *pSkinningData = gEnv->pRenderer->EF_CreateRemappedSkinningData(passInfo.GetIRenderView(), nNumBones, pMaster->arrSkinningRendererData[nList].pSkinningData, nCustomDataSize, pMaster->m_pDefaultSkeleton->GetGuid());
	pSkinningData->pCustomTag = this;
	if (nCustomDataSize)
	{
		SVertexAnimationJob *pVertexAnimation = new (pSkinningData->pCustomData) SVertexAnimationJob();
		pVertexAnimation->commandBufferLength = commandBufferLength;
	}
	m_arrSkinningRendererData[nList].pSkinningData = pSkinningData;
	m_arrSkinningRendererData[nList].nFrameID = nFrameID;
	PREFAST_ASSUME(pSkinningData);

	// set data for motion blur	
	if (m_arrSkinningRendererData[nPrevList].nFrameID == (nFrameID - 1) && m_arrSkinningRendererData[nPrevList].pSkinningData)
	{
		pSkinningData->nHWSkinningFlags |= eHWS_MotionBlured;
		pSkinningData->pPreviousSkinningRenderData = m_arrSkinningRendererData[nPrevList].pSkinningData;
		if (pSkinningData->pPreviousSkinningRenderData->pAsyncJobs)
			gEnv->pJobManager->WaitForJob(*pSkinningData->pPreviousSkinningRenderData->pAsyncJobs);
	}
	else
	{
		// if we don't have motion blur data, use the same as for the current frame
		pSkinningData->pPreviousSkinningRenderData = pSkinningData;
	}

	pSkinningData->pRemapTable = &m_arrRemapTable[0];

	return pSkinningData;
}

SMeshLodInfo CAttachmentVCLOTH::ComputeGeometricMean() const
{
	SMeshLodInfo lodInfo;
	lodInfo.Clear();

	CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(0);
	if (pModelMesh)
	{
		lodInfo.fGeometricMean = pModelMesh->m_geometricMeanFaceArea;
		lodInfo.nFaceCount = pModelMesh->m_faceCount;
	}

	return lodInfo;
}

#ifdef EDITOR_PCDEBUGCODE
void CAttachmentVCLOTH::DrawWireframeStatic(const Matrix34& m34, int nLOD, uint32 color)
{
	CModelMesh* pModelMesh = m_pRenderSkin->GetModelMesh(nLOD);
	if (pModelMesh)
		pModelMesh->DrawWireframeStatic(m34, color);
}
#endif // EDITOR_PCDEBUGCODE

void CAttachmentVCLOTH::HideAttachment(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= (FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
	else
		m_AttFlags &= ~(FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
}

void CAttachmentVCLOTH::HideInRecursion(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_RECURSION;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_RECURSION;
}

void CAttachmentVCLOTH::HideInShadow(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_SHADOW_PASS, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_SHADOW_PASS;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_SHADOW_PASS;
}

extern f32 g_YLine;

struct SEdgeInfo
{
	int m_iEdge;
	float m_cost;
	bool m_skip;

	SEdgeInfo() : m_iEdge(-1), m_cost(1e38f), m_skip(false) { }
};

bool CClothSimulator::AddGeometry(phys_geometry *pgeom)
{
	if (!pgeom || pgeom->pGeom->GetType() != GEOM_TRIMESH)
		return false;

	mesh_data *pMesh = (mesh_data*)pgeom->pGeom->GetData();

	m_nVtx = pMesh->nVertices;
	m_particlesHot = new SParticleHot[m_nVtx];
	m_particlesCold = new SParticleCold[m_nVtx];
	for (int i = 0; i < m_nVtx; i++)
	{
		m_particlesHot[i].pos = Vector4(pMesh->pVertices[i]);
		m_particlesCold[i].prevPos = m_particlesHot[i].pos; // vel = 0
	}

	return true;
}

int CClothSimulator::SetParams(const SVClothParams& params, float* weights)
{
	m_config = params;
	m_config.weights = weights;

	for (int i = 0; i < m_nVtx && m_config.weights; i++)
	{
		m_particlesHot[i].alpha = AttachmentVClothPreProcess::IsAttached(m_config.weights[i]) ? 1.0f : m_config.weights[i];
		m_particlesHot[i].factorAttached = 1.0f - m_config.weights[i];
		m_particlesCold[i].bAttached = AttachmentVClothPreProcess::IsAttached(m_particlesHot[i].alpha);
	}
	// refresh the edges for newly attached vertices
	for (int i = 0; i < m_nEdges; i++)
		PrepareEdge(m_links[i]);
	for (size_t i = 0; i < m_shearLinks.size(); i++)
		PrepareEdge(m_shearLinks[i]);
	for (size_t i = 0; i < m_bendLinks.size(); i++)
		PrepareEdge(m_bendLinks[i]);
	return 1;
}

namespace VClothUtils
{
	static inline float BendDetermineAngleAtRuntime(const Vec3& n0, const Vec3& n1, const Vec3 pRef0, const Vec3& pRef3)
	{
		float nDotN = n0.dot(n1);
		if (nDotN > 1.0f) nDotN = 1.0f;
		if (nDotN < -1.0f) nDotN = -1.0f;
		float alpha = acos(nDotN);
		float sign = n0.dot(pRef3 - pRef0) > 0.0f ? -1.0f : 1.0f;
		alpha *= sign;
		return alpha;
	}
}

void CClothSimulator::SetSkinnedPositions(const Vector4* points)
{
	if (!points)
		return;
	for (int i = 0; i < m_nVtx; i++)
	{
		m_particlesCold[i].skinnedPos = points[i];
	}
}

void CClothSimulator::GetVertices(Vector4* pWorldCoords) const
{
	// TODO: skip conversion, use Vector4 for skinning
	for (int i = 0; i < m_nVtx; i++)
	{
		pWorldCoords[i] = m_particlesHot[i].pos;
	}
}

void CClothSimulator::GetVerticesFaded(Vector4* pWorldCoords)
{
	float t = m_fadeTimeActual / m_config.disableSimulationTimeRange;

	if (m_fadeInOutPhysicsDirection > 0) t = 1.0f - t; // ensure fading direction (in/out)

	for (int i = 0; i < m_nVtx; i++)
		pWorldCoords[i] = m_particlesHot[i].pos;                                // copy positions

	//PushFadeInOutPhysics(); // store positions from simulation
	PositionsSetToSkinned(true, false); // set skinned positions, but not old positions

	// swap positions with pWorldCoords, to keep original positions in m_particlesHot
	Vector4 tmp;
	for (int i = 0; i < m_nVtx; i++)
	{
		tmp = pWorldCoords[i];
		pWorldCoords[i] = m_particlesHot[i].pos;
		m_particlesHot[i].pos = tmp;
	}

	for (int i = 0; i < m_nVtx; i++)
	{
		Vector4& A = pWorldCoords[i];
		const Vector4& B = m_particlesHot[i].pos;
		A = A + t * (B - A);
	}
}

bool CClothSimulator::IsVisible()
{
	if (GetParams().hide) return false;
	if (m_pAttachmentManager->m_pSkelInstance->GetISkeletonPose() && !m_pAttachmentManager->m_pSkelInstance->IsCharacterVisible()) return false;
	return true;
}

void CClothSimulator::StartStep(float time_interval, const QuatT& location)
{
	m_location = location;

	m_time = time_interval - m_config.timeStep; // m_time = time_interval would mean simulating subStepTime01 = 0 - All Interpolations would be the same like the step before with subStepTime01 = 1.0
	if (m_time < 0) m_time = 0;
	m_timeInterval = time_interval;
	m_steps = 0;
	if (m_dtPrev < 0) m_dtPrev = m_config.timeStep;

	uint32 numClothProxie = 0;
	uint32 numProxies = m_pAttachmentManager->GetProxyCount();
	for (uint32 i = 0; i < numProxies; i++)
	{
		if (m_pAttachmentManager->m_arrProxies[i].m_nPurpose == 1) // ==1: cloth
			numClothProxie++;
	}
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, ColorF(0,1,0,1), false,"numProxies: %d",numClothProxie),g_YLine+=16.0f;

	uint32 numCollidables = m_permCollidables.size();
	if (numClothProxie != numCollidables)
		m_permCollidables.resize(numClothProxie);

	Quat rotf = location.q;
	Quaternion ssequat(location.q);
	uint32 c = 0;
	for (uint32 i = 0; i < numProxies; i++)
	{
		if (m_pAttachmentManager->m_arrProxies[i].m_nPurpose != 1) // ==1: cloth
			continue;
		m_permCollidables[c].oldOffset = m_permCollidables[c].offset;
		m_permCollidables[c].oldR = m_permCollidables[c].R;

		const CProxy* pProxy = &m_pAttachmentManager->m_arrProxies[i];
		m_permCollidables[c].offset = ssequat * Vector4(pProxy->m_ProxyModelRelative.t);
		m_permCollidables[c].R = Matrix3(rotf * pProxy->m_ProxyModelRelative.q);
		m_permCollidables[c].cr = pProxy->m_params.w;
		m_permCollidables[c].cx = pProxy->m_params.x;
		m_permCollidables[c].cy = pProxy->m_params.y;
		m_permCollidables[c].cz = pProxy->m_params.z;
		c++;
	}

	// determine external translational velocity - should be moved downwards to use numCollidables (instead of m_permCollidables.size())
	// not used at the moment, since translationBlend, externalBlend default to zero
	// determined by Joint 0 (root)
	if (numProxies > 0)
	{
		//const Vec3& collidablePos = m_permCollidables[0].offset;
		const Vec3& collidablePos = m_pAttachmentManager->GetSkelInstance()->GetISkeletonPose()->GetAbsJointByID(0).t;

		m_externalDeltaTranslation = collidablePos - m_permCollidables0Old;
		m_permCollidables0Old = collidablePos;

		m_externalDeltaTranslation.z = 0; // no up/down movement

		float thresh = 0.1f;
		if (m_externalDeltaTranslation.len2() > thresh * thresh) m_externalDeltaTranslation = Vec3(ZERO);
	}

	for (int i = 0; i < m_nVtx; i++)
	{
		m_particlesCold[i].oldPos = m_particlesHot[i].pos;
	}

	// blend world space with local space movement
	const float rotationBlend = m_config.rotationBlend; //max(m_angModulator, m_config.rotationBlend);
	if ((m_config.rotationBlend > 0.f || m_config.translationBlend > 0.f) && !m_permCollidables.empty())
	{
		Quaternion dq(IDENTITY);
		if (m_config.rotationBlend > 0.f)
		{
			Quaternion qi(m_permCollidables[0].R);
			Quaternion qf(m_permCollidables[0].oldR);
			if (m_config.rotationBlend < 1.f)
				qi.SetNlerp(qf, qi, m_config.rotationBlend);
			dq = qi * !qf;
		}
		Vector4 delta(ZERO);
		if (m_config.translationBlend > 0.f && m_permCollidables.size())
		{
			delta = m_config.translationBlend * (m_permCollidables[0].offset - m_permCollidables[0].oldOffset);
			delta += m_permCollidables[0].oldOffset - dq * m_permCollidables[0].oldOffset;
		}
		for (int i = 0; i < m_nVtx; i++)
		{
			if (!m_particlesCold[i].bAttached)
			{
				m_particlesHot[i].pos = dq * m_particlesHot[i].pos + delta;
				m_particlesCold[i].prevPos = dq * m_particlesCold[i].prevPos + delta;
			}
		}
	}
}

struct SRay
{
	Vector4 origin, dir;
};

struct SIntersection
{
	Vector4 pt, n;
};

struct Quotient
{
	float x, y;

	Quotient(float ax, float ay) : x(ax), y(ay) { }
	float val() { return y != 0 ? x / y : 0; }
	Quotient& operator +=(float op) { x += op * y; return *this; }
};

namespace VClothUtils
{
	inline float      GetMin(float) { return 1E-20f; }
	static ILINE bool operator<(const Quotient&op1, const Quotient&op2)
	{
		return op1.x * op2.y - op2.x * op1.y + GetMin(op1.x) * (op1.x - op2.x) < 0;
	}

	static ILINE bool PrimitiveRayCast(const f32 cr, const f32 ca, const SRay& ray, SIntersection& inters)
	{

		if (ca == 0)
		{
			Vector4 delta = ray.origin;
			float a = ray.dir.len2();
			float b = ray.dir * delta;
			float c = delta.len2() - sqr(cr);
			float d = b * b - a * c;
			if (d >= 0)
			{
				d = sqrt_tpl(d);
				Quotient t(-b - d, a);
				int bHit = isneg(fabs_tpl(t.x * 2 - t.y) - t.y);
				t.x += d * (bHit ^ 1) * 2;
				bHit = isneg(fabs_tpl(t.x * 2 - t.y) - t.y);
				if (!bHit)
					return 0;

				inters.pt = ray.origin + ray.dir * t.val();
				inters.n = (inters.pt).normalized();
				//inters.iFeature[0][0]=inters.iFeature[1][0] = 0x40;
				//inters.iFeature[0][1]=inters.iFeature[1][1] = 0x20;
				return true;
			}
		}

		if (ca)
		{
			//lozenges
			Vector4 axis;
#ifdef CLOTH_SSE
			axis.x = 1, axis.y = 0, axis.z = 0, axis.w = 0;
#else
			axis.x = 1, axis.y = 0, axis.z = 0;
#endif

			Quotient tmin(1, 1);
			int iFeature = -1; // TODO: remove or change
			float a = ray.dir.len2();
			for (int icap = 0; icap < 2; icap++)
			{
				Vector4 vec0 = axis * (ca * (icap * 2 - 1));
				Vector4 delta = ray.origin - vec0;
				float b = ray.dir * delta;
				float c = delta.len2() - sqr(cr);
				float axcdiff = (ray.origin) * axis;
				float axdir = ray.dir * axis;
				float d = b * b - a * c;
				if (d >= 0)
				{
					d = sqrt_tpl(d);
					Quotient t(-b - d, a);
					int bHit = inrange(t.x * tmin.y, 0.f, t.y * tmin.x) & isneg(ca * t.y - fabs_tpl(axcdiff * t.y + axdir * t.x));
					tmin.x += (t.x - tmin.x) * bHit;
					tmin.y += (t.y - tmin.y) * bHit;
					iFeature = 0x41 + icap & -bHit | iFeature & ~- bHit;
					t.x += d * 2;
					bHit = inrange(t.x * tmin.y, 0.f, t.y * tmin.x) & isneg(ca * t.y - fabs_tpl(axcdiff * t.y + axdir * t.x));
					tmin.x += (t.x - tmin.x) * bHit;
					tmin.y += (t.y - tmin.y) * bHit;
					iFeature = 0x41 + icap & -bHit | iFeature & ~- bHit;
				}
			}

			Vector4 vec0 = ray.origin;
			Vector4 vec1 = ray.dir ^ axis;
			a = vec1 * vec1;
			float b = vec0 * vec1;
			float c = vec0 * vec0 - sqr(cr);
			float d = b * b - a * c;
			if (d >= 0)
			{
				d = sqrt_tpl(d);
				Quotient t(-b - d, a);
				int bHit = inrange(t.x * tmin.y, 0.f, t.y * tmin.x) & isneg(fabs_tpl(((ray.origin) * t.y + ray.dir * t.x) * axis) - ca * t.y);
				tmin.x += (t.x - tmin.x) * bHit;
				tmin.y += (t.y - tmin.y) * bHit;
				iFeature = 0x40 & -bHit | iFeature & ~- bHit;
				t.x += d * 2;
				bHit = inrange(t.x * tmin.y, 0.f, t.y * tmin.x) & isneg(fabs_tpl(((ray.origin) * t.y + ray.dir * t.x) * axis) - ca * t.y);
				tmin.x += (t.x - tmin.x) * bHit;
				tmin.y += (t.y - tmin.y) * bHit;
				iFeature = 0x40 & -bHit | iFeature & ~- bHit;
			}

			if (iFeature < 0)
				return 0;

			inters.pt = ray.origin + ray.dir * tmin.val();
			if (iFeature == 0x40)
			{
				inters.n = inters.pt;
				inters.n -= axis * (axis * inters.n);
			}
			else
			{
				inters.n = inters.pt - axis * (ca * ((iFeature - 0x41) * 2 - 1));
			}
			inters.n.normalize();
			//inters.iFeature[0][0]=inters.iFeature[1][0] = iFeature;
			//inters.iFeature[0][1]=inters.iFeature[1][1] = 0x20;
			return true;
		}

		return false;
	}
}

// Implemented according to the paper "Long Range Attachments", Mueller et al.
// but with much better distance constraints by using path finding algorithm and closest neighbors
void CClothSimulator::LongRangeAttachmentsSolve()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	if (m_bUseDijkstraForLRA)
	{
		// Dijekstra / geodesic approach / distances along mesh
		float allowedExtensionSqr = (1.0f + m_config.longRangeAttachmentsAllowedExtension);
		allowedExtensionSqr *= allowedExtensionSqr;
		for (auto it = m_lraNotAttachedOrderedIdx.begin(); it != m_lraNotAttachedOrderedIdx.end(); ++it)
		{
			// better would be to use real length along path for delta, not euklidean distance from initial pose...
			const int i = *it;
			const int& idx = m_particlesHot[i].lraIdx;
			if (idx < 0) continue; // no lra found in initialization
			// determine distance to LRA
			Vector4 lraDistV = m_particlesHot[i].pos - m_particlesHot[idx].pos;
			float lraDistSqr = lraDistV.dot(lraDistV);

			if (lraDistSqr > m_particlesHot[i].lraDist * m_particlesHot[i].lraDist * allowedExtensionSqr)
			{
				// force LRA constraint, by shifting particle in closest neighbor direction
				float delta = sqrt(lraDistSqr) - m_particlesHot[i].lraDist;
				int idxNextClosest = m_particlesHot[i].lraNextParent;
				Vector4 directionClosest = m_particlesHot[idxNextClosest].pos - m_particlesHot[i].pos;
				float distanceClosest = directionClosest.GetLengthFast();
				if (distanceClosest < 0.001f) continue;
				directionClosest = (directionClosest / distanceClosest); //directionClosest.GetNormalizedFast();

				const float moveMaxFactor = m_config.longRangeAttachmentsMaximumShiftFactor;
				const float movePosPrevFactor = m_config.longRangeAttachmentsShiftCollisionFactor; //true; // no velocity change
				if (distanceClosest * moveMaxFactor > delta)
				{
					// move delta in that direction
					m_particlesHot[i].pos += directionClosest * delta * m_particlesHot[i].factorAttached * m_dt;
					if (movePosPrevFactor) m_particlesCold[i].prevPos += directionClosest * delta * movePosPrevFactor * m_particlesHot[i].factorAttached * m_dt;
				}
				else
				{
					// move maximal moveMaxFactor in that direction
					m_particlesHot[i].pos += directionClosest * distanceClosest * moveMaxFactor * m_particlesHot[i].factorAttached * m_dt;
					if (movePosPrevFactor) m_particlesCold[i].prevPos += directionClosest * distanceClosest * moveMaxFactor * movePosPrevFactor * m_particlesHot[i].factorAttached * m_dt;
				}
			}
		}
	}
	else
	{
		// Euclidean distance in local space
		for (int i = 0; i < m_nVtx; i++)
		{
			const int& idx = m_particlesHot[i].lraIdx;
			if (idx < 0) continue; // no LRA for attached vtx

			if (m_particlesCold[i].bAttached) continue; // no lra for attached particles

			// determine distance to LRA
			Vector4 d = m_particlesHot[i].pos - m_particlesHot[idx].pos;
			float distSqr = d.dot(d);

			if (distSqr > m_particlesHot[i].lraDist * m_particlesHot[i].lraDist)
			{
				// force LRA constraint
				d = d.GetNormalizedFast();
				m_particlesHot[i].pos = m_particlesHot[idx].pos + m_particlesHot[i].lraDist * d * m_dt;
			}
		}
	}
}

void CClothSimulator::BendByTriangleAngleDetermineNormals()
{
	const SParticleHot* p = m_particlesHot;

	for (auto it = m_listBendTriangles.begin(); it != m_listBendTriangles.end(); it++)
	{
		SBendTriangle& t = *it;
		t.normal = ((p[t.p1].pos - p[t.p0].pos).cross(p[t.p2].pos - p[t.p0].pos)).GetNormalizedFast();
	}
}

void CClothSimulator::BendByTriangleAngleSolve(float kBend)
{
	BendByTriangleAngleDetermineNormals();

	const float k = kBend;
	SParticleHot* p = m_particlesHot;
	SParticleCold* pO = m_particlesCold;

	for (auto it = m_listBendTrianglePairs.begin(); it != m_listBendTrianglePairs.end(); it++)
	{
		Vec3& n0 = m_listBendTriangles[(*it).idxNormal0].normal;
		Vec3& n1 = m_listBendTriangles[(*it).idxNormal1].normal;

		float alpha = VClothUtils::BendDetermineAngleAtRuntime(n0, n1, p[it->p3].pos, p[it->p0].pos);
		alpha += it->phi0; // add initial angle as offset to constraint this angle

		float factor = k * alpha * 0.01f; // *0.001f; // scale into accurate floating point range

		// add constraint to corner particles
		p[it->p2].pos += n0 * factor * p[it->p2].factorAttached * m_dt;
		p[it->p3].pos += n1 * factor * p[it->p3].factorAttached * m_dt;

		// add inverse constraint to edge particles
		Vec3 nHalf = (n0 + n1).GetNormalizedFast();
		p[it->p0].pos -= nHalf * factor * p[it->p0].factorAttached * m_dt;
		p[it->p1].pos -= nHalf * factor * p[it->p1].factorAttached * m_dt;
	}
}

///////////////////////////////////////////////////////////////////////////////

namespace VClothUtils
{

	static ILINE f32 BoxGetSignedDistance(const Vec3& box, const Vec3& pos)     // signed distance of box
	{
		Vec3 d(abs(pos.x), abs(pos.y), abs(pos.z));
		d -= box;

		float minMax = min(max(d.x, max(d.y, d.z)), 0.0f);

		d.x = max(d.x, 0.0f);
		d.y = max(d.y, 0.0f);
		d.z = max(d.z, 0.0f);

		return minMax + d.len();
	}

	static ILINE f32 ColliderDistance(const SCollidable& coll, const Vec3& pos)     // signed distance of lozenge
	{
		const f32 cr = coll.cr;
		const Vec3 box(coll.cx, coll.cy, coll.cz);

		f32 boxDist = BoxGetSignedDistance(box, pos);

		return boxDist - cr;
	}

	static ILINE bool ColliderDistanceOutsideBoxCheck(const SCollidable& coll, const Vec3& pos)     // quick check, if outside lozenge bounding box
	{
		const float& r = coll.cr;
		if (fabs(pos.z) > coll.cz + r) return true;
		if (fabs(pos.x) > coll.cx + r) return true;
		if (fabs(pos.y) > coll.cy + r) return true;
		return false;
	}

	static ILINE Vec3 ColliderDistanceDirectionNorm(const SCollidable& coll, const Vector4& pos, f32 distance)     // determine normalized direction to surface of lozenge, distance is distance to lozenge-surface at pos
	{
		const f32 eps = 0.001f;
		const Vec3 epsX = Vec3(eps, 0.0f, 0.0f);
		const Vec3 epsY = Vec3(0.0f, eps, 0.0f);
		const Vec3 epsZ = Vec3(0.0f, 0.0f, eps);

		// derive forward
		Vec3 n(
			ColliderDistance(coll, pos + epsX) - distance,
			ColliderDistance(coll, pos + epsY) - distance,
			ColliderDistance(coll, pos + epsZ) - distance);
		return n.GetNormalizedFast();
	}

	static ILINE bool ColliderProjectToSurface(const SCollidable& coll, Vector4& pos, Vector4* normal = nullptr, float factor = 1.0f)     // project pos on surface of lozenge; returns true, if pos is inside coll and has been projected; false otherwise
	{
		if (ColliderDistanceOutsideBoxCheck(coll, pos)) return false;     // quick test, if outside box approximation of lozenge
		f32 distance = ColliderDistance(coll, pos);
		bool isInside = distance < 0;
		if (!isInside) return false;

		const f32 eps = 1.001f;
		if (normal != nullptr)
		{
			*normal = ColliderDistanceDirectionNorm(coll, pos, distance);
			pos -= eps * (*normal) * distance * factor;     // determine surface position
		}
		else
		{
			pos -= eps * ColliderDistanceDirectionNorm(coll, pos, distance) * distance * factor;     // determine surface position
		}
		return true;
	}
}

void CClothSimulator::UpdateCollidablesLerp(f32 t01)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	std::vector<SCollidable>& collidables = m_permCollidables;

	// determine interpolated transformation matrices
	for (size_t k = 0; k < collidables.size(); k++)
	{
		Quaternion qr(collidables[k].R);
		Quaternion qrOld(collidables[k].oldR);

		const float eps = 0.001f;
		collidables[k].qLerp.q = t01 < 1.0f - eps ? Quaternion::CreateSlerp(qrOld, qr, t01) : qr;
		collidables[k].qLerp.t = t01 < 1.0f - eps ? collidables[k].oldOffset + t01 * (collidables[k].offset - collidables[k].oldOffset) : collidables[k].offset;

#ifdef EDITOR_PCDEBUGCODE
		// for debug rendering, only for editor on PC
		if ((m_debugCollidableSubsteppingId == k)) { m_debugCollidableSubsteppingQuatT.push_back(collidables[k].qLerp); }
#endif
	}
}

void CClothSimulator::PositionsProjectToProxySurface(f32 t01)
{
	CRY_PROFILE_REGION(PROFILE_ANIMATION, "CClothSimulator::PositionsProjectToProxySurface");

	std::vector<SCollidable>& collidables = m_permCollidables;
	int colliderId[2];     // special handling for collision with two colliders at the same time, thus store id

	// check all particles
	// for multiple collisions the average of collision resolutions are used
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesCold[i].bAttached) continue; // discard attached particles

		Vector4 collisionResolvePosition(ZERO);
		int nCollisionsDetected = 0; // number of detected collisions
		Vector4 collisionNormal;     // get normal for tangential damping

		const bool instantResolve = true; // directly project onto proxy surface oder iteratively move in that direction

		// check all collidables
		int collId = 0;
		for (auto it = collidables.begin(); it != collidables.end(); it++, collId++)
		{
			const SCollidable& coll = *it;
			const Quaternion& M = coll.qLerp.q;
			Vector4 ipos = (m_particlesHot[i].pos - coll.qLerp.t) * M; // particles position in collider space

			if (instantResolve)
			{
				// instant jump to valid position
				if (VClothUtils::ColliderProjectToSurface(coll, ipos, &collisionNormal))
				{
					if (nCollisionsDetected < 2) { colliderId[nCollisionsDetected] = collId; }
					nCollisionsDetected++;
					ipos = M * ipos + coll.qLerp.t;   // transform back to WS
					collisionResolvePosition += ipos; // set particles new position
				}
			}
			else
			{
				// slow approaching of final collision-resolve-position
				if (VClothUtils::ColliderProjectToSurface(coll, ipos, nullptr, 0.5f)) // approaching with factor 0.5
				{
					ipos = M * ipos + coll.qLerp.t;                           // transform back to WS
					collisionResolvePosition += ipos - m_particlesHot[i].pos; // store sum of delta
					nCollisionsDetected++;
				}
			}
		}

		if (instantResolve)
		{
			switch (nCollisionsDetected)
			{
			case 0:
				break; // no collisions
			case 1:  // one collision
				m_particlesHot[i].pos = collisionResolvePosition;
				//m_particlesCold[i].oldPos = collisionResolvePosition;

				// for attached particles, smooth blending between collision resolve position and skinned position
				if (m_particlesHot[i].alpha) m_particlesHot[i].pos += (m_particlesCold[i].skinnedPos - m_particlesHot[i].pos) * m_particlesHot[i].alpha;

				if (m_config.collisionDampingTangential)
				{
					m_particlesHot[i].collisionExist = true;
					m_particlesHot[i].collisionNormal = collisionNormal;
				}

				break;
			case 2:
				// 2 collisions at the same time: explicitly resolve by half distance into average direction
				if (m_config.collisionMultipleShiftFactor)
				{
					Vector4 collisionResolvePosition(ZERO);
					for (int j = 0; j < nCollisionsDetected; j++)
					{
						const SCollidable& coll = collidables[colliderId[j]];

						const Quaternion& M = coll.qLerp.q;
						Vector4 ipos = (m_particlesHot[i].pos - coll.qLerp.t) * M; // particles position in collider space
						if (VClothUtils::ColliderProjectToSurface(coll, ipos, nullptr, m_config.collisionMultipleShiftFactor))
						{
							ipos = M * ipos + coll.qLerp.t; // transform back to WS
							collisionResolvePosition += ipos - m_particlesHot[i].pos;
						}
					}
					m_particlesHot[i].pos += collisionResolvePosition / 2.0f; // 2 collisions, thus '/ 2.0'f
				}

				// 2 collisions at the same time: collide with union of both colliders / disabled at the moment
				if (false)
				{
					const int id0 = colliderId[0];
					const Quaternion& m0 = collidables[id0].qLerp.q;                        // transformation matrix
					Vector4 ipos0 = (m_particlesHot[i].pos - collidables[id0].offset) * m0; // particles position in collider space
					const float dist0 = VClothUtils::ColliderDistance(collidables[id0], ipos0);

					const int id1 = colliderId[1];
					const Quaternion& m1 = collidables[id1].qLerp.q;                        // transformation matrix
					Vector4 ipos1 = (m_particlesHot[i].pos - collidables[id1].offset) * m1; // particles position in collider space
					const float dist1 = VClothUtils::ColliderDistance(collidables[id1], ipos1);

					if (dist0 < dist1)
					{
						if (VClothUtils::ColliderProjectToSurface(collidables[id0], ipos0))
						{
							m_particlesHot[i].pos = m0 * ipos0 + collidables[id0].offset; // transform back to WS
						}
					}
					else
					{
						if (VClothUtils::ColliderProjectToSurface(collidables[id1], ipos1))
						{
							m_particlesHot[i].pos = m1 * ipos1 + collidables[id1].offset; // transform back to WS
						}
					}
				}
				break;
			default: // more than 2 collisions at the same time - at the moment: do nothing / no resolve
				// m_particlesHot[i].pos = collisionResolvePosition / (float)nCollisionsDetected; // set particles new position by average of possible collision resolutions
				break;
			}
		}
		else
		{
			// slow approaching of final collision-resolve-position
			m_particlesHot[i].pos += collisionResolvePosition / (float)nCollisionsDetected;
		}

	}
}

///////////////////////////////////////////////////////////////////////////////

void CClothSimulator::HandleCameraDistance()
{
	const f32 disableSimAtDist = m_config.disableSimulationAtDistance;

	if (CheckCameraDistanceLessThan(disableSimAtDist))
	{
		if (!IsSimulationEnabled() || IsFadingOut())
		{
			if (!IsFadingOut()) { PositionsSetToSkinned(); /* m_doSkinningForNSteps = 3; */ } // only translate to constraints, if actually is not fading out, otherwise a position jump in cloth would occur in these cases
			EnableSimulation();
			EnableFadeInPhysics();
			SetGpuSkinning(false);
		}
	}
	else
	{
		//m_clothPiece.GetSimulator().EnableSimulation(false); // is done in step now, while handling fading
		if (IsSimulationEnabled() && !IsFadingOut())
		{
			EnableFadeOutPhysics();
		}
	}
}

void CClothSimulator::InitFadeInOutPhysics()
{
	if (m_fadeTimeActual > 0.0f)
	{
		// if fading is already running, everything is already set up
		// only animation step has to be refined, since direction has been changed
		m_fadeTimeActual = m_config.disableSimulationTimeRange - m_fadeTimeActual;
	}
	else
	{
		// fading is not running, thus init step counter and copy initial vertices
		m_fadeTimeActual = m_config.disableSimulationTimeRange;
	}
}

void CClothSimulator::EnableFadeOutPhysics()
{
	InitFadeInOutPhysics();
	m_fadeInOutPhysicsDirection = -1;
}

void CClothSimulator::EnableFadeInPhysics()
{
	InitFadeInOutPhysics();
	m_fadeInOutPhysicsDirection = 1;
}

void CClothSimulator::DecreaseFadeInOutTimer(float dt)
{
	m_fadeTimeActual -= dt;

	// handle end of fading
	if (m_fadeTimeActual <= 0)
	{
		if (m_fadeInOutPhysicsDirection == -1) { EnableSimulation(false); } // disable simulation, if physics has been faded out
		m_fadeInOutPhysicsDirection = 0;
		m_fadeTimeActual = 0.0f;
	}
}

bool CClothSimulator::CheckCameraDistanceLessThan(float dist) const
{
	Vec3 distV = gEnv->p3DEngine->GetRenderingCamera().GetPosition() - m_pAttachmentManager->m_pSkelInstance->m_location.t; // distance vector to camera (animation pivot)
	distV -= m_pAttachmentManager->m_pSkelInstance->GetAABB().GetCenter();                                                  // use center of actual position (determined by BB), not of pivot
	float distSqr = distV.dot(distV);
	return distSqr < dist * dist;
}

bool CClothSimulator::CheckForceSkinningByFpsThreshold()
{
	bool forceSkinning = false;
	float fps = gEnv->pTimer->GetFrameRate();
	forceSkinning = fps < m_config.forceSkinningFpsThreshold;

	// force skinning only after n-th frame with framerate below threshold
	if (forceSkinning && (m_forceSkinningAfterNFramesCounter < Console::GetInst().ca_ClothForceSkinningAfterNFrames))
	{
		m_forceSkinningAfterNFramesCounter++;
		forceSkinning = false;
	}
	else
	{
		m_forceSkinningAfterNFramesCounter = 0;
	}
	
	return forceSkinning;
}

bool CClothSimulator::CheckForceSkinning()
{
	bool forceSkinning = CheckForceSkinningByFpsThreshold(); // detect framerate; force skinning if needed
	forceSkinning |= Console::GetInst().ca_VClothMode == 2;
	forceSkinning |= m_doSkinningForNSteps > 0;
	forceSkinning |= !IsSimulationEnabled();
	forceSkinning |= (m_timeInterval / m_config.timeStep) > (float)m_config.timeStepsMax; // not possible to simulate the actual framerate with the provided max no of substeps
	forceSkinning |= m_config.forceSkinning;
	if (!IsInitialized()) return forceSkinning;
	for (int i = min(m_nVtx, 5); i >= 0; --i)
		forceSkinning |= (m_particlesHot[i].pos - m_particlesCold->prevPos).len2() > m_config.forceSkinningTranslateThreshold * m_config.forceSkinningTranslateThreshold;                                       // check translation distance
	return forceSkinning;
}

bool CClothSimulator::CheckAnimationRewind()
{
	bool animationRewindOccurred = false;
	float normalizedTime = m_pAttachmentManager->m_pSkelInstance->GetISkeletonAnim()->GetAnimationNormalizedTime(&m_pAttachmentManager->m_pSkelInstance->GetISkeletonAnim()->GetAnimFromFIFO(0, 0));
	int isAnimPlaying = m_pAttachmentManager->m_pSkelInstance->m_SkeletonAnim.m_IsAnimPlaying;
	if (isAnimPlaying && (normalizedTime < m_normalizedTimePrev)) // animation rewind has been occurred
	{
		animationRewindOccurred = true;
	}
	m_normalizedTimePrev = normalizedTime;
	return animationRewindOccurred;
}

void CClothSimulator::DoForceSkinning()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	m_doSkinningForNSteps--;
	if (m_doSkinningForNSteps < 0) m_doSkinningForNSteps = 0;

	PositionsSetToSkinned(IsSimulationEnabled());
	UpdateCollidablesLerp();
	PositionsProjectToProxySurface(); // might be removed
	m_dtPrev = m_dt / m_dtNormalize;  // reset m_dtPrev

	for (int i = 0; i < m_nVtx; i++)
		m_particlesHot[i].timer = 0;                                // reset timer -> start resetDamping
}

void CClothSimulator::DoAnimationRewind()
{
	// CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] Animation rewind occured.");
	PositionsSetToSkinned();
	m_doSkinningForNSteps = 3; // fix poor animations with character default pose in 1st frame, which result in strong jump for 2nd frame
	//ProjectToProxySurface(); // deactivated for now, since normal/artist defined skinning should do fine
	m_dtPrev = m_dt / m_dtNormalize;
	for (int i = 0; i < m_nVtx; i++)
		m_particlesHot[i].timer = 0; // reset timer -> start resetDamping
}

void CClothSimulator::DampTangential()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	float k = m_config.collisionDampingTangential;
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesHot[i].collisionExist)
		{
			const Vector4& n = m_particlesHot[i].collisionNormal;
			Vector4 vel = (m_particlesHot[i].pos - m_particlesCold[i].prevPos) / m_dtPrev;

			Vector4 velN = n.dot(vel) * n;
			vel -= velN;
			vel *= 1.0f - k;
			vel += velN;

			m_particlesCold[i].prevPos = m_particlesHot[i].pos - vel * m_dt;
		}
	}
}

void CClothSimulator::PositionsIntegrate()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	const Vector4 dg = (m_gravity * m_config.gravityFactor / 1000.0f) * m_dt; // scale to precise/good floating point domain
	const float resetDampingFactor = 1.0f - m_config.resetDampingFactor;
	const float kd = 1.0f - m_config.friction;
	for (int i = 0; i < m_nVtx; i++)
	{
		Vector4 pos0 = m_particlesHot[i].pos;
		Vector4 dv = (m_particlesHot[i].pos - m_particlesCold[i].prevPos) / m_dtPrev; // determine velocity, using the previous dt, since difference to prePos is used

		if (m_particlesHot[i].timer > 0 && m_particlesHot[i].timer < m_config.resetDampingRange) // damping within resetDampingRange, to smooth hard cloth reset
		{
			dv *= resetDampingFactor;
		}

		dv *= kd;                                                         // simple damping
		Vec3 dxExt = m_externalDeltaTranslation * m_config.externalBlend; // external influence
		m_particlesHot[i].pos += dv * m_dt + dg * m_dt + dxExt;
		m_particlesCold[i].prevPos = pos0;
		m_particlesHot[i].timer++;
	}
}

void CClothSimulator::PositionsPullToSkinnedPositions()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	const float minDist = m_config.maxAnimDistance;
	const float domain = 2 * minDist;
	const Vector4 up(0, 0, 1);
	for (int i = 0; i < m_nVtx; i++)
	{
		if ((m_particlesHot[i].alpha == 0.f && m_config.maxAnimDistance == 0.f) || m_particlesCold[i].bAttached)
			continue;
		Vector4 target = m_particlesCold[i].skinnedPos;
		Vector4 delta = target - m_particlesHot[i].pos;
		float len = delta.len();
		float alpha = 0.0f;
		if (minDist && len > minDist && delta * up < 0.f)
		{
			alpha = min(1.f, max(0.0f, (len - minDist) / domain)); // interpolate in range minDist to 3*minDist from 0..1
		}
		float stiffness = max(alpha, m_config.pullStiffness * m_particlesHot[i].alpha);
		m_particlesHot[i].pos += stiffness * delta * m_dt;
	}
}

void CClothSimulator::PositionsSetToSkinned(bool projectToProxySurface, bool setPosOld)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	// set positions by skinned positions
	for (int n = 0; n < m_nVtx; ++n)
	{
		m_particlesHot[n].pos = m_particlesCold[n].skinnedPos;
	}
	// one single collision step to move particles outside of collision object
	if (projectToProxySurface)
	{
		UpdateCollidablesLerp();
		PositionsProjectToProxySurface();
	}
	// set old positions by new (collided) positions
	if (setPosOld)
	{
		for (int n = 0; n < m_nVtx; ++n)
		{
			m_particlesCold[n].prevPos = m_particlesHot[n].pos;
			m_particlesCold[n].oldPos = m_particlesHot[n].pos;
		}
	}
}

void CClothSimulator::PositionsSetAttachedToSkinnedInterpolated(float t01)
{
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesCold[i].bAttached)
		{
			m_particlesHot[i].pos = m_particlesCold[i].oldPos + t01 * (m_particlesCold[i].skinnedPos - m_particlesCold[i].oldPos); // interpolate substeps for smoother movements
		}
	}
}

int CClothSimulator::Step()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	if (Console::GetInst().ca_VClothMode == 0) return 1; // in case vcloth is disabled by c_var: skip simulation

	// determine dt
	m_dt = m_config.timeStep;
	// m_time starts for substeps with m_timeInterval-m_config.timeStep (might be negative), and ends with 0, split last two timesteps to avoid very small timesteps
	if (m_timeInterval < m_config.timeStep) { m_dt = fmod(m_timeInterval, m_config.timeStep); } // SPF faster than substeps
	else if (m_time < m_config.timeStep)
	{
		m_dt = (fmod(m_timeInterval, m_config.timeStep) + m_config.timeStep) / 2.0f;
		if (m_time > 0) m_time = m_dt;
	}   // split last two substeps into half

	float stepTime01 = 1.0f - m_time / m_timeInterval; // normalized time of substeps for this step, running from 0.0  to 1.0 [per frame]
	stepTime01 = clamp_tpl(stepTime01, 0.0f, 1.0f);
	m_dt *= m_dtNormalize; // normalize dt

#ifdef EDITOR_PCDEBUGCODE
	DebugOutput(stepTime01);
#endif

	// no vertices check, i.e. we are done here...
	if (m_nVtx <= 0)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] No of vertices <= 0");
		return 1;
	}

	// detect animation rewind
	if (m_config.checkAnimationRewind && CheckAnimationRewind()) { DoAnimationRewind(); return 1; }

	// check, if skinning should be forced
	if (CheckForceSkinning())
	{ 
		DoForceSkinning();
		return 1;
	}

	// always update attached vertices to skinned positions; even if not stepping
	PositionsSetAttachedToSkinnedInterpolated(stepTime01);

	// simulation
	m_dtPrev *= m_dtNormalize; // ensure normalization scale
	if (IsSimulationEnabled())
	{
		// clever damping - following paper 'position based dynamics'
		DampPositionBasedDynamics();

		// damping of spring edges
		const f32 springDamping = m_config.springDamping;
		if (springDamping)
		{
			CRY_PROFILE_REGION(PROFILE_ANIMATION, "CClothSimulator::Step::Damping");
			for (int i = 0; i < m_nEdges; i++)
			{
				DampEdge(m_links[i], springDamping);
			}                                                                          // stretch edges
			for (auto it = m_bendLinks.begin(); it != m_bendLinks.end(); ++it)
			{
				DampEdge(*it, springDamping);
			}                                                                                                    // bend edges
			for (auto it = m_shearLinks.begin(); it != m_shearLinks.end(); ++it)
			{
				DampEdge(*it, springDamping);
			}                                                                                                     // shear edges
		}

		// integrate / positions update
		PositionsIntegrate(); // integrate positions here, to ensure collisions are handled correctly at the end of each substep

		// pull towards skinned positions
		if (m_config.pullStiffness || m_config.maxAnimDistance) { PositionsPullToSkinnedPositions(); }

		// damping of colliding particles
		// clear collisions for damping; so after the solver collisions/stiffness, the tangential collisions are handled
		if (m_config.collisionDampingTangential)
		{
			for (int i = 0; i < m_nVtx; i++)
				m_particlesHot[i].collisionExist = false;
		}

		// solver collisions/stiffness
		float stretchStiffness = m_config.stretchStiffness;
		float shearStiffness = m_config.shearStiffness;
		float bendStiffness = m_config.bendStiffness;
		float bendStiffnessByTrianglesAngle = -m_config.bendStiffnessByTrianglesAngle / 10.0f;

		CRY_PROFILE_REGION(PROFILE_ANIMATION, "CClothSimulator::Step::ConstraintsAndCollisions");

		// interpolate transformation of collisionProxies
		UpdateCollidablesLerp(stepTime01);

		// constraint solver
		for (int iter = 0; iter < m_config.numIterations; iter++)
		{
			// long range attachments
			if (m_config.longRangeAttachments) LongRangeAttachmentsSolve();

			// solve springs - stretching, bending & shearing
			{
				CRY_PROFILE_REGION(PROFILE_ANIMATION, "CClothSimulator::Step::SolveEdges");
				if (stretchStiffness)
				{
					for (int i = 0; i < m_nEdges; i++)
					{
						SolveEdge(m_links[i], stretchStiffness);
					}
				}
				if (shearStiffness)
				{
					for (auto it = m_shearLinks.begin(); it != m_shearLinks.end(); ++it)
					{
						SolveEdge(*it, shearStiffness);
					}
				}
				if (bendStiffness)
				{
					for (auto it = m_bendLinks.begin(); it != m_bendLinks.end(); ++it)
					{
						SolveEdge(*it, bendStiffness);
					}
				}
				if (bendStiffnessByTrianglesAngle) { BendByTriangleAngleSolve(bendStiffnessByTrianglesAngle); }
			}

			if (m_config.springDampingPerSubstep && springDamping)
			{
				CRY_PROFILE_REGION(PROFILE_ANIMATION, "CClothSimulator::Step::DampEdges");
				// only damp  stretch links here, in the collision/stiffness loop
				for (int i = 0; i < m_nEdges; i++)
				{
					DampEdge(m_links[i], springDamping);
				}                                                                           // stretch edges
				//for (auto it = m_bendLinks.begin(); it != m_bendLinks.end(); ++it) { DampEdge(*it, springDamping); } // bend edges
				//for (auto it = m_shearLinks.begin(); it != m_shearLinks.end(); ++it) { DampEdge(*it, springDamping); } // shear edges
			}

			// collision handling - project particles lying inside collision proxies onto collision proxies surfaces
			if (m_config.collideEveryNthStep)   // ==0 means no collision
			{
				if ((iter % m_config.collideEveryNthStep == 0) || (iter == m_config.numIterations - 1)) { PositionsProjectToProxySurface(stepTime01); }
			}
		}

		// tangential damping in case of collisions
		if (m_config.collisionDampingTangential) DampTangential();

	} // endif (IsSimulationEnabled())

	// time stepping
	m_dtPrev = m_dt / m_dtNormalize;
	if (m_time == 0) { return 1; } // stop, if frame-dt reached exactly
	m_time -= m_config.timeStep;
	if (m_time < 0) // ensure, that the actual frame-dt will be reached exactly in the next step
	{
		m_time = 0;
		return 0; // one more loop for m_time=0, eq. stepTime01=1 to reach exact frametime
	}
	m_steps++;

	return 0; // continue substepping
}

void CClothSimulator::LaplaceFilterPositions(Vector4* positions, float intensity)
{
	Vector4*& pos = positions;

	m_listLaplacePosSum.clear();
	m_listLaplaceN.clear();
	m_listLaplacePosSum.resize(m_nVtx);
	m_listLaplaceN.resize(m_nVtx);

	// clear laplace-offset posSum & N
	for (auto it = m_listLaplacePosSum.begin(); it != m_listLaplacePosSum.end(); ++it)
	{
		it->zero();
	}
	for (auto it = m_listLaplaceN.begin(); it != m_listLaplaceN.end(); ++it)
	{
		*it = 0.0f;
	}

	// determine laplace-offset dx
	for (int i = 0; i < m_nEdges; i++)
	{
		int idx0 = m_links[i].i1;
		int idx1 = m_links[i].i2;
		m_listLaplacePosSum[idx0] += pos[idx1];
		m_listLaplaceN[idx0] += 1.0f;                                         // add neighbor position
		m_listLaplacePosSum[idx1] += pos[idx0];
		m_listLaplaceN[idx1] += 1.0f;                                         // add neighbor position
	}

	// add laplace-offset dx
	for (int i = 0; i < m_nVtx; i++)
	{
		if (!m_particlesCold[i].bAttached) // don't smooth attached positions
		{
			Vec3 dxPos = intensity * (m_listLaplacePosSum[i] / m_listLaplaceN[i] - pos[i]);
			pos[i] += dxPos;
		}
	}

	// UpdateCollidablesLerp(); ProjectToProxySurface(); // one step of collision projection
}

void CClothSimulator::DebugOutput(float stepTime01)
{
	switch (m_config.debugPrint)
	{
	case 1:
	{
		float offs = stepTime01 * 150;
		g_pAuxGeom->Draw2dLabel(100, 80 + offs, 2.3f, ColorF(1, 0, 0, 1), false, "dt:%4.3f", m_dt / m_dtNormalize);
		g_pAuxGeom->Draw2dLabel(300, 80 + offs, 2.3f, ColorF(1, 0, 0, 1), false, "dtP:%4.3f", m_dtPrev);
		g_pAuxGeom->Draw2dLabel(500, 80 + offs, 2.3f, ColorF(0, 1, 0, 1), false, "stepTime01:  %4.3f", stepTime01);
		if (stepTime01 > 0.9999f) g_pAuxGeom->Draw2dLabel(500, 80, 2.3f, ColorF(0, 1, 0, 1), false, "stepTime01:  0");
		g_pAuxGeom->Draw2dLabel(10, 80 + offs, 2.3f, ColorF(0, 1, 0, 1), false, "No:%i", (int)(m_timeInterval / m_config.timeStep + 0.999f));
	};
	break;
	case 2:
		m_dtPrev = m_dt / m_dtNormalize;
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "[Cloth] Force m_dtPrev = m_dt = %f", m_dt / m_dtNormalize);
		break;
	case 3:
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "[Cloth] GlobalLocatorPos: %f, %f, %f", m_location.t.x, m_location.t.y, m_location.t.z); // global position of character (locator)
		break;
	}
}

void CClothSimulator::DrawHelperInformation()
{
	if (m_nVtx <= 0) return;
	float sphereRadius = this->GetParams().debugDrawVerticesRadius;
	IRenderAuxGeom* pRendererAux = gEnv->pRenderer->GetIRenderAuxGeom();
	Vec3 offs = m_pAttachmentManager->m_pSkelInstance->m_location.t;

	ColorF colorRed(1.0f, 0, 0);
	ColorF colorGreen(0, 1.0f, 0);
	ColorF colorBlue(0, 0, 1.0f);
	ColorF colorWhite(1.0f, 1.0f, 1.0f);
	// debug draw: vertices
	if (sphereRadius > 0.0f)
	{
		for (int i = 0; i < m_nVtx; i++)
		{
			//ColorB colorDynamic((int)(255 * m_particlesHot[i].alpha), (int)(128 * m_particlesHot[i].alpha), (int)(255 - 128 * m_particlesHot[i].alpha));
			float t = m_particlesHot[i].alpha;
			ColorF c = colorBlue * t + colorGreen * (1.0f - t);
			pRendererAux->DrawSphere(SseToVec3(m_particlesHot[i].pos) + offs, sphereRadius, m_particlesCold[i].bAttached ? colorRed : c);
		}
	}

	// debug draw long range attachments
	switch (this->GetParams().debugDrawLRA)
	{
	default:
	case 0: // draw nothing
		break;
	case 1: // draw closest attachment
		for (int i = 0; i < m_nVtx; i++)
		{
			const int lraIdx = m_particlesHot[i].lraIdx;
			if (lraIdx >= 0)
				pRendererAux->DrawLine(SseToVec3(m_particlesHot[i].pos) + offs, colorBlue, SseToVec3(m_particlesHot[lraIdx].pos) + offs, colorRed, 1.0f);
			else // draw
				pRendererAux->DrawLine(SseToVec3(m_particlesHot[i].pos) + offs, colorRed, SseToVec3(m_particlesHot[i].pos + Vector4(5.0f, 0, 0) + offs), colorRed, 4.0f);
		}
		break;
	case 2: // draw closest attachment ordered
		for (int i = 0; i < m_lraNotAttachedOrderedIdx.size(); i++)
		{
			const int idx = m_lraNotAttachedOrderedIdx[i];
			const int lraIdx = m_particlesHot[idx].lraIdx;
			float t = (float)i / (float)m_lraNotAttachedOrderedIdx.size();
			ColorF c = colorRed * t + colorBlue * (1.0f - t);
			if (lraIdx >= 0)
				pRendererAux->DrawLine(SseToVec3(m_particlesHot[idx].pos) + offs, c, SseToVec3(m_particlesHot[lraIdx].pos) + offs, c, 4.0f);
			else // draw problem
				pRendererAux->DrawLine(SseToVec3(m_particlesHot[idx].pos) + offs, colorRed, SseToVec3(m_particlesHot[idx].pos + Vector4(5.0f, 0, 0) + offs), colorRed, 4.0f);
		}
		break;
	case 3: // draw neighbor which is closest to attachment
		for (int i = 0; i < m_lraNotAttachedOrderedIdx.size(); i++)
		{
			const SParticleHot& prt = m_particlesHot[m_lraNotAttachedOrderedIdx[i]];
			const Vector4 p0 = prt.pos + offs;
			if (prt.lraNextParent >= 0)
			{
				const Vector4 p1 = m_particlesHot[prt.lraNextParent].pos + offs;
				float t = (float)i / (float)m_lraNotAttachedOrderedIdx.size();
				ColorF c = colorRed * t + colorBlue * (1.0f - t);
				pRendererAux->DrawLine(SseToVec3(p0), c, SseToVec3(p1), c, 4.0f);
			}
			else // draw problem
				pRendererAux->DrawLine(SseToVec3(p0), colorRed, SseToVec3(p0 + Vector4(5.0f, 0, 0)), colorRed, 4.0f);
		}
		break;
	case 4: // draw particle 0
		pRendererAux->DrawSphere(SseToVec3(m_particlesHot[0].pos) + offs, sphereRadius * 5.0f, colorWhite);
		break;
	}

	// debug draw: cloth links
	float scale = 1.0f;
	switch (this->GetParams().debugDrawCloth)
	{
	case 0:
		break;
	default:
	case 1: // draw stretch links
		for (int i = 0; i < m_nEdges; i++)
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_links[i].i1].pos) + offs, colorGreen, SseToVec3(m_particlesHot[m_links[i].i2].pos) + offs, colorGreen);
		break;
	case 2: // draw shear links
		for (int i = 0; i < m_shearLinks.size(); i++)
		{
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_shearLinks[i].i1].pos) + offs, colorGreen, SseToVec3(m_particlesHot[m_shearLinks[i].i2].pos) + offs, colorGreen);
		}
		break;
	case 3: // draw bend links
		for (int i = 0; i < m_bendLinks.size(); i++)
		{
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_bendLinks[i].i1].pos) + offs, colorGreen, SseToVec3(m_particlesHot[m_bendLinks[i].i2].pos) + offs, colorGreen);
		}
		break;
	case 4: // draw all links
		for (int i = 0; i < m_nEdges; i++)
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_links[i].i1].pos) + offs, colorRed, SseToVec3(m_particlesHot[m_links[i].i2].pos) + offs, colorRed);
		for (int i = 0; i < m_shearLinks.size(); i++)
		{
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_shearLinks[i].i1].pos) + offs, colorGreen, SseToVec3(m_particlesHot[m_shearLinks[i].i2].pos) + offs, colorGreen);
		}
		for (int i = 0; i < m_bendLinks.size(); i++)
		{
			pRendererAux->DrawLine(SseToVec3(m_particlesHot[m_bendLinks[i].i1].pos) + offs, colorBlue, SseToVec3(m_particlesHot[m_bendLinks[i].i2].pos) + offs, colorBlue);
		}
		break;
	case 5: // draw contact points
	// for (int i = 0; i < m_nContacts; i++) { pRendererAux->DrawSphere(SseToVec3(m_contacts[i].pt) + offs, sphereRadius, colorGreen); } break;
	case 6: // draw all positions
		for (int i = 0; i < m_nVtx; ++i)
		{
			pRendererAux->DrawSphere(SseToVec3(m_particlesHot[i].pos) + offs, sphereRadius, colorWhite);
			pRendererAux->DrawSphere(SseToVec3(m_particlesCold[i].prevPos) + offs, sphereRadius, colorGreen);
			pRendererAux->DrawSphere(SseToVec3(m_particlesCold[i].oldPos) + offs, sphereRadius, colorBlue);
			pRendererAux->DrawSphere(SseToVec3(m_particlesCold[i].skinnedPos) + offs, sphereRadius, colorRed);
		}
		break;
	case 10:
		scale *= 0.5;
	case 9:
		scale *= 0.5;
	case 8:
		scale *= 0.5;
	case 7: // draw bendByTriangleAngles - helper
		// normals
		BendByTriangleAngleDetermineNormals();
		for (auto it = m_listBendTriangles.begin(); it != m_listBendTriangles.end(); it++)
		{
			Vector4 p = (m_particlesHot[it->p0].pos + m_particlesHot[it->p1].pos + m_particlesHot[it->p2].pos) / 3.0f; // center of triangle
			pRendererAux->DrawLine(SseToVec3(p) + offs, colorRed, SseToVec3(p + it->normal * scale) + offs, colorBlue);
		}
		// shared edges
		for (auto it = m_listBendTrianglePairs.begin(); it != m_listBendTrianglePairs.end(); it++)
		{
			Vector4& e0 = m_particlesHot[it->p0].pos;
			Vector4& e1 = m_particlesHot[it->p1].pos;
			pRendererAux->DrawLine(SseToVec3(e0) + offs, colorRed, SseToVec3(e1) + offs, colorRed);
		}
		break;
	}

	// draw specific particle
	if (m_config.debugPrint < 0)
	{
		int idx = -m_config.debugPrint;
		if ((idx >= 0) && (idx < m_nVtx))
		{
			pRendererAux->DrawSphere(SseToVec3(m_particlesHot[idx].pos) + offs, sphereRadius * 5.0f, colorWhite);
		}
	}

	// draw proxies substeps - for one  single proxie
	std::vector<SCollidable>& c = m_permCollidables;
	int noOfProxies = c.size();
	if ((m_config.debugPrint >= 100) && (noOfProxies > 0))
	{
		int proxyId = m_config.debugPrint - 100;
		proxyId = proxyId < 0 ? 0 : proxyId;
		if (proxyId >= noOfProxies) { proxyId = noOfProxies - 1; }
		m_debugCollidableSubsteppingId = proxyId;

		// search n-th cloth proxy in all proxies
		uint32 numAllProxies = m_pAttachmentManager->GetProxyCount();
		int numClothProxy = -1;
		CProxy* pProxy = nullptr;
		for (uint32 i = 0; i < numAllProxies; i++)
		{
			if (numClothProxy == proxyId) break;                       // already found
			if (m_pAttachmentManager->m_arrProxies[i].m_nPurpose == 1) // ==1: cloth
			{
				pProxy = const_cast<CProxy*>(&m_pAttachmentManager->m_arrProxies[i]);
				numClothProxy++;
			}
		}
		if (pProxy)
		{
			QuatTS wlocation = m_location * pProxy->m_ProxyModelRelative;

			for (int i = 0; i < m_debugCollidableSubsteppingQuatT.size(); i++)
			{
				QuatT q = m_debugCollidableSubsteppingQuatT[i];
				//color by time (getting brighter)
				float i01 = (i + 1.0f) / m_debugCollidableSubsteppingQuatT.size();
				int i0255 = (int)(i01 * 255.0f);
				unsigned char c = i0255;
				pProxy->Draw(q, RGBA8(c, c, c, 0xff), 16, Vec3(0, 0, 1));
			}
		}
	}
	m_debugCollidableSubsteppingQuatT.clear(); // clear for the next debug render passd
}

namespace VClothUtils
{
	static ILINE Matrix3 PositionBasedDynamicsDampingDetermineR(Vector4 v)
	{
		Matrix3 r;
#ifdef CLOTH_SSE
		r.row1.Set(0, -v.z, v.y);
		r.row2.Set(v.z, 0, -v.x);
		r.row3.Set(-v.y, v.x, 0);
#else
		r.m00 = 0.f;
		r.m01 = -v.z;
		r.m02 = v.y;
		r.m10 = v.z;
		r.m11 = 0.f;
		r.m12 = -v.x;
		r.m20 = -v.y;
		r.m21 = v.x;
		r.m22 = 0.f;
#endif
		return r;
	}
}
// Implemented after the damping method presented in "Position Based Dynamics" by Mueller et al.; Section 3.5.
// See also: https://code.google.com/p/opencloth/source/browse/trunk/OpenCloth_PositionBasedDynamics/OpenCloth_PositionBasedDynamics/main.cpp
void CClothSimulator::DampPositionBasedDynamics()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	if (!m_config.rigidDamping) return;

	Vector4 xcm(ZERO);
	int num = 0;
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesCold[i].bAttached) continue;
		xcm += m_particlesHot[i].pos;
		num++;
	}
	if (num == 0)
	{
		CryLog("[Character cloth] All vertices are attached");
		return;
	}
	xcm *= 1.f / (float)num;

	Vector4 vcm(ZERO);
	Vector4 L(ZERO);
	Matrix3 I;
	I.SetZero();
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesCold[i].bAttached) continue;
		Vector4 r = m_particlesHot[i].pos - xcm;
		Matrix3 rMat = VClothUtils::PositionBasedDynamicsDampingDetermineR(r); // equals matrix (~r), see paper Mueller et al
		I -= rMat * rMat;                                                      // equals: I = Sum( rMat * rMat.transpose() ), see paper Mueller et al
		Vector4 vel = (m_particlesHot[i].pos - m_particlesCold[i].prevPos) / m_dtPrev;
		vcm += vel;
		L += r ^ vel; // equals: L = Sum( rXv )
	}
	vcm *= 1.f / num;

	Vector4 omega(ZERO);
	float det = I.Determinant();
	if (fabs(det) == 0.f) // is invertible?
	{
		// CryLog("[Character cloth] Singular matrix"); // matrix sometimes not invertible - without big side-effects, so no message anymore
		return;
	}
	omega = I.GetInverted() * L;

	float kd = m_config.rigidDamping;
	for (int i = 0; i < m_nVtx; i++)
	{
		if (m_particlesCold[i].bAttached) continue;
		Vector4 r = m_particlesHot[i].pos - xcm;
		Vector4 v = vcm + (omega ^ r);
		Vector4 vel = (m_particlesHot[i].pos - m_particlesCold[i].prevPos) / m_dtPrev;
		Vector4 dv = v - vel;
		v = vel + kd * dv;
		if (vel.z < 0.f) v.z = vel.z;                                  // keep downward velocity unchanged
		m_particlesCold[i].prevPos = m_particlesHot[i].pos - v * m_dt; // update velocity
	}
}

struct VertexCommandClothSkin : public VertexCommand
{
public:
	VertexCommandClothSkin()
		: VertexCommand((VertexCommandFunction)Execute)
		, pTransformations(nullptr)
		, transformationCount(0)
		, pClothPiece(nullptr)
	{}
public:
	static void Execute(VertexCommandClothSkin& command, CVertexData& vertexData)
	{
		CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);
		//		if (!command.pClothPiece->m_bSingleThreaded)
		command.pClothPiece->UpdateSimulation(command.pTransformations, command.transformationCount);

		if (command.pVertexPositionsPrevious)
		{
			command.pClothPiece->SkinSimulationToRenderMesh<true>(command.pClothPiece->m_currentLod, vertexData, command.pVertexPositionsPrevious);
		}
		else
		{
			command.pClothPiece->SkinSimulationToRenderMesh<false>(command.pClothPiece->m_currentLod, vertexData, NULL);
		}
	}

public:
	const DualQuat* pTransformations;
	uint transformationCount;
	CClothPiece* pClothPiece;
	strided_pointer<const Vec3> pVertexPositionsPrevious;
};

bool CClothSimulator::GetMetaData(mesh_data* pMesh, CSkin* pSimSkin)
{
	// get loaded LRA data
	{
		std::vector<AttachmentVClothPreProcessLra> const& loadedLra = pSimSkin->GetVClothData().m_lra;
		std::vector<int> const& loadedLraNotAttachedOrderedIdx = pSimSkin->GetVClothData().m_lraNotAttachedOrderedIdx;

		if (loadedLra.size() != m_nVtx) return false;
		{
			int i = 0;
			for (auto it = loadedLra.begin(); it != loadedLra.end(); ++it, ++i)
			{
				m_particlesHot[i].lraDist = it->lraDist;
				m_particlesHot[i].lraIdx = it->lraIdx;
				m_particlesHot[i].lraNextParent = it->lraNextParent;
			}
		}

		m_lraNotAttachedOrderedIdx.resize(loadedLraNotAttachedOrderedIdx.size());
		std::copy(loadedLraNotAttachedOrderedIdx.begin(), loadedLraNotAttachedOrderedIdx.end(), m_lraNotAttachedOrderedIdx.begin());
	}

	// get loaded bending information
	{
		std::vector<SBendTrianglePair> const& loadedListBendTrianglePair = pSimSkin->GetVClothData().m_listBendTrianglePairs;
		std::vector<SBendTriangle> const& loadedListBendTriangle = pSimSkin->GetVClothData().m_listBendTriangles;

		m_listBendTrianglePairs.resize(loadedListBendTrianglePair.size());
		std::copy(loadedListBendTrianglePair.begin(), loadedListBendTrianglePair.end(), m_listBendTrianglePairs.begin());

		m_listBendTriangles.resize(loadedListBendTriangle.size());
		std::copy(loadedListBendTriangle.begin(), loadedListBendTriangle.end(), m_listBendTriangles.begin());
	}

	// get loaded links
	{
		std::vector<SLink> const* loadedLinks = pSimSkin->GetVClothData().m_links;

		int n = 0;
		if (m_links != nullptr) delete[] m_links;
		m_links = new SLink[loadedLinks[n].size()];
		std::copy(loadedLinks[n].begin(), loadedLinks[n].end(), m_links);
		m_nEdges = loadedLinks[n].size();

		n = 1;
		m_shearLinks.resize(loadedLinks[n].size());
		std::copy(loadedLinks[n].begin(), loadedLinks[n].end(), m_shearLinks.begin());

		n = 2;
		m_bendLinks.resize(loadedLinks[n].size());
		std::copy(loadedLinks[n].begin(), loadedLinks[n].end(), m_bendLinks.begin());
	}
	return true;
}

void CClothSimulator::GenerateMetaData(mesh_data* pMesh, CSkin* pSimSkin, float* weights)
{
	// setup preproc
	int nVtx = pMesh->nVertices;

	std::vector<Vec3> vtx;
	vtx.resize(nVtx);
	if (vtx.size() == 0) return;
	int i = 0;
	for (auto it = vtx.begin(); it != vtx.end(); ++i, ++it)
	{
		(*it) = pMesh->pVertices[i]; // pMesh->pVertices is strided pointer
	}

	std::vector<int> idx;
	idx.resize(pMesh->nTris * 3);
	if (idx.size() == 0) return;
	std::copy(pMesh->pIndices, pMesh->pIndices + idx.size(), idx.begin());

	std::vector<bool> attached;
	attached.resize(nVtx);
	for (int i = 0; i < nVtx; i++)
	{
		attached[i] = (weights != nullptr) && AttachmentVClothPreProcess::IsAttached(weights[i]) ? true : false;
	}

	// call preproc
	AttachmentVClothPreProcess pre;
	pre.PreProcess(vtx, idx, attached);

	// read back generated meta-data

	// get generated LRA data
	{
		std::vector<AttachmentVClothPreProcessLra> const& preLra = pre.GetLra();
		std::vector<int> const& preLraNotAttachedOrderedIdx = pre.GetLraNotAttachedOrderedIdx();
		{
			int i = 0;
			for (auto it = preLra.begin(); it != preLra.end(); ++it, ++i)
			{
				m_particlesHot[i].lraDist = it->lraDist;
				m_particlesHot[i].lraIdx = it->lraIdx;
				m_particlesHot[i].lraNextParent = it->lraNextParent;
			}
		}

		m_lraNotAttachedOrderedIdx.resize(preLraNotAttachedOrderedIdx.size());
		std::copy(preLraNotAttachedOrderedIdx.begin(), preLraNotAttachedOrderedIdx.end(), m_lraNotAttachedOrderedIdx.begin());
	}

	// get loaded bending information
	{
		std::vector<SBendTrianglePair> const& preListBendTrianglePair = pre.GetListBendTrianglePair();
		std::vector<SBendTriangle> const& preListBendTriangle = pre.GetListBendTriangle();

		m_listBendTrianglePairs.resize(preListBendTrianglePair.size());
		std::copy(preListBendTrianglePair.begin(), preListBendTrianglePair.end(), m_listBendTrianglePairs.begin());

		m_listBendTriangles.resize(preListBendTriangle.size());
		std::copy(preListBendTriangle.begin(), preListBendTriangle.end(), m_listBendTriangles.begin());
	}

	// get generated links
	{
		std::vector<SLink> const& preLinksStretch = pre.GetLinksStretch();
		std::vector<SLink> const& preLinksShear = pre.GetLinksShear();
		std::vector<SLink> const& preLinksBend = pre.GetLinksBend();

		if (m_links != nullptr) delete[] m_links;
		m_links = new SLink[preLinksStretch.size()];
		std::copy(preLinksStretch.begin(), preLinksStretch.end(), m_links);
		m_nEdges = preLinksStretch.size();

		m_shearLinks.resize(preLinksShear.size());
		std::copy(preLinksShear.begin(), preLinksShear.end(), m_shearLinks.begin());

		m_bendLinks.resize(preLinksBend.size());
		std::copy(preLinksBend.begin(), preLinksBend.end(), m_bendLinks.begin());
	}
}

bool CClothPiece::Initialize(const CAttachmentVCLOTH* pVClothAttachment)
{
	if (m_initialized)
		return true;

	if (pVClothAttachment == 0)
		return false;

	if (pVClothAttachment->GetType() != CA_VCLOTH)
		return false;

	if (!pVClothAttachment->m_pRenderSkin)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "[Cloth] Skin render attachment '%s' has no model mesh.", pVClothAttachment->GetName());
		return false;
	}

	m_pVClothAttachment = (CAttachmentVCLOTH*)pVClothAttachment;
	m_simulator.SetAttachmentManager(pVClothAttachment->m_pAttachmentManager);

	CSkin* pSimSkin = m_pVClothAttachment->m_pSimSkin;
	CSkin* pRenderSkin = m_pVClothAttachment->m_pRenderSkin;

	if (pSimSkin == 0 || pSimSkin->GetIRenderMesh(0) == 0 || pSimSkin->GetIRenderMesh(0)->GetVerticesCount() == 0)
		return false;

	if (pRenderSkin == 0 || pRenderSkin->GetIRenderMesh(0) == 0 || pRenderSkin->GetIRenderMesh(0)->GetVerticesCount() == 0)
		return false;

	m_numLods = 0;
	_smart_ptr<IRenderMesh> pRenderMeshes[2];
	for (int lod = 0; lod < SClothGeometry::MAX_LODS; lod++)
	{
		pRenderMeshes[lod] = m_pVClothAttachment->m_pRenderSkin->GetIRenderMesh(lod);
		if (pRenderMeshes[lod])
		{
			m_numLods++;
		}
	}
	if (!m_numLods)
		return false;

	//	m_bSingleThreaded = context.bSingleThreaded;
	m_bAlwaysVisible = m_simulator.GetParams().isMainCharacter;

	CModelMesh* pSimModelMesh = pSimSkin->GetModelMesh(0);
	if (!pSimModelMesh)
		return false;
	pSimModelMesh->InitSWSkinBuffer();

	m_clothGeom = g_pCharacterManager->LoadVClothGeometry(*m_pVClothAttachment, pRenderMeshes);
	if (!m_clothGeom)
		return false;

	// init simulator
	m_simulator.AddGeometry(m_clothGeom->pPhysGeom);

	bool hasMetaData = false;
	if (pSimSkin->HasVCloth())
	{
		// CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] Loaded VCloth from file...");
		hasMetaData = m_simulator.GetMetaData((mesh_data*)m_clothGeom->pPhysGeom->pGeom->GetData(), pSimSkin);
	}
	if (!pSimSkin->HasVCloth() || !hasMetaData)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[Cloth] VCloth metadata is not stored in character file. Please regenerate skin with RessourceCompiler and cloth flag set. Metadata is generated on the fly (expensive at runtime).");
		m_simulator.GenerateMetaData((mesh_data*)m_clothGeom->pPhysGeom->pGeom->GetData(), pSimSkin, &m_clothGeom->weights[0]);
	}
	m_simulator.SetParams(m_simulator.GetParams(), &m_clothGeom->weights[0]); // determine links weights

	m_simulator.SetInitialized();
	m_initialized = true;

	return true;
}

void CClothPiece::Dettach()
{
	if (!m_pVClothAttachment)
		return;

	m_simulator.SetInitialized(false);
	WaitForJob(false);

	// make attachment GPU skinned
	uint32 flags = m_pVClothAttachment->GetFlags();
	flags &= ~FLAGS_ATTACH_SW_SKINNING;
	m_pVClothAttachment->SetFlags(flags);
	m_pVClothAttachment->m_vertexAnimation.SetClothData(NULL);
	m_pVClothAttachment = NULL;
}

void CClothPiece::WaitForJob(bool bPrev)
{
	// wait till the SW-Skinning job has finished
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	if (bPrev)
		nFrameID--;
	int nList = nFrameID % 3;
	if (m_pVClothAttachment->m_arrSkinningRendererData[nList].nFrameID == nFrameID && m_pVClothAttachment->m_arrSkinningRendererData[nList].pSkinningData)
	{
		if (m_pVClothAttachment->m_arrSkinningRendererData[nList].pSkinningData->pAsyncDataJobs)
		{
			gEnv->pJobManager->WaitForJob(*m_pVClothAttachment->m_arrSkinningRendererData[nList].pSkinningData->pAsyncDataJobs);
		}
	}
}

bool CClothPiece::PrepareCloth(CSkeletonPose& skeletonPose, const Matrix34& worldMat, bool visible, int lod)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	m_pCharInstance = skeletonPose.m_pInstance;
	m_lastVisible = visible;

	if (!visible) // no need to do the rest if falling back to GPU skinning
	{
		return false;
	}

	WaitForJob(true);

	// get working buffers from the pool
	if (m_buffers != NULL)
	{
		CryLog("[Character Cloth] the previous job is not done: %s - %s", m_pVClothAttachment->GetName(), skeletonPose.m_pInstance->GetFilePath());
		assert(false);
		return false;
	}
	if (m_poolIdx < 0)
	{
		m_poolIdx = m_clothGeom->GetBuffers();
		if (m_poolIdx < 0)
		{
			return false;
		}
	}

	m_currentLod = min(lod, m_numLods - 1);
	m_charLocation = QuatT(worldMat); // TODO: direct conversion

	// 	if (m_bSingleThreaded)
	// 	{
	// 		// software skin the sim mesh
	// 		SSkinningData* pSkinningData = m_pSimAttachment->GetVertexTransformationData(true,lod);
	// 		gEnv->pJobManager->WaitForJob( *pSkinningData->pAsyncJobs ); // stall still the skinning related jobs have finished
	// 		UpdateSimulation(pSkinningData->pBoneQuatsS, pSkinningData->nNumBones);		
	// 	}
//	m_pRenderAttachment->m_vertexAnimation.SetClothData(this);

	return true;
}

void CClothPiece::DrawDebug(const SVertexAnimationJob* pVertexAnimation)
{
	// wait till the SW-Skinning jobs have finished
	while (*pVertexAnimation->pRenderMeshSyncVariable)
		CrySleep(1);
	m_simulator.DrawHelperInformation();
}

void CClothPiece::SetClothParams(const SVClothParams& params)
{
	m_simulator.SetParams(params);
}

const SVClothParams& CClothPiece::GetClothParams()
{
	return m_simulator.GetParams();
}

bool CClothPiece::CompileCommand(SVertexSkinData& skinData, CVertexCommandBuffer& commandBuffer)
{
	VertexCommandClothSkin* pCommand = commandBuffer.AddCommand<VertexCommandClothSkin>();
	if (!pCommand)
		return false;

	pCommand->pTransformations = skinData.pTransformations;
	pCommand->transformationCount = skinData.transformationCount;
	pCommand->pVertexPositionsPrevious = skinData.pVertexPositionsPrevious;
	pCommand->pClothPiece = this;
	return true;
}

struct CRY_ALIGN(16)SMemVec
{
	Vec3 v;
	float w;
};

void CClothPiece::UpdateSimulation(const DualQuat* pTransformations, const uint transformationCount)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	if (!m_simulator.IsVisible())
		return;
	if (m_pVClothAttachment->GetClothParams().hide)
		return;
	if (m_clothGeom->weldMap == NULL)
		return;
	if (m_poolIdx >= 0)
		m_buffers = m_clothGeom->GetBufferPtr(m_poolIdx);
	if (m_buffers == NULL)
		return;
	if (!m_pVClothAttachment->EnsureRemapTableIsValid())
		return;

	DynArray<Vec3>& arrDstPositions = m_buffers->m_arrDstPositions;
	DynArray<Vector4>& tmpClothVtx = m_buffers->m_tmpClothVtx;

	CVertexData vertexData;
	vertexData.m_vertexCount = m_clothGeom->nVtx;
	vertexData.pPositions.data = &arrDstPositions[0];
	vertexData.pPositions.iStride = sizeof(Vec3);
	vertexData.pTangents.data = &m_buffers->m_arrDstTangents[0];
	vertexData.pTangents.iStride = sizeof(SPipTangents);

	uint8 commandBufferData[256];
	CVertexCommandBufferAllocatorStatic commandBufferAllocator(
		commandBufferData, sizeof(commandBufferData));
	CVertexCommandBuffer commandBuffer;
	commandBuffer.Initialize(commandBufferAllocator);

	if (VertexCommandSkin* pCommand = commandBuffer.AddCommand<VertexCommandSkin>())
	{
		CModelMesh* pModelMesh = m_pVClothAttachment->m_pSimSkin->GetModelMesh(0);
		const CSoftwareMesh& geometry = pModelMesh->m_softwareMesh;

		pCommand->pTransformations = pTransformations;
		pCommand->pTransformationRemapTable = &m_pVClothAttachment->m_arrSimRemapTable[0];
		pCommand->transformationCount = transformationCount;
		pCommand->pVertexPositions = geometry.GetPositions();
		pCommand->pVertexQTangents = geometry.GetTangents();
		pCommand->pVertexTransformIndices = geometry.GetBlendIndices();
		pCommand->pVertexTransformWeights = geometry.GetBlendWeights();
		pCommand->vertexTransformCount = geometry.GetBlendCount();
	}

	commandBuffer.Process(vertexData);

	bool doSkinning = false;
	if (!Console::GetInst().ca_ClothBypassSimulation && m_simulator.IsInitialized())
	{
		// transform the resulting vertices into physics space
		for (int i = 0; i < arrDstPositions.size(); i++)
		{
#ifdef CLOTH_SSE
			Vector4 v;
			v.Load((const float*)&arrDstPositions[i]);
#else
			const Vec3& v = arrDstPositions[i];
#endif
			Quaternion ssequat(m_charLocation.q);
			tmpClothVtx[m_clothGeom->weldMap[i]] = ssequat * v + m_charLocation.t * m_simulator.GetParams().translationBlend; // see also below; not used at the moment, since translationBlend, externalBlend default to zero
		}

		// send the target pose to the cloth simulator
		m_simulator.SetSkinnedPositions(&tmpClothVtx[0]);

		// step the cloth
		float dt = m_pCharInstance ? g_AverageFrameTime * m_pCharInstance->GetPlaybackScale() : g_AverageFrameTime;
		dt = dt ? dt : g_AverageFrameTime;

		// don't handle camera distance in character tool
		if (!(m_pCharInstance->m_CharEditMode & CA_CharacterTool))
		{
			m_simulator.HandleCameraDistance();
		}
		if (m_simulator.IsFading())
		{
			//m_simulator.PopFadeInOutPhysics();
			m_simulator.DecreaseFadeInOutTimer(dt);
		}

		// skinning or simulation?
		if (!m_simulator.IsSimulationEnabled())
		{
			m_simulator.SetGpuSkinning(true);
			doSkinning = true;
		}
		else
		{
			m_simulator.SetGpuSkinning(false);
			m_simulator.StartStep(dt, m_charLocation);

			while (!m_simulator.Step()) {};

			// get the result back
			if (m_simulator.IsFading())
			{
				m_simulator.GetVerticesFaded(&tmpClothVtx[0]);
			}
			else
			{
				m_simulator.GetVertices(&tmpClothVtx[0]);
			}

			// filtering of positions, if requested
			if (m_simulator.GetParams().filterLaplace)
			{
				m_simulator.LaplaceFilterPositions(&tmpClothVtx[0], m_simulator.GetParams().filterLaplace);
			}

			for (int i = 0; i < m_clothGeom->nUniqueVtx; i++)
			{
				Quaternion ssequat(m_charLocation.q);
				tmpClothVtx[i] = tmpClothVtx[i] * ssequat - m_charLocation.t * m_simulator.GetParams().translationBlend;// see also above; not used at the moment, since translationBlend, externalBlend default to zero
			}
		}
	}
	else
	{
		doSkinning = true;
		m_simulator.SetGpuSkinning(true);
	}

	if (doSkinning)
	{
		for (int i = 0; i < arrDstPositions.size(); i++)
		{
#ifdef CLOTH_SSE
			tmpClothVtx[m_clothGeom->weldMap[i]].Load((const float*)&arrDstPositions[i]);
#else
			tmpClothVtx[m_clothGeom->weldMap[i]] = arrDstPositions[i];
#endif
		}
	}
}

template<bool PREVIOUS_POSITIONS>
void CClothPiece::SkinSimulationToRenderMesh(int lod, CVertexData& vertexData, const strided_pointer<const Vec3>& pVertexPositionsPrevious)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);
	if (m_clothGeom->skinMap[lod] == NULL || m_buffers == NULL)
		return;

	const int nVtx = vertexData.GetVertexCount();
	strided_pointer<Vec3> pVtx = vertexData.GetPositions();

	const DynArray<Vector4>& tmpClothVtx = m_buffers->m_tmpClothVtx;
	std::vector<Vector4>& normals = m_buffers->m_normals;
	std::vector<STangents>& tangents = m_buffers->m_tangents;

	// compute sim normals
	mesh_data* md = (mesh_data*)m_clothGeom->pPhysGeom->pGeom->GetData();
	for (int i = 0; i < md->nVertices; i++)
		normals[i].zero();
	for (int i = 0; i < md->nTris; i++)
	{
		int base = i * 3;
		const int idx1 = md->pIndices[base++];
		const int idx2 = md->pIndices[base++];
		const int idx3 = md->pIndices[base];
		const Vector4& p1 = tmpClothVtx[idx1];
		const Vector4& p2 = tmpClothVtx[idx2];
		const Vector4& p3 = tmpClothVtx[idx3];
		Vector4 n = (p2 - p1) ^ (p3 - p1);
		normals[idx1] += n;
		normals[idx2] += n;
		normals[idx3] += n;
	}
	for (int i = 0; i < md->nVertices; i++)
		normals[i].normalize();

	// set the positions
	SMemVec newPos;
	for (int i = 0; i < nVtx; i++)
	{
		tangents[i].n.zero();
		tangents[i].t.zero();

		if (m_clothGeom->skinMap[lod][i].iMap < 0)
			continue;

#ifdef CLOTH_SSE
		Vector4 p = SkinByTriangle(i, pVtx, lod);
		p.StoreAligned((float*)&newPos);
#else
		newPos.v = SkinByTriangle(i, pVtx, lod);
#endif
		if (PREVIOUS_POSITIONS)
		{
			vertexData.pVelocities[i] = pVertexPositionsPrevious[i] - newPos.v;
		}
		else
		{
			memset(&vertexData.pVelocities[i], 0, sizeof(Vec3));
		}
		pVtx[i] = newPos.v;
	}

	strided_pointer<SPipTangents> pTangents = vertexData.GetTangents();

	// rebuild tangent frame 
	int nTris = m_clothGeom->numIndices[lod] / 3;
	vtx_idx* pIndices = m_clothGeom->pIndices[lod];
	for (int i = 0; i < nTris; i++)
	{
		int base = i * 3;
		int i1 = pIndices[base++];
		int i2 = pIndices[base++];
		int i3 = pIndices[base];

#ifdef CLOTH_SSE
		Vector4 v1;
		v1.Load((const float*)&pVtx[i1]);
		Vector4 v2;
		v2.Load((const float*)&pVtx[i2]);
		Vector4 v3;
		v3.Load((const float*)&pVtx[i3]);
#else
		const Vec3& v1 = pVtx[i1];
		const Vec3& v2 = pVtx[i2];
		const Vec3& v3 = pVtx[i3];
#endif

		Vector4 u = v2 - v1;
		Vector4 v = v3 - v1;

		const float t1 = m_clothGeom->tangentData[lod][i].t1;
		const float t2 = m_clothGeom->tangentData[lod][i].t2;
		const float r = m_clothGeom->tangentData[lod][i].r;
		Vector4 sdir = (u * t2 - v * t1) * r;

		tangents[i1].t += sdir;
		tangents[i2].t += sdir;
		tangents[i3].t += sdir;

		// compute area averaged normals
		Vector4 n = u ^ v;
		tangents[i1].n += n;
		tangents[i2].n += n;
		tangents[i3].n += n;
	}

#ifdef CLOTH_SSE
	Vector4 minusOne(0.f, 0.f, 0.f, -1.f);
	Vector4 maxShort(32767.f);
	CRY_ALIGN(16) Vec4sf tangentBitangent[2];
#endif
	for (int i = 0; i < nVtx; i++)
	{
		// Gram-Schmidt ortho-normalization
		const Vector4& t = tangents[i].t;
		const Vector4 n = tangents[i].n.normalized();
		Vector4 tan = (t - n * (n * t)).normalized();
		Vector4 biTan = tan ^ n;
#ifdef CLOTH_SSE
		tan.q = _mm_or_ps(tan.q, minusOne.q);
		biTan.q = _mm_or_ps(biTan.q, minusOne.q);

		__m128i tangenti = _mm_cvtps_epi32(_mm_mul_ps(tan.q, maxShort.q));
		__m128i bitangenti = _mm_cvtps_epi32(_mm_mul_ps(biTan.q, maxShort.q));

		__m128i compressed = _mm_packs_epi32(tangenti, bitangenti);
		_mm_store_si128((__m128i*)&tangentBitangent[0], compressed);

		pTangents[i] = SPipTangents(tangentBitangent[0], tangentBitangent[1]);
#else
		pTangents[i] = SPipTangents(tan, biTan, -1);
#endif
	}

	m_clothGeom->ReleaseBuffers(m_poolIdx);
	m_buffers = NULL;
	m_poolIdx = -1;
}

void SClothGeometry::AllocateBuffer()
{
	size_t poolSize = pool.size();
	uint32 maxChars = Console::GetInst().ca_ClothMaxChars;
	if (maxChars == 0 || poolSize < maxChars)
	{
		pool.resize(poolSize + 1);
		freePoolSlots.push_back(poolSize);
		SBuffers* buff = &pool.back();

		buff->m_arrDstPositions.resize(nVtx);
		buff->m_arrDstTangents.resize(nVtx);

		buff->m_tmpClothVtx.resize(nUniqueVtx + 1); // leave room for unaligned store
		buff->m_normals.resize(nUniqueVtx);

		buff->m_tangents.resize(maxVertices);
	}
}

SBuffers* SClothGeometry::GetBufferPtr(int idx)
{
	uint32 numSize = pool.size();
	if (idx >= numSize)
		return NULL;
	std::list<SBuffers>::iterator it = pool.begin();
	std::advance(it, idx);

	return &(*it);
}

int SClothGeometry::GetBuffers()
{
	WriteLock lock(poolLock);
	if (freePoolSlots.size())
	{
		int idx = freePoolSlots.back();
		freePoolSlots.pop_back();
		return idx;
	}

	return -1;
}

void SClothGeometry::ReleaseBuffers(int idx)
{
	WriteLock lock(poolLock);
	freePoolSlots.push_back(idx);
}

void SClothGeometry::Cleanup()
{
	if (pPhysGeom)
	{
		g_pIPhysicalWorld->GetGeomManager()->UnregisterGeometry(pPhysGeom);
		pPhysGeom = NULL;
	}
	SAFE_DELETE_ARRAY(weldMap);
	SAFE_DELETE_ARRAY(weights);
	for (int i = 0; i < MAX_LODS; i++)
	{
		SAFE_DELETE_ARRAY(skinMap[i]);
		SAFE_DELETE_ARRAY(pIndices[i]);
		SAFE_DELETE_ARRAY(tangentData[i]);
	}
}

bool CompareDistances(SClothInfo* a, SClothInfo* b)
{
	return a->distance < b->distance;
}