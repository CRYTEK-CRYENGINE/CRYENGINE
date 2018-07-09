// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TrackViewCore.h"
#include "AnimationContext.h"
#include "TrackViewSequenceManager.h"
#include "Nodes/TrackViewSequence.h"

enum ETrackViewEditorEvent
{
	eTrackViewEditorEvent_OnPlaybackSpeedChanged = 0,
	eTrackViewEditorEvent_OnSnapModeChanged,
	eTrackViewEditorEvent_OnKeyMoveModeChanged,
	eTrackViewEditorEvent_OnFramerateChanged,
	eTrackViewEditorEvent_OnDisplayModeChanged,
	eTrackViewEditorEvent_OnPropertiesChanged
};

class CTrackViewCoreComponent :
	public ITrackViewSequenceManagerListener,
	public ITrackViewSequenceListener,
	public IAnimationContextListener
{
public:
	CTrackViewCoreComponent(CTrackViewCore* pTrackViewCore, bool bUseEngineListeners);
	virtual ~CTrackViewCoreComponent();

	void                Initialize();

	virtual const char* GetComponentTitle() const = 0;
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) = 0;

	static int GetUiRefreshRateMilliseconds();

protected:
	// IAnimationContextListener
	virtual void OnSequenceChanged(CTrackViewSequence* pSequence) override    {}
	virtual void OnTimeChanged(SAnimTime newTime) override                    {}
	virtual void OnPlaybackStateChanged(bool bPlaying, bool bPaused) override {}
	virtual void OnRecordingStateChanged(bool bRecording) override            {}
	// ~IAnimationContextListener

	// ITrackViewSequenceManagerListener
	virtual void OnSequenceAdded(CTrackViewSequence* pSequence) override   { pSequence->AddListener(this); }
	virtual void OnSequenceRemoved(CTrackViewSequence* pSequence) override { pSequence->RemoveListener(this); }
	// ~ITrackViewSequenceManagerListener

	// ITrackViewSequenceListener
	virtual void OnSequenceSettingsChanged(CTrackViewSequence* pSequence, SSequenceSettingsChangedEventArgs& args) override {}
	virtual void OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type) override                                        {}
	virtual void OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName) override                                        {}
	// ~ITrackViewSequenceListener

	void            SetUseEngineListeners(bool bEnable);
	bool            IsUsingEngineListeners() const { return m_bUseEngineListeners; }
	CTrackViewCore* GetTrackViewCore() const       { return m_pTrackViewCore; }

private:
	const bool      m_bUseEngineListeners;
	bool            m_bIsListening;
	CTrackViewCore* m_pTrackViewCore;
};

