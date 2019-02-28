// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StatoscopeTextureStreamingIntervalGroup.h"

#if ENABLE_STATOSCOPE

CStatoscopeTextureStreamingIntervalGroup::CStatoscopeTextureStreamingIntervalGroup()
	: CStatoscopeIntervalGroup('t', "streaming textures",
	                           "['/Textures/' "
	                           "(string filename) "
	                           "(int minMipWanted) "
	                           "(int minMipAvailable) "
	                           "(int inUse)"
	                           "]")
{
}

void CStatoscopeTextureStreamingIntervalGroup::Enable_Impl()
{
	if (gEnv->pRenderer)
	{
		gEnv->pRenderer->SetTextureStreamListener(this);
	}
}

void CStatoscopeTextureStreamingIntervalGroup::Disable_Impl()
{
	if (gEnv->pRenderer)
	{
		gEnv->pRenderer->SetTextureStreamListener(NULL);
	}
}

void CStatoscopeTextureStreamingIntervalGroup::OnCreatedStreamedTexture(void* pHandle, const char* name, int nMips, int nMinMipAvailable)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen =
		  GetValueLength(name) +
		  GetValueLength(0) * 3
		;

		StatoscopeDataWriter::EventBeginInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventBeginInterval>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
		pEv->classId = GetId();

		char* pPayload = (char*)(pEv + 1);
		WriteValue(pPayload, name);
		WriteValue(pPayload, nMips);
		WriteValue(pPayload, nMinMipAvailable);
		WriteValue(pPayload, 0);

		pWriter->EndEvent();
	}
}

void CStatoscopeTextureStreamingIntervalGroup::OnUploadedStreamedTexture(void* pHandle)
{
}

void CStatoscopeTextureStreamingIntervalGroup::OnDestroyedStreamedTexture(void* pHandle)
{
	StatoscopeDataWriter::EventEndInterval* pEv = GetWriter()->BeginEvent<StatoscopeDataWriter::EventEndInterval>();
	pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
	GetWriter()->EndEvent();
}

void CStatoscopeTextureStreamingIntervalGroup::OnTextureWantsMip(void* pHandle, int nMinMip)
{
	OnChangedMip(pHandle, 1, nMinMip);
}

void CStatoscopeTextureStreamingIntervalGroup::OnTextureHasMip(void* pHandle, int nMinMip)
{
	OnChangedMip(pHandle, 2, nMinMip);
}

void CStatoscopeTextureStreamingIntervalGroup::OnBegunUsingTextures(void** pHandles, size_t numHandles)
{
	OnChangedTextureUse(pHandles, numHandles, 1);
}

void CStatoscopeTextureStreamingIntervalGroup::OnEndedUsingTextures(void** pHandles, size_t numHandles)
{
	OnChangedTextureUse(pHandles, numHandles, 0);
}

void CStatoscopeTextureStreamingIntervalGroup::OnChangedTextureUse(void** pHandles, size_t numHandles, int inUse)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen = GetValueLength(1);
		uint32 classId = GetId();

		pWriter->BeginBlock();

		for (size_t i = 0; i < numHandles; ++i)
		{
			StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginBlockEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
			pEv->id = reinterpret_cast<UINT_PTR>(pHandles[i]);
			pEv->classId = classId;
			pEv->field = StatoscopeDataWriter::EventModifyInterval::FieldSplitIntervalMask | 3;

			char* pPayload = (char*)(pEv + 1);
			WriteValue(pPayload, inUse);

			pWriter->EndBlockEvent();
		}

		pWriter->EndBlock();
	}
}

void CStatoscopeTextureStreamingIntervalGroup::OnChangedMip(void* pHandle, int field, int nMinMip)
{
	CStatoscopeEventWriter* pWriter = GetWriter();

	if (pWriter)
	{
		size_t payloadLen = GetValueLength(nMinMip);
		uint32 classId = GetId();

		StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
		pEv->classId = classId;
		pEv->field = StatoscopeDataWriter::EventModifyInterval::FieldSplitIntervalMask | field;

		char* pPayload = (char*)(pEv + 1);
		WriteValue(pPayload, nMinMip);

		pWriter->EndEvent();
	}
}

#endif
