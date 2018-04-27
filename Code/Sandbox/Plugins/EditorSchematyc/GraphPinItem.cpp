// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GraphPinItem.h"

#include "GraphNodeItem.h"
#include "GraphViewModel.h"

#include "VariableStorage/AbstractVariableTypesModel.h"

#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Reflection/TypeDesc.h>

#include <NodeGraph/PinWidget.h>
#include <NodeGraph/NodeGraphViewStyle.h>

#include <QColor>

#include <QtUtil.h>

namespace CrySchematycEditor {

CPinItem::CPinItem(uint32 index, uint32 flags, CNodeItem& nodeItem, CryGraphEditor::CNodeGraphViewModel& model)
	: CAbstractPinItem(model)
	, m_flags(flags)
	, m_nodeItem(nodeItem)
{
	UpdateWithNewIndex(index);
}

CPinItem::~CPinItem()
{

}

CryGraphEditor::CPinWidget* CPinItem::CreateWidget(CryGraphEditor::CNodeWidget& nodeWidget, CryGraphEditor::CNodeGraphView& view)
{
	CryGraphEditor::CPinWidget* pPinWidget = new CryGraphEditor::CPinWidget(*this, nodeWidget, view, true);
	return pPinWidget;
}

CryGraphEditor::CAbstractNodeItem& CPinItem::GetNodeItem() const
{
	return static_cast<CryGraphEditor::CAbstractNodeItem&>(m_nodeItem);
}

QString CPinItem::GetTypeName() const
{
	return m_pDataTypeItem->GetName();
}

QVariant CPinItem::GetId() const
{
	return QVariant::fromValue(m_id);
}

bool CPinItem::HasId(QVariant id) const
{
	const CPinId otherId = id.value<CPinId>();
	return (otherId == m_id);
}

bool CPinItem::CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const
{
	if (pOtherPin)
	{
		if (&pOtherPin->GetNodeItem() != &GetNodeItem())
		{
			CNodeGraphViewModel& model = static_cast<CNodeGraphViewModel&>(GetViewModel());
			Schematyc::IScriptGraph& scriptGraph = model.GetScriptGraph();

			if (IsOutputPin())
			{
				if (pOtherPin->IsInputPin())
				{
					const CryGUID sourceGuid = m_nodeItem.GetGUID();
					const Schematyc::CUniqueId sourceId = GetPortId();

					const CryGUID targetGuid = static_cast<CNodeItem&>(pOtherPin->GetNodeItem()).GetGUID();
					const Schematyc::CUniqueId targetId = static_cast<const CPinItem*>(pOtherPin)->GetPortId();

					return scriptGraph.CanAddLink(sourceGuid, sourceId, targetGuid, targetId);
				}
			}
			else
			{
				if (pOtherPin->IsOutputPin())
				{
					const CryGUID sourceGuid = static_cast<CNodeItem&>(pOtherPin->GetNodeItem()).GetGUID();
					const Schematyc::CUniqueId sourceId = static_cast<const CPinItem*>(pOtherPin)->GetPortId();

					const CryGUID targetGuid = m_nodeItem.GetGUID();
					const Schematyc::CUniqueId targetId = GetPortId();

					return scriptGraph.CanAddLink(sourceGuid, sourceId, targetGuid, targetId);
				}
			}
		}
	}
	else
	{
		if (m_pinType == EPinType::Data || !IsConnected())
		{
			return true;
		}
	}

	return false;
}

Schematyc::CUniqueId CPinItem::GetPortId() const
{
	if (IsInputPin())
		return m_nodeItem.GetScriptElement().GetInputId(m_index);
	else
		return m_nodeItem.GetScriptElement().GetOutputId(m_index);
}

void CPinItem::UpdateWithNewIndex(uint32 index)
{
	CRY_ASSERT(m_index <= USHRT_MAX);
	m_index = index;

	Schematyc::IScriptGraphNode& scriptNode = m_nodeItem.GetScriptElement();
	Schematyc::ScriptGraphPortFlags::UnderlyingType portFlags = 0;
	if (IsInputPin())
	{
		m_name = scriptNode.GetInputName(index);
		m_pDataTypeItem = CDataTypesModel::GetInstance().GetTypeItemByGuid(scriptNode.GetInputTypeGUID(index));

		portFlags = m_nodeItem.GetScriptElement().GetInputFlags(index).UnderlyingValue();
		m_id = CPinId(m_nodeItem.GetScriptElement().GetInputId(index), CPinId::EType::Input);
	}
	else if (IsOutputPin())
	{
		m_name = scriptNode.GetOutputName(index);
		m_pDataTypeItem = CDataTypesModel::GetInstance().GetTypeItemByGuid(scriptNode.GetOutputTypeGUID(index));

		portFlags = m_nodeItem.GetScriptElement().GetOutputFlags(index).UnderlyingValue();
		m_id = CPinId(m_nodeItem.GetScriptElement().GetOutputId(index), CPinId::EType::Output);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Pin must be either of type input or output.");
	}

	if (m_pDataTypeItem == nullptr)
	{
		m_pDataTypeItem = &CDataTypeItem::Empty();
	}

	if (portFlags & static_cast<Schematyc::ScriptGraphPortFlags::UnderlyingType>(Schematyc::EScriptGraphPortFlags::Signal))
	{
		m_pinType = EPinType::Signal;
	}
	else if (portFlags & static_cast<Schematyc::ScriptGraphPortFlags::UnderlyingType>(Schematyc::EScriptGraphPortFlags::Flow))
	{
		m_pinType = EPinType::Execution;
	}
	else
	{
		m_pinType = EPinType::Data;
	}

	switch (m_pinType)
	{
	case EPinType::Signal:
		{
			m_styleId = "Pin::Signal";
		}
		break;
	case EPinType::Execution:
		{
			m_styleId = "Pin::Execution";
		}
		break;
	case EPinType::Data:
		{
			if (*m_pDataTypeItem != CDataTypeItem::Empty())
			{
				m_styleId = "Pin::";
				m_styleId.append(QtUtil::ToString(m_pDataTypeItem->GetName()).c_str());
			}
			else
				m_styleId = "Pin";
		}
		break;
	}

	//SignalInvalidated();
}

}

