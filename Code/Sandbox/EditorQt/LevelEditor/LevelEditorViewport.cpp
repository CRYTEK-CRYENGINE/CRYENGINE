// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LevelEditorViewport.h"

#include "AI/AIManager.h"
#include "Objects/CameraObject.h"
#include "QT/Widgets/QViewportHeader.h"
#include "Terrain/Heightmap.h"
#include "Util/Ruler.h"
#include "CryEditDoc.h"
#include "GameEngine.h"
#include "ObjectCreateTool.h"
#include "ProcessInfo.h"
#include "ViewManager.h"

#include <AssetSystem/AssetManager.h>
#include <Controls/DynamicPopupMenu.h>
#include <EditorFramework/Events.h>
#include <Gizmos/IGizmoManager.h>
#include <Gizmos/Gizmo.h>
#include <LevelEditor/Tools/EditTool.h>
#include <Objects/BaseObject.h>
#include "Objects/ObjectManager.h"
#include <Preferences/SnappingPreferences.h>
#include <Preferences/ViewportPreferences.h>
#include <ILevelEditor.h>
#include <IObjectManager.h>
#include <IUndoManager.h>
#include <IViewportManager.h>
#include <QtUtil.h>
#include <RenderLock.h>
#include <ViewportInteraction.h>
#include <Util/AffineParts.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CryAISystem/IAISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameFramework.h>
#include <CryInput/IHardwareMouse.h>
#include <CryInput/IInput.h>
#include <CryMath/Cry_GeoIntersect.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <DefaultComponents/Cameras/CameraComponent.h>

#include <QResizeEvent>

inline bool SortCameraObjectsByName(CEntityObject* pObject1, CEntityObject* pObject2)
{
	return stricmp(pObject1->GetName(), pObject2->GetName()) < 0;
}

//////////////////////////////////////////////////////////////////////////
static void ToggleBool(bool* variable, bool* disableVariableIfOn)
{
	*variable = !*variable;
	if (*variable && disableVariableIfOn)
		*disableVariableIfOn = false;
}

//////////////////////////////////////////////////////////////////////////
static void ToggleInt(int* variable)
{
	*variable = !*variable;
}
//////////////////////////////////////////////////////////////////////////
static CPopupMenuItem& AddCheckbox(CPopupMenuItem& menu, const char* text, bool* variable, bool* disableVariableIfOn = 0)
{
	return menu.Add(text, functor(ToggleBool), variable, disableVariableIfOn).Check(*variable);
}

//////////////////////////////////////////////////////////////////////////
static CPopupMenuItem& AddCheckbox(CPopupMenuItem& menu, const char* text, int* variable)
{
	return menu.Add(text, functor(ToggleInt), variable).Check(*variable);
}

namespace Private_AssetDrag
{
static CAsset* GetAsset(const CDragDropData& pDragDropData)
{
	if (!pDragDropData.HasCustomData("Assets"))
	{
		return nullptr;
	}

	QVector<quintptr> assets;
	{
		QByteArray byteArray = pDragDropData.GetCustomData("Assets");
		QDataStream stream(byteArray);
		stream >> assets;
	}
	assert(!assets.empty());

	if (assets.size() > 1)
	{
		// We do not handle multi-asset drops.
		return nullptr;
	}

	return reinterpret_cast<CAsset*>(assets[0]);
}

static bool FindClassName(const CDragDropData* pDragDropData, string& className)
{
	if (!pDragDropData->HasCustomData("EngineFilePaths") || !pDragDropData->HasCustomData("ObjectClassNames"))
	{
		return false;
	}

	QStringList engineFilePaths;
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);
		stream >> engineFilePaths;
	}

	QStringList objectClassNames;
	{
		QByteArray byteArray = pDragDropData->GetCustomData("ObjectClassNames");
		QDataStream stream(byteArray);
		stream >> objectClassNames;
	}

	assert(!engineFilePaths.empty() && !objectClassNames.empty());

	if (engineFilePaths.size() > 1)
	{
		// We do not handle multi-asset drops.
		return false;
	}

	className = QtUtil::ToString(objectClassNames.front());
	return !className.empty();
}
}

CLevelEditorViewport::CLevelEditorViewport()
	: m_camFOV(gViewportPreferences.defaultFOV)
	, m_headerWidget(nullptr)
{
}

CLevelEditorViewport::~CLevelEditorViewport()
{
	if (this == GetIEditor()->GetViewManager()->GetGameViewport())
	{
		GetIEditor()->GetGameEngine()->SetGameMode(false);
	}
}

bool CLevelEditorViewport::CreateRenderContext(CRY_HWND hWnd, IRenderer::EViewportType viewportType)
{
	return CRenderViewport::CreateRenderContext(hWnd, IRenderer::eViewportType_Default);
}

bool CLevelEditorViewport::DragEvent(EDragEvent eventId, QEvent* event, int flags)
{
	bool result = CRenderViewport::DragEvent(eventId, event, flags);
	if (!result)
	{
		return HandleDragEvent(eventId, event, flags);
	}
	return result;
}

bool CLevelEditorViewport::HandleDragEvent(EDragEvent eventId, QEvent* event, int flags)
{

	CDragDropData::ClearDragTooltip(GetViewWidget());

	//just return if we are leaving from drop action
	if (eventId == EDragEvent::eDragLeave)
	{
		return true;
	}

	//Get drag&drop info from generic QEvent
	QDropEvent* const pDragEvent = (QDropEvent*)event;
	const CDragDropData* const pDragDropData = CDragDropData::FromMimeData(pDragEvent->mimeData());

	string className;

	//Check if this event is an asset drop
	if (DropHasAsset(*pDragDropData))
	{
		//Check if we can apply this asset to an object
		if (ApplyAsset(*pDragDropData, pDragEvent, eventId))
		{
			return true;
		}

		//Check if this asset has a class name that can be instantiated in the level
		CAsset* pAsset = Private_AssetDrag::GetAsset(*pDragDropData);
		className = pAsset->GetType()->GetObjectClassName();

		if (className.empty())
		{
			if (eventId == eDragEnter)
			{
				event->accept();
				return true;
			}

			return false;
		}

		if (className == "Environment")
		{
			if (!GetIEditor()->GetDocument()->IsDocumentReady())
			{
				return false;
			}

			switch (eventId)
			{
			case eDragEnter:
				break;
			case eDragMove:
				CDragDropData::ShowDragText(GetViewWidget(), QString("%1 \"%2\"").arg(QObject::tr("Apply environment"), QtUtil::ToQString(pAsset->GetName())));
				break;
			case eDragLeave:
				CDragDropData::ClearDragTooltip(GetViewWidget());
				break;
			case eDrop:
				{
					pDragEvent->acceptProposedAction();
					event->accept();

					ITimeOfDay* const pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();
					const string preset = pAsset->GetType()->GetObjectFilePath(pAsset);
					if (pTimeOfDay->SetDefaultPreset(preset))
					{
						const char* szMessage = QT_TR_NOOP("Default environment now set to");
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s %s", szMessage, pAsset->GetName().c_str());
						return true;
					}
					else if (pTimeOfDay->LoadPreset(preset) && pTimeOfDay->SetDefaultPreset(preset))
					{
						const char* szMessage = QT_TR_NOOP("Environment Asset added to the level in Level Settings. Default environment now set to");
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s %s", szMessage, pAsset->GetName().c_str());
						return true;
					}
					return false;
				}
				break;
			}
			event->accept();
			return true;
		}
	}

	//The asset does not have a valid class name, search in the dragDrop data for a class name that we can instantiate
	if (className.empty())
	{
		if (pDragDropData && !Private_AssetDrag::FindClassName(pDragDropData, className))
		{
			return false;
		}
	}

	//Create the ObjectCreateTool too for this class
	if (!GetIEditor()->StartObjectCreation(className.c_str(), nullptr))
	{
		return false;
	}

	//If we destroy the drag object we want to stop creation
	QDrag* const pDrag = pDragEvent->source()->findChild<QDrag*>();
	assert(pDrag);
	QObject::connect(pDrag, &QObject::destroyed, [](auto)
	{
		CEditTool* pEditTool = GetIEditor()->GetLevelEditorSharedState()->GetEditTool();
		CObjectCreateTool* objectCreateTool = pEditTool ? DYNAMIC_DOWNCAST(CObjectCreateTool, pEditTool) : nullptr;

		if (objectCreateTool)
		{
		  objectCreateTool->Abort();
		}
	});

	//pass drag info to current edit tool
	return GetIEditor()->GetLevelEditorSharedState()->GetEditTool()->OnDragEvent(this, eventId, event, flags);
}

bool CLevelEditorViewport::DropHasAsset(const CDragDropData& dragDropData)
{
	return Private_AssetDrag::GetAsset(dragDropData);
}

bool CLevelEditorViewport::ApplyAsset(const CDragDropData& dragDropData, QDropEvent* pDropEvent, EDragEvent eventId)
{
	CPoint point(pDropEvent->pos().x(), pDropEvent->pos().y());
	HitContext hitContext(this);

	CAsset* pAsset = Private_AssetDrag::GetAsset(dragDropData);
	//find if we are hitting an object at "point", if we are check if the asset can be applied
	if (HitTest(point, hitContext) && hitContext.object != nullptr && hitContext.object->CanApplyAsset(*pAsset))
	{
		string dragText;
		dragText.Format("Assign %s \"%s\" to \"%s\"", pAsset->GetType()->GetUiTypeName(), pAsset->GetName(), hitContext.object->GetName());
		switch (eventId)
		{
		case eDragMove:
			{
				CDragDropData::ShowDragText(GetViewWidget(), QString(dragText));
			}
			break;
		case eDrop:
			{
				string undoDescription;
				undoDescription.Format("Apply %s \"%s\" to \"%s\"", pAsset->GetType()->GetUiTypeName(), pAsset->GetName(), hitContext.object->GetName());
				CUndo undo(undoDescription.c_str());
				hitContext.object->ApplyAsset(*pAsset, &hitContext);

				//accept action to complete drop operation
				pDropEvent->acceptProposedAction();
			}
			break;
		}

		pDropEvent->accept();

		return true;
	}

	return false;
}

void CLevelEditorViewport::PopulateMenu(CPopupMenuItem& menu)
{
	{
		ICommandManager* pCommandManager = GetIEditor()->GetICommandManager();
		menu.AddCommand("camera.toggle_speed_height_relative");
		pCommandManager->GetAction("camera.toggle_speed_height_relative")->setChecked(s_cameraPreferences.IsSpeedHeightRelativeEnabled());
		menu.AddCommand("camera.toggle_terrain_collisions");
		pCommandManager->GetAction("camera.toggle_terrain_collisions")->setChecked(s_cameraPreferences.IsTerrainCollisionEnabled());
		menu.AddCommand("camera.toggle_object_collisions");
		pCommandManager->GetAction("camera.toggle_object_collisions")->setChecked(s_cameraPreferences.IsObjectCollisionEnabled());
		menu.AddSeparator();
	}

	// Configure Fov menu
	{
		CPopupMenuItem& fovMenu = menu.Add("FOV");
		m_headerWidget->AddFOVMenus(fovMenu, functor(*this, &CLevelEditorViewport::SetFOVDeg), m_headerWidget->GetCustomFOVPresets());
		fovMenu.AddSeparator();
		fovMenu.Add("Custom...", functor(*m_headerWidget, &QViewportHeader::OnMenuFOVCustom));
	}

	// Resolutions menu
	{
		CPopupMenuItem& resMenu = menu.Add("Resolution");
		m_headerWidget->AddResolutionMenus(resMenu, functor(*this, &CRenderViewport::SetResolution), m_headerWidget->GetCustomResolutionPresets());
		resMenu.AddSeparator();
		resMenu.Add("Custom...", functor(*m_headerWidget, &QViewportHeader::OnMenuResolutionCustom));
		resMenu.Add("Clear Custom Resolutions", functor(*m_headerWidget, &QViewportHeader::OnMenuResolutionCustomClear));
	}

	menu.Add("Create Camera from Current View", functor(*this, &CLevelEditorViewport::OnMenuCreateCameraFromCurrentView));

	if (GetCameraObject())
		menu.Add("Select Current Camera", functor(*this, &CLevelEditorViewport::OnMenuSelectCurrentCamera));

	// Add Cameras.
	AddCameraMenuItems(menu);
}

void CLevelEditorViewport::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	CRenderViewport::OnEditorNotifyEvent(event);

	if (event == eNotify_OnEndNewScene)
	{
		CHeightmap* pHmap = GetIEditorImpl()->GetHeightmap();
		float sx = pHmap->GetWidth() * 0.5f;
		float sy = pHmap->GetHeight() * 0.5f;

		Matrix34 viewTM;
		viewTM.SetIdentity();
		// Initial camera will be at middle of the map and 2 meters above the ground.
		viewTM.SetTranslation(Vec3(sx * pHmap->GetUnitSize(), sy * pHmap->GetUnitSize(), pHmap->GetXY(static_cast<uint32>(sx), static_cast<uint32>(sy)) + 2.0f));
		SetViewTM(viewTM);
	}
}

void CLevelEditorViewport::AddCameraMenuItems(CPopupMenuItem& menu)
{
	if (!menu.Empty())
	{
		menu.AddSeparator();
	}

	menu.Add("Default", functor(*this, &CRenderViewport::SetDefaultCamera)).Check(IsDefaultCamera());

	const std::vector<ICameraDelegate*>& cameraDelegates = GetIEditorImpl()->GetViewManager()->GetCameraDelegates();
	const size_t numCameraDelegates = cameraDelegates.size();
	bool bIsDelegateActive = false;
	for (size_t i = 0; i < numCameraDelegates; ++i)
	{
		const ICameraDelegate* pDelegate = cameraDelegates[i];
		const bool bIsActive = (m_pCameraDelegate == pDelegate);
		menu.Add(pDelegate->GetName(), functor(*this, &CRenderViewport::SetCameraDelegate), pDelegate).Check(bIsActive);

		bIsDelegateActive |= bIsActive;
	}

	CPopupMenuItem& customCameraMenu = menu.Add("Camera Entity");

	std::vector<CEntityObject*> cameraObjects;
	static_cast<CObjectManager*>(GetIEditor()->GetObjectManager())->GetCameras(cameraObjects);
	std::sort(cameraObjects.begin(), cameraObjects.end(), SortCameraObjectsByName);

	const size_t numCameras = cameraObjects.size();
	if (numCameras > 0)
	{
		for (int i = 0; i < cameraObjects.size(); ++i)
		{
			customCameraMenu.Add(cameraObjects[i]->GetName(), functor(*this, &CRenderViewport::SetCameraObject), (CBaseObject*)cameraObjects[i])
			.Check(m_cameraObjectId == cameraObjects[i]->GetId());
		}
	}
	else
	{
		customCameraMenu.Add("No Cameras", functor(*this, &CRenderViewport::SetDefaultCamera))
		.Enable(false);
	}

	// If no camera object is selected in the viewport the camera movement is always locked
	if (m_cameraObjectId != CryGUID::Null() || bIsDelegateActive)
	{
		menu.AddSeparator();
		CPopupMenuItem& lockItem = AddCheckbox(menu, "Lock Camera Movement", &m_bLockCameraMovement);

		lockItem.Enable(true);
		lockItem.Check(m_bLockCameraMovement);
	}
}

template<typename T>
struct CVarAutoSetAndRestore
{
	ICVar* pCVar;
	T      oldValue;

	CVarAutoSetAndRestore(const char* cvarName, T newValue)
	{
		pCVar = GetIEditor()->GetSystem()->GetIConsole()->GetCVar(cvarName);
		if (pCVar)
		{
			oldValue = GetCVarValue();
			pCVar->Set(newValue);
		}
	}
	~CVarAutoSetAndRestore()
	{
		if (pCVar)
			pCVar->Set(oldValue);
	}
private:
	T GetCVarValue();
};

template<>
float CVarAutoSetAndRestore<float >::GetCVarValue()
{
	return pCVar->GetFVal();
}

template<>
int CVarAutoSetAndRestore<int >::GetCVarValue()
{
	return pCVar->GetIVal();
}

void CLevelEditorViewport::OnRender(SDisplayContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	float fNearZ = GetIEditor()->GetConsoleVar("cl_DefaultNearPlane");
	float fFarZ = m_Camera.GetFarPlane();

	int w = m_currentResolution.width;
	int h = m_currentResolution.height;

	CBaseObject* cameraObject = GetCameraObject();
	if (cameraObject)
	{
		m_viewTM = cameraObject->GetWorldTM();

		if (cameraObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			m_Camera.m_bOmniCamera = false;

			if (cameraObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
			{
				CCameraObject* camObj = (CCameraObject*)cameraObject;
				fNearZ = camObj->GetNearZ();
				fFarZ = camObj->GetFarZ();
				m_camFOV = camObj->GetFOV();
				m_Camera.m_bOmniCamera = camObj->GetIsOmniCamera();
			}

			if (IEntity* pCameraEntity = ((CEntityObject*)cameraObject)->GetIEntity())
			{
				if (Cry::DefaultComponents::CCameraComponent* pCameraComponent = pCameraEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>())
				{
					m_viewTM = pCameraComponent->GetWorldTransformMatrix();
					m_camFOV = pCameraComponent->GetFieldOfView().ToRadians();
					fNearZ = pCameraComponent->GetNearPlane();
					fFarZ = pCameraComponent->GetFarPlane();

					m_Camera.m_bOmniCamera = pCameraComponent->GetType() == Cry::DefaultComponents::ECameraType::Omnidirectional;
				}
				else
				{
					m_viewTM = pCameraEntity->GetWorldTM();
				}
			}
		}
		m_viewTM.OrthonormalizeFast();

		m_Camera.SetMatrix(m_viewTM);
		m_Camera.SetFrustum(w, h, m_camFOV, fNearZ, fFarZ);
	}
	else
	{
		// Normal camera.
		float fov = gViewportPreferences.defaultFOV;

		// match viewport fov to default / selected title menu fov
		if (GetFOV() != fov)
		{
			SetFOV(fov);
		}

		// Just for editor: Aspect ratio fix when changing the viewport
		if (!GetIEditor()->IsInGameMode())
		{
			float viewportAspectRatio = float(w) / h;
			float targetAspectRatio = GetAspectRatio();
			if (targetAspectRatio > viewportAspectRatio)
			{
				// Correct for vertical FOV change.
				float maxTargetHeight = float(w) / targetAspectRatio;
				fov = 2 * atanf((h * tan(fov / 2)) / maxTargetHeight);
			}
		}

		m_Camera.SetFrustum(w, h, fov, fNearZ, gEnv->p3DEngine->GetMaxViewDistance());
		m_Camera.m_bOmniCamera = false;
	}

	const int32 renderFlags = (GetType() == ET_ViewportCamera) ? 0 : SHDF_SECONDARY_VIEWPORT;

	if (m_Camera.m_bOmniCamera)
	{
		CCamera tmpCamera = m_Camera;
		const int cubeRes = 512;

		Matrix34 cubeFaceOrientation[6] = {
			Matrix34::CreateRotationZ(DEG2RAD(-90)),
			Matrix34::CreateRotationZ(DEG2RAD(90)),
			Matrix34::CreateRotationX(DEG2RAD(90)),
			Matrix34::CreateRotationX(DEG2RAD(-90)),
			Matrix34::CreateIdentity(),
			Matrix34::CreateRotationZ(DEG2RAD(180)) };

		CVarAutoSetAndRestore<int> intCvars[] = {
			CVarAutoSetAndRestore<int>("r_motionblur",               0),
			CVarAutoSetAndRestore<int>("r_HDRVignetting",            0),
			CVarAutoSetAndRestore<int>("r_AntialiasingMode",         0),
			CVarAutoSetAndRestore<int>("e_clouds",                   0),
			CVarAutoSetAndRestore<int>("r_sunshafts",                0),
			CVarAutoSetAndRestore<int>("e_CoverageBuffer",           0),
			CVarAutoSetAndRestore<int>("e_StatObjBufferRenderTasks", 0) };
		CVarAutoSetAndRestore<float> floatCvars[] = {
			CVarAutoSetAndRestore<float>("e_LodRatio",                1000.0f),
			CVarAutoSetAndRestore<float>("e_ViewDistRatio",           10000.0f),
			CVarAutoSetAndRestore<float>("e_ViewDistRatioVegetation", 10000.0f),
			CVarAutoSetAndRestore<float>("t_scale",                   0.0f) };

		m_engine->Tick();
		m_engine->Update();

		m_renderer->EnableSwapBuffers(false);
		for (int i = 0; i < 6; i++)
		{
			tmpCamera.m_curCubeFace = i;
			tmpCamera.SetMatrix(m_Camera.GetMatrix() * cubeFaceOrientation[i]);
			tmpCamera.SetFrustum(cubeRes, cubeRes, DEG2RAD(90), tmpCamera.GetNearPlane(), tmpCamera.GetFarPlane(), 1.0f);

			// Display Context handle forces rendering of the world go to the current viewport output window.

			SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_graphicsPipelineKey, tmpCamera, SRenderingPassInfo::DEFAULT_FLAGS, false, this->m_displayContextKey);
			RenderAll(CObjectRenderHelper{ context, passInfo });
			m_engine->RenderWorld(renderFlags | m_graphicsPipelineDesc.shaderFlags, passInfo, __FUNCTION__);
		}
		m_renderer->EnableSwapBuffers(true);
	}
	else
	{
		GetIEditor()->GetSystem()->SetViewCamera(m_Camera);

		m_engine->Tick();
		m_engine->Update();

		// Display Context handle forces rendering of the world go to the current viewport output window.

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_graphicsPipelineKey, m_Camera, SRenderingPassInfo::DEFAULT_FLAGS, false, this->m_displayContextKey);
		RenderAll(CObjectRenderHelper{ context, passInfo });
		m_engine->RenderWorld(renderFlags | m_graphicsPipelineDesc.shaderFlags, passInfo, __FUNCTION__);
	}

	if (!m_renderer->IsStereoEnabled() && !m_Camera.m_bOmniCamera)
	{
		CAIManager* pAiMan = GetIEditorImpl()->GetAI();
		if (pAiMan)
		{
			pAiMan->NavigationDebugDisplay();
		}

		/*
		   SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_Camera);
		   m_displayContext.SetView(this);

		   m_renderer->EF_StartEf(passInfo);

		   // Setup two infinite lights for helpers drawing.
		   SRenderLight light[2];
		   light[0].m_Flags |= DLF_DIRECTIONAL;
		   light[0].SetPosition(m_Camera.GetPosition());
		   light[0].SetLightColor(ColorF(1, 1, 1, 1));
		   light[0].SetSpecularMult(1.0f);
		   light[0].SetRadius(1000000);
		   m_renderer->EF_ADDDlight(&light[0], passInfo);


		 */

		//m_renderer->EF_EndEf3D(renderFlags | SHDF_NOASYNC | SHDF_STREAM_SYNC | SHDF_ALLOWHDR, -1, -1, passInfo);

		CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();
		if (heightmap)
		{
			heightmap->UpdateModSectors();
		}

		CGameEngine* ge = GetIEditorImpl()->GetGameEngine();

		// Draw Axis arrow in lower left corner.
		if (ge && ge->IsLevelLoaded())
		{
			DrawAxis(context);
		}

		// Draw 2D helpers.
		context.SetState(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn);

		// Display cursor string.
		RenderCursorString();

		if (gViewportPreferences.showSafeFrame)
		{
			UpdateSafeFrame();
			RenderSafeFrame(context);
		}

		RenderSelectionRectangle(context);
	}
}

void CLevelEditorViewport::RenderAll(CObjectRenderHelper& displayInfo)
{
	// Draw physics helper/proxies, if requested
#if !defined(_RELEASE) && !CRY_PLATFORM_DURANGO
	if (m_bRenderStats)
	{
		GetIEditor()->GetSystem()->RenderPhysicsHelpers();
	}
#endif
	SDisplayContext& dc = displayInfo.GetDisplayContextRef();

	dc.SetState(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn);
	GetIEditor()->GetObjectManager()->Display(displayInfo);

	RenderSelectedRegion(dc);
	RenderSnapMarker(dc);
	RenderSnappingGrid(dc);

	IAISystem* aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (aiSystem)
	{
		aiSystem->DebugDraw();
	}

	// Display editing tool
	if (GetEditTool() && m_bMouseInside)
	{
		GetEditTool()->Display(dc);
	}
}

void CLevelEditorViewport::RenderSnappingGrid(SDisplayContext& context)
{
	if (!context.showSnappingGridGuide)
	{
		return;
	}

	// First, Check whether we should draw the grid or not.
	const CSelectionGroup* pSelGroup = GetIEditorImpl()->GetSelection();
	if (pSelGroup == nullptr || pSelGroup->GetCount() != 1)
		return;
	if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() != CLevelEditorSharedState::EditMode::Move
	    && GetIEditor()->GetLevelEditorSharedState()->GetEditMode() != CLevelEditorSharedState::EditMode::Rotate)
		return;

	if (gSnappingPreferences.gridSnappingEnabled() == false && gSnappingPreferences.angleSnappingEnabled() == false)
		return;

	CEditTool* pTool = GetIEditor()->GetLevelEditorSharedState()->GetEditTool();
	if (pTool && !pTool->IsDisplayGrid() && !(GetIEditor()->GetGizmoManager()->GetHighlightedGizmo() && GetIEditor()->GetGizmoManager()->GetHighlightedGizmo()->NeedsSnappingGrid()))
		return;

	int prevState = context.GetState();
	context.DepthWriteOff();

	Vec3 p = pSelGroup->GetObject(0)->GetWorldPos();

	AABB bbox;
	pSelGroup->GetObject(0)->GetBoundBox(bbox);
	float size = 2 * bbox.GetRadius();
	float alphaMax = 1.0f, alphaMin = 0.2f;
	context.SetLineWidth(3);

	if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::Move && gSnappingPreferences.gridSnappingEnabled())
	// Draw the translation grid.
	{
		Vec3 u = m_constructionPlaneAxisX;
		Vec3 v = m_constructionPlaneAxisY;
		float step = gSnappingPreferences.gridScale() * gSnappingPreferences.gridSize();
		const int MIN_STEP_COUNT = 5;
		const int MAX_STEP_COUNT = 300;
		int nSteps = std::min(std::max(pos_directed_rounding(size / step), MIN_STEP_COUNT), MAX_STEP_COUNT);
		size = nSteps * step;
		for (int i = -nSteps; i <= nSteps; ++i)
		{
			// Draw u lines.
			float alphaCur = alphaMax - fabsf(float(i) / float(nSteps)) * (alphaMax - alphaMin);
			context.DrawLine(p + v * (step * i), p + u * size + v * (step * i),
			                 ColorF(0, 0, 0, alphaCur), ColorF(0, 0, 0, alphaMin));
			context.DrawLine(p + v * (step * i), p - u * size + v * (step * i),
			                 ColorF(0, 0, 0, alphaCur), ColorF(0, 0, 0, alphaMin));
			// Draw v lines.
			context.DrawLine(p + u * (step * i), p + v * size + u * (step * i),
			                 ColorF(0, 0, 0, alphaCur), ColorF(0, 0, 0, alphaMin));
			context.DrawLine(p + u * (step * i), p - v * size + u * (step * i),
			                 ColorF(0, 0, 0, alphaCur), ColorF(0, 0, 0, alphaMin));
		}
	}
	else if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::Rotate && gSnappingPreferences.angleSnappingEnabled())
	// Draw the rotation grid.
	{
		CLevelEditorSharedState::Axis axisConstraint(GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint());
		if (axisConstraint == CLevelEditorSharedState::Axis::X ||
		    axisConstraint == CLevelEditorSharedState::Axis::Y ||
		    axisConstraint == CLevelEditorSharedState::Axis::Z)
		{
			Vec3 xAxis(1, 0, 0);
			Vec3 yAxis(0, 1, 0);
			Vec3 zAxis(0, 0, 1);
			Vec3 rotAxis;
			if (axisConstraint == CLevelEditorSharedState::Axis::X)
				rotAxis = m_snappingMatrix.TransformVector(xAxis);
			else if (axisConstraint == CLevelEditorSharedState::Axis::Y)
				rotAxis = m_snappingMatrix.TransformVector(yAxis);
			else if (axisConstraint == CLevelEditorSharedState::Axis::Z)
				rotAxis = m_snappingMatrix.TransformVector(zAxis);
			Vec3 anotherAxis = m_constructionPlane.n * size;
			float step = gSnappingPreferences.angleSnap();
			int nSteps = pos_directed_rounding(180.0f / step);
			for (int i = 0; i < nSteps; ++i)
			{
				AngleAxis rot(i* step* gf_PI / 180.0, rotAxis);
				Vec3 dir = rot * anotherAxis;
				context.DrawLine(p, p + dir,
				                 ColorF(0, 0, 0, alphaMax), ColorF(0, 0, 0, alphaMin));
				context.DrawLine(p, p - dir,
				                 ColorF(0, 0, 0, alphaMax), ColorF(0, 0, 0, alphaMin));
			}
		}
	}
	context.SetState(prevState);
}

void CLevelEditorViewport::OnCameraSpeedChanged()
{
	if (m_headerWidget)
		m_headerWidget->cameraSpeedChanged(m_moveSpeed, m_moveSpeedIncrements);
}

void CLevelEditorViewport::OnMenuCreateCameraFromCurrentView()
{
	IObjectManager* pObjMgr = GetIEditor()->GetObjectManager();

	// Create new camera
	GetIEditor()->GetIUndoManager()->Begin();

	CryGUID cameraComponentGUID = Schematyc::GetTypeGUID<Cry::DefaultComponents::CCameraComponent>();

	if (CEntityObject* pNewCameraObject = static_cast<CEntityObject*>(pObjMgr->NewObject("EntityWithComponent", nullptr, cameraComponentGUID.ToString())))
	{
		// If new camera was successfully created copy parameters from old camera
		pNewCameraObject->SetWorldTM(m_Camera.GetMatrix());

		auto* pCameraComponent = pNewCameraObject->GetIEntity()->GetComponent<Cry::DefaultComponents::CCameraComponent>();
		CRY_ASSERT(pCameraComponent != nullptr);
		if (pCameraComponent != nullptr)
		{
			pCameraComponent->SetFieldOfView(CryTransform::CAngle::FromRadians(GetFOV()));
		}

		GetIEditor()->GetIUndoManager()->Accept("Create Camera from Current View");
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
}

void CLevelEditorViewport::OnMenuSelectCurrentCamera()
{
	CBaseObject* pCameraObject = GetCameraObject();

	if (pCameraObject && !pCameraObject->IsSelected())
	{
		GetIEditor()->GetIUndoManager()->Begin();
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		pObjectManager->SelectObject(pCameraObject);
		GetIEditor()->GetIUndoManager()->Accept("Select Current Camera");
	}
}

void CLevelEditorViewport::SetFOV(float fov)
{
	if (CEntityObject* pCameraObject = static_cast<CEntityObject*>(GetCameraObject()))
	{
		if (pCameraObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			static_cast<CCameraObject*>(pCameraObject)->SetFOV(fov);
		}
		else if (IEntity* pCameraEntity = pCameraObject->GetIEntity())
		{
			if (Cry::DefaultComponents::CCameraComponent* pCameraComponent = pCameraEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>())
			{
				pCameraComponent->SetFieldOfView(CryTransform::CAngle::FromRadians(fov));
			}
		}
	}

	m_camFOV = fov;
}

void CLevelEditorViewport::SetFOVDeg(float fov)
{
	float fradVaue = DEG2RAD(fov);
	SetFOV(fradVaue);

	// if viewport camera is active, make selected fov new default
	if (GetIEditor()->GetViewportManager()->GetCameraObjectId() == CryGUID::Null())
		gViewportPreferences.defaultFOV = fradVaue;
}

float CLevelEditorViewport::GetFOV() const
{
	if (CEntityObject* pCameraObject = static_cast<CEntityObject*>(GetCameraObject()))
	{
		if (pCameraObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			return static_cast<CCameraObject*>(pCameraObject)->GetFOV();
		}
		else if (IEntity* pCameraEntity = pCameraObject->GetIEntity())
		{
			if (Cry::DefaultComponents::CCameraComponent* pCameraComponent = pCameraEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>())
			{
				return pCameraComponent->GetFieldOfView().ToRadians();
			}
		}
	}

	return m_camFOV;
}

void CLevelEditorViewport::SetSelectedCamera()
{
	if (CBaseObject* pObject = GetIEditor()->GetSelectedObject())
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			if (pObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
			{
				SetCameraObject(static_cast<CEntityObject*>(pObject));
			}
			else if (IEntity* pCameraEntity = static_cast<CEntityObject*>(pObject)->GetIEntity())
			{
				if (Cry::DefaultComponents::CCameraComponent* pCameraComponent = pCameraEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>())
				{
					SetCameraObject(static_cast<CEntityObject*>(pObject));
				}
			}
		}
	}
}

bool CLevelEditorViewport::IsSelectedCamera() const
{
	CBaseObject* pCameraObject = GetCameraObject();
	if (pCameraObject && pCameraObject == GetIEditor()->GetSelectedObject())
		return true;

	return false;
}

void CLevelEditorViewport::CycleCamera()
{
	if (GetCameraObject() == nullptr)    // The default camera is active ATM.
	{
		std::vector<CEntityObject*> objects;
		((CObjectManager*)GetIEditor()->GetObjectManager())->GetCameras(objects);
		if (objects.size() > 0) // If there are some camera objects,
		{
			// Set the first one as active.
			std::sort(objects.begin(), objects.end(), SortCameraObjectsByName);
			SetCameraObject(objects[0]);
		}
	}
	else                                  // One of the custom cameras is active ATM.
	{
		std::vector<CEntityObject*> objects;
		((CObjectManager*)GetIEditor()->GetObjectManager())->GetCameras(objects);
		assert(objects.size() > 0);
		std::sort(objects.begin(), objects.end(), SortCameraObjectsByName);
		// Get the index of the current one.
		int index = -1;
		for (int i = 0; i < objects.size(); ++i)
		{
			if (objects[i] == GetCameraObject())
				index = i;
		}
		assert(0 <= index && index < objects.size());
		if (index + 1 < objects.size())   // If there is a next one, switch to it.
			SetCameraObject(objects[index + 1]);
		else                              // Otherwise, back to the default.
			SetDefaultCamera();
	}
}

void CLevelEditorViewport::CenterOnSelection()
{
	if (!GetIEditorImpl()->GetSelection()->IsEmpty())
	{
		// Get selection bounds
		const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
		CenterOnAABB(&sel->GetBounds());
	}
}

void CLevelEditorViewport::CenterOnAABB(AABB* const aabb)
{
	CViewport* pViewport = GetIEditor()->GetViewManager()->GetGameViewport();

	if (pViewport && aabb)
	{
		Vec3 selectionCenter = aabb->GetCenter();

		// Minimum center size is 10cm
		const float selectionSize = std::max(0.1f, aabb->GetRadius());

		// Move camera 25% further back than required
		const float centerScale = 1.25f;

		// Decompose original transform matrix
		const Matrix34& originalTM = pViewport->GetViewTM();
		AffineParts affineParts;
		affineParts.SpectralDecompose(originalTM);

		// Forward vector is y component of rotation matrix
		Matrix33 rotationMatrix(affineParts.rot);
		const Vec3 viewDirection = rotationMatrix.GetColumn1().GetNormalized();

		// Compute adjustment required by FOV != 90?
		const float fov = GetFOV();
		const float fovScale = (1.0f / tan(fov * 0.5f));

		// Compute new transform matrix
		const float distanceToTarget = selectionSize * fovScale * centerScale;
		const Vec3 newPosition = selectionCenter - (viewDirection * distanceToTarget);
		Matrix34 newTM = Matrix34(rotationMatrix, newPosition);

		// Set new orbit distance
		pViewport->SetViewTM(newTM);
	}
}

Vec3 CLevelEditorViewport::ViewToWorld(POINT vp, bool* collideWithTerrain, bool onlyTerrain, bool bSkipVegetation, bool bTestRenderMesh) const
{
	if (!m_renderer)
		return Vec3(0, 0, 0);

	CRect rcClient;
	GetClientRect(&rcClient);
	CRect rc = rcClient;

	vp.x = vp.x * m_currentResolution.width / (float)rc.Width();
	vp.y = vp.y * m_currentResolution.height / (float)rc.Height();

	Vec3 pos0;
	if (!m_Camera.Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 0), pos0))
		return Vec3(0, 0, 0);
	if (!IsVectorInValidRange(pos0))
		pos0.Set(0, 0, 0);

	Vec3 pos1;
	if (!m_Camera.Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 1), pos1))
		return Vec3(0, 0, 0);
	if (!IsVectorInValidRange(pos1))
		pos1.Set(1, 0, 0);

	Vec3 v = (pos1 - pos0);
	v = v.GetNormalized();
	v = v * 10000.0f;

	if (!_finite(v.x) || !_finite(v.y) || !_finite(v.z))
		return Vec3(0, 0, 0);

	Vec3 colp(0, 0, 0);

	IPhysicalWorld* world = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	if (!world)
		return colp;

	Vec3 vPos(pos0.x, pos0.y, pos0.z);
	Vec3 vDir(v.x, v.y, v.z);
	int flags = rwi_stop_at_pierceable | rwi_ignore_terrain_holes;
	ray_hit hit;

	const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
	m_numSkipEnts = 0;
	for (int i = 0; i < sel->GetCount() && m_numSkipEnts < 32; i++)
	{
		m_pSkipEnts[m_numSkipEnts++] = sel->GetObject(i)->GetCollisionEntity();
	}

	int col = 0;
	const int queryFlags = (onlyTerrain || gSnappingPreferences.IsSnapToTerrainEnabled()) ? ent_terrain : ent_all;
	for (int chcnt = 0; chcnt < 3; chcnt++)
	{
		hit.pCollider = 0;
		col = world->RayWorldIntersection(vPos, vDir, queryFlags, flags, &hit, 1, m_pSkipEnts, m_numSkipEnts);
		if (col == 0)
			break; // No collision.

		if (hit.bTerrain)
			break;

		IRenderNode* pVegNode = 0;
		if (bSkipVegetation && hit.pCollider &&
		    hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC &&
		    (pVegNode = (IRenderNode*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC)) &&
		    pVegNode->GetRenderNodeType() == eERType_Vegetation)
		{
			// skip vegetation
		}
		//else
		//if (onlyTerrain || GetIEditor()->IsTerrainAxisIgnoreObjects())
		//{
		//	if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_STATIC) // If voxel.
		//	{
		//		IRenderNode * pNode = (IRenderNode *) hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		//		if(!pNode)
		//			break;
		//	}
		//}
		else
		{
			if (bTestRenderMesh)
			{
				Vec3 outNormal(0.f, 0.f, 0.f), outPos(0.f, 0.f, 0.f);
				if (AdjustObjectPosition(hit, outNormal, outPos))
				{
					hit.pt = outPos;
				}
			}
			break;
		}
		if (m_numSkipEnts > 64)
			break;
		m_pSkipEnts[m_numSkipEnts++] = hit.pCollider;

		if (!hit.pt.IsZero()) // Advance ray.
			vPos = hit.pt;
	}

	if (collideWithTerrain)
		*collideWithTerrain = hit.bTerrain;

	if (col && hit.dist > 0)
	{
		colp = hit.pt;
		if (hit.bTerrain)
		{
			colp.z = m_engine->GetTerrainElevation(colp.x, colp.y);
		}
	}
	else if (!col)
	{
		// if there was no collision, try intersecting with the ocean plane or, if ocean is missing, the default terrain height
		float planeLevel = m_engine->GetWaterLevel();
		if (planeLevel <= WATER_LEVEL_UNKNOWN)
		{
			planeLevel = CHeightmap::kInitHeight;
		}

		Ray ray(vPos, vDir);
		Plane p(Vec3(0.0f, 0.0f, -1.0f), planeLevel);

		if (!Intersect::Ray_Plane(ray, p, colp, false))
		{
			return Vec3(0, 0, 0);
		}
	}

	return colp;
}

Vec3 CLevelEditorViewport::ViewToWorldNormal(POINT vp, bool onlyTerrain, bool bTestRenderMesh)
{
	if (!m_renderer)
		return Vec3(0, 0, 1);

	SScopedCurrentContext context(this);

	CRect rcClient;
	GetClientRect(&rcClient);
	CRect rc = rcClient;

	vp.x = vp.x * m_currentResolution.width / (float)rc.Width();
	vp.y = vp.y * m_currentResolution.height / (float)rc.Height();

	Vec3 pos0(0, 0, 0), pos1(0, 0, 0);
	if (!GetCamera().Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 0), pos0))
		return Vec3(0, 0, 1);
	if (!GetCamera().Unproject(Vec3(vp.x, m_currentResolution.height - vp.y, 1), pos1))
		return Vec3(0, 0, 1);

	if (!IsVectorInValidRange(pos0))
		pos0 = Vec3(1, 0, 0);
	if (!IsVectorInValidRange(pos1))
		pos1 = Vec3(1, 0, 0);

	Vec3 v = (pos1 - pos0);
	v = v.GetNormalized();
	v = v * 2000.0f;

	if (!_finite(v.x) || !_finite(v.y) || !_finite(v.z))
		return Vec3(0, 0, 1);

	Vec3 colp(0, 0, 0);

	IPhysicalWorld* world = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	if (!world)
		return colp;

	Vec3 vPos(pos0.x, pos0.y, pos0.z);
	Vec3 vDir(v.x, v.y, v.z);
	int flags = rwi_stop_at_pierceable | rwi_ignore_terrain_holes;
	ray_hit hit;

	const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
	m_numSkipEnts = 0;
	for (int i = 0; i < sel->GetCount(); i++)
	{
		m_pSkipEnts[m_numSkipEnts++] = sel->GetObject(i)->GetCollisionEntity();
		if (m_numSkipEnts > 1023)
			break;
	}

	int col = 1;
	const int queryFlags = (onlyTerrain || gSnappingPreferences.IsSnapToGeometryEnabled()) ? ent_terrain : ent_terrain | ent_static;
	while (col)
	{
		hit.pCollider = 0;
		col = world->RayWorldIntersection(vPos, vDir, queryFlags, flags, &hit, 1, m_pSkipEnts, m_numSkipEnts);
		if (hit.dist > 0)
		{
			//if( onlyTerrain || GetIEditor()->IsTerrainAxisIgnoreObjects())
			//{
			//	if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_STATIC) // If voxel.
			//	{
			//		//IRenderNode * pNode = (IRenderNode *) hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			//		//if(pNode && pNode->GetRenderNodeType() == eERType_VoxelObject)
			//		//	break;
			//	}
			//}
			//else
			//{
			if (bTestRenderMesh)
			{
				Vec3 outNormal(0.f, 0.f, 0.f), outPos(0.f, 0.f, 0.f);
				if (AdjustObjectPosition(hit, outNormal, outPos))
				{
					hit.n = outNormal;
				}
			}
			break;
			//}
			//m_pSkipEnts[m_numSkipEnts++] = hit.pCollider;
		}
		//if(m_numSkipEnts>1023)
		//	break;
	}
	return hit.n;
}

bool CLevelEditorViewport::MouseCallback(EMouseEvent event, CPoint& point, int flags)
{
	// If there's no level loaded then don't handle any mouse events on the level editor viewport
	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
	{
		return true;
	}

	if (!CRenderViewport::MouseCallback(event, point, flags))
	{
		if (event == eMouseMDown)
		{
			if (flags & MK_ALT)
			{
				const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();

				// only valid if we had a last point to orbit from or if we have a selection
				if (m_hadLastOrbitTarget || sel->GetCount() > 0)
				{
					m_orbitInitViewMatrix = GetViewTM();
					m_bInOrbitMode = true;
					m_totalMouseMove.x = m_totalMouseMove.y = 0;
					m_hadLastOrbitTarget = true;

					if (sel->GetCount() > 0)
					{
						AABB bbox = sel->GetBounds();
						Vec3 target = bbox.GetCenter();
						m_orbitTarget = target;
					}
					HideCursor();
				}
			}
			else
			{
				m_bInMoveMode = true;
				HideCursor();
			}

			m_mousePos = point;
		}

		//TODO : make Ruler an edit tool instead of its own thing
		CRuler* pRuler = GetIEditor()->GetRuler();
		if (pRuler)
		{
			if (pRuler->MouseCallback(this, event, point, flags))
				return true;
		}

		return false;
	}

	return true;
}

namespace Private_LevelEditorCommands
{
void SetDefaultCamera()
{
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
	if (pViewport && pViewport->IsRenderViewport() && pViewport->GetType() == ET_ViewportCamera)
	{
		static_cast<CLevelEditorViewport*>(pViewport)->SetDefaultCamera();
	}
}

void SetSelectedCamera()
{
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
	if (pViewport && pViewport->IsRenderViewport() && pViewport->GetType() == ET_ViewportCamera)
	{
		static_cast<CLevelEditorViewport*>(pViewport)->SetSelectedCamera();
	}
}

void CycleCurrentCamera()
{
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
	if (pViewport && pViewport->IsRenderViewport() && pViewport->GetType() == ET_ViewportCamera)
	{
		static_cast<CLevelEditorViewport*>(pViewport)->CycleCamera();
	}
}

void ToggleWireFrameMode()
{
	int nWireframe(R_SOLID_MODE);
	ICVar* r_wireframe(gEnv->pConsole->GetCVar("r_wireframe"));

	if (r_wireframe)
	{
		nWireframe = r_wireframe->GetIVal();
	}

	if (nWireframe != R_WIREFRAME_MODE)
	{
		nWireframe = R_WIREFRAME_MODE;
	}
	else
	{
		nWireframe = R_SOLID_MODE;
	}

	if (r_wireframe)
	{
		r_wireframe->Set(nWireframe);
	}
}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelEditorCommands::SetDefaultCamera, viewport, make_default_camera_current,
                                   CCommandDescription("Set current camera to be the default camera"))
REGISTER_EDITOR_UI_COMMAND_DESC(viewport, make_default_camera_current, "Default Camera", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionCamera_Default, viewport, make_default_camera_current)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelEditorCommands::SetSelectedCamera, viewport, make_selected_camera_current,
                                   CCommandDescription("Set selected camera to be the default camera"))
REGISTER_EDITOR_UI_COMMAND_DESC(viewport, make_selected_camera_current, "Selected Camera", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionCamera_Selected_Object, viewport, make_selected_camera_current)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelEditorCommands::SetSelectedCamera, viewport, cycle_current_camera,
                                   CCommandDescription("Cycle current active camera"))
REGISTER_EDITOR_UI_COMMAND_DESC(viewport, cycle_current_camera, "Cycle Camera", "Ctrl+'", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionCamera_Cycle, viewport, cycle_current_camera)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_LevelEditorCommands::ToggleWireFrameMode, viewport, toggle_wireframe_mode,
                                   CCommandDescription("Toggles between solid and wireframe mode in the viewport"))
REGISTER_EDITOR_UI_COMMAND_DESC(viewport, toggle_wireframe_mode, "Wireframe/Solid Mode", "Alt+W", "icons:Viewport/Modes/display_wireframe.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionWireframe, viewport, toggle_wireframe_mode)
