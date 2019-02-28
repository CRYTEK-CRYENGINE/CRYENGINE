// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceConnectionItem.h"
#include "NodeGraph/ConnectionWidget.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		CSubstanceConnectionItem::CSubstanceConnectionItem(CSubstanceOutPinItem& sourcePin, CSubstanceInPinItem& targetPin, CryGraphEditor::CNodeGraphViewModel& viewModel)
			: CryGraphEditor::CAbstractConnectionItem(viewModel)
			, m_sourcePin(sourcePin)
			, m_targetPin(targetPin)
		{
			m_sourcePin.AddConnection(*this);
			m_targetPin.AddConnection(*this);

			string sourceNodeName = QtUtil::ToString(m_sourcePin.GetNodeItem().GetName());
			string sourceNodePin = QtUtil::ToString(m_sourcePin.GetName());
			string targetNodeName = QtUtil::ToString(m_targetPin.GetNodeItem().GetName());
			string targetNodePin = QtUtil::ToString(m_targetPin.GetName());

			string id;
			id.Format("%s-%s::%s-%s", sourceNodeName, sourceNodePin, targetNodeName, targetNodePin);

			m_id = CCrc32::Compute(id.c_str());
		}

		CSubstanceConnectionItem::~CSubstanceConnectionItem()
		{
			m_sourcePin.RemoveConnection(*this);
			m_targetPin.RemoveConnection(*this);
		}

		CryGraphEditor::CConnectionWidget* CSubstanceConnectionItem::CreateWidget(CryGraphEditor::CNodeGraphView& view)
		{
			return new CryGraphEditor::CConnectionWidget(this, view);
		}

	}
}
