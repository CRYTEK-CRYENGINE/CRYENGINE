// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QToolBar>
#include <CryMovie/AnimTime.h>
#include "TrackViewCore.h"
#include "TrackViewCoreComponent.h"

class CAbstractMenu;

class QLabel;
class QContextMenuEvent;
class QTimer;

class CTrackViewPlaybackControlsToolbar : public QToolBar, public CTrackViewCoreComponent
{
	Q_OBJECT

public:
	CTrackViewPlaybackControlsToolbar(CTrackViewCore* pTrackViewCore);
	~CTrackViewPlaybackControlsToolbar() {}

	void        PopulatePlaybackMenu(CAbstractMenu* inOutMenu);
	static void PopulateFramerateMenu(CAbstractMenu* inOutMenu, CTrackViewCore* pTrackViewCore);
	static void PopulatePlaybackSpeedMenu(CAbstractMenu* inOutMenu, CTrackViewCore* pTrackViewCore);

protected:
	virtual void OnPlaybackStateChanged(bool bPlaying, bool bPaused) override;
	virtual void OnRecordingStateChanged(bool bRecording) override;
	virtual void OnLoopingStateChanged(bool bLooping) override;

	virtual void OnTimeChanged(SAnimTime newTime) override;

	// CTrackViewCoreComponent
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override;
	virtual const char* GetComponentTitle() const override { return "Playback Toolbar"; }
	//~CTrackViewCoreComponent

private:
	void OnPlayPause(bool bState);
	void OnRecord(bool bState);
	void OnStop();

	void OnGoToStart();
	void OnGoToEnd();
	void OnLoop(bool bState);

	void OnSetStartMarker();
	void OnSetEndMarker();
	void OnResetMarkers();

	void OnPlayContextMenu(const QPoint& pos) const;
	void OnDisplayLabelContextMenu(const QPoint& pos) const;

	void UpdateTime(SAnimTime newTime, bool bForce = false);
	void UpdateText();

	QAction*                m_pActionPlay;
	QAction*                m_pActionRecord;
	QAction*                m_pActionLoop;

	QLabel*                 m_pDisplayLabel;
	QTimer*                 m_refreshTimer;

	uint                    m_framerate;
	SAnimTime               m_lastTime;
	SAnimTime::EDisplayMode m_displayMode;
};

