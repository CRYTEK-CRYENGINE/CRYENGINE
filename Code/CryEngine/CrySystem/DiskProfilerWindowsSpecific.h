// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include "Log.h"

#include <CryCore/Platform/platform.h>

#if CRY_PLATFORM_WINDOWS

#include <winioctl.h>

class CDiskProfilerWindowsSpecific
{
public:

	CDiskProfilerWindowsSpecific();

	~CDiskProfilerWindowsSpecific();

	void Update(float timeNow, bool writeToLog);

	void DisableDiskProfiling();

	string GetEngineDriveName() const            { return m_engineDriveName; }
	double getMBReadLastSecond() const           { return m_mbRead; }
	double getMBPerSecRead() const               { return m_mbPerSec; }
	double getTotalReadTimeForLastSecond() const { return m_readTimeSec; }

private:

	// Seperates log file management logic from the profiler
	class CDiskProfileInfoLogger
	{
		CLog m_log;
		bool m_hasLoggedToFile = false;

	public:
		CDiskProfileInfoLogger(ISystem* pSystem)
			: m_log(pSystem)
		{}

		template<typename ... Ts>
		void Log(const char* command, Ts&&... args)
		{
			if (!m_hasLoggedToFile)
			{
				m_log.SetFileName("diskusageinfo.log");
				m_hasLoggedToFile = true;
			}
			m_log.Log(command, std::forward<Ts>(args)...);
			m_log.Flush();
		}
	};

	void* m_engineDriveHandle = nullptr;
	DISK_PERFORMANCE m_lastDiskPerf = {};
	DISK_PERFORMANCE m_currDiskPerf = {};
	bool m_hasLastPerf = false;
	float m_lastUpdateTime = 0.0f;

	string m_engineDriveName;
	double m_mbRead      = 0.0;
	double m_mbPerSec    = 0.0;
	double m_readTimeSec = 0.0;

	CDiskProfileInfoLogger m_log;
};

#endif