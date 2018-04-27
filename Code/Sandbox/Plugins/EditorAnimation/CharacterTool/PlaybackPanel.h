// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <vector>
#include <memory>
#include <CrySerialization/Forward.h>
#include <CryMovie/AnimTime.h>
#include "CryIcon.h"

class QPushButton;
class QLineEdit;
class QDoubleSpinBox;
class QLabel;
class CTimeline;

class QMenuComboBox;
struct STimelineContent;

namespace Explorer
{
struct ExplorerEntryModifyEvent;
}

namespace CharacterTool
{
using std::vector;
using std::unique_ptr;

struct AnimEventPreset;
struct System;
class AnimEventPresetPanel;

enum ETimeUnits
{
	TIME_IN_SECONDS,
	TIME_IN_FRAMES,
	TIME_NORMALIZED
};

class PlaybackPanel : public QWidget
{
	Q_OBJECT
public:
	PlaybackPanel(QWidget* parent, System* system, AnimEventPresetPanel* presetPalette);
	virtual ~PlaybackPanel();

	void Serialize(Serialization::IArchive& ar);

	bool HandleKeyEvent(int key);

public slots:
	void PutAnimEvent(const AnimEventPreset& preset);
	void OnPresetsChanged();

protected slots:
	void      OnTimelineScrub(bool scrubThrough);
	void      OnTimelineChanged(bool continuousChange);
	void      OnTimelineSelectionChanged(bool continuousChange);
	void      OnTimelineHotkey(int number);
	void      OnTimelineUndo();
	void      OnTimelineRedo();
	void      OnPlaybackTimeChanged();
	void      OnPlaybackStateChanged();
	void      OnPlaybackOptionsChanged();
	void      OnExplorerEntryModified(Explorer::ExplorerEntryModifyEvent& ev);
	void      OnDocumentActiveAnimationSwitched();

	void      OnPlayPausePushed();
	void      OnTimeEditEditingFinished();
	void      OnTimeEditValueChanged(double newTime);
	void      OnTimeUnitsChanged(int index);
	void      OnTimelinePlay();
	void      OnSpeedChanged(int index);
	void      OnLoopToggled(bool);
	void      OnOptionRestartOnSelection();
	void      OnOptionWrapSlider();
	void      OnOptionSmoothTimelineSlider();
	void      OnOptionFirstFrameAtEnd();
	void      OnOptionPlayFromTheStart();
	void      OnSubselectionChanged(int layer);

	void      OnEventsImport();
	void      OnEventsExport();
private:
	void      ResetTimelineZoom();
	void      WriteTimeline();
	void      ReadTimeline();

	void      UpdateTimeUnitsUI(bool timeChanged, bool durationChanged);
	float     FrameRate() const;

	SAnimTime AnimEventTimeToTimelineTime(float animEventTime) const;
	float     TimelineTimeToAnimEventTime(SAnimTime timelineTime) const;

	CTimeline*                   m_timeline;
	AnimEventPresetPanel*        m_presetPanel;
	QDoubleSpinBox*              m_timeEdit;
	bool                         m_timeEditChanging;
	QLabel*                      m_timeTotalLabel;
	QPushButton*                 m_playPauseButton;
	QMenuComboBox*               m_timeUnitsCombo;
	QMenuComboBox*               m_speedCombo;
	ETimeUnits                   m_timeUnits;
	QPushButton*                 m_loopButton;
	QPushButton*                 m_optionsButton;
	QAction*                     m_actionRestartOnSelection;
	QAction*                     m_actionWrapSlider;
	QAction*                     m_actionSmoothTimelineSlider;
	QAction*                     m_actionFirstFrameAtEnd;
	QAction*                     m_actionPlayFromTheStart;
	vector<QWidget*>             m_activeControls;
	System*                      m_system;
	unique_ptr<STimelineContent> m_timelineContent;
	vector<int>                  m_selectedEvents;

	QString                      m_sAnimationProgressStatusReason;
	QString                      m_sAnimationName;
	QString                      m_sAnimationIDName;
	bool                         m_playing;
	CryIcon                      m_playIcon;
	CryIcon                      m_pauseIcon;

	float                        m_normalizedTime;
	float                        m_duration;
};

}

