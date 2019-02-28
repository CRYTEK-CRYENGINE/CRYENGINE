// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Optimize! Right now we're storing a full copy of the log's scope stack plus two strings per message. Maybe we could serialize to one log string buffer instead?

#pragma once

#include <CryEntitySystem/IEntity.h>
#include <CrySchematyc/Services/ILog.h>
#include <CrySchematyc/Services/ILogRecorder.h>
#include <CrySchematyc/Utils/CryLinkUtils.h>

namespace Schematyc
{
class CLogRecorder : public ILogRecorder
{
private:

	struct SRecordedMessage
	{
		explicit SRecordedMessage(const SLogMessageData& logMessageData);

		CLogMetaData           metaData;
		ELogMessageType        messageType;
		LogStreamId            streamId;
		CryLinkUtils::ECommand linkCommand;
		CryGUID                  elementGUID;
		CryGUID                  detailGUID;
		EntityId               entityId;
		string                 function;
		string                 message;
	};

	typedef std::vector<SRecordedMessage> RecordedMessages;

public:

	~CLogRecorder();

	// ILogRecorder
	virtual void Begin() override;
	virtual void End() override;
	virtual void VisitMessages(const LogMessageVisitor& visitor) override;
	virtual void Clear() override;
	// ~ILogRecorder

private:

	void OnLogMessage(const SLogMessageData& logMessageData);

private:

	RecordedMessages m_recordedMessages;
	CConnectionScope m_connectionScope;
};
}
