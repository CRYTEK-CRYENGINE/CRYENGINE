// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VertexCommandBuffer.h"

#include <CryThreading/IJobManager_JobDelegator.h>
#include "../ModelMesh.h"
#include "VertexData.h"
#include "VertexCommand.h"

/*
   CVertexCommandBufferAllocationCounter
 */

CVertexCommandBufferAllocationCounter::CVertexCommandBufferAllocationCounter() :
	m_count(0)
{
}

CVertexCommandBufferAllocationCounter::~CVertexCommandBufferAllocationCounter()
{
}

// CVertexCommandBufferAllocator

void* CVertexCommandBufferAllocationCounter::Allocate(const uint length)
{
	m_count += length;
	return NULL;
}

/*
   CVertexCommandBufferAllocatorStatic
 */

CVertexCommandBufferAllocatorStatic::CVertexCommandBufferAllocatorStatic(const void* pMemory, const uint length) :
	m_pMemory((uint8*)pMemory),
	m_memoryLeft(length)
{
}

CVertexCommandBufferAllocatorStatic::~CVertexCommandBufferAllocatorStatic()
{
}

// CVertexCommandBufferAllocator

void* CVertexCommandBufferAllocatorStatic::Allocate(const uint length)
{
	if (length > m_memoryLeft)
		return NULL;

	void* pAllocation = m_pMemory;
	m_pMemory += length;
	m_memoryLeft -= length;
	return pAllocation;
}

/*
   CVertexCommandBuffer
 */

void CVertexCommandBuffer::Process(CVertexData& vertexData)
{
	int length = m_commandsLength;
	if (!length)
		return;

	const uint8* pCommands = m_pCommands;
	while (length > sizeof(VertexCommand))
	{
		VertexCommand* pCommand = (VertexCommand*)pCommands;
		pCommands += pCommand->length;
		length -= int(pCommand->length);
		pCommand->Execute(*pCommand, vertexData);
	}
}

/*
   SVertexAnimationJobData
 */

DECLARE_JOB("VertexAnimation", TVertexAnimation, SVertexAnimationJob::Execute);

void SVertexAnimationJob::Execute(int)
{
	if (commandBufferLength)
		commandBuffer.Process(vertexData);

	if (m_previousRenderMesh)
	{
		m_previousRenderMesh->UnlockStream(VSF_GENERAL);
		m_previousRenderMesh->UnLockForThreadAccess();
	}

	m_previousRenderMesh = NULL;

	CryInterlockedDecrement(pRenderMeshSyncVariable);
}

void SVertexAnimationJob::Begin(JobManager::SJobState* pJob)
{
	TVertexAnimation job(0);
	job.SetClassInstance(this);
	job.SetPriorityLevel(JobManager::eRegularPriority);
	job.RegisterJobState(pJob);
	job.Run();
}
