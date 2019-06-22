// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryProfilingSystemSharedImpl.h"

#if ALLOW_BROFILER

class CBrofiler : public CCryProfilingSystemImpl
{
public:
	CBrofiler();
	~CBrofiler();

	void Stop() final {}
	bool IsStopped() const final { return false; }

	void PauseRecording(bool pause) final {}
	bool IsPaused() const final { return false; }

	virtual void StartThread() final;
	virtual void EndThread() final;

	void DescriptionCreated(SProfilingSectionDescription*) final;
	void DescriptionDestroyed(SProfilingSectionDescription*) final;
	
	bool StartSection(SProfilingSection*) final;
	void EndSection(SProfilingSection*) final;

	void StartFrame() final {}
	void EndFrame() final {}

	void RecordMarker(SProfilingMarker*) final {}

	void RegisterCVars() final {}

	static bool StartSectionStatic(SProfilingSection*);
	static void EndSectionStatic(SProfilingSection*);

private:
	static CBrofiler* s_pInstance;
};

#endif