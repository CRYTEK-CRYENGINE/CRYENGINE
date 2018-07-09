// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NodeGraphRuntimeContext.h"

#include "GraphNodeItem.h"
#include "GraphViewWidget.h"

#include <NodeGraph/NodeGraphViewStyle.h>
#include <CrySchematyc/Script/IScriptGraph.h>

// TODO: Replace when CNodeStyle was moved into its own header.
#include "GraphNodeItem.h"

#include "VariableStorage/AbstractVariableTypesModel.h"

#include "NodeGraph/NodeWidgetStyle.h"
#include "NodeGraph/NodeHeaderWidgetStyle.h"
#include "NodeGraph/NodeGraphViewStyle.h"
#include "NodeGraph/ConnectionWidgetStyle.h"
#include "NodeGraph/NodePinWidgetStyle.h"

#include <QtUtil.h>
// ~TODO

#include <QStringList>

namespace CrySchematycEditor {

const CItemModelAttribute CNodesDictionary::s_columnAttributes[] =
{
	CItemModelAttribute("Name",            eAttributeType_String, CItemModelAttribute::Visible,      false, ""),
	CItemModelAttribute("_filter_string_", eAttributeType_String, CItemModelAttribute::AlwaysHidden, false, ""),
	// TODO: This should be a guid string later.
	CItemModelAttribute("_identifier_",    eAttributeType_Int,    CItemModelAttribute::AlwaysHidden, false, "")
	// ~TODO
};

QVariant CNodesDictionaryNodeEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CNodesDictionary::eColumn_Name:
		{
			return QVariant::fromValue(m_name);
		}
	case CNodesDictionary::eColumn_Filter:
		{
			return QVariant::fromValue(m_fullName);
		}
	case CNodesDictionary::eColumn_Identifier:
		{
			return GetIdentifier();
		}
	default:
		break;
	}

	return QVariant();
}

const QIcon* CNodesDictionaryNodeEntry::GetColumnIcon(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CNodesDictionary::eColumn_Name:
		{
			return m_pIcon;
		}
	case CNodesDictionary::eColumn_Filter:
	case CNodesDictionary::eColumn_Identifier:
	default:
		break;
	}

	return nullptr;
}

const CAbstractDictionaryEntry* CNodesDictionaryNodeEntry::GetParentEntry() const
{
	return static_cast<const CAbstractDictionaryEntry*>(m_pParent);
}

QVariant CNodesDictionaryNodeEntry::GetIdentifier() const
{
	// TODO: This should be just a guid.
	return QVariant::fromValue(reinterpret_cast<quintptr>(m_pCommand.get()));
	// ~TODO
}

CNodesDictionaryCategoryEntry::~CNodesDictionaryCategoryEntry()
{
	for (CNodesDictionaryCategoryEntry* pCategoryEntry : m_categories)
	{
		delete pCategoryEntry;
	}
	m_categories.clear();

	for (CNodesDictionaryNodeEntry* pNodeEntry : m_nodes)
	{
		delete pNodeEntry;
	}
	m_nodes.clear();
}

QVariant CNodesDictionaryCategoryEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CNodesDictionary::eColumn_Name:
		{
			return QVariant::fromValue(m_name);
		}
	case CNodesDictionary::eColumn_Filter:
		{
			return QVariant::fromValue(m_fullName);
		}
	case CNodesDictionary::eColumn_Identifier:
	default:
		break;
	}

	return QVariant();
}

const CAbstractDictionaryEntry* CNodesDictionaryCategoryEntry::GetChildEntry(int32 index) const
{
	if (index < m_categories.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_categories[index]);
	}
	const int32 nodeIndex = index - m_categories.size();
	if (nodeIndex < m_nodes.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_nodes[nodeIndex]);
	}
	return nullptr;
}

const CAbstractDictionaryEntry* CNodesDictionaryCategoryEntry::GetParentEntry() const
{
	return static_cast<const CAbstractDictionaryEntry*>(m_pParent);
}

CNodesDictionary::CNodesDictionary()
	: m_pStyle(nullptr)
{

}

CNodesDictionary::~CNodesDictionary()
{
	Clear();
}

void CNodesDictionary::ClearEntries()
{
	for (CNodesDictionaryCategoryEntry* pCategoryEntry : m_categories)
	{
		delete pCategoryEntry;
	}
	m_categories.clear();

	for (CNodesDictionaryNodeEntry* pNodeEntry : m_nodes)
	{
		delete pNodeEntry;
	}
	m_nodes.clear();
}

const CAbstractDictionaryEntry* CNodesDictionary::GetEntry(int32 index) const
{
	if (index < m_categories.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_categories[index]);
	}
	const int32 nodeIndex = index - m_categories.size();
	if (nodeIndex < m_nodes.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_nodes[nodeIndex]);
	}
	return nullptr;
}

const CItemModelAttribute* CNodesDictionary::GetColumnAttribute(int32 index) const
{
	if (index < eColumn_COUNT)
	{
		return &s_columnAttributes[index];
	}
	return nullptr;
}

class CNodesDictionaryCreator : public Schematyc::IScriptGraphNodeCreationMenu
{
public:
	CNodesDictionaryCreator(CNodesDictionary& dictionary)
		: m_dictionary(dictionary)
	{}

	virtual bool AddCommand(const Schematyc::IScriptGraphNodeCreationCommandPtr& pCommand)
	{
		SCHEMATYC_EDITOR_ASSERT(pCommand);
		if (pCommand)
		{
			const char* szBehavior = pCommand->GetBehavior();
			const char* szSubject = pCommand->GetSubject();
			const char* szDescription = pCommand->GetDescription();
			const char* szStyleId = pCommand->GetStyleId();

			m_fullName = szBehavior;
			if (szSubject && (szSubject[0] != '\0'))
			{
				m_fullName.append("::");
				m_fullName.append(szSubject);
			}

			QStringList categories = m_fullName.split("::");
			const QString name = categories.back();
			categories.removeLast();

			m_ppCommand = &pCommand;
			AddRecursive(name, categories, nullptr, szStyleId);

			return true;
		}
		return false;
	}

	void AddRecursive(const QString& name, QStringList& categories, CNodesDictionaryCategoryEntry* pParentCategory, const char* szStyleId = nullptr)
	{
		const CryGraphEditor::CNodeGraphViewStyle* pStyle = m_dictionary.m_pStyle;

		if (categories.size() > 0)
		{
			std::vector<CNodesDictionaryCategoryEntry*>* pCategoryItems = pParentCategory ? &pParentCategory->m_categories : &m_dictionary.m_categories;

			for (CNodesDictionaryCategoryEntry* pCategoryItem : * pCategoryItems)
			{
				if (pCategoryItem->GetName() == categories.front())
				{
					categories.removeFirst();
					AddRecursive(name, categories, pCategoryItem, szStyleId);
					return;
				}
			}

			const QIcon* pIcon = nullptr;
			if (pStyle)
			{
				if (const CryGraphEditor::CNodeWidgetStyle* pNodeStyle = pStyle->GetNodeWidgetStyle(szStyleId))
				{
					pIcon = &pNodeStyle->GetMenuIcon();
				}
			}

			CNodesDictionaryCategoryEntry* pNewCategory = nullptr;
			for (const QString& categoryName : categories)
			{
				pNewCategory = new CNodesDictionaryCategoryEntry(categoryName, pParentCategory);
				pCategoryItems->push_back(pNewCategory);
				pCategoryItems = &pNewCategory->m_categories;
				pParentCategory = pNewCategory;
			}

			if (pNewCategory)
			{
				CNodesDictionaryNodeEntry* pNode = new CNodesDictionaryNodeEntry(name, m_fullName, *m_ppCommand, pNewCategory, pIcon);
				pNewCategory->m_nodes.push_back(pNode);
			}
		}
		else
		{
			const QIcon* pIcon = nullptr;
			if (pStyle)
			{
				if (const CryGraphEditor::CNodeWidgetStyle* pNodeStyle = pStyle->GetNodeWidgetStyle(szStyleId))
				{
					pIcon = &pNodeStyle->GetMenuIcon();
				}
			}

			CNodesDictionaryNodeEntry* pNewNodeItem = new CNodesDictionaryNodeEntry(name, m_fullName, *m_ppCommand, pParentCategory, pIcon);
			if (pParentCategory)
			{
				pParentCategory->m_nodes.push_back(pNewNodeItem);
			}
			else
			{
				m_dictionary.m_nodes.push_back(pNewNodeItem);
			}
		}
	}

private:
	CNodesDictionary& m_dictionary;
	QString           m_fullName;

	const Schematyc::IScriptGraphNodeCreationCommandPtr* m_ppCommand;
};

void CNodesDictionary::LoadLoadsFromScriptGraph(Schematyc::IScriptGraph& scriptGraph)
{
	CNodesDictionaryCreator creator(*this);
	scriptGraph.PopulateNodeCreationMenu(creator);
}

void AddNodeStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, const char* szIcon, QColor color, bool coloredHeaderIconText = true)
{
	CryGraphEditor::CNodeWidgetStyle* pStyle = new CryGraphEditor::CNodeWidgetStyle(szStyleId, viewStyle);
	CryGraphEditor::CNodeHeaderWidgetStyle& headerStyle = pStyle->GetHeaderWidgetStyle();

	headerStyle.SetNodeIconMenuColor(color);

	if (coloredHeaderIconText)
	{
		headerStyle.SetNameColor(color);
		headerStyle.SetLeftColor(QColor(26, 26, 26));
		headerStyle.SetRightColor(QColor(26, 26, 26));
		headerStyle.SetNodeIconViewDefaultColor(color);

		CryIcon icon(szIcon, {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
		  });
		headerStyle.SetNodeIcon(icon);
	}
	else
	{
		headerStyle.SetNameColor(QColor(26, 26, 26));
		headerStyle.SetLeftColor(color);
		headerStyle.SetRightColor(color);
		headerStyle.SetNodeIconViewDefaultColor(QColor(26, 26, 26));

		CryIcon icon(szIcon, {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
		  });
		headerStyle.SetNodeIcon(icon);
	}
}

void AddConnectionStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, float width)
{
	CryGraphEditor::CConnectionWidgetStyle* pStyle = new CryGraphEditor::CConnectionWidgetStyle(szStyleId, viewStyle);
	pStyle->SetWidth(width);
}

void AddPinStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, const char* szIcon, QColor color)
{
	CryIcon icon(szIcon, {
			{ QIcon::Mode::Normal, color }
	  });

	CryGraphEditor::CNodePinWidgetStyle* pStyle = new CryGraphEditor::CNodePinWidgetStyle(szStyleId, viewStyle);
	pStyle->SetIcon(icon);
	pStyle->SetColor(color);
}

CryGraphEditor::CNodeGraphViewStyle* CreateStyle()
{
	CryGraphEditor::CNodeGraphViewStyle* pViewStyle = new CryGraphEditor::CNodeGraphViewStyle("Schematyc");

	AddNodeStyle(*pViewStyle, "Node", "icons:schematyc/node_default.ico", QColor(255, 0, 0));
	// TODO: We should use something like 'Node::Core::FlowControl' for nodes.
	AddNodeStyle(*pViewStyle, "Core::FlowControl::Begin", "icons:schematyc/core_flowcontrol_begin.ico", QColor(97, 172, 236), false);
	AddNodeStyle(*pViewStyle, "Core::FlowControl::End", "icons:schematyc/core_flowcontrol_end.ico", QColor(97, 172, 236), false);
	AddNodeStyle(*pViewStyle, "Core::FlowControl", "icons:schematyc/core_flowcontrol.ico", QColor(255, 255, 255));
	AddNodeStyle(*pViewStyle, "Core::SendSignal", "icons:schematyc/core_sendsignal.ico", QColor(100, 193, 98));
	AddNodeStyle(*pViewStyle, "Core::ReceiveSignal", "icons:schematyc/core_receivesignal.ico", QColor(100, 193, 98));
	AddNodeStyle(*pViewStyle, "Core::Function", "icons:schematyc/core_function.ico", QColor(193, 98, 98));
	AddNodeStyle(*pViewStyle, "Core::Data", "icons:schematyc/core_data.ico", QColor(156, 98, 193));
	AddNodeStyle(*pViewStyle, "Core::Utility", "icons:schematyc/core_utility.ico", QColor(153, 153, 153));
	AddNodeStyle(*pViewStyle, "Core::State", "icons:schematyc/core_state.ico", QColor(192, 193, 98));
	AddNodeStyle(*pViewStyle, "Node::FlowGraph", "icons:schematyc/node_flow_graph.png", QColor(98, 98, 236));
	// ~TODO

	AddConnectionStyle(*pViewStyle, "Connection::Data", 2.0);
	AddConnectionStyle(*pViewStyle, "Connection::Execution", 3.0);

	//AddConnectionStyle(*pViewStyle, "Connection", 2.0);

	//AddPinStyle(*pViewStyle, "Pin", "icons:Graph/Node_connection_arrow_R.ico", QColor(200, 200, 200));

	CDataTypesModel& dataTypesModel = CDataTypesModel::GetInstance();
	for (uint32 i = 0; i < dataTypesModel.GetTypeItemsCount(); ++i)
	{
		CDataTypeItem* pDataType = dataTypesModel.GetTypeItemByIndex(i);
		if (pDataType)
		{
			stack_string styleId = "Pin::";
			styleId.append(QtUtil::ToString(pDataType->GetName()).c_str());

			AddPinStyle(*pViewStyle, styleId.c_str(), "icons:Graph/Node_connection_circle.ico", pDataType->GetColor());
		}
	}

	AddPinStyle(*pViewStyle, "Pin::Execution", "icons:Graph/Node_connection_arrow_R.ico", QColor(200, 200, 200));
	AddPinStyle(*pViewStyle, "Pin::Signal", "icons:Graph/Node_connection_arrow_R.ico", QColor(200, 200, 200));

	return pViewStyle;
}

CNodeGraphRuntimeContext::CNodeGraphRuntimeContext(Schematyc::IScriptGraph& scriptGraph)
	: m_scriptGraph(scriptGraph)
{
	m_pStyle = CreateStyle();
	m_nodesDictionary.SetStyle(m_pStyle);
}

CNodeGraphRuntimeContext::~CNodeGraphRuntimeContext()
{
	m_pStyle->deleteLater();
}

CAbstractDictionary* CNodeGraphRuntimeContext::GetAvailableNodesDictionary()
{
	m_nodesDictionary.Clear();
	m_nodesDictionary.LoadLoadsFromScriptGraph(m_scriptGraph);
	return &m_nodesDictionary;
}

}

