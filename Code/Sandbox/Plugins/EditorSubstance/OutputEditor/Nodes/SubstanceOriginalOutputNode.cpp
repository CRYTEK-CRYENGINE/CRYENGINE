// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceOriginalOutputNode.h"
#include "OutputEditor/GraphViewModel.h"
#include "OutputEditor/Pins/SubstanceOutPinItem.h"
#include "NodeGraph/NodeWidget.h"
#include "NodeGraph/PinGridNodeContentWidget.h"
#include "OutputEditor/SubstanceNodeContentWidget.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		COriginalOutputNode::COriginalOutputNode(const SSubstanceOutput& output, CryGraphEditor::CNodeGraphViewModel& viewModel)
			: CSubstanceOutputNodeBase(output, viewModel)
		{
			SetAcceptsRenaming(true);
			m_outputType = CSubstanceNodeContentWidget::Standard;
			for each (auto var in pinNameMap)
			{
				m_pins.push_back(new CSubstanceOutPinItem(*this, var.first));
			}
		}

	}
}

