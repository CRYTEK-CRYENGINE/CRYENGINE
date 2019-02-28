// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Services/ILog.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>

namespace Schematyc2
{
	struct ILogOutput;

	DECLARE_SHARED_POINTERS(ILogOutput)

	class CLog : public ILog
	{
	public:

		CLog();

		void Init();

		// ILog
		virtual ELogMessageType GetMessageType(const char* szMessageType) override;
		virtual const char* GetMessageTypeName(ELogMessageType messageType) override;
		virtual LogStreamId CreateStream(const char* szName, const LogStreamId& staticStreamId = LogStreamId::s_invalid) override;
		virtual void DestroyStream(const LogStreamId& streamId) override;
		virtual LogStreamId GetStreamId(const char* szName) const override;
		virtual const char* GetStreamName(const LogStreamId& streamId) const override;
		virtual void VisitStreams(const LogStreamVisitor& visitor) const override;
		virtual ILogOutputPtr CreateFileOutput(const char* szFileName) override;
		virtual void Comment(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) override;
		virtual void Warning(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) override;
		virtual void Error(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) override;
		virtual ECriticalErrorStatus CriticalError(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) override;
		virtual void FatalError(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) override;
		virtual void Update() override;
		virtual SLogSignals& Signals() override;
		// ~ILog

	private:

		struct SStream
		{
			SStream(const char* _szName, const LogStreamId& _id);

			string      name;
			LogStreamId id;
		};

		typedef std::vector<SStream>              Streams;
		typedef std::vector<ILogOutputPtr>        Outputs;
		typedef std::vector<string>               UserStreams;
		typedef TemplateUtils::CDelegate<void ()> SettingsModifiedCallback;

		struct SSettings : public IEnvSettings
		{
			SSettings(const SettingsModifiedCallback& _modifiedCallback);

			// IEnvSettings
			virtual void Serialize(Serialization::IArchive& archive) override;
			// ~IEnvSettings

			UserStreams              userStreams;
			SettingsModifiedCallback modifiedCallback;
		};

		DECLARE_SHARED_POINTERS(SSettings)

		static const LogStreamId FIRST_DYNAMIC_STREAM_ID;

		void OnSettingsModified();
		size_t FindStream(const char* szName) const;
		size_t FindStream(const LogStreamId& streamId) const;
		void GetUserStreamsFileName(stack_string& fileName) const;

		LogStreamId  m_nextDynamicStreamId;
		Streams      m_streams;
		Outputs      m_outputs;
		SSettingsPtr m_pSettings;
		SLogSignals  m_signals;
	};

	class CLogRecorder : public ILogRecorder
	{
	public:

		~CLogRecorder();

		// ILogRecorder
		virtual void Begin() override;
		virtual void End() override;
		virtual void VisitMessages(const LogMessageVisitor& visitor) override;
		virtual void Clear() override;
		// ~ILogRecorder

	private:

		// #SchematycTODO : This method of storing messages is far from optimal. If we start recording lots of messages we should think about creating pages of raw data.
		struct SRecordedMessage
		{
			explicit SRecordedMessage(const SLogMessageData& _logMessageData);

			LogStreamId         streamId;
			ELogMessageType     messageType;
			string              message;
			CLogMessageMetaInfo metaInfo;
		};

		typedef std::vector<SRecordedMessage> SRecordedMessageVector;

		void OnLogMessage(const SLogMessageData& logMessageData);

		SRecordedMessageVector          m_recordedMessages;
		TemplateUtils::CConnectionScope m_connectionScope;
	};
}
