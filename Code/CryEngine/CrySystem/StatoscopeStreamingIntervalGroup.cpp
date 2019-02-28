// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StatoscopeStreamingIntervalGroup.h"

#if ENABLE_STATOSCOPE

CStatoscopeStreamingIntervalGroup::CStatoscopeStreamingIntervalGroup()
	: CStatoscopeIntervalGroup('s', "streaming",
	                           "['/Streaming/' "
	                           "(string filename) "
	                           "(int stage) "
	                           "(int priority) "
	                           "(int source) "
	                           "(int perceptualImportance) "
	                           "(int64 sortKey) "
	                           "(int compressedSize) "
	                           "(int mediaType)"
	                           "]")
{
}

void CStatoscopeStreamingIntervalGroup::Enable_Impl()
{
	gEnv->pSystem->GetStreamEngine()->SetListener(this);
}

void CStatoscopeStreamingIntervalGroup::Disable_Impl()
{
	gEnv->pSystem->GetStreamEngine()->SetListener(NULL);
}

void CStatoscopeStreamingIntervalGroup::OnStreamEnqueue(const void* pReq, const char* filename, EStreamTaskType source, const StreamReadParams& readParams)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen =
		  GetValueLength(filename) +
		  GetValueLength(Stage_Waiting) +
		  GetValueLength(0) * 5 +
		  GetValueLength((int64)0);

		StatoscopeDataWriter::EventBeginInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventBeginInterval>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pReq);
		pEv->classId = GetId();

		char* pPayload = (char*)(pEv + 1);
		WriteValue(pPayload, filename);
		WriteValue(pPayload, Stage_Waiting);
		WriteValue(pPayload, (int)readParams.ePriority);
		WriteValue(pPayload, (int)source);
		WriteValue(pPayload, (int)readParams.nPerceptualImportance);
		WriteValue(pPayload, (int64)0);
		WriteValue(pPayload, (int)0);
		WriteValue(pPayload, (int)0);

		pWriter->EndEvent();
	}
}

void CStatoscopeStreamingIntervalGroup::OnStreamComputedSortKey(const void* pReq, uint64 key)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen = GetValueLength((int64)key);
		StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pReq);
		pEv->classId = GetId();
		pEv->field = 5;

		char* pPayload = (char*)(pEv + 1);
		WriteValue(pPayload, (int64)key);

		pWriter->EndEvent();
	}
}

void CStatoscopeStreamingIntervalGroup::OnStreamBeginIO(const void* pReq, uint32 compressedSize, uint32 readSize, EStreamSourceMediaType mediaType)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		{
			size_t payloadLen = GetValueLength((int)compressedSize);
			StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
			pEv->id = reinterpret_cast<UINT_PTR>(pReq);
			pEv->classId = GetId();
			pEv->field = 6;

			char* pPayload = (char*)(pEv + 1);
			WriteValue(pPayload, (int)compressedSize);

			pWriter->EndEvent();
		}
		{
			size_t payloadLen = GetValueLength((int)mediaType);
			StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
			pEv->id = reinterpret_cast<UINT_PTR>(pReq);
			pEv->classId = GetId();
			pEv->field = 7;

			char* pPayload = (char*)(pEv + 1);
			WriteValue(pPayload, (int)mediaType);

			pWriter->EndEvent();
		}
	}

	WriteChangeStageEvent(pReq, Stage_IO, true);
}

void CStatoscopeStreamingIntervalGroup::OnStreamEndIO(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_IO, false);
}

void CStatoscopeStreamingIntervalGroup::OnStreamBeginInflate(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Inflate, true);
}

void CStatoscopeStreamingIntervalGroup::OnStreamEndInflate(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Inflate, false);
}

void CStatoscopeStreamingIntervalGroup::OnStreamBeginDecrypt(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Decrypt, true);
}

void CStatoscopeStreamingIntervalGroup::OnStreamEndDecrypt(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Decrypt, false);
}

void CStatoscopeStreamingIntervalGroup::OnStreamBeginAsyncCallback(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Async, true);
}

void CStatoscopeStreamingIntervalGroup::OnStreamEndAsyncCallback(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Async, false);
}

void CStatoscopeStreamingIntervalGroup::OnStreamPreempted(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Preempted, true);
}

void CStatoscopeStreamingIntervalGroup::OnStreamResumed(const void* pReq)
{
	WriteChangeStageEvent(pReq, Stage_Preempted, false);
}

void CStatoscopeStreamingIntervalGroup::OnStreamDone(const void* pReq)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		StatoscopeDataWriter::EventEndInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventEndInterval>();
		pEv->id = reinterpret_cast<UINT_PTR>(pReq);
		pWriter->EndEvent();
	}
}

void CStatoscopeStreamingIntervalGroup::WriteChangeStageEvent(const void* pReq, int stage, bool entering)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen = GetValueLength(stage) * 2;
		StatoscopeDataWriter::EventModifyIntervalBit* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventModifyIntervalBit>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pReq);
		pEv->classId = GetId();
		pEv->field = StatoscopeDataWriter::EventModifyInterval::FieldSplitIntervalMask | 1;

		char* pPayload = (char*)(pEv + 1);

		if (entering)
		{
			WriteValue(pPayload, (int)-1);
			WriteValue(pPayload, (int)stage);
		}
		else
		{
			WriteValue(pPayload, (int)~stage);
			WriteValue(pPayload, (int)0);
		}

		pWriter->EndEvent();
	}
}

#endif
