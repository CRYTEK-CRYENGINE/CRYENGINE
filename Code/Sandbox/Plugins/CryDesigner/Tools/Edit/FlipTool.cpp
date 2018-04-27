// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlipTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "Core/SmoothingGroupManager.h"

namespace Designer
{
void FlipTool::Enter()
{
	//! The flip tool is carried out as soon as the Flip Tool is switched to and then the selection tool is set.
	//! Because this tools don't need any processes about any interactions with the input events.

	//! Gets the selected elements from the DesignerEditor. An element points out a vertex, an edge or a polygon.
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();

	//! If there are selected elements, The Flip is carried out.
	if (!pSelected->IsEmpty())
		FlipPolygons();

	//! The selected polygons are deleted from the selected elements.
	pSelected->Erase(ePF_Polygon);

	//! Switches the current tool into the Select/Move tool.
	GetDesigner()->SwitchToSelectTool();
}

void FlipTool::Leave()
{
	__super::Leave();

	//! The newly flipped polygons are set as the selected elements just before this tool exits.
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Set(m_FlipedSelectedElements);
}

void FlipTool::FlipPolygons(MainContext mc, std::vector<PolygonPtr>& polygons, ElementSet& outFlipedElements)
{
	outFlipedElements.Clear();

	for (int i = 0, iPolygonCount(polygons.size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = polygons[i];
		if (pPolygon == NULL)
			continue;

		//! The polygon is flipped.
		pPolygon->Flip();
		//! The flipped polygon should be removed from the smoothing group if this polygon is registered to the smoothing group.
		mc.pModel->GetSmoothingGroupMgr()->RemovePolygon(pPolygon);

		//! The flipped polygon is added as an element form to be returned.
		Element flipedElement;
		flipedElement.SetPolygon(mc.pObject, pPolygon);
		outFlipedElements.Add(flipedElement);
	}
}

void FlipTool::FlipPolygons()
{
	//! The current model status is stored in the undo stack.
	CUndo undo("Designer : Flip");

	GetModel()->RecordUndo("Flip", GetBaseObject());
	
	m_FlipedSelectedElements.Clear();
	DesignerSession* pSession = DesignerSession::GetInstance();
	const ElementSet* pSelected = pSession->GetSelectedElements();
	FlipPolygons(pSession->GetMainContext(), pSelected->GetAllPolygons(), m_FlipedSelectedElements);

	//! If there are no flipped polygons, the undo cancelled.
	if (m_FlipedSelectedElements.IsEmpty())
		undo.Cancel();
	else //! If there are flipped polygons, the post process is applied to create the renderable mesh etc.
		ApplyPostProcess();
}
}

//! Registers this tool to CryDesigner
REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Flip, eToolGroup_Edit, "Flip", FlipTool,
                                   flip, "runs flip tool", "designer.flip")

