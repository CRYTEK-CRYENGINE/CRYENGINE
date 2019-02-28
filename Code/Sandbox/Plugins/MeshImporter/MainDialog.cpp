// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// FbxTool main dialog

#include "StdAfx.h"
#include "MainDialog.h"
#include "SandboxPlugin.h"

#include "AsyncTasks.h"
#include "CallRcTask.h"
#include "RcLoader.h"
#include "SceneModel.h"
#include "SceneView.h"
#include "SceneContextMenu.h"
#include "MaterialElement.h"
#include "MaterialModel.h"
#include "MaterialView.h"
#include "TargetMesh.h"
#include "DisplayOptions.h"
#include "TextureManager.h"
#include "GlobalImportSettings.h"
#include "MaterialSettings.h"
#include "AutoLodSettings.h"
#include "EditorMetaData.h"
#include "FilePathUtil.h"
#include "MaterialHelpers.h"
#include "MaterialPanel.h"
#include "ImporterUtil.h"
#include "ModelProperties/ModelProperties.h"
#include "Scene/Scene.h"
#include "Scene/SceneElementSourceNode.h"
#include "Scene/SceneElementPhysProxies.h"
#include "Scene/SceneElementProxyGeom.h"
#include "Scene/SceneElementTypes.h"
#include "Scene/SourceSceneHelper.h"
#include "Scene/ProxySceneHelper.h"
#include "ProxyGenerator/ProxyData.h"
#include "ProxyGenerator/ProxyGenerator.h"
#include "ProxyGenerator/PhysProxiesControlsWidget.h"
#include "ProxyGenerator/WriteProxies.h"
#include "DialogMesh/DialogMesh_SceneUserData.h"
#include "TempRcObject.h"
#include "RenderHelpers.h"
#include "AnimationHelpers/AnimationHelpers.h"
#include "SaveRcObject.h"

#include <CrySerialization/Serializer.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <Material/Material.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <IEditor.h>
#include <Controls\QuestionDialog.h>
#include <QtViewPane.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <ProxyModels/AttributeFilterProxyModel.h>
#include <../../CryEngine/Cry3DEngine/CGF/ChunkFile.h>
#include <../../CryEngine/Cry3DEngine/MeshCompiler/MeshCompiler.h>
#include <../../CryEngine/Cry3DEngine/MeshCompiler/TransformHelpers.h>
#include <Cry3DEngine/CGF/CGFContent.h>  // CGF_NODE_NAME_LOD_PREFIX
#include <Util/FileUtil.h>
#include <CryIcon.h>
#include <FileDialogs/SystemFileDialog.h>
#include <FileDialogs/ExtensionFilter.h>
#include <Material/MaterialManager.h>
#include <Material/MaterialHelpers.h>
#include <Controls/QMenuComboBox.h>
#include <ThreadingUtils.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QItemSelectionModel>
#include <QPixmap>
#include <QMenu>
#include <QMutexLocker>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QColorDialog>
#include <QTextStream>
#include <QTabWidget>
#include <QCloseEvent>
#include <QDir>
#include <QJsonDocument>
#include <QCheckBox>

#pragma warning(push)
#pragma warning(disable:4266) // no override available for virtual member function from base; function is hidden
#include <fbxsdk.h>
#pragma warning(pop)

#include <ppl.h>

void LogPrintf(const char* szformat, ...);

static const char* const kDefaultProxyMaterial = "%EDITOR%/Materials/areasolid";

static const QString s_initialFilePropertyName = QStringLiteral("initialFile");

// An object of type SDisplayScene stores a FBX scene, as well as the compiled meshes for the scene.
// At any point in time, there is at most one display scene that is considered the current display
// scene shown in the dialog (see CMainDialog::m_pDisplayScene).
//
// Multiple threads may read from the same display scene. Ownership is managed by std::shared_ptr,
// which guarantees that display scenes do not get destroyed until the last thread terminates (the
// control block of std::shared_ptr is thread safe.)
//
// Note that each "Import" action results in a display scene. So --- after the user reloads a scene,
// for example --- there may be multiple display scenes referring to the same file name.

struct SStaticObject
{
	IStatObj*             pStatObj;
	const FbxTool::SMesh* pMesh;
	AABB                  boundingBox;
	float                 selectionHeat;
	int                   uberMaterial;
	bool                  bSelected;

	SStaticObject()
		: pStatObj(nullptr)
		, pMesh(nullptr)
		, boundingBox(AABB::RESET)
		, selectionHeat(0.0f)
		, uberMaterial(-1)
		, bSelected(false)
	{}

	const FbxTool::SNode* GetNode() const         { return pMesh->pNode; }
	Matrix34              GetLocalToWorld() const { return pMesh->GetGlobalTransform(); }
};

struct CMainDialog::SDisplayScene
{
	QString                    cgfFilePath;
	string                     lastJson;
	std::vector<SStaticObject> statObjs;
};

////////////////////////////////////////////////////

// Scene context menu.
class CMainDialog::CSceneContextMenu : public CSceneContextMenuCommon
{
public:
	explicit CSceneContextMenu(CMainDialog* pOwner)
		: CSceneContextMenuCommon(pOwner->m_pSceneTree)
		, m_pOwner(pOwner)
	{
	}

private:
	virtual void AddSceneElementSection(CSceneElementCommon* pSceneElement) override
	{
		QModelIndex originalIndex = m_pOwner->m_pSceneModel->GetModelIndexFromSceneElement(pSceneElement);

		m_pOwner->AddSourceNodeElementContextMenu(originalIndex, this->GetMenu());
		AddProxyGenerationContextMenu(this->GetMenu(), m_pOwner->m_pSceneModel.get(), originalIndex, m_pOwner->m_pProxyGenerator.get());
	}

private:
	CMainDialog* m_pOwner;
};

////////////////////////////////////////////////////

void CMainDialog::OnNodeUserDataChanged(const FbxTool::SNode*)
{
	m_pSceneTree->repaint();
	m_pTargetMeshView->repaint();
	m_pPropertyTree->revert();
	m_bRefreshRcMesh = true;
}

void CMainDialog::OnMaterialUserDataChanged()
{
	m_bRefreshRcMesh = true;
}

bool CMainDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::Leave && pObject == m_pViewportContainer->GetSplitViewport())
	{
		ClearSelectionHighlighting();
	}
	return false;  // Forward event.
}

void CMainDialog::OnIdleUpdate()
{
	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->Update();
	m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport()->Update();
}

void CMainDialog::OnReleaseResources()
{
	// All static objects get destroyed when a new level is loaded, so we properly release the
	// RC mesh and avoid dangling references.
	ReleaseStaticMeshes();
	m_bRefreshRcMesh = true;
}

namespace
{

void SetMaterialDiffuseColor(IMaterial* pMaterial, const ColorF& color)
{
	if (!pMaterial)
	{
		assert(false);
		return;
	}

	pMaterial->SetGetMaterialParamVec3("diffuse", color.toVec3(), false);
}

ColorF GetMaterialDiffuseColor(IMaterial* pMaterial)
{
	assert(pMaterial);
	Vec3 v;
	pMaterial->SetGetMaterialParamVec3("diffuse", v, true);
	return ColorF(v.x, v.y, v.z);
}

void DrawBoxes(const AABB& sceneBox)
{
	IRenderer* const pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* const pAux = pRenderer->GetIRenderAuxGeom();

	const ColorB sceneBoxColor(255, 255, 255, 255);
	const ColorB unitBoxColor(255, 0, 128, 255);
	const AABB unitBox(Vec3(-0.5f, -0.5f, 0), Vec3(0.5f, 0.5f, 1));

	pAux->DrawAABB(unitBox, false, unitBoxColor, EBoundingBoxDrawStyle::eBBD_Faceted);
	pAux->DrawAABB(sceneBox, false, sceneBoxColor, EBoundingBoxDrawStyle::eBBD_Faceted);

	const Vec3 boxSize(sceneBox.GetSize());

	// TODO: what is x, y range?
	pAux->Draw2dLabel(100, 10, 1.5, ColorF(1, 1, 1), false,
	                  "box size = %g, %g, %g", boxSize.x, boxSize.y, boxSize.z);
}

} // namespace

void CMainDialog::OnViewportRender(const SRenderContext& rc)
{
	QViewport* const pViewport = rc.viewport;

	if (!GetScene())
	{
		return;
	}

	const FbxTool::CScene* const pScene = GetScene();

	// Update global scene scale.
	{
		const FbxMetaData::Units::EUnitSetting unit = m_pGlobalImportSettings->GetUnit();

		const double fileScale = pScene->GetFileUnitSizeInCm();

		const double cmToM = 0.01;

		// Scene scale accounts for both unit conversion and scale modifier.
		const double scale =
		  cmToM *
		  FbxMetaData::Units::GetUnitSizeInCm(unit, fileScale) *
		  m_pGlobalImportSettings->GetScale();

		GetScene()->SetGlobalScale((float)scale);
	}

	// Update global scene rotation.
	{
		// Compute rotation to CE coordinate system.

		const string dst = GetFormattedAxes(FbxTool::Axes::EAxis::PosZ, FbxTool::Axes::EAxis::NegY);
		const string src = GetFormattedAxes(
		  m_pGlobalImportSettings->GetUpAxis(),
		  m_pGlobalImportSettings->GetForwardAxis());

		Matrix34 transform = Matrix34(IDENTITY);
		const char* const errMsg = TransformHelpers::ComputeForwardUpAxesTransform(
		  transform, src.c_str(), dst.c_str());
		if (errMsg)
		{
			LogPrintf("ComputeForwardUpAxesTransform failed, reason: %s\n", errMsg);
		}
		assert(transform.GetColumn3() == Vec3(0.0f, 0.0f, 0.0f));

		Matrix33 rotation;
		transform.GetRotation33(rotation);

		GetScene()->SetGlobalRotation(rotation);
	}

	if (pViewport == m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport())
	{
		RenderCommon();

		const auto vpMode = m_viewSettings.viewportMode;
		if (vpMode == SViewSettings::eViewportMode_RcOutput ||
		    vpMode == SViewSettings::eViewportMode_Split)
		{
			RenderTargetView(rc);
		}
		else if (vpMode == SViewSettings::eViewportMode_SourceView)
		{
			RenderSourceView(rc);
		}
	}
	else
	{
		RenderSourceView(rc);
	}

	for (size_t i = 0; i < m_displayScene->statObjs.size(); ++i)
	{
		SStaticObject& statObj = m_displayScene->statObjs[i];

		if (!statObj.bSelected && statObj.selectionHeat > 0.0f)
		{
			statObj.selectionHeat -= kSelectionCooldownPerSec * gEnv->pSystem->GetITimer()->GetFrameTime();
		}
	}

	for (size_t i = 0; i < GetScene()->GetNodeCount(); ++i)
	{
		auto& renderInfo = m_nodeRenderInfoMap[i];
		if (!renderInfo.bSelected && renderInfo.selectionHeat > 0.0f)
		{
			renderInfo.selectionHeat -= kSelectionCooldownPerSec * gEnv->pSystem->GetITimer()->GetFrameTime();

			const FbxTool::SNode* const pNode = GetScene()->GetNodeByIndex(i);
			if (!IsNodeVisible(pNode))
			{
				renderInfo.selectionHeat = 0.0f;
			}
		}
	}
}

void CMainDialog::OnViewportMouse(const SMouseEvent& ev)
{
	QViewport* const pViewport = ev.viewport;

	if (!GetScene())
	{
		return;
	}

	if (ev.x < 0 || ev.y < 0 || !ev.viewport || ev.x > ev.viewport->width() || ev.y > ev.viewport->height())
	{
		ClearSelectionHighlighting();
		return;
	}

	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement(), true);
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->OnMouse(pPhysProxiesElement->GetPhysProxies(), ev, *ev.viewport->Camera());
			return;  // Silent return.
		}
	}

	const bool bViewportShowsSource =
	  pViewport == m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport() ||
	  m_viewSettings.viewportMode == SViewSettings::eViewportMode_SourceView;

	if (bViewportShowsSource)
	{
		MouseSourceView(ev);
	}
	else
	{
		MouseTargetView(ev);
	}
}

namespace Private_MainDialog
{

// eye, ray in world space.
static bool RaycastTransformedStatObj(IStatObj* pStatObj, const Vec3& eye, const Ray& ray, const Matrix34& localToWorld, float& outDist)
{
	const Matrix34 worldToLocal = localToWorld.GetInverted();

	Ray localRay; // In local space.
	localRay.origin = worldToLocal.TransformPoint(ray.origin);
	localRay.direction = worldToLocal.TransformVector(ray.direction);
	localRay.direction.Normalize();

	SRayHitInfo hitInfo;
	hitInfo.inRay = localRay;
	hitInfo.inReferencePoint = worldToLocal.TransformPoint(eye);
	const int flags = pStatObj->GetFlags();
	pStatObj->SetFlags(flags & ~STATIC_OBJECT_COMPOUND);
	bool ret = false;
	if (pStatObj->RayIntersection(hitInfo))
	{
		const Vec3 hit = localToWorld.TransformPoint(hitInfo.vHitPos); // In world space.
		const float dist = (hit - eye).len2();
		outDist = dist;
		ret = true;
	}
	pStatObj->SetFlags(flags);
	return ret;
}

// eye, ray in world space.
template<typename T, typename V>
static bool RaycastVisibleCollection(
  const Vec3& eye,
  const Ray& ray,
  V& isVisible,
  std::vector<T>& collection,
  int& outHitIndex)
{
	int hitIndex = -1;
	float hitDist = 0.0f;

	for (size_t i = 0; i < collection.size(); ++i)
	{
		T& element = collection[i];

		if (!isVisible(element.GetNode()))
		{
			continue;
		}

		IStatObj* const pStatObj = element.pStatObj;
		float dist;
		if (pStatObj && RaycastTransformedStatObj(pStatObj, eye, ray, element.GetLocalToWorld(), dist))
		{
			if (hitIndex < 0 || hitDist > dist)
			{
				hitIndex = (int)i;
				hitDist = dist;
			}
		}
	}

	outHitIndex = hitIndex;
	return hitIndex >= 0;
}

// CShowMeshesModeWidget

class CShowMeshesModeWidget : public QWidget
{
public:
	enum EShowMeshesMode
	{
		eShowMeshesMode_All,
		eShowMeshesMode_SelectedNode,
		eShowMeshesMode_SelectedSubtree
	};

	explicit CShowMeshesModeWidget(QWidget* pParent = nullptr)
		: QWidget(pParent)
	{
		m_pShowMeshesMode = new QMenuComboBox();

		m_pShowMeshesMode->Clear();
		m_pShowMeshesMode->AddItem(tr("Show all meshes"), eShowMeshesMode_All);
		m_pShowMeshesMode->AddItem(tr("Show selected node"), eShowMeshesMode_SelectedNode);
		m_pShowMeshesMode->AddItem(tr("Show selected subtree"), eShowMeshesMode_SelectedSubtree);

		QVBoxLayout* const pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->addWidget(m_pShowMeshesMode);
		setLayout(pLayout);
	}

	EShowMeshesMode GetShowMeshesMode() const
	{
		const int currentIndex = m_pShowMeshesMode->GetCheckedItem();
		CRY_ASSERT(currentIndex >= 0);  // Is single-selection?
		return (EShowMeshesMode)m_pShowMeshesMode->GetData(currentIndex).toInt();
	}
private:
	QMenuComboBox* m_pShowMeshesMode;
};

} // namespace Private_MainDialog

void CMainDialog::MouseTargetView(const SMouseEvent& ev)
{
	using namespace Private_MainDialog;

	QViewport* pViewport = ev.viewport;

	ClearSelectionHighlighting();

	const Vec3 eye_ws = pViewport->Camera()->GetPosition();

	Ray ray_ws;
	if (!pViewport->ScreenToWorldRay(&ray_ws, ev.x, ev.y))
	{
		return;
	}

	if (!FindUnmergedMesh())
	{
		return;
	}

	int hitIndex = -1;
	auto isVisible = std::bind(&CMainDialog::IsNodeVisible, this, std::placeholders::_1);
	if (!RaycastVisibleCollection(eye_ws, ray_ws, isVisible, m_rcMeshUnmergedContent, hitIndex))
	{
		return;
	}

	SStatObjChild& hitChild = m_rcMeshUnmergedContent[hitIndex];

	// Highlight hit object.
	for (size_t i = 0; i < m_pSceneModel->GetElementCount(); ++i)
	{
		auto pSceneElement = m_pSceneModel->GetElementById(i);

		if (pSceneElement->GetType() != ESceneElementType::SourceNode)
		{
			continue;
		}

		const FbxTool::SNode* pNode = ((CSceneElementSourceNode*)pSceneElement)->GetNode();
		if (pNode == hitChild.pNode)
		{
			m_nodeRenderInfoMap[pNode->id].bSelected = true;
			m_nodeRenderInfoMap[pNode->id].selectionHeat = 1.0f;
			m_targetHitInfo.szLabel = pNode->szName;
		}
	}

	if (SMouseEvent::BUTTON_LEFT == ev.button)
	{
		SelectSceneElementWithNode(m_pSceneModel.get(), m_pSceneTree, hitChild.pNode);
	}
}

void CMainDialog::MouseSourceView(const SMouseEvent& ev)
{
	using namespace Private_MainDialog;

	QViewport* const pViewport = ev.viewport;

	// Reset selection.
	for (size_t i = 0; i < m_displayScene->statObjs.size(); ++i)
	{
		m_displayScene->statObjs[i].bSelected = false;
	}
	m_sourceHitInfo.szLabel = "";

	const Vec3 eye_ws = pViewport->Camera()->GetPosition();

	Ray ray_ws;
	if (!pViewport->ScreenToWorldRay(&ray_ws, ev.x, ev.y))
	{
		return;
	}

	int hitIndex = -1;
	auto isVisible = std::bind(&CMainDialog::IsNodeVisible, this, std::placeholders::_1);
	if (!RaycastVisibleCollection(eye_ws, ray_ws, isVisible, m_displayScene->statObjs, hitIndex))
	{
		return;
	}

	const FbxTool::SNode* const pNode = m_displayScene->statObjs[hitIndex].pMesh->pNode;

	// Highlight hit object.
	m_displayScene->statObjs[hitIndex].bSelected = true;
	m_displayScene->statObjs[hitIndex].selectionHeat = 1.0f;
	m_sourceHitInfo.szLabel = pNode->szName;

	if (SMouseEvent::BUTTON_LEFT == ev.button)
	{
		SelectSceneElementWithNode(m_pSceneModel.get(), m_pSceneTree, pNode);
	}
}

bool CMainDialog::IsNodeVisible(const FbxTool::SNode* pNode) const
{
	using namespace Private_MainDialog;

	if (m_pShowMeshesModeWidget->GetShowMeshesMode() == CShowMeshesModeWidget::eShowMeshesMode_All)
	{
		CSceneElementSourceNode* const pSceneElement = m_pSceneModel->FindSceneElementOfNode(pNode);
		CRY_ASSERT(pSceneElement);

		const FbxTool::SNode* const pNode = pSceneElement->GetNode();

		return pNode->pScene->GetNodeLod(pNode) == m_viewSettings.lod;
	}
	else
	{
		return m_nodeVisibilityMap[pNode->id];
	}
}

static void DrawPivot(IRenderAuxGeom& aux, const ColorB& col, const Vec3& p)
{
	const float scale = 0.15f;
	const float lineWidth = 4.0f;
	const ColorB cx(255, 0, 0, 255);
	const ColorB cy(0, 255, 0, 255);
	const ColorB cz(0, 0, 255, 255);
	aux.DrawLine(p + Vec3(-scale, 0, 0), cx, p, cx, lineWidth);
	aux.DrawLine(p + Vec3(0, -scale, 0), cy, p, cy, lineWidth);
	aux.DrawLine(p + Vec3(0, 0, -scale), cz, p, cz, lineWidth);
	aux.DrawLine(p, cx, p + Vec3(scale, 0, 0), cx, lineWidth);
	aux.DrawLine(p, cy, p + Vec3(0, scale, 0), cy, lineWidth);
	aux.DrawLine(p, cz, p + Vec3(0, 0, scale), cz, lineWidth);
}

void CMainDialog::ClearSelectionHighlighting()
{
	for (size_t i = 0; i < m_nodeRenderInfoMap.size(); ++i)
	{
		m_nodeRenderInfoMap[i].bSelected = false;
	}
	m_targetHitInfo.szLabel = "";
}

void CMainDialog::MarkVisibleNodes()
{
	using namespace Private_MainDialog;

	m_nodeVisibilityMap.clear();
	m_nodeVisibilityMap.resize(GetScene()->GetNodeCount() + 1, 0);

	const int showMeshesMode = m_pShowMeshesModeWidget->GetShowMeshesMode();

	std::vector<const FbxTool::SNode*> elementStack;
	elementStack.reserve(GetScene()->GetNodeCount());

	// Add roots of subtrees for traversal.

	if (showMeshesMode == CShowMeshesModeWidget::eShowMeshesMode_All)
	{
		CSceneElementSourceNode* pRootElement = m_pSceneModel->GetRootElement();
		elementStack.push_back(pRootElement->GetNode());
	}
	else if (
	  showMeshesMode == CShowMeshesModeWidget::eShowMeshesMode_SelectedSubtree ||
	  showMeshesMode == CShowMeshesModeWidget::eShowMeshesMode_SelectedNode)
	{
		QItemSelectionModel* const pSelectionModel = m_pSceneTree->selectionModel();
		QList<QModelIndex> selectedIndices = pSelectionModel->selection().indexes();

		const auto* const pFilter = m_pSceneViewContainer->GetFilter();

		for (int i = 0; i < selectedIndices.size(); ++i)
		{
			CSceneElementCommon* const pSceneElement = m_pSceneModel->GetSceneElementFromModelIndex(pFilter->mapToSource(selectedIndices[i]));
			if (pSceneElement->GetType() == ESceneElementType::SourceNode)
			{
				const FbxTool::SNode* const pNode = ((CSceneElementSourceNode*)pSceneElement)->GetNode();
				elementStack.push_back(pNode);
			}
		}
	}

	while (!elementStack.empty())
	{
		const FbxTool::SNode* const pNode = elementStack.back();
		elementStack.pop_back();

		if (pNode->pParent &&
		    pNode->numMeshes &&
		    (m_viewSettings.bShowProxies || !pNode->pScene->IsProxy(pNode)))
		{
			m_nodeVisibilityMap[pNode->id] = true;
		}

		if (showMeshesMode != CShowMeshesModeWidget::eShowMeshesMode_SelectedNode)
		{
			for (int j = 0; j < pNode->numChildren; ++j)
			{
				elementStack.push_back(pNode->ppChildren[j]);
			}
		}
	}
}

void CMainDialog::RenderCommon()
{
	MarkVisibleNodes();
}

namespace Private_MainDialog
{

static SGeometryDebugDrawInfo CreateDebugDrawInfo()
{
	SGeometryDebugDrawInfo debugDrawInfo;
	debugDrawInfo.bDrawInFront = true;
	debugDrawInfo.color = ColorB(0, 0, 0, 0);
	debugDrawInfo.lineColor = ColorB(255, 255, 0, 160);
	return debugDrawInfo;
}

std::vector<CAutoLodSettings::sNode> GetAutoLodNodes(const FbxTool::CScene* pScene)
{
	std::vector<const FbxTool::SNode*> nodeStack;

	std::vector<CAutoLodSettings::sNode> res;

	const FbxTool::SNode* const pRoot = pScene->GetRootNode();
	nodeStack.push_back(pRoot);
	while (nodeStack.size() != 0)
	{
		const FbxTool::SNode* node = nodeStack.back();
		nodeStack.pop_back();

		if (node == NULL)
			continue;

		const CAutoLodSettings::sNodeParam nodeParam = pScene->GetAutoLodProperties(node);

		if (nodeParam.m_bAutoGenerate)
		{
			CAutoLodSettings::sNode snode;
			snode.m_nodeName = node->szName;
			snode.m_nodeParam = nodeParam;
			res.push_back(snode);
		}

		for (int i = 0; i < node->numChildren; ++i)
		{
			if (node->ppChildren[i] != NULL)
				nodeStack.push_back(node->ppChildren[i]);
		}
	}

	return res;
}

} // namespace Private_MainDialog

void CMainDialog::RenderStaticMesh(const SRenderContext& rc)
{
	using namespace Private_MainDialog;

	string frameText;

	if (!m_displayScene)
	{
		// Nothing to draw.
		return;
	}

	// Render proxies.
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement(), true);
		if (pPhysProxiesElement)
		{
			SPhysProxies* const pPhysProxies = pPhysProxiesElement->GetPhysProxies();

			CProxyGenerator::SShowParams showParams;
			showParams.sourceVisibility = m_pPhysProxiesControls->GetSourceVisibility();
			showParams.primitivesVisibility = m_pPhysProxiesControls->GetPrimitivesVisibility();
			showParams.meshesVisibility = m_pPhysProxiesControls->GetMeshesVisibility();
			showParams.bShowVoxels = m_pPhysProxiesControls->IsVoxelsVisible();

			phys_geometry* pSelectedProxyGeom = nullptr;
			if (GetSelectedSceneElement()->GetType() == ESceneElementType::ProxyGeom)
			{
				pSelectedProxyGeom = ((CSceneElementProxyGeom*)GetSelectedSceneElement())->GetPhysGeom();
			}

			m_pProxyGenerator->Render(pPhysProxies, pSelectedProxyGeom, rc, showParams);

			return;  // Silent return.
		}
	}

	// Refresh RC mesh, if necessary.
	const bool bRcMeshVisible = m_viewSettings.viewportMode != SViewSettings::eViewportMode_SourceView;
	if (bRcMeshVisible && m_bRefreshRcMesh && !IsLoadingSuspended())
	{
		UpdateResources();
		m_bRefreshRcMesh = false;
	}

	// Update mesh material, if necessary.
	if (m_pMeshStatObj && m_pMaterialPanel->GetMaterialSettings()->GetMaterial() && m_pMeshStatObj->GetMaterial() != m_pMaterialPanel->GetMaterialSettings()->GetMaterial()->GetMatInfo())
	{
		m_pMeshStatObj->SetMaterial(m_pMaterialPanel->GetMaterialSettings()->GetMaterial()->GetMatInfo());
	}

	AABB sceneBox(AABB::RESET);

	if (m_pShowMeshesModeWidget->GetShowMeshesMode() == CShowMeshesModeWidget::eShowMeshesMode_All)
	{
		// Draw RC output.
		if (m_pMeshStatObj)
		{
			Matrix34 identity(IDENTITY);
			rc.renderParams->pMatrix = &identity;
			rc.renderParams->lodValue = m_viewSettings.lod;
			
			if (m_viewSettings.shadingMode == SViewSettings::eShadingMode_VertexColors)
			{
				if (m_pVertexColorMaterial)
				{
					rc.renderParams->pMaterial = m_pVertexColorMaterial.get();
				}
				else
				{
					frameText += "Material needed for previewing vertex colors is missing.\n";
				}
			}
			else if (m_viewSettings.shadingMode == SViewSettings::eShadingMode_VertexAlphas)
			{
				if (m_pVertexAlphaMaterial)
				{
					rc.renderParams->pMaterial = m_pVertexAlphaMaterial.get();
				}
				else
				{
					frameText += "Material needed for previewing vertex colors is missing.\n";
				}
			}

			m_pMeshStatObj->Render(*rc.renderParams, *rc.passInfo);
			if (m_viewSettings.bShowEdges)
			{
				auto debugDrawInfo = CreateDebugDrawInfo();
				debugDrawInfo.tm = identity;
				m_pMeshStatObj->DebugDraw(debugDrawInfo);
			}

			if (m_viewSettings.bShowSize)
			{
				DrawBoxes(m_pMeshStatObj->GetAABB());
			}
			sceneBox = m_pMeshStatObj->GetAABB();
		}

		if (m_viewSettings.lod > 0)
		{
			if (!m_bHasLods && !GetAutoLodNodes(GetScene()).empty())
			{
				IRenderAuxGeom* const pAux = gEnv->pRenderer->GetIRenderAuxGeom();

				string label;
				label.Format("Waiting for generation of LOD %d.\n", m_viewSettings.lod);
				frameText += label;
			}

			if (m_bAutoLodCheckViewSettingsFlag)
			{
				UpdateStaticMeshes();
				m_bAutoLodCheckViewSettingsFlag = false;
			}
		}
	}

	const FbxTool::CScene* pScene = GetScene();

	for (size_t i = 0; i < m_pSceneModel->GetElementCount(); ++i)
	{
		if (m_pSceneModel->GetElementById(i)->GetType() != ESceneElementType::SourceNode)
		{
			continue;
		}

		const CSceneElementSourceNode* const pSceneElement = (CSceneElementSourceNode*)m_pSceneModel->GetElementById(i);
		if (!m_nodeVisibilityMap[pSceneElement->GetNode()->id])
		{
			continue;
		}

		IStatObj* pRenderObj = nullptr;
		Matrix34 renderTM(IDENTITY);

		for (size_t j = 0; j < m_rcMeshUnmergedContent.size(); ++j)
		{
			const auto& child = m_rcMeshUnmergedContent[j];
			if (child.pNode == pSceneElement->GetNode())
			{
				pRenderObj = child.pStatObj;
				renderTM = child.localToWorld;
			}
		}

		if (pRenderObj)
		{
			const int flags = pRenderObj->GetFlags();
			pRenderObj->SetFlags(flags & ~STATIC_OBJECT_COMPOUND);

			if (m_pShowMeshesModeWidget->GetShowMeshesMode() != CShowMeshesModeWidget::eShowMeshesMode_All)
			{
				rc.renderParams->pMatrix = &renderTM;
				rc.renderParams->lodValue = m_viewSettings.lod;
				pRenderObj->Render(*rc.renderParams, *rc.passInfo);
			}

			float selectionHeat = m_nodeRenderInfoMap[pSceneElement->GetNode()->id].selectionHeat;
			if (selectionHeat > 0.0f)
			{
				auto debugDrawInfo = CreateDebugDrawInfo();
				debugDrawInfo.lineColor.a = 255 * selectionHeat;
				debugDrawInfo.tm = renderTM;
				pRenderObj->DebugDraw(debugDrawInfo);
			}

			pRenderObj->SetFlags(flags);
		}
	}

	if (m_viewSettings.bShowSize)
	{
		DrawBoxes(sceneBox);
	}

	// Draw name of picked mesh.
	const QViewport* const vp = m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport();
	ShowLabelNextToCursor(vp, m_targetHitInfo.szLabel);

	if (!frameText.empty())
	{
		IRenderAuxGeom* const pAux = gEnv->pRenderer->GetIRenderAuxGeom();
		static float black[4] = { 0.0f };
		pAux->Draw2dLabel(10, 40, 1.5, black, false, frameText.c_str());
	}

	// Focus camera to current bounding box.
	if (m_bCameraNeedsReset && !sceneBox.IsReset())
	{
		// Move the camera along a given ray for objects relative close to the world's origin to have a nice view.
		// Put the camera on the ray passing through the world origin in the center of the object box for the remote object.
		// Interpolate the direction of the ray for the object between these cases.
		// Thus the world origin will be visible in the viewport that gives the user a visual hint of the object location.

		Vec3 dir = Vec3(1.0f, -1.0f, 0.5f).normalized();
		const Vec3 lookAtPoint = sceneBox.GetCenter();
		const float lengthSquared = lookAtPoint.GetLengthSquared();
		if (lengthSquared > 0 && sceneBox.GetRadius() > 0)
		{
			const float length = sqrt(lengthSquared);
			const float r = length / sceneBox.GetRadius(); // a relative distance to the word origin.
			const float r0 = 3.0f;
			const float r1 = 10.0f;

			if (r > r1)
			{
				dir = lookAtPoint / length;
			}
			else if (r > r0)
			{
				const Vec3 dir1 = lookAtPoint / length;
				const float k = (r - r0) / (r1 - r0);
				dir = Vec3::CreateSlerp(dir, dir1, k);
			}
		}

		// Fit the object into viewport window.
		const float fow = rc.camera->GetProjRatio() >= 1.0f ? rc.camera->GetFov() : rc.camera->GetFov() * rc.camera->GetProjRatio();
		const float radiusFactor = tanf(fow * 0.5f);

		SViewportState state = rc.viewport->GetState();
		state.cameraTarget.t = lookAtPoint + dir * sceneBox.GetRadius() / radiusFactor;
		rc.viewport->SetState(state);
		rc.viewport->LookAt(sceneBox.GetCenter(), sceneBox.GetRadius(), true);
		m_bCameraNeedsReset = false;
	}
}

void CMainDialog::RenderSkin(const SRenderContext& rc)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	IRenderer* const pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* const pAux = pRenderer->GetIRenderAuxGeom();

	// Update skin material, if necessary
	if (m_pCharacterInstance && m_pMaterialPanel->GetMaterialSettings()->GetMaterial() && m_pCharacterInstance->GetIMaterial() != m_pMaterialPanel->GetMaterialSettings()->GetMaterial()->GetMatInfo())
	{
		m_pCharacterInstance->SetIMaterial_Instance(m_pMaterialPanel->GetMaterialSettings()->GetMaterial()->GetMatInfo());
	}

	GetIEditor()->GetSystem()->GetIAnimationSystem()->UpdateStreaming(-1, -1);

	if (!m_pCharacterInstance)
	{
		return;
	}

	m_pCharacterInstance->StartAnimationProcessing(SAnimationProcessParams());
	m_pCharacterInstance->FinishAnimationComputations();

	SRendParams rp = *rc.renderParams;
	assert(rp.pMatrix);
	assert(rp.pPrevMatrix);

	Matrix34 identity(IDENTITY);
	rp.pMatrix = &identity;

	const SRenderingPassInfo& passInfo = *rc.passInfo;
	auto pInstanceBase = m_pCharacterInstance;
	gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstanceBase, pInstanceBase->GetIMaterial(), identity, 0, 1.f, 4, true, passInfo);
	pInstanceBase->SetViewdir(rc.camera->GetViewdir());
	pInstanceBase->Render(rp, passInfo);

	DrawSkeleton(pAuxGeom, &m_pCharacterInstance->GetIDefaultSkeleton(), m_pCharacterInstance->GetISkeletonPose(),
	             QuatT(IDENTITY), "", rc.viewport->GetState().cameraTarget);

	// draw joint names
	ISkeletonPose& skeletonPose = *m_pCharacterInstance->GetISkeletonPose();
	IDefaultSkeleton& defaultSkeleton = m_pCharacterInstance->GetIDefaultSkeleton();
	uint32 numJoints = defaultSkeleton.GetJointCount();

	if (m_viewSettings.bShowJointNames)
	{
		for (int j = 0; j < numJoints; j++)
	{
			const char* pJointName = defaultSkeleton.GetJointNameByID(j);

			QuatT jointTM = QuatT(skeletonPose.GetAbsJointByID(j));
			IRenderAuxText::DrawLabel(jointTM.t, 2, string().Format("%s", pJointName));
		}
	}
}

void CMainDialog::RenderTargetView(const SRenderContext& rc)
{
	if (m_pSceneUserData->GetSelectedSkin())
	{
		RenderSkin(rc);
	}
	else
		{
		RenderStaticMesh(rc);
	}
}

void CMainDialog::RenderSourceView(const SRenderContext& rc)
{
	using namespace Private_MainDialog;

	IRenderer* const pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* const pAux = pRenderer->GetIRenderAuxGeom();

	if (!m_displayScene)
	{
		// Nothing to draw.
		return;
	}

	const FbxTool::CScene* const pScene = GetScene();

	// Update material colors.
	const int nMaterials = pScene->GetMaterialCount();
	for (int i = 0; i < nMaterials; ++i)
	{
		const char* const szName = pScene->GetMaterials()[i]->szName;
		CMaterialElement* const pMaterialElement = m_pMaterialPanel->GetMaterialModel()->FindElement(QLatin1String(szName));
		if (!pMaterialElement)
		{
			continue;
		}

		const int uberMaterialIdx = i / MAX_SUB_MATERIALS;
		const int subMaterialIdx = i % MAX_SUB_MATERIALS;

		IMaterial* const uberMaterial = m_uberMaterials[uberMaterialIdx].get();
		IMaterial* subMaterial = uberMaterial->GetSubMtl(subMaterialIdx);

		if (pMaterialElement->GetPhysicalizeSetting() == FbxMetaData::EPhysicalizeSetting::ProxyOnly)
		{
			if (subMaterial != m_proxyMaterial)
			{
				uberMaterial->SetSubMtl(subMaterialIdx, m_proxyMaterial);
			}
		}
		else
		{
			if (subMaterial != m_materials[i].get())
			{
				uberMaterial->SetSubMtl(subMaterialIdx, m_materials[i].get());
			}
			subMaterial = m_materials[i].get();
			if (GetMaterialDiffuseColor(subMaterial) != pMaterialElement->GetColor())
			{
				SetMaterialDiffuseColor(subMaterial, pMaterialElement->GetColor());
			}
		}
	}

	const std::vector<SStaticObject>& statObjs = m_displayScene->statObjs;

	// Invalidate all static objects and update materials, if necessary.
	for (size_t i = 0; i < statObjs.size(); ++i)
	{
		IStatObj* const pStatObj = statObjs[i].pStatObj;
		if (!pStatObj->GetRenderMesh())
		{
			pStatObj->Invalidate(false);
		}
		if (pScene->IsProxy(statObjs[i].pMesh->pNode))
		{
			if (pStatObj->GetMaterial() != m_proxyMaterial)
			{
				pStatObj->SetMaterial(m_proxyMaterial);
			}
		}
		else if (pStatObj->GetMaterial() != m_uberMaterials[statObjs[i].uberMaterial])
		{
			pStatObj->SetMaterial(m_uberMaterials[statObjs[i].uberMaterial]);
		}
	}

	if (m_bGlobalAabbNeedsRefresh && !IsLoadingSuspended())
	{
		for (SStaticObject& statObject : m_displayScene->statObjs)
		{
			statObject.boundingBox = statObject.pMesh->ComputeAabb(statObject.pMesh->GetGlobalTransform());
		}
		m_bGlobalAabbNeedsRefresh = false;
	}
	// Make sure we draw a mesh at most once.
	std::vector<int> visited(statObjs.size(), 0);

	AABB sceneBox(AABB::RESET);

	for (size_t i = 0; i < m_pSceneModel->GetElementCount(); ++i)
	{
		if (m_pSceneModel->GetElementById(i)->GetType() != ESceneElementType::SourceNode)
		{
			continue;
		}

		const CSceneElementSourceNode* const pSceneElement = (CSceneElementSourceNode*)m_pSceneModel->GetElementById(i);
		if (!m_nodeVisibilityMap[pSceneElement->GetNode()->id])
		{
			continue;
		}

		const FbxTool::SMesh* const pMesh = pSceneElement->GetNode()->ppMeshes[0];

		if (pMesh->numVertices == 0 || pMesh->numTriangles == 0)
		{
			// There is no static object for an empty mesh.
			continue;
		}

		// Find static object for mesh.
		bool bFoundStatObj = false;
		for (int j = 0; j < statObjs.size(); ++j)
		{
			if (statObjs[j].pMesh == pMesh)
			{
				if (!visited[j])
				{
					IStatObj* const pStatObj = statObjs[j].pStatObj;
					Matrix34 localToWorld = statObjs[j].pMesh->GetGlobalTransform();
					rc.renderParams->pMatrix = &localToWorld;
					pStatObj->Render(*rc.renderParams, *rc.passInfo);
					if (statObjs[j].selectionHeat > 0.0f)
					{
						auto debugDrawInfo = CreateDebugDrawInfo();
						debugDrawInfo.tm = localToWorld;
						debugDrawInfo.lineColor.a = 255 * statObjs[j].selectionHeat;
						pStatObj->DebugDraw(debugDrawInfo);
					}

					// Draw line and gizmo to show offset from pivot.
					if (m_viewSettings.bShowPivots)
					{
						SAuxGeomRenderFlags oldFlags = pAux->GetRenderFlags();
						pAux->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);

						const Vec3 origin = localToWorld.TransformPoint(Vec3(ZERO));
						const ColorB pivotColor(0, 0, 0);
						pAux->DrawLine(
						  statObjs[j].boundingBox.GetCenter(),
						  pivotColor,
						  origin,
						  pivotColor,
						  4.0f);
						DrawPivot(*pAux, pivotColor, origin);

						pAux->SetRenderFlags(oldFlags);
					}

					sceneBox.Add(statObjs[j].boundingBox);

					visited[j] = 1;
				}

				bFoundStatObj = true;
				break;
			}
		}
		assert(bFoundStatObj);
	}

	if (m_viewSettings.bShowSize)
	{
		DrawBoxes(sceneBox);
	}

	// Draw name of picked mesh.
	const QWidget* const vp =
	  m_viewSettings.viewportMode == SViewSettings::eViewportMode_SourceView ?
	  m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport() :
	  m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport();

	const float xOffset = 10.0f;
	const float yOffset = 10.0f;

	QPoint mousePos = vp->mapFromGlobal(QCursor::pos());
	if (mousePos.x() >= 0 && mousePos.x() < vp->width() &&
	    mousePos.y() >= 0 && mousePos.y() < vp->height())
	{
		pAux->Draw2dLabel(mousePos.x() + xOffset, mousePos.y() + yOffset, 1.5, ColorF(1, 1, 1), false,
		                  "%s", m_sourceHitInfo.szLabel);
	}
}

void CMainDialog::OnGlobalImportsTreeChanged()
{
	m_bRefreshRcMesh = true;
	m_bGlobalAabbNeedsRefresh = true;
	m_pTargetMeshView->model()->Clear();
	}

void CMainDialog::CreateMeshFromFile(const string& filePath)
{
	// Load stat obj from file.
	m_pMeshStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filePath.c_str(), NULL, NULL, false);
	m_pMeshStatObj->AddRef();

	CreateUnmergedContent();

	// Load target mesh view from file.
	m_pTargetMeshView->model()->LoadCgf(filePath);

	SProxyTree proxyTree(m_pProxyData.get(), GetScene());
	WriteAutoGenProxies(filePath, &proxyTree);
	}

void CMainDialog::CreateUnmergedMeshFromFile(const string& filePath)
{
	// Load stat obj from file.
	m_pUnmergedMeshStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filePath.c_str(), NULL, NULL, false);
	m_pUnmergedMeshStatObj->AddRef();

	CreateUnmergedContent();
}

void CMainDialog::CreateSkinFromFile(const string& filePath)
	{
	string materialFilename;
	if (m_pMaterialPanel->GetMaterialSettings()->GetMaterial())
	{
		materialFilename = m_pMaterialPanel->GetMaterialSettings()->GetMaterialName();
	}

	ICharacterManager* const pCharacterManager = GetIEditor()->GetSystem()->GetIAnimationSystem();
	m_pCharacterInstance = CreateTemporaryCharacter(
		QtUtil::ToQString(filePath),
		QtUtil::ToQString(filePath),
		QtUtil::ToQString(materialFilename));
}

void CMainDialog::UpdateResources()
	{
	CRY_ASSERT(m_pSceneUserData);
	if (m_pSceneUserData->GetSelectedSkin())
		{
		UpdateSkin();
		}
	else
	{
		UpdateStaticMeshes();
		}
}

CMainDialog::CMainDialog(QWidget* pParent /*= nullptr*/)
	: CBaseDialog(pParent)
	, m_bCameraNeedsReset(false)
	, m_uberMaterials()
	, m_bGlobalAabbNeedsRefresh(false)
	, m_pAutoMaterial()
	, m_bRefreshRcMesh(false)
	, m_bHasLods(false)
	, m_bAutoLodCheckViewSettingsFlag(false)
	, m_pSceneUserData(new DialogMesh::CSceneUserData())
	, m_pMeshStatObj(nullptr)
	, m_pUnmergedMeshStatObj(nullptr)
	, m_ppSelectionMesh(nullptr)
	, m_pCharacterInstance(nullptr)
{
	m_pMeshRcObject = CreateTempRcObject();

	m_pUnmergedMeshRcObject = CreateTempRcObject();
	m_pUnmergedMeshRcObject->SetFinalize(CTempRcObject::Finalize([this](const CTempRcObject* pTempRcObject)
	{
		const string filePath = PathUtil::ReplaceExtension(pTempRcObject->GetFilePath(), "cgf");
		CreateUnmergedMeshFromFile(filePath);
	}));

	m_pSkinRcObject = CreateTempRcObject();
	m_pSkinRcObject->SetFinalize(CTempRcObject::Finalize([this](const CTempRcObject* pTempRcObject)
	{
		const string filePath = PathUtil::ReplaceExtension(pTempRcObject->GetFilePath(), "skin");
		CreateSkinFromFile(filePath);
	}));

	m_pSceneUserData->sigSelectedSkinChanged.Connect(std::function<void()>([this]()
{
		UpdateResources();

		m_pGlobalImportSettings->SetStaticMeshSettingsEnabled(!m_pSceneUserData->GetSelectedSkin());
		m_pGlobalImportSettingsTree->revert();
	}));

	m_pSceneModel.reset(new CSceneModel());

	m_pProxyData.reset(new CProxyData());
	m_pProxyGenerator.reset(new CProxyGenerator(m_pProxyData.get()));

	setupUi(this);
	Init();
	InitMaterials();

	m_pAutoLodSettings.reset(new CAutoLodSettings());

	// Load personalization.
	m_initialFilePath = CFbxToolPlugin::GetInstance()->GetPersonalizationProperty(s_initialFilePropertyName).toString();
	if (m_initialFilePath.isEmpty())
	{
		m_initialFilePath = GetAbsoluteGameFolderPath();
	}

	auto pDisplayOptionsWidget = m_pViewportContainer->GetDisplayOptionsWidget();

	pDisplayOptionsWidget->SetUserOptions(Serialization::SStruct(m_viewSettings), "viewSettings", "View");
	connect(pDisplayOptionsWidget, &CDisplayOptionsWidget::SigChanged, [this]()
	{
		m_pViewportContainer->GetSplitViewport()->SetSplit(m_viewSettings.viewportMode == SViewSettings::eViewportMode_Split);
	});
}

CMainDialog::~CMainDialog()
{

	for (auto& connection : m_connections)
	{
		disconnect(connection);
	}
	m_connections.clear();

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->RemoveConsumer(this);
	m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport()->RemoveConsumer(this);
}

static QString PromptForImportFbxFile(QWidget* pParent, const QString& initialFile)
{
	FbxTool::TIndex numExtensions;
	const char* const* const ppExtensions = FbxTool::GetSupportedFileExtensions(numExtensions);

	ExtensionFilterVector filters;
	QStringList extensions;

	for (FbxTool::TIndex i = 0; i < numExtensions; ++i)
	{
		const QString extension = QtUtil::ToQString(ppExtensions[i]);
		extensions << extension;
		filters << CExtensionFilter(QObject::tr("%1 files").arg(extension.toUpper()), extension);
	}
	if (!filters.isEmpty())
	{
		filters.prepend(CExtensionFilter(QObject::tr("All supported file types (%1)").arg(extensions.join(L',')), extensions));
	}

	filters << CExtensionFilter(QObject::tr("All files (*.*)"), "*");

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = QFileInfo(initialFile).absolutePath();
	runParams.initialFile = initialFile;
	runParams.title = QObject::tr("Select file to import");
	runParams.extensionFilters = filters;

	return CSystemFileDialog::RunImportFile(runParams, pParent);
}

namespace
{

void MakeSelectionMutuallyExclusive(std::vector<QMetaObject::Connection>& connections, QAbstractItemView* pView0, QAbstractItemView* pView1)
{
	connections.emplace_back(QObject::connect(pView0->selectionModel(), &QItemSelectionModel::selectionChanged, [pView1](const QItemSelection& selected, const QItemSelection&)
		{
			if (!selected.indexes().empty())
			{
			  pView1->clearSelection();
			}
	  }));
	connections.emplace_back(QObject::connect(pView0, &QAbstractItemView::clicked, [pView1](const QModelIndex&)
		{
			pView1->clearSelection();
	  }));
	connections.emplace_back(QObject::connect(pView1->selectionModel(), &QItemSelectionModel::selectionChanged, [pView0](const QItemSelection& selected, const QItemSelection&)
		{
			if (!selected.indexes().empty())
			{
			  pView0->clearSelection();
			}
	  }));
	connections.emplace_back(QObject::connect(pView1, &QAbstractItemView::clicked, [pView0](const QModelIndex&)
		{
			pView0->clearSelection();
	  }));
}

QString GetFilename(const QString& filePath)
{
	QFileInfo fileInfo(filePath);
	return fileInfo.exists() ? fileInfo.fileName() : QString();
}

} // namespace

void CMainDialog::setupUi(CMainDialog* MainDialog)
{
	m_pMaterialPanel = new CMaterialPanel(&GetSceneManager(), GetTaskHost());
	m_pMaterialPanel->setEnabled(false);
	m_pMaterialPanel->GetMaterialSettings()->SetMaterialChangedCallback([this]()
	{
		m_bRefreshRcMesh = true;
	});

	m_pMaterialPanel->GetMaterialView()->SetMaterialContextMenuCallback(std::bind(&CMainDialog::CreateMaterialContextMenu, this, std::placeholders::_1, std::placeholders::_2));

	// Create scene tree widget.

	m_pSceneTree = new CSceneViewCgf();
	m_pSceneTree->setToolTip(tr("Displays loaded scenes"));
	m_pSceneTree->setStatusTip(tr("Displays loaded scenes"));

	m_pSceneViewContainer = new CSceneViewContainer(m_pSceneModel.get(), m_pSceneTree, this);

	m_pSceneTree->setColumnWidth(0, 128);

	// Create target mesh widget.

	m_pTargetMeshView = new CTargetMeshView(this);

	// Create property tree widget.

	m_pGlobalImportSettingsTree = new QPropertyTree();
	m_pGlobalImportSettingsTree->setSizeToContent(false);
	m_pGlobalImportSettingsTree->setEnabled(false);
	connect(m_pGlobalImportSettingsTree, &QPropertyTree::signalChanged, this, &CMainDialog::OnGlobalImportsTreeChanged);

	m_pGlobalImportSettings.reset(new CGlobalImportSettings());
	m_pGlobalImportSettingsTree->attach(Serialization::SStruct(*m_pGlobalImportSettings));
	m_pGlobalImportSettingsTree->expandAll();

	m_pPropertyTree = new QPropertyTree();
	m_pPropertyTree->setSliderUpdateDelay(5);
	m_pPropertyTree->setToolTip(tr("Properties of selected scene object or material"));
	m_pPropertyTree->setStatusTip(tr("Properties of selected scene object or material"));
	m_pPropertyTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

	// Create viewport container.
	m_pViewportContainer = new CSplitViewportContainer();
	m_pViewportContainer->GetSplitViewport()->installEventFilter(this);

	m_pShowMeshesModeWidget = new Private_MainDialog::CShowMeshesModeWidget();
	m_pViewportContainer->SetHeaderWidget(m_pShowMeshesModeWidget);

	// Create tab widget.

	QTabWidget* const pTabWidget = new QTabWidget();
	pTabWidget->setObjectName("notificationTabs");

	// Create geometry widget.
	// The geometry widget consists of settings and scene view.
	QSplitter* const pGeometryWidget = new QSplitter(Qt::Vertical);

	pGeometryWidget->addWidget(m_pGlobalImportSettingsTree);
	{
		QTabWidget* const pSceneTabWidget = new QTabWidget();
		pSceneTabWidget->setObjectName("notificationTabs");
		if (m_pSceneTree)
		{
			pSceneTabWidget->addTab(m_pSceneViewContainer, "Source");
		}
		if (m_pTargetMeshView)
		{
			pSceneTabWidget->addTab(m_pTargetMeshView, "Target");
		}
		pGeometryWidget->addWidget(pSceneTabWidget);
	}

	// Give scene view maximal vertical screen estate.
	pGeometryWidget->setStretchFactor(0, 0);
	pGeometryWidget->setStretchFactor(1, 1);

	// Create properties tab.
	m_pPhysProxiesControls = new CPhysProxiesControlsWidget();
	QSplitter* const pPropsSplitter = new QSplitter(Qt::Vertical);
	pPropsSplitter->addWidget(m_pPhysProxiesControls);
	pPropsSplitter->addWidget(m_pPropertyTree);
	QList<int> sizesProps;
	sizesProps.push_back(110);
	sizesProps.push_back(800);
	pPropsSplitter->setSizes(sizesProps);
	pTabWidget->addTab(pPropsSplitter, tr("Properties"));

	// Create material tab.
	pTabWidget->addTab(m_pMaterialPanel, "Material");

	// Main Dialog layout
	{
		QVBoxLayout* const pLayout = new QVBoxLayout;
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->setMargin(0);

		QSplitter* const pSplitter = new QSplitter(Qt::Horizontal);
		{
			pSplitter->addWidget(pGeometryWidget);
			pSplitter->addWidget(m_pViewportContainer);
			pSplitter->addWidget(pTabWidget);

			pSplitter->setStretchFactor(0, 0);
			pSplitter->setStretchFactor(1, 1);
			pSplitter->setStretchFactor(2, 0);
		}

		pLayout->addWidget(pSplitter, 1);

		pLayout->update();

		if (layout())
		{
			delete layout();
		}
		setLayout(pLayout);
	}

	QMetaObject::connectSlotsByName(MainDialog);
}

// Reads meta data from either .cgf or .json file (in that order).
bool ReadMetaDataFromFile(const QString& filePath, FbxMetaData::SMetaData& metaData)
{
	if (!metaData.pEditorMetaData)
	{
		metaData.pEditorMetaData.reset(new SEditorMetaData());
	}

	CChunkFile cf;
	if (cf.Read(QtUtil::ToString(filePath).c_str()))
	{
		// Reading JSON data from ChunkType_ImportSettings chunk of a chunk file.
		const IChunkFile::ChunkDesc* const pChunk = cf.FindChunkByType(ChunkType_ImportSettings);
		if (pChunk)
		{
			const string json((const char*)pChunk->data, (size_t)pChunk->size);
			return metaData.FromJson(json);
		}
		else
		{
			LogPrintf("ChunkType_ImportSettings chunk (JSON) is missing in '%s'", QtUtil::ToString(filePath).c_str());
		}
	}
	else
	{
		// Reading JSON data from .json file.
		QFile file(filePath);
		if (file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			QTextStream inStream(&file);
			const string json = QtUtil::ToString(inStream.readAll());

			file.close();
			return metaData.FromJson(json);
		}
		else
		{
			LogPrintf("unable to open file '%s' for reading.\n", QtUtil::ToString(filePath).c_str());
		}
	}

	CQuestionDialog::SWarning("Cannot open .cgf file", "Only .cgf files that have been created from .fbx can be opened.");
	return false;

}

bool CMainDialog::MayUnloadScene()
{
	if (!GetScene())
	{
		return true;
	}

	string json;

	FbxMetaData::SMetaData metaData;
	if (CreateMetaData(metaData, ""))
	{
		json = metaData.ToJson();
	}

	if (json == m_displayScene->lastJson)
	{
		return true;
	}

	return false;
}

int CMainDialog::GetToolButtons() const
{
	return eToolButtonFlags_Import | eToolButtonFlags_Open | eToolButtonFlags_Save | eToolButtonFlags_Reimport;
}

int CMainDialog::GetOpenFileFormatFlags()
{
	return eFileFormat_CGF | eFileFormat_SKIN;
}

QString CMainDialog::ShowSaveAsDialog()
{
	if (m_pSceneUserData->GetSelectedSkin())
	{
		return ShowSaveAsFilePrompt(eFileFormat_SKIN);
	}
	else
	{
	return ShowSaveAsFilePrompt(eFileFormat_CGF);
}
}

// Reimporting a file is reloading the current file from its original file path, and applying the current settings.
void CMainDialog::ReimportFile()
{
	if (!GetScene())
	{
		return;
	}

	const QString originalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();
	const QString filename = GetSceneManager().GetImportFile()->GetFilename();

	std::unique_ptr<MeshImporter::SImportScenePayload> pPayload(new MeshImporter::SImportScenePayload());
	pPayload->pMetaData.reset(new FbxMetaData::SMetaData());
	CreateMetaData(*(pPayload->pMetaData), filename);

	ImportFile(originalFilePath, pPayload.release());
}

void CMainDialog::OnOpenMetaData()
{
	QString filePath = ShowOpenFilePrompt(eFileFormat_CGF | eFileFormat_JSON);
	if (!filePath.isEmpty())
	{
		filePath = GetAbsoluteGameFolderPath() + QDir::separator() + filePath;

		std::unique_ptr<FbxMetaData::SMetaData> pMetaData(new FbxMetaData::SMetaData());
		if (!ReadMetaDataFromFile(filePath, *pMetaData))
		{
			LogPrintf("Unable to read import settings from file.");
		}
		else
		{
			const QString sourceFilePath = QFileInfo(filePath).absoluteDir().filePath(QtUtil::ToQString(pMetaData->sourceFilename));

			std::unique_ptr<MeshImporter::SImportScenePayload> pPayload(new MeshImporter::SImportScenePayload());
			pPayload->pMetaData = std::move(pMetaData);

			if (filePath.endsWith(QStringLiteral("cgf"), Qt::CaseInsensitive))
			{
				pPayload->targetFilename = filePath;
			}

			ImportFile(sourceFilePath, pPayload.release());
		}
	}
}

string GetMatName(const QString& filePath);

bool   CMainDialog::SaveAs(SSaveContext& ctx)
{
	bool bSuccess = true;

	const QString filePath = QtUtil::ToQString(ctx.targetFilePath);

	// TODO: Is it right time to setup the viewport title?
	m_pGlobalImportSettings->SetOutputFilePath(QtUtil::ToString(filePath));
	m_pGlobalImportSettingsTree->revert();

	// Write complete file path.
	const QString filename = GetSceneManager().GetImportFile()->GetFilePath();

	std::shared_ptr<QTemporaryDir> pTempDir(std::move(ctx.pTempDir));

	if (m_pSceneUserData->GetSelectedSkin())
	{
		if (!SaveSkin(pTempDir, PathUtil::ReplaceExtension(QtUtil::ToString(filePath), "skin"), filename))
		{
			CQuestionDialog::SCritical(tr("Save to SKIN failed"), tr("Failed to write current data to %1").arg(filePath));
			bSuccess = false;
		}
	}
	else if (!SaveCgf(pTempDir, PathUtil::ReplaceExtension(QtUtil::ToString(filePath), "cgf"), filename))
	{
		CQuestionDialog::SCritical(tr("Save to CGF failed"), tr("Failed to write current data to %1").arg(filePath));
		bSuccess = false;
	}

	m_displayScene->cgfFilePath = filePath;

	TouchLastJson();

	return bSuccess;
}

namespace Private_MainDialog
{

std::unique_ptr<CModelProperties::SSerializer> CreateSerializer(QAbstractItemModel* pModel, const QModelIndex& modelIndex)
{
	QVariant data = modelIndex.data(eItemDataRole_YasliSStruct);
	if (data.isValid() && data.canConvert<void*>())
	{
		std::unique_ptr<Serialization::SStruct> sstruct((Serialization::SStruct*)data.value<void*>());

		std::unique_ptr<CModelProperties::SSerializer> pSerializer(new CModelProperties::SSerializer());
		pSerializer->m_sstruct = *sstruct;
		return pSerializer;
	}
	return std::unique_ptr<CModelProperties::SSerializer>();
}

} // namespace Private_MainDialog

void CMainDialog::Init()
{
	m_connections.emplace_back(connect(m_pGlobalImportSettingsTree, &QPropertyTree::signalChanged, [this]()
	{
		m_pMaterialPanel->ApplyMaterial();
	}));

	m_pModelProperties.reset(new CModelProperties(m_pPropertyTree));
	m_pModelProperties->AddCreateSerializerFunc(&Private_MainDialog::CreateSerializer);
	m_pModelProperties->ConnectViewToPropertyObject(m_pSceneTree);
	m_pModelProperties->ConnectViewToPropertyObject(m_pMaterialPanel->GetMaterialView());
	m_pModelProperties->ConnectViewToPropertyObject(m_pTargetMeshView);

	// Context menu.
	m_pSceneContextMenu.reset(new CSceneContextMenu(this));
	m_pSceneContextMenu->Attach();

	// Since both the scene tree and the material tree use the properties panel, we make their selections
	// mutually exclusive.

	MakeSelectionMutuallyExclusive(m_connections, m_pSceneTree, m_pMaterialPanel->GetMaterialView());
	MakeSelectionMutuallyExclusive(m_connections, m_pTargetMeshView, m_pSceneTree);
	MakeSelectionMutuallyExclusive(m_connections, m_pTargetMeshView, m_pMaterialPanel->GetMaterialView());

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->AddConsumer(this);
	m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport()->AddConsumer(this);
	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->Update();
	m_pViewportContainer->GetSplitViewport()->GetSecondaryViewport()->Update();

	auto propertiesChanged = [this](CSceneElementCommon*) { UpdateStaticMeshes(); };
	m_pSceneModel->GetModelScene()->signalPropertiesChanged.Connect(std::function<void(CSceneElementCommon*)>(propertiesChanged));

	// Proxy generation.

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigCloseHoles, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->CloseHoles(pPhysProxiesElement->GetPhysProxies());
		}
	});

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigReopenHoles, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->ReopenHoles(pPhysProxiesElement->GetPhysProxies());
		}
	});

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigSelectAll, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->SelectAll(pPhysProxiesElement->GetPhysProxies());
		}
	});

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigSelectNone, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->SelectNone(pPhysProxiesElement->GetPhysProxies());
		}
	});

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigGenerateProxies, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			AddProxyGeometries(pPhysProxiesElement, m_pProxyGenerator.get());
		}
	});

	connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::SigResetProxies, [this]()
	{
		CSceneElementPhysProxies* const pPhysProxiesElement = GetSelectedPhysProxiesElement(GetSelectedSceneElement());
		if (pPhysProxiesElement)
		{
			m_pProxyGenerator->ResetProxies(pPhysProxiesElement->GetPhysProxies());

			const QModelIndex index = m_pSceneViewContainer->GetFilter()->mapFromSource(m_pSceneModel->GetModelIndexFromSceneElement(pPhysProxiesElement));
			m_pSceneTree->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows | QItemSelectionModel::Clear);
		}
	});

	// If a proxy element has been added to the scene, we make it visible.
	auto expandSceneElement = [this](CSceneElementCommon*, CSceneElementCommon* pChildElement)
	{
		if (!pChildElement->GetParent())
		{
			return;
		}

		const QModelIndex childIndex = m_pSceneViewContainer->GetFilter()->mapFromSource(m_pSceneModel->GetModelIndexFromSceneElement(pChildElement));

		if (pChildElement->GetType() == ESceneElementType::PhysProxy)
		{
			m_pSceneTree->selectionModel()->clear();
			m_pSceneTree->selectionModel()->select(childIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
			m_pSceneTree->ExpandParents(childIndex);
		}
		else if (pChildElement->GetType() == ESceneElementType::ProxyGeom)
		{
			m_pSceneTree->ExpandParents(childIndex);
		}
	};
	m_pSceneModel->GetModelScene()->signalHierarchyChanged.Connect(std::function<void(CSceneElementCommon*, CSceneElementCommon*)>(expandSceneElement));

	// If a physics proxy container is removed, remove its scene element.
	auto removePhysProxies = [this](SPhysProxies* pPhysProxies)
	{
		if (!pPhysProxies)
		{
			return;
		}

		CSceneElementPhysProxies* const pPhysProxiesElement = FindSceneElementOfPhysProxies(*m_pSceneModel->GetModelScene(), pPhysProxies);
		if (!pPhysProxiesElement)
		{
			return;
		}

		m_pSceneTree->selectionModel()->clear();

		// Prevent crash since QAdvancedTreeView stores a list of all expanded indices on reset.
		const QModelIndex index = m_pSceneViewContainer->GetFilter()->mapFromSource(m_pSceneModel->GetModelIndexFromSceneElement(pPhysProxiesElement));
		m_pSceneTree->setExpanded(index, false);

		CSceneElementCommon::MakeRoot(pPhysProxiesElement);
		m_pSceneModel->GetModelScene()->DeleteSubtree(pPhysProxiesElement);
	};
	m_pProxyData->signalPhysProxiesAboutToBeRemoved.Connect(std::function<void(SPhysProxies*)>(removePhysProxies));

	// If a proxy geometry is removed, remove its scene element.
	auto removeProxyGeom = [this](phys_geometry* pPhysGeom)
	{
		CRY_ASSERT(pPhysGeom);

		CSceneElementProxyGeom* const pProxyGeomElement = FindSceneElementOfProxyGeom(*m_pSceneModel->GetModelScene(), pPhysGeom);
		CRY_ASSERT(pProxyGeomElement);

		m_pSceneTree->selectionModel()->clear();

		CSceneElementCommon::MakeRoot(pProxyGeomElement);
		m_pSceneModel->GetModelScene()->DeleteSubtree(pProxyGeomElement);
	};
	m_pProxyData->signalPhysGeometryAboutToBeRemoved.Connect(std::function<void(phys_geometry*)>(removeProxyGeom));

	// Update a proxy geometry if it's being reused.
	auto reuseProxyGeom = [this](phys_geometry* pOldGeom, phys_geometry* pNewGeom)
	{
		if (!pOldGeom)
	{
			return;
		}

		CSceneElementProxyGeom* const pProxyGeomElement = FindSceneElementOfProxyGeom(*m_pSceneModel->GetModelScene(), pOldGeom);
		if (!pProxyGeomElement)
		{
			return;
		}

		pProxyGeomElement->SetPhysGeom(pNewGeom);
	};
	m_pProxyData->signalPhysGeometryAboutToBeReused.Connect(std::function<void(phys_geometry*, phys_geometry*)>(reuseProxyGeom));

	auto createProxyGeom = [this](SPhysProxies* pPhysProxies, phys_geometry* pPhysGeom)
		  {
		CSceneElementPhysProxies* const pPhysProxiesElement = FindSceneElementOfPhysProxies(*m_pSceneModel->GetModelScene(), pPhysProxies);
		if (!pPhysProxiesElement)
		    {
		      return;
		    }

		CSceneElementProxyGeom* const pProxyGeomElement = pPhysProxiesElement->GetScene()->NewElement<CSceneElementProxyGeom>();
		pProxyGeomElement->SetPhysGeom(pPhysGeom);
		pPhysProxiesElement->AddChild(pProxyGeomElement);

		const QModelIndex index = m_pSceneViewContainer->GetFilter()->mapFromSource(m_pSceneModel->GetModelIndexFromSceneElement(pProxyGeomElement));
		m_pSceneTree->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows | QItemSelectionModel::Clear);
	};
	m_pProxyData->signalPhysGeometryCreated.Connect(std::function<void(SPhysProxies*, phys_geometry*)>(createProxyGeom));

	// Update phys proxies widget.
	m_pProxyGenerator->signalProxyIslandsChanged.Connect(m_pPhysProxiesControls, &CPhysProxiesControlsWidget::OnProxyIslandsChanged);

	auto setPhysProxiesWidgetVisibility = [this](const QItemSelection& selected, const QItemSelection&)
	{
		m_pPhysProxiesControls->setVisible(false);

		m_pProxyGenerator->Reset();

		if (selected.indexes().isEmpty())
		    {
			return;
		    }

		const QModelIndex index = m_pSceneViewContainer->GetFilter()->mapToSource(selected.indexes().first());
		if (!index.isValid())
		{
			return;
		  }

		CSceneElementCommon* const pSceneElement = m_pSceneModel->GetSceneElementFromModelIndex(index);
		if (pSceneElement->GetType() == ESceneElementType::PhysProxy)
		  {
			CSceneElementPhysProxies* const pPhysProxyElement = (CSceneElementPhysProxies*)pSceneElement;
			m_pPhysProxiesControls->OnProxyIslandsChanged(pPhysProxyElement->GetPhysProxies());
			m_pPhysProxiesControls->setVisible(true);
		  }
	};
	connect(m_pSceneTree->selectionModel(), &QItemSelectionModel::selectionChanged, setPhysProxiesWidgetVisibility);
	setPhysProxiesWidgetVisibility(QItemSelection(), QItemSelection());  // Set initial state.
		}

namespace Private_MainDialog
{

IMaterial* CreateVertexColorMaterial()
{
	IMaterialManager* const pMaterialManager = GetIEditor()->Get3DEngine()->GetMaterialManager();
	IMaterial* const pMat = pMaterialManager->LoadMaterial("%ENGINE%/EngineAssets/Materials/MeshImporter/MI_PreviewVertexColor");
	if (!pMat)
	{
		LogPrintf("Cannot find material needed for previewing vertex colors.\n");
	}
	return pMat;
}

IMaterial* CreateVertexAlphaMaterial()
{
	IMaterialManager* const pMaterialManager = GetIEditor()->Get3DEngine()->GetMaterialManager();
	IMaterial* const pMat = pMaterialManager->LoadMaterial("%ENGINE%/EngineAssets/Materials/MeshImporter/MI_PreviewVertexAlpha");
	if (!pMat)
	{
		LogPrintf("Cannot find material needed for previewing vertex alphas.\n");
	}
	return pMat;
}

} // namespace Private_MainDialog

void CMainDialog::InitMaterials()
{
	IMaterialManager* const pMaterialManager = GetIEditor()->Get3DEngine()->GetMaterialManager();
	m_proxyMaterial = pMaterialManager->LoadMaterial(kDefaultProxyMaterial);

	m_pVertexColorMaterial = Private_MainDialog::CreateVertexColorMaterial();
	m_pVertexAlphaMaterial = Private_MainDialog::CreateVertexAlphaMaterial();
}

static bool FinalizeMesh(CMesh* pMesh, const char*& szError)
{
	// Compute bounding boxes and texture mapping information.
	// Note: We can't do this earlier, since there's the possibility transform baking happens during merges.
	int numPositions = 0;
	const Vec3* pPositions = pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS, &numPositions);
	assert(numPositions > 0);
	AABB totalBoundingBox(AABB::RESET);
	float totalPosArea = 0.0f;
	float totalTexArea = 0.0f;
	for (size_t i = 0; i < pMesh->m_subsets.size(); ++i)
	{
		// Bounding box data
		auto& subset = pMesh->m_subsets[i];
		assert(numPositions >= subset.nFirstVertId + subset.nNumVerts);
		AABB subsetBoundingBox(AABB::RESET);
		for (int j = subset.nFirstVertId; j < subset.nFirstVertId + subset.nNumVerts; ++j)
		{
			subsetBoundingBox.Add(pPositions[j]);
		}
		subset.vCenter = subsetBoundingBox.GetCenter();
		subset.fRadius = subsetBoundingBox.GetRadius();
		totalBoundingBox.Add(subsetBoundingBox);

		// Texture mapping data
		float posArea, texArea;
		if (!pMesh->ComputeSubsetTexMappingAreas(i, posArea, texArea, szError))
		{
			subset.fTexelDensity = 0.0f;
		}
		else
		{
			szError = nullptr;
			subset.fTexelDensity = texArea / posArea;
			totalPosArea += posArea;
			totalTexArea += texArea;
		}
	}

	pMesh->m_bbox = totalBoundingBox;
	pMesh->m_texMappingDensity = totalPosArea > 0.0f ? totalTexArea / totalPosArea : 0.0f;

	// Compute face area
	if (!pMesh->RecomputeGeometricMeanFaceArea())
	{
		pMesh->m_geometricMeanFaceArea = 0.0f;
	}

	return pMesh->Validate(&szError);
}

void CMainDialog::AssignScene(const MeshImporter::SImportScenePayload* pPayload)
{
	m_bMaterialNameWasRelative = false;

	FbxTool::CScene* const pActiveScene = GetScene();

	m_displayScene.reset(new SDisplayScene());
	m_pSceneUserData->Init(pActiveScene);

	const QString filePath = GetSceneManager().GetImportFile()->GetFilePath();
	const QString originalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();

	m_pSceneModel->SetScene(pActiveScene, GetSceneManager().GetUserSettings());
	m_pSceneModel->SetSceneUserData(m_pSceneUserData.get());
	m_pSceneTree->ExpandRecursive(m_pSceneViewContainer->GetFilter()->index(0, 0, QModelIndex()), true);

	//m_pTargetMeshView->model()->SetScene(pActiveScene);
	m_pTargetMeshView->model()->Clear();

	m_pProxyData->SetScene(pActiveScene);

	pActiveScene->AddObserver(this);

	*m_pAutoLodSettings = CAutoLodSettings();
	m_pAutoLodSettings->getGlobalParams().m_fViewreSolution = 26.6932144;
	m_pAutoLodSettings->getGlobalParams().m_iViewsAround = 12;
	m_pAutoLodSettings->getGlobalParams().m_iViewElevations = 3;
	m_pAutoLodSettings->getGlobalParams().m_fSilhouetteWeight = 5.0;
	m_pAutoLodSettings->getGlobalParams().m_fVertexWelding = 0.001;
	m_pAutoLodSettings->getGlobalParams().m_bCheckTopology = true;
	m_pAutoLodSettings->getGlobalParams().m_bObjectHasBase = false;

	*m_pGlobalImportSettings = CGlobalImportSettings();
	m_pGlobalImportSettings->SetFileUnitSizeInCm(pActiveScene->GetFileUnitSizeInCm());
	m_pGlobalImportSettings->SetUpAxis(pActiveScene->GetUpAxis());
	m_pGlobalImportSettings->SetForwardAxis(pActiveScene->GetForwardAxis());
	m_pGlobalImportSettingsTree->attach(Serialization::SStruct(*m_pGlobalImportSettings));
	m_pGlobalImportSettingsTree->expandAll();
	m_pGlobalImportSettingsTree->setEnabled(true);

	m_pMaterialPanel->GetMaterialSettings()->SetMaterial("");
	m_pMaterialPanel->setEnabled(true);

	{
		IMaterialManager* const pMaterialManager = GetIEditor()->Get3DEngine()->GetMaterialManager();
		const char* const szDefaultMaterial = "%ENGINE%/EngineAssets/TextureMsg/DefaultSolids";
		TSmartPtr<IMaterial> const pReferenceMaterial = pMaterialManager->LoadMaterial(szDefaultMaterial);
		const int materialCount = pActiveScene->GetMaterialCount();

		// Create engine materials.

		m_materials.resize(materialCount);
		for (int i = 0; i < materialCount; ++i)
		{
			m_materials[i] = pMaterialManager->CloneMaterial(pReferenceMaterial);
		}

		// Create uber-materials.

		const int nUberMaterials = materialCount / MAX_SUB_MATERIALS + 1;

		m_uberMaterials.resize(nUberMaterials);

		for (int i = 0; i < nUberMaterials; ++i)
		{
			m_uberMaterials[i] = pMaterialManager->CloneMaterial(pReferenceMaterial);
			m_uberMaterials[i]->SetSubMtlCount(MAX_SUB_MATERIALS);
		}

		for (int i = 0; i < materialCount; ++i)
		{
			const int uberMaterial = i / MAX_SUB_MATERIALS;
			const int subMaterial = i % MAX_SUB_MATERIALS;
			m_uberMaterials[uberMaterial]->SetSubMtl(subMaterial, m_materials[i]);
		}
	}

	// Create Static Objects ---

	// Create all static objects.
	for (const auto& meshIt : pActiveScene->GetMeshes())
	{
		for (const auto& displayMesh : meshIt->displayMeshes)
		{
			IStatObj* const pStatObj = GetIEditor()->Get3DEngine()->CreateStatObj();

			IIndexedMesh* const pIndexedMesh = pStatObj->GetIndexedMesh();
			assert(pIndexedMesh);

			pIndexedMesh->SetMesh(*displayMesh->pEngineMesh);

			pStatObj->SetMaterial(m_uberMaterials[displayMesh->uberMaterial]);

			SStaticObject statObj;
			statObj.pMesh = meshIt.get();
			statObj.pStatObj = pStatObj;
			statObj.uberMaterial = displayMesh->uberMaterial;

			m_displayScene->statObjs.push_back(statObj);
		}
	}

	// Compile static objects.

	ITimer* const pTimer = gEnv->pSystem->GetITimer();
	float tMeshCompiling = 0.0f;

	// for (size_t i = 0; i < m_displayScene->statObjs.size(); ++i)
	concurrency::parallel_for(0, (int)m_displayScene->statObjs.size(), [this, &pTimer, &tMeshCompiling](int i)
	{
		IStatObj* const pStatObj = m_displayScene->statObjs[i].pStatObj;

		CMesh* const pEngineMesh = pStatObj->GetIndexedMesh()->GetMesh();

		const float startTime = pTimer->GetAsyncCurTime();
		mesh_compiler::CMeshCompiler meshCompiler;
		meshCompiler.Compile(*pEngineMesh, mesh_compiler::MESH_COMPILE_TANGENTS | mesh_compiler::MESH_COMPILE_IGNORE_TANGENT_SPACE_ERRORS);
		tMeshCompiling += pTimer->GetAsyncCurTime() - startTime;

		const char* szError = "";
		if (!FinalizeMesh(pEngineMesh, szError))
		{
		  LogPrintf("FinalizeMesh failed: %s\n", szError);
		}
	});

	const QString fileName = GetFilename(filePath);

	ResetCamera();

	m_pSceneTree->parentWidget()->setWindowTitle(tr("Scene: %1").arg(fileName));
	m_pGlobalImportSettings->SetInputFilePath(QtUtil::ToString(originalFilePath));
	m_pGlobalImportSettingsTree->revert();

	m_bRefreshRcMesh = true;

	// Resetting node maps.

	const int mapSize = pActiveScene->GetNodeCount() + 1;

	m_nodeRenderInfoMap.clear();
	m_nodeRenderInfoMap.resize(mapSize);

	m_nodeVisibilityMap.clear();
	m_nodeVisibilityMap.resize(mapSize, 0);

	// TODO: fix it, currently pPayload is processed in ApplyMetaData.
	m_pMaterialPanel->AssignScene(nullptr/*pPayload*/); 

	// Apply meta data, if present.

	if (pPayload)
	{
		if (pPayload->pMetaData)
		{
			ApplyMetaData(*pPayload->pMetaData);
		}

		if (!pPayload->targetFilename.isEmpty())
		{
			m_displayScene->cgfFilePath = pPayload->targetFilename;
		}
	}

	TouchLastJson();
}

const char* CMainDialog::GetDialogName() const
{
	return "Mesh";
}

void CMainDialog::ResetCamera()
{
	m_bCameraNeedsReset = true;       // Flags the camera for reset in next frame.
	m_bGlobalAabbNeedsRefresh = true; // Camera positioning needs valid bounding box.
}

void CMainDialog::SelectMaterialsOnMesh(const FbxTool::SMesh* pMesh)
{
	CMaterialView* const pView = m_pMaterialPanel->GetMaterialView();
	pView->selectionModel()->clear();
	for (int i = 0; i < pMesh->numMaterials; ++i)
	{
		const FbxTool::SMaterial* pMaterial = pMesh->ppMaterials[i];
		if (pMaterial)
		{
			const QString name = QtUtil::ToQStringSafe(pMaterial->szName);

			int numMaterials = pView->model()->rowCount(QModelIndex());
			for (int j = 0; j < numMaterials; ++j)
			{
				const QModelIndex index = pView->model()->index(j, 0);
				assert(index.isValid());
				CMaterialElement* pElement = pView->model()->AtIndex(index);
				if (pElement->GetName() == name)
				{
					pView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
			}
		}
	}
}

void CMainDialog::SelectMeshesUsingMaterial(const CMaterialElement* pMaterialElement)
{
	QItemSelectionModel* const pSelectionModel = m_pSceneTree->selectionModel();
	pSelectionModel->clear();

	const FbxTool::SMaterial* const pMaterial = pMaterialElement->GetMaterial();
	CSceneElementCommon* const pSceneElement = m_pSceneModel->GetRootElement();
	CRY_ASSERT(pSceneElement);
	SelectMeshesUsingMaterial(pMaterial, pSceneElement);
}

void CMainDialog::SelectMeshesUsingMaterial(const FbxTool::SMaterial* pNeedle, const CSceneElementCommon* pHaystack)
{
	assert(pNeedle && pHaystack);
	const int numChildren = pHaystack->GetNumChildren();
	for (int i = 0; i < numChildren; ++i)
	{
		CSceneElementCommon* const pElement = pHaystack->GetChild(i);
		if (pElement->GetType() == ESceneElementType::SourceNode)
		{
			CSceneElementSourceNode* pSourceNodeElement = (CSceneElementSourceNode*)pElement;
			if (pSourceNodeElement->GetNode() && pSourceNodeElement->GetNode()->numMeshes > 0)
		{
				const FbxTool::SMesh* const pMesh = pSourceNodeElement->GetNode()->ppMeshes[0];

			int numMaterials = pMesh->numMaterials;
			for (int j = 0; j < numMaterials; ++j)
			{
				if (pMesh->ppMaterials[j] == pNeedle)
				{
					const auto* const pFilter = m_pSceneViewContainer->GetFilter();
						const QModelIndex selector = pFilter->mapFromSource(m_pSceneModel->GetModelIndexFromSceneElement(pSourceNodeElement));
					if (selector.isValid())
					{
						m_pSceneTree->selectionModel()->select(selector, QItemSelectionModel::Select | QItemSelectionModel::Rows);
						m_pSceneTree->ExpandParents(selector);
					}
				}
			}
		}
		}
		SelectMeshesUsingMaterial(pNeedle, pElement);
	}
}

CSceneElementCommon* CMainDialog::GetSelectedSceneElement()
{
	if (m_pSceneTree->selectionModel()->selectedIndexes().isEmpty())
	{
		return nullptr;
	}
	const QModelIndex index = m_pSceneTree->selectionModel()->selectedIndexes().first();
	if (!index.isValid())
	{
		return nullptr;
	}

	const auto* const pFilter = m_pSceneViewContainer->GetFilter();

	return m_pSceneModel->GetSceneElementFromModelIndex(pFilter->mapToSource(index));
}

void CMainDialog::TouchLastJson()
{
	FbxMetaData::SMetaData metaData;
	if (CreateMetaData(metaData, ""))
	{
		m_displayScene->lastJson = metaData.ToJson();
	}
}

void CMainDialog::UnloadScene()
{
	m_pModelProperties->DetachPropertyObject();

	m_pSceneTree->parentWidget()->setWindowTitle(tr("Scene"));
	m_pGlobalImportSettings->ClearFilePaths();
	m_pGlobalImportSettingsTree->revert();
	m_pMaterialPanel->GetMaterialSettingsTree()->revert();
	m_pSceneModel->ClearScene();
	m_pTargetMeshView->model()->Clear();
	m_pMaterialPanel->GetMaterialView()->model()->ClearScene();

	GetScene()->RemoveObserver(this);

	m_pGlobalImportSettingsTree->detach();

	m_pMaterialPanel->UnloadScene();

	// Free static objects.
	for (size_t i = 0; i < m_displayScene->statObjs.size(); ++i)
	{
		m_displayScene->statObjs[i].pStatObj->Release();
	}
	m_displayScene->statObjs.clear();

	// Free uber-materials.
	m_uberMaterials.clear();

	m_pAutoMaterial.reset();

	ReleaseStaticMeshes();
	m_bRefreshRcMesh = false;

	// Clearing node maps.

	m_nodeRenderInfoMap.clear();
	m_nodeVisibilityMap.clear();

	m_bGlobalAabbNeedsRefresh = true;
}

static string MakeMaterialNameRelative(const string& dir, const string& materialName)
{
	// Prefix is asset-relative path with trailing slash.
	const string prefix = PathUtil::ToUnixPath(PathUtil::AddSlash(PathUtil::ToGamePath(dir)));
	const size_t prefixLen = prefix.length();
	if (dir.length() > 1 && !strnicmp(prefix.c_str(), materialName.c_str(), prefixLen))
	{
		return materialName.substr(prefixLen);
	}
	return materialName;
}

bool CMainDialog::SaveCgf(const std::shared_ptr<QTemporaryDir>& pTempDir, const string& targetFilePath, const QString& sourceFilename)
	{
	FbxMetaData::SMetaData metaData;
	if (!CreateMetaData(metaData, sourceFilename))
	{
		LogPrintf("%s: Creating meta data failed.", __FUNCTION__);
		return false;
	}

	if (m_bMaterialNameWasRelative)
	{
		const string targetPath = PathUtil::GetPathWithoutFilename(targetFilePath);
		metaData.materialFilename = MakeMaterialNameRelative(targetPath, metaData.materialFilename);
	}

	SRcObjectSaveState saveState;
	CaptureRcObjectSaveState(pTempDir, metaData, saveState);

	SProxyTree* const pProxyTree(new SProxyTree(m_pProxyData.get(), GetScene()));

	const QString absOriginalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();
	ThreadingUtils::Async([saveState, pProxyTree, absOriginalFilePath, targetFilePath, pTempDir]()
	{
		std::unique_ptr<SProxyTree> proxyTree(pProxyTree);

		// Asset relative path to directory. targetFilePath is absolute path.
		const string dir = PathUtil::GetPathWithoutFilename(PathUtil::AbsolutePathToGamePath(targetFilePath));

		const std::pair<bool, string> ret = CopySourceFileToDirectoryAsync(QtUtil::ToString(absOriginalFilePath), dir).get();
		if (!ret.first)
		{
			const string& error = ret.second;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Copying source file to '%s' failed: '%s'",
				dir.c_str(), error.c_str());
			return;
		}

		// TODO: Use generalized lambda-capture to move unique_ptr.
		proxyTree.release();
		SaveRcObjectAsync(saveState, targetFilePath, [pProxyTree](bool bSuccess, const string& filePath)
		{
			std::unique_ptr<SProxyTree> proxyTree(pProxyTree);
			if (bSuccess)
			{
				WriteAutoGenProxies(filePath, proxyTree.get());
			}
		});
	});

	return true;
}

bool CMainDialog::SaveSkin(const std::shared_ptr<QTemporaryDir>& pTempDir, const string& targetFilePath, const QString& sourceFilename)
	{
	FbxMetaData::SMetaData metaData;
	CreateSkinMetaData(metaData);

	if (m_bMaterialNameWasRelative)
	{
		const string targetPath = PathUtil::GetPathWithoutFilename(targetFilePath);
		metaData.materialFilename = MakeMaterialNameRelative(targetPath, metaData.materialFilename);
	}

	SRcObjectSaveState saveState;
	CaptureRcObjectSaveState(pTempDir, metaData, saveState);

	const QString absOriginalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();
	ThreadingUtils::Async([saveState, absOriginalFilePath, targetFilePath, pTempDir]()
	{
		// Asset relative path to directory. targetFilePath is absolute path.
		const string dir = PathUtil::GetPathWithoutFilename(PathUtil::AbsolutePathToGamePath(targetFilePath));

		const std::pair<bool, string> ret = CopySourceFileToDirectoryAsync(QtUtil::ToString(absOriginalFilePath), dir).get();
		if (!ret.first)
	{
			const string& error = ret.second;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Copying source file to '%s' failed: '%s'",
				dir.c_str(), error.c_str());
			return;
	}

		SaveRcObjectAsync(saveState, targetFilePath);
	});

	return true;
	}

bool CMainDialog::NeedAutoLod() const
{
	using namespace Private_MainDialog;

	CRY_ASSERT(!GetAutoLodNodes(GetScene()).empty());

	if (!GetIEditor()->GetSystem()->GetIConsole()->GetCVar("mi_lazyLodGeneration")->GetIVal())
	{
	return true;
}

	if (m_pShowMeshesModeWidget->GetShowMeshesMode() != CShowMeshesModeWidget::eShowMeshesMode_All)
{
		return false;
	}

	if (!m_viewSettings.lod)
	{
		return false;
	}

	return true;
}

//! Properties read from chunkfile (not exposed by statobj).
struct SChunkFileContent
{
	string geoName;
	int    subObjectMeshCount;

	void ReadChunkfileContent(const char* szFilePath);
};

void SChunkFileContent::ReadChunkfileContent(const char* szFilePath)
{
	CContentCGF* pCGF = GetIEditor()->Get3DEngine()->CreateChunkfileContent(szFilePath);
	if (pCGF && GetIEditor()->Get3DEngine()->LoadChunkFileContent(pCGF, szFilePath))
{
		subObjectMeshCount = 0;
		geoName = "";

		for (int i = 0; i < pCGF->GetNodeCount(); i++)
{
			CNodeCGF* pNode = pCGF->GetNode(i);
			if (pNode->type == CNodeCGF::NODE_MESH)
{
				if (geoName.empty())
	{
					geoName = pNode->name;
				}
				subObjectMeshCount++;
			}
		}
	}
	}

static bool IsProxyFromName(const char* szName)
{
	const char* reservedNames[] = { CGF_NODE_NAME_PHYSICS_PROXY0, CGF_NODE_NAME_PHYSICS_PROXY1, CGF_NODE_NAME_PHYSICS_PROXY2 };
	for (const char* szProxyName : reservedNames)
	{
		static const size_t proxyNameLength = strlen(szProxyName);
		if (strnicmp(szName, szProxyName, proxyNameLength) == 0)
	{
			return true;
		}
	}
	return false;
}

static string GetCGFNodeName(const FbxTool::CScene* pScene, const FbxTool::SNode* pNode)
{
	const int lodIndex = pScene->GetNodeLod(pNode);
	const bool isProxy = pScene->IsProxy(pNode);

	if (isProxy && (lodIndex > 0))
{
		CRY_ASSERT(0 && "'Physics Proxy' and 'LOD[1..n]' are mutually exclusive properties of node.");
}

	if (isProxy)
	{
		if (IsProxyFromName(pNode->szName))
{
			return pNode->szName;
}

		return string().Format("%s_%s", CGF_NODE_NAME_PHYSICS_PROXY2, pNode->szName);
	}

	static const size_t lodNamePrefixLength = strlen(CGF_NODE_NAME_LOD_PREFIX);
	int lodIndexFromName = -1;
	if (!strnicmp(pNode->szName, CGF_NODE_NAME_LOD_PREFIX, lodNamePrefixLength))
	{
		lodIndexFromName = atoi(pNode->szName + lodNamePrefixLength);
	}

	if (lodIndex == lodIndexFromName)
	{
		return pNode->szName;
	}

	if (lodIndex > 0)
{
		return string().Format("%s%d_%s", CGF_NODE_NAME_LOD_PREFIX, lodIndex, pNode->szName);
	}

	if (lodIndexFromName > 0)
	{
		return string().Format("lod0_%s", pNode->szName);
	}

	return pNode->szName;
}

void CMainDialog::CreateUnmergedContent()
{
	using namespace Private_MainDialog;

	IStatObj* pUnmergedStatObj = FindUnmergedMesh();
	auto& content = m_rcMeshUnmergedContent;

	SStatObjChild child;

	content.clear();

	if (!pUnmergedStatObj)
	{
		return;
	}

	std::vector<IStatObj*> statObjStack;

	SChunkFileContent chunkFileContent;
	// chunkFileContent.ReadChunkfileContent(pRcMesh->filePath.toLocal8Bit().constData());
	chunkFileContent.ReadChunkfileContent(pUnmergedStatObj->GetFilePath());

	// Add self.
	child.pStatObj = pUnmergedStatObj;
	child.localToWorld = Matrix34(IDENTITY);
	child.geoName = chunkFileContent.geoName;
	child.pNode = nullptr;
	statObjStack.push_back(child.pStatObj);

	if (chunkFileContent.subObjectMeshCount <= 1)
	{
		content.push_back(child);
	}

	// Add all subobjects and LODs.
	while (!statObjStack.empty())
	{
		IStatObj* const pStatObj = statObjStack.back();
		statObjStack.pop_back();

		for (size_t i = 0; i < pStatObj->GetSubObjectCount(); ++i)
		{
			IStatObj::SSubObject* const pSubObj = pStatObj->GetSubObject(i);

			if (pSubObj->pStatObj)
			{
				child.pStatObj = pSubObj->pStatObj;
				child.localToWorld = pSubObj->tm;
				child.geoName = pSubObj->pStatObj->GetGeoName();
				child.pNode = nullptr;

				content.push_back(child);
				statObjStack.push_back(child.pStatObj);
			}
		}

		for (size_t i = 1; i < MAX_STATOBJ_LODS_NUM; ++i)
		{
			IStatObj* const pLodObj = pStatObj->GetLodObject(i, false);
			if (pLodObj)
			{
				child.pStatObj = pLodObj;
				child.localToWorld = Matrix34(IDENTITY);
				child.geoName = pLodObj->GetGeoName();
				child.pNode = nullptr;

				content.push_back(child);
				statObjStack.push_back(child.pStatObj);
			}
		}
	}

	// Resolve nodes.
	const FbxTool::CScene* const pScene = GetScene();
	for (size_t i = 0; i < content.size(); ++i)
	{
		SStatObjChild& child = content[i];
		child.pNode = nullptr;

		for (size_t j = 0; j < m_pSceneModel->GetElementCount(); ++j)
		{
			if (m_pSceneModel->GetElementById(j)->GetType() != ESceneElementType::SourceNode)
			{
				continue;
			}

			const CSceneElementSourceNode* const pSceneElement = (CSceneElementSourceNode*)m_pSceneModel->GetElementById(j);

			const FbxTool::SNode* const pNode = pSceneElement->GetNode();
			if (child.geoName == GetCGFNodeName(pScene, pNode))
			{
				child.pNode = pNode;
			}
		}
	}

	content.erase(std::remove_if(content.begin(), content.end(), [](const SStatObjChild& child) { return !child.pNode; }), content.end());
}

static FbxMetaData::TString DeserializeMaterialName(const FbxMetaData::TString& name)
{
	if (name.empty())
	{
		return FbxTool::SMaterial::DummyMaterialName();
	}
	return name;
}

namespace
{

FbxMetaData::EExportSetting ConvertExportSetting(FbxTool::ENodeExportSetting exportSetting)
{
	switch (exportSetting)
	{
	case FbxTool::eNodeExportSetting_Exclude:
		return FbxMetaData::EExportSetting::Exclude;
	case FbxTool::eNodeExportSetting_Include:
		return FbxMetaData::EExportSetting::Include;
	default:
		assert(0 && "unkown export setting");
		return FbxMetaData::EExportSetting::Exclude;
	}
}

FbxTool::ENodeExportSetting ConvertExportSetting(FbxMetaData::EExportSetting exportSetting)
{
	switch (exportSetting)
	{
	case FbxMetaData::EExportSetting::Exclude:
		return FbxTool::eNodeExportSetting_Exclude;
	case FbxMetaData::EExportSetting::Include:
		return FbxTool::eNodeExportSetting_Include;
	default:
		assert(0 && "unkown export setting");
		return FbxTool::eNodeExportSetting_Exclude;
	}
}

} // namespace

void CreateNodeMetaData(const FbxTool::CScene* pScene, const FbxTool::SNode* pNode, FbxMetaData::SNodeMeta& nodeMeta)
{
	nodeMeta.name = GetCGFNodeName(pScene, pNode);
	nodeMeta.nodePath = FbxTool::GetPath(pNode);

	nodeMeta.properties = pScene->GetPhysicalProperties(pNode);
	nodeMeta.udps = pScene->GetUserDefinedProperties(pNode);
		}

void WriteNodeMetaData(FbxMetaData::SMetaData& metaData, const FbxTool::CScene* pScene)
{
	using namespace Private_MainDialog;

	struct SStackItem
	{
		const FbxTool::SNode*                pNode;
		std::vector<FbxMetaData::SNodeMeta>* pParentMeta;
	};

	std::vector<SStackItem> stack;

	const FbxTool::SNode* const pRoot = pScene->GetRootNode();
	metaData.nodeData.reserve(pRoot->numChildren);
	for (int i = 0; i < pRoot->numChildren; ++i)
	{
		SStackItem item;
		item.pNode = pRoot->ppChildren[i];
		item.pParentMeta = &metaData.nodeData;
		stack.push_back(item);
	}

	while (!stack.empty())
	{
		SStackItem item = stack.back();
		stack.pop_back();

		if (!pScene->IsNodeIncluded(item.pNode))
		{
			continue;
		}

		// RC fails on empty meshes.
		if (item.pNode->bAttributeMesh && !(item.pNode->ppMeshes[0]->numTriangles && item.pNode->ppMeshes[0]->numVertices))
		{
			LogPrintf("Node '%s' has empty mesh. Do not write to Json.", item.pNode->szName);
			continue;
		}

		// Condition (A) allows the loading of SketchUp files whose single root node has no attributes at all.
		const bool bWrite =
		  !item.pNode->bAnyAttributes ||  // (A)
		  item.pNode->bAttributeMesh ||
		  item.pNode->bAttributeNull;
		if (!bWrite)
		{
			continue;
		}

		item.pParentMeta->emplace_back();
		FbxMetaData::SNodeMeta* const pMeta = &item.pParentMeta->back();

		CreateNodeMetaData(pScene, item.pNode, *pMeta);

		pMeta->children.reserve(item.pNode->numChildren);
		for (int i = 0; i < item.pNode->numChildren; ++i)
		{
			SStackItem childItem;
			childItem.pNode = item.pNode->ppChildren[i];
			childItem.pParentMeta = &pMeta->children;
			stack.push_back(childItem);
		}
	}
}

bool CMainDialog::CreateMetaData(FbxMetaData::SMetaData& metaData, const QString& sourceFilename, int flags)
{
	using namespace Private_MainDialog;

	metaData.Clear();

	if (!GetScene())
	{
		return false;
	}

	CRY_ASSERT(m_pGlobalImportSettings);

	// Gather conversion settings.
	metaData.unit = m_pGlobalImportSettings->GetUnit();
	metaData.scale = m_pGlobalImportSettings->GetScale();

	metaData.forwardUpAxes = GetFormattedAxes(
	  m_pGlobalImportSettings->GetUpAxis(),
	  m_pGlobalImportSettings->GetForwardAxis());

	CSortedMaterialModel* const pMaterialModel = m_pMaterialPanel->GetMaterialModel();
	assert(m_pSceneModel && pMaterialModel);
	m_pMaterialPanel->ApplyMaterial();

	const FbxTool::CScene* const pScene = GetScene();

	const int numMaterials = pScene->GetMaterialCount();

	if (numMaterials == 0)
	{
		LogPrintf("CMainDialog::CreateMetaData: warning - FBX does not contain any materials\n");
	}

	WriteMaterialMetaData(pScene, metaData.materialData);

	metaData.settings = m_pSceneModel->GetSceneUserSettings();
	metaData.sourceFilename = QtUtil::ToString(sourceFilename);
	metaData.outputFileExt = "cgf";

	if (m_pMaterialPanel->GetMaterialSettings()->GetMaterial())
	{
		metaData.materialFilename = m_pMaterialPanel->GetMaterialSettings()->GetMaterialName();
	}
	else
	{
		// If no material has been selected, we assign an existing default material. This way, a mesh
		// always has a valid material.
		// Alternatively, we could create a material from the FBX. For previewing, however, this is
		// tricky, as we do not know where to save it, and we want to avoid having temporaries in the
		// asset directory.
		metaData.materialFilename = "%ENGINE%/EngineAssets/Materials/material_default";
	}

	////////////////////////////////////////////////////
	// Write node meta data.
	WriteNodeMetaData(metaData, pScene);

	metaData.bMergeAllNodes = m_pGlobalImportSettings->IsMergeAllNodes();
	metaData.bSceneOrigin = m_pGlobalImportSettings->IsSceneOrigin();
	metaData.bComputeNormals = m_pGlobalImportSettings->IsComputeNormals();
	metaData.bComputeUv = m_pGlobalImportSettings->IsComputeUv();

	// Output settings
	metaData.bVertexPositionFormatF32 = m_pGlobalImportSettings->IsVertexPositionFormatF32();

	////////////////////////////////////////////////////
	// Save/restore editor state.
	auto pEditorMetaData = std::make_unique<SEditorMetaData>();
	pEditorMetaData->editorGlobalImportSettings = *m_pGlobalImportSettings;
	m_pAutoLodSettings->getNodeList() = GetAutoLodNodes(GetScene());
	if ((flags & eCreateMetaDataFlags_OmitAutoLods) == 0)
	{
	*metaData.pAutoLodSettings = *m_pAutoLodSettings;
	}
	FbxTool::Meta::WriteNodeMetaData(*GetScene(), pEditorMetaData->editorNodeMeta);
	FbxTool::Meta::WriteMaterialMetaData(*GetScene(), pEditorMetaData->editorMaterialMeta);
	metaData.pEditorMetaData = std::move(pEditorMetaData);

	return true;
}

struct SBlockSignals
{
	QWidget* pWidget;
	bool     bOldBlocked;

	explicit SBlockSignals(QWidget* pWidget)
		: pWidget(pWidget)
		, bOldBlocked(pWidget->signalsBlocked())
	{
		pWidget->blockSignals(true);
	}

	~SBlockSignals()
	{
		pWidget->blockSignals(bOldBlocked);
	}
};

static FbxTool::Axes::EAxis ParseAxis(const FbxMetaData::TString str)
{
	if (str == "+X")
	{
		return FbxTool::Axes::EAxis::PosX;
	}
	else if (str == "+Y")
	{
		return FbxTool::Axes::EAxis::PosY;
	}
	else if (str == "+Z")
	{
		return FbxTool::Axes::EAxis::PosZ;
	}
	if (str == "-X")
	{
		return FbxTool::Axes::EAxis::NegX;
	}
	else if (str == "-Y")
	{
		return FbxTool::Axes::EAxis::NegY;
	}
	else if (str == "-Z")
	{
		return FbxTool::Axes::EAxis::NegZ;
	}

	assert(0 && "unknown axis");
	return FbxTool::Axes::EAxis::PosX;
}

static CSceneElementCommon* FindChildElement(
  CSceneElementCommon* pParent,
  std::vector<string>::const_iterator pathBegin,
  std::vector<string>::const_iterator pathEnd)
{
	assert(pParent);

	if (pathBegin == pathEnd)
	{
		return pParent;
	}

	for (int i = 0; i < pParent->GetNumChildren(); ++i)
	{
		CSceneElementCommon* const pChild = pParent->GetChild(i);
		if (pChild->GetName() == *pathBegin)
		{
			return FindChildElement(pChild, pathBegin + 1, pathEnd);
		}
	}

	return nullptr;
}

static CSceneElementCommon* FindChildElement(CSceneElementCommon* pParent, const std::vector<string>& path)
{
	return FindChildElement(pParent, path.cbegin(), path.cend());
}

static string GetNodePathAsString(const std::vector<string>& path)
{
	if (path.empty())
	{
		return string();
	}
	auto pathIt = path.cbegin();
	string result = *pathIt;
	while (pathIt != path.cend())
	{
		result += '/';
		result += *pathIt;
		++pathIt;
	}
	return result;
}

void CMainDialog::ApplyMetaDataCommon(const FbxMetaData::SMetaData& metaData)
{
	// Apply global settings.

	m_pGlobalImportSettings->SetUnit(metaData.unit);
	m_pGlobalImportSettings->SetScale(metaData.scale);

	if (!metaData.forwardUpAxes.empty())
	{
	assert(4 == metaData.forwardUpAxes.length());
	m_pGlobalImportSettings->SetForwardAxis(ParseAxis(metaData.forwardUpAxes.substr(0, 2)));
	m_pGlobalImportSettings->SetUpAxis(ParseAxis(metaData.forwardUpAxes.substr(2, 2)));
	}
	else
	{
		// Empty axes string means default axes.
		// This convention mainly benefits the importing of assets with the asset browser, i.e., class
		// CAssetImporterFBX, since we do not need to read the FBX file on editor side just to extract
		// the default axes. As a consequence, however, the final CGF also contains this empty string,
		// hence this workaround.
		// Note that the RC leaves the meta-data unchanged.
		FbxTool::CScene* const pActiveScene = GetScene();
		m_pGlobalImportSettings->SetUpAxis(pActiveScene->GetUpAxis());
		m_pGlobalImportSettings->SetForwardAxis(pActiveScene->GetForwardAxis());
	}

	// Apply material element settings.

	CSortedMaterialModel* const pMaterialModel = m_pMaterialPanel->GetMaterialModel();

	for (size_t i = 0; i < metaData.materialData.size(); ++i)
	{
		const FbxMetaData::SMaterialMetaData& materialMeta = metaData.materialData[i];
		const QString name = QtUtil::ToQString(DeserializeMaterialName(materialMeta.name));
		CMaterialElement* const pMaterialElement = pMaterialModel->FindElement(name);

		if (!pMaterialElement)
		{
			LogPrintf("Error. Cannot find material for meta data with name = %s\n", QtUtil::ToString(name).c_str());
			continue;
		}

		if (materialMeta.settings.subMaterialIndex < 0)
		{
			pMaterialElement->MarkForDeletion();
		}
		else
		{
			pMaterialElement->SetSubMaterialIndex(materialMeta.settings.subMaterialIndex);
		}

		pMaterialElement->SetPhysicalizeSetting(materialMeta.settings.physicalizeSetting);

		pMaterialElement->MarkForIndexAutoAssignment(materialMeta.settings.bAutoAssigned);
	}

	assert(m_pGlobalImportSettings);

	m_pGlobalImportSettings->SetMergeAllNodes(metaData.bMergeAllNodes);
	m_pGlobalImportSettings->SetSceneOrigin(metaData.bSceneOrigin);
	m_pGlobalImportSettings->SetVertexPositionFormatF32(metaData.bVertexPositionFormatF32);
	m_pGlobalImportSettings->SetComputeNormals(metaData.bComputeNormals);
	m_pGlobalImportSettings->SetComputeUv(metaData.bComputeUv);

	m_pGlobalImportSettingsTree->revert();

	CRY_ASSERT(metaData.pEditorMetaData);
	if (metaData.pEditorMetaData)
	{
		SEditorMetaData* const pEditorMetaData = (SEditorMetaData*)metaData.pEditorMetaData.get();
		FbxTool::Meta::ReadNodeMetaData(pEditorMetaData->editorNodeMeta, *GetScene());
		FbxTool::Meta::ReadMaterialMetaData(pEditorMetaData->editorMaterialMeta, *GetScene());
	}

	{
		// We read the material from the StatObj instead of the json, so that we get a fully qualified
		// name (i.e., the StatObj loading does the lookup).
		const string targetFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), GetTargetFilePath());
		IStatObj* const pTarget = GetIEditor()->Get3DEngine()->LoadStatObj(targetFilePath.c_str(), NULL, NULL, false);

		// Get material name (asset relative path) from absolute path.
		const string materialName = pTarget->GetMaterial() ? PathUtil::ToGamePath(pTarget->GetMaterial()->GetName()) : string();

		m_bMaterialNameWasRelative = materialName != metaData.materialFilename;

		m_pMaterialPanel->GetMaterialSettings()->SetMaterial(materialName);
	m_pMaterialPanel->GetMaterialSettingsTree()->revert();
	}

	m_bRefreshRcMesh = true;
	m_bGlobalAabbNeedsRefresh = true;

	ResetCamera();

	m_pMaterialPanel->ApplyMaterial();
}

void CMainDialog::ApplyMetaDataSkin(const FbxMetaData::SMetaData& metaData)
{
	CRY_ASSERT(metaData.nodeData.size() == 1);
	const FbxMetaData::SNodeMeta& nodeMeta = metaData.nodeData.front();
	const FbxTool::SNode* pNode = FbxTool::FindChildNode(GetScene()->GetRootNode(), nodeMeta.nodePath);
	if (!pNode)
	{
		return;
	}
	m_pSceneUserData->SelectAnySkin(pNode);
}

bool CMainDialog::ApplyMetaData(const FbxMetaData::SMetaData& metaData)
{
	ApplyMetaDataCommon(metaData);

	if (!strcmp(metaData.outputFileExt, "skin"))
	{
		ApplyMetaDataSkin(metaData);
	}

	return true;
}

static FbxTool::ENodeExportSetting EvaluateExportSetting(const FbxTool::SNode* pFbxNode)
{
	return pFbxNode->pScene->IsNodeIncluded(pFbxNode) ? FbxTool::eNodeExportSetting_Include : FbxTool::eNodeExportSetting_Exclude;
}

void CMainDialog::AddSourceNodeElementContextMenu(const QModelIndex& index, QMenu* pMenu)
{
	CSceneElementCommon* const pElement = m_pSceneModel->GetSceneElementFromModelIndex(index);

	if (!pElement || pElement->GetType() != ESceneElementType::SourceNode)
	{
		return;
	}

	auto* const pSourceNodeElement = (CSceneElementSourceNode*)pElement;

	const FbxTool::SNode* const pNode = pSourceNodeElement->GetNode();

		// Include/Exclude

		QAction* const pInclude = pMenu->addAction(tr("Include this node"));
		connect(pInclude, &QAction::triggered, [=]()
		{
			if (index.isValid())
			{
			const FbxTool::ENodeExportSetting oldExportSetting = EvaluateExportSetting(pNode);
			GetScene()->SetNodeExportSetting(pNode, FbxTool::eNodeExportSetting_Include);
			m_bRefreshRcMesh = m_bRefreshRcMesh || oldExportSetting != EvaluateExportSetting(pNode);
			  if (m_bRefreshRcMesh)
			  {
			    m_pTargetMeshView->model()->Clear();
			  }
			}
		});

		QAction* const pExclude = pMenu->addAction(tr("Exclude this node"));
		connect(pExclude, &QAction::triggered, [=]()
		{
			if (index.isValid())
			{
			const FbxTool::ENodeExportSetting oldExportSetting = EvaluateExportSetting(pNode);
			  m_pSceneModel->SetExportSetting(index, FbxTool::eNodeExportSetting_Exclude);
			m_bRefreshRcMesh = m_bRefreshRcMesh || oldExportSetting != EvaluateExportSetting(pNode);
			  //m_pTargetMeshView->model()->Rebuild();
			  if (m_bRefreshRcMesh)
			  {
			    m_pTargetMeshView->model()->Clear();
			  }
			}
		});

		pMenu->addSeparator();

		// Reset scene's export settings.

	if (!pNode->pParent)
		{
			QAction* pAct = pMenu->addAction(tr("Reset selection"));
			connect(pAct, &QAction::triggered, [=]()
			{
				const char* const szText =
				  "Do you really want to reset the selection? "
				  "All node export settings of this LOD will be discarded.";
				if (QDialogButtonBox::Yes == CQuestionDialog::SQuestion(tr("Reset selection"), tr(szText)))
				{
				  m_pSceneModel->ResetExportSettingsInSubtree(index);
				  m_bRefreshRcMesh = true;
				  m_pTargetMeshView->model()->Clear();
				}
			});
			pMenu->addSeparator();
		}

		// Material selection

	if (pNode->numMeshes)
		{
			QAction* const pSelectMaterials = pMenu->addAction(tr("Select materials used by mesh"));
			connect(pSelectMaterials, &QAction::triggered, [=]()
			{
			CSceneElementCommon* const pElement = m_pSceneModel->GetSceneElementFromModelIndex(index);
			if (pElement->GetType() == ESceneElementType::SourceNode)
				{
				CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pElement;
				const FbxTool::SMesh* const pMesh = pSourceNodeElement->GetNode()->ppMeshes[0];
				  if (pMesh)
				  {
				    SelectMaterialsOnMesh(pMesh);
				  }
				}
			});

			pMenu->addSeparator();
		}
}

void CMainDialog::CreateMaterialContextMenu(QMenu* pMenu, CMaterialElement* pElement)
{
	QAction* const pSelectMeshes = pMenu->addAction(tr("Select meshes using this material"));
	pSelectMeshes->setEnabled(pElement != nullptr);
	connect(pSelectMeshes, &QAction::triggered, [=]()
	{
		if (pElement)
		{
		  SelectMeshesUsingMaterial(pElement);
		}
	});
}

void ClearAutoLodNodes(FbxMetaData::SMetaData& metaData)
{
	metaData.pAutoLodSettings.reset(new CAutoLodSettings());
}

void CMainDialog::UpdateStaticMeshes()
{	
	using namespace Private_MainDialog;

	FbxMetaData::SMetaData metaData;
	const QString filePath = GetSceneManager().GetImportFile()->GetFilePath();
	if (!CreateMetaData(metaData, filePath))
	{
		LogPrintf("Creating meta data failed.\n");
		return;
	}

	m_bAutoLodCheckViewSettingsFlag = false;

	// If necessary, create second, unmerged mesh.
	if (m_pGlobalImportSettings->IsMergeAllNodes())
	{
		FbxMetaData::SMetaData unmergedMeshMetaData = metaData;
		unmergedMeshMetaData.bMergeAllNodes = false;
		ClearAutoLodNodes(unmergedMeshMetaData);

		m_pUnmergedMeshRcObject->SetMetaData(unmergedMeshMetaData);
		m_pUnmergedMeshRcObject->CreateAsync();

		unmergedMeshMetaData.bMergeAllNodes = true;

		m_ppSelectionMesh = &m_pUnmergedMeshStatObj;
	}
	else
	{
		m_ppSelectionMesh = &m_pMeshStatObj;
	}

	FbxMetaData::SMetaData meshMetaData = metaData;

	m_pMeshRcObject->ClearMessage();
	bool bHasLods = false;

	if (!GetAutoLodNodes(GetScene()).empty())
	{
		// Even when some nodes want auto-generated LODs, we only create them for preview when really needed.
		if (NeedAutoLod())
		{
			m_pMeshRcObject->SetMessage("Generating LODs...");
			bHasLods = true;
		}
		else
		{
			ClearAutoLodNodes(meshMetaData);
			m_bAutoLodCheckViewSettingsFlag = !m_viewSettings.lod;
		}
	}

	m_pMeshRcObject->SetFinalize(CTempRcObject::Finalize([this, bHasLods](const CTempRcObject* pTempRcObject)
	{
		const string filePath = PathUtil::ReplaceExtension(pTempRcObject->GetFilePath(), "cgf");
		CreateMeshFromFile(filePath);

		m_bHasLods = bHasLods;
	}));
	m_pMeshRcObject->SetMetaData(meshMetaData);
	m_pMeshRcObject->CreateAsync();
}

IStatObj* CMainDialog::FindUnmergedMesh()
{
	IStatObj* pUnmergedMesh = nullptr;
	if (m_ppSelectionMesh)
	{
		pUnmergedMesh = *m_ppSelectionMesh;
	}
	return pUnmergedMesh;
}

void CMainDialog::ReleaseStaticMeshes()
{
	m_pMeshStatObj = nullptr;
	m_pUnmergedMeshStatObj = nullptr;

	m_rcMeshUnmergedContent.clear();
}

void CMainDialog::CreateSkinMetaData(FbxMetaData::SMetaData& metaData)
{
	const QString filePath = GetSceneManager().GetImportFile()->GetFilePath();
	if (!CreateMetaData(metaData, filePath))
	{
		LogPrintf("Creating meta data failed.\n");
		return;
	}

	metaData.outputFileExt = "skin";

	CRY_ASSERT(m_pSceneUserData && m_pSceneUserData->GetSelectedSkin());
	metaData.nodeData.clear();
	
	FbxMetaData::SNodeMeta nodeMeta;
	CreateNodeMetaData(GetScene(), m_pSceneUserData->GetSelectedSkin()->pNode, nodeMeta);
	metaData.nodeData.push_back(nodeMeta);
}

void CMainDialog::UpdateSkin()
{
	FbxMetaData::SMetaData metaData;
	CreateSkinMetaData(metaData);

	m_pSkinRcObject->SetMetaData(metaData);
	m_pSkinRcObject->CreateAsync();
}

