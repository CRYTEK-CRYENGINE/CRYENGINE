// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractPinItem.h>
namespace EditorSubstance
{
	namespace OutputEditor
	{

		enum EOutputPinType {
			eOutputRGBA = 1,
			eOutputRGB = 2,
			eOutputR = 4,
			eOutputG = 8,
			eOutputB = 16,
			eOutputA = 32,
			eIndividualOutputs = eOutputA | eOutputB | eOutputG | eOutputR,

		};

		static std::map<EOutputPinType, string> pinNameMap{
			{ eOutputRGBA, "RGBA" },
			{ eOutputRGB, "RGB" },
			{ eOutputR, "R" },
			{ eOutputG, "G" },
			{ eOutputB, "B" },
			{ eOutputA, "A" },
		};

		class CSubstanceBasePinItem : public CryGraphEditor::CAbstractPinItem
		{
		public:
			CSubstanceBasePinItem(CryGraphEditor::CAbstractNodeItem& nodeItem, EOutputPinType pinType);
			virtual CryGraphEditor::CAbstractNodeItem& GetNodeItem() const override { return m_nodeItem; }
			CryGraphEditor::CPinWidget* CreateWidget(CryGraphEditor::CNodeWidget& nodeWidget, CryGraphEditor::CNodeGraphView& view) override;

			virtual bool CanConnect(const CAbstractPinItem* pOtherPin) const override;

			virtual QString GetName() const override;
			virtual QString GetDescription() const override;

			virtual QString GetTypeName() const override;

			virtual QVariant GetId() const override;
			virtual bool HasId(QVariant id) const override;

		protected:
			CryGraphEditor::CAbstractNodeItem& m_nodeItem;
			EOutputPinType m_pinType;
		};

	}
}
