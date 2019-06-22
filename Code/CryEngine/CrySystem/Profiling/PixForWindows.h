// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CryProfilingSystemSharedImpl.h"

class CPixForWindows : public CCryProfilingSystemImpl
{
public:
	void Stop() final;
	bool IsStopped() const final;

	void PauseRecording(bool pause) final;
	bool IsPaused() const final;

	void RegisterCVars() final;

	void StartThread() final;
	void EndThread() final;

	void DescriptionCreated(SProfilingSectionDescription*) final;
	void DescriptionDestroyed(SProfilingSectionDescription*) final;

	bool StartSection(SProfilingSection*) final;
	void EndSection(SProfilingSection*) final;

	void RecordMarker(SProfilingMarker*) final;

	void StartFrame() final;
	void EndFrame() final;

	static bool StartSectionStatic(SProfilingSection*);
	static void EndSectionStatic(SProfilingSection*);
	static void RecordMarkerStatic(SProfilingMarker*);
};