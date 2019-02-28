// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QToolBar>
#include "TrackViewCoreComponent.h"

class QActionGroup;

class CTrackViewKeysToolbar : public QToolBar, public CTrackViewCoreComponent
{
	Q_OBJECT

public:
	CTrackViewKeysToolbar(CTrackViewCore* pTrackViewCore);
	~CTrackViewKeysToolbar() {}

protected:
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override;
	virtual const char* GetComponentTitle() const override { return "Keys Toolbar"; }

private slots:
	void OnGoToPreviousKey();
	void OnGoToNextKey();
	void OnSlideKeys();
	void OnScaleKeys();
	void OnSnapModeChanged();
	void OnSyncSelectedTracksToBase();
	void OnSyncSelectedTracksFromBase();

private:
	QActionGroup* m_pSnappingGroup;
	QActionGroup* m_pMoveModeGroup;
};

