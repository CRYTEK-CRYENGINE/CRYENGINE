// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

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

	CRY_ASSERT(ctx.pInstance != nullptr);
	CRY_ASSERT(ctx.pInstance->GetRefCount() > 0);
	CRY_ASSERT(ctx.pInstance->GetProcessingContext() == &ctx);

	if (ctx.parentSlot >= 0)
	{
		const CCharInstance* pParent = CRY_VERIFY(g_pCharacterManager->GetContextSyncQueue().GetContext(ctx.parentSlot).pInstance);

		ctx.pInstance->SetupThroughParent(pParent);

		// Anticipated skip for quasi-static objects
		// This allows to skip the animation update for those objects which just stay in the same pose / animation loop for a long time
		ctx.pInstance->AdvanceQuasiStaticSleepTimer();
		if (ctx.pInstance->IsQuasiStaticSleeping())
		{
			g_pCharacterManager->Debug_IncreaseQuasiStaticCullCounter();
			return SContext::EState::JobCulled;
		}
	}

	const uint32 wasAnimPlaying = ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying;

	ctx.pInstance->m_SkeletonAnim.m_IsAnimPlaying = false;
	QuatTS location = ctx.pInstance->m_location;

	// Animation queue update
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

	// Skip rest of the update (pose update) for objects that are not in view
	if (!ctx.pInstance->m_SkeletonPose.m_bFullSkeletonUpdate)
	{
		return SContext::EState::JobSkipped;
	}

	return SContext::EState::StartAnimationProcessed;
}

SContext::EState SExecuteJob::operator()(const SContext& ctx)
{
	DEFINE_PROFILER_FUNCTION();

	if (ctx.state != SContext::EState::StartAnimationProcessed)
		return SContext::EState::Failure;

	CRY_ASSERT(ctx.pInstance != nullptr);
	CRY_ASSERT(ctx.pInstance->GetRefCount() > 0);
	CRY_ASSERT(ctx.pInstance->GetProcessingContext() == &ctx);

	CSkeletonAnim& skelAnim = ctx.pInstance->m_SkeletonAnim;
	CRY_ASSERT_MESSAGE(skelAnim.m_pSkeletonPose->m_bFullSkeletonUpdate,   "The current job should have been skipped!");
	CRY_ASSERT_MESSAGE(ctx.state != SContext::EState::JobSkipped,         "The current job should have been skipped!");

	CSkeletonPose* pSkeletonPose = skelAnim.m_pSkeletonPose;
	if (!pSkeletonPose->GetPoseDataWriteable())
		return SContext::EState::Failure;

	if (!ctx.pCommandBuffer)
		return SContext::EState::Failure;

	ctx.pCommandBuffer->Execute();

	pSkeletonPose->GetPoseData().Validate(*pSkeletonPose->m_pInstance->m_pDefaultSkeleton);

	pSkeletonPose->m_pInstance->m_AttachmentManager.UpdateSockets(*pSkeletonPose->GetPoseDataWriteable());

	return SContext::EState::JobExecuted;
}

SContext::EState SFinishAnimationComputations::operator()(const SContext& ctx)
{
	DEFINE_PROFILER_FUNCTION();

	// Finish Animation Computations has been called explicitly already
	// earlier in the frame, so we don't need to process it again
	if (ctx.state == SContext::EState::Finished)
		return ctx.state;

	CRY_ASSERT(ctx.pInstance != nullptr);
	CRY_ASSERT(ctx.pInstance->GetRefCount() > 0);
	CRY_ASSERT(ctx.pInstance->GetProcessingContext() == &ctx);

	CRY_ASSERT(ctx.state != SContext::EState::StartAnimationProcessed);
	CRY_ASSERT(ctx.state != SContext::EState::Failure);

	if (ctx.parentSlot >= 0)
	{
		const CCharInstance* pParent = CRY_VERIFY(g_pCharacterManager->GetContextSyncQueue().GetContext(ctx.parentSlot).pInstance);

		if (const IAttachment* pAttachment = pParent->m_AttachmentManager.GetInterfaceByNameCRC(ctx.attachmentNameCRC))
		{
			ctx.pInstance->m_location = pAttachment->GetAttWorldAbsolute();
		}
	}

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
			ctx.pInstance->m_AttachmentManager.UpdateAttachedObjects();
			ctx.pInstance->m_SkeletonAnim.PoseModifiersSwapBuffersAndClearActive();
		}
	}

	ctx.pInstance->ClearProcessingContext();

	return SContext::EState::Finished;
}

void SContext::Initialize(CCharInstance* _pInst, const IAttachment* _pAttachment, int _parentSlot, int _numChildren)
{
	pInstance = _pInst;
	attachmentNameCRC = _pAttachment ? _pAttachment->GetNameCRC() : 0;
	numChildren = _numChildren;
	parentSlot = _parentSlot;
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
		ctx.attachmentNameCRC = 0;
		ctx.numChildren = 0;
		ctx.parentSlot = -1;
		ctx.slot = -1;
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
