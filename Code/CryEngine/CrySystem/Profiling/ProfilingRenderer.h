// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/Profilers/ILegacyProfiler.h>
#include <CryInput/IInput.h>

class CCryProfilingSystem;

class CProfilingRenderer : public ICryProfilerFrameListener, public IInputEventListener
{
public:
	enum EDisplayQuantity
	{
		SELF_VALUE,
		TOTAL_VALUE,
		COUNTS,
		PEAKS,
		SUBSYSTEM_INFO,
		THREAD_INFO,
		MINIMAL,

		DISPLAY_SETTINGS_COUNT,
	};

	CProfilingRenderer();
	~CProfilingRenderer();

	// ICryProfilerFrameListener
	void OnFrameEnd(TTime, ILegacyProfiler*) override;
	// ~ICryProfilerFrameListener

	void Render(CCryProfilingSystem*);
	
	// IInputEventListener
	bool OnInputEvent(const SInputEvent& event) override;
	// ~IInputEventListener

	void RegisterCVars();

private:
	bool Enabled() const { return profile != 0; }
	void DrawLabel(float col, float row, const float* fColor, float glow, const char* szText, float fScale = 1.0f);
	void DrawRect(float col1, float row1, float col2, float row2, const float* fColor);

	void RenderMainHeader(CCryProfilingSystem*);
	void RenderTrackerValues(ILegacyProfiler*);
	void RenderTrackerValues(SProfilingSectionTracker* pProfiler, float row, bool bSelected);

	void RenderPeaks(ILegacyProfiler*);
	void RenderSubSystems(CCryProfilingSystem*);
	void RenderThreadInfo(ILegacyProfiler*);

	SProfilingSectionTracker* GetSelectedTracker(ILegacyProfiler* pProfSystem) const;
	const char* GetNameWithThread(SProfilingSectionTracker* pTracker) const;

	bool m_isCollectionPaused;
	EDisplayQuantity m_displayQuantity;

	// render settings
	int m_selectedRow;

	// CVars
	static void ChangeListenStatusCommand(struct ICVar*);

	static int   profile;
	static float profile_row;
	static float profile_col;
	static float profile_peak_display;
 	static int   profile_log;
	static float profile_min_display_ms;
	static int   profile_hide_waits;
};