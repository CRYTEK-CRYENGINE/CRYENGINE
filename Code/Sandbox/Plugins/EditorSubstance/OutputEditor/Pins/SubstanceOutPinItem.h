// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceBasePinItem.h"
#include <NodeGraph/AbstractNodeItem.h>

namespace EditorSubstance
{
	namespace OutputEditor
	{


		class CSubstanceOutPinItem : public CSubstanceBasePinItem
		{
		public:
			CSubstanceOutPinItem(CryGraphEditor::CAbstractNodeItem& nodeItem, EOutputPinType pinType)
				: CSubstanceBasePinItem(nodeItem, pinType)
			{}
			virtual ~CSubstanceOutPinItem() {}
			virtual bool IsInputPin() const override { return false; }
			virtual bool IsOutputPin() const override { return true; }
			virtual const char* GetStyleId() const override
			{
				return "Pin::Substance";
			}
		};


	}
}


