// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Plane.h"
#include "Core/Polygon.h"
#include "Core/ModelCompiler.h"
#include "Tools/ToolCommon.h"
#include "IDataBaseManager.h"
#include "DesignerSession.h"

class DesignerEditor;
class IBasePanel;
struct ITransformManipulator;
struct IDisplayViewport;

namespace Designer
{
class ElementSet;

class BaseTool : public _i_reference_target_t
{
public:
	EDesignerTool       Tool() const { return m_Tool; }
	virtual const char* ToolClass() const;
	const char*         ToolName() const;

public:
	BaseTool(EDesignerTool tool) :
		m_Tool(tool)
	{
	}

	virtual ~BaseTool() {}

	virtual bool    OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)   { return false; }
	virtual bool    OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)     { return false; }
	virtual bool    OnLButtonDblClk(CViewport* view, UINT nFlags, CPoint point) { return false; }
	virtual bool    OnRButtonDown(CViewport* view, UINT nFlags, CPoint point)   { return false; }
	virtual bool    OnRButtonUp(CViewport* view, UINT nFlags, CPoint point)     { return false; }
	virtual bool    OnMButtonDown(CViewport* view, UINT nFlags, CPoint point)   { return false; }
	virtual bool    OnMouseMove(CViewport* view, UINT nFlags, CPoint point);
	virtual bool    OnMouseWheel(CViewport* view, UINT nFlags, CPoint point)    { return false; }
	virtual bool    OnFocusLeave(CViewport* view, UINT nFlags, CPoint point)    { return false; }
	virtual bool    OnFocusEnter(CViewport* view, UINT nFlags, CPoint point)    { return false; }

	virtual bool    OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual void    Display(DisplayContext& dc);
	virtual void    OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int flags)  {}
	virtual void    OnManipulatorBegin(IDisplayViewport* view, ITransformManipulator* pManipulator, CPoint& point, int flags) {}
	virtual void    OnManipulatorEnd(IDisplayViewport* view, ITransformManipulator* pManipulator) {}

	virtual void    Enter();
	virtual void    Leave();

	virtual void    OnEditorNotifyEvent(EEditorNotifyEvent event)                       {}

	virtual void    OnChangeParameter(bool continuous)          {}
	virtual bool    EnabledSeamlessSelection() const            { return true; }
	virtual bool    IsPhaseFirstStepOnPrimitiveCreation() const { return true; }
	virtual bool    IsManipulatorVisible() 
	{
		DesignerSession* pSession = DesignerSession::GetInstance();
		return pSession->GetSelectedElements()->GetCount() > 0;
	}

public:
	Model*         GetModel() const;
	ModelCompiler* GetCompiler() const;
	CBaseObject*   GetBaseObject() const;
	MainContext    GetMainContext() const;

	void           DisplayDimensionHelper(DisplayContext& dc, ShelfID nShelf = eShelf_Any);
	void           DisplayDimensionHelper(DisplayContext& dc, const AABB& aabb);

	bool           IsModelEmpty() const;
	void           ApplyPostProcess(int postprocesses = ePostProcess_All);

protected:
	BrushMatrix34 GetWorldTM() const;
	void          CompileShelf(ShelfID nShelf);
	void          UpdateSurfaceInfo();

private:
	EDesignerTool m_Tool;
};
}

