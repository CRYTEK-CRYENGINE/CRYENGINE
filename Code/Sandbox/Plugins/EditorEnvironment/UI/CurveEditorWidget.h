// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryMath/Range.h>

#include <QWidget>

class CController;
class CCurveEditor;

enum class PlaybackMode;

class CCurveEditorWidget : public QWidget
{
public:
	CCurveEditorWidget(CController& controller);
	~CCurveEditorWidget();

private:
	// QWidget
	virtual bool eventFilter(QObject* pObj, QEvent* pEvent) override;

	void         CreateControls();
	void         ConnectSignals();

	void         OnAssetOpened();
	void         OnAssetClosed();

	void         OnCurveDragging();

	void         CurveEditTimeChanged();

	void         ShowControls(bool show);

	void         DrawGradientBar(QPainter& painter, const QRect& rect, const Range& visibleRange);

	void         OnNewVariableSelected();
	void         OnPlaybackModeChanged(PlaybackMode newMode);
	void         OnTimeChanged(QWidget* pSender, float newTime);

	void         UpdateCurveContent();

	void         SetEngineTime();

	CController&  m_controller;
	const float   m_animTimeSecondsIn24h;
	CCurveEditor* m_pCurveEdit;
	QToolBar*     m_pCurveEditToolbar;
};
