// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Tools/BaseTool.h"
#include "DesignerEditor.h"
#include "ViewManager.h"
#include "Core/Model.h"
#include "Core/UVIslandManager.h"
#include "Core/SmoothingGroupManager.h"
#include "Objects/DesignerObject.h"
#include "Core/ModelCompiler.h"
#include "Core/Helper.h"
#include "ToolFactory.h"
#include <CrySerialization/Enum.h>
#include "Util/ExcludedEdgeManager.h"
#include "DesignerSession.h"
#include "Objects/DisplayContext.h"

namespace Designer
{

const char* BaseTool::ToolClass() const
{
	return Serialization::getEnumDescription<EDesignerTool>().label(Tool());
}

const char* BaseTool::ToolName() const
{
	return Serialization::getEnumDescription<EDesignerTool>().name(Tool());
}

BrushMatrix34 BaseTool::GetWorldTM() const
{
	CBaseObject* pBaseObj = GetBaseObject();
	DESIGNER_ASSERT(pBaseObj);
	return pBaseObj->GetWorldTM();
}

void BaseTool::Enter()
{
	DesignerEditor* pEditor = GetDesigner();
	if (pEditor)
	{
		pEditor->ReleaseObjectGizmo();
	}
}

void BaseTool::Leave()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	pSession->GetExcludedEdgeManager()->Clear();
	pSession->ReleaseSelectionMesh();
}

bool BaseTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	MainContext ctx = pSession->GetMainContext();
	Model* pModel = ctx.pModel;
	CBaseObject* pObject = ctx.pObject;
	ModelCompiler* pCompiler = ctx.pCompiler;

	int nPolygonIndex(0);
	if (pModel->QueryPolygon(GetDesigner()->GetRay(), nPolygonIndex))
	{
		PolygonPtr pPolygon = pModel->GetPolygon(nPolygonIndex);
		pSession->UpdateSelectionMesh(pPolygon, pCompiler, pObject);
	}
	else
	{
		pSession->UpdateSelectionMesh(NULL, pCompiler, pObject);
	}

	return true;
}

// remove those ASAP please
ModelCompiler* BaseTool::GetCompiler() const
{
	return 	DesignerSession::GetInstance()->GetCompiler();
}

CBaseObject* BaseTool::GetBaseObject() const
{
	return 	DesignerSession::GetInstance()->GetBaseObject();
}

MainContext BaseTool::GetMainContext() const
{
	return 	DesignerSession::GetInstance()->GetMainContext();
}

void BaseTool::ApplyPostProcess(int postprocesses)
{
	Designer::ApplyPostProcess(GetMainContext(), postprocesses);
}

void BaseTool::CompileShelf(ShelfID nShelf)
{
	if (!GetCompiler())
		return;
	GetCompiler()->Compile(GetBaseObject(), GetModel(), nShelf);
}

void BaseTool::UpdateSurfaceInfo()
{
	if (!GetModel())
		return;
	GetModel()->GetUVIslandMgr()->Clean(GetModel());

	if (!GetModel())
		return;
	GetModel()->GetSmoothingGroupMgr()->Clean(GetModel());
}

Model* BaseTool::GetModel() const
{
	return DesignerSession::GetInstance()->GetModel();
}

void BaseTool::Display(DisplayContext& dc)
{
	if (dc.flags & DISPLAY_2D)
		return;
	DisplayDimensionHelper(dc);
}

bool BaseTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		return GetDesigner()->EndCurrentDesignerTool();
	return true;
}

void BaseTool::DisplayDimensionHelper(DisplayContext& dc, const AABB& aabb)
{
	if (gDesignerSettings.bDisplayDimensionHelper)
	{
		Matrix34 poppedTM = dc.GetMatrix();
		dc.PopMatrix();
		GetBaseObject()->DrawDimensionsImpl(dc, aabb);
		dc.PushMatrix(poppedTM);
	}
}

void BaseTool::DisplayDimensionHelper(DisplayContext& dc, ShelfID nShelf)
{
	AABB aabb = GetModel()->GetBoundBox(nShelf);
	if (!aabb.IsReset())
	{
		if (gDesignerSettings.bDisplayDimensionHelper)
			DisplayDimensionHelper(dc, aabb);
	}
}

bool BaseTool::IsModelEmpty() const
{
	return GetModel() && GetModel()->IsEmpty(eShelf_Base) ? true : false;
}

}

