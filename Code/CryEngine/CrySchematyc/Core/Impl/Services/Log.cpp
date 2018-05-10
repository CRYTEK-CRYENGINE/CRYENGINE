// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Log.h"

#include <CryCore/Platform/CryWindows.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CryString/CryStringUtils.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/ScopedConnection.h>

#include "CVars.h"
#include "Core.h"

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, ELogMessageType, "CrySchematyc Log Message Type")
SERIALIZATION_ENUM(Schematyc::ELogMessageType::Comment, "Comment", "Comment")
SERIALIZATION_ENUM(Schematyc::ELogMessageType::Warning, "Warning", "Warning")
SERIALIZATION_ENUM(Schematyc::ELogMessageType::Error, "Error", "Error")
SERIALIZATION_ENUM(Schematyc::ELogMessageType::CriticalError, "CriticalError", "Critical Error")
SERIALIZATION_ENUM_END()

namespace Schematyc
{
namespace LogUtils
{
template<uint32 SIZE> inline void FormatMessage(char (&messageBuffer)[SIZE], const char* szFormat, va_list va_args)
{
	const uint32 pos = strlen(messageBuffer);
	cry_vsprintf(messageBuffer + pos, sizeof(messageBuffer) - pos - 1, szFormat, va_args);
}

inline ECriticalErrorStatus DisplayCriticalErrorMessage(const char* szMessage)
{
#if defined(CRY_PLATFORM_WINDOWS)
	string message = szMessage;
	message.append("\r\n\r\n");
	message.append("Would you like to continue?\r\n\r\nClick 'Yes' to continue, 'No' to break or 'Cancel' to continue and ignore this error.");
	switch (CryMessageBox(message.c_str(), "CrySchematyc - Critical Error!", eMB_YesNoCancel))
	{
	case eQR_Yes:
		{
			return ECriticalErrorStatus::Continue;
		}
	case eQR_Cancel:
		{
			return ECriticalErrorStatus::Ignore;
		}
	}
#else
	CryMessageBox(szMessage, "CrySchematyc - Critical Error!", eMB_Error);
#endif
	return ECriticalErrorStatus::Break;
}

void CriticalErrorCommand(IConsoleCmdArgs* pArgs)
{
	const char* szMessage = "";
	if (pArgs->GetArgCount() == 2)
	{
		szMessage = pArgs->GetArg(1);
	}
	SCHEMATYC_CORE_CRITICAL_ERROR(szMessage);
}

void FatalErrorCommand(IConsoleCmdArgs* pArgs)
{
	const char* szMessage = "";
	if (pArgs->GetArgCount() == 2)
	{
		szMessage = pArgs->GetArg(1);
	}
	SCHEMATYC_CORE_FATAL_ERROR(szMessage);
}

class CFileOutput : public ILogOutput, public IErrorObserver
{
private:

	typedef CryStackStringT<char, 512>   MessageString;
	typedef std::vector<MessageString>   MessageQueue;
	typedef std::vector<LogStreamId>     StreamIds;
	typedef std::vector<ELogMessageType> MessageTypes;

public:

	CFileOutput(LogMessageSignal& messageSignal, const char* szFileName)
		: m_pFile(nullptr)
		, m_fileName(szFileName)
		, m_bErrorObserverRegistered(false)
	{
		Initialize();
		messageSignal.GetSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CFileOutput::OnLogMessage, *this), m_connectionScope);
	}

	~CFileOutput()
	{
		FlushAndClose();
	}

	// ILogOutput

	virtual void EnableStream(LogStreamId streamId) override
	{
		stl::push_back_unique(m_streamIds, streamId);
	}

	virtual void DisableAllStreams() override
	{
		m_streamIds.clear();
	}

	virtual void EnableMessageType(ELogMessageType messageType) override
	{
		stl::push_back_unique(m_messageTypes, messageType);
	}

	virtual void DisableAllMessageTypes() override
	{
		m_messageTypes.clear();
	}

	virtual void Update() override
	{
		FlushMessages();
	}

	// ~ILogOutput

	// IErrorObserver

	virtual void OnAssert(const char* szCondition, const char* szMessage, const char* szFileName, unsigned int fileLineNumber) override
	{
		FlushMessages();
	}

	virtual void OnFatalError(const char* szMessage) override
	{
		FlushMessages();
	}

	// ~IErrorObserver

	void OnLogMessage(const SLogMessageData& logMessageData)
	{
		if (std::find(m_streamIds.begin(), m_streamIds.end(), logMessageData.streamId) != m_streamIds.end())
		{
			if (std::find(m_messageTypes.begin(), m_messageTypes.end(), logMessageData.messageType) != m_messageTypes.end())
			{
				EnqueueMessage(logMessageData);
			}
		}
	}

	void FlushMessages()
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);

		const uint32 valueSize = sizeof(MessageString::CharTraits::value_type);
		CryAutoCriticalSection lock(m_criticalSection);
		if (m_pFile && !m_messagQueue.empty())
		{
			for (const MessageString& message : m_messagQueue)
			{
				fwrite(message.c_str(), valueSize, message.size(), m_pFile);
			}
			m_messagQueue.clear();
			fflush(m_pFile);
		}
	}

	void FlushAndClose()
	{
		CryAutoCriticalSection lock(m_criticalSection);

		FlushMessages();

		if (m_pFile)
		{
			fclose(m_pFile);
			m_pFile = nullptr;
		}

		if (m_bErrorObserverRegistered)
		{
			m_bErrorObserverRegistered = false;
			gEnv->pSystem->UnregisterErrorObserver(this);
		}

		m_messagQueue.shrink_to_fit();
	}

private:

	void Initialize()
	{
		if (!m_pFile)
		{
			m_bErrorObserverRegistered = gEnv->pSystem->RegisterErrorObserver(this);

			FILE* pFile = fxopen(m_fileName.c_str(), "rb");
			if (pFile)
			{
				fclose(pFile);

				CStackString backupFileName("LogBackups/");
				backupFileName.append(m_fileName.c_str());
#if CRY_PLATFORM_DURANGO
				CRY_ASSERT_MESSAGE(false, "MoveFileEx not supported on Durango!");
#else
				CopyFile(m_fileName.c_str(), backupFileName.c_str(), true);
#endif
			}

			m_pFile = fxopen(m_fileName.c_str(), "wtc");
			setvbuf(m_pFile, m_writeBuffer, _IOFBF, sizeof(m_writeBuffer));

			m_messagQueue.reserve(100);
		}
	}

	void EnqueueMessage(const SLogMessageData& logMessageData)
	{
		MessageString output;
		if (logMessageData.pMetaData)
		{
			CStackString uri;
			logMessageData.pMetaData->CreateUri(uri);
			if (!uri.empty())
			{
				output.append("[");
				output.append(uri.c_str());
				output.append("]");
			}
		}
		output.append(logMessageData.szMessage);
		output.append("\n");
		{
			CryAutoCriticalSection lock(m_criticalSection);
			m_messagQueue.push_back(output);
		}
	}

private:

	MessageQueue       m_messagQueue;
	StreamIds          m_streamIds;
	MessageTypes       m_messageTypes;
	FILE* volatile     m_pFile;
	string             m_fileName;
	CryCriticalSection m_criticalSection;
	char               m_writeBuffer[1024 * 1024];
	bool               m_bErrorObserverRegistered;
	CConnectionScope   m_connectionScope;
};
} // LogUtils

const uint32 CLog::ms_firstDynamicStreamId = 0xffff0000;

CLog::SStream::SStream(const char* _szName, LogStreamId _id)
	: name(_szName)
	, id(_id)
{}

CLog::CLog()
	: m_nextDynamicStreamId(ms_firstDynamicStreamId)
{
	m_pSettings = SSettingsPtr(new SSettings(SCHEMATYC_MEMBER_DELEGATE(&CLog::OnSettingsModified, *this)));
}

CLog::SSettings::SSettings(const SettingsModifiedCallback& _modifiedCallback)
	: modifiedCallback(_modifiedCallback)
{}

void CLog::SSettings::Serialize(Serialization::IArchive& archive)
{
	archive(userStreams, "userStreams", "User Streams");
	if (archive.isInput() && modifiedCallback)
	{
		modifiedCallback();
	}
}

void CLog::Init()
{
	CreateStream("Default", LogStreamId::Default);
	CreateStream("Core", LogStreamId::Core);
	CreateStream("Compiler", LogStreamId::Compiler);
	CreateStream("Editor", LogStreamId::Editor);
	CreateStream("Environment", LogStreamId::Env);

	CCore::GetInstance().GetSettingsManager().RegisterSettings("log_settings", m_pSettings);

	REGISTER_COMMAND("sc_CriticalError", LogUtils::CriticalErrorCommand, VF_NULL, "Trigger Schematyc critical error");
	REGISTER_COMMAND("sc_FatalError", LogUtils::FatalErrorCommand, VF_NULL, "Trigger Schematyc fatal error");
}

void CLog::Shutdown()
{
	m_outputs.clear();
}

ELogMessageType CLog::GetMessageType(const char* szMessageType)
{
	SCHEMATYC_CORE_ASSERT(szMessageType);
	if (szMessageType)
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<ELogMessageType>();
		for (int logMessageTypeIdx = 0, logMessageTypeCount = enumDescription.count(); logMessageTypeIdx < logMessageTypeCount; ++logMessageTypeIdx)
		{
			if (stricmp(enumDescription.labelByIndex(logMessageTypeIdx), szMessageType) == 0)
			{
				return static_cast<ELogMessageType>(enumDescription.valueByIndex(logMessageTypeIdx));
			}
		}
	}
	return ELogMessageType::Invalid;
}

const char* CLog::GetMessageTypeName(ELogMessageType messageType)
{
	const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<ELogMessageType>();
	return enumDescription.labelByIndex(enumDescription.indexByValue(static_cast<int>(messageType)));
}

LogStreamId CLog::CreateStream(const char* szName, LogStreamId staticStreamId)
{
	SCHEMATYC_CORE_ASSERT(szName);
	if (szName && (FindStream(szName) == InvalidIdx))
	{
		LogStreamId streamId;
		if (staticStreamId == LogStreamId::Invalid)
		{
			streamId = static_cast<LogStreamId>(m_nextDynamicStreamId++);
		}
		else if (static_cast<uint32>(staticStreamId) < ms_firstDynamicStreamId)
		{
			if (FindStream(staticStreamId) == InvalidIdx)
			{
				streamId = staticStreamId;
			}
		}
		SCHEMATYC_CORE_ASSERT(streamId != LogStreamId::Invalid);
		if (streamId != LogStreamId::Invalid)
		{
			m_streams.push_back(SStream(szName, streamId));
			return streamId;
		}
	}
	return LogStreamId::Invalid;
}

void CLog::DestroyStream(LogStreamId streamId)
{
	for (Streams::iterator itStream = m_streams.begin(), itEndStream = m_streams.end(); itStream != itEndStream; ++itStream)
	{
		if (itStream->id == streamId)
		{
			m_streams.erase(itStream);
			break;
		}
	}
}

LogStreamId CLog::GetStreamId(const char* szName) const
{
	const uint32 streamIdx = FindStream(szName);
	return streamIdx != InvalidIdx ? m_streams[streamIdx].id : LogStreamId::Invalid;
}

const char* CLog::GetStreamName(LogStreamId streamId) const
{
	const uint32 streamIdx = FindStream(streamId);
	return streamIdx != InvalidIdx ? m_streams[streamIdx].name.c_str() : "";
}

void CLog::VisitStreams(const LogStreamVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const SStream& stream : m_streams)
		{
			visitor(stream.name.c_str(), stream.id);
		}
	}
}

ILogOutputPtr CLog::CreateFileOutput(const char* szFileName)
{
	ILogOutputPtr pFileOutput(new LogUtils::CFileOutput(m_signals.message, szFileName));
	m_outputs.push_back(pFileOutput);
	return pFileOutput;
}

void CLog::PushScope(SLogScope* pScope)
{
	SCHEMATYC_CORE_ASSERT(pScope);
	if (pScope)
	{
		m_scopeStack.push_back(pScope);
	}
}

void CLog::PopScope(SLogScope* pScope)
{
	SCHEMATYC_CORE_ASSERT(!m_scopeStack.empty() && (pScope == m_scopeStack.back()));
	m_scopeStack.pop_back();
}

void CLog::Comment(LogStreamId streamId, const char* szFormat, va_list va_args)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	char messageBuffer[1024] = "";
	LogUtils::FormatMessage(messageBuffer, szFormat, va_args);
	m_signals.message.Send(SLogMessageData(!m_scopeStack.empty() ? &m_scopeStack.back()->metaData : nullptr, ELogMessageType::Comment, streamId, messageBuffer));
}

void CLog::Warning(LogStreamId streamId, const char* szFormat, va_list va_args)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	char messageBuffer[1024] = "";
	LogUtils::FormatMessage(messageBuffer, szFormat, va_args);
	m_signals.message.Send(SLogMessageData(!m_scopeStack.empty() ? &m_scopeStack.back()->metaData : nullptr, ELogMessageType::Warning, streamId, messageBuffer));

	for (SLogScope* pScope : m_scopeStack)
	{
		++pScope->warningCount;
	}
}

void CLog::Error(LogStreamId streamId, const char* szFormat, va_list va_args)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	char messageBuffer[1024] = "";
	LogUtils::FormatMessage(messageBuffer, szFormat, va_args);
	m_signals.message.Send(SLogMessageData(!m_scopeStack.empty() ? &m_scopeStack.back()->metaData : nullptr, ELogMessageType::Error, streamId, messageBuffer));

	for (SLogScope* pScope : m_scopeStack)
	{
		++pScope->errorCount;
	}
}

ECriticalErrorStatus CLog::CriticalError(LogStreamId streamId, const char* szFormat, va_list va_args)
{
	char messageBuffer[1024] = "";
	LogUtils::FormatMessage(messageBuffer, szFormat, va_args);
	m_signals.message.Send(SLogMessageData(!m_scopeStack.empty() ? &m_scopeStack.back()->metaData : nullptr, ELogMessageType::CriticalError, streamId, messageBuffer));

	for (SLogScope* pScope : m_scopeStack)
	{
		++pScope->errorCount;
	}

	if (CVars::sc_DisplayCriticalErrors)
	{
		return LogUtils::DisplayCriticalErrorMessage(messageBuffer);
	}
	else
	{
		return ECriticalErrorStatus::Continue;
	}
}

void CLog::FatalError(LogStreamId streamId, const char* szFormat, va_list va_args)
{
	char messageBuffer[1024] = "";
	LogUtils::FormatMessage(messageBuffer, szFormat, va_args);
	m_signals.message.Send(SLogMessageData(!m_scopeStack.empty() ? &m_scopeStack.back()->metaData : nullptr, ELogMessageType::FatalError, streamId, messageBuffer));

	for (SLogScope* pScope : m_scopeStack)
	{
		++pScope->errorCount;
	}
}

void CLog::Update()
{
	for (ILogOutputPtr& pOutput : m_outputs)
	{
		pOutput->Update();
	}
}

LogMessageSignal::Slots& CLog::GetMessageSignalSlots()
{
	return m_signals.message.GetSlots();
}

void CLog::OnSettingsModified()
{
	for (const string& userStream : m_pSettings->userStreams)
	{
		const char* szStreamName = userStream.c_str();
		if (szStreamName[0] && (FindStream(szStreamName) == InvalidIdx))
		{
			CreateStream(szStreamName);
		}
	}
}

uint32 CLog::FindStream(const char* szName) const
{
	SCHEMATYC_CORE_ASSERT(szName);
	if (szName)
	{
		for (uint32 streamIdx = 0, steamCount = m_streams.size(); streamIdx < steamCount; ++streamIdx)
		{
			if (stricmp(m_streams[streamIdx].name.c_str(), szName) == 0)
			{
				return streamIdx;
			}
		}
	}
	return InvalidIdx;
}

uint32 CLog::FindStream(LogStreamId streamId) const
{
	for (uint32 streamIdx = 0, steamCount = m_streams.size(); streamIdx < steamCount; ++streamIdx)
	{
		if (m_streams[streamIdx].id == streamId)
		{
			return streamIdx;
		}
	}
	return InvalidIdx;
}

void CLog::GetUserStreamsFileName(CStackString& fileName) const
{
	fileName = gEnv->pSystem->GetIPak()->GetGameFolder();
	fileName.append("/");
	fileName.append(CVars::sc_RootFolder->GetString());
	fileName.append("/");
	fileName.append("log_user_streams.xml");
}
} // Schematyc
