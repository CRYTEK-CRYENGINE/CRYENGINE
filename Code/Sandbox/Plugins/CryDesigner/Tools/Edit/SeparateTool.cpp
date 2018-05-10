// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SeparateTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/Model.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"
#include <QObject>

namespace Designer
{
void SeparateTool::Enter()
{
	CUndo undo("Designer : Separate a Part");
	GetModel()->RecordUndo("Designer : Separate a Part", GetBaseObject());
	DesignerObject* pNewObj = Separate(GetMainContext());
	DesignerSession* pSession = DesignerSession::GetInstance();
	if (pNewObj)
	{
		pSession->signalDesignerEvent(eDesignerNotify_PolygonsModified, nullptr);
		pSession->SetBaseObject(pNewObj);
		UpdateSurfaceInfo();
	}

	GetDesigner()->SwitchToPrevTool();
}

DesignerObject* SeparateTool::Separate(MainContext& mc)
{
	ElementSet* pSelected = mc.pSelected;
	int iElementCount(pSelected->GetCount());

	// Nothing selected, nothing to separate
	if (iElementCount == 0)
	{
		MessageBox(QObject::tr("Warning"), QObject::tr("No selected polygons to separate"));
		return nullptr;
	}

	DesignerObject* pNewObj = (DesignerObject*)GetIEditor()->NewObject("Designer", "");
	DESIGNER_ASSERT(pNewObj);
	if (!pNewObj)
	{
		return nullptr;
	}

	pNewObj->SetWorldTM(mc.pObject->GetWorldTM());

	_smart_ptr<Model> pNewModel = pNewObj->GetModel();
	pNewModel->SetFlag(mc.pModel->GetFlag());
	pNewModel->SetSubdivisionLevel(mc.pModel->GetSubdivisionLevel());

	MODEL_SHELF_RECONSTRUCTOR_POSTFIX(pNewModel, 0);
	MODEL_SHELF_RECONSTRUCTOR_POSTFIX(mc.pModel, 1);

	pNewModel->SetShelf(eShelf_Base);
	mc.pModel->SetShelf(eShelf_Base);

	for (int i = 0, iElementCount(pSelected->GetCount()); i < iElementCount; ++i)
	{
		const Element& elementInfo = (*pSelected)[i];
		if (!elementInfo.IsPolygon())
			continue;

		DESIGNER_ASSERT(elementInfo.m_pPolygon);
		if (!elementInfo.m_pPolygon)
			continue;

		mc.pModel->RemovePolygon(mc.pModel->QueryEquivalentPolygon(elementInfo.m_pPolygon));
		pNewModel->AddPolygon(elementInfo.m_pPolygon->Clone());
	}
	pSelected->Clear();

	Designer::ApplyPostProcess(mc);
	Designer::ApplyPostProcess(pNewObj->GetMainContext());

	pNewObj->SetMaterial(mc.pObject->GetMaterial());

	GetIEditor()->SelectObject(pNewObj);
	GetIEditor()->GetObjectManager()->UnselectObject(mc.pObject);

	return pNewObj;
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Separate, eToolGroup_Edit, "Separate", SeparateTool,
                                   separate, "runs separate tool", "designer.separate")

