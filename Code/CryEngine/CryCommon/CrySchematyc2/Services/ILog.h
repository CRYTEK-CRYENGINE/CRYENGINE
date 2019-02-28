// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Add support for indentation/blocks.
// #SchematycTODO : Move ILogRecorder to separate header?
// #SchematycTODO : Rename 'Game' stream to 'Env'?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySystem/ICryLink.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IFramework.h"

#define SCHEMATYC2_LOG_METAINFO(cryLinkCommand, ...)       Schematyc2::CLogMessageMetaInfo((cryLinkCommand), Schematyc2::SLogMetaFunction(__FUNCTION__), __VA_ARGS__)
#define SCHEMATYC2_LOG_METAINFO_DEFAULT(cryLinkCommand)    Schematyc2::CLogMessageMetaInfo((cryLinkCommand), Schematyc2::SLogMetaFunction(__FUNCTION__))

#define SCHEMATYC2_COMMENT(streamId, ...)                  gEnv->pSchematyc2->GetLog().Comment(streamId, SCHEMATYC2_LOG_METAINFO_DEFAULT(Schematyc2::ECryLinkCommand::None), __VA_ARGS__)
#define SCHEMATYC2_ENTITY_COMMENT(streamId, entityId, ...) gEnv->pSchematyc2->GetLog().Comment(streamId, SCHEMATYC2_LOG_METAINFO(Schematyc2::ECryLinkCommand::None, Schematyc2::SLogMetaEntityId(entityId)), __VA_ARGS__)
#define SCHEMATYC2_WARNING(streamId, ...)                  gEnv->pSchematyc2->GetLog().Warning(streamId, SCHEMATYC2_LOG_METAINFO_DEFAULT(Schematyc2::ECryLinkCommand::None), __VA_ARGS__)
#define SCHEMATYC2_ENTITY_WARNING(streamId, entityId, ...) gEnv->pSchematyc2->GetLog().Warning(streamId, SCHEMATYC2_LOG_METAINFO(Schematyc2::ECryLinkCommand::None, Schematyc2::SLogMetaEntityId(entityId)), __VA_ARGS__)
#define SCHEMATYC2_ERROR(streamId, ...)                    gEnv->pSchematyc2->GetLog().Error(streamId, SCHEMATYC2_LOG_METAINFO_DEFAULT(Schematyc2::ECryLinkCommand::None), __VA_ARGS__)
#define SCHEMATYC2_ENTITY_ERROR(streamId, entityId, ...)   gEnv->pSchematyc2->GetLog().Error(streamId, SCHEMATYC2_LOG_METAINFO(Schematyc2::ECryLinkCommand::None, Schematyc2::SLogMetaEntityId(entityId)), __VA_ARGS__)
#define SCHEMATYC2_FATAL_ERROR(streamId, ...)              gEnv->pSchematyc2->GetLog().FatalError(streamId, SCHEMATYC2_LOG_METAINFO_DEFAULT(Schematyc2::ECryLinkCommand::None), __VA_ARGS__); CryFatalError(__VA_ARGS__)

#define SCHEMATYC2_CRITICAL_ERROR(streamId, ...)                                                                                                    \
  {                                                                                                                                                 \
    static bool bIgnore = false;                                                                                                                    \
    if (!bIgnore)                                                                                                                                   \
    {                                                                                                                                               \
      switch (gEnv->pSchematyc2->GetLog().CriticalError(streamId, SCHEMATYC2_LOG_METAINFO_DEFAULT(Schematyc2::ECryLinkCommand::None), __VA_ARGS__)) \
      {                                                                                                                                             \
      case Schematyc2::ECriticalErrorStatus::Break:                                                                                                 \
        {                                                                                                                                           \
          SCHEMATYC2_DEBUG_BREAK;                                                                                                                   \
          break;                                                                                                                                    \
        }                                                                                                                                           \
      case Schematyc2::ECriticalErrorStatus::Ignore:                                                                                                \
        {                                                                                                                                           \
          bIgnore = true;                                                                                                                           \
          break;                                                                                                                                    \
        }                                                                                                                                           \
      }                                                                                                                                             \
    }                                                                                                                                               \
  }

#define SCHEMATYC2_SYSTEM_COMMENT(...)                           SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_SYSTEM, __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_WARNING(...)                           SCHEMATYC2_WARNING(Schematyc2::LOG_STREAM_SYSTEM, __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_ERROR(...)                             SCHEMATYC2_ERROR(Schematyc2::LOG_STREAM_SYSTEM, __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_CRITICAL_ERROR(...)                    SCHEMATYC2_CRITICAL_ERROR(Schematyc2::LOG_STREAM_SYSTEM, __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_FATAL_ERROR(...)                       SCHEMATYC2_FATAL_ERROR(Schematyc2::LOG_STREAM_SYSTEM, __VA_ARGS__)

#define SCHEMATYC2_COMPILER_COMMENT(...)                         SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)
#define SCHEMATYC2_COMPILER_WARNING(...)                         SCHEMATYC2_WARNING(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)
#define SCHEMATYC2_COMPILER_ERROR(...)                           SCHEMATYC2_ERROR(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)
#define SCHEMATYC2_COMPILER_CRITICAL_ERROR(...)                  SCHEMATYC2_CRITICAL_ERROR(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)
#define SCHEMATYC2_COMPILER_FATAL_ERROR(...)                     SCHEMATYC2_FATAL_ERROR(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)

#define SCHEMATYC2_GAME_COMMENT(...)                             SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_GAME, __VA_ARGS__)
#define SCHEMATYC2_GAME_WARNING(...)                             SCHEMATYC2_WARNING(Schematyc2::LOG_STREAM_GAME, __VA_ARGS__)
#define SCHEMATYC2_GAME_ERROR(...)                               SCHEMATYC2_ERROR(Schematyc2::LOG_STREAM_GAME, __VA_ARGS__)
#define SCHEMATYC2_GAME_CRITICAL_ERROR(...)                      SCHEMATYC2_CRITICAL_ERROR(Schematyc2::LOG_STREAM_GAME, __VA_ARGS__)
#define SCHEMATYC2_GAME_FATAL_ERROR(...)                         SCHEMATYC2_FATAL_ERROR(Schematyc2::LOG_STREAM_GAME, __VA_ARGS__)

#define SCHEMATYC2_METAINFO_COMMENT(streamId, metaInfo, ...)     gEnv->pSchematyc2->GetLog().Comment(streamId, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_METAINFO_WARNING(streamId, metaInfo, ...)     gEnv->pSchematyc2->GetLog().Warning(streamId, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_METAINFO_ERROR(streamId, metaInfo, ...)       gEnv->pSchematyc2->GetLog().Error(streamId, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_METAINFO_FATAL_ERROR(streamId, metaInfo, ...) gEnv->pSchematyc2->GetLog().FatalError(streamId, (metaInfo), __VA_ARGS__); CryFatalError(__VA_ARGS__)

#define SCHEMATYC2_METAINFO_CRITICAL_ERROR(streamId, metaInfo, ...)                         \
  {                                                                                         \
    static bool bIgnore = false;                                                            \
    if (!bIgnore)                                                                           \
    {                                                                                       \
      switch (gEnv->pSchematyc2->GetLog().CriticalError(streamId, (metaInfo), __VA_ARGS__)) \
      {                                                                                     \
      case Schematyc2::ECriticalErrorStatus::Break:                                         \
        {                                                                                   \
          SCHEMATYC2_DEBUG_BREAK;                                                           \
          break;                                                                            \
        }                                                                                   \
      case Schematyc2::ECriticalErrorStatus::Ignore:                                        \
        {                                                                                   \
          bIgnore = true;                                                                   \
          break;                                                                            \
        }                                                                                   \
      }                                                                                     \
    }                                                                                       \
  }

#define SCHEMATYC2_SYSTEM_METAINFO_COMMENT(metaInfo, ...)          SCHEMATYC2_METAINFO_COMMENT(Schematyc2::LOG_STREAM_SYSTEM, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_METAINFO_WARNING(metaInfo, ...)          SCHEMATYC2_METAINFO_WARNING(Schematyc2::LOG_STREAM_SYSTEM, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_METAINFO_ERROR(metaInfo, ...)            SCHEMATYC2_METAINFO_ERROR(Schematyc2::LOG_STREAM_SYSTEM, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_METAINFO_CRITICAL_ERROR(metaInfo, ...)   SCHEMATYC2_METAINFO_CRITICAL_ERROR(Schematyc2::LOG_STREAM_SYSTEM, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_SYSTEM_METAINFO_FATAL_ERROR(metaInfo, ...)      SCHEMATYC2_METAINFO_FATAL_ERROR(Schematyc2::LOG_STREAM_SYSTEM, (metaInfo), __VA_ARGS__)

#define SCHEMATYC2_COMPILER_METAINFO_COMMENT(metaInfo, ...)        SCHEMATYC2_METAINFO_COMMENT(Schematyc2::LOG_STREAM_COMPILER, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_COMPILER_METAINFO_WARNING(metaInfo, ...)        SCHEMATYC2_METAINFO_WARNING(Schematyc2::LOG_STREAM_COMPILER, __VA_ARGS__)
#define SCHEMATYC2_COMPILER_METAINFO_ERROR(metaInfo, ...)          SCHEMATYC2_METAINFO_ERROR(Schematyc2::LOG_STREAM_COMPILER, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_COMPILER_METAINFO_CRITICAL_ERROR(metaInfo, ...) SCHEMATYC2_METAINFO_CRITICAL_ERROR(Schematyc2::LOG_STREAM_COMPILER, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_COMPILER_METAINFO_FATAL_ERROR(metaInfo, ...)    SCHEMATYC2_METAINFO_FATAL_ERROR(Schematyc2::LOG_STREAM_COMPILER, (metaInfo), __VA_ARGS__)

#define SCHEMATYC2_GAME_METAINFO_COMMENT(metaInfo, ...)            SCHEMATYC2_METAINFO_COMMENT(Schematyc2::LOG_STREAM_GAME, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_GAME_METAINFO_WARNING_(metaInfo, ...)           SCHEMATYC2_METAINFO_WARNING(Schematyc2::LOG_STREAM_GAME, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_GAME_METAINFO_ERROR(metaInfo, ...)              SCHEMATYC2_METAINFO_ERROR(Schematyc2::LOG_STREAM_GAME, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_GAME_METAINFO_CRITICAL_ERROR(metaInfo, ...)     SCHEMATYC2_METAINFO_CRITICAL_ERROR(Schematyc2::LOG_STREAM_GAME, (metaInfo), __VA_ARGS__)
#define SCHEMATYC2_GAME_METAINFO_FATAL_ERROR(metaInfo, ...)        SCHEMATYC2_METAINFO_FATAL_ERROR(Schematyc2::LOG_STREAM_GAME, (metaInfo), __VA_ARGS__)

#if SCHEMATYC2_ASSERTS_ENABLED

	#define SCHEMATYC2_ASSERT(streamId, expression)                    if (!((expression))) { SCHEMATYC2_CRITICAL_ERROR(streamId, ("ASSERT" # expression)); }
	#define SCHEMATYC2_ASSERT_MESSAGE(streamId, expression, ...)       if (!((expression))) { SCHEMATYC2_CRITICAL_ERROR(streamId, __VA_ARGS__); }
	#define SCHEMATYC2_ASSERT_FATAL(streamId, expression)              if (!((expression))) { SCHEMATYC2_FATAL_ERROR(streamId, ("ASSERT" # expression)); }
	#define SCHEMATYC2_ASSERT_FATAL_MESSAGE(streamId, expression, ...) if (!((expression))) { SCHEMATYC2_FATAL_ERROR(streamId, __VA_ARGS__); }

	#define SCHEMATYC2_SYSTEM_ASSERT(expression)                       SCHEMATYC2_ASSERT(Schematyc2::LOG_STREAM_SYSTEM, (expression))
	#define SCHEMATYC2_SYSTEM_ASSERT_MESSAGE(expression, ...)          SCHEMATYC2_ASSERT_MESSAGE(Schematyc2::LOG_STREAM_SYSTEM, (expression), __VA_ARGS__)
	#define SCHEMATYC2_SYSTEM_ASSERT_FATAL(expression)                 SCHEMATYC2_ASSERT_FATAL(Schematyc2::LOG_STREAM_SYSTEM, (expression))
	#define SCHEMATYC2_SYSTEM_ASSERT_FATAL_MESSAGE(expression, ...)    SCHEMATYC2_ASSERT_FATAL_MESSAGE(Schematyc2::LOG_STREAM_SYSTEM, (expression), __VA_ARGS__)

	#define SCHEMATYC2_COMPILER_ASSERT(expression)                     SCHEMATYC2_ASSERT(Schematyc2::LOG_STREAM_COMPILER, (expression))
	#define SCHEMATYC2_COMPILER_ASSERT_MESSAGE(expression, ...)        SCHEMATYC2_ASSERT_MESSAGE(Schematyc2::LOG_STREAM_COMPILER, (expression), __VA_ARGS__)
	#define SCHEMATYC2_COMPILER_ASSERT_FATAL(expression)               SCHEMATYC2_ASSERT_FATAL(Schematyc2::LOG_STREAM_COMPILER, (expression))
	#define SCHEMATYC2_COMPILER_ASSERT_FATAL_MESSAGE(expression, ...)  SCHEMATYC2_ASSERT_FATAL_MESSAGE(Schematyc2::LOG_STREAM_COMPILER, (expression), __VA_ARGS__)

	#define SCHEMATYC2_GAME_ASSERT(expression)                         SCHEMATYC2_ASSERT(Schematyc2::LOG_STREAM_GAME, (expression))
	#define SCHEMATYC2_GAME_ASSERT_MESSAGE(expression, ...)            SCHEMATYC2_ASSERT_MESSAGE(Schematyc2::LOG_STREAM_GAME, (expression), __VA_ARGS__)
	#define SCHEMATYC2_GAME_ASSERT_FATAL(expression)                   SCHEMATYC2_ASSERT_FATAL(Schematyc2::LOG_STREAM_GAME, (expression))
	#define SCHEMATYC2_GAME_ASSERT_FATAL_MESSAGE(expression, ...)      SCHEMATYC2_ASSERT_FATAL_MESSAGE(Schematyc2::LOG_STREAM_GAME, (expression), __VA_ARGS__)

#else

	#define SCHEMATYC2_ASSERT(streamId, expression)                    ((void)0)
	#define SCHEMATYC2_ASSERT_MESSAGE(streamId, expression, ...)       ((void)0)
	#define SCHEMATYC2_ASSERT_FATAL(streamId, expression)              ((void)0)
	#define SCHEMATYC2_ASSERT_FATAL_MESSAGE(streamId, expression, ...) ((void)0)

	#define SCHEMATYC2_SYSTEM_ASSERT(expression)                       ((void)0)
	#define SCHEMATYC2_SYSTEM_ASSERT_MESSAGE(expression, ...)          ((void)0)
	#define SCHEMATYC2_SYSTEM_ASSERT_FATAL(expression)                 ((void)0)
	#define SCHEMATYC2_SYSTEM_ASSERT_FATAL_MESSAGE(expression, ...)    ((void)0)

	#define SCHEMATYC2_COMPILER_ASSERT(expression)                     ((void)0)
	#define SCHEMATYC2_COMPILER_ASSERT_MESSAGE(expression, ...)        ((void)0)
	#define SCHEMATYC2_COMPILER_ASSERT_FATAL(expression)               ((void)0)
	#define SCHEMATYC2_COMPILER_ASSERT_FATAL_MESSAGE(expression, ...)  ((void)0)

	#define SCHEMATYC2_GAME_ASSERT(expression)                         ((void)0)
	#define SCHEMATYC2_GAME_ASSERT_MESSAGE(expression, ...)            ((void)0)
	#define SCHEMATYC2_GAME_ASSERT_FATAL(expression)                   ((void)0)
	#define SCHEMATYC2_GAME_ASSERT_FATAL_MESSAGE(expression, ...)      ((void)0)

#endif

namespace Schematyc2
{
enum class ELogMessageType
{
	Comment,
	Warning,
	Error,
	CriticalError,
	FatalError,
	Invalid
};

enum class ECriticalErrorStatus
{
	Break,
	Continue,
	Ignore
};

WRAP_TYPE(uint32, LogStreamId, 0)

typedef DynArray<LogStreamId> LogStreamIdArray;

static const LogStreamId LOG_STREAM_DEFAULT = LogStreamId(1);
static const LogStreamId LOG_STREAM_SYSTEM = LogStreamId(2);
static const LogStreamId LOG_STREAM_COMPILER = LogStreamId(3);
static const LogStreamId LOG_STREAM_GAME = LogStreamId(4);
static const LogStreamId LOG_STREAM_CUSTOM = LogStreamId(5);

enum class ECryLinkCommand
{
	None = 0,
	Show,
	Goto
};

struct SLogMetaFunction
{
	inline SLogMetaFunction(const char* _szName = "")
		: szName(_szName)
	{}

	const char* szName;
};

struct SLogMetaChildGUID
{
	inline SLogMetaChildGUID() {}

	inline SLogMetaChildGUID(const SGUID& _guid)
		: guid(_guid)
	{}

	SGUID guid;
};

struct SLogMetaItemGUID
{
	inline SLogMetaItemGUID() {}

	inline SLogMetaItemGUID(const SGUID& _guid)
		: guid(_guid)
	{}

	SGUID guid;
};

struct SLogMetaEntityId
{
	inline SLogMetaEntityId()
		: entityId(INVALID_ENTITYID)
	{}

	inline SLogMetaEntityId(EntityId _entityId)
		: entityId(_entityId)
	{}

	EntityId entityId;
};

// TODO: We should think about how we want to handle string information
//       because this class gets passed through library boundaries.
class CLogMessageMetaInfo
{
public:

	CLogMessageMetaInfo()
	{}

	template<typename T1>
	CLogMessageMetaInfo(ECryLinkCommand command, const T1& value1)
		: m_command(command)
	{
		Set<const T1&>(value1);
	}

	template<typename T1, typename T2>
	CLogMessageMetaInfo(ECryLinkCommand command, const T1& value1, const T2& value2)
		: m_command(command)
	{
		Set<const T1&>(value1);
		Set<const T2&>(value2);
	}

	template<typename T1, typename T2, typename T3>
	CLogMessageMetaInfo(ECryLinkCommand command, const T1& value1, const T2& value2, const T3& value3)
		: m_command(command)
	{
		Set<const T1&>(value1);
		Set<const T2&>(value2);
		Set<const T3&>(value3);
	}

	template<typename T1, typename T2, typename T3, typename T4>
	CLogMessageMetaInfo(ECryLinkCommand command, const T1& value1, const T2& value2, const T3& value3, const T4& value4)
		: m_command(command)
	{
		Set<const T1&>(value1);
		Set<const T2&>(value2);
		Set<const T3&>(value3);
		Set<const T4&>(value4);
	}

	EntityId GetEntityId() const
	{
		return m_entityId.entityId;
	}

	const SGUID& GetItemGUID() const
	{
		return m_itemGuid.guid;
	}

	const SGUID& GetChildGUID() const
	{
		return m_childGuid.guid;
	}

	const char* GetFunction() const
	{
		return m_function.szName;
	}

	string GetUri() const
	{
		// TODO: Move this into CSchematycCryLink.
		switch (m_command)
		{
		case ECryLinkCommand::Show:
			{
				char itemGuid[StringUtils::s_guidStringBufferSize] = { 0 };
				StringUtils::SysGUIDToString(m_itemGuid.guid.sysGUID, itemGuid);
				char childGuid[StringUtils::s_guidStringBufferSize] = { 0 };
				StringUtils::SysGUIDToString(m_childGuid.guid.sysGUID, childGuid);

				stack_string command;
				CryLinkService::CCryLinkUriFactory cryLinkFactory("editor", CryLinkService::ELinkType::Commands);

				command.Format("sc_rpcShowLegacy %s %s", itemGuid, childGuid);
				cryLinkFactory.AddCommand(command.c_str(), command.length());

				return cryLinkFactory.GetUri();
			}
		case ECryLinkCommand::Goto:
			{
				char itemGuid[StringUtils::s_guidStringBufferSize] = { 0 };
				StringUtils::SysGUIDToString(m_itemGuid.guid.sysGUID, itemGuid);
				char itemBackGuid[StringUtils::s_guidStringBufferSize] = { 0 };
				StringUtils::SysGUIDToString(m_childGuid.guid.sysGUID, itemBackGuid);

				stack_string command;
				CryLinkService::CCryLinkUriFactory cryLinkFactory("editor", CryLinkService::ELinkType::Commands);

				command.Format("sc_rpcGotoLegacy %s %s", itemGuid, itemBackGuid);
				cryLinkFactory.AddCommand(command.c_str(), command.length());

				return cryLinkFactory.GetUri();
			}
		case ECryLinkCommand::None:
		default:
			return "";
		}
	}

private:
	template<typename T> void Set(T);

private:
	SLogMetaFunction  m_function;
	SLogMetaItemGUID  m_itemGuid;
	SLogMetaChildGUID m_childGuid;
	SLogMetaEntityId  m_entityId;
	ECryLinkCommand   m_command;
};

template<> inline void CLogMessageMetaInfo::Set(const SLogMetaEntityId& entityId)
{
	m_entityId = entityId;
}

template<> inline void CLogMessageMetaInfo::Set(const SLogMetaItemGUID& guid)
{
	m_itemGuid = guid;
}

template<> inline void CLogMessageMetaInfo::Set(const SLogMetaChildGUID& guid)
{
	m_childGuid = guid;
}

template<> inline void CLogMessageMetaInfo::Set(const SLogMetaFunction& function)
{
	m_function = function;
}

struct SLogMessageData
{
	explicit inline SLogMessageData(const LogStreamId& _streamId, ELogMessageType _messageType, CLogMessageMetaInfo& _metaInfo, const char* _szMessage)
		: streamId(_streamId)
		, szMessage(_szMessage)
		, metaInfo(_metaInfo)
		, messageType(_messageType)
	{}

	const LogStreamId&   streamId;
	const char*          szMessage;
	CLogMessageMetaInfo& metaInfo;
	ELogMessageType      messageType;
};

typedef TemplateUtils::CDelegate<EVisitStatus(const SLogMessageData&)>          LogMessageVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const char*, const LogStreamId&)> LogStreamVisitor;

struct ILogOutput
{
	virtual ~ILogOutput() {}

	virtual void ConfigureFileOutput(bool bWriteToFile, bool bForwardToStandardLog) = 0;   // Hack! Allows game to decide where messages are forwarded.
	virtual void EnableStream(const LogStreamId& streamId) = 0;
	virtual void ClearStreams() = 0;
	virtual void EnableMessageType(ELogMessageType messageType) = 0;
	virtual void ClearMessageTypes() = 0;
	virtual void Update() = 0;
};

DECLARE_SHARED_POINTERS(ILogOutput)

typedef TemplateUtils::CSignalv2<void (const SLogMessageData&)> LogMessageSignal;

struct SLogSignals
{
	LogMessageSignal message;
};

struct ILog
{
	virtual ~ILog() {}

	virtual ELogMessageType      GetMessageType(const char* szMessageType) = 0;
	virtual const char*          GetMessageTypeName(ELogMessageType messageType) = 0;
	virtual LogStreamId          CreateStream(const char* szName, const LogStreamId& staticStreamId = LogStreamId::s_invalid) = 0;
	virtual void                 DestroyStream(const LogStreamId& streamId) = 0;
	virtual LogStreamId          GetStreamId(const char* szName) const = 0;
	virtual const char*          GetStreamName(const LogStreamId& streamId) const = 0;
	virtual void                 VisitStreams(const LogStreamVisitor& visitor) const = 0;
	virtual ILogOutputPtr        CreateFileOutput(const char* szFileName) = 0;
	virtual void                 Comment(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) = 0;
	virtual void                 Warning(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) = 0;
	virtual void                 Error(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) = 0;
	virtual ECriticalErrorStatus CriticalError(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) = 0;
	virtual void                 FatalError(const LogStreamId& streamId, CLogMessageMetaInfo metaInfo, const char* szFormat, ...) = 0;
	virtual void                 Update() = 0;
	virtual SLogSignals&         Signals() = 0;
};

struct ILogRecorder
{
	virtual ~ILogRecorder() {}

	virtual void Begin() = 0;
	virtual void End() = 0;
	virtual void VisitMessages(const LogMessageVisitor& visitor) = 0;
	virtual void Clear() = 0;
};

namespace LogUtils
{
typedef char TimeStringBuffer[32];

inline char* FormatTime(TimeStringBuffer& stringBuffer)
{
	time_t timeValue;
	time(&timeValue);

	strftime(stringBuffer, CRY_ARRAY_COUNT(stringBuffer), "%H:%M:%S", localtime(&timeValue));

	return stringBuffer;
}
}
}
