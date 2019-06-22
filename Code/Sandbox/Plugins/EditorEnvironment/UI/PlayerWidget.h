// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class CController;
class CTimeEditControl;
class QBoxLayout;
class QLineEdit;

enum class PlaybackMode;

class CPlayerWidget : public QWidget
{
public:
	CPlayerWidget(QWidget* pParent, CController& controller);
	~CPlayerWidget();

	// Set state of all controls with the state taken from the controller
	void ResetState();

private:
	void  CreateControls();
	void  ConnectSignals();

	void  ResetPlaybackEdits();
	void  OnPlaybackParamsChanged();

	float GetTimeAsFloat(CTimeEditControl* pTimeCtrl) const;
	QTime FloatToQTime(float time) const;

	void  OnPlaybackModeChanged(PlaybackMode newMode);
	void  OnCurrentTimeChanged(QWidget* pSender, float newTime);
	void  CurrentTimeEdited();

	CController&      m_controller;

	CTimeEditControl* m_pStartTimeEdit;
	CTimeEditControl* m_pEndTimeEdit;
	QLineEdit*        m_pPlaySpeedEdit;

	CTimeEditControl* m_pCurrentTimeEdit;

	QToolButton*      m_pStopButton;
	QToolButton*      m_pPlayButton;
};