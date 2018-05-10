// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Settings.h"
#include "RenderViewport.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include "DesignerEditor.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/DesignerObject.h"
#include "Objects/AreaSolidObject.h"
#include "Mission.h"
#include "CryEditDoc.h"
#include "Objects/EnvironmentProbeObject.h"
#include "Objects/ObjectLoader.h"
#include <CrySerialization/Decorators/Range.h>

namespace Designer
{
class UndoExclusiveMode : public IUndoObject
{
public:
	UndoExclusiveMode(const char* undoDescription = NULL)
	{
	}

	int         GetSize()        { return sizeof(*this); }
	const char* GetDescription() { return "Undo for Designer Exclusive Mode"; }

	void        Undo(bool bUndo = true) override
	{
	}

	void Redo() override
	{
	}
};

DesignerExclusiveMode gExclusiveModeSettings;
DesignerSettings gDesignerSettings;

DesignerEditor* GetDesignerEditTool()
{
	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (pEditor && pEditor->IsKindOf(RUNTIME_CLASS(DesignerEditor)))
		return (DesignerEditor*)pEditor;
	return NULL;
}

CRenderViewport* GetRenderViewport()
{
	CViewport* pViewport = GetIEditor()->GetActiveView();
	if (pViewport->IsRenderViewport())
		return (CRenderViewport*)pViewport;
	return NULL;
}

void DesignerExclusiveMode::CenterCameraForExclusiveMode()
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	CRenderViewport* pView = GetRenderViewport();
	if (pSelection->GetCount() > 0 && pView)
	{
		AABB selBoundBox = pSelection->GetBounds();
		Vec3 vTopCenterPos = pSelection->GetCenter();
		vTopCenterPos.z = selBoundBox.max.z;
		vTopCenterPos -= pView->GetCamera().GetViewdir() * (selBoundBox.GetRadius() + 10.0f);
		Matrix34 cameraTM = pView->GetViewTM();
		cameraTM.SetTranslation(vTopCenterPos);
		pView->SetViewTM(cameraTM);
	}
}

void DesignerExclusiveMode::EnableExclusiveMode(bool bEnable)
{
	if (m_bExclusiveMode == bEnable)
		return;

	if (bEnable)
	{
		const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();
		if (pGroup->GetCount() == 0)
			return;
		for (int i = 0, iObjectCount(pGroup->GetCount()); i < iObjectCount; ++i)
		{
			CBaseObject* pObject = pGroup->GetObject(i);
			if (!pObject->IsKindOf(RUNTIME_CLASS(DesignerObject)))
				return;
		}
	}

	GetIEditor()->GetIUndoManager()->Suspend();

	m_bExclusiveMode = bEnable;

	CRenderViewport* pView = GetRenderViewport();
	if (pView == NULL)
	{
		GetIEditor()->GetIUndoManager()->Resume();
		return;
	}

	if (m_bExclusiveMode)
	{
		CBaseObject* pCameraObj = GetIEditor()->GetObjectManager()->FindObject(ExcludeModeCameraID);
		if (pCameraObj)
			GetIEditor()->GetObjectManager()->DeleteObject(pCameraObj);

		char workingDirectory[_MAX_PATH] = { 0, };
		GetCurrentDirectory(_MAX_PATH, workingDirectory);
		string designerCameraPath = string(workingDirectory) + "\\Editor\\Objects\\ExcludeModeCamera.grp";

		XmlNodeRef cameraNode = XmlHelpers::LoadXmlFromFile(designerCameraPath);
		if (cameraNode)
		{
			CObjectArchive cameraArchive(GetIEditor()->GetObjectManager(), cameraNode, true);
			GetIEditor()->GetObjectManager()->LoadObjects(cameraArchive, false);
			pCameraObj = GetIEditor()->GetObjectManager()->FindObject(ExcludeModeCameraID);
		}

		if (!pCameraObj)
		{
			GetIEditor()->GetIUndoManager()->Resume();
			return;
		}

		pCameraObj->SetFlags(OBJFLAG_HIDE_HELPERS);
		DynArray<_smart_ptr<CBaseObject>> allLightsUnderCamera;
		pCameraObj->GetAllChildren(allLightsUnderCamera);
		for (int i = 0, iCount(allLightsUnderCamera.size()); i < iCount; ++i)
			allLightsUnderCamera[i]->SetFlags(OBJFLAG_HIDE_HELPERS);

		m_bOldLockCameraMovement = pView->IsCameraMovementLocked();
		pView->LockCameraMovement(false);

		m_OldCameraTM = pView->GetViewTM();
		pView->SetCameraObject(pCameraObj);
		pView->SetViewTM(m_OldCameraTM);

		m_OldObjectHideMask = gViewportDebugPreferences.GetObjectHideMask();
		gViewportDebugPreferences.SetObjectHideMask(0);

		SetCVForExclusiveMode();
		SetTimeOfDayForExclusiveMode();
		SetObjectsFlagForExclusiveMode();
		CenterCameraForExclusiveMode();
	}
	else if (pView->GetCameraObject() && pView->GetCameraObject()->GetId() == ExcludeModeCameraID)
	{
		Matrix34 cameraTM = pView->GetCamera().GetMatrix();

		pView->SetCameraObject(NULL);
		pView->LockCameraMovement(m_bOldLockCameraMovement);

		_smart_ptr<CBaseObject> pCameraObj = GetIEditor()->GetObjectManager()->FindObject(ExcludeModeCameraID);
		if (pCameraObj)
		{
			DynArray<_smart_ptr<CBaseObject>> allLightsUnderCamera;
			pCameraObj->GetAllChildren(allLightsUnderCamera);
			for (int i = 0, iCount(allLightsUnderCamera.size()); i < iCount; ++i)
				GetIEditor()->GetObjectManager()->DeleteObject(allLightsUnderCamera[i]);
			GetIEditor()->GetObjectManager()->DeleteObject(pCameraObj);
			pView->SetViewTM(m_OldCameraTM);
		}

		gViewportDebugPreferences.SetObjectHideMask(m_OldObjectHideMask);
		m_OldObjectHideMask = 0;

		RestoreCV();
		RestoreTimeOfDay();
		RestoreObjectsFlag();
	}

	GetIEditor()->GetIUndoManager()->Resume();
}

void DesignerExclusiveMode::SetCVForExclusiveMode()
{
	IConsole* pConsole = GetIEditor()->GetSystem()->GetIConsole();
	if (!pConsole)
		return;

	ICVar* pDisplayInfo = pConsole->GetCVar("r_DisplayInfo");
	if (pDisplayInfo)
	{
		m_OldConsoleVars.r_DisplayInfo = pDisplayInfo->GetIVal();
		pDisplayInfo->Set(0);
	}

	//  ICVar* pHDRRendering = pConsole->GetCVar("r_HDRRendering");
	//  if( pHDRRendering )
	//  {
	//    m_OldConsoleVars.r_HDRRendering = pHDRRendering->GetIVal();
	//    pHDRRendering->Set(0);
	//  }

	ICVar* pSSDO = pConsole->GetCVar("r_ssdo");
	if (pSSDO)
	{
		m_OldConsoleVars.r_ssdo = pSSDO->GetIVal();
		pSSDO->Set(0);
	}

	ICVar* pPostProcessEffect = pConsole->GetCVar("r_PostProcessEffects");
	if (pPostProcessEffect)
	{
		m_OldConsoleVars.r_PostProcessEffects = pPostProcessEffect->GetIVal();
		pPostProcessEffect->Set(0);
	}

	ICVar* pRenderVegetation = pConsole->GetCVar("e_Vegetation");
	if (pRenderVegetation)
	{
		m_OldConsoleVars.e_Vegetation = pRenderVegetation->GetIVal();
		pRenderVegetation->Set(0);
	}

	ICVar* pWaterOcean = pConsole->GetCVar("e_WaterOcean");
	if (pWaterOcean)
	{
		m_OldConsoleVars.e_WaterOcean = pWaterOcean->GetIVal();
		pWaterOcean->Set(0);
	}

	ICVar* pWaterVolume = pConsole->GetCVar("e_WaterVolumes");
	if (pWaterVolume)
	{
		m_OldConsoleVars.e_WaterVolumes = pWaterVolume->GetIVal();
		pWaterVolume->Set(0);
	}

	ICVar* pTerrain = pConsole->GetCVar("e_Terrain");
	if (pTerrain)
	{
		m_OldConsoleVars.e_Terrain = pTerrain->GetIVal();
		pTerrain->Set(0);
	}

	ICVar* pShadow = pConsole->GetCVar("e_Shadows");
	if (pShadow)
	{
		m_OldConsoleVars.e_Shadows = pShadow->GetIVal();
		pShadow->Set(0);
	}

	ICVar* pParticle = pConsole->GetCVar("e_Particles");
	if (pParticle)
	{
		m_OldConsoleVars.e_Particles = pParticle->GetIVal();
		pParticle->Set(0);
	}

	ICVar* pClouds = pConsole->GetCVar("e_Clouds");
	if (pClouds)
	{
		m_OldConsoleVars.e_Clouds = pClouds->GetIVal();
		pClouds->Set(0);
	}

	ICVar* pSkybox = pConsole->GetCVar("e_SkyBox");
	if (pSkybox)
	{
		m_OldConsoleVars.e_SkyBox = pSkybox->GetIVal();
		pSkybox->Set(1);
	}
}

void DesignerExclusiveMode::SetObjectsFlagForExclusiveMode()
{
	DynArray<CBaseObject*> objects;
	GetIEditor()->GetObjectManager()->GetObjects(objects);

	m_ObjectHiddenFlagMap.clear();

	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();

	for (int i = 0, iObjectCount(objects.size()); i < iObjectCount; ++i)
	{
		m_ObjectHiddenFlagMap[objects[i]] = objects[i]->IsHidden();
		bool bSelected = false;
		for (int k = 0, iSelectionCount(pSelection->GetCount()); k < iSelectionCount; ++k)
		{
			if (pSelection->GetObject(k) == objects[i])
			{
				bSelected = true;
				break;
			}
		}
		if (bSelected)
			continue;
		if (objects[i]->GetId() != ExcludeModeCameraID && (!objects[i]->GetParent() || objects[i]->GetParent()->GetId() != ExcludeModeCameraID))
			objects[i]->SetHidden(true);
	}
}

void DesignerExclusiveMode::SetTime(ITimeOfDay* pTOD, float fTime)
{
	if (pTOD == NULL)
		return;
	pTOD->SetTime(fTime, true);
	GetIEditor()->SetConsoleVar("e_TimeOfDay", fTime);

	pTOD->Update(false, true);
	GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
}

void DesignerExclusiveMode::SetTimeOfDayForExclusiveMode()
{
	char workingDirectory[_MAX_PATH] = { 0, };
	GetCurrentDirectory(_MAX_PATH, workingDirectory);
	string designerTimeOfDay = string(workingDirectory) + "\\Editor\\CryDesigner_TimeOfDay.xml";

	XmlNodeRef root = GetIEditor()->GetSystem()->LoadXmlFromFile(designerTimeOfDay);
	if (root)
	{
		ITimeOfDay* pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();

		m_OldTimeOfDay = GetIEditor()->GetSystem()->CreateXmlNode();
		m_OldTimeOfTOD = GetIEditor()->GetConsoleVar("e_TimeOfDay");
		pTimeOfDay->Serialize(m_OldTimeOfDay, false);

		pTimeOfDay->Serialize(root, true);
		SetTime(pTimeOfDay, 0);
	}
}

void DesignerExclusiveMode::RestoreTimeOfDay()
{
	if (m_OldTimeOfDay)
	{
		ITimeOfDay* pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();
		pTimeOfDay->Serialize(m_OldTimeOfDay, true);
		SetTime(pTimeOfDay, m_OldTimeOfTOD);
		m_OldTimeOfDay = NULL;
	}
}

void DesignerExclusiveMode::RestoreCV()
{
	IConsole* pConsole = GetIEditor()->GetSystem()->GetIConsole();
	if (!pConsole)
		return;

	ICVar* pDisplayInfo = pConsole->GetCVar("r_DisplayInfo");
	if (pDisplayInfo)
		pDisplayInfo->Set(m_OldConsoleVars.r_DisplayInfo);

	//  ICVar* pHDRRendering = pConsole->GetCVar("r_HDRRendering");
	//  if( pHDRRendering )
	//    pHDRRendering->Set(m_OldConsoleVars.r_HDRRendering);

	ICVar* pPostProcessEffect = pConsole->GetCVar("r_PostProcessEffects");
	if (pPostProcessEffect)
		pPostProcessEffect->Set(m_OldConsoleVars.r_PostProcessEffects);

	ICVar* pSSDO = pConsole->GetCVar("r_ssdo");
	if (pSSDO)
		pSSDO->Set(m_OldConsoleVars.r_ssdo);

	ICVar* pRenderVegetation = pConsole->GetCVar("e_Vegetation");
	if (pRenderVegetation)
		pRenderVegetation->Set(m_OldConsoleVars.e_Vegetation);

	ICVar* pWaterOcean = pConsole->GetCVar("e_WaterOcean");
	if (pWaterOcean)
		pWaterOcean->Set(m_OldConsoleVars.e_WaterOcean);

	ICVar* pWaterVolume = pConsole->GetCVar("e_WaterVolumes");
	if (pWaterVolume)
		pWaterVolume->Set(m_OldConsoleVars.e_WaterVolumes);

	ICVar* pTerrain = pConsole->GetCVar("e_Terrain");
	if (pTerrain)
		pTerrain->Set(m_OldConsoleVars.e_Terrain);

	ICVar* pShadow = pConsole->GetCVar("e_Shadows");
	if (pShadow)
		pShadow->Set(m_OldConsoleVars.e_Shadows);

	ICVar* pParticle = pConsole->GetCVar("e_Particles");
	if (pParticle)
		pParticle->Set(m_OldConsoleVars.e_Particles);

	ICVar* pClouds = pConsole->GetCVar("e_Clouds");
	if (pClouds)
		pClouds->Set(m_OldConsoleVars.e_Clouds);

	ICVar* pSkybox = pConsole->GetCVar("e_SkyBox");
	if (pSkybox)
		pSkybox->Set(m_OldConsoleVars.e_SkyBox);
}

void DesignerExclusiveMode::RestoreObjectsFlag()
{
	DynArray<CBaseObject*> objects;
	GetIEditor()->GetObjectManager()->GetObjects(objects);

	for (int i = 0, iObjectCount(objects.size()); i < iObjectCount; ++i)
	{
		if (m_ObjectHiddenFlagMap.find(objects[i]) != m_ObjectHiddenFlagMap.end())
			objects[i]->SetHidden(m_ObjectHiddenFlagMap[objects[i]]);
	}

	GetIEditor()->SetConsoleVar("e_Vegetation", 1.0f);
}

DesignerSettings::DesignerSettings()
{
	bExclusiveMode = false;
	bDisplayBackFaces = false;
	bKeepPivotCentered = false;
	bHighlightElements = true;
	fHighlightBoxSize = 24 * 0.001f;
	bSeamlessEdit = false;
	bDisplayTriangulation = false;
	bDisplayDimensionHelper = true;
	bDisplaySubdividedResult = true;
	bDisplayVertexNormals = false;
	bDisplayPolygonNormals = false;
}

void DesignerSettings::Load()
{
	LoadSettings(Serialization::SStruct(*this), "DesignerSetting");
}

void DesignerSettings::Save()
{
	SaveSettings(Serialization::SStruct(*this), "DesignerSetting");
}

void DesignerSettings::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("designer_properties", "Properties"))
	{
		if (ar.isEdit())
		{
			// Exclusive mode was hack using layers and will probably come back as a standalone editor later, 
			// hide option for now
			// TODO: clean this up completely this later
			//ar(bExclusiveMode, "ExclusiveMode", "Exclusive Mode");
			ar(bDisplayBackFaces, "DisplayBackFaces", "Display Back Facing Polygons");
		}
		ar(bSeamlessEdit, "SeamlessEdit", "Seamless Edit");
		ar(bKeepPivotCentered, "KeepPivotCenter", "Keep Pivot Centered");
		ar(bHighlightElements, "HighlightElements", "Highlight Elements");
		ar(Serialization::Range(fHighlightBoxSize, 0.005f, 2.0f), "HighlightBoxSize", "Highlight Box Size");
		ar(bDisplayDimensionHelper, "DisplayDimensionHelper", "Display Dimension Helper");
		ar(bDisplayTriangulation, "DisplayTriangulation", "Display Triangulation");
		ar(bDisplaySubdividedResult, "DisplaySubdividedResult", "Display Subdivided Result");
		ar(bDisplayVertexNormals, "DisplayVertexNormals", "Display Vertex Normals");
		ar(bDisplayPolygonNormals, "DisplayPolygonNormals", "Display Polygon Normals");

		ar.closeBlock();
	}
}

void DesignerSettings::Update(bool continuous)
{
	gExclusiveModeSettings.EnableExclusiveMode(bExclusiveMode);
	if (bKeepPivotCentered)
		ApplyPostProcess(DesignerSession::GetInstance()->GetMainContext(), ePostProcess_CenterPivot | ePostProcess_Mesh);
	Save();
}
}

