// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ClipboardItemCollection.h"

#include "AbstractConnectionItem.h"
#include "AbstractNodeItem.h"
#include "AbstractPinItem.h"
#include "AbstractNodeGraphViewModel.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>

Q_DECLARE_METATYPE(XmlNodeRef);

namespace CryGraphEditor {

void CClipboardItemCollection::SClipboardConnectionItem::Serialize(Serialization::IArchive& archive)
{
	archive(sourceNodeIndex, "sourceNode");
	archive(sourcePinIndex, "sourcePinIndex");
	archive(targetNodeIndex, "targetNode");
	archive(targetPinIndex, "targetPinIndex");
}

void CClipboardItemCollection::SClipboardNodeItem::Serialize(Serialization::IArchive& archive)
{
	archive(positionX, "posX");
	archive(positionY, "posY");

	CClipboardItemCollection* pClipboardItems = static_cast<CClipboardItemCollection*>(archive.context<CItemCollection>());

	CRY_ASSERT_MESSAGE(pClipboardItems, "Context for deserialization not set.");
	if (pClipboardItems)
	{
		if (archive.isOutput())
		{
			pClipboardItems->SaveNodeDataToXml(*pNodeItem, archive);
		}
		else
		{
			pNodeItem = pClipboardItems->RestoreNodeFromXml(archive);
		}
	}
}

CClipboardItemCollection::CClipboardItemCollection(CNodeGraphViewModel& model)
{
	SetModel(&model);
}

void CClipboardItemCollection::Serialize(Serialization::IArchive& archive)
{
	CryGraphEditor::CItemCollection::Serialize(archive);

	std::vector<SClipboardNodeItem> nodeSer;
	std::vector<SClipboardConnectionItem> connectionSer;

	if (archive.isOutput())
	{
		NodeIndexByInstance nodeIndexByInstance;

		if (GetNodes().size())
		{
			QPointF centerOfSelection = GetItemsRect().center();

			for (CAbstractNodeItem* pNodeItem : GetNodes())
			{
				const size_t index = nodeSer.size();
				nodeIndexByInstance.emplace(pNodeItem, index);

				XmlNodeRef xmlNodes = GetIEditor()->GetSystem()->CreateXmlNode("Nodes");
				Serialization::SaveXmlNode(xmlNodes, *pNodeItem);

				SClipboardNodeItem n;
				n.pNodeItem = pNodeItem;
				n.positionX = pNodeItem->GetPosition().x() - centerOfSelection.x();
				n.positionY = pNodeItem->GetPosition().y() - centerOfSelection.y();

				nodeSer.emplace_back(n);
			}

			for (CAbstractConnectionItem* pConnectionItem : GetConnections())
			{
				auto sourceNode = nodeIndexByInstance.find(&pConnectionItem->GetSourcePinItem().GetNodeItem());
				auto targetNode = nodeIndexByInstance.find(&pConnectionItem->GetTargetPinItem().GetNodeItem());

				if (sourceNode != nodeIndexByInstance.end() && targetNode != nodeIndexByInstance.end())
				{
					CAbstractPinItem& sourcePinItem = pConnectionItem->GetSourcePinItem();
					CAbstractPinItem& targetPinItem = pConnectionItem->GetTargetPinItem();

					SClipboardConnectionItem c;
					c.sourceNodeIndex = sourceNode->second;
					c.sourcePinIndex = sourcePinItem.GetIndex();
					c.targetNodeIndex = targetNode->second;
					c.targetPinIndex = targetPinItem.GetIndex();
					connectionSer.emplace_back(c);
				}
			}

			archive(nodeSer, "nodes");
			archive(connectionSer, "connections");
		}
	}
	else
	{
		archive(nodeSer, "nodes");
		archive(connectionSer, "connections");

		CNodeGraphViewModel* pModel = GetModel();
		NodesByIndex nodes;

		if (nodeSer.size() > 0)
		{
			IXmlParser* pXmlParser = GetISystem()->GetXmlUtils()->CreateXmlParser();

			for (SClipboardNodeItem& n : nodeSer)
			{
				if (n.pNodeItem)
				{
					const QPointF nodePosition = QPointF(n.positionX, n.positionY) + m_scenePosition;
					n.pNodeItem->SetPosition(nodePosition);
					nodes.push_back(n.pNodeItem);
					AddNode(*n.pNodeItem);
					continue;
				}

				nodes.push_back(nullptr);
			}

			pXmlParser->Release();

			for (SClipboardConnectionItem& c : connectionSer)
			{
				const bool isValidSource = c.sourceNodeIndex < nodes.size() && c.sourceNodeIndex != InvalidIndex;
				const bool isValidTarget = c.targetNodeIndex < nodes.size() && c.targetNodeIndex != InvalidIndex;
				if (isValidSource && isValidTarget)
				{
					CAbstractNodeItem* pSourceNode = nodes[c.sourceNodeIndex];
					CAbstractNodeItem* pTargetNode = nodes[c.targetNodeIndex];

					CAbstractPinItem* pSourcePin = pSourceNode ? pSourceNode->GetPinItemByIndex(c.sourcePinIndex) : nullptr;
					CAbstractPinItem* pTargetPin = pTargetNode ? pTargetNode->GetPinItemByIndex(c.targetPinIndex) : nullptr;
					if (pSourcePin && pTargetPin)
					{
						if (CAbstractConnectionItem* pConnectionItem = pModel->CreateConnection(*pSourcePin, *pTargetPin))
						{
							AddConnection(*pConnectionItem);
						}
					}
				}
			}
		}
	}
}

}

