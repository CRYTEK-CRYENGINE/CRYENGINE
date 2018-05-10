// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GraphItem.h"
#include "StateMachineItem.h"
#include "SignalItem.h"

#include <CrySandbox/CrySignal.h>

#include <QVariant>
#include <QString>

namespace Schematyc {

struct IScriptElement;

}

namespace CrySchematycEditor {

class CGraphItem;
class CStateMachineItem;

class CAbstractObjectStructureModel
{
public:
	CAbstractObjectStructureModel() {}
	virtual ~CAbstractObjectStructureModel() {}

	virtual uint32                     GetGraphItemCount() const                                                       { return 0; }
	virtual CGraphItem*                GetGraphItemByIndex(uint32 index) const                                         { return nullptr; }
	virtual CGraphItem*                CreateGraph()                                                                   { return nullptr; }
	virtual bool                       RemoveGraph(CGraphItem& functionItem)                                           { return false; }

	virtual uint32                     GetStateMachineItemCount() const                                                { return 0; }
	virtual CStateMachineItem*         GetStateMachineItemByIndex(uint32 index) const                                  { return nullptr; }
	virtual CStateMachineItem*         CreateStateMachine(Schematyc::EScriptStateMachineLifetime stateMachineLifetime) { return nullptr; }
	virtual bool                       RemoveStateMachine(CStateMachineItem& stateItem)                                { return false; }

	virtual Schematyc::IScriptElement* GetScriptElement() const = 0;

	//
	uint32                             GetNumItems() const;
	CAbstractObjectStructureModelItem* GetChildItemByIndex(uint32 index) const;
	uint32                             GetChildItemIndex(const CAbstractObjectStructureModelItem& item) const;
	bool                               RemoveItem(CAbstractObjectStructureModelItem& item);

public:
	CCrySignal<void(CAbstractObjectStructureModelItem&)> SignalObjectStructureItemAdded;
	CCrySignal<void(CAbstractObjectStructureModelItem&)> SignalObjectStructureItemRemoved;
	CCrySignal<void(CAbstractObjectStructureModelItem&)> SignalObjectStructureItemInvalidated;
};

inline uint32 CAbstractObjectStructureModel::GetNumItems() const
{
	return GetGraphItemCount() + GetStateMachineItemCount();
}

inline CAbstractObjectStructureModelItem* CAbstractObjectStructureModel::GetChildItemByIndex(uint32 index) const
{
	uint32 row = index;
	if (row < GetGraphItemCount())
	{
		return static_cast<CAbstractObjectStructureModelItem*>(GetGraphItemByIndex(row));
	}
	else if (row -= GetGraphItemCount(), row < GetStateMachineItemCount())
	{
		return static_cast<CAbstractObjectStructureModelItem*>(GetStateMachineItemByIndex(row));
	}

	return nullptr;
}

inline uint32 CAbstractObjectStructureModel::GetChildItemIndex(const CAbstractObjectStructureModelItem& item) const
{
	switch (item.GetType())
	{
	case eObjectItemType_Graph:
		{
			const uint32 e = GetGraphItemCount();
			for (uint32 i = 0; i < e; ++i)
			{
				const CGraphItem* pItem = GetGraphItemByIndex(i);
				if (pItem == &item)
					return i;
			}
		}
		break;
	case eObjectItemType_StateMachine:
		{
			const uint32 e = GetStateMachineItemCount();
			for (uint32 i = 0; i < e; ++i)
			{
				const CStateMachineItem* pItem = GetStateMachineItemByIndex(i);
				if (pItem == &item)
					return i + GetGraphItemCount();
			}
		}
		break;
	default:
		break;
	}

	return 0xffffffff;
}

inline bool CAbstractObjectStructureModel::RemoveItem(CAbstractObjectStructureModelItem& item)
{
	switch (item.GetType())
	{
	case eObjectItemType_Graph:
		return RemoveGraph(static_cast<CGraphItem&>(item));
	case eObjectItemType_StateMachine:
		return RemoveStateMachine(static_cast<CStateMachineItem&>(item));
	default:
		break;
	}

	return false;
}

}

