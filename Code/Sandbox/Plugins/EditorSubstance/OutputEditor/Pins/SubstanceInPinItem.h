// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceBasePinItem.h"
#include <NodeGraph/AbstractNodeItem.h>

namespace EditorSubstance
{
	namespace OutputEditor
	{

		class CSubstanceInPinItem : public CSubstanceBasePinItem
		{
		public:
			CSubstanceInPinItem(CryGraphEditor::CAbstractNodeItem& nodeItem, EOutputPinType pinType)
				: CSubstanceBasePinItem(nodeItem, pinType)
			{}
			virtual ~CSubstanceInPinItem() {};
			virtual bool IsInputPin() const override { return true; }
			virtual bool IsOutputPin() const override { return false; }
		};
	}
}



