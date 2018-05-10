// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectModel.h"

#include "VariableItem.h"
#include "GraphItem.h"
#include "StateMachineItem.h"

#include "ScriptBrowserUtils.h"

#include <CrySchematyc/Reflection/TypeDesc.h>

#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Script/Elements/IScriptConstructor.h>
#include <CrySchematyc/Script/Elements/IScriptStateMachine.h>
#include <CrySchematyc/Script/Elements/IScriptInterface.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvElement.h>

namespace CrySchematycEditor {

CObjectModel::CObjectModel(Schematyc::IScriptView& scriptView, EResourceType resourceType)
	: m_scriptView(scriptView)
	, m_resourceType(resourceType)
	, m_scriptRegistry(gEnv->pSchematyc->GetScriptRegistry())
	, m_scriptElement(*gEnv->pSchematyc->GetScriptRegistry().GetElement(scriptView.GetScopeGUID()))
{
	LoadFromScriptElement();

	if (Schematyc::ScriptBrowserUtils::CanAddScriptElement(Schematyc::EScriptElementType::ComponentInstance, &m_scriptElement))
	{
		m_componentsDictionary.Load(&m_scriptElement);
	}
}

CObjectModel::~CObjectModel()
{

}

CComponentItem* CObjectModel::GetComponentItemByIndex(uint32 index) const
{
	if (index < m_components.size())
	{
		return m_components[index];
	}
	return nullptr;
}

CComponentItem* CObjectModel::CreateComponent(CryGUID typeId, const char* szName)
{
	Schematyc::IScriptComponentInstance* pComponentInstance = m_scriptRegistry.AddComponentInstance(szName, typeId, &m_scriptElement);
	if (pComponentInstance)
	{
		CComponentItem* pComponentItem = new CComponentItem(*pComponentInstance, *this);
		m_components.push_back(pComponentItem);

		SignalAddedComponentItem(*pComponentItem);
		return pComponentItem;
	}

	return nullptr;
}

bool CObjectModel::RemoveComponent(CComponentItem& component)
{
	SignalRemovedComponentItem(component);
	m_scriptRegistry.RemoveElement(component.GetInstance().GetGUID());

	const Components::iterator result = std::find(m_components.begin(), m_components.end(), &component);
	if (result != m_components.end())
	{
		m_components.erase(result);
	}

	return true;
}

CGraphItem* CObjectModel::GetGraphItemByIndex(uint32 index) const
{
	if (index < m_graphs.size())
	{
		return m_graphs[index];
	}
	return nullptr;
}

CGraphItem* CObjectModel::CreateGraph()
{
	Schematyc::CStackString name = "Function";
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(name, &m_scriptElement);

	Schematyc::IScriptFunction* pFunctionElement = m_scriptRegistry.AddFunction(name, &m_scriptElement);
	if (pFunctionElement)
	{
		CGraphItem* pFunctionItem = new CGraphItem(*pFunctionElement, *this);
		m_graphs.push_back(pFunctionItem);

		SignalObjectStructureItemAdded(*pFunctionItem);
		return pFunctionItem;
	}

	return nullptr;
}

bool CObjectModel::RemoveGraph(CGraphItem& functionItem)
{
	SignalObjectStructureItemRemoved(functionItem);
	m_scriptRegistry.RemoveElement(functionItem.GetGUID());

	const Graphs::iterator result = std::find(m_graphs.begin(), m_graphs.end(), &functionItem);
	if (result != m_graphs.end())
	{
		m_graphs.erase(result);
	}

	return true;
}

CStateMachineItem* CObjectModel::GetStateMachineItemByIndex(uint32 index) const
{
	if (index < m_stateMachines.size())
	{
		return m_stateMachines[index];
	}
	return nullptr;
}

CStateMachineItem* CObjectModel::CreateStateMachine(Schematyc::EScriptStateMachineLifetime stateMachineLifetime)
{
	Schematyc::CStackString name = "StateMachine";
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(name, &m_scriptElement);

	Schematyc::IScriptStateMachine* pStateMachineElement = nullptr;//m_scriptRegistry.AddStateMachine(name, stateMachineLifetime, &m_scriptElement);
	if (pStateMachineElement)
	{
		CStateMachineItem* pStateMachineItem = new CStateMachineItem(*pStateMachineElement, *this);
		m_stateMachines.push_back(pStateMachineItem);

		SignalObjectStructureItemAdded(*pStateMachineItem);
		return pStateMachineItem;
	}

	return nullptr;
}

bool CObjectModel::RemoveStateMachine(CStateMachineItem& stateItem)
{
	SignalObjectStructureItemRemoved(stateItem);
	m_scriptRegistry.RemoveElement(stateItem.GetGUID());

	auto result = std::find(m_stateMachines.begin(), m_stateMachines.end(), &stateItem);
	if (result != m_stateMachines.end())
	{
		m_stateMachines.erase(result);
	}

	return true;
}

CAbstractVariablesModelItem* CObjectModel::GetVariableItemByIndex(uint32 index)
{
	if (index < m_variables.size())
	{
		return m_variables[index];
	}
	return nullptr;
}

uint32 CObjectModel::GetVariableItemIndex(const CAbstractVariablesModelItem& variableItem) const
{
	uint32 index = 0;
	for (const CVariableItem* pItem : m_variables)
	{
		if (pItem == &variableItem)
		{
			return index;
		}
		++index;
	}

	return 0xffffffff;
}

CAbstractVariablesModelItem* CObjectModel::CreateVariable()
{
	Schematyc::CStackString uniqueName = "Variable";
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(uniqueName, &m_scriptElement);

	Schematyc::IScriptVariable* pVariableElement = m_scriptRegistry.AddVariable(uniqueName, Schematyc::SElementId(), CryGUID(), &m_scriptElement);
	if (pVariableElement)
	{
		CVariableItem* pVariableItem = new CVariableItem(*pVariableElement, *this);
		m_variables.push_back(pVariableItem);

		SignalVariableAdded(*pVariableItem);
		return static_cast<CAbstractVariablesModelItem*>(pVariableItem);
	}

	return nullptr;
}

bool CObjectModel::RemoveVariable(CAbstractVariablesModelItem& variableItem)
{
	SignalVariableRemoved(variableItem);

	CVariableItem* pVariableItem = static_cast<CVariableItem*>(&variableItem);
	m_scriptRegistry.RemoveElement(pVariableItem->GetGUID());

	const Variables::iterator result = std::find(m_variables.begin(), m_variables.end(), pVariableItem);
	if (result != m_variables.end())
	{
		m_variables.erase(result);
	}

	return true;
}

void CObjectModel::LoadFromScriptElement()
{
	Schematyc::IScriptElement* pElement = m_scriptElement.GetFirstChild();
	while (pElement)
	{
		const Schematyc::EScriptElementType elementType = pElement->GetType();
		switch (elementType)
		{
		case Schematyc::EScriptElementType::ComponentInstance:
			{
				Schematyc::IScriptComponentInstance& scriptElement = static_cast<Schematyc::IScriptComponentInstance&>(*pElement);
				CComponentItem* pComponentItem = new CComponentItem(scriptElement, *this);
				m_components.push_back(pComponentItem);
			}
			break;
		case Schematyc::EScriptElementType::Function:
			{
				Schematyc::IScriptFunction& scriptElement = static_cast<Schematyc::IScriptFunction&>(*pElement);
				CGraphItem* pFunctionItem = new CGraphItem(scriptElement, *this);
				m_graphs.push_back(pFunctionItem);
			}
			break;
		case Schematyc::EScriptElementType::Constructor:
			{
				Schematyc::IScriptConstructor& scriptElement = static_cast<Schematyc::IScriptConstructor&>(*pElement);
				CGraphItem* pFunctionItem = new CGraphItem(scriptElement, *this);
				m_graphs.push_back(pFunctionItem);
			}
			break;
		case Schematyc::EScriptElementType::SignalReceiver:
			{
				Schematyc::IScriptSignalReceiver& scriptSignalReceiver = static_cast<Schematyc::IScriptSignalReceiver&>(*pElement);
				CGraphItem* pFunctionItem = new CGraphItem(scriptSignalReceiver, *this);
				m_graphs.push_back(pFunctionItem);
			}
			break;
		case Schematyc::EScriptElementType::StateMachine:
			{
				Schematyc::IScriptStateMachine& scriptElement = static_cast<Schematyc::IScriptStateMachine&>(*pElement);
				CStateMachineItem* pStateMachineItem = new CStateMachineItem(scriptElement, *this);
				m_stateMachines.push_back(pStateMachineItem);
			}
			break;
		case Schematyc::EScriptElementType::Interface:
			{
				/*Schematyc::IScriptInterface& scriptElement = static_cast<Schematyc::IScriptInterface&>(*pElement);
				   CInterfaceItem* pStateMachineItem = new CInterfaceItem(scriptElement, *this);
				   m_interfaces.push_back(pStateMachineItem);*/
			}
			break;
		case Schematyc::EScriptElementType::Variable:
			{
				Schematyc::IScriptVariable& scriptVariable = static_cast<Schematyc::IScriptVariable&>(*pElement);
				CVariableItem* pVariableItem = new CVariableItem(scriptVariable, *this);
				m_variables.push_back(pVariableItem);
			}
			break;
		case Schematyc::EScriptElementType::Struct:
			{
				// TODO
			}
			break;
		case Schematyc::EScriptElementType::Enum:
			{
				// TODO
			}
			break;
		default:
			break;   // Something unexpected!
		}

		pElement = pElement->GetNextSibling();
	}
}

}

