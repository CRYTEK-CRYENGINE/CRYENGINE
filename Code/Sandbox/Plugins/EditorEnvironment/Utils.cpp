// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utils.h"
#include "Controller.h"

#include <EditorFramework/Editor.h>
#include <EditorFramework/PersonalizationManager.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <IPlugin.h>

#include <CrySerialization/Color.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

STodParameter::STodParameter()
	: value(ZERO)
	, id(0)
	, type(EType::Float)
	, name("DefaultName")
	, label("DefaultLabel")
{}

const char* STodParameter::GetMimeType() const
{
	switch (type)
	{
	case STodParameter::EType::Float:
		return "binary/curveeditorcontentfloat";
	case STodParameter::EType::Color:
		return "binary/curveeditorcontentcolor";
	}

	CRY_ASSERT(!"Unsupported type");
	return "Unsupported type";
}

// Should be free function in order to fill tree properly
bool Serialize(Serialization::IArchive& ar, STodParameter& param, const char* szName, const char* szLabel)
{
	Vec3 value = param.value;

	if (param.type == STodParameter::EType::Color)
	{
		if (!ar(Serialization::Vec3AsColor(value), szName, szLabel))
			return false;
	}
	else if (param.type == STodParameter::EType::Float)
	{
		if (value.y < value.z)
		{
			if (!ar(Serialization::Range(value.x, value.y, value.z), szName, szLabel))
			{
				return false;
			}
		}
		else
		{
			if (!ar(value.x, szName, szLabel))
			{
				return false;
			}
		}
	}

	if (ar.isInput())
	{
		// Special step: validating/interpolating values.
		if (CController* pController = ar.context<CController>())
		{
			pController->InterpolateVarTreeChanges(param, value);
		}
	}
	return true;
}

void STodGroup::Serialize(Serialization::IArchive& ar)
{
	for (size_t i = 0, nParamCount = params.size(); i < nParamCount; ++i)
	{
		ar(params[i], params[i].name, params[i].label);
	}
}

void STodVariablesState::Serialize(Serialization::IArchive& ar)
{
	for (auto& group : groups)
	{
		const char* name = group.name.c_str();
		ar(group, name, name);
	}
}

CVariableUndoCommand::CVariableUndoCommand(int paramId, const string& paramName, ITimeOfDay::IPreset* pPreset, CController& controller, DynArray<char>&& curveContent, QObject* pCancelUndoSignal)
	: m_paramId(paramId)
	, m_pPreset(pPreset)
	, m_pController(&controller)
	, m_undoState(curveContent)
{
	m_commandDescription.Format("Environment Editor: Curve content changed for '%s'", paramName);

	m_connection = QObject::connect(pCancelUndoSignal, &QObject::destroyed, [this]()
	{
		m_pController = nullptr;
	});
}

const char* CVariableUndoCommand::GetDescription()
{
	return m_commandDescription;
}

void CVariableUndoCommand::Undo(bool bUndo)
{
	if (!m_pController)
	{
		return;
	}

	m_pController->UndoVariableChange(m_pPreset, m_paramId, bUndo, m_undoState, m_redoState);
}

void CVariableUndoCommand::Redo()
{
	if (!m_pController)
	{
		return;
	}

	m_pController->RedoVariableChange(m_pPreset, m_paramId, m_redoState);
}

void CVariableUndoCommand::SaveNewState(DynArray<char>&& buff)
{
	m_redoState = buff;
}

void SaveTreeState(const CEditor& editor, const QPropertyTreeLegacy* pTree, const char* szPropertyName)
{
	yasli::JSONOArchive oa;
	oa(*pTree, szPropertyName);
	QString state = QtUtil::ToQString(oa.buffer());

	GetIEditor()->GetPersonalizationManager()->SetProperty(editor.GetEditorName(), szPropertyName, state);
}

void RestoreTreeState(const CEditor& editor, QPropertyTreeLegacy* pTree, const char* szPropertyName)
{
	QString state;
	GetIEditor()->GetPersonalizationManager()->GetProperty(editor.GetEditorName(), szPropertyName, state);

	yasli::JSONIArchive ia;
	if (ia.open(state.toStdString().c_str(), state.size()))
	{
		ia(*pTree);
	}
	else
	{
		pTree->collapseAll();
	}
}
