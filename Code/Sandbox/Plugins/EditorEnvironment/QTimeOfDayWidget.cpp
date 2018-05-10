// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <QApplication>
#include <QShortcut>
#include <QPainter>
#include <QKeyEvent>
#include <QFileDialog>
#include <QLabel>
#include <QSlider>
#include <QSplitter>
#include <QGroupBox>
#include <QLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QTimeEdit>
#include <QLineEdit>
#include <QToolBar>
#include <QToolButton>
#include <QBrush>
#include <QStyle>
#include <QClipboard>
#include <QMimeData>

#include "QTimeOfDayWidget.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>

#include "IUndoObject.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/IArchiveHost.h>

#include "QViewport.h"

#include "QPropertyTree/ContextList.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "CurveEditor.h"
#include "TimeEditControl.h"
#include <CryIcon.h>
#include <EditorStyleHelper.h>
#include "CryMath/Bezier_impl.h"

#include <Preferences/LightingPreferences.h>

SERIALIZATION_ENUM_BEGIN(ETODParamType, "ETodParamType")
SERIALIZATION_ENUM(eFloatType, "eFloatType", "eFloatType")
SERIALIZATION_ENUM(eColorType, "eColorType", "eColorType")
SERIALIZATION_ENUM_END()

namespace
{
const float nTimeLineMinutesScale = 60.0f;
const int nRulerHeight = 30;
const int nRulerGradientHeight = 14;
const float fToDParameterComareEpsilon = 0.0001f;
const char* sClipboardCurveContentMimeTypeFloat = "binary/curveeditorcontentfloat";
const char* sClipboardCurveContentMimeTypeColor = "binary/curveeditorcontentcolor";

ITimeOfDay* GetTimeOfDay()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	assert(pTimeOfDay);
	return pTimeOfDay;
}

float QTimeToFloat(const QTime& time)
{
	const int nSeconds = QTime(0, 0, 0).secsTo(time);
	const float fTime = nSeconds / 3600.0f;
	return fTime;
}

QTime FloatToQTime(float fTime)
{
	unsigned int hour = floor(fTime);
	unsigned int minute = floor((fTime - floor(fTime)) * nTimeLineMinutesScale + 0.5f);
	if (hour > 23)
	{
		hour = 23;
		minute = 59;
	}
	return QTime(hour, minute);
}

void SetQTimeEditTimeBlocking(QTimeEdit* timeEdit, QTime time)
{
	timeEdit->blockSignals(true);
	timeEdit->setTime(time);
	timeEdit->blockSignals(false);
}

void SetQTimeEditTimeBlocking(CTimeEditControl* timeEdit, QTime time)
{
	timeEdit->blockSignals(true);
	timeEdit->setTime(time);
	timeEdit->blockSignals(false);
}

void UpdateToDAdvancedInfo(std::function<void (ITimeOfDay::SAdvancedInfo& sAdvInfo)> updateFunc)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		ITimeOfDay::SAdvancedInfo sAdvInfo;
		pTimeOfDay->GetAdvancedInfo(sAdvInfo);
		updateFunc(sAdvInfo);
		pTimeOfDay->SetAdvancedInfo(sAdvInfo);
		pTimeOfDay->Update(false, true);
		GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
	}
}

void UpdateCurveContentFromToDVar(int nVarId, int nSplineId, SCurveEditorContent* pContent)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();

	if (nSplineId >= 0 && nSplineId < pContent->m_curves.size())
	{
		std::vector<SCurveEditorKey>& splineKeys = pContent->m_curves[nSplineId].m_keys;
		splineKeys.clear();

		const uint nKeysNum = pTimeOfDay->GetSplineKeysCount(nVarId, nSplineId);
		if (nKeysNum)
		{
			std::vector<SBezierKey> keys;
			keys.resize(nKeysNum);
			pTimeOfDay->GetSplineKeysForVar(nVarId, nSplineId, &keys[0], nKeysNum);

			splineKeys.reserve(nKeysNum);
			for (auto& key : keys)
			{
				SCurveEditorKey newKey;
				newKey.m_time = key.m_time;
				newKey.m_controlPoint = key.m_controlPoint;
				splineKeys.push_back(newKey);
			}
		}
	}
}

void UpdateToDVarFromCurveContent(int nVarId, int nSplineId, const SCurveEditorContent* pContent)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();

	if (nSplineId >= 0 && nSplineId < pContent->m_curves.size())
	{
		const SCurveEditorCurve& curve = pContent->m_curves[nSplineId];
		std::vector<SBezierKey> keys;
		keys.reserve(curve.m_keys.size());
		for (auto& key : curve.m_keys)
		{
			if (!key.m_bDeleted)
			{
				SBezierKey newKey;
				newKey.m_time = key.m_time;
				newKey.m_controlPoint = key.m_controlPoint;
				keys.push_back(newKey);
			}
		}

		pTimeOfDay->SetSplineKeysForVar(nVarId, nSplineId, &keys[0], keys.size());
	}
}

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

void DrawGradient(ITimeOfDay* pTimeOfDay, int selectedParam, QPainter& painter, const QRect& rect, const Range& visibleRange)
{
	if (selectedParam >= 0 && selectedParam < pTimeOfDay->GetVariableCount())
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (pTimeOfDay->GetVariableInfo(selectedParam, sVarInfo))
		{
			const int xMin = rect.left();
			const int xMax = rect.right();
			const int nRange = xMax - xMin;
			const float fRange = float(nRange);

			QPoint from = QPoint(xMin, rect.bottom() - nRulerGradientHeight + 1);
			QPoint to = QPoint(xMin, rect.bottom() + 1);

			std::vector<Vec3> gradient;
			gradient.resize(nRange);
			pTimeOfDay->InterpolateVarInRange(selectedParam, visibleRange.start, visibleRange.end, nRange, &gradient[0]);

			if (ITimeOfDay::TYPE_COLOR == sVarInfo.type)
			{
				for (size_t i = 0; i < nRange; ++i)
				{
					const Vec3& val = gradient[i];
					QColor color(ColorLinearToGamma(val));
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
}
}

//////////////////////////////////////////////////////////////////////////
STODParameter::STODParameter() : m_value(ZERO), m_ID(0), m_Type(eFloatType), m_pGroup(NULL), m_IDWithinGroup(-1)
{
	m_ParamName = "DefaultName";
	m_LabelName = "DefaultLabel";
	m_GroupName = "DefaultGroup";
}

bool Serialize(Serialization::IArchive& ar, STODParameter& param, const char* name, const char* label)
{
	Vec3 value = param.GetValue();

	if (param.GetTODParamType() == ITimeOfDay::TYPE_COLOR)
	{
		if (!ar(Serialization::Vec3AsColor(value), name, label))
			return false;
	}
	else if (param.GetTODParamType() == ITimeOfDay::TYPE_FLOAT)
	{
		if (value.y < value.z)
		{
			if (!ar(Serialization::Range(value.x, value.y, value.z), name, label))
				return false;
		}
		else
		{
			if (!ar(value.x, name, label))
				return false;
		}
	}

	if (ar.isInput())
	{
		if (QTimeOfDayWidget* window = ar.context<QTimeOfDayWidget>())
		{
			window->CheckParameterChanged(param, value);
		}
	}
	return true;
}

void STODParameterGroup::Serialize(IArchive& ar)
{
	for (size_t i = 0, nParamCount = m_Params.size(); i < nParamCount; ++i)
	{
		ar(m_Params[i], m_Params[i].GetName(), m_Params[i].GetLabel());
	}
}

void STODParameterGroupSet::Serialize(IArchive& ar)
{
	for (TPropertyGroupMap::iterator it = m_propertyGroups.begin(); it != m_propertyGroups.end(); ++it)
	{
		STODParameterGroup& group = *it;
		const char* name = group.m_name.c_str();
		ar(group, name, name);
	}
}

//////////////////////////////////////////////////////////////////////////

class QTimeOfDayWidget::CContentChangedUndoCommand : public IUndoObject
{
public:
	CContentChangedUndoCommand(QTimeOfDayWidget* pWidget)
	{
		m_paramId = pWidget->m_selectedParamId;
		gLightingPreferences.bForceSkyUpdate = pWidget->m_forceSkyUpdateBtn->isChecked();
		SCurveEditorContent* pContent = pWidget->m_curveContent.get();
		Serialization::SaveBinaryBuffer(m_oldState, *pContent);
	}

	virtual int         GetSize()        { return sizeof(*this) + m_oldState.size() + m_newState.size(); }
	virtual const char* GetDescription() { return "QTimeOfDayWidget content changed"; }

	void                SaveNewState(QTimeOfDayWidget* pWidget)
	{
		SCurveEditorContent* pContent = pWidget->m_curveContent.get();
		Serialization::SaveBinaryBuffer(m_newState, *pContent);
	}

	virtual void Undo(bool bUndo)
	{
		Update(m_oldState);
	}

	virtual void Redo()
	{
		Update(m_newState);
	}

	int GetParamId() const { return m_paramId; }
protected:

	void Update(DynArray<char>& array)
	{
		SCurveEditorContent tempContent;
		Serialization::LoadBinaryBuffer(tempContent, array.begin(), array.size());

		const SCurveEditorContent* pContent = &tempContent;
		const int paramID = m_paramId;
		if (paramID >= 0)
		{
			UpdateToDVarFromCurveContent(paramID, 0, pContent);
			UpdateToDVarFromCurveContent(paramID, 1, pContent);
			UpdateToDVarFromCurveContent(paramID, 2, pContent);

			ITimeOfDay* pTimeOfDay = GetTimeOfDay();
			pTimeOfDay->Update(true, gLightingPreferences.bForceSkyUpdate);
			GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
		}
	}

	DynArray<char> m_oldState;
	DynArray<char> m_newState;
	int            m_paramId;
};

//////////////////////////////////////////////////////////////////////////

QTimeOfDayWidget::QTimeOfDayWidget() : m_pContextList(new Serialization::CContextList()),
	m_propertyTree(NULL), m_forceSkyUpdateBtn(NULL),
	m_currentTimeEdit(NULL), m_startTimeEdit(NULL), m_endTimeEdit(NULL), m_playSpeedEdit(NULL), //m_playSpeedSpinBox(NULL),
	m_curveEdit(NULL), m_curveContent(new SCurveEditorContent), m_selectedParamId(-1), m_pUndoCommand(NULL),
	m_fAnimTimeSecondsIn24h(0.0), m_bIsEditing(false)
{
	m_pContextList->Update<QTimeOfDayWidget>(this);

	m_bIsPlaying = false;

	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	m_fAnimTimeSecondsIn24h = pTimeOfDay->GetAnimTimeSecondsIn24h();

	CreateUi();
	LoadPropertiesTree();

	Refresh();

	GetIEditor()->RegisterNotifyListener(this);
}

//---------------------------------------------
QTimeOfDayWidget::~QTimeOfDayWidget()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void QTimeOfDayWidget::Refresh()
{
	UpdateValues();
	UpdateCurveContent();

	if (m_selectedParamId < 0)
	{
		m_curveEdit->SetFitMargin(0.0f);
		m_curveEdit->ZoomToTimeRange(SAnimTime(0.0f), SAnimTime(m_fAnimTimeSecondsIn24h));

	}
	else
	{
		m_curveEdit->OnFitCurvesVertically();
		m_curveEdit->OnFitCurvesHorizontally();
	}
}

void QTimeOfDayWidget::OnIdleUpdate()
{
	if (m_bIsPlaying)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		float fHour = pTimeOfDay->GetTime();

		ITimeOfDay::SAdvancedInfo advInfo;
		pTimeOfDay->GetAdvancedInfo(advInfo);
		float dt = gEnv->pTimer->GetFrameTime();
		float fTime = fHour + dt * advInfo.fAnimSpeed;
		if (fTime > advInfo.fEndTime)
			fTime = advInfo.fStartTime;
		if (fTime < advInfo.fStartTime)
			fTime = advInfo.fEndTime;

		SetTODTime(fTime);
		UpdateValues();
	}
}

void QTimeOfDayWidget::UpdateCurveContent()
{
	const QColor& textColor = GetStyleHelper()->textColor();

	if (m_selectedParamId >= 0)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		const int paramID = m_selectedParamId;
		ITimeOfDay::SVariableInfo sVarInfo;
		pTimeOfDay->GetVariableInfo(m_selectedParamId, sVarInfo);

		if (ITimeOfDay::TYPE_FLOAT == sVarInfo.type)
		{
			m_curveContent->m_curves.resize(1);
			m_curveContent->m_curves[0].m_color.set(textColor.red(), textColor.green(), textColor.blue(), 255);
			m_curveEdit->SetValueRange(sVarInfo.fValue[1], sVarInfo.fValue[2]);
		}
		else if (ITimeOfDay::TYPE_COLOR == sVarInfo.type)
		{
			m_curveContent->m_curves.resize(3);
			m_curveContent->m_curves[0].m_color.set(190, 20, 20, 255); // r
			m_curveContent->m_curves[1].m_color.set(20, 190, 20, 255); // g
			m_curveContent->m_curves[2].m_color.set(20, 20, 190, 255); // b
			m_curveEdit->SetValueRange(0.0f, 1.0f);
		}

		UpdateCurveContentFromToDVar(paramID, 0, m_curveContent.get());
		UpdateCurveContentFromToDVar(paramID, 1, m_curveContent.get());
		UpdateCurveContentFromToDVar(paramID, 2, m_curveContent.get());
	}
	else
	{
		m_curveEdit->SetValueRange(0.0f, 1.0f);
		m_curveContent->m_curves.resize(0);
	}
}

void QTimeOfDayWidget::CurveEditTimeChanged()
{
	float time = m_curveEdit->Time().ToFloat();
	time /= m_fAnimTimeSecondsIn24h;
	float fTODTime = time * 24.0f;
	SetTODTime(fTODTime);
	UpdateCurrentTimeEdit();
	UpdateProperties();
}

void QTimeOfDayWidget::CurrentTimeEdited()
{
	const float fTODTime = QTimeToFloat(m_currentTimeEdit->time());
	SetTODTime(fTODTime);
	UpdateCurveTime();
	UpdateProperties();
}

void QTimeOfDayWidget::OnForceSkyUpdateChk()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	gLightingPreferences.bForceSkyUpdate = m_forceSkyUpdateBtn->isChecked();
	pTimeOfDay->SetTime(pTimeOfDay->GetTime(), gLightingPreferences.bForceSkyUpdate);
	pTimeOfDay->Update(false, gLightingPreferences.bForceSkyUpdate);
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
}

void QTimeOfDayWidget::OnPropertySelected()
{
	const int oldSelectedParam = m_selectedParamId;

	UpdateSelectedParamId();

	if (m_selectedParamId < 0)
	{
		UpdateCurveContent();
		m_curveEdit->ZoomToTimeRange(SAnimTime(0.0f), SAnimTime(m_fAnimTimeSecondsIn24h));
		return;
	}

	if (oldSelectedParam != m_selectedParamId)
	{
		UpdateCurveContent();
		m_curveEdit->OnFitCurvesVertically();
		m_curveEdit->OnFitCurvesHorizontally();
	}
}

void QTimeOfDayWidget::UndoBegin()
{
	if (m_curveContent && !CUndo::IsRecording())
	{
		if (!m_pUndoCommand)//signalPushUndo might be called twice from QPropertyTree when editing the property
		{
			GetIEditor()->GetIUndoManager()->Begin();
			m_pUndoCommand = new CContentChangedUndoCommand(this);
		}
	}
}

void QTimeOfDayWidget::UndoEnd()
{
	SignalContentChanged();
	if (m_pUndoCommand)
	{
		m_pUndoCommand->SaveNewState(this);
		const int paramId = m_pUndoCommand->GetParamId();
		if (paramId >= 0)
		{
			const STODParameter* pParam = m_groups.m_params[paramId];
			const char* paramName = pParam->GetName();

			GetIEditor()->GetIUndoManager()->RecordUndo(m_pUndoCommand);
			m_pUndoCommand = NULL;

			string undoDesc;
			undoDesc.Format("EnvironmentEditor: Curve content changed for '%s'", paramName);
			GetIEditor()->GetIUndoManager()->Accept(undoDesc);
		}
		else
		{
			GetIEditor()->GetIUndoManager()->Cancel();
		}
	}
}

void QTimeOfDayWidget::CheckParameterChanged(STODParameter& param, const Vec3& newValue)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		const int paramID = param.GetParamID();
		const float fTime = pTimeOfDay->GetTime();
		const float fSplineTime = (fTime / 24.0f) * m_fAnimTimeSecondsIn24h;

		bool bChanged = false;
		const Vec3 oldValue = param.GetValue();
		if (fabs(oldValue.x - newValue.x) > fToDParameterComareEpsilon)
		{
			bChanged = true;
			pTimeOfDay->UpdateSplineKeyForVar(paramID, 0, fSplineTime, newValue.x);
		}
		if (fabs(oldValue.y - newValue.y) > fToDParameterComareEpsilon)
		{
			bChanged = true;
			pTimeOfDay->UpdateSplineKeyForVar(paramID, 1, fSplineTime, newValue.y);
		}
		if (fabs(oldValue.z - newValue.z) > fToDParameterComareEpsilon)
		{
			bChanged = true;
			pTimeOfDay->UpdateSplineKeyForVar(paramID, 2, fSplineTime, newValue.z);
		}

		if (bChanged)
		{
			param.SetValue(newValue);
			gLightingPreferences.bForceSkyUpdate = m_forceSkyUpdateBtn->isChecked();
			pTimeOfDay->Update(true, gLightingPreferences.bForceSkyUpdate);
			m_bIsEditing = true;
			GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
			m_bIsEditing = false;
			UpdateCurveContent();
		}
	}
}

void QTimeOfDayWidget::OnSplineEditing()
{
	if (m_selectedParamId < 0)
		return;

	SCurveEditorContent* pContent = m_curveContent.get();

	if (!m_bIsPlaying)
	{
		// update current time if something is selected
		float fMinSelectedTime = std::numeric_limits<float>::max();
		for (auto& curve : pContent->m_curves)
		{
			for (auto& key : curve.m_keys)
			{
				if (key.m_bSelected)
					fMinSelectedTime = std::min(fMinSelectedTime, key.m_time.ToFloat());
			}
		}

		fMinSelectedTime /= m_fAnimTimeSecondsIn24h;
		if (fMinSelectedTime >= 0.0f && fMinSelectedTime <= 1.0f)
		{
			SetTODTime(fMinSelectedTime * 24.0f);
			UpdateCurrentTimeEdit();
			UpdateCurveTime();
		}
	}

	UpdateToDVarFromCurveContent(m_selectedParamId, 0, pContent);
	UpdateToDVarFromCurveContent(m_selectedParamId, 1, pContent);
	UpdateToDVarFromCurveContent(m_selectedParamId, 2, pContent);

	gLightingPreferences.bForceSkyUpdate = m_forceSkyUpdateBtn->isChecked();
	GetTimeOfDay()->Update(true, gLightingPreferences.bForceSkyUpdate);
	m_bIsEditing = true;
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
	m_bIsEditing = false;
	UpdateProperties();
}

void QTimeOfDayWidget::OnCopyCurveContent()
{
	if (m_selectedParamId < 0 || m_selectedParamId >= m_groups.m_params.size())
		return;

	SCurveEditorContent* pContent = m_curveContent.get();
	if (pContent)
	{
		DynArray<char> buffer;
		Serialization::SaveBinaryBuffer(buffer, *pContent);

		const STODParameter* pParam = m_groups.m_params[m_selectedParamId];
		const char* mimeType = (pParam->GetTODParamType() == eFloatType) ? sClipboardCurveContentMimeTypeFloat : sClipboardCurveContentMimeTypeColor;

		QByteArray byteArray(buffer.begin(), buffer.size());
		QMimeData* mime = new QMimeData;
		mime->setData(mimeType, byteArray);

		QApplication::clipboard()->setMimeData(mime);
	}
}

void QTimeOfDayWidget::OnPasteCurveContent()
{
	if (m_selectedParamId < 0 || m_selectedParamId >= m_groups.m_params.size())
		return;

	const QMimeData* mime = QApplication::clipboard()->mimeData();
	if (mime)
	{
		STODParameter* pParam = m_groups.m_params[m_selectedParamId];
		const char* mimeType = (pParam->GetTODParamType() == eFloatType) ? sClipboardCurveContentMimeTypeFloat : sClipboardCurveContentMimeTypeColor;

		QByteArray array = mime->data(mimeType);
		if (array.isEmpty())
			return;

		SCurveEditorContent* pContent = m_curveContent.get();
		if (pContent)
		{
			UndoBegin();
			DynArray<char> buffer;
			buffer.resize(array.size());
			memcpy(buffer.begin(), array.data(), array.size());
			Serialization::LoadBinaryBuffer(*pContent, buffer.begin(), buffer.size());
			UndoEnd();
			OnSplineEditing();
		}
	}
}

void QTimeOfDayWidget::SetTODTime(const float fTime)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	gLightingPreferences.bForceSkyUpdate = m_forceSkyUpdateBtn->isChecked();
	pTimeOfDay->SetTime(fTime, gLightingPreferences.bForceSkyUpdate);

	GetIEditor()->SetCurrentMissionTime(fTime);

	m_bIsEditing = true;
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
	m_bIsEditing = false;
}

void QTimeOfDayWidget::CreateUi()
{
	QStyle* style = this->style();

	QBoxLayout* rootVerticalLayput = new QBoxLayout(QBoxLayout::TopToBottom);
	rootVerticalLayput->setContentsMargins(1, 1, 1, 1);
	setLayout(rootVerticalLayput);

	QSplitter* splitter = new QSplitter(Qt::Orientation::Horizontal);
	splitter->setChildrenCollapsible(false);
	rootVerticalLayput->addWidget(splitter, Qt::AlignCenter);

	{
		// left part
		m_propertyTree = new QPropertyTree(this);
		m_propertyTree->setMinimumWidth(310);
		//m_propertyTree->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
		//m_propertyTree->setValueColumnWidth(0.6f);
		m_propertyTree->setHideSelection(false);
		m_propertyTree->setUndoEnabled(false, false);
		connect(m_propertyTree, &QPropertyTree::signalSelected, this, &QTimeOfDayWidget::OnPropertySelected);

		connect(m_propertyTree, &QPropertyTree::signalPushUndo, this, &QTimeOfDayWidget::UndoBegin);
		connect(m_propertyTree, &QPropertyTree::signalChanged, this, &QTimeOfDayWidget::UndoEnd);

		splitter->addWidget(m_propertyTree);
	}

	{
		//center
		QBoxLayout* centralVerticalLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		QWidget* w = new QWidget();
		w->setLayout(centralVerticalLayout);
		splitter->addWidget(w);

		QToolBar* toolBar = new QToolBar(this);
		centralVerticalLayout->addWidget(toolBar, Qt::AlignTop);

		CCurveEditor* curveEditor = new CCurveEditor(this);
		curveEditor->FillWithCurveToolsAndConnect(toolBar);
		curveEditor->SetContent(m_curveContent.get());
		curveEditor->SetTimeRange(SAnimTime(0.0f), SAnimTime(m_fAnimTimeSecondsIn24h));
		curveEditor->SetRulerVisible(true);
		curveEditor->SetRulerHeight(nRulerHeight);
		curveEditor->SetRulerTicksYOffset(nRulerGradientHeight);
		curveEditor->SetHandlesVisible(false);
		curveEditor->SetGridVisible(true);
		curveEditor->OnFitCurvesHorizontally();
		curveEditor->OnFitCurvesVertically();
		curveEditor->installEventFilter(this);
		curveEditor->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		//curveEditor->setEnabled()

		connect(curveEditor, &CCurveEditor::SignalScrub, this, &QTimeOfDayWidget::CurveEditTimeChanged);
		connect(curveEditor, &CCurveEditor::SignalDrawRulerBackground,
		        [this](QPainter& painter, const QRect& rulerRect, const Range& visibleRange)
		{
			DrawGradient(GetTimeOfDay(), m_selectedParamId, painter, rulerRect, visibleRange);
		}
		        );
		connect(curveEditor, &CCurveEditor::SignalContentAboutToBeChanged, this, &QTimeOfDayWidget::UndoBegin);
		connect(curveEditor, &CCurveEditor::SignalContentChanging, this, &QTimeOfDayWidget::OnSplineEditing);
		connect(curveEditor, &CCurveEditor::SignalContentChanged, [this](){ OnSplineEditing(); UndoEnd(); });

		centralVerticalLayout->addWidget(curveEditor, Qt::AlignTop);
		m_curveEdit = curveEditor;

		toolBar->addSeparator();
		toolBar->addAction(CryIcon("icons:CurveEditor/Copy_Curve.ico"), "Copy curve content", this, SLOT(OnCopyCurveContent()));
		toolBar->addAction(CryIcon("icons:CurveEditor/Paste_Curve.ico"), "Paste curve content", this, SLOT(OnPasteCurveContent()));

		QBoxLayout* bottomLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		bottomLayout->setContentsMargins(5, 1, 5, 3);
		centralVerticalLayout->addLayout(bottomLayout, Qt::AlignBottom);

		{
			QBoxLayout* pMediaBarLayout = new QHBoxLayout;
			pMediaBarLayout->setContentsMargins(QMargins(0, 0, 0, 0));

			QBoxLayout* pMediaBarLayoutLeft = new QHBoxLayout;
			QBoxLayout* pMediaBarLayoutCenter = new QHBoxLayout;
			QBoxLayout* pMediaBarLayoutRight = new QHBoxLayout;

			pMediaBarLayout->addLayout(pMediaBarLayoutLeft);
			pMediaBarLayout->addLayout(pMediaBarLayoutCenter);
			pMediaBarLayout->addLayout(pMediaBarLayoutRight);

			QLabel* startTimeLabel = new QLabel("Start:");
			startTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			//QTimeEdit* startTimeEdit = new QTimeEdit(this);
			CTimeEditControl* startTimeEdit = new CTimeEditControl(this);
			startTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			//startTimeEdit->setDisplayFormat("hh:mm");
			connect(startTimeEdit, &CTimeEditControl::timeChanged,
			        [](const QTime& time)
			{
				const float fTODTime = QTimeToFloat(time);
				UpdateToDAdvancedInfo([fTODTime](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fStartTime = fTODTime; });
			}
			        );
			m_startTimeEdit = startTimeEdit;
			pMediaBarLayoutLeft->addWidget(startTimeLabel);
			pMediaBarLayoutLeft->addWidget(startTimeEdit);
			pMediaBarLayoutLeft->addStretch();

			QLabel* currentTimeLabel = new QLabel("Current:");
			currentTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			CTimeEditControl* currentTimeEdit = new CTimeEditControl(this);
			//currentTimeEdit = new QTimeEdit(this);
			currentTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			m_currentTimeEdit = currentTimeEdit;
			//m_currentTimeEdit->setDisplayFormat("hh:mm");
			connect(m_currentTimeEdit, &CTimeEditControl::timeChanged, this, &QTimeOfDayWidget::CurrentTimeEdited);

			QToolButton* stopButton = new QToolButton;
			stopButton->setIcon(CryIcon("icons:Trackview/Stop_Sequence.ico"));
			stopButton->setCheckable(true);
			stopButton->setChecked(!m_bIsPlaying);
			stopButton->setAutoExclusive(true);

			QToolButton* playButton = new QToolButton;
			playButton->setIcon(CryIcon("icons:Trackview/Play_Sequence.ico"));
			playButton->setCheckable(true);
			playButton->setChecked(m_bIsPlaying);
			playButton->setAutoExclusive(true);

			QLabel* playSpeedLabel = new QLabel("Speed: ");
			playSpeedLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			QLineEdit* playSpeedEdit = new QLineEdit;
			playSpeedEdit->setFixedWidth(37);
			playSpeedEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			QDoubleValidator* validator = new QDoubleValidator(0.0, 99.999, 3, playSpeedEdit);
			validator->setNotation(QDoubleValidator::StandardNotation);
			playSpeedEdit->setValidator(validator);
			playSpeedEdit->setToolTip("TimeOfDay play speed\n Valid range:[0.000 - 99.999]");

			connect(playSpeedEdit, &QLineEdit::editingFinished,
			        [this, playSpeedEdit]()
			{
				m_curveEdit->setFocus();
				const QString value = playSpeedEdit->text();
				const float fFloatValue = value.toFloat();
				UpdateToDAdvancedInfo([fFloatValue](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fAnimSpeed = fFloatValue; });
			}
			        );
			m_playSpeedEdit = playSpeedEdit;

			connect(stopButton, &QToolButton::clicked, [this]() { m_bIsPlaying = false; m_playSpeedEdit->clearFocus(); m_propertyTree->setEnabled(true); });
			connect(playButton, &QToolButton::clicked, [this]() { m_bIsPlaying = true; m_playSpeedEdit->clearFocus();  m_propertyTree->setEnabled(false); });

			m_forceSkyUpdateBtn = new QCheckBox;
			m_forceSkyUpdateBtn->setText("Force Sky Update");
			m_forceSkyUpdateBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			m_forceSkyUpdateBtn->setChecked(gLightingPreferences.bForceSkyUpdate);
			m_forceSkyUpdateBtn->setChecked(true);

			pMediaBarLayoutCenter->setSpacing(0);
			pMediaBarLayoutCenter->setMargin(0);

			pMediaBarLayoutCenter->addWidget(currentTimeLabel);
			pMediaBarLayoutCenter->addWidget(m_currentTimeEdit);
			pMediaBarLayoutCenter->addSpacing(5);
			pMediaBarLayoutCenter->addWidget(stopButton);
			pMediaBarLayoutCenter->addWidget(playButton);
			pMediaBarLayoutCenter->addSpacing(5);
			pMediaBarLayoutCenter->addWidget(playSpeedLabel);
			pMediaBarLayoutCenter->addWidget(playSpeedEdit);
			pMediaBarLayoutCenter->addSpacing(15);
			pMediaBarLayoutCenter->addWidget(m_forceSkyUpdateBtn);

			QLabel* endTimeLabel = new QLabel("End:");
			endTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			//QTimeEdit* endTimeEdit = new QTimeEdit(QTime(23, 59, 59, 99), this);
			CTimeEditControl* endTimeEdit = new CTimeEditControl(this);
			endTimeEdit->setTime(QTime(23, 59));
			endTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			//endTimeEdit->setDisplayFormat("hh:mm");
			connect(endTimeEdit, &CTimeEditControl::timeChanged,
			        [](const QTime& time)
			{
				const float fTODTime = QTimeToFloat(time);
				UpdateToDAdvancedInfo([fTODTime](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fEndTime = fTODTime; });
			}
			        );
			m_endTimeEdit = endTimeEdit;

			pMediaBarLayoutRight->addStretch();
			pMediaBarLayoutRight->addWidget(endTimeLabel, Qt::AlignRight);
			pMediaBarLayoutRight->addWidget(endTimeEdit, Qt::AlignRight);

			bottomLayout->addLayout(pMediaBarLayout, Qt::AlignTop);
		}
	}
	setMinimumSize(size());
} // setupUi

void QTimeOfDayWidget::LoadPropertiesTree()
{
	m_propertyTree->setArchiveContext(m_pContextList->Tail());
	m_propertyTree->detach();

	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	const int nTODParamCount = pTimeOfDay->GetVariableCount();

	std::map<string, size_t> tempMap;
	int nGroupId = 0;
	for (int varID = 0; varID < nTODParamCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (pTimeOfDay->GetVariableInfo(varID, sVarInfo))
		{
			auto it = tempMap.find(sVarInfo.group);
			if (it == tempMap.end())
			{
				tempMap[sVarInfo.group] = nGroupId++;
			}
		}
	}

	m_groups.m_propertyGroups.resize(nGroupId);
	for (int varID = 0; varID < nTODParamCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (pTimeOfDay->GetVariableInfo(varID, sVarInfo))
		{
			const int nParamID = sVarInfo.nParamId;
			const char* sGroupName = sVarInfo.group;

			const int groupId = tempMap[sGroupName];

			STODParameterGroup& group = m_groups.m_propertyGroups[groupId];
			group.m_id = groupId;
			group.m_name = sGroupName;

			STODParameter todParam;
			todParam.SetParamID(nParamID);
			todParam.SetName(sVarInfo.name);
			todParam.SetGroupName(sGroupName);
			todParam.SetLabel(sVarInfo.displayName);
			todParam.SetTODParamType((sVarInfo.type == ITimeOfDay::TYPE_FLOAT) ? eFloatType : eColorType);
			todParam.SetValue(Vec3(sVarInfo.fValue[0], sVarInfo.fValue[1], sVarInfo.fValue[2]));
			todParam.m_pGroup = &group;
			todParam.m_IDWithinGroup = group.m_Params.size();
			group.m_Params.push_back(todParam);
		}
	}

	//fill param map for fast update
	m_groups.m_params.resize(nTODParamCount);
	for (STODParameterGroupSet::TPropertyGroupMap::iterator it = m_groups.m_propertyGroups.begin(); it != m_groups.m_propertyGroups.end(); ++it)
	{
		STODParameterGroup& group = *it;
		const size_t nParamSize = group.m_Params.size();
		for (size_t i = 0; i < nParamSize; ++i)
		{
			STODParameter* pParam = &group.m_Params[i];
			const int nParamId = pParam->GetParamID();
			if (nParamId >= 0 && nParamId < m_groups.m_params.size())
			{
				m_groups.m_params[pParam->GetParamID()] = pParam;
			}
			else
			{
				assert(false);
				CryLogAlways("ERROR: QTimeOfDayWidget::LoadPropertiesTree() Parameter mismatch");
			}
		}
	}

	m_propertyTree->attach(Serialization::SStruct(m_groups));
	m_propertyTree->expandAll();
}

void QTimeOfDayWidget::UpdateValues()
{
	UpdateProperties();
	UpdateCurveTime();
	UpdateCurrentTimeEdit();

	ITimeOfDay* pTimeOfDay = GetTimeOfDay();

	ITimeOfDay::SAdvancedInfo advInfo;
	pTimeOfDay->GetAdvancedInfo(advInfo);

	const QTime startTime = FloatToQTime(advInfo.fStartTime);
	SetQTimeEditTimeBlocking(m_startTimeEdit, startTime);
	const QTime endTime = FloatToQTime(advInfo.fEndTime);
	SetQTimeEditTimeBlocking(m_endTimeEdit, endTime);

	m_playSpeedEdit->blockSignals(true);
	//m_playSpeedEdit->setValue(advInfo.fAnimSpeed);
	m_playSpeedEdit->setText(QString::number(advInfo.fAnimSpeed));
	m_playSpeedEdit->blockSignals(false);
}

void QTimeOfDayWidget::UpdateSelectedParamId()
{
	PropertyRow* row = m_propertyTree->selectedRow();
	if (!row)
	{
		m_selectedParamId = -1;
		return;
	}

	STODParameter* pParam = NULL;
	while (row && row->parent())
	{
		if (STODParameterGroup* pGroup = row->parent()->serializer().cast<STODParameterGroup>())
		{
			int index = row->parent()->childIndex(row);
			if (index >= 0 && index < pGroup->m_Params.size())
				pParam = &pGroup->m_Params[index];
			break;
		}
		row = row->parent();
	}

	m_selectedParamId = pParam ? pParam->GetParamID() : -1;
}

void QTimeOfDayWidget::UpdateProperties()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	const int nToDVarCount = pTimeOfDay->GetVariableCount();
	for (int varID = 0; varID < nToDVarCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (pTimeOfDay->GetVariableInfo(varID, sVarInfo))
		{
			if (sVarInfo.nParamId >= 0 && sVarInfo.nParamId < m_groups.m_params.size())
			{
				STODParameter* pParam = m_groups.m_params[sVarInfo.nParamId];
				pParam->SetValue(Vec3(sVarInfo.fValue[0], sVarInfo.fValue[1], sVarInfo.fValue[2]));
			}
		}
	}

	m_propertyTree->revert();
}

void QTimeOfDayWidget::UpdateCurrentTimeEdit()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	const float fTime = pTimeOfDay->GetTime();
	QTime currentTime = FloatToQTime(fTime);

	SetQTimeEditTimeBlocking(m_currentTimeEdit, currentTime);
}

void QTimeOfDayWidget::UpdateCurveTime()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	const float fTime = pTimeOfDay->GetTime();
	const float fSplineTime = (fTime / 24.0f) * m_fAnimTimeSecondsIn24h;
	m_curveEdit->SetTime(SAnimTime(fSplineTime));
}

bool QTimeOfDayWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == m_curveEdit)
	{
		//forward some keys to m_propertyTree, when its not focused
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
			if ((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down) ||
			    (keyEvent->key() == Qt::Key_Left) || (keyEvent->key() == Qt::Key_Right) ||
			    (keyEvent->key() == Qt::Key_F && keyEvent->modifiers() == Qt::CTRL) ||
			    (keyEvent->key() == Qt::Key_Enter) || (keyEvent->key() == Qt::Key_Return)
			    )
			{
				return QApplication::sendEvent(m_propertyTree, event);
			}
		}
	}

	return QObject::eventFilter(obj, event);
}

void QTimeOfDayWidget::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnTimeOfDayChange && !m_bIsEditing)
	{
		UpdateCurveContent();
		UpdateProperties();
	}
}

