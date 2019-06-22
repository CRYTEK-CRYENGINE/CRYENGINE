// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IUndoObject.h>
#include <Cry3DEngine/ITimeOfDay.h>

class CController;
class CEditor;
class QPropertyTreeLegacy;

// Variables inside TimeOfDay are data driven by design
// STodPreset - is a root structure to represent all of them in a propertyTree (variables)

struct STodParameter
{
	STodParameter();
	const char* GetMimeType() const;

	enum class EType
	{
		Float,
		Color,
	};

	EType  type;
	Vec3   value;
	int    id;
	string name;
	string label;
};

bool Serialize(Serialization::IArchive& ar, STodParameter& param, const char* szName, const char* szLabel);

struct STodGroup
{
	void Serialize(Serialization::IArchive& ar);

	int                        id;
	std::string                name;
	std::vector<STodParameter> params;
};

// State of a TOD variables at CURRENT time. 3dEngine has simulation of curves.
// When time is changed inside 3dEngine, values inside this structure has to be taken from 3dEngine.
struct STodVariablesState
{
	void Serialize(Serialization::IArchive& ar);

	std::vector<STodGroup>      groups;
	std::vector<STodParameter*> remapping; // Fast access mapping: paramID to pointer
};

class CVariableUndoCommand : public IUndoObject
{
public:
	CVariableUndoCommand(int paramId, const string& paramName, ITimeOfDay::IPreset* pPreset, CController& controller, DynArray<char>&& curveContent, QObject* pCancelUndoSignal);

	virtual const char*  GetDescription() override;

	int                  GetParamId() const { return m_paramId; }
	ITimeOfDay::IPreset* GetPreset() const  { return m_pPreset; }

	void                 SaveNewState(DynArray<char>&& buff);

private:
	virtual void Undo(bool bUndo) override;
	virtual void Redo() override;

	const int               m_paramId;
	ITimeOfDay::IPreset*    m_pPreset;
	CController*            m_pController;
	QMetaObject::Connection m_connection;

	DynArray<char>          m_undoState;
	DynArray<char>          m_redoState;
	string                  m_commandDescription; // for UI
};

void SaveTreeState(const CEditor& editor, const QPropertyTreeLegacy* pTree, const char* szPropertyName);
void RestoreTreeState(const CEditor& editor, QPropertyTreeLegacy* pTree, const char* szPropertyName);
