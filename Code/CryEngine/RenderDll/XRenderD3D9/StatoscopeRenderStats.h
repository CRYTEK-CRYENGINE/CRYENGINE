// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STATOSCOPERENDERSTATS_H
#define STATOSCOPERENDERSTATS_H

#if ENABLE_STATOSCOPE
class CD3D9Renderer;

class CGPUTimesDG : public IStatoscopeDataGroup
{
public:
	CGPUTimesDG(CD3D9Renderer* pRenderer);

	virtual SDescription GetDescription() const;
	virtual void         Enable();
	virtual void         Write(IStatoscopeFrameRecord& fr);

protected:
	CD3D9Renderer* m_pRenderer;
};

class CDetailedRenderTimesDG : public IStatoscopeDataGroup
{
public:
	CDetailedRenderTimesDG(CD3D9Renderer* pRenderer);

	virtual SDescription GetDescription() const;
	virtual void         Enable();
	virtual void         Write(IStatoscopeFrameRecord& fr);
	virtual uint32       PrepareToWrite();

protected:
	CD3D9Renderer* m_pRenderer;
	const DynArray<RPProfilerDetailedStats>* m_stats;
};

class CGraphicsDG : public IStatoscopeDataGroup
{
public:
	CGraphicsDG(CD3D9Renderer* pRenderer);

	virtual SDescription GetDescription() const;
	virtual void         Write(IStatoscopeFrameRecord& fr);
	virtual void         Enable();

protected:
	void ResetGPUUsageHistory();
	void TrackGPUUsage(float gpuLoad, float frameTimeMs, int totalDPs);

	static const int   GPU_HISTORY_LENGTH = 10;
	static const int   SCREEN_SHOT_FREQ = 60;
	static const int   GPU_TIME_THRESHOLD_MS = 40;
	static const int   DP_THRESHOLD = 2200;

	CD3D9Renderer*     m_pRenderer;
	std::vector<float> m_gpuUsageHistory;
	std::vector<float> m_frameTimeHistory;
	int                m_nFramesGPULmited;
	float              m_totFrameTime;
	int                m_lastFrameScreenShotRequested;
	int                m_cvarScreenCapWhenGPULimited;
};

class CPerformanceOverviewDG : public IStatoscopeDataGroup
{
public:
	CPerformanceOverviewDG(CD3D9Renderer* pRenderer);

	virtual SDescription GetDescription() const;
	virtual void         Write(IStatoscopeFrameRecord& fr);
	virtual void         Enable();

protected:
	CD3D9Renderer* m_pRenderer;
};

#endif // ENABLE_STATOSCOPE

#endif // STATOSCOPERENDERSTATS_H
