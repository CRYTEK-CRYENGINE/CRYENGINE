// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeGraphUndo.h"

#include "AbstractNodeGraphViewModel.h"
#include "AbstractNodeItem.h"
#include "AbstractConnectionItem.h"
#include "AbstractPinItem.h"

#include <CrySerialization/IArchiveHost.h>
#include <QtUtil.h>

namespace CryGraphEditor
{

CUndoObject::CUndoObject(CNodeGraphViewModel* pModel)
	: m_pModel(pModel)
{
	pModel->SignalDestruction.Connect(this, &CUndoObject::OnModelDestruction);
}

CUndoObject::~CUndoObject()
{
	if (m_pModel)
	{
		m_pModel->SignalDestruction.DisconnectObject(this);
	}
}

CUndoNodeMove::CUndoNodeMove(CAbstractNodeItem& node, const string& description)
	: CUndoObject(&node.GetViewModel())
{
	m_nodeId = node.GetId();
	m_position = node.GetPosition();

	if (description.empty())
	{
		string name = QtUtil::ToString(node.GetName());
		m_description.Format("Undo move of node '%s'", name.c_str());
	}
	else
		m_description = description;
}

void CUndoNodeMove::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			QPointF position = pNodeItem->GetPosition();
			pNodeItem->SetPosition(m_position);
			m_position = position;
		}
	}
}

void CUndoNodeMove::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			QPointF position = pNodeItem->GetPosition();
			pNodeItem->SetPosition(m_position);
			m_position = position;
		}
	}
}

CUndoNodePropertiesChange::CUndoNodePropertiesChange(CAbstractNodeItem& node, const string& description)
	: CUndoObject(&node.GetViewModel())
{
	m_nodeId = node.GetId();
	Serialization::SaveJsonBuffer(m_nodeData, node);
	m_nodeData.push_back('\0');

	if (description.empty())
	{
		string name = QtUtil::ToString(node.GetName());
		m_description.Format("Undo properties change of node '%s'", name.c_str());
	}
	else
		m_description = description;
}

void CUndoNodePropertiesChange::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			DynArray<char> nodeData;
			Serialization::SaveJsonBuffer(nodeData, *pNodeItem);
			nodeData.push_back('\0');

			Serialization::LoadJsonBuffer(*pNodeItem, m_nodeData.data(), m_nodeData.size());

			m_nodeData.swap(nodeData);
		}
	}
}

void CUndoNodePropertiesChange::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			DynArray<char> nodeData;
			Serialization::SaveJsonBuffer(nodeData, *pNodeItem);
			nodeData.push_back('\0');

			Serialization::LoadJsonBuffer(*pNodeItem, m_nodeData.data(), m_nodeData.size());

			m_nodeData.swap(nodeData);
		}
	}
}

CUndoNodeNameChange::CUndoNodeNameChange(CAbstractNodeItem& node, const char* szOldName, const string& description)
	: CUndoObject(&node.GetViewModel())
{
	m_nodeId = node.GetId();
	m_name = szOldName;

	if (description.empty())
	{
		string newName = QtUtil::ToString(node.GetName());
		m_description.Format("Undo node renaming '%s' -> '%s'", szOldName, newName.c_str());
	}
	else
		m_description = description;
}

void CUndoNodeNameChange::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			QString name = pNodeItem->GetName();
			pNodeItem->SetName(m_name);
			m_name = name;

			// TODO: This is just a workaround for the particle editor!
			//			 Get rid of this as soon as nodeId is not name based anymore!
			m_nodeId = pNodeItem->GetId();
			// ~TODO
		}
	}
}

void CUndoNodeNameChange::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			QString name = pNodeItem->GetName();
			pNodeItem->SetName(m_name);
			m_name = name;

			// TODO: This is just a workaround for the particle editor!
			//			 Get rid of this as soon as nodeId is not name based anymore!
			m_nodeId = pNodeItem->GetId();
			// ~TODO
		}
	}
}

CUndoConnectionCreate::CUndoConnectionCreate(CAbstractConnectionItem& connection, const string& description)
	: CUndoObject(&connection.GetViewModel())
{
	m_sourcePinId = connection.GetSourcePinItem().GetId();
	m_targetPinId = connection.GetTargetPinItem().GetId();
	m_sourceNodeId = connection.GetSourcePinItem().GetNodeItem().GetId();
	m_targetNodeId = connection.GetTargetPinItem().GetNodeItem().GetId();
	m_connectionId = connection.GetId();

	Serialization::SaveJsonBuffer(m_connectionData, connection);
	m_connectionData.push_back('\0');

	if (description.empty())
		m_description.Format("Undo connection creation");
	else
		m_description = description;
}

void CUndoConnectionCreate::Undo(bool bUndo)
{
	if (m_pModel && bUndo)
	{
		if (CAbstractConnectionItem* pConnectionItem = m_pModel->GetConnectionItemById(m_connectionId))
		{
			Serialization::SaveJsonBuffer(m_connectionData, *pConnectionItem);
			m_connectionData.push_back('\0');

			m_pModel->RemoveConnection(*pConnectionItem);
		}
	}
}

void CUndoConnectionCreate::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pSourceNodeItem = m_pModel->GetNodeItemById(m_sourceNodeId);
		CAbstractNodeItem* pTargetNodeItem = m_pModel->GetNodeItemById(m_targetNodeId);
		if (pSourceNodeItem && pTargetNodeItem)
		{
			CAbstractPinItem* pSourcePinItem = pSourceNodeItem->GetPinItemById(m_sourcePinId);
			CAbstractPinItem* pTargetPinItem = pTargetNodeItem->GetPinItemById(m_targetPinId);
			if (pSourcePinItem && pTargetPinItem)
			{
				if (CAbstractConnectionItem* pConnectionItem = m_pModel->CreateConnection(*pSourcePinItem, *pTargetPinItem))
				{
					Serialization::LoadJsonBuffer(*pConnectionItem, m_connectionData.data(), m_connectionData.size());
					m_connectionId = pConnectionItem->GetId();
				}
			}
		}
	}
}

CUndoConnectionRemove::CUndoConnectionRemove(CAbstractConnectionItem& connection, const string& description)
	: CUndoObject(&connection.GetViewModel())
{
	m_sourcePinId = connection.GetSourcePinItem().GetId();
	m_targetPinId = connection.GetTargetPinItem().GetId();
	m_sourceNodeId = connection.GetSourcePinItem().GetNodeItem().GetId();
	m_targetNodeId = connection.GetTargetPinItem().GetNodeItem().GetId();
	m_connectionId = connection.GetId();

	Serialization::SaveJsonBuffer(m_connectionData, connection);
	m_connectionData.push_back('\0');

	if (description.empty())
		m_description.Format("Undo connection deletion");
	else
		m_description = description;
}

void CUndoConnectionRemove::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pSourceNodeItem = m_pModel->GetNodeItemById(m_sourceNodeId);
		CAbstractNodeItem* pTargetNodeItem = m_pModel->GetNodeItemById(m_targetNodeId);
		if (pSourceNodeItem && pTargetNodeItem)
		{
			CAbstractPinItem* pSourcePinItem = pSourceNodeItem->GetPinItemById(m_sourcePinId);
			CAbstractPinItem* pTargetPinItem = pTargetNodeItem->GetPinItemById(m_targetPinId);
			if (pSourcePinItem && pTargetPinItem)
			{
				if (CAbstractConnectionItem* pConnectionItem = m_pModel->CreateConnection(*pSourcePinItem, *pTargetPinItem))
				{
					Serialization::LoadJsonBuffer(*pConnectionItem, m_connectionData.data(), m_connectionData.size());
					m_connectionId = pConnectionItem->GetId();
				}
			}
		}
	}
}

void CUndoConnectionRemove::Redo()
{
	if (m_pModel)
	{
		if (CAbstractConnectionItem* pConnectionItem = m_pModel->GetConnectionItemById(m_connectionId))
		{
			Serialization::SaveJsonBuffer(m_connectionData, *pConnectionItem);
			m_connectionData.push_back('\0');

			m_pModel->RemoveConnection(*pConnectionItem);
		}
	}
}

CUndoNodeCreate::CUndoNodeCreate(CAbstractNodeItem& node, const string& description)
	: CUndoObject(&node.GetViewModel())
{
	m_nodeId = node.GetId();
	m_typdeId = node.GetTypeId();
	m_position = node.GetPosition();

	Serialization::SaveJsonBuffer(m_nodeData, node);
	m_nodeData.push_back('\0');

	if (description.empty())
	{
		string name = QtUtil::ToString(node.GetName());
		m_description.Format("Undo creation of node '%s'", name.c_str());
	}
	else
		m_description = description;
}

void CUndoNodeCreate::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			// TODO: Workaround for Schematyc because when creating the undo step during pasting the
			//			 node doesn't have any data. Remove as soon as those issues are fixed in Schematyc backend.
			Serialization::SaveJsonBuffer(m_nodeData, *pNodeItem);
			m_nodeData.push_back('\0');
			// ~TODO

			m_pModel->RemoveNode(*pNodeItem);
		}
	}
}

void CUndoNodeCreate::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->CreateNode(m_typdeId, m_position);
		if (pNodeItem)
		{
			Serialization::LoadJsonBuffer(*pNodeItem, m_nodeData.data(), m_nodeData.size());
			m_nodeId = pNodeItem->GetId();
		}
	}
}

CUndoNodeRemove::CUndoNodeRemove(CAbstractNodeItem& node, const string& description)
	: CUndoObject(&node.GetViewModel())
{
	m_nodeId = node.GetId();
	m_typdeId = node.GetTypeId();
	m_position = node.GetPosition();

	Serialization::SaveJsonBuffer(m_nodeData, node);
	m_nodeData.push_back('\0');

	if (description.empty())
	{
		string name = QtUtil::ToString(node.GetName());
		m_description.Format("Undo deletion of node '%s'", name.c_str());
	}
	else
		m_description = description;
}

void CUndoNodeRemove::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->CreateNode(m_typdeId, m_position);
		if (pNodeItem)
		{
			Serialization::LoadJsonBuffer(*pNodeItem, m_nodeData.data(), m_nodeData.size());
			m_nodeId = pNodeItem->GetId();
		}
	}
}

void CUndoNodeRemove::Redo()
{
	if (m_pModel)
	{
		CAbstractNodeItem* pNodeItem = m_pModel->GetNodeItemById(m_nodeId);
		if (pNodeItem)
		{
			m_pModel->RemoveNode(*pNodeItem);
		}
	}
}

}

