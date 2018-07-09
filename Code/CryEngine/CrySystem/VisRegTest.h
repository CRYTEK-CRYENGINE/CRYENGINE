// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VisRegTest.h
//  Version:     v1.00
//  Created:     14/07/2010 by Nicolas Schulz.
//  Description: Visual Regression Test
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VisRegTest_h__
#define __VisRegTest_h__
#pragma once

struct IConsoleCmdArgs;

class CVisRegTest
{
public:
	static const uint32 SampleCount = 16;
	static const uint32 MaxNumGPUTimes = 5;
	static const float  MaxStreamingWait;

protected:

	enum ECmd
	{
		eCMDStart,
		eCMDFinish,
		eCMDOnMapLoaded,
		eCMDConsoleCmd,
		eCMDLoadMap,
		eCMDWaitStreaming,
		eCMDWaitFrames,
		eCMDGoto,
		eCMDCaptureSample
	};

	struct SCmd
	{
		ECmd   cmd;
		uint32 freq;
		string args;

		SCmd() {}
		SCmd(ECmd cmd, const string& args, uint32 freq = 1)
		{ this->cmd = cmd; this->args = args; this->freq = freq; }
	};

	struct SSample
	{
		const char* imageName;
		float       frameTime;
		uint32      drawCalls;
		float       gpuTimes[MaxNumGPUTimes];

		SSample() : imageName(NULL), frameTime(0.f), drawCalls(0)
		{
			for (uint32 i = 0; i < MaxNumGPUTimes; ++i)
				gpuTimes[i] = 0;
		}
	};

protected:
	string               m_testName;
	std::vector<SCmd>    m_cmdBuf;
	std::vector<SSample> m_dataSamples;
	uint32               m_nextCmd;
	uint32               m_cmdFreq;
	int                  m_waitFrames;
	float                m_streamingTimeout;
	int                  m_curSample;
	bool                 m_quitAfterTests;

public:

	CVisRegTest();

	void Init(IConsoleCmdArgs* pParams);
	void AfterRender();

protected:

	bool LoadConfig(const char* configFile);
	void ExecCommands();
	void LoadMap(const char* mapName);
	void CaptureSample(const SCmd& cmd);
	void Finish();
	bool WriteResults();
};

#endif // __VisRegTest_h__
