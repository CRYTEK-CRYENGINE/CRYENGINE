// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STATOSCOPESTREAMINGINTERVALGROUP_H
#define STATOSCOPESTREAMINGINTERVALGROUP_H

#include "Statoscope.h"

#include <CrySystem/IStreamEngine.h>

#if ENABLE_STATOSCOPE

class CStatoscopeStreamingIntervalGroup : public CStatoscopeIntervalGroup, public IStreamEngineListener
{
public:
	enum
	{
		Stage_Waiting   = 0,
		Stage_IO        = (1 << 0),
		Stage_Inflate   = (1 << 1),
		Stage_Async     = (1 << 2),
		Stage_Preempted = (1 << 3),
		Stage_Decrypt   = (1 << 4),
	};

public:
	CStatoscopeStreamingIntervalGroup();

	void Enable_Impl();
	void Disable_Impl();

public: // IStreamEngineListener Members
	void OnStreamEnqueue(const void* pReq, const char* filename, EStreamTaskType source, const StreamReadParams& readParams);
	void OnStreamComputedSortKey(const void* pReq, uint64 key);
	void OnStreamBeginIO(const void* pReq, uint32 compressedSize, uint32 readSize, EStreamSourceMediaType mediaType);
	void OnStreamEndIO(const void* pReq);
	void OnStreamBeginInflate(const void* pReq);
	void OnStreamEndInflate(const void* pReq);
	void OnStreamBeginDecrypt(const void* pReq);
	void OnStreamEndDecrypt(const void* pReq);
	void OnStreamBeginAsyncCallback(const void* pReq);
	void OnStreamEndAsyncCallback(const void* pReq);
	void OnStreamDone(const void* pReq);
	void OnStreamPreempted(const void* pReq);
	void OnStreamResumed(const void* pReq);

private:
	void WriteChangeStageEvent(const void* pReq, int stage, bool entering);
};

#endif

#endif
