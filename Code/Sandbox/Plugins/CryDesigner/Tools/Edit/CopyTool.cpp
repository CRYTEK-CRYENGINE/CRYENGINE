// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CopyTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/Model.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"
#include "Util/ElementSet.h"

namespace Designer
{
void CopyTool::Enter()
{
	CUndo undo("Designer : Copy a Part");

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	GetModel()->SetShelf(eShelf_Base);
	GetModel()->RecordUndo("Designer : Copy a Part", GetBaseObject());

	ElementSet copiedElements;
	Copy(MainContext(GetBaseObject(), GetCompiler(), GetModel()), &copiedElements);

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();
	pSelected->Add(copiedElements);

	ApplyPostProcess();
	GetDesigner()->SwitchToSelectTool();
}

void CopyTool::Copy(MainContext& mc, ElementSet* pOutCopiedElements)
{
	DESIGNER_ASSERT(pOutCopiedElements);
	if (!pOutCopiedElements)
		return;

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	std::vector<PolygonPtr> selectedPolygons;

	for (int i = 0, iElementCount(pSelected->GetCount()); i < iElementCount; ++i)
	{
		const Element& elementInfo = pSelected->Get(i);
		if (!elementInfo.IsPolygon())
			continue;

		DESIGNER_ASSERT(elementInfo.m_pPolygon);
		if (!elementInfo.m_pPolygon || !elementInfo.m_pPolygon->IsValid())
			continue;

		PolygonPtr pPolygon = mc.pModel->QueryEquivalentPolygon(elementInfo.m_pPolygon);
		selectedPolygons.push_back(pPolygon);
	}

	pOutCopiedElements->Clear();
	for (int i = 0, iPolygonCount(selectedPolygons.size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pClone = selectedPolygons[i]->Clone();
		mc.pModel->AddPolygon(pClone, false);
		Element ei;
		ei.SetPolygon(mc.pObject, pClone);
		ei.m_bIsolated = true;
		pOutCopiedElements->Add(ei);
	}
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Copy, eToolGroup_Edit, "Copy", CopyTool,
                                   copy, "runs copy tool", "designer.copy")

