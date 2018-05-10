// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVMappingEditor.h"
#include "QViewportSettings.h"
#include "Core/Helper.h"
#include "Core/Model.h"
#include "Material/MaterialManager.h"
#include "Core/UVIslandManager.h"
#include "UIs/UICommon.h"
#include "UIs/DesignerPanel.h"
#include "Core/UVIslandManager.h"
#include "Util/ElementSet.h"
#include "Core/SmoothingGroupManager.h"
#include "UVCursor.h"
#include "CryIcon.h"
#include "Controls/QMenuComboBox.h"
#include "DesignerSession.h"
#include "Objects/DisplayContext.h"

#include <QBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QToolBar>
#include <QMenuBar>
#include <QShortcut>
#include <QToolButton>
#include <QCheckBox>
#include <QKeySequence>
#include <QLabel>

namespace Designer {
namespace UVMapping {

UVMappingEditor* g_pUVMappingEditor = NULL;

UVMappingEditor* GetUVEditor()
{
	return g_pUVMappingEditor;
}

UVMappingEditor::UVMappingEditor()
{
	GetIEditor()->RegisterNotifyListener(this);
	GetIEditor()->GetMaterialManager()->AddListener(this);

	m_PrevTool = m_Tool = eUVMappingTool_Island;

	g_pUVMappingEditor = this;

	m_bLButtonDown = false;
	m_bHitGizmo = false;
	m_PrevPivotType = ePivotType_Selection;
	m_CameraZAngle = PI * 0.5f;

	m_ElementSet = new UVElementSet;
	m_SharedElementSet = new UVElementSet;
	m_pUVCursor = new UVCursor;
	m_pGizmo = new UVGizmo;

	QWidget* centralWidget = new QWidget();
	setCentralWidget(centralWidget);
	centralWidget->setContentsMargins(0, 0, 0, 0);

	QBoxLayout* pEntireLayout = new QBoxLayout(QBoxLayout::TopToBottom, centralWidget);

	QBoxLayout* pTopLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	pEntireLayout->addLayout(pTopLayout);

	OrganizeToolbar(pTopLayout);
	SetTool(eUVMappingTool_Island);

	setContentsMargins(0, 0, 0, 0);

	m_pViewport = new QViewport(gEnv, 0);
	m_pViewport->AddConsumer(m_pGizmo.get());
	m_pViewportAdapter.reset(new CDisplayViewportAdapter(m_pViewport));

	m_pGizmo->AddCallback(this);

	SViewportState viewState = m_pViewport->GetState();
	viewState.cameraTarget.SetFromVectors(Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(0.5f, 0.5f, 75.0f));
	m_pViewport->SetState(viewState);

	SViewportSettings viewportSettings = m_pViewport->GetSettings();
	viewportSettings.rendering.fps = false;
	viewportSettings.grid.showGrid = true;
	viewportSettings.grid.spacing = 0.5f;
	viewportSettings.camera.moveSpeed = 0.012f;
	viewportSettings.camera.moveBackForthSpeed = 1.65f;
	viewportSettings.camera.bIndependentBackForthSpeed = true;
	viewportSettings.camera.zoomSpeed = 6.5f;
	viewportSettings.camera.showViewportOrientation = false;
	viewportSettings.camera.transformRestraint = eCameraTransformRestraint_Rotation;
	viewportSettings.camera.fov = 1;
	m_pViewport->SetSettings(viewportSettings);
	m_pViewport->SetSceneDimensions(Vec3(100.0f, 100.0f, 100.0f));

	connect(m_pViewport, SIGNAL(SignalRender(const SRenderContext &)), this, SLOT(OnRender(const SRenderContext &)));
	connect(m_pViewport, SIGNAL(SignalMouse(const SMouseEvent &)), this, SLOT(OnMouseEvent(const SMouseEvent &)));

	pEntireLayout->addWidget(m_pViewport, 2);

	setLayout(pEntireLayout);
	CreateTexturePanelMesh();

	Matrix34 polygonTM = Matrix34::CreateIdentity();
	polygonTM.SetTranslation(Vec3(0, 0, 0.0f));
	m_pMaterialMesh->SetWorldTM(polygonTM);

	RegisterDesignerNotifyCallback();
	InitializeMaterialComboBox();
	UpdateObject();
	OnRotateCamera();
	OnUpdateUVIslands();
}

UVMappingEditor::~UVMappingEditor()
{
	GetIEditor()->UnregisterNotifyListener(this);
	m_pGizmo->RemoveCallback(this);
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
	DesignerSession::GetInstance()->signalDesignerEvent.DisconnectObject(this);
	g_pUVMappingEditor = NULL;
}

void UVMappingEditor::CreateTexturePanelMesh()
{
	m_pMaterialMesh = new PolygonMesh;
	std::vector<Vertex> rectangle(4);
	rectangle[0] = Vertex(BrushVec3(0, 0, 0), Vec2(0, 0));
	rectangle[1] = Vertex(BrushVec3(1, 0, 0), Vec2(1, 0));
	rectangle[2] = Vertex(BrushVec3(1, 1, 0), Vec2(1, 1));
	rectangle[3] = Vertex(BrushVec3(0, 1, 0), Vec2(0, 1));
	m_pMaterialMesh->SetPolygon(new Polygon(rectangle), true);
}

void UVMappingEditor::RegisterDesignerNotifyCallback()
{
	DesignerSession::GetInstance()->signalDesignerEvent.Connect(this, &UVMappingEditor::OnDesignerNotifyHandler);
}

void UVMappingEditor::OrganizeToolbar(QBoxLayout* pTopLayout)
{
	QGridLayout* pLeftLayout = new QGridLayout;
	QGridLayout* pManipulationLayout = new QGridLayout;

	pTopLayout->addLayout(pLeftLayout, 0);
	pTopLayout->addLayout(pManipulationLayout, 1);

	RegisterMenuButtons(eUVMappingToolGroup_Manipulation, pManipulationLayout, 5);
	RegisterMenuButtons(eUVMappingToolGroup_Unwrapping, pLeftLayout, 3);

	m_pPlaneAxisComboBox = new QMenuComboBox;
	m_pPlaneAxisComboBox->SetMultiSelect(false);
	m_pPlaneAxisComboBox->SetCanHaveEmptySelection(false);

	int comboBoxPos = UVMappingToolGroupMapper::the().GetToolList(eUVMappingToolGroup_Unwrapping).size();
	pLeftLayout->addWidget(m_pPlaneAxisComboBox, comboBoxPos / 3, comboBoxPos % 3, 1, 1);
	for (int i = 0, iCount(Serialization::getEnumDescription<EPlaneAxis>().count()); i < iCount; ++i)
		m_pPlaneAxisComboBox->AddItem(Serialization::getEnumDescription<EPlaneAxis>().nameByIndex(i));

	QToolBar* pToolBar = new QToolBar;
	pToolBar->setIconSize(QSize(32, 32));
	pToolBar->setStyleSheet("QToolBar { border: 0px }");
	pToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	pLeftLayout->addWidget(pToolBar, 2, 0, 1, 3);

	m_pViewAllCheckBox = new QCheckBox("View All");
	pToolBar->addWidget(m_pViewAllCheckBox);
	QObject::connect(m_pViewAllCheckBox, SIGNAL(toggled(bool)), this, SLOT(OnViewAll(bool)));

	m_pSubMatComboBox = new QMaterialComboBox;
	pToolBar->addWidget(m_pSubMatComboBox);

	m_pPivotComboBox = new QMenuComboBox;
	m_pPivotComboBox->SetMultiSelect(false);
	m_pPivotComboBox->SetCanHaveEmptySelection(false);
	m_pPivotComboBox->AddItem("Selection", QVariant((int)ePivotType_Selection));
	m_pPivotComboBox->AddItem("Cursor", QVariant((int)ePivotType_Cursor));
	m_pPivotComboBox->SetChecked(0, true);
	pToolBar->addWidget(m_pPivotComboBox);

	m_pRotationPushBox = new QToolButton;
	m_pRotationPushBox->setText("Rotate Camera");
	m_pRotationPushBox->setIcon(CryIcon("icons:Designer/Designer_UV_Rotate_Camera.ico"));
	m_pRotationPushBox->setIconSize(QSize(24, 24));
	m_pRotationPushBox->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_pRotationPushBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

	pToolBar->addWidget(m_pRotationPushBox);
	QObject::connect(m_pRotationPushBox, &QToolButton::clicked, this, [ = ] { OnRotateCamera();
	                 });

	m_bViewAllUVIslands = false;
	m_PrevPivotType = ePivotType_Selection;
}

void UVMappingEditor::RegisterMenuButtons(EUVMappingToolGroup what, QGridLayout* where, int columnNumber)
{
	std::vector<EUVMappingTool> tools = UVMappingToolGroupMapper::the().GetToolList(what);
	int counter = 0;
	auto iTool = tools.begin();
	for (; iTool != tools.end(); ++iTool)
	{
		EUVMappingTool tool = *iTool;
		const char* szName = GetUVMapptingToolDesc().name(tool);
		QString icon = QString("icons:Designer/Designer_UV_%1.ico").arg(szName);
		icon.replace(" ", "_");

		QToolButton* pButton = new QToolButton();
		pButton->setText(szName);
		pButton->setIcon(CryIcon(icon));
		pButton->setIconSize(QSize(24, 24));
		pButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		pButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		pButton->setCheckable(true);
		QObject::connect(pButton, &QToolButton::clicked, this, [ = ] { SetTool(tool);
		                 });
		where->addWidget(pButton, counter / columnNumber, counter % columnNumber, 1, 1);
		m_pButtons[tool] = pButton;
		++counter;
	}
}

void UVMappingEditor::OnMouseEvent(const SMouseEvent& me_)
{
	BaseTool* pTool = GetCurrentTool();
	SMouseEvent me = me_;
	if (!me.viewport || !pTool)
		return;

	if (me.type == SMouseEvent::TYPE_PRESS && me.button == SMouseEvent::BUTTON_LEFT)
	{
		int axis;
		m_bLButtonDown = true;
		m_bHitGizmo = m_pGizmo->HitTest(me.viewport, me.x, me.y, axis);
		if (!m_bHitGizmo)
			pTool->OnLButtonDown(me);
	}

	if (me.type == SMouseEvent::TYPE_RELEASE && me.button == SMouseEvent::BUTTON_LEFT)
	{
		m_bLButtonDown = false;
		pTool->OnLButtonUp(me);
	}

	if (me.type == SMouseEvent::TYPE_MOVE)
	{
		if (m_bLButtonDown && !m_bHitGizmo)
		{
			me.button = SMouseEvent::BUTTON_LEFT;
			pTool->OnMouseMove(me);
		}
		else
		{
			pTool->OnMouseMove(me);
		}
	}
}

void UVMappingEditor::OnRender(const SRenderContext& rc)
{
	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();

	DisplayContext dc;
	m_pViewportAdapter->EnableXYViewport(true);
	dc.SetView(m_pViewportAdapter.get());

	aux->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);

	if (m_pObject && m_pMaterialMesh && m_pMaterialMesh->GetRenderNode())
		m_pMaterialMesh->GetRenderNode()->Render(*rc.renderParams, *rc.passInfo);

	dc.DepthTestOff();

	RenderUnwrappedMesh(dc);
	RenderElements(dc, ColorB(255, 200, 50), m_ElementSet);
	RenderElements(dc, ColorB(50, 200, 255), m_SharedElementSet);

	if (GetCurrentTool())
		GetCurrentTool()->Display(dc);

	if (m_pGizmo)
		m_pGizmo->Draw(dc);

	const float cursorRadius = 0.01f;
	m_pUVCursor->SetRadius(dc.view->GetScreenScaleFactor(m_pUVCursor->GetPos()) * cursorRadius * 0.02f);
	if (m_pUVCursor)
		m_pUVCursor->Draw(dc);

	dc.DepthTestOn();
}

void UVMappingEditor::OnIdle()
{
	if (m_pViewport)
		m_pViewport->Update();

	if (m_PrevPivotType != GetPivotType())
	{
		if (GetPivotType() == ePivotType_Cursor)
			GetGizmo()->SetPos(GetUVCursor()->GetPos());
		else
			GetGizmo()->SetPos(GetElementSet()->GetCenterPos());
	}

	m_PrevPivotType = GetPivotType();

	GetGizmo()->Hide(GetElementSet()->IsEmpty());
}

void UVMappingEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEditModeChange:
		switch (GetIEditor()->GetEditMode())
		{
		case eEditModeMove:
			OnTranslation();
			break;

		case eEditModeRotate:
			OnRotation();
			break;

		case eEditModeScale:
			OnScale();
			break;
		}
		break;

	case eNotify_OnIdleUpdate:
		OnIdle();
		UpdateObject();
		break;

	case eNotify_OnEditToolEndChange:
		InitializeMaterialComboBox();
		break;

	case eNotify_OnSelectionChange:
		ClearSelection();
		break;

	case eNotify_OnEndLoad:
	case eNotify_OnEndNewScene:
		CreateTexturePanelMesh();
		break;

	case eNotify_OnBeginLoad:
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneClose:
		m_pObject = NULL;
		m_pMaterialMesh = NULL;
		break;
	}
}

void UVMappingEditor::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (!pItem)
		return;

	if (event == EDB_ITEM_EVENT_SELECTED)
		m_pSubMatComboBox->SelectMaterial(DesignerSession::GetInstance()->GetBaseObject(), (CMaterial*)pItem);
}

void UVMappingEditor::UpdateObject()
{
	std::vector<DesignerObject*> objects = GetSelectedDesignerObjects();
	if (!objects.empty())
	{
		m_pObject = objects[0];
		if (!m_pMaterialMesh)
			return;

		if (m_pObject->GetMaterial() && m_pObject->GetMaterial()->GetMatInfo())
		{
			if (m_pObject->GetMaterial()->GetSubMaterialCount() > 0)
			{
				IMaterial* pMat = m_pObject->GetMaterial()->GetMatInfo()->GetSubMtl(GetSubMatID());
				m_pMaterialMesh->SetMaterial(pMat);
			}
			else
			{
				m_pMaterialMesh->SetMaterial(m_pObject->GetMaterial()->GetMatInfo());
			}
		}
		else
		{
			m_pMaterialMesh->SetMaterial(NULL);
		}
	}
	else
	{
		m_pObject = NULL;
		if (m_pMaterialMesh)
			m_pMaterialMesh->SetMaterial(NULL);
	}
}

void UVMappingEditor::RenderUnwrappedMesh(DisplayContext& dc)
{
	if (!m_pObject)
		return;

	UVIslandManager* pUVIslandMgr = m_pObject->GetModel()->GetUVIslandMgr();
	for (int i = 0, iIslandCount(pUVIslandMgr->GetCount()); i < iIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		const UVElement* pElement = GetElementSet()->FindElement(UVElement(pUVIsland));
		ColorB color = pElement ? ColorB(255, 200, 50) : ColorB(255, 255, 255);
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			RenderUnwrappedPolygon(dc, pUVIsland, polygon, color);
		}
	}
}

void UVMappingEditor::RenderUnwrappedPolygon(DisplayContext& dc, UVIslandPtr pUVIsland, PolygonPtr pPolygon, ColorB color)
{
	if (!pPolygon || (!m_bViewAllUVIslands && pPolygon->GetSubMatID() != GetSubMatID()))
		return;

	dc.SetColor(color);
	for (int i = 0, iEdgeCount(pPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		const IndexPair& edge = pPolygon->GetEdgeIndexPair(i);

		UVVertex uv0(pUVIsland, pPolygon, edge.m_i[0]);
		UVVertex uv1(pUVIsland, pPolygon, edge.m_i[1]);
		UVEdge uvEdge(pUVIsland, uv0, uv1);

		if (!GetElementSet()->FindElement(UVElement(uvEdge)) && !GetSharedElementSet()->FindElement(UVElement(uvEdge)))
		{
			const Vec2& uv0 = pPolygon->GetUV(edge.m_i[0]);
			const Vec2& uv1 = pPolygon->GetUV(edge.m_i[1]);

			dc.DrawLine(Vec3(uv0.x, uv0.y, 0.01f), Vec3(uv1.x, uv1.y, 0.01f));
		}
	}
}

int UVMappingEditor::GetSubMatID() const
{
	int subMatID = m_pSubMatComboBox->GetSubMatID();
	if (subMatID == -1)
		return 0;
	return subMatID;
}

void UVMappingEditor::SetSubMatID(int nSubMatID)
{
	if (nSubMatID >= 0 && nSubMatID < m_pSubMatComboBox->GetItemCount())
	{
		m_pSubMatComboBox->SetCurrentIndex(nSubMatID);
	}
}

void UVMappingEditor::RenderElements(DisplayContext& dc, ColorB color, UVElementSetPtr pUVElementSet)
{
	dc.SetColor(color);
	for (int i = 0, iCount(pUVElementSet->GetCount()); i < iCount; ++i)
	{
		const UVElement& element = pUVElementSet->GetElement(i);
		if (!GetUVEditor()->AllIslandsViewed() && element.m_pUVIsland->GetSubMatID() != GetSubMatID())
			continue;

		if (element.IsPolygon())
		{
			PolygonPtr polygon = element.m_UVVertices[0].pPolygon;
			for (int k = 0, iEdgeCount(polygon->GetEdgeCount()); k < iEdgeCount; ++k)
			{
				const IndexPair& e = polygon->GetEdgeIndexPair(k);
				const Vec2& uv0 = polygon->GetUV(e.m_i[0]);
				const Vec2& uv1 = polygon->GetUV(e.m_i[1]);
				dc.DrawLine(uv0, uv1);
			}
		}
		else if (element.IsEdge())
		{
			int iVertexCount = element.m_UVVertices.size();
			for (int k = 0; k < iVertexCount - 1; ++k)
			{
				const Vec2& uv0 = element.m_UVVertices[k].GetUV();
				const Vec2& uv1 = element.m_UVVertices[(k + 1) % iVertexCount].GetUV();
				dc.DrawLine(uv0, uv1);
			}
		}
		else if (element.IsVertex())
		{
			const Vec3& uv = element.m_UVVertices[0].GetUV();
			float size = 0.0002f * dc.view->GetScreenScaleFactor(uv);
			dc.DrawQuad(uv - Vec3(-size, -size, 0), uv + Vec3(size, -size, 0), uv - Vec3(size, size, 0), uv + Vec3(-size, size, 0));
		}
	}
}

void UVMappingEditor::SetTool(EUVMappingTool tool)
{
	//Note: the "tool" design is also being used for tools that behave more like buttons and not like modes
	//These tools reset the previous tool on Enter(), however this can lead to stack overflows if two of these tools are
	//called one after another, so be careful!
	if (m_Tool == tool)
		return;

	if (m_ToolMap.find(m_Tool) != m_ToolMap.end())
	{
		if (GetDesigner())
			m_ToolMap[m_Tool]->Leave();
	}

	m_PrevTool = m_Tool;
	m_Tool = tool;

	if (m_ToolMap.find(m_Tool) == m_ToolMap.end())
		m_ToolMap[m_Tool] = UVMappingToolFactory::the().Create(m_Tool);

	if (GetDesigner())
		m_ToolMap[m_Tool]->Enter();

	for (auto it = m_pButtons.begin(); it != m_pButtons.end(); ++it)
	{
		it->second->setChecked(false);
	}

	m_pButtons[m_Tool]->setChecked(true);
}

UVIslandManager* UVMappingEditor::GetUVIslandMgr() const
{
	Model* pModel = DesignerSession::GetInstance()->GetModel();
	if (!pModel)
		return nullptr;
	return pModel->GetUVIslandMgr();
}

EPivotType UVMappingEditor::GetPivotType() const
{
	const int currentIndex = m_pPivotComboBox->GetCheckedItem();
	return (EPivotType)m_pPivotComboBox->GetData(currentIndex).toInt();
}

EPlaneAxis UVMappingEditor::GetPlaneAxis() const
{
	return (EPlaneAxis)m_pPlaneAxisComboBox->GetCheckedItem();
}

// I just love duplication
Model* UVMappingEditor::GetModel() const
{
	return DesignerSession::GetInstance()->GetModel();
}

CBaseObject* UVMappingEditor::GetBaseObject() const
{
	return DesignerSession::GetInstance()->GetBaseObject();
}

ElementSet* UVMappingEditor::GetSelectedElements() const
{
	return DesignerSession::GetInstance()->GetSelectedElements();
}

const CCamera* UVMappingEditor::Get3DViewportCamera() const
{
	if (GetDesigner() == NULL)
		return NULL;
	return &GetDesigner()->GetCamera();
}

void UVMappingEditor::ApplyPostProcess(int updateType)
{
	if (GetDesigner() == NULL)
		return;
	Designer::MainContext mc = DesignerSession::GetInstance()->GetMainContext();
	Designer::ApplyPostProcess(mc, updateType);
}

void UVMappingEditor::CompileShelf(ShelfID shelf)
{
	Designer::MainContext mc = DesignerSession::GetInstance()->GetMainContext();
	if (!mc.IsValid())
		return;
	mc.pCompiler->Compile(mc.pObject, mc.pModel, shelf);
}

void UVMappingEditor::OnGizmoLMBDown(int mode)
{
	if (GetCurrentTool())
		GetCurrentTool()->OnGizmoLMBDown(mode);
}

void UVMappingEditor::OnGizmoLMBUp(int mode)
{
	if (GetCurrentTool())
		GetCurrentTool()->OnGizmoLMBUp(mode);
}

void UVMappingEditor::OnTransformGizmo(int mode, const Vec3& offset)
{
	if (GetCurrentTool())
		GetCurrentTool()->OnTransformGizmo(mode, offset);
	if (mode == CAxisHelper::MOVE_MODE)
		m_pGizmo->MovePos(offset);
}

void UVMappingEditor::OnTranslation()
{
	m_pGizmo->SetMode(eGizmoMode_Translation);
}

void UVMappingEditor::OnRotation()
{
	m_pGizmo->SetMode(eGizmoMode_Rotation);
}

void UVMappingEditor::OnScale()
{
	m_pGizmo->SetMode(eGizmoMode_Scale);
}

void UVMappingEditor::OnUndo()
{
	GetIEditor()->GetIUndoManager()->Undo();
}

void UVMappingEditor::OnRedo()
{
	GetIEditor()->GetIUndoManager()->Redo();
}

void UVMappingEditor::OnUnmap()
{
	SetTool(eUVMappingTool_Unmap);
}

void UVMappingEditor::OnRotateCamera()
{
	SViewportState vs = m_pViewport->GetState();
	m_CameraZAngle += PI * 0.5f;
	if (m_CameraZAngle + 0.01f > 2.0f * PI)
		m_CameraZAngle = 0;
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(vs.cameraTarget.q));
	ypr.x = m_CameraZAngle;
	vs.cameraTarget.q = Quat(CCamera::CreateOrientationYPR(ypr));
	m_pViewport->SetState(vs);
	m_pViewport->Camera()->SetMatrix(Matrix34(vs.cameraTarget));
}

void UVMappingEditor::OnGoto()
{
	UVElementSetPtr pElementSet = GetElementSet();
	if (pElementSet->IsEmpty())
		return;

	Vec3 centerPos = pElementSet->GetCenterPos();
	CCamera* camera = m_pViewport->Camera();
	Vec3 cameraPos = camera->GetPosition();
	SViewportState state = m_pViewport->GetState();
	if (cameraPos.z < 20.0f)
		cameraPos.z = 100.0f;
	state.cameraTarget.SetTranslation(Vec3(centerPos.x, centerPos.y, cameraPos.z));
	m_pViewport->SetState(state);
}

void UVMappingEditor::OnSelectAll()
{
	UVElementSetPtr pElementSet = GetElementSet();

	if (pElementSet->IsEmpty())
	{
		UVIslandManager* pUVIslandMgr = GetUVIslandMgr();
		for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
		{
			UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
			pElementSet->AddElement(UVElement(pUVIsland));
		}
	}
	else
	{
		ClearSelection();
	}

	ClearSharedSelection();
	SyncSelection();
}

void UVMappingEditor::OnViewAll(bool bViewAll)
{
	m_bViewAllUVIslands = bViewAll;
}

void UVMappingEditor::OnUpdateUVIslands()
{
	if (GetUVIslandMgr())
	{
		Model* pModel = DesignerSession::GetInstance()->GetModel();

		GetUVIslandMgr()->Clean(pModel);
		GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	}
}

void UVMappingEditor::UpdateSubMatComboBox()
{
	m_pSubMatComboBox->FillWithSubMaterials();

	if (GetUVIslandMgr())
	{
		for (int i = 0, iCount(m_pSubMatComboBox->GetItemCount()); i < iCount; ++i)
		{
			if (GetUVIslandMgr()->HasSubMatID(i))
			{
				m_pSubMatComboBox->SetCurrentIndex(i);
				break;
			}
		}
	}
}

void UVMappingEditor::UpdateSelectionFromDesigner()
{
	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();
	ClearSelection();

	for (int i = 0, iCount(pElements->GetCount()); i < iCount; ++i)
	{
		const Element& element = pElements->Get(i);
		if (element.IsVertex())
		{
			std::vector<UVVertex> uvVertices;
			SearchUtil::FindUVVerticesFromPos(element.GetPos(), GetSubMatID(), uvVertices);
			auto ii = uvVertices.begin();
			for (; ii != uvVertices.end(); ++ii)
				GetElementSet()->AddElement(UVElement(*ii));
		}
		else if (element.IsEdge())
		{
			std::vector<UVEdge> uvEdges;
			SearchUtil::FindUVEdgesFromEdge(element.GetEdge(), GetSubMatID(), uvEdges);
			auto ii = uvEdges.begin();
			for (; ii != uvEdges.end(); ++ii)
				GetElementSet()->AddElement(UVElement(*ii));
		}
		else if (element.IsPolygon())
		{
			std::vector<UVPolygon> uvPolygons;
			SearchUtil::FindUVPolygonsFromPolygon(element.m_pPolygon, GetSubMatID(), uvPolygons);
			auto ii = uvPolygons.begin();
			for (; ii != uvPolygons.end(); ++ii)
				GetElementSet()->AddElement(UVElement(*ii));
		}
	}

	UpdateGizmoPos();
}

void UVMappingEditor::UpdateSharedSelection()
{
	m_SharedElementSet->Clear();

	for (int i = 0, iCount(m_ElementSet->GetCount()); i < iCount; ++i)
		m_SharedElementSet->AddSharedOtherElements(m_ElementSet->GetElement(i));

	for (int i = 0, iCount(m_ElementSet->GetCount()); i < iCount; ++i)
		m_SharedElementSet->EraseAllElementWithIdenticalUVs(m_ElementSet->GetElement(i));
}

void UVMappingEditor::InitializeMaterialComboBox()
{
	m_pSubMatComboBox->signalCurrentIndexChanged.Connect(this, &UVMappingEditor::OnSubMatSelectionChanged);

	UpdateSubMatComboBox();
}

void UVMappingEditor::UpdateGizmoPos()
{
	Vec3 pivot(0, 0, 0);
	UVElementSetPtr pElements = GetElementSet();

	if (GetPivotType() == ePivotType_Selection)
	{
		if (!pElements->IsEmpty())
			GetGizmo()->SetPos(pElements->GetCenterPos());
	}
	else if (GetPivotType() == ePivotType_Cursor)
	{
		UVCursorPtr pUVCursor = GetUVCursor();
		GetGizmo()->SetPos(pUVCursor->GetPos());
	}
}

void UVMappingEditor::OnSubMatSelectionChanged(int nSelectedItem)
{
	if (m_pSubMatComboBox->GetCurrentIndex() != nSelectedItem && nSelectedItem != -1)
	{
		m_pSubMatComboBox->SetCurrentIndex(nSelectedItem);

		DesignerSession::GetInstance()->SetCurrentSubMatID(nSelectedItem);
	}
	ClearSelection();
}

void UVMappingEditor::OnDesignerNotifyHandler(EDesignerNotify notification, void* param)
{
	switch (notification)
	{
	case eDesignerNotify_RefreshSubMaterialList:
	case eDesignerNotify_ObjectSelect:
		UpdateSubMatComboBox();
		break;
	case eDesignerNotify_SubMaterialSelectionChanged:
		{
			MaterialChangeEvent* evt = static_cast<MaterialChangeEvent*>(param);
			uintptr_t nSelectedItem = evt->matID;
			if (m_pSubMatComboBox->GetCurrentIndex() != nSelectedItem && nSelectedItem != -1)
			{
				m_pSubMatComboBox->SetCurrentIndex(nSelectedItem);
			}
			ClearSelection();
			break;
		}
	case eDesignerNotify_Select:
		UpdateSelectionFromDesigner();
		break;
	case eDesignerNotify_PolygonsModified:
	case eDesignerNotify_BeginDesignerSession:
		RefreshPolygonsInUVIslands();
		GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
		OnUpdateUVIslands();
		break;
	}
}

void UVMappingEditor::SelectUVIslands(const std::vector<UVIslandPtr>& uvIslands)
{
	ClearSelection();
	auto ii = uvIslands.begin();
	for (; ii != uvIslands.end(); ++ii)
		m_ElementSet->AddElement(UVElement(*ii));
}

void UVMappingEditor::RefreshPolygonsInUVIslands()
{
	UVIslandManager* pUVIslandMgr = GetUVIslandMgr();
	Model* pModel = DesignerSession::GetInstance()->GetModel();

	for (int i = 0, iUVIslandCount(pUVIslandMgr->GetCount()); i < iUVIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		pUVIsland->ResetPolygonsInUVIslands(pModel);
	}
}

void UVMappingEditor::ClearSelection()
{
	m_ElementSet->Clear();
	m_SharedElementSet->Clear();
}

void UVMappingEditor::ClearSharedSelection()
{
	m_SharedElementSet->Clear();
}

BaseTool* UVMappingEditor::GetCurrentTool() const
{
	auto iTool = m_ToolMap.find(m_Tool);
	if (iTool != m_ToolMap.end())
		return iTool->second;
	return NULL;
}
}
}

namespace DesignerUVMappingCommand
{
void PyTranslationMode()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnTranslation();
}
void PyRotationMode()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnRotation();
}
void PyScaleMode()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnScale();
}
void PySelectAll()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnSelectAll();
}
void PyRefresh()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnUpdateUVIslands();
}
void PyGoto()
{
	if (Designer::UVMapping::GetUVEditor())
		Designer::UVMapping::GetUVEditor()->OnGoto();
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PyTranslationMode, uvmapping, translation_mode, "Switch to translation mode", "uvmapping.translation_mode")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PyRotationMode, uvmapping, rotation_mode, "Switch to rotation mode", "uvmapping.rotation_mode")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PyScaleMode, uvmapping, scale_mode, "Switch to scale mode", "uvmapping.scale_mode")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PySelectAll, uvmapping, select_all, "Select all", "uvmapping.select_all")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PyRefresh, uvmapping, refresh, "Refresh islands to remove unused and sync", "uvmapping.refresh")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::PyGoto, uvmapping, goto, "Moves the camera to the selected island", "uvmapping.goto")

