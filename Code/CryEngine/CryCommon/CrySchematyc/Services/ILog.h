// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/ICore.h"
#include "CrySchematyc/Services/LogMetaData.h"
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"
#include "CrySchematyc/Utils/Signal.h"

#ifndef SCHEMATYC_LOGGING_ENABLED
	#ifdef _RELEASE
		#define SCHEMATYC_LOGGING_ENABLED 0
	#else
		#define SCHEMATYC_LOGGING_ENABLED 1
	#endif
#endif

#if SCHEMATYC_LOGGING_ENABLED

	#define SCHEMATYC_LOG_SCOPE(metaData)        const Schematyc::SLogScope schematycLogScope(metaData)
	#define SCHEMATYC_COMMENT(streamId, ...)     gEnv->pSchematyc->GetLog().Comment(streamId, __VA_ARGS__)
	#define SCHEMATYC_WARNING(streamId, ...)     gEnv->pSchematyc->GetLog().Warning(streamId, __VA_ARGS__)
	#define SCHEMATYC_ERROR(streamId, ...)       gEnv->pSchematyc->GetLog().Error(streamId, __VA_ARGS__)
	#define SCHEMATYC_FATAL_ERROR(streamId, ...) gEnv->pSchematyc->GetLog().FatalError(streamId, __VA_ARGS__); CryFatalError(__VA_ARGS__)

	#define SCHEMATYC_CRITICAL_ERROR(streamId, ...)                                \
	  {                                                                            \
	    static bool bIgnore = false;                                               \
	    if (!bIgnore)                                                              \
	    {                                                                          \
	      switch (gEnv->pSchematyc->GetLog().CriticalError(streamId, __VA_ARGS__)) \
	      {                                                                        \
	      case Schematyc::ECriticalErrorStatus::Break:                             \
	        {                                                                      \
	          SCHEMATYC_DEBUG_BREAK;                                               \
	          break;                                                               \
	        }                                                                      \
	      case Schematyc::ECriticalErrorStatus::Ignore:                            \
	        {                                                                      \
	          bIgnore = true;                                                      \
	          break;                                                               \
	        }                                                                      \
	      }                                                                        \
	    }                                                                          \
	  }

	#define SCHEMATYC_CORE_COMMENT(...)            SCHEMATYC_COMMENT(Schematyc::LogStreamId::Core, __VA_ARGS__)
	#define SCHEMATYC_CORE_WARNING(...)            SCHEMATYC_WARNING(Schematyc::LogStreamId::Core, __VA_ARGS__)
	#define SCHEMATYC_CORE_ERROR(...)              SCHEMATYC_ERROR(Schematyc::LogStreamId::Core, __VA_ARGS__)
	#define SCHEMATYC_CORE_CRITICAL_ERROR(...)     SCHEMATYC_CRITICAL_ERROR(Schematyc::LogStreamId::Core, __VA_ARGS__)
	#define SCHEMATYC_CORE_FATAL_ERROR(...)        SCHEMATYC_FATAL_ERROR(Schematyc::LogStreamId::Core, __VA_ARGS__)

	#define SCHEMATYC_COMPILER_COMMENT(...)        SCHEMATYC_COMMENT(Schematyc::LogStreamId::Compiler, __VA_ARGS__)
	#define SCHEMATYC_COMPILER_WARNING(...)        SCHEMATYC_WARNING(Schematyc::LogStreamId::Compiler, __VA_ARGS__)
	#define SCHEMATYC_COMPILER_ERROR(...)          SCHEMATYC_ERROR(Schematyc::LogStreamId::Compiler, __VA_ARGS__)
	#define SCHEMATYC_COMPILER_CRITICAL_ERROR(...) SCHEMATYC_CRITICAL_ERROR(Schematyc::LogStreamId::Compiler, __VA_ARGS__)
	#define SCHEMATYC_COMPILER_FATAL_ERROR(...)    SCHEMATYC_FATAL_ERROR(Schematyc::LogStreamId::Compiler, __VA_ARGS__)

	#define SCHEMATYC_EDITOR_COMMENT(...)          SCHEMATYC_COMMENT(Schematyc::LogStreamId::Editor, __VA_ARGS__)
	#define SCHEMATYC_EDITOR_WARNING(...)          SCHEMATYC_WARNING(Schematyc::LogStreamId::Editor, __VA_ARGS__)
	#define SCHEMATYC_EDITOR_ERROR(...)            SCHEMATYC_ERROR(Schematyc::LogStreamId::Editor, __VA_ARGS__)
	#define SCHEMATYC_EDITOR_CRITICAL_ERROR(...)   SCHEMATYC_CRITICAL_ERROR(Schematyc::LogStreamId::Editor, __VA_ARGS__)
	#define SCHEMATYC_EDITOR_FATAL_ERROR(...)      SCHEMATYC_FATAL_ERROR(Schematyc::LogStreamId::Editor, __VA_ARGS__)

	#define SCHEMATYC_ENV_COMMENT(...)             SCHEMATYC_COMMENT(Schematyc::LogStreamId::Env, __VA_ARGS__)
	#define SCHEMATYC_ENV_WARNING(...)             SCHEMATYC_WARNING(Schematyc::LogStreamId::Env, __VA_ARGS__)
	#define SCHEMATYC_ENV_ERROR(...)               SCHEMATYC_ERROR(Schematyc::LogStreamId::Env, __VA_ARGS__)
	#define SCHEMATYC_ENV_CRITICAL_ERROR(...)      SCHEMATYC_CRITICAL_ERROR(Schematyc::LogStreamId::Env, __VA_ARGS__)
	#define SCHEMATYC_ENV_FATAL_ERROR(...)         SCHEMATYC_FATAL_ERROR(Schematyc::LogStreamId::Env, __VA_ARGS__)

#else

	#define SCHEMATYC_LOG_SCOPE(metaData)           SCHEMATYC_NOP
	#define SCHEMATYC_COMMENT(streamId, ...)        SCHEMATYC_NOP
	#define SCHEMATYC_WARNING(streamId, ...)        SCHEMATYC_NOP
	#define SCHEMATYC_ERROR(streamId, ...)          SCHEMATYC_NOP
	#define SCHEMATYC_FATAL_ERROR(streamId, ...)    SCHEMATYC_NOP

	#define SCHEMATYC_CRITICAL_ERROR(streamId, ...) SCHEMATYC_NOP

	#define SCHEMATYC_CORE_COMMENT(...)             SCHEMATYC_NOP
	#define SCHEMATYC_CORE_WARNING(...)             SCHEMATYC_NOP
	#define SCHEMATYC_CORE_ERROR(...)               SCHEMATYC_NOP
	#define SCHEMATYC_CORE_CRITICAL_ERROR(...)      SCHEMATYC_NOP
	#define SCHEMATYC_CORE_FATAL_ERROR(...)         SCHEMATYC_NOP

	#define SCHEMATYC_COMPILER_COMMENT(...)         SCHEMATYC_NOP
	#define SCHEMATYC_COMPILER_WARNING(...)         SCHEMATYC_NOP
	#define SCHEMATYC_COMPILER_ERROR(...)           SCHEMATYC_NOP
	#define SCHEMATYC_COMPILER_CRITICAL_ERROR(...)  SCHEMATYC_NOP
	#define SCHEMATYC_COMPILER_FATAL_ERROR(...)     SCHEMATYC_NOP

	#define SCHEMATYC_EDITOR_COMMENT(...)           SCHEMATYC_NOP
	#define SCHEMATYC_EDITOR_WARNING(...)           SCHEMATYC_NOP
	#define SCHEMATYC_EDITOR_ERROR(...)             SCHEMATYC_NOP
	#define SCHEMATYC_EDITOR_CRITICAL_ERROR(...)    SCHEMATYC_NOP
	#define SCHEMATYC_EDITOR_FATAL_ERROR(...)       SCHEMATYC_NOP

	#define SCHEMATYC_ENV_COMMENT(...)              SCHEMATYC_NOP
	#define SCHEMATYC_ENV_WARNING(...)              SCHEMATYC_NOP
	#define SCHEMATYC_ENV_ERROR(...)                SCHEMATYC_NOP
	#define SCHEMATYC_ENV_CRITICAL_ERROR(...)       SCHEMATYC_NOP
	#define SCHEMATYC_ENV_FATAL_ERROR(...)          SCHEMATYC_NOP

#endif

namespace Schematyc
{
// Forward declare structures.
struct SLogScope;

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

enum class LogStreamId : uint32
{
	Invalid = 0,
	Default,
	Core,
	Compiler,
	Editor,
	Env,
	Custom
};

inline bool Serialize(Serialization::IArchive& archive, LogStreamId& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		return archive(static_cast<uint32>(value), szName, szLabel);
	}
	return true;
}

struct SLogMessageData
{
	explicit inline SLogMessageData(const CLogMetaData* _pMetaData, ELogMessageType _messageType, LogStreamId _streamId, const char* _szMessage)
		: pMetaData(_pMetaData)
		, messageType(_messageType)
		, streamId(_streamId)
		, szMessage(_szMessage)
	{}

	const CLogMetaData* pMetaData;
	ELogMessageType     messageType;
	LogStreamId         streamId;
	const char*         szMessage;
};

typedef std::function<EVisitStatus(const char*, LogStreamId)> LogStreamVisitor;

struct ILogOutput
{
	virtual ~ILogOutput() {}

	virtual void EnableStream(LogStreamId streamId) = 0;
	virtual void DisableAllStreams() = 0;
	virtual void EnableMessageType(ELogMessageType messageType) = 0;
	virtual void DisableAllMessageTypes() = 0;
	virtual void Update() = 0;
};

DECLARE_SHARED_POINTERS(ILogOutput)

typedef CSignal<void (const SLogMessageData&)> LogMessageSignal;

struct ILog
{
	virtual ~ILog() {}

	virtual ELogMessageType          GetMessageType(const char* szMessageType) = 0;
	virtual const char*              GetMessageTypeName(ELogMessageType messageType) = 0;

	virtual LogStreamId              CreateStream(const char* szName, LogStreamId staticStreamId = LogStreamId::Invalid) = 0;
	virtual void                     DestroyStream(LogStreamId streamId) = 0;
	virtual LogStreamId              GetStreamId(const char* szName) const = 0;
	virtual const char*              GetStreamName(LogStreamId streamId) const = 0;
	virtual void                     VisitStreams(const LogStreamVisitor& visitor) const = 0;

	virtual ILogOutputPtr            CreateFileOutput(const char* szFileName) = 0;

	virtual void                     PushScope(SLogScope* pScope) = 0;
	virtual void                     PopScope(SLogScope* pScope) = 0;

	virtual void                     Comment(LogStreamId streamId, const char* szFormat, va_list va_args) = 0;
	virtual void                     Warning(LogStreamId streamId, const char* szFormat, va_list va_args) = 0;
	virtual void                     Error(LogStreamId streamId, const char* szFormat, va_list va_args) = 0;
	virtual ECriticalErrorStatus     CriticalError(LogStreamId streamId, const char* szFormat, va_list va_args) = 0;
	virtual void                     FatalError(LogStreamId streamId, const char* szFormat, va_list va_args) = 0;

	virtual void                     Update() = 0;
	virtual LogMessageSignal::Slots& GetMessageSignalSlots() = 0;

	inline void                      Comment(LogStreamId streamId, const char* szFormat, ...)
	{
		va_list va_args;
		va_start(va_args, szFormat);
		Comment(streamId, szFormat, va_args);
		va_end(va_args);
	}

	inline void Warning(LogStreamId streamId, const char* szFormat, ...)
	{
		va_list va_args;
		va_start(va_args, szFormat);
		Warning(streamId, szFormat, va_args);
		va_end(va_args);
	}

	inline void Error(LogStreamId streamId, const char* szFormat, ...)
	{
		va_list va_args;
		va_start(va_args, szFormat);
		Error(streamId, szFormat, va_args);
		va_end(va_args);
	}

	inline ECriticalErrorStatus CriticalError(LogStreamId streamId, const char* szFormat, ...)
	{
		va_list va_args;
		va_start(va_args, szFormat);
		const ECriticalErrorStatus result = CriticalError(streamId, szFormat, va_args);
		va_end(va_args);
		return result;
	}

	inline void FatalError(LogStreamId streamId, const char* szFormat, ...)
	{
		va_list va_args;
		va_start(va_args, szFormat);
		FatalError(streamId, szFormat, va_args);
		va_end(va_args);
	}
};

struct SLogScope
{
public:

	inline SLogScope(const CLogMetaData& _metaData)
		: metaData(_metaData)
		, warningCount(0)
		, errorCount(0)
	{
		gEnv->pSchematyc->GetLog().PushScope(this);
	}

	inline ~SLogScope()
	{
		gEnv->pSchematyc->GetLog().PopScope(this);
	}

	const CLogMetaData& metaData;
	uint32              warningCount;
	uint32              errorCount;
};
} // Schematyc
