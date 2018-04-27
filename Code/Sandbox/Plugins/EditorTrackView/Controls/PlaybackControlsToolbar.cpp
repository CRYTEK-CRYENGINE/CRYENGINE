// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlaybackControlsToolbar.h"
#include "TrackViewPlugin.h"

#include "Menu/AbstractMenu.h"
#include "Menu/MenuWidgetBuilders.h"

#include <QAction>
#include <QPixmap>
#include <QString>
#include <QIcon>

#include <QMenu>
#include <QLabel>
#include <QActionEvent>
#include <QContextMenuEvent>
#include <QTimer>

#include <CryIcon.h>

CTrackViewPlaybackControlsToolbar::CTrackViewPlaybackControlsToolbar(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, true)
{
	setMovable(true);
	setWindowTitle(tr("Playback Controls"));

	m_pDisplayLabel = new QLabel(tr("00:00:00:00f"), this);
	m_pDisplayLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	m_pDisplayLabel->setMinimumWidth(130);
	m_pDisplayLabel->setAlignment(Qt::AlignCenter);
	addWidget(m_pDisplayLabel);

	auto action = addAction(CryIcon("icons:Trackview/Rewind_Backward_End.ico"), tr("Go to start of sequence"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnGoToStart);

	QIcon* pPlayPauseIcon = new CryIcon();
	pPlayPauseIcon->addPixmap(QPixmap("icons:Trackview/Play_Sequence.ico"), QIcon::Normal, QIcon::Off);
	pPlayPauseIcon->addPixmap(QPixmap("icons:Trackview/trackview_time_pause.ico"), QIcon::Normal, QIcon::On);

	m_pActionPlay = new QAction(*pPlayPauseIcon, tr("Play"), this);
	m_pActionPlay->connect(m_pActionPlay, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnPlayPause);
	m_pActionPlay->setCheckable(true);
	addAction(m_pActionPlay);

	action = addAction(CryIcon("icons:Trackview/Stop_Sequence.ico"), tr("Stop"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnStop);

	action = addAction(CryIcon("icons:Trackview/Rewind_Forward_End.ico"), tr("Go to end of sequence"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnGoToEnd);

	m_pActionLoop = addAction(CryIcon("icons:Trackview/trackview_time_loop.ico"), tr("Loop"));
	m_pActionLoop->setCheckable(true);
	connect(m_pActionLoop, &QAction::toggled, this, &CTrackViewPlaybackControlsToolbar::OnLoop);

	addSeparator();

	action = addAction(CryIcon("icons:Trackview/trackview_time_limit_L_toolbar.ico"), tr("Set Playback Start"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnSetStartMarker);

	action = addAction(CryIcon("icons:Trackview/trackview_time_limit_R_toolbar.ico"), tr("Set Playback End"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnSetEndMarker);

	addSeparator();

	m_pActionRecord = addAction(CryIcon("icons:Trackview/trackview_time_record.ico"), tr("Record"));
	m_pActionRecord->setCheckable(true);
	connect(m_pActionRecord, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnRecord);

	auto playActionWidget = widgetForAction(m_pActionPlay);
	playActionWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(playActionWidget, &QWidget::customContextMenuRequested, this, &CTrackViewPlaybackControlsToolbar::OnPlayContextMenu);

	m_pDisplayLabel->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pDisplayLabel, &QWidget::customContextMenuRequested, this, &CTrackViewPlaybackControlsToolbar::OnDisplayLabelContextMenu);

	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	connect(m_refreshTimer, &QTimer::timeout, this, &CTrackViewPlaybackControlsToolbar::UpdateText);
}

void CTrackViewPlaybackControlsToolbar::OnPlayPause(bool bState)
{
	GetTrackViewCore()->OnPlayPause(bState);
}

void CTrackViewPlaybackControlsToolbar::OnStop()
{
	GetTrackViewCore()->OnStop();
}

void CTrackViewPlaybackControlsToolbar::OnGoToStart()
{
	GetTrackViewCore()->OnGoToStart();
}

void CTrackViewPlaybackControlsToolbar::OnGoToEnd()
{
	GetTrackViewCore()->OnGoToEnd();
}

void CTrackViewPlaybackControlsToolbar::OnLoop(bool bState)
{
	GetTrackViewCore()->OnLoop(bState);
}

void CTrackViewPlaybackControlsToolbar::OnSetStartMarker()
{
	GetTrackViewCore()->SetPlaybackStart();
}

void CTrackViewPlaybackControlsToolbar::OnSetEndMarker()
{
	GetTrackViewCore()->SetPlaybackEnd();
}

void CTrackViewPlaybackControlsToolbar::OnResetMarkers()
{
	GetTrackViewCore()->ResetPlaybackRange();
}

void CTrackViewPlaybackControlsToolbar::OnRecord(bool bState)
{
	GetTrackViewCore()->OnRecord(bState);
}

void CTrackViewPlaybackControlsToolbar::PopulatePlaybackMenu(CAbstractMenu* inOutMenu)
{
	auto action = inOutMenu->CreateAction(CryIcon("icons:Trackview/Play_Sequence.ico"), tr("Play"));
	connect(action, &QAction::triggered, [this]() { OnPlayPause(true); });

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/trackview_time_pause.ico"), tr("Pause"));
	connect(action, &QAction::triggered, [this]() { OnPlayPause(false); });

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/Stop_Sequence.ico"), tr("Stop"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnStop);

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/trackview_time_loop.ico"), tr("Loop"));
	action->setCheckable(true);
	action->setChecked(GetTrackViewCore()->IsLooping());
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnLoop);

	int sec = inOutMenu->GetNextEmptySection();

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/trackview_time_limit_L_toolbar.ico"), tr("Set Playback Start"), sec);
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnSetStartMarker);

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/trackview_time_limit_R_toolbar.ico"), tr("Set Playback End"), sec);
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnSetEndMarker);

	action = inOutMenu->CreateAction(tr("Reset Start/End"));
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnResetMarkers);

	sec = inOutMenu->GetNextEmptySection();

	action = inOutMenu->CreateAction(CryIcon("icons:Trackview/trackview_time_record.ico"), tr("Record"), sec);
	action->setCheckable(true);
	action->setChecked(GetTrackViewCore()->IsRecording());
	connect(action, &QAction::triggered, this, &CTrackViewPlaybackControlsToolbar::OnRecord);

	sec = inOutMenu->GetNextEmptySection();

	CAbstractMenu* const playbackMenu = inOutMenu->CreateMenu(tr("Playback Speed"), sec);
	playbackMenu->signalAboutToShow.Connect([playbackMenu, this]()
	{
		playbackMenu->Clear();
		PopulatePlaybackSpeedMenu(playbackMenu, GetTrackViewCore());
	});

	CAbstractMenu* const framerateMenu = inOutMenu->CreateMenu(tr("Framerate"));
	framerateMenu->signalAboutToShow.Connect([framerateMenu, this]()
	{
		framerateMenu->Clear();
		PopulateFramerateMenu(framerateMenu, GetTrackViewCore());
	});
}

void CTrackViewPlaybackControlsToolbar::PopulateFramerateMenu(CAbstractMenu* inOutMenu, CTrackViewCore* pTrackViewCore)
{
	int sec = inOutMenu->GetNextEmptySection();
	for (uint i = 0; i < SAnimTime::eFrameRate_Num; ++i)
	{
		const char* szFrameRateName = SAnimTime::GetFrameRateName((SAnimTime::EFrameRate)i);

		auto action = inOutMenu->CreateAction(szFrameRateName, sec);
		action->setCheckable(true);
		action->setChecked(pTrackViewCore->GetCurrentFramerate() == i);
		connect(action, &QAction::triggered, [pTrackViewCore, i]() { pTrackViewCore->SetFramerate((SAnimTime::EFrameRate)i); });

		if (i == SAnimTime::eFrameRate_120fps)
		{
			sec = inOutMenu->GetNextEmptySection();
		}
	}
}

static const float gPlaybackSpeeds[] = { 0.01f, 0.1f, 0.25f, 0.50f, 0.f, 1.0f, 0.f, 1.5f, 2.0f, 10.0f };

void CTrackViewPlaybackControlsToolbar::PopulatePlaybackSpeedMenu(CAbstractMenu* inOutMenu, CTrackViewCore* pTrackViewCore)
{
	int sec = inOutMenu->GetNextEmptySection();
	for (size_t i = 0; i < sizeof(gPlaybackSpeeds) / sizeof(gPlaybackSpeeds[0]); ++i)
	{
		float speed = gPlaybackSpeeds[i];
		if (speed == 0.0f)
		{
			sec = inOutMenu->GetNextEmptySection();
		}
		else
		{
			QString str;
			str.sprintf("%.2fx", speed);

			auto action = inOutMenu->CreateAction(str, sec);
			action->setCheckable(true);
			action->setChecked(pTrackViewCore->GetCurrentPlaybackSpeed() == speed);
			connect(action, &QAction::triggered, [pTrackViewCore, speed]() { pTrackViewCore->SetPlaybackSpeed(speed); });
		}
	}
}

void CTrackViewPlaybackControlsToolbar::OnPlayContextMenu(const QPoint& pos) const
{
	CAbstractMenu abstractMenu;
	PopulatePlaybackSpeedMenu(&abstractMenu, GetTrackViewCore());
	auto menu = new QMenu();
	abstractMenu.Build(MenuWidgetBuilders::CMenuBuilder(menu));
	menu->exec(widgetForAction(m_pActionPlay)->mapToGlobal(pos));
}

void CTrackViewPlaybackControlsToolbar::OnDisplayLabelContextMenu(const QPoint& pos) const
{
	CAbstractMenu abstractMenu;
	PopulateFramerateMenu(&abstractMenu, GetTrackViewCore());
	auto menu = new QMenu();
	abstractMenu.Build(MenuWidgetBuilders::CMenuBuilder(menu));
	menu->exec(m_pDisplayLabel->mapToGlobal(pos));
}

void CTrackViewPlaybackControlsToolbar::OnTrackViewEditorEvent(ETrackViewEditorEvent event)
{
	switch (event)
	{
	case eTrackViewEditorEvent_OnFramerateChanged:
		{
			SAnimTime::EFrameRate currentFramerate = GetTrackViewCore()->GetCurrentFramerate();
			m_framerate = SAnimTime::GetFrameRateValue(currentFramerate);
			//intentional fall through
		}
	case eTrackViewEditorEvent_OnDisplayModeChanged:
	case eTrackViewEditorEvent_OnPlaybackSpeedChanged:
		{
			m_displayMode = GetTrackViewCore()->GetCurrentDisplayMode();
			UpdateTime(CTrackViewPlugin::GetAnimationContext()->GetTime(), true);
			break;
		}
	}
}

void CTrackViewPlaybackControlsToolbar::UpdateTime(SAnimTime newTime, bool bForce)
{
	if ((newTime == m_lastTime) && !bForce)
	{
		return;
	}

	m_lastTime = newTime;

	if (bForce)
	{
		UpdateText();
	}
	else if (!m_refreshTimer->isActive())
	{
		m_refreshTimer->start(GetUiRefreshRateMilliseconds());
	}
}

void CTrackViewPlaybackControlsToolbar::UpdateText()
{
	QString text;
	text.reserve(256);

	const int32 ticks = m_lastTime.GetTicks();
	const int32 totalSeconds = (int32)ticks / SAnimTime::numTicksPerSecond;
	const int32 totalRemainder = ticks % SAnimTime::numTicksPerSecond;
	const int32 mins = (int32)totalSeconds / 60;
	const int32 secs = totalSeconds % 60;

	switch (m_displayMode)
	{
	case SAnimTime::EDisplayMode::Ticks:
		{
			text.sprintf("%010d (%d tps)", ticks, SAnimTime::numTicksPerSecond);
		}
		break;

	case SAnimTime::EDisplayMode::Frames:
		{
			const int32 ticksPerFrame = SAnimTime::numTicksPerSecond / m_framerate;
			const int32 totalFrames = ticks / ticksPerFrame;
			text.sprintf("%06d (%d fps)", totalFrames, m_framerate);
		}
		break;

	case SAnimTime::EDisplayMode::Time:
		{
			const int32 millisecs = int_round(((float)totalRemainder / SAnimTime::numTicksPerSecond) * 1000.f);
			text.sprintf("%02d:%02d:%03d (%d fps)", mins, secs, millisecs, m_framerate);
		}
		break;

	case SAnimTime::EDisplayMode::Timecode:
		{
			const int32 ticksPerFrame = SAnimTime::numTicksPerSecond / m_framerate;
			const int32 leftOverFrames = totalRemainder / (ticksPerFrame == 0 ? 1 : ticksPerFrame);

			if (m_framerate < 100)
				text.sprintf("%02d:%02d:%02d (%d fps)", mins, secs, leftOverFrames, m_framerate);
			else if (m_framerate < 1000)
				text.sprintf("%02d:%02d:%03d (%d fps)", mins, secs, leftOverFrames, m_framerate);
			else
				text.sprintf("%02d:%02d:%04d (%d fps)", mins, secs, leftOverFrames, m_framerate);
		}
		break;

	default:
		text.sprintf("Undefined Display Mode");
		break;
	}

	m_pDisplayLabel->setText(text);
}


void CTrackViewPlaybackControlsToolbar::OnPlaybackStateChanged(bool bPlaying, bool bPaused)
{
	m_pActionPlay->setChecked(bPlaying && !bPaused);
}

void CTrackViewPlaybackControlsToolbar::OnRecordingStateChanged(bool bRecording)
{
	m_pActionRecord->setChecked(bRecording);
}

void CTrackViewPlaybackControlsToolbar::OnLoopingStateChanged(bool bLooping)
{
	m_pActionLoop->setChecked(bLooping);
}

void CTrackViewPlaybackControlsToolbar::OnTimeChanged(SAnimTime newTime)
{
	UpdateTime(newTime);
}

