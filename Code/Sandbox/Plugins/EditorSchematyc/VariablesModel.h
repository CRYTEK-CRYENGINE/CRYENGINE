// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

namespace CrySchematycEditor {

class CAbstractVariablesModel;

class CAbstractVariablesModelItem
{
public:
	CAbstractVariablesModelItem(CAbstractVariablesModel& model)
		: m_model(model)
	{}
	~CAbstractVariablesModelItem() {}

	CAbstractVariablesModel& GetModel() const { return m_model; }

	virtual QString          GetName() const = 0;
	virtual void             SetName(QString name) = 0;

	virtual uint32           GetIndex() const;

	virtual void             Serialize(Serialization::IArchive& archive) {}

public:
	CCrySignal<void(CAbstractVariablesModelItem&)> SignalNameChanged;

private:
	CAbstractVariablesModel& m_model;
};

class CAbstractVariablesModel
{
public:
	CAbstractVariablesModel() {}
	virtual ~CAbstractVariablesModel() {}

	virtual uint32                       GetNumVariables() const                                                     { return 0; }
	virtual CAbstractVariablesModelItem* GetVariableItemByIndex(uint32 index)                                        { return nullptr; }
	virtual uint32                       GetVariableItemIndex(const CAbstractVariablesModelItem& variableItem) const { return 0xffffffff; }
	virtual CAbstractVariablesModelItem* CreateVariable()                                                            { return nullptr; }
	virtual bool                         RemoveVariable(CAbstractVariablesModelItem& variableItem)                   { return false; }

public:
	CCrySignal<void(CAbstractVariablesModelItem&)> SignalVariableAdded;
	CCrySignal<void(CAbstractVariablesModelItem&)> SignalVariableRemoved;
	CCrySignal<void(CAbstractVariablesModelItem&)> SignalVariableInvalidated;
};

}

