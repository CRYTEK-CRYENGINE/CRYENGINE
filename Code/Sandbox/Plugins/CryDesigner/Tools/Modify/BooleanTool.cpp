// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BooleanTool.h"
#include "DesignerEditor.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "Core/Helper.h"
#include "DesignerSession.h"

using Serialization::ActionButton;

namespace Designer
{
// Make it a non modal operator
void BooleanTool::Enter()
{
	__super::Enter();

	const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();
	int nDesignerObjectCount = 0;
	int nOtherObjectCount = 0;
	for (int i = 0, iObjectCount(pGroup->GetCount()); i < iObjectCount; ++i)
	{
		CBaseObject* pObject = pGroup->GetObject(i);
		if (pObject->GetType() == OBJTYPE_SOLID)
			++nDesignerObjectCount;
		else
			++nOtherObjectCount;
	}

	if (nOtherObjectCount > 0 || nDesignerObjectCount < 2)
	{
		if (nOtherObjectCount > 0)
			MessageBox("Warning", "Only designer objects should be selected to use this tool");
		else if (nDesignerObjectCount < 2)
			MessageBox("Warning", "At least two designer objects should be selected to use this tool");
		//TODO: we should just quit the tool
		GetDesigner()->SwitchTool(eDesigner_Invalid);
	}
}

void BooleanTool::BooleanOperation(EBooleanOperationEnum booleanType)
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();

	std::vector<_smart_ptr<DesignerObject>> designerObjList;
	for (int i = 0, iSelectionCount(pSelection->GetCount()); i < iSelectionCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(iSelectionCount - i - 1);
		if (!pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
			continue;
		designerObjList.push_back((DesignerObject*)pObj);
	}

	if (designerObjList.size() < 2)
	{
		MessageBox("Warning", "More than 2 designer objects should be selected to be operated.");
		return;
	}

	string undoMsg("Designer : Boolean.");

	if (booleanType == eBOE_Union) undoMsg += "Union";
	else if (booleanType == eBOE_Intersection)
		undoMsg += "Intersection";
	else if (booleanType == eBOE_Difference)
		undoMsg += "Difference";

	CUndo undo(undoMsg);
	designerObjList[0]->StoreUndo(undoMsg);
	ResetXForm(designerObjList[0]->GetMainContext());

	DesignerSession::GetInstance()->SetBaseObject(nullptr);

	for (int i = 1, iDesignerObjCount(designerObjList.size()); i < iDesignerObjCount; ++i)
	{
		BrushVec3 offset = designerObjList[i]->GetWorldTM().GetTranslation() - designerObjList[0]->GetWorldTM().GetTranslation();
		Matrix34 targetTM(designerObjList[i]->GetWorldTM());
		targetTM.SetTranslation(offset);
		designerObjList[i]->StoreUndo(undoMsg);
		designerObjList[i]->GetModel()->Transform(targetTM);

		if (booleanType == eBOE_Union)
			designerObjList[0]->GetModel()->Union(designerObjList[i]->GetModel());
		else if (booleanType == eBOE_Difference)
			designerObjList[0]->GetModel()->Subtract(designerObjList[i]->GetModel());
		else if (booleanType == eBOE_Intersection)
			designerObjList[0]->GetModel()->Intersect(designerObjList[i]->GetModel());

		GetIEditor()->DeleteObject(designerObjList[i]);
	}

	UpdateSurfaceInfo();
	Designer::ApplyPostProcess(designerObjList[0]->GetMainContext());
	GetIEditor()->SelectObject(designerObjList[0]);

	DesignerSession::GetInstance()->SetBaseObject(designerObjList[0]);
	//TODO: we should just quit the tool
	GetDesigner()->SwitchTool(eDesigner_Invalid);
}

void BooleanTool::Serialize(Serialization::IArchive& ar)
{
	ar(ActionButton([this] { Union();
	                }), "Union", "Union");
	ar(ActionButton([this] { Subtract();
	                }), "Subtract", "Subtract");
	ar(ActionButton([this] { Intersection();
	                }), "Intersection", "Intersection");
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Boolean, eToolGroup_Modify, "Boolean", BooleanTool,
                                                           boolean, "runs boolean tool", "designer.boolean")

