// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CharacterInstance.h"

#include "ModelMesh.h"
#include "CharacterManager.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "FacialAnimation/FacialInstance.h"
#include <CryMath/GeomQuery.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include "Vertex/VertexCommandBuffer.h"

DECLARE_JOB("SkinningTransformationsComputation", TSkinningTransformationsComputation, CCharInstance::SkinningTransformationsComputation);

CCharInstance::CCharInstance(const string& strFileName, CDefaultSkeleton* pDefaultSkeleton) : m_skinningTransformationsCount(0), m_skinningTransformationsMovement(0)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Character Instance");

	g_pCharacterManager->RegisterInstanceSkel(pDefaultSkeleton, this);
	m_pDefaultSkeleton = pDefaultSkeleton;
	m_strFilePath = strFileName;

	m_CharEditMode = 0;
	m_nRefCounter = 0;
	m_nAnimationLOD = 0;
	m_LastRenderedFrameID = 0;
	m_Viewdir = Vec3(0, 1, 0);

	m_fDeltaTime = 0;
	m_fOriginalDeltaTime = 0;

	m_location.SetIdentity();

	m_AttachmentManager.m_pSkelInstance = this;

	m_fPostProcessZoomAdjustedDistanceFromCamera = 0;

	m_bHideMaster = false;
	m_bWasVisible = false;
	m_bPlayedPhysAnim = false;
	m_bEnableStartAnimation = 1;

	m_fPlaybackScale = 1;
	m_LastUpdateFrameID_Pre = 0;
	m_LastUpdateFrameID_Post = 0;

	m_fZoomDistanceSq = std::numeric_limits<float>::max();

	m_rpFlags = CS_FLAG_DRAW_MODEL;
	memset(arrSkinningRendererData, 0, sizeof(arrSkinningRendererData));
	m_processingContext = -1;
	m_SkeletonAnim.InitSkeletonAnim(this, &this->m_SkeletonPose);
	RuntimeInit(0);
}

//--------------------------------------------------------------------------------------------
//--          re-initialize all members related to CDefaultSkeleton
//--------------------------------------------------------------------------------------------
void CCharInstance::RuntimeInit(CDefaultSkeleton* pExtDefaultSkeleton)
{
	for (uint32 i = 0; i < 3; i++)
	{
		if (arrSkinningRendererData[i].pSkinningData)
		{
			if (arrSkinningRendererData[i].pSkinningData->pPreviousSkinningRenderData->pAsyncJobs)
				gEnv->pJobManager->WaitForJob(*arrSkinningRendererData[i].pSkinningData->pPreviousSkinningRenderData->pAsyncJobs);
		}
	}
	memset(arrSkinningRendererData, 0, sizeof(arrSkinningRendererData));
	m_SkeletonAnim.FinishAnimationComputations();

	//-----------------------------------------------
	if (pExtDefaultSkeleton)
	{
		g_pCharacterManager->UnregisterInstanceSkel(m_pDefaultSkeleton, this);
		g_pCharacterManager->RegisterInstanceSkel(pExtDefaultSkeleton, this);
		m_pDefaultSkeleton = pExtDefaultSkeleton;
	}

	uint32 numJoints = m_pDefaultSkeleton->m_arrModelJoints.size();
	m_skinningTransformationsCount = 0;
	m_skinningTransformationsMovement = 0;
	if (m_pDefaultSkeleton->m_ObjectType == CHR)
		m_skinningTransformationsCount = numJoints;

	m_SkeletonAnim.m_facialDisplaceInfo.Initialize(0);
	m_bFacialAnimationEnabled = true;

	if (m_pDefaultSkeleton->GetFacialModel())
	{
		m_pFacialInstance = new CFacialInstance(m_pDefaultSkeleton->GetFacialModel(), this);
	}

	m_SkeletonPose.InitSkeletonPose(this, &this->m_SkeletonAnim);

	//
	m_SkeletonPose.UpdateBBox(1);
	m_SkeletonPose.m_physics.m_bHasPhysics = m_pDefaultSkeleton->m_bHasPhysics2;
	m_SkeletonPose.m_physics.m_bHasPhysicsProxies = false;

	m_bHasVertexAnimation = false;
}

//////////////////////////////////////////////////////////////////////////
CCharInstance::~CCharInstance()
{
	// The command buffer needs to be flushed here, because if he is flushed
	// later on, one of the commands might access the
	// default skeleton which is no longer available
	m_AttachmentManager.UpdateBindings();

	assert(m_nRefCounter == 0);
	m_SkeletonPose.m_physics.DestroyPhysics();
	KillAllSkeletonEffects();

	WaitForSkinningJob();

	const char* pFilePath = GetFilePath();
	g_pCharacterManager->ReleaseCDF(pFilePath);

	if (m_pDefaultSkeleton)
		g_pCharacterManager->UnregisterInstanceSkel(m_pDefaultSkeleton, this);
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

void CCharInstance::StartAnimationProcessing(const SAnimationProcessParams& params)
{
	DEFINE_PROFILER_FUNCTION();
	ANIMATION_LIGHT_PROFILER();

	// execute only if start animation processing has not been started for this character
	if (GetProcessingContext())
		return;

	CharacterInstanceProcessing::CContextQueue& queue = g_pCharacterManager->GetContextSyncQueue();

	// generate contexts for me and all my attached instances
	CharacterInstanceProcessing::SContext& ctx = queue.AppendContext();
	m_processingContext = ctx.slot;
	int numberOfChildren = m_AttachmentManager.GenerateAttachedInstanceContexts();
	ctx.Initialize(this, nullptr, nullptr, numberOfChildren);
	queue.ExecuteForContextAndAllChildrenRecursively(
	  m_processingContext, CharacterInstanceProcessing::SStartAnimationProcessing(params));

	bool bImmediate = (Console::GetInst().ca_thread == 0);

	WaitForSkinningJob();
	ctx.job.Begin(bImmediate);

	if (bImmediate)
	{
		m_SkeletonAnim.FinishAnimationComputations();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharInstance::CopyPoseFrom(const ICharacterInstance& rICharInstance)
{
	const ISkeletonPose* srcIPose = rICharInstance.GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = rICharInstance.GetIDefaultSkeleton();

	CRY_ASSERT_TRACE(srcIPose, ("CopyPoseFrom: %s Copying from an invalid pose!", GetFilePath()));
	if (srcIPose)
	{
		CRY_ASSERT_TRACE(m_pDefaultSkeleton->GetJointCount() == rIDefaultSkeleton.GetJointCount(), ("CopyPoseFrom: %s Joint count mismatch %d vs %d", GetFilePath(), rIDefaultSkeleton.GetJointCount(), m_pDefaultSkeleton->GetJointCount()));

		const CSkeletonPose* srcPose = (CSkeletonPose*)srcIPose;
		m_SkeletonPose.GetPoseDataForceWriteable().Initialize(srcPose->GetPoseData());
	}
}

//////////////////////////////////////////////////////////////////////////
// TODO: Should be part of CSkeletonPhysics!
void CCharInstance::UpdatePhysicsCGA(Skeleton::CPoseData& poseData, float fScale, const QuatTS& rAnimLocationNext)
{
	DEFINE_PROFILER_FUNCTION();
	CAnimationSet* pAnimationSet = m_pDefaultSkeleton->m_pAnimationSet;

	if (!m_SkeletonPose.GetCharacterPhysics())
		return;

	if (m_fOriginalDeltaTime <= 0.0f)
		return;

	QuatT* const __restrict pAbsolutePose = poseData.GetJointsAbsolute();

	pe_params_part params;
	pe_action_set_velocity asv;
	pe_status_pos sp;
	int numNodes = poseData.GetJointCount();
	assert(numNodes);

	int i, iLayer, iLast = -1;
	bool bSetVel = 0;
	uint32 nType = m_SkeletonPose.GetCharacterPhysics()->GetType();
	bool bBakeRotation = m_SkeletonPose.GetCharacterPhysics()->GetType() == PE_ARTICULATED;
	if (bSetVel = bBakeRotation)
	{
		for (iLayer = 0; iLayer < numVIRTUALLAYERS && m_SkeletonAnim.GetNumAnimsInFIFO(iLayer) == 0; iLayer++)
			;
		bSetVel = iLayer < numVIRTUALLAYERS;
	}
	params.bRecalcBBox = false;

	for (i = 0; i < numNodes; i++)
	{
SetAgain:
		if (m_SkeletonPose.m_physics.m_ppBonePhysics && m_SkeletonPose.m_physics.m_ppBonePhysics[i])
		{
			sp.ipart = 0;
			MARK_UNUSED sp.partid;
			m_SkeletonPose.m_physics.m_ppBonePhysics[i]->GetStatus(&sp);
			pAbsolutePose[i].q = !rAnimLocationNext.q * sp.q;
			pAbsolutePose[i].t = !rAnimLocationNext.q * (sp.pos - rAnimLocationNext.t);
			continue;
		}
		if (m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID == ~0)
			continue;
		//	params.partid = joint->m_nodeid;
		params.partid = m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID;

		assert(pAbsolutePose[i].IsValid());
		params.q = pAbsolutePose[i].q;

		params.pos = pAbsolutePose[i].t * rAnimLocationNext.s;
		if (rAnimLocationNext.s != 1.0f)
			params.scale = rAnimLocationNext.s;

		if (bBakeRotation)
		{
			params.pos = rAnimLocationNext.q * params.pos;
			params.q = rAnimLocationNext.q * params.q;
		}
		if (m_SkeletonPose.GetCharacterPhysics()->SetParams(&params))
			iLast = i;
		if (params.bRecalcBBox)
			break;
	}

	// Recompute box after.
	if (iLast >= 0 && !params.bRecalcBBox)
	{
		params.bRecalcBBox = true;
		i = iLast;
		goto SetAgain;
	}

	if (bSetVel)
	{
		ApplyJointVelocitiesToPhysics(m_SkeletonPose.GetCharacterPhysics(), rAnimLocationNext.q);
		m_bPlayedPhysAnim = true;
	}
	else if (bBakeRotation && m_bPlayedPhysAnim)
	{
		asv.v.zero();
		asv.w.zero();
		for (i = 0; i < numNodes; i++)
			if ((asv.partid = m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID) != ~0)
				m_SkeletonPose.GetCharacterPhysics()->Action(&asv);
		m_bPlayedPhysAnim = false;
	}

	if (m_SkeletonPose.m_physics.m_ppBonePhysics)
		m_SkeletonPose.UpdateBBox(1);
}

void CCharInstance::ApplyJointVelocitiesToPhysics(IPhysicalEntity* pent, const Quat& qrot, const Vec3& velHost)
{
	int i, iParent, numNodes = m_SkeletonPose.GetPoseData().GetJointCount();
	QuatT qParent;
	IController* pController;
	float t, dt;
	pe_action_set_velocity asv;
	CAnimationSet* pAnimationSet = m_pDefaultSkeleton->m_pAnimationSet;

	PREFAST_SUPPRESS_WARNING(6255)
	QuatT * qAnimCur = (QuatT*)alloca(numNodes * sizeof(QuatT));
	PREFAST_SUPPRESS_WARNING(6255)
	QuatT * qAnimNext = (QuatT*)alloca(numNodes * sizeof(QuatT));
	for (i = 0; i < numNodes; i++)
		qAnimCur[i] = qAnimNext[i] = m_SkeletonPose.GetPoseData().GetJointRelative(i);

	if (!m_SkeletonAnim.m_layers[0].m_transitionQueue.m_animations.size())
	{
		for (i = 0; i < numNodes; i++)
			if (m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID != ~0)
			{
				asv.partid = m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID;
				asv.v = velHost;
				pent->Action(&asv);
			}
		return;
	}
	const CAnimation& anim = m_SkeletonAnim.GetAnimFromFIFO(0, 0);

	int idGlobal = pAnimationSet->GetGlobalIDByAnimID_Fast(anim.GetAnimationId());
	GlobalAnimationHeaderCAF& animHdr = g_AnimationManager.m_arrGlobalCAF[idGlobal];

	const float FRAME_TIME = 0.01f;
	const float fAnimDuration = max(anim.GetExpectedTotalDurationSeconds(), 1e-5f);
	dt = static_cast<float>(__fsel(fAnimDuration - FRAME_TIME, FRAME_TIME / fAnimDuration, 1.0f));
	t = min(1.0f - dt, m_SkeletonAnim.GetAnimationNormalizedTime(&anim));

	for (i = 0; i < numNodes; i++)
		if (pController = animHdr.GetControllerByJointCRC32(m_pDefaultSkeleton->m_arrModelJoints[i].m_nJointCRC32))                        //m_pDefaultSkeleton->m_arrModelJoints[i].m_arrControllersMJoint[idAnim])
			pController->GetOP(animHdr.NTime2KTime(t), qAnimCur[i].q, qAnimCur[i].t),
			pController->GetOP(animHdr.NTime2KTime(t + dt), qAnimNext[i].q, qAnimNext[i].t);

	for (i = 0; i < numNodes; i++)
		if ((iParent = m_pDefaultSkeleton->m_arrModelJoints[i].m_idxParent) >= 0 && iParent != i)
		{
			PREFAST_ASSUME(iParent >= 0 && iParent < numNodes);
			qAnimCur[i] = qAnimCur[iParent] * qAnimCur[i], qAnimNext[i] = qAnimNext[iParent] * qAnimNext[i];
		}

	const float INV_FRAME_TIME = 1.0f / FRAME_TIME;
	for (i = 0; i < numNodes; i++)
		if (m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID != ~0)
		{
			asv.partid = m_pDefaultSkeleton->m_arrModelJoints[i].m_NodeID;
			asv.v = qrot * ((qAnimNext[i].t - qAnimCur[i].t) * INV_FRAME_TIME) + velHost;
			Quat dq = qAnimNext[i].q * !qAnimCur[i].q;
			float sin2 = dq.v.len();
			asv.w = qrot * (sin2 > 0 ? dq.v * (atan2_tpl(sin2 * dq.w * 2, dq.w * dq.w - sin2 * sin2) * INV_FRAME_TIME / sin2) : Vec3(0));
			pent->Action(&asv);
		}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

SMeshLodInfo CCharInstance::ComputeGeometricMean() const
{
	SMeshLodInfo lodInfo;
	lodInfo.Clear();

	const int attachmentCount = m_AttachmentManager.GetAttachmentCount();
	for (int i = 0; i < attachmentCount; ++i)
	{
		const IAttachment* const pIAttachment = m_AttachmentManager.GetInterfaceByIndex(i);

		if (pIAttachment != NULL && pIAttachment->GetIAttachmentObject() != NULL)
		{
			const IAttachmentObject* const pIAttachmentObject = pIAttachment->GetIAttachmentObject();
			SMeshLodInfo attachmentLodInfo;

			if (pIAttachmentObject->GetIAttachmentSkin())
			{
				attachmentLodInfo = pIAttachmentObject->GetIAttachmentSkin()->ComputeGeometricMean();
			}
			else if (pIAttachmentObject->GetIStatObj())
			{
				attachmentLodInfo = pIAttachmentObject->GetIStatObj()->ComputeGeometricMean();
			}
			else if (pIAttachmentObject->GetICharacterInstance())
			{
				attachmentLodInfo = pIAttachmentObject->GetICharacterInstance()->ComputeGeometricMean();
			}

			lodInfo.Merge(attachmentLodInfo);
		}
	}

	if (GetObjectType() == CGA)
	{
		// joints
		if (ISkeletonPose* pSkeletonPose = const_cast<CCharInstance*>(this)->GetISkeletonPose())
		{
			uint32 numJoints = GetIDefaultSkeleton().GetJointCount();

			// check StatObj attachments
			for (uint32 i = 0; i < numJoints; i++)
			{
				IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(i);
				if (pStatObj)
				{
					SMeshLodInfo attachmentLodInfo = pStatObj->ComputeGeometricMean();

					lodInfo.Merge(attachmentLodInfo);
				}
			}
		}
	}

	return lodInfo;
}

phys_geometry* CCharInstance::GetPhysGeom(int nType) const
{
	if (IPhysicalEntity* pPhysics = m_SkeletonPose.GetCharacterPhysics())
	{
		pe_params_part part;
		part.ipart = 0;
		if (pPhysics->GetParams(&part))
			return part.pPhysGeom;
	}
	return NULL;
}

IPhysicalEntity* CCharInstance::GetPhysEntity() const
{
	IPhysicalEntity* pPhysics = m_SkeletonPose.GetCharacterPhysics();
	if (!pPhysics)
		pPhysics = m_SkeletonPose.GetCharacterPhysics(0);
	return pPhysics;
}

float CCharInstance::GetExtent(EGeomForm eForm)
{
	// Sum extents from base mesh, CGA joints, and attachments.
	CGeomExtent& extent = m_Extents.Make(eForm);

	extent.Clear();
	extent.ReserveParts(3);

	// Add base model as first part.
	float fModelExt = 0.f;
	if (m_pDefaultSkeleton)
	{
		if (IRenderMesh* pMesh = m_pDefaultSkeleton->GetIRenderMesh())
			fModelExt = pMesh->GetExtent(eForm);
	}
	extent.AddPart(fModelExt);

	extent.AddPart(m_SkeletonPose.GetExtent(eForm));
	extent.AddPart(m_AttachmentManager.GetExtent(eForm));

	return extent.TotalExtent();
}

void CCharInstance::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	CGeomExtent const& ext = m_Extents[eForm];
	for (auto part : ext.RandomPartsAliasSum(points, seed))
	{
		if (part.iPart-- == 0)
		{
			// Base model.
			if (IRenderMesh* pMesh = m_pDefaultSkeleton->GetIRenderMesh())
			{
				SSkinningData* pSkinningData = NULL;
				int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
				for (int n = 0; n < 3; n++)
				{
					int nList = (nFrameID - n) % 3;
					if (arrSkinningRendererData[nList].nFrameID == nFrameID - n)
					{
						pSkinningData = arrSkinningRendererData[nList].pSkinningData;
						break;
					}
				}

				pMesh->GetRandomPoints(part.aPoints, seed, eForm, pSkinningData);
			}
			else
				part.aPoints.fill(ZERO);
		}

		else if (part.iPart-- == 0)
		{
			// Choose CGA joint.
			m_SkeletonPose.GetRandomPoints(part.aPoints, seed, eForm);
		}

		else if (part.iPart-- == 0)
		{
			// Choose attachment.
			m_AttachmentManager.GetRandomPoints(part.aPoints, seed, eForm);
		}
		else
			part.aPoints.fill(ZERO);
	}
}

void CCharInstance::OnDetach()
{
	pe_params_rope pr;
	pr.pEntTiedTo[0] = pr.pEntTiedTo[1] = 0;
	m_SkeletonPose.m_physics.SetAuxParams(&pr);
}

// Skinning

void CCharInstance::BeginSkinningTransformationsComputation(SSkinningData* pSkinningData)
{
	if (m_pDefaultSkeleton->m_ObjectType == CHR)
	{
		assert(pSkinningData->pMasterSkinningDataList);
		*pSkinningData->pMasterSkinningDataList = SKINNING_TRANSFORMATION_RUNNING_INDICATOR;

		TSkinningTransformationsComputation job(pSkinningData, m_pDefaultSkeleton.get(), 0, &m_SkeletonPose.GetPoseData(), &m_skinningTransformationsMovement);
		job.SetClassInstance(this);
		job.RegisterJobState(pSkinningData->pAsyncJobs);
		job.SetPriorityLevel(JobManager::eHighPriority);
		job.Run();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharInstance::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Character instance serialization");

		ser.BeginGroup("CCharInstance");
		ser.Value("fPlaybackScale", (float&)(m_fPlaybackScale));
		ser.EndGroup();

		m_SkeletonAnim.Serialize(ser);
		m_AttachmentManager.Serialize(ser);

		if (ser.IsReading())
		{
			// Make sure that serialized characters that are ragdoll are updated even if their physic entity is sleeping
			m_SkeletonPose.m_physics.m_bPhysicsWasAwake = true;
		}
	}
}

#ifdef EDITOR_PCDEBUGCODE
void CCharInstance::DrawWireframeStatic(const Matrix34& m34, int nLOD, uint32 color)
{
	(void)nLOD; // TODO

	CModelMesh* pModelMesh = m_pDefaultSkeleton->GetModelMesh();
	if (pModelMesh)
	{
		pModelMesh->DrawWireframeStatic(m34, color);
	}
}

void CCharInstance::ReloadCHRPARAMS()
{
	g_pCharacterManager->StopAnimationsOnAllInstances(*GetIAnimationSet());
	g_pCharacterManager->SyncAllAnimations();

	g_pCharacterManager->GetParamLoader().ClearLists();
	m_pDefaultSkeleton->m_pAnimationSet->ReleaseAnimationData();
	m_pDefaultSkeleton->SetModelAnimEventDatabase("");
	string paramFileNameBase(m_pDefaultSkeleton->GetModelFilePath());
	// Extended skeletons are identified with underscore prefix
	if (!paramFileNameBase.empty() && paramFileNameBase[0] == '_')
		paramFileNameBase.erase(0, 1);
	paramFileNameBase.replace(".chr", ".chrparams");
	paramFileNameBase.replace(".cga", ".chrparams");
	CryLog("Reloading %s", paramFileNameBase.c_str());
	m_pDefaultSkeleton->m_pAnimationSet->NotifyListenersAboutReloadStarted();
	m_pDefaultSkeleton->LoadCHRPARAMS(paramFileNameBase.c_str());
	m_pDefaultSkeleton->m_pAnimationSet->NotifyListenersAboutReload();

	//--------------------------------------------------------------------------
	//--- check if there is an attached SKEL and reload its list as well
	//--------------------------------------------------------------------------
	uint32 numAttachments = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i = 0; i < numAttachments; i++)
	{
		IAttachment* pIAttachment = m_AttachmentManager.m_arrAttachments[i];
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject == 0)
			continue;

		uint32 type = pIAttachment->GetType();
		CCharInstance* pCharInstance = (CCharInstance*)pIAttachmentObject->GetICharacterInstance();
		if (pCharInstance == 0)
			continue;  //its not a CHR at all

		//reload Animation list for an attached character (attached SKELs are a rare case)
		g_pCharacterManager->GetParamLoader().ClearLists();
		m_pDefaultSkeleton->m_pAnimationSet->ReleaseAnimationData();
		string paramFileName(m_pDefaultSkeleton->GetModelFilePath());
		paramFileName.replace(".chr", ".chrparams");
		CryLog("Reloading %s", paramFileName.c_str());
		m_pDefaultSkeleton->m_pAnimationSet->NotifyListenersAboutReloadStarted();
		m_pDefaultSkeleton->LoadCHRPARAMS(paramFileName.c_str());
		m_pDefaultSkeleton->m_pAnimationSet->NotifyListenersAboutReload();
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
IFacialInstance* CCharInstance::GetFacialInstance()
{
	return m_pFacialInstance;
};

//////////////////////////////////////////////////////////////////////////
const IFacialInstance* CCharInstance::GetFacialInstance() const
{
	return m_pFacialInstance;
};

//////////////////////////////////////////////////////////////////////////
void CCharInstance::EnableFacialAnimation(bool bEnable)
{
	m_bFacialAnimationEnabled = bEnable;

	for (int attachmentIndex = 0, end = int(m_AttachmentManager.m_arrAttachments.size()); attachmentIndex < end; ++attachmentIndex)
	{
		IAttachment* pIAttachment = m_AttachmentManager.m_arrAttachments[attachmentIndex];
		IAttachmentObject* pIAttachmentObject = (pIAttachment ? pIAttachment->GetIAttachmentObject() : 0);
		ICharacterInstance* pIAttachmentCharacter = (pIAttachmentObject ? pIAttachmentObject->GetICharacterInstance() : 0);
		if (pIAttachmentCharacter)
			pIAttachmentCharacter->EnableFacialAnimation(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharInstance::EnableProceduralFacialAnimation(bool bEnable)
{
	IFacialInstance* pInst = GetFacialInstance();
	if (pInst)
		pInst->EnableProceduralFacialAnimation(bEnable);
}

void CCharInstance::SkeletonPostProcess()
{
	f32 fZoomAdjustedDistanceFromCamera = m_fPostProcessZoomAdjustedDistanceFromCamera;
	QuatTS rPhysLocation = m_location;
	m_SkeletonPose.SkeletonPostProcess(m_SkeletonPose.GetPoseDataExplicitWriteable());
	m_skeletonEffectManager.Update(&m_SkeletonAnim, &m_SkeletonPose, rPhysLocation);
};

void CCharInstance::SkinningTransformationsComputation(SSkinningData* pSkinningData, CDefaultSkeleton* pDefaultSkeleton, int nRenderLod, const Skeleton::CPoseData* pPoseData, f32* pSkinningTransformationsMovement)
{
	DEFINE_PROFILER_FUNCTION();
	assert(pSkinningData->pBoneQuatsS);
	assert(pSkinningData->pPreviousSkinningRenderData);
	assert(pSkinningData->pPreviousSkinningRenderData->pBoneQuatsS);

	DualQuat* pSkinningTransformations = pSkinningData->pBoneQuatsS;
	DualQuat* pSkinningTransformationsPrevious = pSkinningData->pPreviousSkinningRenderData->pBoneQuatsS;

	//	float fColor[4] = {1,0,0,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SkinningTransformationsComputationTask" );
	//	g_YLine+=16.0f;

	const QuatT* pJointsAbsolute = pPoseData->GetJointsAbsolute();
	const QuatT* pJointsAbsoluteDefault = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsolute();

	uint32 jointCount = pDefaultSkeleton->GetJointCount();
	const CDefaultSkeleton::SJoint* pJoints = &pDefaultSkeleton->m_arrModelJoints[0];

	QuatT defaultInverse;
	//--- Deliberate querying off the end of arrays for speed, prefetching off the end of an array is safe & avoids extra branches
	const QuatT* absPose_PreFetchOnly = &pJointsAbsolute[0];
#ifndef _DEBUG
	CryPrefetch(&pJointsAbsoluteDefault[0]);
	CryPrefetch(&absPose_PreFetchOnly[0]);
	CryPrefetch(&pSkinningTransformations[0]);
	CryPrefetch(&pJointsAbsoluteDefault[1]);
	CryPrefetch(&absPose_PreFetchOnly[4]);
	CryPrefetch(&pSkinningTransformations[4]);
#endif
	defaultInverse = pJointsAbsoluteDefault[0].GetInverted();
	pSkinningTransformations[0] = pJointsAbsolute[0] * defaultInverse;
	f32& movement = *pSkinningTransformationsMovement;
	movement = 0.0f;
	for (uint32 i = 1; i < jointCount; ++i)
	{
#ifndef _DEBUG
		CryPrefetch(&pJointsAbsoluteDefault[i + 1]);
		CryPrefetch(&absPose_PreFetchOnly[i + 4]);
		CryPrefetch(&pSkinningTransformations[i + 4]);
		CryPrefetch(&pJoints[i + 1].m_idxParent);
#endif
		defaultInverse = pJointsAbsoluteDefault[i].GetInverted();

		DualQuat& qd = pSkinningTransformations[i];
		qd = pJointsAbsolute[i] * defaultInverse;

		int32 p = pJoints[i].m_idxParent;
		if (p > -1)
		{
			f32 cosine = qd.nq | pSkinningTransformations[p].nq;
			f32 mul = (f32)__fsel(cosine, 1.0f, -1.0f);
			qd.nq *= mul;
			qd.dq *= mul;
		}

		const Quat& q0 = pSkinningTransformations[i].nq;
		const Quat& q1 = pSkinningTransformationsPrevious[i].nq;
		f32 fQdot = q0 | q1;
		f32 fQdotSign = (f32)__fsel(fQdot, 1.0f, -1.0f);
		movement += 1.0f - (fQdot * fQdotSign);
	}

	CAttachmentManager* pAttachmentManager = static_cast<CAttachmentManager*>(GetIAttachmentManager());
	const uint32 numJoints = pDefaultSkeleton->m_arrModelJoints.size();
	const uint32 extraBonesCount = pAttachmentManager->GetExtraBonesCount();
	for (uint32 i = 0; i < extraBonesCount; ++i)
	{
		if (IAttachment* pAttachment = pAttachmentManager->m_extraBones[i])
		{
			if (pAttachment->IsMerged())
			{
				if (pAttachment->GetType() == CA_FACE || (pAttachment->GetType() == CA_BONE && static_cast<CAttachmentBONE*>(pAttachment)->m_nJointID != -1))
				{
					DualQuat& dq = pSkinningTransformations[i + numJoints];
					dq = pAttachment->GetAttModelRelative() * pAttachment->GetAdditionalTransformation();
				}
			}
		}
	}

	assert(pSkinningData->pMasterSkinningDataList);

	// set the list to NULL to indicate the mainthread that the skinning transformation job has finished
	SSkinningData* pSkinningJobList = NULL;
	do
	{
		pSkinningJobList = *(const_cast<SSkinningData* volatile*>(pSkinningData->pMasterSkinningDataList));
	}
	while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(pSkinningData->pMasterSkinningDataList), NULL, pSkinningJobList) != pSkinningJobList);

	// start SW-Skinning Job till the list is empty
	while (pSkinningJobList != SKINNING_TRANSFORMATION_RUNNING_INDICATOR)
	{
		SVertexAnimationJob* pVertexAnimation = static_cast<SVertexAnimationJob*>(pSkinningJobList->pCustomData);
		pVertexAnimation->Begin(pSkinningJobList->pAsyncJobs);

		pSkinningJobList = pSkinningJobList->pNextSkinningData;
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CCharInstance::SizeOfCharInstance() const
{
	if (ICrySizer* pSizer = gEnv->pSystem->CreateSizer())
	{
		pSizer->AddObject(this);
		pSizer->End();

		const auto totalSize = pSizer->GetTotalSize();

		pSizer->Release();

		return totalSize;
	}
	else
	{
		return 0;
	}
}

void CCharInstance::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObjectSize(this);

	pSizer->AddObjectSize(m_pFacialInstance.get());
	pSizer->AddObject(m_strFilePath);

	{
		SIZER_COMPONENT_NAME(pSizer, "CAttachmentManager");
		pSizer->AddObject(m_AttachmentManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "skeletonEffectManager");
		pSizer->AddObject(m_skeletonEffectManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "FacialInstance");
		pSizer->AddObject(m_pFacialInstance);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SkeletonAnim");
		pSizer->AddObject(m_SkeletonAnim);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SkeletonPose");
		pSizer->AddObject(m_SkeletonPose);
	}

	{
		// TODO: The m_Extents member allocates dynamic data and should also be included in the computation.
	}
}

CharacterInstanceProcessing::SContext* CCharInstance::GetProcessingContext()
{
	if (m_processingContext >= 0)
	{
		return &g_pCharacterManager->GetContextSyncQueue().GetContext(m_processingContext);
	}
	return nullptr;
}

void CCharInstance::WaitForSkinningJob() const
{
	// wait for the skinning transformation from the last frame to finish
	// *note* the skinning pool id is increased in EF_Start, which is called during the frame
	// after the CommandBuffer Job, thus we need the current id to get the skinning data from the last frame
	int nFrameID = gEnv->pRenderer ? gEnv->pRenderer->EF_GetSkinningPoolID() : 0;
	int nList = nFrameID % 3;
	SSkinningData* pSkinningData = arrSkinningRendererData[nList].pSkinningData;
	if (arrSkinningRendererData[nList].nFrameID == nFrameID && pSkinningData && pSkinningData->pAsyncJobs)
		gEnv->pJobManager->WaitForJob(*pSkinningData->pAsyncJobs);
}

void CCharInstance::SetupThroughParent(const CCharInstance* pParent)
{
	CRY_ASSERT(pParent != nullptr);
	m_fPostProcessZoomAdjustedDistanceFromCamera =
	  pParent->m_fPostProcessZoomAdjustedDistanceFromCamera;
	m_fOriginalDeltaTime = pParent->m_fOriginalDeltaTime;
	m_fDeltaTime = pParent->m_fDeltaTime;
	m_nAnimationLOD = pParent->GetAnimationLOD();
	m_SkeletonPose.m_bFullSkeletonUpdate = pParent->m_SkeletonPose.m_bFullSkeletonUpdate;
	m_SkeletonPose.m_bInstanceVisible = pParent->m_SkeletonPose.m_bInstanceVisible;
}

void CCharInstance::SetupThroughParams(const SAnimationProcessParams* pParams)
{
	CRY_ASSERT(pParams != nullptr);
	static ICVar* pUseSinglePositionVar = gEnv->pConsole->GetCVar("g_useSinglePosition");
	if (pUseSinglePositionVar && pUseSinglePositionVar->GetIVal() & 4)
	{
		m_SkeletonPose.m_physics.SynchronizeWithPhysics(
		  m_SkeletonPose.GetPoseDataExplicitWriteable());
	}

	m_location = pParams->locationAnimation; // this the current location
	m_fPostProcessZoomAdjustedDistanceFromCamera =
	  pParams->zoomAdjustedDistanceFromCamera;

	uint32 nErrorCode = 0;
	uint32 minValid = m_SkeletonPose.m_AABB.min.IsValid();
	if (minValid == 0)
		nErrorCode |= 0x8000;
	uint32 maxValid = m_SkeletonPose.m_AABB.max.IsValid();
	if (maxValid == 0)
		nErrorCode |= 0x8000;
	if (Console::GetInst().ca_SaveAABB && nErrorCode)
	{
		m_SkeletonPose.m_AABB.max = Vec3(2, 2, 2);
		m_SkeletonPose.m_AABB.min = Vec3(-2, -2, -2);
	}

	float fNewDeltaTime = clamp_tpl(g_pITimer->GetFrameTime(), 0.0f, 0.5f);
	fNewDeltaTime =
	  (float)__fsel(pParams->overrideDeltaTime, pParams->overrideDeltaTime, fNewDeltaTime);
	m_fOriginalDeltaTime = fNewDeltaTime;
	m_fDeltaTime = fNewDeltaTime * m_fPlaybackScale;

	//---------------------------------------------------------------------------------

	m_SkeletonPose.m_bFullSkeletonUpdate = false;
	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; // g_pIRenderer->GetFrameID(false);
	uint32 dif = nCurrentFrameID - m_LastRenderedFrameID;
	m_SkeletonPose.m_bInstanceVisible =
	  (dif < 5) || m_SkeletonAnim.GetTrackViewStatus();

	if (m_SkeletonPose.m_bInstanceVisible)
		m_SkeletonPose.m_bFullSkeletonUpdate = true;
	if (m_SkeletonAnim.m_TrackViewExclusive)
		m_SkeletonPose.m_bFullSkeletonUpdate = true;
	if (m_SkeletonPose.m_nForceSkeletonUpdate)
		m_SkeletonPose.m_bFullSkeletonUpdate = true;
	if (Console::GetInst().ca_ForceUpdateSkeletons != 0)
		m_SkeletonPose.m_bFullSkeletonUpdate = true;
}

//////////////////////////////////////////////////////////////////////////
void CCharInstance::PerFrameUpdate()
{
	if (m_rpFlags & CS_FLAG_UPDATE)
	{
		if ((m_rpFlags & CS_FLAG_UPDATE_ALWAYS) ||
				(m_rpFlags & CS_FLAG_RENDER_NODE_VISIBLE))
		{
			// If we need to be updated always, or our render node is potentially visible.
			// Animation should start

			const CCamera& camera = GetISystem()->GetViewCamera();
			float fDistance = (camera.GetPosition() - m_location.t).GetLength();
			float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

			SAnimationProcessParams params;
			params.locationAnimation = m_location;
			params.bOnRender = 0;
			params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
			StartAnimationProcessing(params);
		}
	}
}
