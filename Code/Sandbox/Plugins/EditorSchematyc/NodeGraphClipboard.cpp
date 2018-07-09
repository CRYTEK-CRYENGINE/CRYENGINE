// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NodeGraphClipboard.h"

#include "GraphViewModel.h"
#include "GraphNodeItem.h"

#include <CrySchematyc/SerializationUtils/SerializationUtils.h>

namespace CrySchematycEditor {

CNodeGraphClipboard::CNodeGraphClipboard(CryGraphEditor::CNodeGraphViewModel& model)
	: CClipboardItemCollection(model)
{}

void CNodeGraphClipboard::SaveNodeDataToXml(CryGraphEditor::CAbstractNodeItem& node, Serialization::IArchive& archive)
{
	CNodeItem& nodeItem = static_cast<CNodeItem&>(node);
	const Schematyc::IScriptGraphNode& scriptGraphNode = nodeItem.GetScriptElement();

	Schematyc::CStackString typeGuidString;
	Schematyc::GUID::ToString(typeGuidString, scriptGraphNode.GetTypeGUID());
	archive(typeGuidString, "typeGUID");
	archive(CopySerialize(scriptGraphNode), "dataBlob");
}

CryGraphEditor::CAbstractNodeItem* CNodeGraphClipboard::RestoreNodeFromXml(Serialization::IArchive& archive)
{
	CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(GetModel());
	if (pModel)
	{
		Schematyc::CStackString typeGuidString;
		archive(typeGuidString, "typeGUID");

		const CryGUID typeGuid = CryGUID::FromString(typeGuidString.c_str());
		CNodeItem* pNodeItem = pModel->CreateNode(typeGuid);
		if (pNodeItem)
		{
			Schematyc::IScriptGraphNode& scriptGraphNode = pNodeItem->GetScriptElement();
			archive(PasteSerialize(scriptGraphNode), "dataBlob");
			pNodeItem->Refresh(true);
			return pNodeItem;
		}
	}
	return nullptr;
}

}

