// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StateItem.h"

#include "ObjectModel.h"
#include "ScriptBrowserUtils.h"

#include <CrySchematyc/SerializationUtils/ISerializationContext.h>

#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Script/Elements/IScriptState.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>

#include <VariableItem.h>

#include <CryIcon.h>

#include <QtUtil.h>
#include <QObject>
#include <QMenu>

namespace CrySchematycEditor {

CStateItem::CStateItem(Schematyc::IScriptState& scriptState, CAbstractObjectStructureModel& model)
	: CAbstractObjectStructureModelItem(model)
	, m_scriptState(scriptState)
	, m_pParentItem(nullptr)
{
	LoadFromScriptElement();
}

CStateItem::~CStateItem()
{
	for (CGraphItem* pItem : m_graphs)
		delete pItem;

	for (CStateItem* pItem : m_states)
		delete pItem;
}

void CStateItem::SetName(QString name)
{
	Schematyc::CStackString uniqueName = QtUtil::ToString(name).c_str();
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(uniqueName, m_model.GetScriptElement());

	m_scriptState.SetName(uniqueName);
	m_name = m_scriptState.GetName();
}

const CryIcon*  CStateItem::GetIcon() const
{
	std::unique_ptr<CryIcon> pIcon;
	if (pIcon.get() == nullptr)
	{
		pIcon = stl::make_unique<CryIcon>("icons:schematyc/script_state.png");
	}

	return pIcon.get();
}

uint32 CStateItem::GetNumChildItems() const
{
	return m_graphs.size() + m_states.size();
}

CAbstractObjectStructureModelItem* CStateItem::GetChildItemByIndex(uint32 index) const
{
	if (index < m_graphs.size())
		return m_graphs[index];

	index -= m_graphs.size();

	if (index < m_states.size())
		return m_states[index];

	return nullptr;
}

uint32 CStateItem::GetIndex() const
{
	CRY_ASSERT(m_pParentItem);
	if (m_pParentItem)
	{
		return m_pParentItem->GetChildItemIndex(*this);
	}

	return 0xffffffff;
}

void CStateItem::Serialize(Serialization::IArchive& archive)
{
	// TODO: This will only work for serialization to properties in inspector!
	Schematyc::SSerializationContextParams serParams(archive, Schematyc::ESerializationPass::Edit);
	Schematyc::ISerializationContextPtr pSerializationContext = gEnv->pSchematyc->CreateSerializationContext(serParams);
	// ~TODO

	m_scriptState.Serialize(archive);
}

bool CStateItem::AllowsRenaming() const
{
	const bool allowsRenaming = !m_scriptState.GetFlags().Check(Schematyc::EScriptElementFlags::FixedName);
	return allowsRenaming;
}

uint32 CStateItem::GetChildItemIndex(const CAbstractObjectStructureModelItem& item) const
{
	switch (item.GetType())
	{
	case eObjectItemType_Graph:
		{
			uint32 index = 0;
			for (const CGraphItem* pItem : m_graphs)
			{
				if (pItem == &item)
					return index;
				++index;
			}
		}
		break;
	case eObjectItemType_State:
		{
			uint32 index = 0;
			for (const CStateItem* pItem : m_states)
			{
				if (pItem == &item)
					return index + m_graphs.size();
				++index;
			}
		}
		break;
	default:
		break;
	}

	return 0xffffffff;
}

CGraphItem* CStateItem::CreateGraph(CGraphItem::EGraphType type)
{
	CGraphItem* pGraphItem = nullptr;
	switch (type)
	{
	case CGraphItem::eGraphType_SignalReceiver:
		{
			Schematyc::IScriptSignalReceiver* pSignalReceiverElement = Schematyc::ScriptBrowserUtils::AddScriptSignalReceiver(m_model.GetScriptElement());
			if (pSignalReceiverElement)
			{
				pGraphItem = new CGraphItem(*pSignalReceiverElement, m_model);
			}
		}
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "Invalid graph type.");
		break;
	}

	if (pGraphItem)
	{
		pGraphItem->SetParentItem(this);

		m_graphs.push_back(pGraphItem);
		GetModel().SignalObjectStructureItemAdded(*pGraphItem);

		return pGraphItem;
	}

	return nullptr;
}

bool CStateItem::RemoveGraph(CGraphItem& functionItem)
{
	GetModel().SignalObjectStructureItemRemoved(functionItem);
	gEnv->pSchematyc->GetScriptRegistry().RemoveElement(functionItem.GetGUID());

	const auto result = std::find(m_graphs.begin(), m_graphs.end(), &functionItem);
	if (result != m_graphs.end())
	{
		m_graphs.erase(result);
	}

	return true;
}

CStateItem* CStateItem::CreateState()
{
	Schematyc::IScriptState* pStateElement = Schematyc::ScriptBrowserUtils::AddScriptState(&m_scriptState);
	if (pStateElement)
	{
		CStateItem* pStateItem = new CStateItem(*pStateElement, m_model);
		pStateItem->SetParentItem(this);

		m_states.push_back(pStateItem);
		GetModel().SignalObjectStructureItemAdded(*pStateItem);

		return pStateItem;
	}

	return nullptr;
}

bool CStateItem::RemoveState(CStateItem& stateItem)
{
	GetModel().SignalObjectStructureItemRemoved(stateItem);
	gEnv->pSchematyc->GetScriptRegistry().RemoveElement(stateItem.GetGUID());

	const auto result = std::find(m_states.begin(), m_states.end(), &stateItem);
	if (result != m_states.end())
	{
		m_states.erase(result);
	}

	return true;
}

CryGUID CStateItem::GetGUID() const
{
	return m_scriptState.GetGUID();
}

void CStateItem::LoadFromScriptElement()
{
	m_name = m_scriptState.GetName();

	Schematyc::IScriptElement* pElement = m_scriptState.GetFirstChild();
	while (pElement)
	{
		const Schematyc::EScriptElementType elementType = pElement->GetType();
		switch (elementType)
		{
		case Schematyc::EScriptElementType::Function:
			{
				Schematyc::IScriptFunction& scriptFunction = static_cast<Schematyc::IScriptFunction&>(*pElement);
				CGraphItem* pFunctionItem = new CGraphItem(scriptFunction, m_model);
				pFunctionItem->SetParentItem(this);
				m_graphs.push_back(pFunctionItem);
			}
			break;
		case Schematyc::EScriptElementType::SignalReceiver:
			{
				Schematyc::IScriptSignalReceiver& scriptSignalReceiver = static_cast<Schematyc::IScriptSignalReceiver&>(*pElement);
				CGraphItem* pFunctionItem = new CGraphItem(scriptSignalReceiver, m_model);
				pFunctionItem->SetParentItem(this);
				m_graphs.push_back(pFunctionItem);
			}
			break;
		case Schematyc::EScriptElementType::State:
			{
				Schematyc::IScriptState& scriptState = static_cast<Schematyc::IScriptState&>(*pElement);
				CStateItem* pStateItem = new CStateItem(scriptState, m_model);
				pStateItem->SetParentItem(this);
				m_states.push_back(pStateItem);
			}
			break;
		case Schematyc::EScriptElementType::Variable:
			{
				//Schematyc::IScriptVariable& scriptVariable = static_cast<Schematyc::IScriptVariable&>(*pElement);
				//CVariableItem* pVariableItem = new CVariableItem(scriptVariable, *this);
				//m_variables.push_back(pVariableItem);
			}
			break;
		default:
			break;     // Something unexpected!
		}

		pElement = pElement->GetNextSibling();
	}
}

}

