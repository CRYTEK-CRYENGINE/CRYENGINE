// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceBasePinItem.h"

#include <NodeGraph/AbstractNodeItem.h>
#include <NodeGraph/PinWidget.h>

namespace EditorSubstance
{
	namespace OutputEditor
	{

		CSubstanceBasePinItem::CSubstanceBasePinItem(CryGraphEditor::CAbstractNodeItem& nodeItem, EOutputPinType pinType)
			: CryGraphEditor::CAbstractPinItem(nodeItem.GetViewModel())
			, m_nodeItem(nodeItem)
			, m_pinType(pinType)
		{
		}

		CryGraphEditor::CPinWidget* CSubstanceBasePinItem::CreateWidget(CryGraphEditor::CNodeWidget& nodeWidget, CryGraphEditor::CNodeGraphView& view)
		{
			return new CryGraphEditor::CPinWidget(*this, nodeWidget, view);
		}

		bool CSubstanceBasePinItem::CanConnect(const CAbstractPinItem* pOtherPin) const
		{
			if (IsDeactivated())
				return false;
			if (!pOtherPin)
				return true;
			if (pOtherPin->IsDeactivated())
				return false;
			if ((IsOutputPin() && pOtherPin->IsOutputPin()) || (IsInputPin() && pOtherPin->IsInputPin()))
				return false;
			if ((pOtherPin->IsInputPin() && pOtherPin->IsConnected()) || IsInputPin() && IsConnected())
				return false;
			const CSubstanceBasePinItem* otherPin = static_cast<const CSubstanceBasePinItem*>(pOtherPin);
			if (m_pinType == eOutputRGBA && m_pinType == otherPin->m_pinType)
				return true;
			else if (m_pinType == eOutputRGB && m_pinType == otherPin->m_pinType)
				return true;
			else if (eIndividualOutputs & m_pinType && eIndividualOutputs & otherPin->m_pinType)
				return true;

			return false;
		}

		QString CSubstanceBasePinItem::GetName() const
		{
			return QString(pinNameMap[m_pinType].c_str());
		}

		QString CSubstanceBasePinItem::GetDescription() const
		{
			return QString(pinNameMap[m_pinType].c_str());
		}

		QString CSubstanceBasePinItem::GetTypeName() const
		{
			return QString("OutputConfiguration");
		}

		QVariant CSubstanceBasePinItem::GetId() const
		{
			return QVariant::fromValue(QString(pinNameMap[m_pinType].c_str()));
		}

		bool CSubstanceBasePinItem::HasId(QVariant id) const
		{
			QString name = pinNameMap[m_pinType].c_str();
			return (name == id.value<QString>());
		}
	}
}
