// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/ClipboardItemCollection.h>

namespace CrySchematycEditor {

class CNodeGraphClipboard : public CryGraphEditor::CClipboardItemCollection
{
public:
	CNodeGraphClipboard(CryGraphEditor::CNodeGraphViewModel& model);

	virtual void                               SaveNodeDataToXml(CryGraphEditor::CAbstractNodeItem& node, Serialization::IArchive& archive) override;
	virtual CryGraphEditor::CAbstractNodeItem* RestoreNodeFromXml(Serialization::IArchive& archive) override;
};

}

