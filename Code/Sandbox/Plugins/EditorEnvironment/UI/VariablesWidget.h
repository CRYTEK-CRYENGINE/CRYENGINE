// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include <QPropertyTreeLegacy/ContextList.h>
#include <QWidget>

class CController;
class QPropertyTreeLegacy;

enum class PlaybackMode;

class CVariablesWidget : public QWidget
{
public:
	CVariablesWidget(CController& controller);
	~CVariablesWidget();

private:
	void OnTreeSelectionChanged();
	void OnSelectedVariableStartChange();
	void OnSelectedVariableEndChange();

	void OnAssetOpened();
	void OnAssetClosed();
	void ResetTree();
	void OnPlaybackModeChanged(PlaybackMode newMode);
	void ProcessUserEventsFromCurveEditor(QEvent* pEvent);

	CController&                m_controller;
	QPropertyTreeLegacy*              m_pPropertyTree;
	Serialization::CContextList m_treeContext;
};
