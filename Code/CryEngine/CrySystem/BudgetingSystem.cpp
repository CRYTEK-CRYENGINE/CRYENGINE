// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BudgetingSystem.h"
#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/IStreamEngine.h>
#include "System.h"
#include "CrySizerImpl.h"
#include <CryExtension/CryCreateClassInstance.h>
#include <CrySystem/Profilers/IPerfHud.h>

extern int CryMemoryGetAllocatedSize();

const int c_yStepSizeText(18);
const int c_yStepSizeTextMeter(8);
const float c_fontScale(1.3f);

const char c_sys_budget_sysmem[] = "sys_budget_sysmem";
const char c_sys_budget_videomem[] = "sys_budget_videomem";
const char c_sys_budget_frametime[] = "sys_budget_frametime";
const char c_sys_budget_soundchannels[] = "sys_budget_soundchannels";
const char c_sys_budget_soundmem[] = "sys_budget_soundmem";
const char c_sys_budget_soundCPU[] = "sys_budget_soundCPU";
const char c_sys_budget_numdrawcalls[] = "sys_budget_numdrawcalls";
const char c_sys_budget_numpolys[] = "sys_budget_numpolys";
const char c_sys_budget_streamingthroughput[] = "sys_budget_streamingthroughput";

CBudgetingSystem::CBudgetingSystem()
	: m_pRenderer(0)
	, m_pAuxRenderer(0)
	, m_pTimer(0)
	, m_pIAudioSystem(NULL)
	, m_pStreamEngine(NULL)
	, m_sysMemLimitInMB(512)
	, m_videoMemLimitInMB(90)
	, m_frameTimeLimitInMS(50.0f)
	, m_soundChannelsPlayingLimit(32)
	, m_soundMemLimitInMB(32)
	, m_soundCPULimitInPercent(15)
	, m_numDrawCallsLimit(2000)
	, m_numPolysLimit(500000)
	, m_streamingThroughputLimit(1024)
	, m_width(0)
	, m_height(0)
{
	IConsole* pConsole(gEnv->pConsole);
	if (0 != pConsole)
	{
		REGISTER_CVAR2(c_sys_budget_sysmem, &m_sysMemLimitInMB, m_sysMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for system memory (in MB) when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_videomem, &m_videoMemLimitInMB, m_videoMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for video memory (in MB) when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_frametime, &m_frameTimeLimitInMS, m_frameTimeLimitInMS, VF_DUMPTODISK, "Sets the upper limit for frame time (in ms) when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_soundchannels, &m_soundChannelsPlayingLimit, m_soundChannelsPlayingLimit, VF_DUMPTODISK, "Sets the upper limit for sound channels playing when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_soundmem, &m_soundMemLimitInMB, m_soundMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for sound memory (in MB) when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_soundCPU, &m_soundCPULimitInPercent, m_soundCPULimitInPercent, VF_DUMPTODISK, "Sets the upper limit for sound CPU (in Percent) when monitoring budget.");
		REGISTER_CVAR2(c_sys_budget_numdrawcalls, &m_numDrawCallsLimit, m_numDrawCallsLimit, VF_DUMPTODISK, "Sets the upper limit for number of draw calls per frame.");
		REGISTER_CVAR2(c_sys_budget_numpolys, &m_numPolysLimit, m_numPolysLimit, VF_DUMPTODISK, "Sets the upper limit for number of polygons per frame.");
		REGISTER_CVAR2(c_sys_budget_streamingthroughput, &m_streamingThroughputLimit, m_streamingThroughputLimit, VF_DUMPTODISK, "Sets the upper limit for streaming throughput(KB/s).");
	}

	RegisterWithPerfHUD();
}

CBudgetingSystem::~CBudgetingSystem()
{
}

void CBudgetingSystem::RegisterWithPerfHUD()
{
	ICryPerfHUDPtr pPerfHUD;
	CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD);
	minigui::IMiniGUIPtr pGUI;
	CryCreateClassInstanceForInterface(cryiidof<minigui::IMiniGUI>(), pGUI);

	if (pPerfHUD && pGUI)
	{
		minigui::IMiniCtrl* pBudgetMenu = pPerfHUD->CreateMenu("Budgets");

		if (pBudgetMenu)
		{
			pPerfHUD->CreateInfoMenuItem(pBudgetMenu, "Budget Info", CBudgetingSystem::PerfHudRender, minigui::Rect(200, 300, 750, 550));
		}
	}
}

void
CBudgetingSystem::Release()
{
	IConsole* pConsole(gEnv->pConsole);
	if (0 != pConsole)
	{
		pConsole->UnregisterVariable(c_sys_budget_sysmem);
		pConsole->UnregisterVariable(c_sys_budget_videomem);
		pConsole->UnregisterVariable(c_sys_budget_frametime);
		pConsole->UnregisterVariable(c_sys_budget_soundchannels);
		pConsole->UnregisterVariable(c_sys_budget_soundmem);
		pConsole->UnregisterVariable(c_sys_budget_soundCPU);
		pConsole->UnregisterVariable(c_sys_budget_numdrawcalls);
		pConsole->UnregisterVariable(c_sys_budget_numpolys);
		pConsole->UnregisterVariable(c_sys_budget_streamingthroughput);
	}

	delete this;
}

void
CBudgetingSystem::SetSysMemLimit(int sysMemLimitInMB)
{
	m_sysMemLimitInMB = sysMemLimitInMB;
}

void
CBudgetingSystem::SetVideoMemLimit(int videoMemLimitInMB)
{
	m_videoMemLimitInMB = videoMemLimitInMB;
}

void
CBudgetingSystem::SetFrameTimeLimit(float frameTimeLimitInMS)
{
	m_frameTimeLimitInMS = frameTimeLimitInMS;
}

void
CBudgetingSystem::SetSoundChannelsPlayingLimit(int soundChannelsPlayingLimit)
{
	m_soundChannelsPlayingLimit = soundChannelsPlayingLimit;
}

void
CBudgetingSystem::SetSoundMemLimit(int SoundMemLimit)
{
	m_soundMemLimitInMB = SoundMemLimit;
}

void
CBudgetingSystem::SetSoundCPULimit(int SoundCPULimit)
{
	m_soundCPULimitInPercent = SoundCPULimit;
}

void
CBudgetingSystem::SetNumDrawCallsLimit(int numDrawCallsLimit)
{
	m_numDrawCallsLimit = numDrawCallsLimit;
}

void
CBudgetingSystem::SetBudget(int sysMemLimitInMB, int videoMemLimitInMB,
                            float frameTimeLimitInMS, int soundChannelsPlayingLimit, int SoundMemLimitInMB, int SoundCPULimit, int numDrawCallsLimit)
{
	m_sysMemLimitInMB = sysMemLimitInMB;
	m_videoMemLimitInMB = videoMemLimitInMB;
	m_frameTimeLimitInMS = frameTimeLimitInMS;
	m_soundChannelsPlayingLimit = soundChannelsPlayingLimit;
	m_soundMemLimitInMB = SoundMemLimitInMB;
	m_soundCPULimitInPercent = SoundCPULimit;
	m_numDrawCallsLimit = numDrawCallsLimit;
}

int
CBudgetingSystem::GetSysMemLimit() const
{
	return(m_sysMemLimitInMB);
}

int
CBudgetingSystem::GetVideoMemLimit() const
{
	return(m_videoMemLimitInMB);
}

float
CBudgetingSystem::GetFrameTimeLimit() const
{
	return(m_frameTimeLimitInMS);
}

int
CBudgetingSystem::GetSoundChannelsPlayingLimit() const
{
	return(m_soundChannelsPlayingLimit);
}

int
CBudgetingSystem::GetSoundMemLimit() const
{
	return(m_soundMemLimitInMB);
}

int
CBudgetingSystem::GetSoundCPULimit() const
{
	return(m_soundCPULimitInPercent);
}

int
CBudgetingSystem::GetNumDrawCallsLimit() const
{
	return(m_numDrawCallsLimit);
}

void
CBudgetingSystem::GetBudget(int& sysMemLimitInMB, int& videoMemLimitInMB,
                            float& frameTimeLimitInMS, int& soundChannelsPlayingLimit, int& soundMemLimitInMB, int& soundCPULimit, int& numDrawCallsLimit) const
{
	sysMemLimitInMB = m_sysMemLimitInMB;
	videoMemLimitInMB = m_videoMemLimitInMB;
	frameTimeLimitInMS = m_frameTimeLimitInMS;
	soundChannelsPlayingLimit = m_soundChannelsPlayingLimit;
	soundMemLimitInMB = m_soundMemLimitInMB;
	soundCPULimit = m_soundCPULimitInPercent;
	numDrawCallsLimit = m_numDrawCallsLimit;
}

void
CBudgetingSystem::MonitorBudget()
{
	// get required interfaces
	if (0 == m_pRenderer)
		m_pRenderer = gEnv->pRenderer;

	if (!m_pRenderer)
	{
		return;
	}

	// set to 2D mode for font rendering
	m_pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(true, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f);

	Render(60.f, 40.f);

	// set back to 3D mode
	m_pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(false);
}

void CBudgetingSystem::Render(float x, float y)
{
	// get required interfaces
	if (0 == m_pRenderer)
		m_pRenderer = gEnv->pRenderer;

	if (0 == m_pAuxRenderer)
		m_pAuxRenderer = m_pRenderer->GetIRenderAuxGeom();

	if (0 == m_pTimer)
		m_pTimer = gEnv->pTimer;

	if (0 == m_pIAudioSystem)
		m_pIAudioSystem = gEnv->pAudioSystem;

	if (0 == m_pStreamEngine)
		m_pStreamEngine = gEnv->pSystem->GetStreamEngine();

	// get height and width of view port
	m_width  = m_pRenderer->GetOverlayWidth();
	m_height = m_pRenderer->GetOverlayHeight();

	//Aux Render setup
	SAuxGeomRenderFlags oldFlags = m_pAuxRenderer->GetRenderFlags();

	SAuxGeomRenderFlags flags(e_Def2DPublicRenderflags);
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetDepthWriteFlag(e_DepthWriteOff);
	flags.SetCullMode(e_CullModeNone);
	m_pAuxRenderer->SetRenderFlags(flags);

	x += 10.f;
	y += 5.f;

	// draw meters
	MonitorSystemMemory(x, y);
	MonitorVideoMemory(x, y);
	MonitorFrameTime(x, y);
	MonitorSoundChannels(x, y);
	MonitorSoundMemory(x, y);
	MonitorSoundCPU(x, y);
	MonitorStreaming(x, y);
	MonitorDrawCalls(x, y);
	MonitorPolyCount(x, y);

	// warning if in editor
	if (gEnv->IsEditor())
	{
		float color[4] = { 1, 0, 0, 1 };
		DrawText(x, y, color, "WARNING: Editor makes budgets invalid (check it only in pure game)");
	}

	//restore Aux Render setup
	m_pAuxRenderer->SetRenderFlags(oldFlags);
}

void
CBudgetingSystem::DrawText(float& x, float& y, float* pColor, const char* format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	IRenderAuxText::Draw2dLabel(x, y, c_fontScale, pColor, false, "%s", buffer);
	y += c_yStepSizeText;
}

void
CBudgetingSystem::DrawMeter(float& x, float& y, float scale)
{
	// draw frame for meter
	vtx_idx indLines[8] =
	{
		0, 1, 1, 2,
		2, 3, 3, 0
	};

	const float barWidth = 0.4f; //normalised screen size
	const float yellowStart = 0.5f * barWidth;
	const float redStart = 0.75f * barWidth;

	Vec3 frame[4] =
	{
		Vec3((x - 1) / (float) m_width,      (y - 1) / (float) m_height,                    0),
		Vec3(x / (float) m_width + barWidth, (y - 1) / (float) m_height,                    0),
		Vec3(x / (float) m_width + barWidth, (y + c_yStepSizeTextMeter) / (float) m_height, 0),
		Vec3((x - 1) / (float) m_width,      (y + c_yStepSizeTextMeter) / (float) m_height, 0)
	};

	m_pAuxRenderer->DrawLines(frame, 4, indLines, 8, ColorB(255, 255, 255, 255));

	// draw meter itself
	vtx_idx indTri[6] =
	{
		0, 1, 2,
		0, 2, 3
	};

	// green part (0.0 <= scale <= 0.5)
	{
		float lScale(max(min(scale, 0.5f), 0.0f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) m_width,                     y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) m_height, 0),
			Vec3(x / (float) m_width,                     (y + c_yStepSizeTextMeter) / (float) m_height, 0)
		};
		m_pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, ColorB(0, 255, 0, 255));
	}

	// green to yellow part (0.5 < scale <= 0.75)
	if (scale > 0.5f)
	{
		float lScale(min(scale, 0.75f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) m_width + yellowStart,       y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) m_height, 0),
			Vec3(x / (float) m_width + yellowStart,       (y + c_yStepSizeTextMeter) / (float) m_height, 0)
		};

		float color[4];
		GetColor(lScale, color);

		ColorB colSegStart(0, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		m_pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
	}

	// yellow to red part (0.75 < scale <= 1.0)
	if (scale > 0.75f)
	{
		float lScale(min(scale, 1.0f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) m_width + redStart,          y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, y / (float) m_height,                          0),
			Vec3(x / (float) m_width + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) m_height, 0),
			Vec3(x / (float) m_width + redStart,          (y + c_yStepSizeTextMeter) / (float) m_height, 0)
		};

		float color[4];
		GetColor(lScale, color);

		ColorB colSegStart(255, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		m_pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
	}

	y += c_yStepSizeTextMeter;
}

void
CBudgetingSystem::GetColor(float scale, float* pColor)
{
	if (scale <= 0.5f)
	{
		pColor[0] = 0;
		pColor[1] = 1;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else if (scale <= 0.75f)
	{
		pColor[0] = (scale - 0.5f) * 4.0f;
		pColor[1] = 1;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else if (scale <= 1.0f)
	{
		pColor[0] = 1;
		pColor[1] = 1 - (scale - 0.75f) * 4.0f;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else
	{
		float time(m_pTimer->GetAsyncCurTime());
		float blink(sinf(time * 6.28f) * 0.5f + 0.5f);
		pColor[0] = 1;
		pColor[1] = blink;
		pColor[2] = blink;
		pColor[3] = 1;
	}
}

void
CBudgetingSystem::MonitorSystemMemory(float& x, float& y)
{
	if (!m_pRenderer)
		return;

	int memUsageInMB_Engine(CryMemoryGetAllocatedSize());
	int memUsageInMB_SysCopyMeshes = 0;
	int memUsageInMB_SysCopyTextures = 0;
	m_pRenderer->EF_Query(EFQ_Alloc_APIMesh, memUsageInMB_SysCopyMeshes);
	m_pRenderer->EF_Query(EFQ_Alloc_APITextures, memUsageInMB_SysCopyTextures);

	int memUsageInMB(RoundToClosestMB((size_t) memUsageInMB_Engine +
	                                  (size_t) memUsageInMB_SysCopyMeshes + (size_t) memUsageInMB_SysCopyTextures));

	memUsageInMB_Engine = RoundToClosestMB(memUsageInMB_Engine);
	memUsageInMB_SysCopyMeshes = RoundToClosestMB(memUsageInMB_SysCopyMeshes);
	memUsageInMB_SysCopyTextures = RoundToClosestMB(memUsageInMB_SysCopyTextures);

	float scale((float) memUsageInMB / (float) m_sysMemLimitInMB);

	float color[4];
	GetColor(scale, color);

	if (scale <= 1.0f)
	{
		DrawText(x, y, color, "System memory usage: %d MB (current budget is %d MB).", memUsageInMB, m_sysMemLimitInMB);
	}
	else
	{
		DrawText(x, y, color, "System memory usage: %d MB (current budget is %d MB). You're over budget!!!", memUsageInMB, m_sysMemLimitInMB);
	}

	DrawText(x, y, color, "[%d MB (engine), %d MB (managed textures), %d MB (managed meshes)]", memUsageInMB_Engine, memUsageInMB_SysCopyTextures, memUsageInMB_SysCopyMeshes);

	DrawMeter(x, y, scale);
}

void
CBudgetingSystem::MonitorVideoMemory(float& x, float& y)
{
	if (!m_pRenderer)
		return;

	size_t vidMemUsedThisFrame(0), vidMemUsedRecently(0);
	m_pRenderer->GetVideoMemoryUsageStats(vidMemUsedThisFrame, vidMemUsedRecently);

	int vidMemUsedThisFrameMB = RoundToClosestMB(vidMemUsedThisFrame);
	int vidMemUsedRecentlyMB = RoundToClosestMB(vidMemUsedRecently);

	float scale((float) vidMemUsedRecentlyMB / (float) m_videoMemLimitInMB);

	float color[4];
	GetColor(scale, color);

	if (scale <= 1.0f)
		DrawText(x, y, color, "Video mem usage: %d MB this frame / %d MB recently (current budget is: %d MB).", vidMemUsedThisFrameMB, vidMemUsedRecentlyMB, m_videoMemLimitInMB);
	else
		DrawText(x, y, color, "Video mem usage: %d MB this frame / %d MB recently (current budget is: %d MB). You're over budget, texture thrashing is likely to occur!!!", vidMemUsedThisFrameMB, vidMemUsedRecentlyMB, m_videoMemLimitInMB);

	DrawMeter(x, y, scale);
}

void
CBudgetingSystem::MonitorFrameTime(float& x, float& y)
{
	if (!m_pTimer)
		return;

	static float s_fps(100.0f);
	static float s_startTime(m_pTimer->GetAsyncCurTime());
	static float s_fpsAccu(0.0f);
	static int s_numFramesMeasured(0);

	// accumulate all fps over a period of one second
	s_fpsAccu += m_pTimer->GetFrameRate();
	++s_numFramesMeasured;

	// check if accumulation period ellapsed,
	// if so calc new average fps and reset accumulators
	float curTime(m_pTimer->GetAsyncCurTime());
	if (curTime - s_startTime >= 1.0f)
	{
		s_fps = s_fpsAccu / s_numFramesMeasured;

		s_startTime = curTime;
		s_fpsAccu = 0.0f;
		s_numFramesMeasured = 0;
	}

	// calc scale and update budget info
	float curFrameTimeInMS(1000.0f / s_fps);
	float scale(curFrameTimeInMS / m_frameTimeLimitInMS);

	float color[4];
	GetColor(scale, color);

	if (scale <= 1.0f)
	{
		DrawText(x, y, color, "Frame time: %3.1f ms = %3.1f fps (current budget is %3.1f ms = %3.1f fps).", curFrameTimeInMS, s_fps, m_frameTimeLimitInMS, 1000.0f / m_frameTimeLimitInMS);
	}
	else
	{
		DrawText(x, y, color, "Frame time: %3.1f ms = %3.1f fps (current budget is %3.1f ms = %3.1f fps). You're over budget!!!", curFrameTimeInMS, s_fps, m_frameTimeLimitInMS, 1000.0f / m_frameTimeLimitInMS);
	}

	DrawMeter(x, y, scale);
}

void
CBudgetingSystem::MonitorSoundChannels(float& x, float& y)
{
	/*if(!m_pIAudioSystem)
	   return;
	   int channelsPlaying= m_pIAudioSystem->GetInterfaceExtended()->GetUsedVoices();
	   float scale = 1.0f;

	   if (m_soundChannelsPlayingLimit)
	   scale = (float) channelsPlaying / (float) m_soundChannelsPlayingLimit;

	   float color[ 4 ];
	   GetColor( scale, color );

	   if( scale <= 1.0f )
	   {
	   DrawText( x, y, color, "Sound Channels playing: %d (current budget is %d).", channelsPlaying, m_soundChannelsPlayingLimit );
	   }
	   else
	   {
	   DrawText( x, y, color, "Sound Channels playing: %d (current budget is %d). You're over budget!!!", channelsPlaying, m_soundChannelsPlayingLimit );
	   }

	   DrawMeter( x, y, scale );*/
}

void
CBudgetingSystem::MonitorSoundMemory(float& x, float& y)
{
	/*if (!m_pIAudioSystem)
	   return;

	   unsigned int nSizeInMB = 0;
	   unsigned int nCacheSizeAllInMB = 0;
	   float scale = 1.0f;
	   m_pIAudioSystem->GetIAudioDevice()->GetMemoryStats(admqALL, &nSizeInMB);
	   m_pIAudioSystem->GetIAudioDevice()->GetMemoryStats(admqCACHESIZEALL, &nCacheSizeAllInMB);
	   nSizeInMB = (nSizeInMB/1024)/1024;
	   nCacheSizeAllInMB = (nCacheSizeAllInMB/1024)/1024;

	   if (nCacheSizeAllInMB)
	   scale = (float) nSizeInMB / (float) nCacheSizeAllInMB;

	   float color[ 4 ];
	   GetColor( scale, color );

	   if( scale <= 1.0f )
	   {
	    DrawText( x, y, color, "Sound mem usage: %d MB (current budget is %d).", nSizeInMB, nCacheSizeAllInMB );
	   }
	   else
	   {
	   DrawText( x, y, color, "Sound mem usage: %d MB (current budget is %d). You're over budget!!!", nSizeInMB, nCacheSizeAllInMB );
	   }

	   DrawMeter( x, y, scale );*/
}

void
CBudgetingSystem::MonitorSoundCPU(float& x, float& y)
{
	/*if (!m_pIAudioSystem)
	   return;

	   float fCPUPercent = m_pIAudioSystem->GetInterfaceExtended()->GetCPUUsage();
	   float scale = 1.0f;
	   float color[ 4 ];

	   if (m_soundCPULimitInPercent)
	   scale = fCPUPercent / (float) m_soundCPULimitInPercent;

	   GetColor( scale, color );

	   if( scale <= 1.0f )
	   {
	   DrawText( x, y, color, "Sound CPU usage: %.1f Percent (current budget is %d Percent).", fCPUPercent, m_soundCPULimitInPercent );
	   }
	   else
	   {
	   DrawText( x, y, color, "Sound CPU usage: %.1f Percent (current budget is %d Percent). You're over budget!!!", fCPUPercent, m_soundCPULimitInPercent );
	   }

	   DrawMeter( x, y, scale );*/
}

void CBudgetingSystem::MonitorDrawCalls(float& x, float& y)
{
	if (!m_pRenderer)
		return;
	int numDrawCalls(m_pRenderer->GetCurrentNumberOfDrawCalls());
	float scale((float) numDrawCalls / (float) m_numDrawCallsLimit);

	float color[4];
	GetColor(scale, color);

	if (scale <= 1.0f)
	{
		DrawText(x, y, color, "Number of draw calls: %d (current budget is %d).", numDrawCalls, m_numDrawCallsLimit);
	}
	else
	{
		DrawText(x, y, color, "Number of draw calls: %d (current budget is %d). You're over budget!!!", numDrawCalls, m_numDrawCallsLimit);
	}

	DrawMeter(x, y, scale);
}

void CBudgetingSystem::MonitorPolyCount(float& x, float& y)
{
	if (!m_pRenderer)
		return;

	int polyCount = m_pRenderer->GetPolyCount();

	float scale = (float)polyCount / (float)m_numPolysLimit;

	float color[4];
	GetColor(scale, color);

	if (scale <= 1.0f)
	{
		DrawText(x, y, color, "Number of tris: %d (current budget is %d).", polyCount, m_numPolysLimit);
	}
	else
	{
		DrawText(x, y, color, "Number of tris: %d (current budget is %d). You're over budget!!!", polyCount, m_numPolysLimit);
	}

	DrawMeter(x, y, scale);
}

void CBudgetingSystem::MonitorStreaming(float& x, float& y)
{
	if (!m_pStreamEngine || !m_pTimer)
		return;

#if defined(STREAMENGINE_ENABLE_STATS)
	static float thp = 0.f;

	SStreamEngineStatistics& stats = m_pStreamEngine->GetStreamingStatistics();

	//float dt = max(stats.m_fDeltaTime / 1000, .00001f);
	float dt = 1.0f;

	float newThp = (float)stats.nTotalCurrentReadBandwidth / 1024;
	thp = (thp + min(1.f, dt / 5) * (newThp - thp));  // lerp

	float scale = thp / m_streamingThroughputLimit;

	float color[4];
	GetColor(scale, color);

	DrawText(x, y, color, "Streaming throughput: %.2f KB/s (current budget is %.2f KB/s).", thp, m_streamingThroughputLimit);

	DrawMeter(x, y, scale);
#endif
}

float CBudgetingSystem::GetStreamingThroughputLimit() const
{
	return m_streamingThroughputLimit;
}

void CBudgetingSystem::SetStreamingThroughputLimit(float streamingThroughputLimit)
{
	m_streamingThroughputLimit = streamingThroughputLimit;
}

//////////////////////////////////////////////////////////////////////////
void CBudgetingSystem::PerfHudRender(float x, float y)
{
	IBudgetingSystem* pBudgetSystem = gEnv->pSystem->GetIBudgetingSystem();

	if (pBudgetSystem)
	{
		pBudgetSystem->Render(x, y);
	}
}
