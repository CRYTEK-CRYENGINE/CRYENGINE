// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IStatoscope.h
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IStatoscope_h__
#define __IStatoscope_h__
#pragma once

#if ENABLE_STATOSCOPE

struct STexturePoolAllocation;

struct IStatoscopeFrameRecord
{
	virtual void AddValue(float f) = 0;
	virtual void AddValue(const char* s) = 0;
	virtual void AddValue(int i) = 0;

protected:
	virtual ~IStatoscopeFrameRecord() {}
};

struct IStatoscopeDataGroup
{
	struct SDescription
	{
		SDescription()
			: key(0)
			, name("")
			, format("")
		{
		}

		SDescription(char key, const char* name, const char* format)
			: key(key)
			, name(name)
			, format(format)
		{
		}

		char        key;
		const char* name;
		const char* format;
	};

	IStatoscopeDataGroup()
		: m_bEnabled(false)
	{}

	virtual ~IStatoscopeDataGroup() {}

	virtual SDescription GetDescription() const = 0;

	bool                 IsEnabled() const { return m_bEnabled; }

	//! Called when the data group is enabled in statoscope.
	virtual void Enable() { m_bEnabled = true; }

	//! Called when the data group is disabled in statoscope.
	virtual void Disable() { m_bEnabled = false; }

	//! Called when the data group is about to be written. Should return number of data sets.
	virtual uint32 PrepareToWrite() { return 1U; }

	//! Called when the data group needs to write frame info. Should populate fr and reset.
	virtual void Write(IStatoscopeFrameRecord& fr) = 0;

private:
	bool m_bEnabled;
};

//! Statoscope interface, access through gEnv->pStatoscope.
struct IStatoscope
{
	virtual ~IStatoscope(){}
private:
	//! Having variables in an interface is generally bad design, but having this here avoids several virtual function calls from the renderer that may be repeated thousands of times a frame.
	bool m_bIsRunning;

public:
	IStatoscope() : m_bIsRunning(false) {}

	virtual bool        RegisterDataGroup(IStatoscopeDataGroup* pDG) = 0;
	virtual void        UnregisterDataGroup(IStatoscopeDataGroup* pDG) = 0;

	virtual void        Tick() = 0;
	virtual void        AddUserMarker(const char* path, const char* name) = 0;        //!< A copy of the strings is taken, so they don't need to persist.
	virtual void        AddUserMarkerFmt(const char* path, const char* fmt, ...) = 0; //!< A copy of the strings is taken, so they don't need to persist.
	virtual void        LogCallstack(const char* tag) = 0;                            //!< Likewise with tag.
	virtual void        LogCallstackFormat(const char* tagFormat, ...) = 0;
	virtual void        SetCurrentProfilerRecords(const std::vector<CFrameProfiler*>* profilers) = 0;
	virtual void        Flush() = 0;
	inline bool         IsRunning()                         { return m_bIsRunning; }
	inline void         SetIsRunning(const bool bIsRunning) { m_bIsRunning = bIsRunning; }
	virtual bool        IsLoggingForTelemetry() = 0;
	virtual void        SetupFPSCaptureCVars() = 0;
	virtual bool        RequestScreenShot() = 0;
	virtual bool        RequiresParticleStats(bool& bEffectStats) = 0;
	virtual void        AddParticleEffect(const char* pEffectName, int count) = 0;
	virtual void        AddPhysEntity(const struct phys_profile_info* pInfo) = 0;
	virtual const char* GetLogFileName() = 0;
	virtual void        CreateTelemetryStream(const char* postHeader, const char* hostname, int port) = 0;
	virtual void        CloseTelemetryStream() = 0;
};

#else // ENABLE_STATOSCOPE

struct IStatoscopeDataGroup;

struct IStatoscope
{
	bool        RegisterDataGroup(IStatoscopeDataGroup* pDG)                                  { return false; }
	void        UnregisterDataGroup(IStatoscopeDataGroup* pDG)                                {}

	void        Tick()                                                                        {}
	void        AddUserMarker(const char* path, const char* name)                             {}
	void        AddUserMarkerFmt(const char* path, const char* fmt, ...)                      {}
	void        LogCallstack(const char* tag)                                                 {}
	void        LogCallstackFormat(const char* tagFormat, ...)                                {}
	void        SetCurrentProfilerRecords(const std::vector<CFrameProfiler*>* profilers)      {}
	void        Flush()                                                                       {}
	bool        IsRunning()                                                                   { return false; }
	void        SetIsRunning(const bool bIsRunning)                                           {};
	bool        IsLoggingForTelemetry()                                                       { return false; }
	void        SetupFPSCaptureCVars()                                                        {}
	bool        RequestScreenShot()                                                           { return false; }
	bool        RequiresParticleStats(bool& bEffectStats)                                     { return false; }
	void        AddParticleEffect(const char* pEffectName, int count)                         {}
	void        AddPhysEntity(const struct phys_profile_info* pInfo)                          {}
	const char* GetLogFileName()                                                              { return ""; }
	void        CreateTelemetryStream(const char* postHeader, const char* hostname, int port) {}
	void        CloseTelemetryStream()                                                        {}
};

#endif // ENABLE_STATOSCOPE

#endif  // __IStatoscope_h__
