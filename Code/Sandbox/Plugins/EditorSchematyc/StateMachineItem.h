// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractObjectItem.h"

#include <CryIcon.h>

#include <QString>

namespace Schematyc {

struct IScriptStateMachine;

}

namespace CrySchematycEditor {

class CStateItem;

class CStateMachineItem : public CAbstractObjectStructureModelItem
{
public:
	CStateMachineItem(Schematyc::IScriptStateMachine& scriptStateMachine, CAbstractObjectStructureModel& model);
	virtual ~CStateMachineItem();

	// CAbstractObjectStructureModelItem
	virtual void                               SetName(QString name) override;
	virtual int32                              GetType() const override          { return eObjectItemType_StateMachine; }

	virtual const CryIcon*                     GetIcon() const override;
	virtual CAbstractObjectStructureModelItem* GetParentItem() const override    { return static_cast<CAbstractObjectStructureModelItem*>(m_pParentItem); }

	virtual uint32                             GetNumChildItems() const override { return GetNumStates(); }
	virtual CAbstractObjectStructureModelItem* GetChildItemByIndex(uint32 index) const override;
	virtual uint32                             GetChildItemIndex(const CAbstractObjectStructureModelItem& item) const override;

	virtual uint32                             GetIndex() const;
	virtual void                               Serialize(Serialization::IArchive& archive) override;

	virtual bool                               AllowsRenaming() const override;
	// ~CAbstractObjectStructureModelItem

	void             SetParentItem(CAbstractObjectStructureModelItem* pParentItem) { m_pParentItem = pParentItem; }

	uint32           GetNumStates() const                                          { return m_states.size(); }
	CStateItem*      GetStateItemByIndex(uint32 index) const;
	CStateItem*      CreateState();
	bool             RemoveState();

	CryGUID GetGUID() const;

protected:
	void LoadFromScriptElement();

private:
	CAbstractObjectStructureModelItem* m_pParentItem;
	Schematyc::IScriptStateMachine&    m_scriptStateMachine;
	std::vector<CStateItem*>           m_states;
};

}

