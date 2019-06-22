// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CurveEditorWidget.h"

#include "EnvironmentEditor.h"
#include "PlayerWidget.h"

#include <CurveEditor.h>

#include <QBoxLayout>
#include <QKeyEvent>
#include <QToolBar>

namespace Private_CurveEditorTab
{
const int gRulerHeight = 30;
const int gRulerGradientHeight = 14;

QRgb ColorLinearToGamma(const Vec3& col)
{
	float r = clamp_tpl(col.x, 0.0f, 1.0f);
	float g = clamp_tpl(col.y, 0.0f, 1.0f);
	float b = clamp_tpl(col.z, 0.0f, 1.0f);

	r = (float)(r <= 0.0031308 ? (12.92 * r) : (1.055 * pow((double)r, 1.0 / 2.4) - 0.055));
	g = (float)(g <= 0.0031308 ? (12.92 * g) : (1.055 * pow((double)g, 1.0 / 2.4) - 0.055));
	b = (float)(b <= 0.0031308 ? (12.92 * b) : (1.055 * pow((double)b, 1.0 / 2.4) - 0.055));

	r *= 255.0f;
	g *= 255.0f;
	b *= 255.0f;

	return qRgb(int(r), int(g), int(b));
}

}

CCurveEditorWidget::CCurveEditorWidget(CController& controller)
	: m_controller(controller)
	, m_animTimeSecondsIn24h(controller.GetEditor().GetTimeOfDay()->GetAnimTimeSecondsIn24h())
	, m_pCurveEdit(nullptr)
{
	CreateControls();
	ConnectSignals();
	SetEngineTime();
	ShowControls(controller.GetEditor().GetPreset() != nullptr);
}

CCurveEditorWidget::~CCurveEditorWidget()
{
	m_controller.signalAssetOpened.DisconnectObject(this);
	m_controller.signalAssetClosed.DisconnectObject(this);
	m_controller.signalNewVariableSelected.DisconnectObject(this);
	m_controller.signalCurveContentChanged.DisconnectObject(this);
	m_controller.signalCurrentTimeChanged.DisconnectObject(this);
	m_controller.signalPlaybackModeChanged.DisconnectObject(this);
}

void CCurveEditorWidget::CreateControls()
{
	QBoxLayout* pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(pMainLayout);

	m_pCurveEditToolbar = new QToolBar(this);
	pMainLayout->addWidget(m_pCurveEditToolbar, Qt::AlignTop);

	m_pCurveEdit = new CCurveEditor(this);
	m_pCurveEdit->FillWithCurveToolsAndConnect(m_pCurveEditToolbar);
	m_pCurveEdit->SetContent(&m_controller.GetCurveContent());
	m_pCurveEdit->SetTimeRange(SAnimTime(0.0f), SAnimTime(m_animTimeSecondsIn24h));
	m_pCurveEdit->SetRulerVisible(true);
	m_pCurveEdit->SetRulerHeight(Private_CurveEditorTab::gRulerHeight);
	m_pCurveEdit->SetRulerTicksYOffset(Private_CurveEditorTab::gRulerGradientHeight);
	m_pCurveEdit->SetHandlesVisible(false);
	m_pCurveEdit->SetGridVisible(true);
	m_pCurveEdit->OnFitCurvesHorizontally();
	m_pCurveEdit->OnFitCurvesVertically();
	m_pCurveEdit->installEventFilter(this);
	m_pCurveEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	pMainLayout->addWidget(m_pCurveEdit, Qt::AlignTop);

	CPlayerWidget* pPlayer = new CPlayerWidget(this, m_controller);
	pMainLayout->addWidget(pPlayer, 0, Qt::AlignBottom);

	// Additional commands to toolbar
	m_pCurveEditToolbar->addSeparator();

	m_pCurveEditToolbar->addAction(CryIcon("icons:CurveEditor/Copy_Curve.ico"), "Copy curve content", [&]() { m_controller.CopySelectedCurveToClipboard(); });
	m_pCurveEditToolbar->addAction(CryIcon("icons:CurveEditor/Paste_Curve.ico"), "Paste curve content", [&]() { m_controller.PasteCurveContentFromClipboard(); });
}

void CCurveEditorWidget::ConnectSignals()
{
	connect(m_pCurveEdit, &CCurveEditor::SignalDrawRulerBackground, this, &CCurveEditorWidget::DrawGradientBar);
	connect(m_pCurveEdit, &CCurveEditor::SignalScrub, this, &CCurveEditorWidget::CurveEditTimeChanged);

	connect(m_pCurveEdit, &CCurveEditor::SignalContentAboutToBeChanged, [&]() { m_controller.OnSelectedVariableStartChange(); });
	connect(m_pCurveEdit, &CCurveEditor::SignalContentChanging, this, &CCurveEditorWidget::OnCurveDragging);
	connect(m_pCurveEdit, &CCurveEditor::SignalContentChanged, [&]() { m_controller.OnCurveEditorEndChange(); });

	m_controller.signalAssetOpened.Connect(this, &CCurveEditorWidget::OnAssetOpened);
	m_controller.signalAssetClosed.Connect(this, &CCurveEditorWidget::OnAssetClosed);
	m_controller.signalNewVariableSelected.Connect(this, &CCurveEditorWidget::OnNewVariableSelected);
	m_controller.signalCurveContentChanged.Connect(this, &CCurveEditorWidget::UpdateCurveContent);
	m_controller.signalCurrentTimeChanged.Connect(this, &CCurveEditorWidget::OnTimeChanged);
	m_controller.signalPlaybackModeChanged.Connect(this, &CCurveEditorWidget::OnPlaybackModeChanged);
}

void CCurveEditorWidget::ShowControls(bool show)
{
	m_pCurveEdit->setVisible(show);
	m_pCurveEditToolbar->setVisible(show);
}

void CCurveEditorWidget::OnAssetOpened()
{
	ShowControls(true);
	UpdateCurveContent();
}

void CCurveEditorWidget::OnAssetClosed()
{
	ShowControls(false);
}

void CCurveEditorWidget::CurveEditTimeChanged()
{
	float time = m_pCurveEdit->Time().ToFloat();
	time /= m_animTimeSecondsIn24h;
	const float todTime = time * 24.0f;

	m_controller.SetCurrentTime(this, todTime);
}

void CCurveEditorWidget::OnCurveDragging()
{
	if (m_controller.GetPlaybackMode() != PlaybackMode::Edit)
	{
		return;
	}

	// User modifies curve. Need to reset time if curve point is moving along time axis
	float minSelectedTime = std::numeric_limits<float>::max();
	for (const auto& curve : m_controller.GetCurveContent().m_curves)
	{
		for (auto& key : curve.m_keys)
		{
			if (key.m_bSelected)
				minSelectedTime = std::min(minSelectedTime, key.m_time.ToFloat());
		}
	}

	float todTime = minSelectedTime / m_animTimeSecondsIn24h * 24.0f;
	if (todTime < 0.0f)
	{
		todTime = 0.0f;
	}
	m_controller.SetCurrentTime(nullptr, todTime);

	// Time is set. Process curve change
	m_controller.OnCurveDragging();
}

void CCurveEditorWidget::UpdateCurveContent()
{
	blockSignals(true);
	m_pCurveEdit->update();
	blockSignals(false);
}

void CCurveEditorWidget::DrawGradientBar(QPainter& painter, const QRect& rect, const Range& visibleRange)
{
	ITimeOfDay::IPreset* pPreset = m_controller.GetEditor().GetPreset();
	if (!pPreset)
	{
		return;
	}

	const int selectedParam = m_controller.GetSelectedVariableIndex();
	ITimeOfDay::IVariables& variables = pPreset->GetVariables();
	if (selectedParam < 0 || selectedParam >= variables.GetVariableCount())
	{
		return;
	}

	ITimeOfDay::SVariableInfo sVarInfo;
	if (variables.GetVariableInfo(selectedParam, sVarInfo))
	{
		const int xMin = rect.left();
		const int xMax = rect.right();
		const int nRange = xMax - xMin;
		const float fRange = float(nRange);

		QPoint from = QPoint(xMin, rect.bottom() - Private_CurveEditorTab::gRulerGradientHeight + 1);
		QPoint to = QPoint(xMin, rect.bottom() + 1);

		std::vector<Vec3> gradient;
		gradient.resize(nRange);
		variables.InterpolateVarInRange(selectedParam, visibleRange.start, visibleRange.end, nRange, &gradient[0]);

		if (ITimeOfDay::TYPE_COLOR == sVarInfo.type)
		{
			for (size_t i = 0; i < nRange; ++i)
			{
				const Vec3& val = gradient[i];
				QColor color(Private_CurveEditorTab::ColorLinearToGamma(val));
				painter.setPen(color);
				from.setX(i + xMin);
				to.setX(i + xMin);
				painter.drawLine(from, to);
			}
		}
		else
		{
			for (size_t i = 0; i < nRange; ++i)
			{
				const Vec3& val = gradient[i];
				const float value = clamp_tpl(val.x, sVarInfo.fValue[1], sVarInfo.fValue[2]);
				const float normVal = (value - sVarInfo.fValue[1]) / (sVarInfo.fValue[2] - sVarInfo.fValue[1]);
				const int grayScaled = int(normVal * 255.0f);
				QColor color(grayScaled, grayScaled, grayScaled);
				painter.setPen(color);
				from.setX(i + xMin);
				to.setX(i + xMin);
				painter.drawLine(from, to);
			}
		}
	}
}

void CCurveEditorWidget::OnNewVariableSelected()
{
	if (m_controller.GetSelectedVariableIndex() < 0)
	{
		m_pCurveEdit->SetFitMargin(0.0f);
		m_pCurveEdit->ZoomToTimeRange(SAnimTime(0.0f), SAnimTime(m_animTimeSecondsIn24h));
	}
	else
	{
		m_pCurveEdit->OnFitCurvesVertically();
		m_pCurveEdit->OnFitCurvesHorizontally();
	}

	UpdateCurveContent();
}

void CCurveEditorWidget::OnTimeChanged(QWidget* pSender, float newTime)
{
	if (pSender == this)
	{
		return;
	}

	blockSignals(true);

	const float splineTime = (newTime / 24.0f) * m_animTimeSecondsIn24h;
	m_pCurveEdit->SetTime(SAnimTime(splineTime));

	blockSignals(false);
}

void CCurveEditorWidget::SetEngineTime()
{
	blockSignals(true);

	float currTime = m_controller.GetCurrentTime();
	const float splineTime = (currTime / 24.0f) * m_animTimeSecondsIn24h;
	m_pCurveEdit->SetTime(SAnimTime(splineTime));

	blockSignals(false);
}

void CCurveEditorWidget::OnPlaybackModeChanged(PlaybackMode newMode)
{
	// Disable only toolbar: user should be able to see moving scribble, and also able to set time via scribble
	m_pCurveEditToolbar->setEnabled(newMode == PlaybackMode::Edit);
}

bool CCurveEditorWidget::eventFilter(QObject* pObj, QEvent* pEvent)
{
	if (pObj == m_pCurveEdit && pEvent->type() == QEvent::KeyPress)
	{
		// Forward some events to VarPropertyTree, when its exists and not focused
		const int key = static_cast<QKeyEvent*>(pEvent)->key();
		const auto modifiers = static_cast<QKeyEvent*>(pEvent)->modifiers();

		if ((key == Qt::Key_Up) || (key == Qt::Key_Down) || (key == Qt::Key_Left) || (key == Qt::Key_Right) ||
		    (key == Qt::Key_F && modifiers == Qt::CTRL) ||
		    (key == Qt::Key_Enter) || (key == Qt::Key_Return))
		{
			m_controller.signalHandleKeyEventsInVarPropertyTree(pEvent);
			return true;
		}
	}

	return QObject::eventFilter(pObj, pEvent);
}
