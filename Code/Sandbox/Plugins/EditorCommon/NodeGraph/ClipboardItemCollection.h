// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"
#include "ItemCollection.h"

#include "EditorCommonAPI.h"

namespace CryGraphEditor {

class CAbstractNodeItem;
class CNodeGraphViewModel;
class CAbstractNodeItem;

class EDITOR_COMMON_API CClipboardItemCollection : public CryGraphEditor::CItemCollection
{
protected:
	static const uint32 InvalidIndex = 0xffffffff;

	struct SClipboardConnectionItem
	{
		uint32 sourceNodeIndex;
		uint32 sourcePinIndex;
		uint32 targetNodeIndex;
		uint32 targetPinIndex;

		SClipboardConnectionItem()
			: sourceNodeIndex(InvalidIndex)
			, sourcePinIndex(InvalidIndex)
			, targetNodeIndex(InvalidIndex)
			, targetPinIndex(InvalidIndex)
		{}

		void Serialize(Serialization::IArchive& archive);
	};

	struct SClipboardNodeItem
	{
		CAbstractNodeItem* pNodeItem;

		float              positionX;
		float              positionY;

		SClipboardNodeItem()
			: pNodeItem(nullptr)
			, positionX(.0f)
			, positionY(.0f)
		{}

		void Serialize(Serialization::IArchive& archive);
	};

	typedef std::map<CAbstractNodeItem*, uint32 /* index */> NodeIndexByInstance;
	typedef std::vector<CAbstractNodeItem*>                  NodesByIndex;

public:
	CClipboardItemCollection(CNodeGraphViewModel& model);

	virtual void               Serialize(Serialization::IArchive& archive) override;

	virtual void               SaveNodeDataToXml(CAbstractNodeItem& node, Serialization::IArchive& archive) = 0;
	virtual CAbstractNodeItem* RestoreNodeFromXml(Serialization::IArchive& archive) = 0;
};

}

