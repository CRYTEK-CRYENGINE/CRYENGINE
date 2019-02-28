// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VisRegTest.cpp
//  Version:     v1.00
//  Created:     14/07/2010 by Nicolas Schulz.
//  Description: Visual Regression Test
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VisRegTest.h"

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <CryInput/IInput.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/IStreamEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

const float CVisRegTest::MaxStreamingWait = 30.0f;

CVisRegTest::CVisRegTest()
	: m_nextCmd(0)
	, m_cmdFreq(0)
	, m_waitFrames(0)
	, m_streamingTimeout(0.0f)
	, m_curSample(0)
	, m_quitAfterTests(false)
{
	CryLog("Enabled visual regression tests");
}

void CVisRegTest::Init(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	stack_string configFile("visregtest.xml");

	// Reset
	m_cmdBuf.clear();
	m_dataSamples.clear();
	m_nextCmd = 0;
	m_cmdFreq = 0;
	m_waitFrames = 0;
	m_streamingTimeout = 0.0f;
	m_testName = "test";
	m_quitAfterTests = false;

	// Parse arguments
	if (pParams->GetArgCount() >= 2)
	{
		m_testName = pParams->GetArg(1);
	}
	if (pParams->GetArgCount() >= 3)
	{
		configFile = pParams->GetArg(2);
	}
	if (pParams->GetArgCount() >= 4)
	{
		if (_stricmp(pParams->GetArg(3), "quit") == 0)
			m_quitAfterTests = true;
	}

	// Fill cmd buffer
	if (!LoadConfig(configFile.c_str()))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "VisRegTest: Failed to load config file '%s' from game folder", configFile.c_str());
		return;
	}

	gEnv->pTimer->SetTimeScale(0);
	gEnv->pSystem->GetRandomGenerator().Seed(0);
	srand(0);
}

void CVisRegTest::AfterRender()
{
	ExecCommands();
}

bool CVisRegTest::LoadConfig(const char* configFile)
{
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(PathUtil::GetGameFolder() + "/" + configFile);
	if (!rootNode || !rootNode->isTag("VisRegTest")) return false;

	m_cmdBuf.push_back(SCmd(eCMDStart, ""));

	for (int i = 0; i < rootNode->getChildCount(); ++i)
	{
		XmlNodeRef node = rootNode->getChild(i);

		if (node->isTag("Config"))
		{
			SCmd cmd;
			for (int j = 0; j < node->getChildCount(); ++j)
			{
				XmlNodeRef node2 = node->getChild(j);

				if (node2->isTag("ConsoleCmd"))
				{
					m_cmdBuf.push_back(SCmd(eCMDConsoleCmd, node2->getAttr("cmd")));
				}
			}
		}
		else if (node->isTag("Map"))
		{
			const char* mapName = node->getAttr("name");
			int imageIndex = 0;

			m_cmdBuf.push_back(SCmd(eCMDLoadMap, mapName));
			m_cmdBuf.push_back(SCmd(eCMDOnMapLoaded, ""));

			for (int j = 0; j < node->getChildCount(); ++j)
			{
				XmlNodeRef node2 = node->getChild(j);

				if (node2->isTag("ConsoleCmd"))
				{
					m_cmdBuf.push_back(SCmd(eCMDConsoleCmd, node2->getAttr("cmd")));
				}
				else if (node2->isTag("Sample"))
				{
					m_cmdBuf.push_back(SCmd(eCMDGoto, node2->getAttr("location")));
					m_cmdBuf.push_back(SCmd(eCMDWaitStreaming, ""));

					char strbuf[256];
#if CRY_PLATFORM_WINDOWS
					cry_sprintf(strbuf, "%s_%i.bmp", mapName, imageIndex++);
#else
					cry_sprintf(strbuf, "%s_%i.tga", mapName, imageIndex++);
#endif
					m_cmdBuf.push_back(SCmd(eCMDCaptureSample, strbuf, SampleCount));
				}
			}
		}
	}

	m_cmdBuf.push_back(SCmd(eCMDFinish, ""));

	return true;
}

void CVisRegTest::ExecCommands()
{
	if (m_nextCmd >= m_cmdBuf.size()) return;

	float col[] = { 0, 1, 0, 1 };
	IRenderAuxText::Draw2dLabel(10, 10, 2, col, false, "Visual Regression Test");

	if (m_waitFrames > 0)
	{
		--m_waitFrames;
		return;
	}
	else if (m_waitFrames < 0)   // Wait for streaming
	{
		SStreamEngineOpenStats stats;
		gEnv->pSystem->GetStreamEngine()->GetStreamingOpenStatistics(stats);
		if (stats.nOpenRequestCount > 0 && m_streamingTimeout > 0)
		{
			gEnv->pConsole->ExecuteString("t_FixedStep 0");
			m_streamingTimeout -= gEnv->pTimer->GetRealFrameTime();
			m_waitFrames = -16;
		}
		else if (++m_waitFrames == 0)
		{
			gEnv->pConsole->ExecuteString("t_FixedStep 0.033333");
			m_waitFrames = 64;  // Wait some more frames for tone-mapper to adapt
		}

		return;
	}

	stack_string tmp;

	while (m_nextCmd < m_cmdBuf.size())
	{
		SCmd& cmd = m_cmdBuf[m_nextCmd];

		if (m_cmdFreq == 0)
			m_cmdFreq = cmd.freq;

		switch (cmd.cmd)
		{
		case eCMDStart:
			break;
		case eCMDFinish:
			Finish();
			break;
		case eCMDOnMapLoaded:
			gEnv->pTimer->SetTimer(ITimer::ETIMER_GAME, 0);
			gEnv->pTimer->SetTimer(ITimer::ETIMER_UI, 0);
			gEnv->pConsole->ExecuteString("t_FixedStep 0.033333");
			gEnv->pSystem->GetRandomGenerator().Seed(0);
			srand(0);
			break;
		case eCMDConsoleCmd:
			gEnv->pConsole->ExecuteString(cmd.args.c_str());
			break;
		case eCMDLoadMap:
			LoadMap(cmd.args.c_str());
			break;
		case eCMDWaitStreaming:
			m_waitFrames = -1;
			m_streamingTimeout = MaxStreamingWait;
			break;
		case eCMDWaitFrames:
			m_waitFrames = (uint32)atoi(cmd.args.c_str());
			break;
		case eCMDGoto:
			tmp = "playerGoto ";
			tmp.append(cmd.args.c_str());
			gEnv->pConsole->ExecuteString(tmp.c_str());
			m_waitFrames = 1;
			break;
		case eCMDCaptureSample:
			CaptureSample(cmd);
			break;
		}

		if (m_cmdFreq-- == 1)
			++m_nextCmd;

		if (m_waitFrames != 0) break;
	}
}

void CVisRegTest::LoadMap(const char* mapName)
{
	string mapCmd("map ");
	mapCmd.append(mapName);
	gEnv->pConsole->ExecuteString(mapCmd.c_str());

	gEnv->pTimer->SetTimer(ITimer::ETIMER_GAME, 0);
	gEnv->pTimer->SetTimer(ITimer::ETIMER_UI, 0);
	gEnv->pConsole->ExecuteString("t_FixedStep 0");
	gEnv->pSystem->GetRandomGenerator().Seed(0);
	srand(0);

	// Disable user input
	gEnv->pInput->EnableDevice(eIDT_Keyboard, false);
	gEnv->pInput->EnableDevice(eIDT_Mouse, false);
	gEnv->pInput->EnableDevice(eIDT_Gamepad, false);
}

void CVisRegTest::CaptureSample(const SCmd& cmd)
{
	if (m_cmdFreq == SampleCount)   // First sample
	{
		m_dataSamples.push_back(SSample());
		gEnv->pConsole->ExecuteString("t_FixedStep 0");
	}

	SSample& sample = m_dataSamples.back();

	// Collect stats
	sample.imageName = cmd.args.c_str();
	sample.frameTime += gEnv->pTimer->GetRealFrameTime() * 1000.f;
	sample.drawCalls += gEnv->pRenderer->GetCurrentNumberOfDrawCalls();

	const RPProfilerStats* pRPPStats = gEnv->pRenderer->GetRPPStatsArray();

	if (pRPPStats)
	{
		sample.gpuTimes[0] += pRPPStats[eRPPSTATS_OverallFrame].gpuTime;

		sample.gpuTimes[1] += pRPPStats[eRPPSTATS_SceneOverall].gpuTime;
		sample.gpuTimes[2] += pRPPStats[eRPPSTATS_ShadowsOverall].gpuTime;
		sample.gpuTimes[3] += pRPPStats[eRPPSTATS_LightingOverall].gpuTime;
		sample.gpuTimes[4] += pRPPStats[eRPPSTATS_VfxOverall].gpuTime;
	}

	if (m_cmdFreq == 1)   // Final sample
	{
		// Screenshot
		stack_string filename("%USER%/TestResults/VisReg/");
		filename += m_testName + "/" + cmd.args.c_str();
		gEnv->pRenderer->ScreenShot(filename);

		// Average results
		sample.frameTime /= (float)SampleCount;
		sample.drawCalls /= SampleCount;
		for (uint32 i = 0; i < MaxNumGPUTimes; ++i)
			sample.gpuTimes[i] /= (float)SampleCount;

		gEnv->pConsole->ExecuteString("t_FixedStep 0.033333");
	}
}

void CVisRegTest::Finish()
{
	WriteResults();

	gEnv->pInput->EnableDevice(eIDT_Keyboard, true);
	gEnv->pInput->EnableDevice(eIDT_Mouse, true);
	gEnv->pInput->EnableDevice(eIDT_Gamepad, true);

	gEnv->pConsole->ExecuteString("t_FixedStep 0");
	gEnv->pTimer->SetTimeScale(1);

	CryLog("VisRegTest: Finished tests");

	if (m_quitAfterTests)
	{
		gEnv->pConsole->ExecuteString("quit");
		return;
	}
}

bool CVisRegTest::WriteResults()
{
	stack_string filename("%USER%/TestResults/VisReg/");
	filename += m_testName + "/visreg_results.xml";

	FILE* f = fxopen(filename.c_str(), "wb");
	if (!f)
		return false;

	fprintf(f, "<testsuites>\n");
	fprintf(f, "\t<testsuite name=\"Visual Regression Test\" LevelName=\"\">\n");

	for (int i = 0; i < (int)m_dataSamples.size(); ++i)
	{
		SSample sample = m_dataSamples[i];

		fprintf(f, "\t\t<phase name=\"%i\" duration=\"1\" image=\"%s\">\n", i, sample.imageName);

		fprintf(f, "\t\t\t<metrics name=\"general\">\n");
		fprintf(f, "\t\t\t\t<metric name=\"frameTime\" value=\"%f\" />\n", sample.frameTime);
		fprintf(f, "\t\t\t\t<metric name=\"drawCalls\" value=\"%u\" />\n", sample.drawCalls);
		fprintf(f, "\t\t\t</metrics>\n");

		fprintf(f, "\t\t\t<metrics name=\"gpu_times\">\n");
		fprintf(f, "\t\t\t\t<metric name=\"frame\" value=\"%f\" />\n", sample.gpuTimes[0]);
		fprintf(f, "\t\t\t\t<metric name=\"scene\" value=\"%f\" />\n", sample.gpuTimes[1]);
		fprintf(f, "\t\t\t\t<metric name=\"shadows\" value=\"%f\" />\n", sample.gpuTimes[2]);
		fprintf(f, "\t\t\t\t<metric name=\"lighting\" value=\"%f\" />\n", sample.gpuTimes[3]);
		fprintf(f, "\t\t\t\t<metric name=\"vfx\" value=\"%f\" />\n", sample.gpuTimes[4]);
		fprintf(f, "\t\t\t</metrics>\n");

		fprintf(f, "\t\t</phase>\n");
	}

	fprintf(f, "\t</testsuite>\n");
	fprintf(f, "</testsuites>");
	fclose(f);

	return true;
}
