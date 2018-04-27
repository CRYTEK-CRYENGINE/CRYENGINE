// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CharacterInstanceProcessing.h"

#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "Memory/Memory.h"
#include "Memory/Pool.h"
#include "Command_Buffer.h"
#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("CommandBufferExecute", TCommandBufferExecuteJob, CharacterInstanceProcessing::CJob::Job_Execute);

namespace CharacterInstanceProcessing
{

SContext::EState SStartAnimationProcessing::operator()(const SContext& ctx)
{
	DEFINE_PROFILER_FUNCTION();
	CRY_ASSERT(ctx.state == SContext::EState::Unstarted);

	if (ctx.pParent)
	{
		ctx.pInstance->SetupThroughParent(ctx.pParent);
	}
	else
	{
		ctx.pInstance->SetupThroughParams(m_pParams);
	}

	const uint32 wasAnimPlaying = ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying;

	ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying = false;
	QuatTS location = ctx.pInstance->m_location;

	ctx.pInstance->m_SkeletonAnim.ProcessAnimations(location);

	if (!GetMemoryPool())
		return SContext::EState::Failure;

	CSkeletonPose* pSkeletonPose = ctx.pInstance->m_SkeletonAnim.m_pSkeletonPose;
	if (!pSkeletonPose->PreparePoseDataAndLocatorWriteables(*GetMemoryPool()))
		return SContext::EState::Failure;

	ctx.pInstance->m_SkeletonAnim.PoseModifiersPrepare(location);

	auto result = ctx.pInstance->m_SkeletonAnim.FillCommandBuffer(location, *ctx.pCommandBuffer);
	ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying = (result == CSkeletonAnim::EFillCommandBufferResult::AnimationPlaying);

	pSkeletonPose->m_physics.SetLocation(location);
	pSkeletonPose->m_physics.Job_SynchronizeWithPhysicsPrepare(*GetMemoryPool());

	if (wasAnimPlaying && !ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying && ctx.pInstance->GetObjectType() == CGA)
	{
		pSkeletonPose->m_physics.RequestForcedPostSynchronization();
	}

	return SContext::EState::StartAnimationProcessed;
}

SContext::EState SExecuteJob::operator()(const SContext& ctx)
{
	DEFINE_PROFILER_FUNCTION();
	CRY_ASSERT(ctx.pInstance != nullptr);
	CRY_ASSERT(ctx.pInstance->GetRefCount() > 0);

	if (ctx.state != SContext::EState::StartAnimationProcessed)
		return SContext::EState::Failure;

	if (ctx.pParent)
	{
		CRY_ASSERT(ctx.pBone != nullptr);
		ctx.pInstance->m_location = ctx.pBone->GetAttWorldAbsolute();
	}

	CSkeletonAnim& skelAnim = ctx.pInstance->m_SkeletonAnim;
	if (!skelAnim.m_pSkeletonPose->m_bFullSkeletonUpdate)
		return SContext::EState::JobSkipped;

	CSkeletonPose* pSkeletonPose = skelAnim.m_pSkeletonPose;
	if (!pSkeletonPose->GetPoseDataWriteable())
		return SContext::EState::Failure;

	if (!ctx.pCommandBuffer)
		return SContext::EState::Failure;

	ctx.pCommandBuffer->Execute();

	pSkeletonPose->GetPoseData().Validate(*pSkeletonPose->m_pInstance->m_pDefaultSkeleton);

	pSkeletonPose->m_pInstance->m_AttachmentManager.UpdateLocationsExceptExecute(
		*pSkeletonPose->GetPoseDataWriteable());

	pSkeletonPose->m_pInstance->m_AttachmentManager.UpdateLocationsExecute(
	  *pSkeletonPose->GetPoseDataWriteable());

	return SContext::EState::JobExecuted;
}

SContext::EState SFinishAnimationComputations::operator()(const SContext& ctx)
{
	DEFINE_PROFILER_FUNCTION();
	CRY_ASSERT(ctx.pInstance != nullptr);
	CRY_ASSERT(ctx.pInstance->GetRefCount() > 0);

	// Finish Animation Computations has been called explicitly already
	// earlier in the frame, so we don't need to process it again
	if (ctx.state == SContext::EState::Finished)
		return ctx.state;

	CRY_ASSERT(ctx.state != SContext::EState::StartAnimationProcessed);
	CRY_ASSERT(ctx.state != SContext::EState::Failure);

	// make sure that the job has been run or has been skipped
	// for some reason (usually because we did not request a full skeleton update)
	if (ctx.state == SContext::EState::JobExecuted || ctx.state == SContext::EState::JobSkipped)
	{
		ctx.pInstance->m_SkeletonAnim.m_pSkeletonPose->SynchronizePoseDataAndLocatorWriteables();

		ctx.pInstance->m_SkeletonAnim.m_pSkeletonPose->GetPoseData().ValidateRelative(
		  *ctx.pInstance->m_pDefaultSkeleton);

		// check if the oldest animation in the queue is still needed
		for (uint32 nVLayerNo = 0; nVLayerNo < numVIRTUALLAYERS; nVLayerNo++)
		{
			DynArray<CAnimation>& rAnimations =
			  ctx.pInstance->m_SkeletonAnim.m_layers[nVLayerNo].m_transitionQueue.m_animations;

			if (rAnimations.size() > 0 && rAnimations[0].GetRemoveFromQueue())
				ctx.pInstance->m_SkeletonAnim.RemoveAnimFromFIFO(nVLayerNo, 0, true);
		}

		// if the full job was executed, we do SkeletonPostProcess, otherwise we just do a
		// lightweight update to support fall&play
		if (ctx.state == SContext::EState::JobExecuted)
		{

			ctx.pInstance->m_SkeletonAnim.m_pSkeletonPose->GetPoseData().ValidateRelative(
			  *ctx.pInstance->m_pDefaultSkeleton);

			QuatTS rPhysLocation = ctx.pInstance->m_location;
			ctx.pInstance->m_SkeletonPose.SkeletonPostProcess(
			  ctx.pInstance->m_SkeletonPose.GetPoseDataExplicitWriteable());
			ctx.pInstance->m_skeletonEffectManager.Update(
			  &ctx.pInstance->m_SkeletonAnim,
			  &ctx.pInstance->m_SkeletonPose,
			  rPhysLocation);

			ctx.pInstance->m_SkeletonAnim.m_transformPinningPoseModifier.reset();
		}
		else
		{
			// This is currently not working 100%, if you look away while the character
			// is ragdollized, he will not blend into animation again.

			// [*DavidR | 24/Jan/2011] Update Fall and Play's standup timer even when the postprocess step
			// is not invoked (usually when it is offscreen),
			// otherwise it would cause the character's pose to blend during 1 second after it reenters
			// the frustrum even if the standup anim has finished
			// (see CSkeletonPose::ProcessPhysicsFallAndPlay)
			// [*DavidR | 24/Jan/2011] ToDo: We may want to update more timers (e.g., lying timer) the
			// same way
			ctx.pInstance->m_SkeletonPose.m_physics.m_timeStandingUp +=
			  static_cast<float>(__fsel(ctx.pInstance->m_SkeletonPose.m_physics.m_timeStandingUp,
			                            ctx.pInstance->m_fOriginalDeltaTime, 0.0f));
			ctx.pInstance->m_AttachmentManager.UpdateLocationsExecuteUnsafe(
			  ctx.pInstance->m_SkeletonPose.GetPoseDataExplicitWriteable());
			ctx.pInstance->m_SkeletonAnim.PoseModifiersSwapBuffersAndClearActive();
		}
	}

	ctx.pInstance->m_AttachmentManager.UpdateBindings();

	ctx.pInstance->ClearProcessingContext();

	return SContext::EState::Finished;
}

void SContext::Initialize(CCharInstance* _pInst, const CAttachmentBONE* _pBone, const CCharInstance* _pParent, int _numChildren)
{
	pInstance = _pInst;
	pBone = _pBone;
	pParent = _pParent;
	numChildren = _numChildren;
	pCommandBuffer = (Command::CBuffer*)CharacterInstanceProcessing::GetMemoryPool()->Allocate(sizeof(Command::CBuffer));
	job = CJob(this);
}

bool SContext::IsInProgress() const
{
	return (state != EState::Unstarted) 
		&& (state != EState::Finished) 
		&& (state != EState::Failure);
}

SContext& CContextQueue::AppendContext()
{
	CRY_ASSERT(m_numContexts < kMaxNumberOfCharacterInstanceProcessingContexts - 1);
	m_contexts[m_numContexts] = SContext();
	m_contexts[m_numContexts].slot = m_numContexts;
	return m_contexts[m_numContexts++];
}

void CContextQueue::ClearContexts()
{
	for (int i = 0; i < m_numContexts; ++i)
	{
		auto& ctx = m_contexts[i];
		ctx.pInstance.reset();
		ctx.pBone = nullptr;
		ctx.pParent = nullptr;
		ctx.numChildren = 0;
		ctx.pCommandBuffer = nullptr;
		CRY_ASSERT(ctx.state == SContext::EState::Finished);
		ctx.state = SContext::EState::Unstarted;
	}

	m_numContexts = 0;
}

void CJob::Begin(bool bImmediate)
{
	if (m_jobState.IsRunning())
	{
		CryFatalError("Animation Job started while already running!");
		return;
	}

	if (bImmediate)
	{
		Execute(true);
		return;
	}

	TCommandBufferExecuteJob job;
	job.SetClassInstance(this);
	job.RegisterJobState(&m_jobState);
	job.Run();
}

void CJob::Wait() const
{
	DEFINE_PROFILER_FUNCTION();

	if (!m_jobState.IsRunning())
		return;
	// wait for task to finish
	gEnv->GetJobManager()->WaitForJob(m_jobState);
}

void CJob::Job_Execute()
{
	// in the extremely unlikely case that there
	// are two different character instances during the same frame
	// that got the same address, the last one will be the only one
	// that gets its AuxGeom drawn
	Execute(false);
	if (gEnv->pRenderer)
	{
		//gEnv->pRenderer->GetIRenderAuxGeom()->Submit(1);
	}
}

void CJob::Execute(bool bImmediate)
{
	CharacterInstanceProcessing::CContextQueue& queue = g_pCharacterManager->GetContextSyncQueue();

	CRY_ASSERT(m_pCtx->slot >= 0);
	queue.ExecuteForContext(m_pCtx->slot, CharacterInstanceProcessing::SExecuteJob());
	queue.ExecuteForDirectChildrenWithoutStateChange(
	  m_pCtx->slot, [bImmediate](CharacterInstanceProcessing::SContext& ctx)
		{
			ctx.pInstance->WaitForSkinningJob();
			ctx.job.Begin(bImmediate);
	  });
}

Memory::CPoolFrameLocal* s_pMemoryPool = NULL;

bool InitializeMemoryPool()
{
	Memory::CContext& memoryContext = CAnimationContext::Instance().GetMemoryContext();
	s_pMemoryPool = Memory::CPoolFrameLocal::Create(memoryContext, 256000);
	if (!s_pMemoryPool)
		return false;

	return true;
}

Memory::CPoolFrameLocal* GetMemoryPool()
{
	return s_pMemoryPool;
}

}
