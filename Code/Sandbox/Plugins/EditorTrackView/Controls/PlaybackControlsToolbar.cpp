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
	for (uint i = 0; i < SAnimData::eFrameRate_Num; ++i)
	{
		const char* szFrameRateName = SAnimData::GetFrameRateName((SAnimData::EFrameRate)i);

		auto action = inOutMenu->CreateAction(szFrameRateName, sec);
		action->setCheckable(true);
		action->setChecked(pTrackViewCore->GetCurrentFramerate() == i);
		connect(action, &QAction::triggered, [pTrackViewCore, i]() { pTrackViewCore->SetFramerate((SAnimData::EFrameRate)i); });

		if (i == SAnimData::eFrameRate_120fps)
		{
			sec = inOutMenu->GetNextEmptySection();
		}
	}
}

static const mpfloat gPlaybackSpeeds[] = { "0.01", "0.1", "0.25", "0.50", 0, 1, 0, "1.5", 2, 10 };

void CTrackViewPlaybackControlsToolbar::PopulatePlaybackSpeedMenu(CAbstractMenu* inOutMenu, CTrackViewCore* pTrackViewCore)
{
	int sec = inOutMenu->GetNextEmptySection();
	for (size_t i = 0; i < sizeof(gPlaybackSpeeds) / sizeof(gPlaybackSpeeds[0]); ++i)
	{
		mpfloat speed = gPlaybackSpeeds[i];
		if (speed == 0)
		{
			sec = inOutMenu->GetNextEmptySection();
		}
		else
		{
			QString str;
			str.sprintf("%.2fx", (float)speed);

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
			SAnimData::EFrameRate currentFramerate = GetTrackViewCore()->GetCurrentFramerate();
			m_framerate = SAnimData::GetFrameRateValue(currentFramerate);
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

void CTrackViewPlaybackControlsToolbar::UpdateTime(const CTimeValue& newTime, bool bForce)
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

	//const int32 ticks = m_lastTime.GetTicks();
	const CTimeValue totalTime = m_lastTime;
	const CTimeValue totalRemainder = m_lastTime % 1;
	const int32 mins = (int32)totalTime.GetSeconds() / 60;
	const int32 secs = (int32)mod(totalTime.GetSeconds(), mpfloat(60));

	switch (m_displayMode)
	{
	case SAnimData::EDisplayMode::Ticks:
		{
			text.sprintf("%010d (%d tps)", (int)(m_lastTime.GetSeconds() * SAnimData::numTicksPerSecond), SAnimData::numTicksPerSecond);
		}
		break;

	case SAnimData::EDisplayMode::Frames:
		{
			const CTimeValue ticksPerFrame = CTimeValue(1) / m_framerate;
			const nTime totalFrames = totalTime / ticksPerFrame;
			text.sprintf("%06d (%d fps)", (int)totalFrames, m_framerate);
		}
		break;

	case SAnimData::EDisplayMode::Time:
		{
			text.sprintf("%02d:%02d:%03d (%d fps)", mins, secs, (float)totalRemainder.GetMilliSeconds(), m_framerate);
		}
		break;

	case SAnimData::EDisplayMode::Timecode:
		{
			const CTimeValue timePerFrame = CTimeValue(1) / m_framerate;
			const nTime leftOverFrames = totalRemainder / (timePerFrame == 0 ? 1 : timePerFrame);

			if (m_framerate < 100)
				text.sprintf("%02d:%02d:%02d (%d fps)", mins, secs, (int)leftOverFrames, m_framerate);
			else if (m_framerate < 1000)
				text.sprintf("%02d:%02d:%03d (%d fps)", mins, secs, (int)leftOverFrames, m_framerate);
			else
				text.sprintf("%02d:%02d:%04d (%d fps)", mins, secs, (int)leftOverFrames, m_framerate);
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

void CTrackViewPlaybackControlsToolbar::OnTimeChanged(const CTimeValue& newTime)
{
	UpdateTime(newTime);
}

