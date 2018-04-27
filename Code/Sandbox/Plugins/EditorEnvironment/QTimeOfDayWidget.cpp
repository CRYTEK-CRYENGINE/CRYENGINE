// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QSplitter>
#include <QTimeEdit>
#include <QToolBar>
#include <QToolButton>

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
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

#include "QPropertyTree/ContextList.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "CurveEditor.h"
#include "TimeEditControl.h"
#include "QViewport.h"
#include <CryIcon.h>
#include <EditorStyleHelper.h>
#include "CryMath/Bezier_impl.h"

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
	assert(pTimeOfDay); //pTimeOfDay is always exists
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

	ITimeOfDay::SAdvancedInfo sAdvInfo;
	pTimeOfDay->GetAdvancedInfo(sAdvInfo);
	updateFunc(sAdvInfo);
	pTimeOfDay->SetAdvancedInfo(sAdvInfo);
	pTimeOfDay->Update(false, true);
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
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
} // unnamed namespace

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
	explicit CContentChangedUndoCommand(QTimeOfDayWidget* pWidget)
	{
		m_paramId = pWidget->m_selectedParamId;
		SCurveEditorContent* pContent = pWidget->m_curveContent.get();
		Serialization::SaveBinaryBuffer(m_oldState, *pContent);
	}

	void SaveNewState(QTimeOfDayWidget* pWidget)
	{
		SCurveEditorContent* pContent = pWidget->m_curveContent.get();
		Serialization::SaveBinaryBuffer(m_newState, *pContent);
	}

	int GetParamId() const { return m_paramId; }

private:
	virtual const char* GetDescription() override { return "QTimeOfDayWidget content changed"; }

	virtual void        Undo(bool bUndo) override
	{
		Update(m_oldState);
	}

	virtual void Redo() override
	{
		Update(m_newState);
	}

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

			GetTimeOfDay()->Update(true, true);
			GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
		}
	}

	DynArray<char> m_oldState;
	DynArray<char> m_newState;
	int            m_paramId;
};

//////////////////////////////////////////////////////////////////////////
class QTimeOfDayWidget::CUndoConstPropTreeCommand : public IUndoObject
{
public:
	explicit CUndoConstPropTreeCommand(QTimeOfDayWidget* pTodWidget)
		: m_pTodWidget(pTodWidget)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(m_undoState, pTimeOfDay->GetConstantParams());
	}

private:
	virtual const char* GetDescription() override { return "Set Environment constant properties"; }

	virtual void        Undo(bool bUndo = true) override
	{
		if (bUndo)
		{
			ITimeOfDay* pTimeOfDay = GetTimeOfDay();
			gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(m_redoState, pTimeOfDay->GetConstantParams());
		}

		ApplyState(m_undoState);
	}

	virtual void Redo() override
	{
		ApplyState(m_redoState);
	}

	void ApplyState(const DynArray<char>& state)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		pTimeOfDay->ResetConstants(state);
		m_pTodWidget->UpdateConstPropTree();
	}

	QTimeOfDayWidget* m_pTodWidget;
	DynArray<char>    m_undoState;
	DynArray<char>    m_redoState;
};

//////////////////////////////////////////////////////////////////////////
QTimeOfDayWidget::QTimeOfDayWidget()
	: m_pContextList(new Serialization::CContextList())
	, m_propertyTreeVar(nullptr)
	, m_currentTimeEdit(nullptr)
	, m_startTimeEdit(nullptr)
	, m_endTimeEdit(nullptr)
	, m_playSpeedEdit(nullptr)
	, m_curveEdit(nullptr)
	, m_curveContent(new SCurveEditorContent)
	, m_selectedParamId(-1)
	, m_pUndoCommand(nullptr)
	, m_fAnimTimeSecondsIn24h(0.0f)
	, m_bIsPlaying(false)
	, m_bIsEditing(false)
	, m_splitterBetweenTrees(nullptr)
	, m_propertyTreeConst(nullptr)
{
	m_pContextList->Update<QTimeOfDayWidget>(this);

	m_fAnimTimeSecondsIn24h = GetTimeOfDay()->GetAnimTimeSecondsIn24h();

	CreateUi();
	LoadPropertiesTrees();

	Refresh();

	GetIEditor()->RegisterNotifyListener(this);
}

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
	UpdateVarPropTree();
}

void QTimeOfDayWidget::CurrentTimeEdited()
{
	const float fTODTime = QTimeToFloat(m_currentTimeEdit->time());
	SetTODTime(fTODTime);
	UpdateCurveTime();
	UpdateVarPropTree();
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

void QTimeOfDayWidget::UndoConstantProperties()
{
	CUndo undo("Update Environment Constants");
	CUndo::Record(new CUndoConstPropTreeCommand(this));
}

void QTimeOfDayWidget::CheckParameterChanged(STODParameter& param, const Vec3& newValue)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
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
		pTimeOfDay->Update(true, true);
		m_bIsEditing = true;
		GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
		m_bIsEditing = false;
		UpdateCurveContent();
	}
}

namespace {

void RestoreTreeState(const QVariantMap& state, const char* name, QPropertyTree* pTree)
{
	QVariant var = state.value(name);
	if (!var.isValid())
	{
		return;
	}

	auto arr = var.toByteArray();
	yasli::JSONIArchive ia;
	if (!ia.open(arr, arr.size()))
	{
		return;
	}

	ia(*pTree);
}

void SaveTreeState(const QPropertyTree* pTree, const char* name, QVariantMap& state)
{
	yasli::JSONOArchive oa;
	oa(*pTree, name);
	QVariant variant = QString(oa.buffer());
	state.insert(name, variant);
}

} //~unnamed namespace

void QTimeOfDayWidget::SetPersonalizationState(const QVariantMap& state)
{
	QVariant var = state.value("TreeSplitter");
	if (var.isValid())
	{
		m_splitterBetweenTrees->restoreState(QByteArray::fromBase64(var.toByteArray()));
	}

	RestoreTreeState(state, "TreeVar", m_propertyTreeVar);
	RestoreTreeState(state, "TreeConst", m_propertyTreeConst);
}

QVariantMap QTimeOfDayWidget::GetPersonalizationState() const
{
	QVariantMap state;
	state.insert("TreeSplitter", m_splitterBetweenTrees->saveState().toBase64());

	SaveTreeState(m_propertyTreeVar, "TreeVar", state);
	SaveTreeState(m_propertyTreeConst, "TreeConst", state);

	return state;
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

	GetTimeOfDay()->Update(true, true);
	m_bIsEditing = true;
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
	m_bIsEditing = false;
	UpdateVarPropTree();
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
	GetTimeOfDay()->SetTime(fTime, true);

	GetIEditor()->SetCurrentMissionTime(fTime);

	m_bIsEditing = true;
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
	m_bIsEditing = false;
}

void QTimeOfDayWidget::CreateUi()
{
	QBoxLayout* rootLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	rootLayout->setContentsMargins(1, 1, 1, 1);
	setLayout(rootLayout);

	QSplitter* splitter = new QSplitter(Qt::Orientation::Horizontal);
	splitter->setChildrenCollapsible(false);
	rootLayout->addWidget(splitter, Qt::AlignCenter);

	CreatePropertyTrees(splitter);
	CreateCurveEditor(splitter);

	setMinimumSize(size());
}

void QTimeOfDayWidget::CreatePropertyTrees(QSplitter* pParent)
{
	QWidget* varWidget = new QWidget;
	{
		QLabel* label = new QLabel("Variables");
		label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		m_propertyTreeVar = new QPropertyTree(this);
		m_propertyTreeVar->setHideSelection(false);
		m_propertyTreeVar->setUndoEnabled(false, false);
		connect(m_propertyTreeVar, &QPropertyTree::signalSelected, this, &QTimeOfDayWidget::OnPropertySelected);
		connect(m_propertyTreeVar, &QPropertyTree::signalPushUndo, this, &QTimeOfDayWidget::UndoBegin);
		connect(m_propertyTreeVar, &QPropertyTree::signalChanged, this, &QTimeOfDayWidget::UndoEnd);

		auto* layout = new QVBoxLayout;
		layout->setContentsMargins(QMargins(0, 0, 0, 0));
		layout->addWidget(label);
		layout->addWidget(m_propertyTreeVar);
		varWidget->setLayout(layout);
	}

	QWidget* constWidget = new QWidget;
	{
		QLabel* label = new QLabel("Constants");
		label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		m_propertyTreeConst = new QPropertyTree(this);
		m_propertyTreeConst->setHideSelection(false);
		m_propertyTreeConst->setUndoEnabled(false, false);
		connect(m_propertyTreeConst, &QPropertyTree::signalChanged, this, []() { GetTimeOfDay()->ConstantsChanged(); });
		connect(m_propertyTreeConst, &QPropertyTree::signalPushUndo, this, &QTimeOfDayWidget::UndoConstantProperties);

		auto* layout = new QVBoxLayout;
		layout->setContentsMargins(QMargins(0, 0, 0, 0));
		layout->addWidget(label);
		layout->addWidget(m_propertyTreeConst);
		constWidget->setLayout(layout);
	}

	m_splitterBetweenTrees = new QSplitter(Qt::Orientation::Vertical);
	m_splitterBetweenTrees->setChildrenCollapsible(false);

	m_splitterBetweenTrees->addWidget(varWidget);
	m_splitterBetweenTrees->addWidget(constWidget);

	pParent->addWidget(m_splitterBetweenTrees);
}

void QTimeOfDayWidget::CreateCurveEditor(QSplitter* pParent)
{
	QBoxLayout* centralVerticalLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	QWidget* w = new QWidget();
	w->setLayout(centralVerticalLayout);
	pParent->addWidget(w);

	QToolBar* toolBar = new QToolBar(this);
	centralVerticalLayout->addWidget(toolBar, Qt::AlignTop);

	m_curveEdit = new CCurveEditor(this);
	m_curveEdit->FillWithCurveToolsAndConnect(toolBar);
	m_curveEdit->SetContent(m_curveContent.get());
	m_curveEdit->SetTimeRange(SAnimTime(0.0f), SAnimTime(m_fAnimTimeSecondsIn24h));
	m_curveEdit->SetRulerVisible(true);
	m_curveEdit->SetRulerHeight(nRulerHeight);
	m_curveEdit->SetRulerTicksYOffset(nRulerGradientHeight);
	m_curveEdit->SetHandlesVisible(false);
	m_curveEdit->SetGridVisible(true);
	m_curveEdit->OnFitCurvesHorizontally();
	m_curveEdit->OnFitCurvesVertically();
	m_curveEdit->installEventFilter(this);
	m_curveEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

	connect(m_curveEdit, &CCurveEditor::SignalScrub, this, &QTimeOfDayWidget::CurveEditTimeChanged);
	connect(m_curveEdit, &CCurveEditor::SignalDrawRulerBackground, [this](QPainter& painter, const QRect& rulerRect, const Range& visibleRange)
	{
		DrawGradient(GetTimeOfDay(), m_selectedParamId, painter, rulerRect, visibleRange);
	});
	connect(m_curveEdit, &CCurveEditor::SignalContentAboutToBeChanged, this, &QTimeOfDayWidget::UndoBegin);
	connect(m_curveEdit, &CCurveEditor::SignalContentChanging, this, &QTimeOfDayWidget::OnSplineEditing);
	connect(m_curveEdit, &CCurveEditor::SignalContentChanged, [this]() { OnSplineEditing(); UndoEnd(); });

	centralVerticalLayout->addWidget(m_curveEdit, Qt::AlignTop);

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

		m_startTimeEdit = new CTimeEditControl(this);
		m_startTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		connect(m_startTimeEdit, &CTimeEditControl::timeChanged, [](const QTime& time)
		{
			const float fTODTime = QTimeToFloat(time);
			UpdateToDAdvancedInfo([fTODTime](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fStartTime = fTODTime; });
		});
		pMediaBarLayoutLeft->addWidget(startTimeLabel);
		pMediaBarLayoutLeft->addWidget(m_startTimeEdit);
		pMediaBarLayoutLeft->addStretch();

		QLabel* currentTimeLabel = new QLabel("Current:");
		currentTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		m_currentTimeEdit = new CTimeEditControl(this);
		m_currentTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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

		m_playSpeedEdit = new QLineEdit;
		m_playSpeedEdit->setFixedWidth(37);
		m_playSpeedEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QDoubleValidator* validator = new QDoubleValidator(0.0, 99.999, 3, m_playSpeedEdit);
		validator->setNotation(QDoubleValidator::StandardNotation);
		m_playSpeedEdit->setValidator(validator);
		m_playSpeedEdit->setToolTip("TimeOfDay play speed\n Valid range:[0.000 - 99.999]");

		connect(m_playSpeedEdit, &QLineEdit::editingFinished, [this]()
		{
			m_curveEdit->setFocus();
			const QString value = m_playSpeedEdit->text();
			const float fFloatValue = value.toFloat();
			UpdateToDAdvancedInfo([fFloatValue](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fAnimSpeed = fFloatValue; });
		});

		connect(stopButton, &QToolButton::clicked, [this]() { m_bIsPlaying = false; m_playSpeedEdit->clearFocus(); m_propertyTreeVar->setEnabled(true); });
		connect(playButton, &QToolButton::clicked, [this]() { m_bIsPlaying = true; m_playSpeedEdit->clearFocus();  m_propertyTreeVar->setEnabled(false); });

		pMediaBarLayoutCenter->setSpacing(0);
		pMediaBarLayoutCenter->setMargin(0);

		pMediaBarLayoutCenter->addWidget(currentTimeLabel);
		pMediaBarLayoutCenter->addWidget(m_currentTimeEdit);
		pMediaBarLayoutCenter->addSpacing(5);
		pMediaBarLayoutCenter->addWidget(stopButton);
		pMediaBarLayoutCenter->addWidget(playButton);
		pMediaBarLayoutCenter->addSpacing(5);
		pMediaBarLayoutCenter->addWidget(playSpeedLabel);
		pMediaBarLayoutCenter->addWidget(m_playSpeedEdit);
		pMediaBarLayoutCenter->addSpacing(15);

		QLabel* endTimeLabel = new QLabel("End:");
		endTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		m_endTimeEdit = new CTimeEditControl(this);
		m_endTimeEdit->setTime(QTime(23, 59));
		m_endTimeEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		connect(m_endTimeEdit, &CTimeEditControl::timeChanged, [](const QTime& time)
		{
			const float fTODTime = QTimeToFloat(time);
			UpdateToDAdvancedInfo([fTODTime](ITimeOfDay::SAdvancedInfo& sAdvInfo) { sAdvInfo.fEndTime = fTODTime; });
		});

		pMediaBarLayoutRight->addStretch();
		pMediaBarLayoutRight->addWidget(endTimeLabel, Qt::AlignRight);
		pMediaBarLayoutRight->addWidget(m_endTimeEdit, Qt::AlignRight);

		bottomLayout->addLayout(pMediaBarLayout, Qt::AlignTop);
	}
}

void QTimeOfDayWidget::LoadPropertiesTrees()
{
	m_propertyTreeVar->setArchiveContext(m_pContextList->Tail());
	m_propertyTreeVar->detach();

	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
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

	m_propertyTreeVar->attach(Serialization::SStruct(m_groups));
	m_propertyTreeConst->attach(pTimeOfDay->GetConstantParams());

	//By default, trees are collapsed. If layout is present, it will restore state
	m_propertyTreeVar->collapseAll();
	m_propertyTreeConst->collapseAll();
}

void QTimeOfDayWidget::UpdateValues()
{
	UpdateVarPropTree();
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
	m_playSpeedEdit->setText(QString::number(advInfo.fAnimSpeed));
	m_playSpeedEdit->blockSignals(false);

	UpdateConstPropTree();
}

void QTimeOfDayWidget::UpdateSelectedParamId()
{
	PropertyRow* row = m_propertyTreeVar->selectedRow();
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

void QTimeOfDayWidget::UpdateVarPropTree()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
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
		//forward some keys to m_propertyTreeVar, when its not focused
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
			if ((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down) ||
			    (keyEvent->key() == Qt::Key_Left) || (keyEvent->key() == Qt::Key_Right) ||
			    (keyEvent->key() == Qt::Key_F && keyEvent->modifiers() == Qt::CTRL) ||
			    (keyEvent->key() == Qt::Key_Enter) || (keyEvent->key() == Qt::Key_Return)
			    )
			{
				return QApplication::sendEvent(m_propertyTreeVar, event);
			}
		}
	}

	return QObject::eventFilter(obj, event);
}

void QTimeOfDayWidget::UpdateConstPropTree()
{
	m_propertyTreeVar->revert();
	m_propertyTreeConst->attach(GetTimeOfDay()->GetConstantParams());
}

void QTimeOfDayWidget::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnTimeOfDayChange && !m_bIsEditing)
	{
		UpdateCurveContent();
		UpdateVarPropTree();
		UpdateConstPropTree(); //Sun can be changed
	}
}
