// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   objmancullqueue.h
//  Version:     v1.00
//  Created:     2/12/2009 by Michael Glueck
//  Compilers:   Visual Studio.NET
//  Description: Declaration and entry point for asynchronous obj-culling queue
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef CObjManCullQueue_H
#define CObjManCullQueue_H

#include <CryCore/Platform/platform.h>
#include <CryMath/Cry_Camera.h>

#include <CryMath/Cry_Geo.h>

class CZBufferCuller;
class CCullBuffer;

namespace NCullQueue
{
class CCullTask;
#ifdef USE_CULL_QUEUE
enum {MAX_CULL_QUEUE_ITEM_COUNT = 4096};  //128 KB
#else
enum {MAX_CULL_QUEUE_ITEM_COUNT = 1};
#endif

struct CRY_ALIGN(16) SCullItem
{
	AABB objBox;
	uint32 BufferID;
	OcclusionTestClient* pOcclTestVars;
};

class SCullQueue
{
private:
	//State variables for asynchronous double-buffering
	volatile bool m_TestItemQueueReady;     //State variable for the queue of items to test. Filled from the main thread during scene parsing.
	volatile bool m_BufferAndCameraReady;   //State variable for the data to test the items in the queue against. Namely the coverage buffer (depth buffer) and a camera for projection which are set by the render thread.

	//Data for processing next time ProcessInternal is called
	uint32          m_MainFrameID;
	const CCamera*  m_pCam;

	volatile uint32 cullBufWriteHeadIndex;
	volatile uint32 cullBufReadHeadIndex;
	Vec3            sunDir;

	CRY_ALIGN(16) SCullItem cullItemBuf[MAX_CULL_QUEUE_ITEM_COUNT + 1];

public:
	void ProcessInternal(uint32 mainFrameID, CZBufferCuller* const pCullBuffer, const CCamera* const pCam);

	SCullQueue();
	~SCullQueue();

	uint32     Size()              { return cullBufWriteHeadIndex; }
	bool       HasItemsToProcess() { return cullBufReadHeadIndex < cullBufWriteHeadIndex; }
	//Routine to reset the coverage buffer state variables. Called when the renderer's buffers are swapped ready for a new frame's worth of work on the Render and Main threads.
	ILINE void ResetSignalVariables()
	{
		m_TestItemQueueReady = false;
		m_BufferAndCameraReady = false;
	}

	ILINE bool BufferAndCameraReady() { return m_BufferAndCameraReady; }

	ILINE void AddItem(const AABB& objBox, const Vec3& lightDir, OcclusionTestClient* pOcclTestVars, uint32 mainFrameID)
	{
		// Quick quiet-nan check
		assert(objBox.max.x == objBox.max.x);
		assert(objBox.max.y == objBox.max.y);
		assert(objBox.max.z == objBox.max.z);
		assert(objBox.min.x == objBox.min.x);
		assert(objBox.min.y == objBox.min.y);
		assert(objBox.min.z == objBox.min.z);
#ifdef USE_CULL_QUEUE
		if (cullBufWriteHeadIndex < MAX_CULL_QUEUE_ITEM_COUNT - 1)
		{
			SCullItem& RESTRICT_REFERENCE rItem = cullItemBuf[cullBufWriteHeadIndex];
			rItem.objBox = objBox;
			sunDir = lightDir;
			rItem.BufferID = 14;       //1<<1 1<<2 1<<3  shadows
			rItem.pOcclTestVars = pOcclTestVars;
			++cullBufWriteHeadIndex;  //store here to make it more likely SCullItem has been written
			return;
		}
#endif
		pOcclTestVars->nLastVisibleMainFrameID = mainFrameID;  //not enough space to hold item
	}

	ILINE void AddItem(const AABB& objBox, float fDistance, OcclusionTestClient* pOcclTestVars, uint32 mainFrameID)
	{
		assert(objBox.max.x == objBox.max.x);
		assert(objBox.max.y == objBox.max.y);
		assert(objBox.max.z == objBox.max.z);
		assert(objBox.min.x == objBox.min.x);
		assert(objBox.min.y == objBox.min.y);
		assert(objBox.min.z == objBox.min.z);
#ifdef USE_CULL_QUEUE
		if (cullBufWriteHeadIndex < MAX_CULL_QUEUE_ITEM_COUNT - 1)
		{
			SCullItem& RESTRICT_REFERENCE rItem = cullItemBuf[cullBufWriteHeadIndex];
			rItem.objBox = objBox;
			rItem.BufferID = 1;         //1<<0 zbuffer
			rItem.pOcclTestVars = pOcclTestVars;
			++cullBufWriteHeadIndex;  //store here to make it more likely SCullItem has been written
			return;
		}
#endif
		pOcclTestVars->nLastVisibleMainFrameID = mainFrameID;  //not enough space to hold item
	}
	//Sets a signal variable. If both signal variables are set then process() is called
	void FinishedFillingTestItemQueue();
	//Sets a signal variable and sets up the parameters for the next test. If both signal variables are set then process() is called
	void SetTestParams(uint32 mainFrameID, const CCamera* pCam);

	void Process();
	void Wait();
};

};

#endif // CObjManCullQueue_H
