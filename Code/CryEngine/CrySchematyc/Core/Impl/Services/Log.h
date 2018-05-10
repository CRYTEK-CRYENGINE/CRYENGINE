// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Services/ILog.h>
#include <CrySchematyc/Services/ISettingsManager.h>
#include <CrySchematyc/Utils/ScopedConnection.h>
#include <CrySchematyc/Utils/StackString.h>

namespace Schematyc
{
// Forward declare interfaces.
struct ILogOutput;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(ILogOutput)

class CLog : public ILog
{
private:

	struct SStream
	{
		SStream(const char* _szName, LogStreamId _id);

		string      name;
		LogStreamId id;
	};

	typedef std::vector<SStream>       Streams;
	typedef std::vector<ILogOutputPtr> Outputs;
	typedef std::vector<SLogScope*>    ScopeStack;

	typedef std::function<void ()>         SettingsModifiedCallback;

	struct SSettings : public ISettings
	{
		typedef std::vector<string> UserStreams;

		SSettings(const SettingsModifiedCallback& _modifiedCallback);

		// ISettings
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~ISettings

		UserStreams              userStreams;
		SettingsModifiedCallback modifiedCallback;
	};

	DECLARE_SHARED_POINTERS(SSettings)

	struct SSignals
	{
		LogMessageSignal message;
	};

public:

	CLog();

	void Init();
	void Shutdown();

	// ILog
	virtual ELogMessageType          GetMessageType(const char* szMessageType) override;
	virtual const char*              GetMessageTypeName(ELogMessageType messageType) override;
	virtual LogStreamId              CreateStream(const char* szName, LogStreamId staticStreamId = LogStreamId::Invalid) override;
	virtual void                     DestroyStream(LogStreamId streamId) override;
	virtual LogStreamId              GetStreamId(const char* szName) const override;
	virtual const char*              GetStreamName(LogStreamId streamId) const override;
	virtual void                     VisitStreams(const LogStreamVisitor& visitor) const override;
	virtual ILogOutputPtr            CreateFileOutput(const char* szFileName) override;
	virtual void                     PushScope(SLogScope* pScope) override;
	virtual void                     PopScope(SLogScope* pScope) override;
	virtual void                     Comment(LogStreamId streamId, const char* szFormat, va_list va_args) override;
	virtual void                     Warning(LogStreamId streamId, const char* szFormat, va_list va_args) override;
	virtual void                     Error(LogStreamId streamId, const char* szFormat, va_list va_args) override;
	virtual ECriticalErrorStatus     CriticalError(LogStreamId streamId, const char* szFormat, va_list va_args) override;
	virtual void                     FatalError(LogStreamId streamId, const char* szFormat, va_list va_args) override;
	virtual void                     Update() override;
	virtual LogMessageSignal::Slots& GetMessageSignalSlots() override;
	// ~ILog

private:

	void   OnSettingsModified();
	uint32 FindStream(const char* szName) const;
	uint32 FindStream(LogStreamId streamId) const;
	void   GetUserStreamsFileName(CStackString& fileName) const;

private:

	uint32              m_nextDynamicStreamId;
	Streams             m_streams;
	Outputs             m_outputs;
	ScopeStack          m_scopeStack;      // #SchematycTODO : If we want to support multi-threading we will need to allocate one stack per thread.
	SSettingsPtr        m_pSettings;
	SSignals            m_signals;

	static const uint32 ms_firstDynamicStreamId;
};
} // Schematyc
