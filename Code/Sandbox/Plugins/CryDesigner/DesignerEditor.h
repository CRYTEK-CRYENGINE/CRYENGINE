// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Model.h"
#include "Core/ModelCompiler.h"
#include "Objects/DesignerObject.h"
#include "Tools/ToolCommon.h"
#include "ToolFactory.h"
#include "IDataBaseManager.h"
#include "Core/PolygonMesh.h"
#include "DesignerSession.h"
#include "Gizmos/ITransformManipulator.h"

struct IDataBaseItem;
enum EDataBaseItemEvent;

namespace Designer
{
class ElementSet;
class BaseTool;
class Model;
class ExcludedEdgeManager;

//! DesignerEditor class plays a role of managing the global resources(CBaseObject, Designer::Element , main panels and designer tools etc),
//! and dispatching and receiving windows, editor and designer tool messages or vice versa.
class DesignerEditor : public CEditTool, public ITransformManipulatorOwner
{
	DECLARE_DYNCREATE(DesignerEditor)

public:
	DesignerEditor();
	void           Create(const char* szObjectType);
	bool           HasEditingObject() const               { return m_pEditingObject != nullptr; }
	void           SetEditingObject(CBaseObject* pObject) { m_pEditingObject = pObject; }

	virtual string GetDisplayName() const override        { return "Designer"; }
	virtual bool   OnKeyDown(
	  CViewport* view,
	  uint32 nChar,
	  uint32 nRepCnt,
	  uint32 nFlags) override;

	void Display(DisplayContext& dc) override;
	bool MouseCallback(
	  CViewport* view,
	  EMouseEvent event,
	  CPoint& point, int flags) override;
	void OnManipulatorDrag(
	  IDisplayViewport* view,
	  ITransformManipulator* pManipulator,
	  const Vec2i& p0,
	  const Vec3& value,
	  int nFlags);
	void OnManipulatorBegin(
	  IDisplayViewport* view,
	  ITransformManipulator* pManipulator,
	  const Vec2i& point,
	  int flags);
	void OnManipulatorEnd(
	  IDisplayViewport* view,
	  ITransformManipulator* pManipulator);

	bool      IsDisplayGrid() override;
	bool      IsNeedMoveTool() override  { return true; }
	bool      IsAllowTabKey() override   { return true; }
	bool      IsExclusiveMode() override { return gDesignerSettings.bExclusiveMode; }
	void      OnEditorNotifyEvent(EEditorNotifyEvent event);

	void      SelectAllElements();
	void      LeaveCurrentTool();
	void      EnterCurrentTool();
	BaseTool* GetTool(EDesignerTool tool) const;
	BaseTool* GetCurrentTool() const;
	bool      SwitchTool(
	  EDesignerTool tool,
	  bool bForceChange = false,
	  bool bAllowMultipleMode = true);
	void          SwitchToSelectTool() { SwitchTool(m_PreviousSelectTool); }
	bool          EndCurrentDesignerTool();
	void          SwitchToPrevTool();
	EDesignerTool GetPrevTool()                   { return m_PreviousTool; }
	EDesignerTool GetPrevSelectTool()             { return m_PreviousSelectTool; }
	void          SetPrevTool(EDesignerTool tool) { m_PreviousTool = tool; }
	void          StoreSelectionUndo(bool bRestoreAfterUndoing = true);
	void          DisableTool(
	  IDesignerPanel* pEditPanel,
	  EDesignerTool tool);

	void ReleaseObjectGizmo();
	bool SelectDesignerObject(CPoint point);
	bool IsNeedToSkipPivotBoxForObjects() override { return true; }
	void UpdateTMManipulator(
	  const BrushVec3& localPos,
	  const BrushVec3& localNormal);
	void UpdateTMManipulator(
	  const BrushVec3& localPos,
	  const Matrix33& localOrts);
	void           UpdateTMManipulatorBasedOnElements(ElementSet* elements);
	const CCamera& GetCamera() const
	{
		return m_Camera;
	}
	const RECT& GetViewportRect() const
	{
		return m_ViewportRect;
	}
	BrushRay GetRay() const;

	// ITransformManipulatorOwner
	virtual bool GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm);
	virtual void GetManipulatorPosition(Vec3& position);
	virtual bool IsManipulatorVisible();

protected:
	virtual ~DesignerEditor();
	// Delete itself.
	void DeleteThis() { delete this; };

	bool SetSelectionDesignerMode(EDesignerTool tool, bool bAllowMultipleMode = true);

	void StartCreation(const char* szObjectType);

	void BeginEdit();
	void EndEdit();

protected:
	EDesignerTool           m_Tool;
	EDesignerTool           m_PreviousSelectTool;
	EDesignerTool           m_PreviousTool;

	std::set<EDesignerTool> m_DisabledTools;

	std::vector<Functor2<EDesignerNotify, TDesignerNotifyParam>> m_NotifyCallbacks;
	int                      m_iter;

	CCamera                  m_Camera;
	RECT                     m_ViewportRect;
	CViewport*               m_pView;
	CBaseObject*             m_pEditingObject;
	CPoint                   m_ScreenPos;
	ITransformManipulator*   m_pManipulator;
	Matrix34                 m_manipulatorMatrix;
	Vec3                     m_manipulatorPosition;

	mutable TOOLDESIGNER_MAP m_ToolMap;
};

// Used to create a DesignerEditor that works on a created object
class CreateDesignerObjectTool : public DesignerEditor
{
public:
	DECLARE_DYNCREATE(CreateDesignerObjectTool)
	CreateDesignerObjectTool();
};

}

