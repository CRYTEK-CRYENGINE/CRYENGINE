// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AttachmentSkin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IShader.h>
#include <CryAnimation/IAttachment.h>
#include "ModelMesh.h"
#include "AttachmentManager.h"
#include "CharacterInstance.h"
#include <CryMath/QTangent.h>
#include "CharacterManager.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryMath/QTangent.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include "Vertex/VertexAnimation.h"
#include "Vertex/VertexData.h"
#include "Vertex/VertexCommand.h"
#include "Vertex/VertexCommandBuffer.h"

void CAttachmentSKIN::ReleaseSoftwareRenderMeshes()
{
	m_pRenderMeshsSW[0] = NULL;
	m_pRenderMeshsSW[1] = NULL;
}

uint32 CAttachmentSKIN::Immediate_AddBinding( IAttachmentObject* pIAttachmentObject,ISkin* pISkin,uint32 nLoadingFlags ) 
{
	if (pIAttachmentObject==0) 
		return 0; //no attachment objects 

	if (pISkin==0)
		CryFatalError("CryAnimation: if you create the binding for a Skin-Attachment, then you have to pass the pointer to an ISkin as well");

	uint32 nLogWarnings = (nLoadingFlags&CA_DisableLogWarnings)==0;
	CSkin* pCSkinModel = (CSkin*)pISkin;

	//only the SKIN-Instance is allowed to keep a smart-ptr CSkin object 
	//this should prevent ptr-hijacking in the future
	if (m_pModelSkin!=pCSkinModel)
	{
		ReleaseModelSkin();
		g_pCharacterManager->RegisterInstanceSkin(pCSkinModel,this); //register this CAttachmentSKIN instance in CSkin. 
		m_pModelSkin=pCSkinModel;            //increase the Ref-Counter 		
	}

	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pSkelFilePath = pDefaultSkeleton->GetModelFilePath();
	const char* pSkinFilePath = m_pModelSkin->GetModelFilePath();

	uint32 numJointsSkel = pDefaultSkeleton->m_arrModelJoints.size();
	uint32 numJointsSkin = m_pModelSkin->m_arrModelJoints.size();

	uint32 NotMatchingNames=0;
	m_arrRemapTable.resize(numJointsSkin, 0);
	for(uint32 js=0; js<numJointsSkin; js++) 
	{
		int32 nID;
		if (m_pModelSkin->m_arrModelJoints[js].m_nJointCRC32Lower == -1)
			nID = m_pModelSkin->m_arrModelJoints[js].m_nExtraJointID + numJointsSkel;
		else
			nID = pDefaultSkeleton->GetJointIDByCRC32(m_pModelSkin->m_arrModelJoints[js].m_nJointCRC32Lower);

		if (nID>=0)
			m_arrRemapTable[js]=nID;
#ifdef EDITOR_PCDEBUGCODE
		else
		{
			NotMatchingNames++;
			if (nLogWarnings)
			{
				g_pILog->LogWarning("Extending Skeleton, because the joint-name (%s) of SKIN (%s) was not found in SKEL:  %s", m_pModelSkin->m_arrModelJoints[js].m_NameModelSkin.c_str(), pSkinFilePath, pSkelFilePath);
			}
		}
#endif
	} //for loop

	if (NotMatchingNames)
	{
		// For now limited to CharEdit
		if (pInstanceSkel->m_CharEditMode || (nLoadingFlags & CA_CharEditModel))
		{
			assert(pInstanceSkel->m_CharEditMode);
			assert(nLoadingFlags & CA_CharEditModel);
			RecreateDefaultSkeleton(pInstanceSkel, nLoadingFlags | CA_CharEditModel);
		}
		else
		{
			if (nLogWarnings)
			{
				CryLogAlways("SKEL: %s",pDefaultSkeleton->GetModelFilePath() );
				CryLogAlways("SKIN: %s",m_pModelSkin->GetModelFilePath() );
				uint32 numJointCount = pDefaultSkeleton->GetJointCount();
				for (uint32 i=0; i<numJointCount; i++)
				{
					const char* pJointName = pDefaultSkeleton->GetJointNameByID(i);
					CryLogAlways("%03d JointName: %s",i,pJointName );
				}
			}

			// Free the new attachment as we cannot use it
			SAFE_RELEASE(pIAttachmentObject);
			return 0; //critical! incompatible skeletons. cant create skin-attachment
		}
	}

	// Patch the remapping 
	if (!(nLoadingFlags & CA_SkipBoneRemapping))
	{
		for (size_t i=0; i<m_pModelSkin->GetNumLODs(); i++)
		{
			CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(i);
			IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
			if (!pRenderMesh)
				continue; 
			pRenderMesh->CreateRemappedBoneIndicesPair(m_arrRemapTable, pDefaultSkeleton->GetGuid(), this);
		}
	}
	
	SAFE_RELEASE(m_pIAttachmentObject);
	m_pIAttachmentObject=pIAttachmentObject;
	return 1; 
}

void CAttachmentSKIN::Immediate_ClearBinding(uint32 nLoadingFlags)
{
	if (m_pIAttachmentObject)
	{
		m_pIAttachmentObject->Release();
		m_pIAttachmentObject = 0;
		ReleaseModelSkin();

		if (nLoadingFlags & CA_SkipSkelRecreation)
			return;

		// For now limited to CharEdit
		CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
		if (pInstanceSkel->m_CharEditMode || (nLoadingFlags & CA_CharEditModel))
		{
			assert(pInstanceSkel->m_CharEditMode);
			assert(nLoadingFlags & CA_CharEditModel);
			RecreateDefaultSkeleton(pInstanceSkel, nLoadingFlags | CA_CharEditModel);
		}
	}
}; 

void CAttachmentSKIN::RecreateDefaultSkeleton(CCharInstance* pInstanceSkel, uint32 nLoadingFlags) 
{
	const CDefaultSkeleton* const pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pOriginalFilePath = pDefaultSkeleton->GetModelFilePath();
	if (pDefaultSkeleton->GetModelFilePathCRC64() && pOriginalFilePath[0]=='_')
	{
		pOriginalFilePath++; // All extended skeletons have an '_' in front of the filepath to not confuse them with regular skeletons.
	}

	CDefaultSkeleton* pOrigDefaultSkeleton = g_pCharacterManager->CheckIfModelSKELLoaded(pOriginalFilePath,nLoadingFlags);
	if (!pOrigDefaultSkeleton)
	{
		return;
	}

	pOrigDefaultSkeleton->SetKeepInMemory(true);

	std::vector<const char*> mismatchingSkins;
	uint64 nExtendedCRC64 = CCrc32::ComputeLowercase(pOriginalFilePath);
	for (auto&& pAttachment : m_pAttachmentManager->m_arrAttachments)
	{
		if (pAttachment->GetType() != CA_SKIN)
		{
			continue;
		}

		const CSkin* const pSkin  = static_cast<CAttachmentSKIN*>(pAttachment.get())->m_pModelSkin;
		if (!pSkin)
		{
			continue;
		}

		const char* pSkinFilename = pSkin->GetModelFilePath();
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

void CAttachmentSKIN::UpdateRemapTable() 
{
	if (m_pModelSkin==0)
		return;

	ReleaseRemapTablePair();
	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	const char* pSkelFilePath = pDefaultSkeleton->GetModelFilePath();
	const char* pSkinFilePath = m_pModelSkin->GetModelFilePath();

	uint32 numJointsSkel = pDefaultSkeleton->m_arrModelJoints.size();
	uint32 numJointsSkin = m_pModelSkin->m_arrModelJoints.size();

	m_arrRemapTable.resize(numJointsSkin, 0);
	for(uint32 js=0; js<numJointsSkin; js++) 
	{
		const int32 nID = pDefaultSkeleton->GetJointIDByCRC32(m_pModelSkin->m_arrModelJoints[js].m_nJointCRC32Lower);
		if (nID>=0)
			m_arrRemapTable[js]=nID;
		else
			CryFatalError("ModelError: data-corruption when executing UpdateRemapTable for SKEL (%s) and SKIN (%s) ",pSkelFilePath,pSkinFilePath); //a fail in this case is virtually impossible, because the SKINs are alread attached 
	}
	
	// Patch the remapping 
	for (size_t i=0; i<m_pModelSkin->GetNumLODs(); i++)
	{
		CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(i);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (!pRenderMesh)
			continue; 
		pRenderMesh->CreateRemappedBoneIndicesPair(m_arrRemapTable, pDefaultSkeleton->GetGuid(), this);
	}
}

void CAttachmentSKIN::ReleaseRemapTablePair()
{
	if (!m_pModelSkin)
		return;
	CCharInstance* pInstanceSkel = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pModelSkel = pInstanceSkel->m_pDefaultSkeleton;
	const uint skeletonGuid = pModelSkel->GetGuid();
	for (size_t i=0; i<m_pModelSkin->GetNumLODs(); i++)
	{
		CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(i);
		IRenderMesh* pRenderMesh = pModelMesh->m_pIRenderMesh;
		if (!pRenderMesh)
			continue; 
		pRenderMesh->ReleaseRemappedBoneIndicesPair(skeletonGuid);
	}
}

uint32 CAttachmentSKIN::Immediate_SwapBinding(IAttachment* pNewAttachment)
{
	CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "CAttachmentSKIN::SwapBinding attempting to swap skin attachment bindings this is not supported");
	return 0;
}

float CAttachmentSKIN::GetExtent(EGeomForm eForm)
{
	if (m_pModelSkin)
	{
		int nLOD = m_pModelSkin->SelectNearestLoadedLOD(0);
		if (IRenderMesh* pMesh = m_pModelSkin->GetIRenderMesh(nLOD))
			return pMesh->GetExtent(eForm);
	}
	return 0.f;
}

void CAttachmentSKIN::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	int nLOD = m_pModelSkin->SelectNearestLoadedLOD(0);
	IRenderMesh* pMesh = m_pModelSkin->GetIRenderMesh(nLOD);
	if (!pMesh)
	{
		points.fill(ZERO);
		return;
	}

	SSkinningData* pSkinningData = NULL;
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	for (int n = 0; n < 3; n++)
	{
		int nList = (nFrameID-n) % 3;
		if (m_arrSkinningRendererData[nList].nFrameID == nFrameID-n)
		{
			pSkinningData = m_arrSkinningRendererData[nList].pSkinningData;
			break;
		}
	}

	pMesh->GetRandomPoints(points, seed, eForm, pSkinningData);
}

const QuatTS CAttachmentSKIN::GetAttWorldAbsolute() const 
{ 
	QuatTS rPhysLocation = m_pAttachmentManager->m_pSkelInstance->m_location;
	return rPhysLocation;
};

void CAttachmentSKIN::UpdateAttModelRelative()
{
}

int CAttachmentSKIN::GetGuid() const
{
	return m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetGuid();
}

void CAttachmentSKIN::ReleaseModelSkin()
{
	if (m_pModelSkin)
	{
		g_pCharacterManager->UnregisterInstanceSkin(m_pModelSkin,this);
		ReleaseRemapTablePair();
		m_pModelSkin = 0;
	}
}

CAttachmentSKIN::~CAttachmentSKIN() 
{
	if (m_AttFlags & FLAGS_ATTACH_SW_SKINNING)
	{
		int nFrameID = gEnv->pRenderer ? gEnv->pRenderer->EF_GetSkinningPoolID() : 0;
		int nList = nFrameID % 3;
		if(m_arrSkinningRendererData[nList].nFrameID == nFrameID && m_arrSkinningRendererData[nList].pSkinningData)
		{                              
			if(m_arrSkinningRendererData[nList].pSkinningData->pAsyncJobs)
				gEnv->pJobManager->WaitForJob( *m_arrSkinningRendererData[nList].pSkinningData->pAsyncJobs );
		}
	}

	ReleaseModelSkin();

	CVertexAnimation::RemoveSoftwareRenderMesh(this);
	for (uint32 j=0; j<2; ++j)
			m_pRenderMeshsSW[j] = NULL;
}

void CAttachmentSKIN::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CAttachmentSKIN");

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

size_t CAttachmentSKIN::SizeOfThis() const
{
	size_t nSize = sizeof(CAttachmentSKIN) + sizeofVector(m_strSocketName);
	nSize += m_arrRemapTable.get_alloc_size();
	return nSize;
}

void CAttachmentSKIN::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject( m_strSocketName );
	pSizer->AddObject( m_arrRemapTable );	
}

_smart_ptr<IRenderMesh> CAttachmentSKIN::CreateVertexAnimationRenderMesh(uint lod, uint id)
{
	m_pRenderMeshsSW[id] = NULL; // smart pointer release

	if ((m_AttFlags & FLAGS_ATTACH_SW_SKINNING) == 0)
		return _smart_ptr<IRenderMesh>(NULL);
	if (m_pModelSkin == 0)
		return _smart_ptr<IRenderMesh>(NULL);

	uint32 numModelMeshes = m_pModelSkin->m_arrModelMeshes.size();
	if (lod >= numModelMeshes)
		return _smart_ptr<IRenderMesh>(NULL);

	_smart_ptr<IRenderMesh> pIStaticRenderMesh = m_pModelSkin->m_arrModelMeshes[lod].m_pIRenderMesh;
	if (pIStaticRenderMesh==0)
		return _smart_ptr<IRenderMesh>(NULL);
	;

	CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(lod);
	uint32 success = pModelMesh->InitSWSkinBuffer(); 
	if (success==0)
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
	for (size_t i=0; i<size_t(chunks.size()); ++i)
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

void CAttachmentSKIN::CullVertexFrames(const SRenderingPassInfo& passInfo, float fDistance)
{
	DynArray<SVertexFrameState>& frameStates = m_vertexAnimation.GetFrameStates();
	const uint stateCount = uint(frameStates.size());

	const CCamera& camera = passInfo.GetCamera();
	float fDeltaSize = 1.f / tan(camera.GetFov()/2.0f);
	float fPixelThreshold = Console::GetInst().ca_vaBlendCullingThreshold / camera.GetViewSurfaceZ();

	const float fNearPlane = camera.GetNearPlane();
	fDistance *= passInfo.GetInverseZoomFactor();
	fDistance = fDistance < fNearPlane ? fNearPlane : fDistance;
	fDeltaSize /= fDistance;

	for (uint i=0; i<stateCount; ++i)
	{
		SVertexFrameState& frameState = frameStates[i];
		if (const SSoftwareVertexFrame* const pMorphTarget = frameState.pFrame)
		{
			if (std::abs(frameState.weight * pMorphTarget->vertexMaxLength * fDeltaSize) < fPixelThreshold)
			{
				frameState.flags |= VA_FRAME_CULLED;
			}
		}
	}
}

void CAttachmentSKIN::DrawAttachment(SRendParams& RendParams, const SRenderingPassInfo &passInfo, const Matrix34& rWorldMat34, f32 fZoomFactor)
{
	//-----------------------------------------------------------------------------
	//---              map logical LOD to render LOD                            ---
	//-----------------------------------------------------------------------------
	int32 numLODs = m_pModelSkin->m_arrModelMeshes.size();
	if (numLODs==0)
		return;
	int nDesiredRenderLOD = RendParams.lodValue.LodA();
	if (nDesiredRenderLOD == -1)
		nDesiredRenderLOD = RendParams.lodValue.LodB();
	if (nDesiredRenderLOD>=numLODs)
	{
		if (m_AttFlags&FLAGS_ATTACH_RENDER_ONLY_EXISTING_LOD)
			return;  //early exit, if LOD-file doesn't exist
		nDesiredRenderLOD = numLODs-1; //render the last existing LOD-file
	}
	int nRenderLOD = m_pModelSkin->SelectNearestLoadedLOD(nDesiredRenderLOD);  //we can render only loaded LODs

	m_pModelSkin->m_arrModelMeshes[nRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();

//	float fColor[4] = {1,0,1,1};
//	extern f32 g_YLine;
//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"CSkin:  numLODs: %d  nLodLevel: %d   nRenderLOD: %d   Model: %s",numLODs, RendParams.nLod, nRenderLOD, m_pModelSkin->GetModelFilePath() ); g_YLine+=0x10;

	Matrix34 FinalMat34 = rWorldMat34;
	RendParams.pMatrix = &FinalMat34;
	RendParams.pInstance = this;
	RendParams.pMaterial = (IMaterial*)m_pIAttachmentObject->GetReplacementMaterial(nRenderLOD); //the Replacement-Material has priority
	if (RendParams.pMaterial==0)
		RendParams.pMaterial = (IMaterial*)m_pModelSkin->GetIMaterial(nRenderLOD); //as a fall back take the Base-Material from the model

	bool bNewFrame = false;
	if (m_vertexAnimation.m_skinningPoolID != gEnv->pRenderer->EF_GetSkinningPoolID())
	{
		m_vertexAnimation.m_skinningPoolID = gEnv->pRenderer->EF_GetSkinningPoolID();
		++m_vertexAnimation.m_RenderMeshId;
		bNewFrame = true;
	}

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObj)
		return;

	pObj->m_pRenderNode = RendParams.pRenderNode;
	pObj->m_editorSelectionID = RendParams.nEditorSelectionID;
	uint64 uLocalObjFlags = pObj->m_ObjFlags;

	//check if it should be drawn close to the player
	CCharInstance* pMaster	=	m_pAttachmentManager->m_pSkelInstance;
	if (pMaster->m_rpFlags & CS_FLAG_DRAW_NEAR)
	{ 
		uLocalObjFlags |= FOB_NEAREST;
	}
	else
	{
		uLocalObjFlags &= ~FOB_NEAREST;
	}

	pObj->m_fAlpha = RendParams.fAlpha;
	pObj->m_fDistance =	RendParams.fDistance;
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
	if(RendParams.nCustomFlags & COB_CLOAK_HIGHLIGHT)
	{
		pD->m_fTempVars[5] = RendParams.fCustomData[0];
	}
	else if(RendParams.nCustomFlags & COB_POST_3D_RENDER)
	{
		memcpy(&pD->m_fTempVars[5],&RendParams.fCustomData[0],sizeof(float)*4);
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
	if(pMaster->m_rpFlags & CS_FLAG_BIAS_SKIN_SORT_DIST)
	{
		pObj->m_fSort = SORT_BIAS_AMOUNT;
	}

	// get skinning data		
	static ICVar* cvar_gd         = gEnv->pConsole->GetCVar("r_ComputeSkinning");
	static ICVar* cvar_gdMorphs   = gEnv->pConsole->GetCVar("r_ComputeSkinningMorphs");
	static ICVar* cvar_gdTangents = gEnv->pConsole->GetCVar("r_ComputeSkinningTangents");
	
	bool bAllowGPUComputeDeformation = (cvar_gd         && (cvar_gd        ->GetIVal() == 2)) || (m_AttFlags & FLAGS_ATTACH_COMPUTE_SKINNING);
	bool bAllowGPUComputePreMorphs   = (cvar_gdMorphs   && (cvar_gdMorphs  ->GetIVal() == 2)) || (m_AttFlags & FLAGS_ATTACH_COMPUTE_SKINNING_PREMORPHS);
	bool bAllowGPUComputeTagent      = (cvar_gdTangents && (cvar_gdTangents->GetIVal() == 2)) || (m_AttFlags & FLAGS_ATTACH_COMPUTE_SKINNING_TANGENTS);

	bool bUseGPUComputeDeformation = (nRenderLOD == 0) && (cvar_gd && cvar_gd->GetIVal()) && bAllowGPUComputeDeformation;
	bool bUseCPUDeformation        = !bUseGPUComputeDeformation && (nRenderLOD == 0) && ShouldSwSkin() && (Console::GetInst().ca_vaEnable != 0);

	IF (!bUseCPUDeformation, 1) 
		pObj->m_ObjFlags |= FOB_SKINNED;

	if (bNewFrame)
	{
		bool bUpdateActiveMorphs = (bUseGPUComputeDeformation && (cvar_gdMorphs && cvar_gdMorphs->GetIVal())) || (bUseCPUDeformation);
		if (bUpdateActiveMorphs && Console::GetInst().ca_vaBlendEnable && m_vertexAnimation.GetFrameStates().size())
		{
			m_vertexAnimation.UpdateFrameWeightsFromPose(m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose.GetPoseData());
			CullVertexFrames(passInfo, pObj->m_fDistance);
		}
	}

	pD->m_pSkinningData = GetVertexTransformationData(bUseCPUDeformation, nRenderLOD, passInfo);

	Vec3 skinOffset = m_pModelSkin->m_arrModelMeshes[nRenderLOD].m_vRenderMeshOffset;
	pD->m_pSkinningData->vecAdditionalOffset = skinOffset;

	IRenderMesh* pRenderMesh = m_pModelSkin->GetIRenderMesh(nRenderLOD);

	if (pRenderMesh && bUseGPUComputeDeformation && bNewFrame)
	{
		pObj->m_pCurrMaterial = RendParams.pMaterial;
		if(pObj->m_pCurrMaterial == 0)
			pObj->m_pCurrMaterial = m_pModelSkin->GetIMaterial(0);

		pD->m_pSkinningData->nHWSkinningFlags |= eHWS_DC_deformation_Skinning;
		if (bAllowGPUComputePreMorphs)
			pD->m_pSkinningData->nHWSkinningFlags |= eHWS_DC_Deformation_PreMorphs;
		if (bAllowGPUComputeTagent)
			pD->m_pSkinningData->nHWSkinningFlags |= eHWS_DC_Deformation_Tangents;
	
		g_pIRenderer->EF_EnqueueComputeSkinningData(passInfo.GetIRenderView(), pD->m_pSkinningData);

		pRenderMesh->Render(pObj,passInfo);
		return;
	}

	if (ShouldSkinLinear())
		pD->m_pSkinningData->nHWSkinningFlags |= eHWS_SkinnedLinear; 

	if(bUseCPUDeformation && pRenderMesh) 
	{
		SVertexAnimationJob *pVertexAnimation = static_cast<SVertexAnimationJob*>(pD->m_pSkinningData->pCustomData);
		uint iCurrentRenderMeshID = m_vertexAnimation.m_RenderMeshId & 1;

		if (bNewFrame)
		{
			CreateVertexAnimationRenderMesh(nRenderLOD, iCurrentRenderMeshID);
			CVertexAnimation::RegisterSoftwareRenderMesh(this);
		}

		pRenderMesh = m_pRenderMeshsSW[iCurrentRenderMeshID];

		IF (pRenderMesh && bNewFrame, 1)
		{
			CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(nRenderLOD);
			CSoftwareMesh& geometry = pModelMesh->m_softwareMesh;

			SVertexSkinData vertexSkinData;
			vertexSkinData.pTransformations = pD->m_pSkinningData->pBoneQuatsS;
			vertexSkinData.pTransformationRemapTable = pD->m_pSkinningData->pRemapTable;
			vertexSkinData.transformationCount = pD->m_pSkinningData->nNumBones;
			vertexSkinData.pVertexPositions = geometry.GetPositions();
			vertexSkinData.pVertexColors = geometry.GetColors();
			vertexSkinData.pVertexCoords = geometry.GetCoords();
			vertexSkinData.pVertexQTangents = geometry.GetTangents();
			vertexSkinData.pVertexTransformIndices = geometry.GetBlendIndices();
			vertexSkinData.pVertexTransformWeights = geometry.GetBlendWeights();
			vertexSkinData.vertexTransformCount = geometry.GetBlendCount();
			vertexSkinData.pIndices = geometry.GetIndices();
			vertexSkinData.indexCount = geometry.GetIndexCount();
			vertexSkinData.pTangentUpdateTriangles = geometry.GetTangentUpdateData();
			vertexSkinData.tangetUpdateTriCount = geometry.GetTangentUpdateDataCount();
			vertexSkinData.pTangentUpdateVertIds = geometry.GetTangentUpdateVertIds();
			vertexSkinData.tangetUpdateVertIdsCount = geometry.GetTangentUpdateTriIdsCount();
			vertexSkinData.vertexCount = geometry.GetVertexCount();
			CRY_ASSERT(pRenderMesh->GetVerticesCount() == geometry.GetVertexCount());

#if CRY_PLATFORM_DURANGO
			const uint fslCreate = FSL_VIDEO_CREATE;
			const uint fslRead = FSL_READ|FSL_VIDEO;
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

			pVertexAnimation->vertexData.pPositions.data    = (Vec3*)(&pGeneral[0].xyz);
			pVertexAnimation->vertexData.pPositions.iStride = sizeof(pGeneral[0]);
			pVertexAnimation->vertexData.pColors.data       = (uint32*)(&pGeneral[0].color);
			pVertexAnimation->vertexData.pColors.iStride    = sizeof(pGeneral[0]);
			pVertexAnimation->vertexData.pCoords.data       = (Vec2*)(&pGeneral[0].st);
			pVertexAnimation->vertexData.pCoords.iStride    = sizeof(pGeneral[0]);
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

#ifndef _RELEASE
				if (Console::GetInst().ca_vaProfile != 0)
				{
					m_vertexAnimation.m_vertexAnimationStats.vertexCount = geometry.GetVertexCount();
					g_vertexAnimationProfiler.AddVertexAnimationStats(m_vertexAnimation.m_vertexAnimationStats);
				}
#endif
			}

			pVertexAnimation->pRenderMeshSyncVariable = pRenderMesh->SetAsyncUpdateState();

			SSkinningData *pCurrentJobSkinningData = *pD->m_pSkinningData->pMasterSkinningDataList;
			if(pCurrentJobSkinningData == NULL)
			{
				pVertexAnimation->Begin(pD->m_pSkinningData->pAsyncJobs);
			}
			else
			{
				// try to append to list
				pD->m_pSkinningData->pNextSkinningData = pCurrentJobSkinningData;
				void *pUpdatedJobSkinningData = CryInterlockedCompareExchangePointer( alias_cast<void *volatile *>(pD->m_pSkinningData->pMasterSkinningDataList), pD->m_pSkinningData, pCurrentJobSkinningData );

				// in case we failed (job has finished in the meantime), we need to start the job from the main thread
				if(pUpdatedJobSkinningData == NULL )
				{
					pVertexAnimation->Begin(pD->m_pSkinningData->pAsyncJobs);
				}
			}

			if ((Console::GetInst().ca_DebugSWSkinning > 0) || (pMaster->m_CharEditMode & CA_CharacterTool))
			{
				m_vertexAnimation.DrawVertexDebug(pRenderMesh, QuatT(RenderMat34), pVertexAnimation);
			}

			pRenderMesh->UnLockForThreadAccess();
		}
	}

	if(pRenderMesh) 
	{
		IMaterial* pMaterial = RendParams.pMaterial;
		if(pMaterial==0)
			pMaterial = m_pModelSkin->GetIMaterial(0);
		pObj->m_pCurrMaterial = pMaterial;
#ifndef _RELEASE
		CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(nRenderLOD);
		static ICVar *p_e_debug_draw = gEnv->pConsole->GetCVar("e_DebugDraw");
		if(p_e_debug_draw && p_e_debug_draw->GetIVal() > 0)
			pModelMesh->DrawDebugInfo(pMaster->m_pDefaultSkeleton, nRenderLOD, RenderMat34, p_e_debug_draw->GetIVal(), pMaterial, pObj, RendParams, passInfo.IsGeneralPass(), (IRenderNode*)RendParams.pRenderNode, m_pAttachmentManager->m_pSkelInstance->m_SkeletonPose.GetAABB(),passInfo );
#endif
		pRenderMesh->Render(pObj, passInfo);

#if EDITOR_PCDEBUGCODE
		// Draw debug for vertex/compute skinning shaders.
		// CPU skinning is handled natively by CVertexAnimation::DrawVertexDebug().
		if (!bUseCPUDeformation && (pMaster->m_CharEditMode & CA_CharacterTool))
		{
			const Console& rConsole = Console::GetInst();

			const bool tang = (rConsole.ca_DrawTangents != 0);
			const bool bitang = (rConsole.ca_DrawBinormals != 0);
			const bool norm = (rConsole.ca_DrawNormals != 0);
			const bool wire = (rConsole.ca_DrawWireframe != 0);
			if (tang || bitang || norm || wire) 
			{
				CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(nRenderLOD);
				gEnv->pJobManager->WaitForJob(*pD->m_pSkinningData->pAsyncJobs);
				SoftwareSkinningDQ_VS_Emulator(pModelMesh, pObj->GetMatrix(passInfo), tang, bitang, norm, wire, pD->m_pSkinningData->pBoneQuatsS);
			}
		}
#endif
	}

}

//-----------------------------------------------------------------------------
//---     trigger streaming of desired LODs (only need in CharEdit)         ---
//-----------------------------------------------------------------------------
void CAttachmentSKIN::TriggerMeshStreaming(uint32 nDesiredRenderLOD, const SRenderingPassInfo &passInfo)
{
	if (!m_pModelSkin)
		return;

	uint32 numLODs = m_pModelSkin->m_arrModelMeshes.size();
	if (numLODs==0)
		return;
	if (m_AttFlags&FLAGS_ATTACH_HIDE_ATTACHMENT)
		return;  //mesh not visible
	if(nDesiredRenderLOD>=numLODs)
	{
		if (m_AttFlags&FLAGS_ATTACH_RENDER_ONLY_EXISTING_LOD)
			return;  //early exit, if LOD-file doesn't exist
		nDesiredRenderLOD = numLODs-1; //render the last existing LOD-file
	}
	m_pModelSkin->m_arrModelMeshes[nDesiredRenderLOD].m_stream.nFrameId = passInfo.GetMainFrameID();
}

SSkinningData* CAttachmentSKIN::GetVertexTransformationData(bool useSwSkinningCpu, uint8 nRenderLOD, const SRenderingPassInfo& passInfo)
{
	DEFINE_PROFILER_FUNCTION();
	CCharInstance* pMaster = m_pAttachmentManager->m_pSkelInstance;
	if (pMaster==0)
	{
		CryFatalError("CryAnimation: pMaster is zero");
		return NULL;
	}

	// get data to fill
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	int nList = nFrameID % 3;
	int nPrevList = (nFrameID - 1 ) % 3;
	// before allocating new skinning date, check if we already have for this frame
	if(m_arrSkinningRendererData[nList].nFrameID == nFrameID && m_arrSkinningRendererData[nList].pSkinningData)
	{
		return m_arrSkinningRendererData[nList].pSkinningData;
	}

	if(pMaster->arrSkinningRendererData[nList].nFrameID != nFrameID )
	{
		pMaster->GetSkinningData(passInfo); // force master to compute skinning data if not available
		assert(pMaster->arrSkinningRendererData[nList].nFrameID == nFrameID);
		assert(pMaster->arrSkinningRendererData[nList].pSkinningData);
	}

	uint32 nCustomDataSize = 0;
	uint commandBufferLength = 0;
	if (useSwSkinningCpu)
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
	else
	{
		static ICVar* cvar_gd       = gEnv->pConsole->GetCVar("r_ComputeSkinning");
		static ICVar* cvar_gdMorphs = gEnv->pConsole->GetCVar("r_ComputeSkinningMorphs");

		bool bAllowGPUComputeDeformation = (cvar_gd       && (cvar_gd      ->GetIVal() == 2)) || (m_AttFlags & FLAGS_ATTACH_COMPUTE_SKINNING);
		bool bAllowGPUComputePreMorphs   = (cvar_gdMorphs && (cvar_gdMorphs->GetIVal() == 2)) || (m_AttFlags & FLAGS_ATTACH_COMPUTE_SKINNING_PREMORPHS);
		bool bUpdateActiveMorphs = 
			(cvar_gd       && cvar_gd      ->GetIVal()) && bAllowGPUComputeDeformation &&
			(cvar_gdMorphs && cvar_gdMorphs->GetIVal()) && bAllowGPUComputePreMorphs;

		if (bUpdateActiveMorphs)
		{
			const bool bAllowCulling = Console::GetInst().ca_vaProfile != 1;
			const bool bDebugCulling = Console::GetInst().ca_vaBlendCullingDebug && gEnv->pRenderer->EF_GetSkinningPoolID() % 2;

			SSkinningData* sd = pMaster->arrSkinningRendererData[nList].pSkinningData;

			uint32 numActiveMorphs = 0;
			DynArray<SVertexFrameState>& frameStates = m_vertexAnimation.GetFrameStates();
			const uint frameCount = uint(frameStates.size());
			for (uint i=0; i<frameCount; ++i)
			{
				const SVertexFrameState& frameState = frameStates[i];

				const bool bCullFrame = bAllowCulling && (frameState.flags == VA_FRAME_CULLED);
				const bool bUseFrame = frameState.weight > 0.0f && !bCullFrame;

				if (bUseFrame || bDebugCulling)
				{
					sd->pActiveMorphs[numActiveMorphs].morphIndex = frameState.frameIndex;
					sd->pActiveMorphs[numActiveMorphs].weight = frameState.weight;

					++numActiveMorphs;
				}
			}
			sd->nNumActiveMorphs = numActiveMorphs;
		}
	}

	const uint32 skinningQuatCount = m_arrRemapTable.size();
	const uint32 skinningQuatCountMax = pMaster->arrSkinningRendererData[nList].pSkinningData->nNumBones;
	const uint32 nNumBones = std::min(skinningQuatCount, skinningQuatCountMax);

	SSkinningData *pSkinningData = gEnv->pRenderer->EF_CreateRemappedSkinningData(passInfo.GetIRenderView(), nNumBones, pMaster->arrSkinningRendererData[nList].pSkinningData, nCustomDataSize, pMaster->m_pDefaultSkeleton->GetGuid());	
	pSkinningData->pCustomTag = this;
	pSkinningData->pRenderMesh = m_pModelSkin->GetIRenderMesh(nRenderLOD);
	if (nCustomDataSize)
	{
		SVertexAnimationJob *pVertexAnimation = new (pSkinningData->pCustomData) SVertexAnimationJob();
		pVertexAnimation->commandBufferLength = commandBufferLength;
	}
	m_arrSkinningRendererData[nList].pSkinningData = pSkinningData;
	m_arrSkinningRendererData[nList].nFrameID = nFrameID;
	PREFAST_ASSUME(pSkinningData);

	// set data for motion blur	
	if(m_arrSkinningRendererData[nPrevList].nFrameID == (nFrameID - 1) && m_arrSkinningRendererData[nPrevList].pSkinningData)
	{		
		pSkinningData->nHWSkinningFlags |= eHWS_MotionBlured;
		pSkinningData->pPreviousSkinningRenderData = m_arrSkinningRendererData[nPrevList].pSkinningData;
		if(pSkinningData->pPreviousSkinningRenderData->pAsyncJobs)
			gEnv->pJobManager->WaitForJob( *pSkinningData->pPreviousSkinningRenderData->pAsyncJobs );
	}	
	else
	{
		// if we don't have motion blur data, use the some as for the current frame
		pSkinningData->pPreviousSkinningRenderData = pSkinningData;
	}

	pSkinningData->pRemapTable = &m_arrRemapTable[0];

	return pSkinningData;
}

SMeshLodInfo CAttachmentSKIN::ComputeGeometricMean() const
{
	SMeshLodInfo lodInfo;
	lodInfo.Clear();

	CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(0);
	if (pModelMesh)
	{
		lodInfo.fGeometricMean = pModelMesh->m_geometricMeanFaceArea;
		lodInfo.nFaceCount = pModelMesh->m_faceCount;
	}

	return lodInfo;
}

void CAttachmentSKIN::HideAttachment( uint32 x ) 
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION, x!=0);

	if(x)
		m_AttFlags |=  (FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
	else
		m_AttFlags &= ~(FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
}

void CAttachmentSKIN::HideInRecursion( uint32 x ) 
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_RECURSION, x!=0);

	if(x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_RECURSION;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_RECURSION;
}

void CAttachmentSKIN::HideInShadow( uint32 x ) 
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_SHADOW_PASS, x!=0);

	if(x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_SHADOW_PASS;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_SHADOW_PASS;
}

#ifdef EDITOR_PCDEBUGCODE
//These functions are need only for the Editor on PC

void CAttachmentSKIN::DrawWireframeStatic( const Matrix34& m34, int nLOD, uint32 color) 
{
	CModelMesh* pModelMesh = m_pModelSkin->GetModelMesh(nLOD);
	if (pModelMesh)
		pModelMesh->DrawWireframeStatic(m34,color);
}

//////////////////////////////////////////////////////////////////////////
// skinning of external Vertices and QTangents 
//////////////////////////////////////////////////////////////////////////
void CAttachmentSKIN::SoftwareSkinningDQ_VS_Emulator( CModelMesh* pModelMesh, Matrix34 rRenderMat34, uint8 tang,uint8 binorm,uint8 norm,uint8 wire, const DualQuat* const pSkinningTransformations)
{
	// TODO: This method produces inaccurate results and should be replaced by a functionally equivalent
	// debug visualization performed with geometry that has been deformed by the actual shader implementation.

#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	CRenderAuxGeomRenderFlagsRestore _renderFlagsRestore(g_pAuxGeom);

	if (pModelMesh->m_pIRenderMesh==0)
		return;

	static DynArray<Vec3> g_arrExtSkinnedStream;
	static DynArray<Quat> g_arrQTangents;
	static DynArray<uint8> arrSkinned;

	uint32 numExtIndices	= pModelMesh->m_pIRenderMesh->GetIndicesCount();
	uint32 numExtVertices	= pModelMesh->m_pIRenderMesh->GetVerticesCount();
	assert(numExtVertices && numExtIndices);

	uint32 ssize=g_arrExtSkinnedStream.size();
	if (ssize<numExtVertices)
	{
		g_arrExtSkinnedStream.resize(numExtVertices);
		g_arrQTangents.resize(numExtVertices);
		arrSkinned.resize(numExtVertices);
	}
	memset(&arrSkinned[0],0,numExtVertices);

	pModelMesh->m_pIRenderMesh->LockForThreadAccess();
	++pModelMesh->m_iThreadMeshAccessCounter;

	vtx_idx* pIndices				= pModelMesh->m_pIRenderMesh->GetIndexPtr(FSL_READ);
	if (pIndices==0)
		return;
	int32		nPositionStride;
	uint8*	pPositions			= pModelMesh->m_pIRenderMesh->GetPosPtr(nPositionStride, FSL_READ);
	if (pPositions==0)
		return;
	int32		nQTangentStride;
	uint8*	pQTangents			= pModelMesh->m_pIRenderMesh->GetQTangentPtr(nQTangentStride, FSL_READ);
	if (pQTangents==0)
		return;
	int32		nSkinningStride;
	uint8 * pSkinningInfo				= pModelMesh->m_pIRenderMesh->GetHWSkinPtr(nSkinningStride, FSL_READ); //pointer to weights and bone-id
	if (pSkinningInfo==0)
		return;	

	DualQuat arrRemapSkinQuat[MAX_JOINT_AMOUNT];	//dual quaternions for skinning
	for (size_t b = 0; b < m_arrRemapTable.size(); ++b)
	{
		const uint16 sidx = m_arrRemapTable[b]; //is array can be different for every skin-attachment 
		arrRemapSkinQuat[b] = pSkinningTransformations[sidx];
	}

	const uint32 numSubsets = pModelMesh->m_arrRenderChunks.size();
	if (numSubsets)
		g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.5f, ColorF(1.0f,0.9f,1.0f,1.0f), false,"FilePath: %s",this->GetISkin()->GetModelFilePath()),g_YLine+=0x17;

	for (uint32 s=0; s<numSubsets; s++)
	{
		uint32 startIndex		= pModelMesh->m_arrRenderChunks[s].m_nFirstIndexId;
		uint32 endIndex			= pModelMesh->m_arrRenderChunks[s].m_nFirstIndexId+pModelMesh->m_arrRenderChunks[s].m_nNumIndices;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1.0f, 0.0f, 1.0f, 1.0f), false, "subset: %d  startIndex: %d  endIndex: %d", s, startIndex, endIndex);
		g_YLine += 0x10;

		for (uint32 idx=startIndex; idx<endIndex; ++idx)
		{
			uint32 e = pIndices[idx];
			if (arrSkinned[e])
				continue;

			arrSkinned[e]=1;
			Vec3		hwPosition	= *(Vec3*)(pPositions + e * nPositionStride) + pModelMesh->m_vRenderMeshOffset;
			SMeshQTangents	hwQTangent	= *(SMeshQTangents*)(pQTangents + e * nQTangentStride);
			uint16*	hwIndices   = ((SVF_W4B_I4S*)(pSkinningInfo + e*nSkinningStride))->indices;
			ColorB	hwWeights		= *(ColorB*)&((SVF_W4B_I4S*)(pSkinningInfo + e*nSkinningStride))->weights;

			//---------------------------------------------------------------------
			//---     this is CPU emulation of Dual-Quat skinning              ---
			//---------------------------------------------------------------------
			//get indices for bones (always 4 indices per vertex)
			uint32 id0 = hwIndices[0];
			assert(id0 < m_arrRemapTable.size());
			uint32 id1 = hwIndices[1];
			assert(id1 < m_arrRemapTable.size());
			uint32 id2 = hwIndices[2];
			assert(id2 < m_arrRemapTable.size());
			uint32 id3 = hwIndices[3];
			assert(id3 < m_arrRemapTable.size());

			//get weights for vertices (always 4 weights per vertex)
			f32 w0 = hwWeights[0]/255.0f;
			f32 w1 = hwWeights[1]/255.0f;
			f32 w2 = hwWeights[2]/255.0f;
			f32 w3 = hwWeights[3]/255.0f;

			// Current skinning shader implementations support more than 4 weights per vertex.
			// This function produces inaccurate results already due to implementations having drifted
			// apart, so for the time being we ignore any extra weights and simply normalize the first 4.
			const f32 weightSum = w0 + w1 + w2 + w3;
			w0 = w0 / weightSum;
			w1 = w1 / weightSum;
			w2 = w2 / weightSum;
			w3 = w3 / weightSum;

			const DualQuat& q0=arrRemapSkinQuat[id0];
			const DualQuat& q1=arrRemapSkinQuat[id1];
			const DualQuat& q2=arrRemapSkinQuat[id2];
			const DualQuat& q3=arrRemapSkinQuat[id3];
			DualQuat wquat		=	q0*w0 +  q1*w1 + q2*w2 +	q3*w3;

			f32 l=1.0f/wquat.nq.GetLength();
			wquat.nq*=l;
			wquat.dq*=l;
			g_arrExtSkinnedStream[e] = rRenderMat34*(wquat*hwPosition);  //transform position by dual-quaternion

			Quat QTangent = hwQTangent.GetQ();
			assert( QTangent.IsUnit() );

			g_arrQTangents[e] = wquat.nq*QTangent;
			if (g_arrQTangents[e].w<0.0f)
				g_arrQTangents[e]=-g_arrQTangents[e]; //make it positive
			f32 flip = QTangent.w<0 ? -1.0f : 1.0f; 
			if (flip<0)
				g_arrQTangents[e]=-g_arrQTangents[e]; //make it negative
		}
	}
	g_YLine+=0x10;

	pModelMesh->m_pIRenderMesh->UnLockForThreadAccess();
	--pModelMesh->m_iThreadMeshAccessCounter;
	if (pModelMesh->m_iThreadMeshAccessCounter==0)
	{
		pModelMesh->m_pIRenderMesh->UnlockStream(VSF_GENERAL);
		pModelMesh->m_pIRenderMesh->UnlockStream(VSF_TANGENTS);
		pModelMesh->m_pIRenderMesh->UnlockStream(VSF_HWSKIN_INFO);
	}

	//---------------------------------------------------------------------------
	//---------------------------------------------------------------------------
	//---------------------------------------------------------------------------

	uint32 numBuffer	=	g_arrExtSkinnedStream.size();
	if (numBuffer<numExtVertices)
		return;
	const f32 VLENGTH=0.03f;

	if (tang) 
	{
		static std::vector<ColorB> arrExtVColors;
		uint32 csize = arrExtVColors.size();
		if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
		for(uint32 i=0; i<numExtVertices*2; i=i+2)	
		{
			arrExtVColors[i+0] = RGBA8(0x3f,0x00,0x00,0x00);
			arrExtVColors[i+1] = RGBA8(0xff,0x7f,0x7f,0x00);
		}

		Matrix33 WMat33 = Matrix33(rRenderMat34);
		static std::vector<Vec3> arrExtSkinnedStream;
		uint32 vsize = arrExtSkinnedStream.size();
		if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
		for(uint32 i=0,t=0; i<numExtVertices; i++)	
		{
			Vec3 vTangent  = g_arrQTangents[i].GetColumn0()*VLENGTH;
			arrExtSkinnedStream[t+0] = g_arrExtSkinnedStream[i]; 
			arrExtSkinnedStream[t+1] = WMat33*vTangent+arrExtSkinnedStream[t];
			t=t+2;
		}

		SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
		g_pAuxGeom->SetRenderFlags( renderFlags );
		g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
	}

	if (binorm) 
	{
		static std::vector<ColorB> arrExtVColors;
		uint32 csize = arrExtVColors.size();
		if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
		for(uint32 i=0; i<numExtVertices*2; i=i+2)	
		{
			arrExtVColors[i+0] = RGBA8(0x00,0x3f,0x00,0x00);
			arrExtVColors[i+1] = RGBA8(0x7f,0xff,0x7f,0x00);
		}

		Matrix33 WMat33 = Matrix33(rRenderMat34);
		static std::vector<Vec3> arrExtSkinnedStream;
		uint32 vsize = arrExtSkinnedStream.size();
		if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
		for(uint32 i=0,t=0; i<numExtVertices; i++)	
		{
			Vec3 vBitangent = g_arrQTangents[i].GetColumn1()*VLENGTH;
			arrExtSkinnedStream[t+0] = g_arrExtSkinnedStream[i]; 
			arrExtSkinnedStream[t+1] = WMat33*vBitangent+arrExtSkinnedStream[t];
			t=t+2;
		}

		SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
		g_pAuxGeom->SetRenderFlags( renderFlags );
		g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
	}

	if (norm) 
	{
		static std::vector<ColorB> arrExtVColors;
		uint32 csize = arrExtVColors.size();
		if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
		for(uint32 i=0; i<numExtVertices*2; i=i+2)	
		{
			arrExtVColors[i+0] = RGBA8(0x00,0x00,0x3f,0x00);
			arrExtVColors[i+1] = RGBA8(0x7f,0x7f,0xff,0x00);
		}

		Matrix33 WMat33 = Matrix33(rRenderMat34);
		static std::vector<Vec3> arrExtSkinnedStream;
		uint32 vsize = arrExtSkinnedStream.size();
		if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
		for(uint32 i=0,t=0; i<numExtVertices; i++)	
		{
			f32 flip = (g_arrQTangents[i].w<0) ? -VLENGTH : VLENGTH; 
			Vec3 vNormal = g_arrQTangents[i].GetColumn2()*flip;
			arrExtSkinnedStream[t+0] = g_arrExtSkinnedStream[i]; 
			arrExtSkinnedStream[t+1] = WMat33*vNormal + arrExtSkinnedStream[t];
			t=t+2;
		}
		SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
		g_pAuxGeom->SetRenderFlags( renderFlags );
		g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
	}

	if (wire) 
	{
		uint32 color = RGBA8(0x00,0xff,0x00,0x00);
		SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
		renderFlags.SetFillMode( e_FillModeWireframe );
		renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
		renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
		g_pAuxGeom->SetRenderFlags( renderFlags );
		g_pAuxGeom->DrawTriangles(&g_arrExtSkinnedStream[0],numExtVertices, pIndices,numExtIndices,color);		
	}
}

#endif