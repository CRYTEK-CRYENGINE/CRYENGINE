// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NodeGraph/AbstractConnectionItem.h"
#include "OutputEditor/Pins/SubstanceOutPinItem.h"
#include "OutputEditor/Pins/SubstanceInPinItem.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		class CSubstanceConnectionItem : public CryGraphEditor::CAbstractConnectionItem
		{
		public:
			CSubstanceConnectionItem(CSubstanceOutPinItem& sourcePin, CSubstanceInPinItem& targetPin, CryGraphEditor::CNodeGraphViewModel& viewModel);

			virtual ~CSubstanceConnectionItem();

			virtual QVariant GetId() const override
			{
				return QVariant::fromValue(m_id);
			}
			virtual bool HasId(QVariant id) const override
			{
				return (GetId().value<uint32>() == id.value<uint32>());
			}
			virtual CryGraphEditor::CConnectionWidget* CreateWidget(CryGraphEditor::CNodeGraphView& view) override;

			virtual CSubstanceOutPinItem& GetSourcePinItem() const override
			{
				return m_sourcePin;
			}
			virtual CSubstanceInPinItem& GetTargetPinItem() const override
			{
				return m_targetPin;
			}

			virtual const char* GetStyleId() const override
			{
				return "Connection::Substance";
			}
		private:
			CSubstanceOutPinItem& m_sourcePin;
			CSubstanceInPinItem& m_targetPin;

			uint32        m_id;
		};

	}
}
