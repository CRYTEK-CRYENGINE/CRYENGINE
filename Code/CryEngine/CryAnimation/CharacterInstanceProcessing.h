// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/ICryAnimation.h>
#include "SkeletonAnim.h"
#include "CharacterInstance.h"

class CAttachmentBONE;
class CAttachmentManager;
namespace Memory {
class CPoolFrameLocal;
};

namespace CharacterInstanceProcessing
{
static const int kMaxNumberOfCharacterInstanceProcessingContexts = 8192;

class CJob
{
public:
	CJob(SContext* pCtx) : m_pCtx(pCtx) {}
public:

	void Begin(bool bImmediate);
	void Wait() const;

	void Job_Execute();
	void Execute(bool bImmediate);
private:
	SContext*                     m_pCtx;
	mutable JobManager::SJobState m_jobState;
};

struct SContext
{
	SContext()
		: pInstance(nullptr)
		, pBone(nullptr)
		, pParent(nullptr)
		, numChildren(0)
		, slot(-1)
		, pCommandBuffer(nullptr)
		, job(nullptr)
		, state(EState::Unstarted) 
	{
	}

	void Initialize(CCharInstance* pInst, const CAttachmentBONE* pBone, const CCharInstance* pParent, int numChildren);

	//! Checks if computation of this processing context is currently in progress.
	//! \return True if computation is still in progress. False if computation did not yet start or already finished for the current frame.
	bool IsInProgress() const;

	_smart_ptr<CCharInstance> pInstance;
	const CAttachmentBONE*    pBone;
	const CCharInstance*      pParent;

	int                       numChildren;
	int                       slot;

	Command::CBuffer*         pCommandBuffer;
	CJob                      job;

	enum class EState
	{
		Unstarted,
		StartAnimationProcessed,
		JobExecuted,
		JobSkipped,
		Finished,
		Failure
	};

	EState state;
};

class CContextQueue
{
public:
	CContextQueue() : m_numContexts(0) { ClearContexts(); }
	template<class Function>
	void ExecuteForDirectChildren(int parentIndex, Function f)
	{
		const int start = parentIndex + 1;
		const int end = start + m_contexts[parentIndex].numChildren;
		for (int i = start; i < end; ++i)
		{
			auto& ctx = m_contexts[i];
			ctx.state = f(ctx);
			i += ctx.numChildren;
		}
	}
	template<class Function>
	void ExecuteForDirectChildrenWithoutStateChange(int parentIndex, Function f)
	{
		const int start = parentIndex + 1;
		const int end = start + m_contexts[parentIndex].numChildren;
		for (int i = start; i < end; ++i)
		{
			auto& ctx = m_contexts[i];
			f(ctx);
			i += ctx.numChildren;
		}
	}
	template<class Function>
	void ExecuteForContextAndAllChildrenRecursively(int parentIndex, Function f)
	{
		const int start = parentIndex;
		const int end = start + m_contexts[parentIndex].numChildren + 1;
		for (int i = start; i < end; ++i)
		{
			auto& ctx = m_contexts[i];
			ctx.state = f(ctx);
		}
	}
	template<class Function>
	void ExecuteForContext(int index, Function f)
	{
		auto& ctx = m_contexts[index];
		ctx.state = f(ctx);
	}
	template<class Function>
	void ExecuteForAll(Function f)
	{
		const int num = m_numContexts;
		for (int i = 0; i < num; ++i)
		{
			auto& ctx = m_contexts[i];
			ctx.state = f(ctx);
		}
	}
	SContext& AppendContext();
	void      ClearContexts();
	int       GetNumberOfContexts() const { return m_numContexts; };

	SContext& GetContext(int i)           { return m_contexts[i]; }
private:
	CContextQueue(const CContextQueue& other);      // non construction-copyable
	CContextQueue& operator=(const CContextQueue&); // non copyable

	StaticArray<SContext, kMaxNumberOfCharacterInstanceProcessingContexts> m_contexts;

	int m_numContexts;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// In the following, these are the different update phases of the character instance processing //
//////////////////////////////////////////////////////////////////////////////////////////////////

struct SStartAnimationProcessing
{
	SStartAnimationProcessing() : m_pParams(nullptr) {}
	SStartAnimationProcessing(const SAnimationProcessParams& params) : m_pParams(&params) {}
	SContext::EState operator()(const SContext& ctx);

private:
	const SAnimationProcessParams* m_pParams;
};

struct SExecuteJob
{
	SContext::EState operator()(const SContext& ctx);
};

struct SFinishAnimationComputations
{
	SContext::EState operator()(const SContext& ctx);
};

bool                     InitializeMemoryPool();
Memory::CPoolFrameLocal* GetMemoryPool();
}
