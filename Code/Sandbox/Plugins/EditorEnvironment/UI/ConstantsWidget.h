// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

enum class PlaybackMode;

class CController;
class QPropertyTreeLegacy;

class CConstantsWidget : public QWidget
{
public:
	CConstantsWidget(CController& controller);
	~CConstantsWidget();
	void OnChanged();

private:
	class CUndoConstPropTreeCommand;

	void OnAssetOpened();
	void OnAssetClosed();

	void OnBeginUndo();
	void OnEndUndo(bool acceptUndo);
	void OnPlaybackModeChanged(PlaybackMode newMode);

	CController&   m_controller;
	QPropertyTreeLegacy* m_pPropertyTree;
};
