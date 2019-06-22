// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Controller.h"
#include "EnvironmentEditor.h"

#include <EditorStyleHelper.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CrySystem/ISystem.h>

#include <QClipboard>

CController::CController(CEnvironmentEditor& editor)
	: m_editor(editor)
	, m_playbackMode(PlaybackMode::Edit)
	, m_selectedVariableIndex(-1)
{
}

void CController::OnOpenAsset()
{
	// QObject is enough for transfer signal for Undo's
	m_pUndoVarCancelSignal.reset(new QObject);

	RebuildVariableTreeFromPreset(true);
	RebuildCurveContentFromPreset();

	signalAssetOpened();
}

void CController::OnCloseAsset()
{
	m_pUndoVarCancelSignal.reset();
	signalAssetClosed();
}

void CController::CopySelectedCurveToClipboard()
{
	if (m_selectedVariableIndex < 0)
	{
		return;
	}

	DynArray<char> buffer;
	Serialization::SaveBinaryBuffer(buffer, m_selectedVariableContent);

	const STodParameter* pParam = m_variables.remapping[m_selectedVariableIndex];
	const char* szMimeType = pParam->GetMimeType();

	QByteArray byteArray(buffer.begin(), buffer.size());
	QMimeData* pMime = new QMimeData;
	pMime->setData(szMimeType, byteArray);

	QApplication::clipboard()->setMimeData(pMime);
}

void CController::PasteCurveContentFromClipboard()
{
	if (m_selectedVariableIndex < 0)
	{
		return;
	}

	const QMimeData* pMime = QApplication::clipboard()->mimeData();
	if (!pMime)
	{
		return;
	}

	STodParameter* pParam = m_variables.remapping[m_selectedVariableIndex];
	const char* mimeType = pParam->GetMimeType();

	QByteArray array = pMime->data(mimeType);
	if (array.isEmpty())
	{
		return;
	}

	OnSelectedVariableStartChange();

	DynArray<char> buffer;
	buffer.resize(array.size());
	memcpy(buffer.begin(), array.data(), array.size());
	Serialization::LoadBinaryBuffer(m_selectedVariableContent, buffer.begin(), buffer.size());
	signalCurveContentChanged();

	OnCurveEditorEndChange();
}

void CController::RebuildVariableTreeFromPreset(bool newPreset)
{
	auto& variables = m_editor.GetPreset()->GetVariables();
	const int nTODParamCount = variables.GetVariableCount();

	std::map<string, size_t> tempMap;
	int nGroupId = 0;
	for (int varID = 0; varID < nTODParamCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (variables.GetVariableInfo(varID, sVarInfo))
		{
			auto it = tempMap.find(sVarInfo.szGroup);
			if (it == tempMap.end())
			{
				tempMap[sVarInfo.szGroup] = nGroupId++;
			}
		}
	}

	if (newPreset)
	{
		m_variables.groups.clear();
		m_variables.groups.resize(tempMap.size());
	}

	// Create [GroupId <=> current element] counter
	std::map<int, size_t> groupElementCounter;
	for (auto it : tempMap)
	{
		groupElementCounter[it.second] = 0;
	}

	for (int varID = 0; varID < nTODParamCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (variables.GetVariableInfo(varID, sVarInfo))
		{
			const int nParamID = sVarInfo.nParamId;
			const char* sGroupName = sVarInfo.szGroup;

			const int groupId = tempMap[sGroupName];

			STodGroup& group = m_variables.groups[groupId];
			group.id = groupId;
			group.name = sGroupName;

			STodParameter todParam;
			todParam.type = (sVarInfo.type == ITimeOfDay::TYPE_FLOAT) ? STodParameter::EType::Float : STodParameter::EType::Color;
			todParam.value = Vec3(sVarInfo.fValue[0], sVarInfo.fValue[1], sVarInfo.fValue[2]);
			todParam.id = nParamID;

			if (newPreset)
			{
				todParam.name = sVarInfo.szName;
				todParam.label = sVarInfo.szDisplayName;
				group.params.push_back(todParam);
			}
			else
			{
				// Update value only. Pointer to name should not be changed for proper selection in a tree.
				int indexInGroup = groupElementCounter[groupId];
				
				CRY_ASSERT(group.params[indexInGroup].id == todParam.id);
				CRY_ASSERT(group.params[indexInGroup].type == todParam.type);

				group.params[indexInGroup].value = todParam.value;
				++groupElementCounter[groupId];
			}
		}
	}

	// Fill remapping
	m_variables.remapping.resize(nTODParamCount);
	for (auto& group : m_variables.groups)
	{
		const size_t itemsInTheGroup = group.params.size();
		for (size_t i = 0; i < itemsInTheGroup; ++i)
		{
			STodParameter* pParam = &group.params[i];
			const int nParamId = pParam->id;
			if (nParamId >= 0 && nParamId < m_variables.remapping.size())
			{
				m_variables.remapping[pParam->id] = pParam;
			}
			else
			{
				assert(false);
				CryLogAlways("ERROR: CController::OnOpenAsset() Parameter mismatch");
			}
		}
	}

	signalVariableTreeChanged();
}

void CController::RebuildCurveContentFromPreset()
{
	if (m_selectedVariableIndex >= 0)
	{
		ITimeOfDay::IPreset* pPreset = m_editor.GetPreset();
		ITimeOfDay::SVariableInfo sVarInfo;
		pPreset->GetVariables().GetVariableInfo(m_selectedVariableIndex, sVarInfo);

		if (ITimeOfDay::TYPE_FLOAT == sVarInfo.type)
		{
			const QColor& textColor = GetStyleHelper()->textColor();

			m_selectedVariableContent.m_curves.resize(1);
			m_selectedVariableContent.m_curves[0].m_color.set(textColor.red(), textColor.green(), textColor.blue(), 255);
		}
		else if (ITimeOfDay::TYPE_COLOR == sVarInfo.type)
		{
			m_selectedVariableContent.m_curves.resize(3);
			m_selectedVariableContent.m_curves[0].m_color.set(190, 20, 20, 255); // r
			m_selectedVariableContent.m_curves[1].m_color.set(20, 190, 20, 255); // g
			m_selectedVariableContent.m_curves[2].m_color.set(20, 20, 190, 255); // b
		}

		for (int nSplineId = 0; nSplineId < m_selectedVariableContent.m_curves.size(); ++nSplineId)
		{
			std::vector<SCurveEditorKey>& splineKeys = m_selectedVariableContent.m_curves[nSplineId].m_keys;
			splineKeys.clear();

			const ITimeOfDay::IVariables& variables = pPreset->GetVariables();

			const uint nKeysNum = variables.GetSplineKeysCount(m_selectedVariableIndex, nSplineId);
			if (nKeysNum)
			{
				std::vector<SBezierKey> keys;
				keys.resize(nKeysNum);
				variables.GetSplineKeysForVar(m_selectedVariableIndex, nSplineId, &keys[0], nKeysNum);

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
	else
	{
		m_selectedVariableContent.m_curves.resize(0);
	}

	signalCurveContentChanged();
}

void CController::OnVariableSelected(int id)
{
	const bool newParamSelected = (id != m_selectedVariableIndex);
	if (newParamSelected && m_pUndoVarCommand)
	{
		// Can happen when user modifies variable via tree
		OnVariableTreeEndChange();
	}

	m_selectedVariableIndex = id;

	if (newParamSelected)
	{
		RebuildCurveContentFromPreset();
		signalNewVariableSelected();
	}
}

void CController::InterpolateVarTreeChanges(STodParameter& param, const Vec3& newValue)
{
	ITimeOfDay::IPreset* const pPreset = m_editor.GetPreset();
	CRY_ASSERT(pPreset);

	const float time = m_editor.GetTimeOfDay()->GetTime();
	const float currTime = (time / 24.0f) * m_editor.GetTimeOfDay()->GetAnimTimeSecondsIn24h();

	bool changed = false;

	const Vec3 oldValue = param.value;

	const float todParameterCompareEpsilon = 0.0001f;
	ITimeOfDay::IVariables& variables = pPreset->GetVariables();

	if (fabs(newValue.x - oldValue.x) > todParameterCompareEpsilon)
	{
		changed = true;
		variables.UpdateSplineKeyForVar(param.id, 0, currTime, newValue.x);
	}
	if (fabs(newValue.y - oldValue.y) > todParameterCompareEpsilon)
	{
		changed = true;
		variables.UpdateSplineKeyForVar(param.id, 1, currTime, newValue.y);
	}
	if (fabs(newValue.z - oldValue.z) > todParameterCompareEpsilon)
	{
		changed = true;
		variables.UpdateSplineKeyForVar(param.id, 2, currTime, newValue.z);
	}

	if (changed)
	{
		param.value = newValue;
		m_editor.GetTimeOfDay()->Update(true, true);
		GetIEditor()->Notify(eNotify_OnTimeOfDayChange);

		if (param.id == m_selectedVariableIndex)
		{
			RebuildCurveContentFromPreset();
		}
	}
}

void CController::OnSelectedVariableStartChange()
{
	if (CUndo::IsRecording() || m_pUndoVarCommand || m_selectedVariableIndex < 0 || !m_editor.GetPreset())
	{
		return;
	}

	DynArray<char> buff;
	Serialization::SaveBinaryBuffer(buff, m_selectedVariableContent);
	m_pUndoVarCommand.reset(new CVariableUndoCommand(m_selectedVariableIndex, m_variables.remapping[m_selectedVariableIndex]->name, m_editor.GetPreset(), *this, std::move(buff),
	                                                 m_pUndoVarCancelSignal.get()));

	GetIEditor()->GetIUndoManager()->Begin();
	GetIEditor()->GetIUndoManager()->RecordUndo(m_pUndoVarCommand.get());
}

void CController::OnCurveDragging()
{
	ApplyVariableChangeToPreset(m_selectedVariableIndex, m_selectedVariableContent);
	RebuildVariableTreeFromPreset();
}

void CController::OnCurveEditorEndChange()
{
	if (!m_pUndoVarCommand || m_selectedVariableIndex < 0)
	{
		return;
	}

	CRY_ASSERT(m_pUndoVarCommand->GetParamId() == m_selectedVariableIndex && m_pUndoVarCommand->GetPreset() == m_editor.GetPreset());

	DynArray<char> buff;
	Serialization::SaveBinaryBuffer(buff, m_selectedVariableContent);
	m_pUndoVarCommand->SaveNewState(std::move(buff));

	ApplyVariableChangeToPreset(m_selectedVariableIndex, m_selectedVariableContent);
	RebuildVariableTreeFromPreset();

	GetIEditor()->GetIUndoManager()->Accept(m_pUndoVarCommand->GetDescription());
	m_pUndoVarCommand.release();
}

void CController::OnVariableTreeEndChange()
{
	if (!m_pUndoVarCommand || m_selectedVariableIndex < 0)
	{
		return;
	}

	CRY_ASSERT(m_pUndoVarCommand->GetParamId() == m_selectedVariableIndex && m_pUndoVarCommand->GetPreset() == m_editor.GetPreset());

	// 3dEngine data and Curve content are already updated via InterpolateVarTreeChanges
	GetIEditor()->GetIUndoManager()->Accept(m_pUndoVarCommand->GetDescription());
	m_pUndoVarCommand.release();

	m_editor.GetAssetBeingEdited()->SetModified(true);
}

void UpdateToDVarFromCurveContent(ITimeOfDay::IPreset* pPreset, int nVarId, int nSplineId, SCurveEditorContent& content)
{
	if (nSplineId < 0 || nSplineId >= content.m_curves.size())
	{
		return;
	}

	const SCurveEditorCurve& curve = content.m_curves[nSplineId];
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

	pPreset->GetVariables().SetSplineKeysForVar(nVarId, nSplineId, &keys[0], keys.size());
}

void CController::ApplyVariableChangeToPreset(int paramId, SCurveEditorContent& content)
{
	auto* pPreset = m_editor.GetPreset();
	UpdateToDVarFromCurveContent(pPreset, paramId, 0, content);
	UpdateToDVarFromCurveContent(pPreset, paramId, 1, content);
	UpdateToDVarFromCurveContent(pPreset, paramId, 2, content);

	m_editor.GetTimeOfDay()->Update(true, true);

	m_editor.GetAssetBeingEdited()->SetModified(true);
}

void CController::UndoVariableChange(ITimeOfDay::IPreset* pPreset, int paramId, bool undo, const DynArray<char>& undoState, DynArray<char>& redoState)
{
	CRY_ASSERT(m_editor.GetPreset() == pPreset);

	if (paramId != m_selectedVariableIndex)
	{
		OnVariableSelected(paramId);
	}

	if (undo)
	{
		Serialization::SaveBinaryBuffer(redoState, m_selectedVariableContent);
	}

	// Push previous state to the Engine
	Serialization::LoadBinaryBuffer(m_selectedVariableContent, undoState.begin(), undoState.size());
	ApplyVariableChangeToPreset(m_selectedVariableIndex, m_selectedVariableContent);

	// Update controls
	RebuildVariableTreeFromPreset();
	RebuildCurveContentFromPreset();
}

void CController::RedoVariableChange(ITimeOfDay::IPreset* pPreset, int paramId, const DynArray<char>& redoState)
{
	CRY_ASSERT(m_editor.GetPreset() == pPreset);

	if (paramId != m_selectedVariableIndex)
	{
		OnVariableSelected(paramId);
	}

	Serialization::LoadBinaryBuffer(m_selectedVariableContent, redoState.begin(), redoState.size());
	ApplyVariableChangeToPreset(m_selectedVariableIndex, m_selectedVariableContent);

	// Update controls
	RebuildVariableTreeFromPreset();
	RebuildCurveContentFromPreset();
}

void CController::SetCurrentTime(QWidget* pSender, float time)
{
	ITimeOfDay* const pTimeOfDay = m_editor.GetTimeOfDay();
	pTimeOfDay->SetTime(time, true);

	GetIEditor()->SetCurrentMissionTime(time);
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);

	// Update state of variables from 3dEngine state for current time
	ITimeOfDay::IVariables& engineVars = m_editor.GetPreset()->GetVariables();
	const size_t varCount = m_variables.remapping.size();
	for (int varID = 0; varID < varCount; ++varID)
	{
		ITimeOfDay::SVariableInfo sVarInfo;
		if (engineVars.GetVariableInfo(varID, sVarInfo))
		{
			m_variables.remapping[varID]->value = Vec3(sVarInfo.fValue[0], sVarInfo.fValue[1], sVarInfo.fValue[2]);
		}
	}

	signalCurrentTimeChanged(pSender, time);
}

void CController::GetEnginePlaybackParams(float& startTime, float& endTime, float& speed) const
{
	ITimeOfDay* const pTimeOfDay = m_editor.GetTimeOfDay();

	ITimeOfDay::SAdvancedInfo info;
	pTimeOfDay->GetAdvancedInfo(info);

	startTime = info.fStartTime;
	endTime = info.fEndTime;
	speed = info.fAnimSpeed;
}

float CController::GetCurrentTime() const
{
	ITimeOfDay* const pTimeOfDay = m_editor.GetTimeOfDay();
	return pTimeOfDay->GetTime();
}

void CController::SetEnginePlaybackParams(float startTime, float endTime, float speed) const
{
	ITimeOfDay::SAdvancedInfo info;
	info.fStartTime = startTime;
	info.fEndTime = endTime;
	info.fAnimSpeed = speed;

	ITimeOfDay* const pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	pTimeOfDay->SetAdvancedInfo(info);

	pTimeOfDay->Update(false, true);
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
}

PlaybackMode CController::GetPlaybackMode() const
{
	return m_playbackMode;
}

void CController::TogglePlaybackMode()
{
	m_playbackMode = (m_playbackMode == PlaybackMode::Edit) ? PlaybackMode::Play : PlaybackMode::Edit;
	signalPlaybackModeChanged(m_playbackMode);
}

void CController::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate)
	{
		if (&m_editor.GetTimeOfDay()->GetCurrentPreset() == m_editor.GetPreset() && m_playbackMode == PlaybackMode::Play)
		{
			AnimateTime();
		}
	}
}

void CController::AnimateTime()
{
	ITimeOfDay* const pTimeOfDay = m_editor.GetTimeOfDay();

	const float hour = pTimeOfDay->GetTime();

	ITimeOfDay::SAdvancedInfo advInfo;
	pTimeOfDay->GetAdvancedInfo(advInfo);
	float time = hour + gEnv->pTimer->GetFrameTime() * advInfo.fAnimSpeed;
	if (time > advInfo.fEndTime)
		time = advInfo.fStartTime;
	if (time < advInfo.fStartTime)
		time = advInfo.fEndTime;

	// Update all widgets
	SetCurrentTime(nullptr, time);
}
