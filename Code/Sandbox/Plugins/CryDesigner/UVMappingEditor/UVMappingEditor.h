// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QMainWindow>
#include "UVMappingEditorCommon.h"
#include "Core/PolygonMesh.h"
#include "Objects/DesignerObject.h"
#include "BaseTool.h"
#include "QViewport.h"
#include "Core/UVIsland.h"
#include "QViewportEvents.h"
#include "UVElement.h"
#include "UVCursor.h"
#include "UVGizmo.h"
#include "Tools/ToolCommon.h"
#include "DisplayViewportAdapter.h"
#include "QtViewPane.h"

class QToolbar;
class QToolButton;
class QBoxLayout;
class QGridLayout;
class UVIslandManager;
class CAxisHelper;
struct SRenderContext;
class CBaseObject;
class QCheckBox;

class QMenuComboBox;

namespace Designer
{
class QMaterialComboBox;
class Model;
class ElementSet;

namespace UVMapping
{
enum EPivotType
{
	ePivotType_Selection,
	ePivotType_Cursor
};

class UVMappingEditor : public CDockableWindow, public IEditorNotifyListener, public IGizmoTransform, public IDataBaseManagerListener
{
	Q_OBJECT

public:

	UVMappingEditor();
	~UVMappingEditor();

	void              OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void              OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event) override;
	void              OnDesignerNotifyHandler(EDesignerNotify notification, void* param);

	void              ApplyPostProcess(int updateType = ePostProcess_Mesh | ePostProcess_SmoothingGroup);
	void              CompileShelf(ShelfID shelf);
	void              ClearSelection();
	void              ClearSharedSelection();

	UVElementSetPtr   GetElementSet() const       { return m_ElementSet;       }
	UVElementSetPtr   GetSharedElementSet() const { return m_SharedElementSet; }
	UVCursorPtr       GetUVCursor() const         { return m_pUVCursor;        }
	UVIslandManager*  GetUVIslandMgr() const;
	UVGizmoPtr        GetGizmo() const            { return m_pGizmo;           }
	EPivotType        GetPivotType() const;
	IDisplayViewport* GetViewport() const         { return m_pViewportAdapter.get(); }
	EPlaneAxis        GetPlaneAxis() const;
	bool              AllIslandsViewed() const    { return m_bViewAllUVIslands; }
	Model*            GetModel() const;
	ElementSet*       GetSelectedElements() const;
	CBaseObject*      GetBaseObject() const;
	const CCamera*    Get3DViewportCamera() const;

	void              SetTool(EUVMappingTool tool);
	EUVMappingTool    GetPrevTool() const { return m_PrevTool; }
	BaseTool*         GetCurrentTool() const;
	int               GetSubMatID() const;
	void              SetSubMatID(int nSubMatID);
	void              SelectUVIslands(const std::vector<UVIslandPtr>& uvIslands);

	void              OnGizmoLMBDown(int mode) override;
	void              OnGizmoLMBUp(int mode) override;
	void              OnTransformGizmo(int mode, const Vec3& offset) override;

	void              UpdateGizmoPos();
	void              UpdateSubMatComboBox();
	void              UpdateSelectionFromDesigner();
	void              UpdateSharedSelection();

	//////////////////////////////////////////////////////////
	// CDockableWindow implementation
	virtual const char*                       GetPaneTitle() const override        { return "UV Mapping"; };
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	//////////////////////////////////////////////////////////

public slots:

	void OnRender(const SRenderContext& rc);
	void OnMouseEvent(const SMouseEvent& me);
	void OnIdle();

	void OnTranslation();
	void OnRotation();
	void OnScale();
	void OnUndo();
	void OnRedo();
	void OnUnmap();
	void OnRotateCamera();
	void OnGoto();
	void OnSelectAll();
	void OnUpdateUVIslands();
	void OnViewAll(bool bViewAll);

	void OnSubMatSelectionChanged(int nSelectedItem);

private:

	void RenderUnwrappedMesh(DisplayContext& dc);
	void RenderUnwrappedPolygon(DisplayContext & dc, UVIslandPtr pUVIsland, PolygonPtr pPolygon, ColorB);
	void RenderElements(DisplayContext& dc, ColorB color, UVElementSetPtr pUVElementSet);
	void UpdateObject();
	void OrganizeToolbar(QBoxLayout* pTopLayout);
	void InitializeMaterialComboBox();
	void RefreshPolygonsInUVIslands();
	void CreateTexturePanelMesh();
	void RegisterMenuButtons(EUVMappingToolGroup what, QGridLayout* where, int columnNumber);
	void RegisterDesignerNotifyCallback();

	QViewport*                                     m_pViewport;
	std::shared_ptr<CDisplayViewportAdapter>       m_pViewportAdapter;
	QMenuComboBox*                                 m_pPivotComboBox;
	QMenuComboBox*                                 m_pPlaneAxisComboBox;
	QMaterialComboBox*                             m_pSubMatComboBox;
	QToolButton*                                   m_pRotationPushBox;
	QCheckBox*                                     m_pViewAllCheckBox;

	std::map<EGizmoMode, QAction*>                 m_TransformActions;
	std::map<EUVMappingTool, QToolButton*>         m_pButtons;

	_smart_ptr<PolygonMesh>                        m_pMaterialMesh;
	_smart_ptr<DesignerObject>                     m_pObject;

	EUVMappingTool                                 m_Tool;
	EUVMappingTool                                 m_PrevTool;
	EPivotType                                     m_PrevPivotType;
	std::map<EUVMappingTool, _smart_ptr<BaseTool>> m_ToolMap;

	bool            m_bLButtonDown;
	bool            m_bHitGizmo;
	float           m_CameraZAngle;
	bool            m_bViewAllUVIslands;

	UVElementSetPtr m_ElementSet;
	UVElementSetPtr m_SharedElementSet;
	UVCursorPtr     m_pUVCursor;
	UVGizmoPtr      m_pGizmo;
};

UVMappingEditor* GetUVEditor();

}
}

