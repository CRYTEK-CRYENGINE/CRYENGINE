// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LogRecorder.h"

#include <CrySchematyc/Utils/Assert.h>

namespace Schematyc
{
	CLogRecorder::~CLogRecorder()
	{
		End();
	}

	void CLogRecorder::Begin()
	{
		m_recordedMessages.reserve(1024);
		gEnv->pSchematyc->GetLog().GetMessageSignalSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CLogRecorder::OnLogMessage, *this), m_connectionScope);
	}

	void CLogRecorder::End()
	{
		m_connectionScope.Release();
	}

	void CLogRecorder::VisitMessages(const LogMessageVisitor& visitor)
	{
		SCHEMATYC_CORE_ASSERT(visitor);
		if(visitor)
		{
			for(auto recordedMessage : m_recordedMessages)
			{
				if(visitor(SLogMessageData(!recordedMessage.metaData.IsEmpty() ? &recordedMessage.metaData : nullptr, recordedMessage.messageType, recordedMessage.streamId, recordedMessage.message.c_str())) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	void CLogRecorder::Clear()
	{
		m_recordedMessages.clear();
	}

	CLogRecorder::SRecordedMessage::SRecordedMessage(const SLogMessageData& logMessageData)
		: metaData(logMessageData.pMetaData ? *logMessageData.pMetaData : CLogMetaData())
		, messageType(logMessageData.messageType)
		, streamId(logMessageData.streamId)
		, linkCommand(CryLinkUtils::ECommand::None)
		, entityId(INVALID_ENTITYID)
		, message(logMessageData.szMessage)
	{}

	void CLogRecorder::OnLogMessage(const SLogMessageData& logMessageData)
	{
		m_recordedMessages.push_back(SRecordedMessage(logMessageData));
	}
}
