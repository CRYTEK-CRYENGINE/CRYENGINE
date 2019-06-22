// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorFramework/Editor.h"

#include <CryExtension/CryGUID.h>
#include <QtViewPane.h>

#include "TrackViewCore.h"

class CTrackViewCore;

class CTrackViewWindow : public CDockableEditor
{
	Q_OBJECT

public:
	CTrackViewWindow(QWidget* pParent = nullptr);
	~CTrackViewWindow();

	static std::vector<CTrackViewWindow*> GetTrackViewWindows() { return ms_trackViewWindows; }

	// CEditor
	virtual const char* GetEditorName() const override { return "Track View"; }

	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;

	virtual void        customEvent(QEvent* pEvent) override;
	// ~CEditor
	
private:
	void         RegisterActions();
	void         InitMenu();

	bool OnNew();
	bool OnOpen();
	bool OnClose();
	bool OnUndo();
	bool OnRedo();
	bool OnCopy();
	bool OnCut();
	bool OnPaste();
	bool OnDelete();
	bool OnDuplicate();
	bool OnZoomIn();
	bool OnZoomOut();

	bool         OnExportSequence();
	bool         OnImportSequence();
	bool         OnUnitsChanged(SAnimTime::EDisplayMode mode);
	bool         OnRender();
	bool         OnDeleteSequence();
	bool         OnSequenceProperties();
	bool         OnNewEvent();
	bool         OnShowAllEvents();
	bool         OnLightAnimationSet();
	bool         OnToggleShowDopesheet();
	bool         OnToggleShowCurveEditor();
	bool         OnToggleLinkTimelines();
	bool         OnToggleSyncSelection();
	bool         OnSetUnitsTicks();
	bool         OnSetUnitsTime();
	bool         OnSetUnitsFramecode();
	bool         OnSetUnitsFrames();
	bool         OnGoToStart();
	bool         OnGoToEnd();
	bool         OnPlayPause();
	bool         OnStop();
	bool         OnRecord();
	bool         OnGoToNextKey();
	bool         OnGoToPrevKey();
	bool         OnToggleLoop();
	bool         OnSetPlaybackStart();
	bool         OnSetPlaybackEnd();
	bool         OnResetPlaybackStartEnd();
	bool         OnSyncSelectedTracksToBasePosition();
	bool         OnSyncSelectedTracksFromBasePosition();
	bool         OnNoSnap();
	bool         OnMagnetSnap();
	bool         OnFrameSnap();
	bool         OnGridSnap();
	bool         OnDeleteSelectedTracks();
	bool         OnDisableSelectedTracks();
	bool         OnMuteSelectedTracks();
	bool         OnEnableSelectedTracks();
	bool         OnSelectMoveKeysTool();
	bool         OnSelectSlideKeysTool();
	bool         OnSelectScaleKeysTools();
	bool         OnSetTangentAuto();
	bool         OnSetTangentInZero();
	bool         OnSetTangentInStep();
	bool         OnSetTangentInLinear();
	bool         OnSetTangentOutZero();
	bool         OnSetTangentOutStep();
	bool         OnSetTangentOutLinear();
	bool         OnBreakTangents();
	bool         OnUnifyTangents();
	bool         OnFitViewHorizontal();
	bool         OnFitViewVertical();
	bool         OnAddTrackPosition();
	bool         OnAddTrackRotation();
	bool         OnAddTrackScale();
	bool         OnAddTrackVisibility();
	bool         OnAddTrackAnimation();
	bool         OnAddTrackMannequin();
	bool         OnAddTrackNoise();
	bool         OnAddTrackAudioFile();
	bool         OnAddTrackAudioParameter();
	bool         OnAddTrackAudioSwitch();
	bool         OnAddTrackAudioTrigger();
	bool         OnAddTrackDRSSignal();
	bool         OnAddTrackEvent();
	bool         OnAddTrackExpression();
	bool         OnAddTrackFacialSequence();
	bool         OnAddTrackLookAt();
	bool         OnAddTrackPhysicalize();
	bool         OnAddTrackPhysicsDriven();
	bool         OnAddTrackProceduralEyes();

	virtual void keyPressEvent(QKeyEvent* e) override;

	CTrackViewCore*                       m_pTrackViewCore;
	static std::vector<CTrackViewWindow*> ms_trackViewWindows;
};
