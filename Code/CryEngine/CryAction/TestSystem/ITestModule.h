// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ITestModule_H__
#define __ITestModule_H__

#if _MSC_VER > 1000
	#pragma once
#endif

typedef enum
{
	TM_NONE,
	TM_GLOBAL,
	TM_TIMEDEMO,
	TM_LAST // leave this at last
} ETestModuleType;

struct ITestModule
{
	virtual ~ITestModule(){}
	virtual void            StartSession() = 0;
	virtual void            StartRecording(IConsoleCmdArgs* pArgs) = 0;
	virtual void            StopSession() = 0;
	virtual void            PreUpdate() = 0;
	virtual void            Update() = 0;
	virtual void            Record(bool enable) = 0;
	virtual void            Play(bool enable) = 0;
	virtual void            PlayInit(IConsoleCmdArgs* pArgs) = 0;
	virtual void            Pause(bool paused) = 0;
	virtual bool            RecordFrame() = 0;
	virtual bool            PlayFrame() = 0;
	virtual void            Restart() = 0;
	virtual float           RenderInfo(float y = 0) = 0;
	virtual ETestModuleType GetType() const = 0;
	virtual void            ParseParams(XmlNodeRef node) = 0;
	virtual void            SetVariable(const char* name, const char* szValue) = 0;
	virtual void            SetVariable(const char* name, float value) = 0;
	virtual int             GetNumberOfFrames() = 0;
	virtual int             GetTotalPolysRecorded() = 0;
	virtual void            EndLog() = 0;
	virtual void            GetMemoryUsage(ICrySizer* s) const = 0;
	// common stuff
	bool                    m_bEnabled;
	void Enable(bool en)   { m_bEnabled = en; }
	bool IsEnabled() const { return m_bEnabled; }
};

#endif //__ITestModule_H__
