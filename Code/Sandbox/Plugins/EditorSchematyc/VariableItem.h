// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VariablesModel.h"

#include <QString>

namespace Schematyc {

struct IScriptVariable;

}

namespace CrySchematycEditor {

class CStateItem;
class CObjectModel;

class CVariableItem : public CAbstractVariablesModelItem
{
	enum EOwner
	{
		State,
		Object,
	};

public:
	//CVariableItem(Schematyc::IScriptVariable& scriptVariable, CStateItem& state);
	CVariableItem(Schematyc::IScriptVariable& scriptVariable, CObjectModel& object);
	~CVariableItem();

	// CAbstractVariableItem
	virtual QString GetName() const override;
	virtual void    SetName(QString name) override;

	virtual void    Serialize(Serialization::IArchive& archive) override;
	// ~CAbstractVariableItem

	CryGUID GetGUID() const;

private:
	Schematyc::IScriptVariable& m_scriptVariable;

	EOwner                      m_owner;
};

}

