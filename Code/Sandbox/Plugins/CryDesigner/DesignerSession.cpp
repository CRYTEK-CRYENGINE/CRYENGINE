// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerSession.h"

#include "Util/DesignerSettings.h"
#include "Util/Converter.h"
#include "Core/Helper.h"

#include "IEditor.h"
#include "Material/MaterialManager.h"

#include "Core/ModelCompiler.h"
#include "Objects/DesignerObject.h"
#include "Objects/AreaSolidObject.h"
#include "Objects/ClipVolumeObject.h"
#include "Objects/EntityObject.h"
#include "Objects/BrushObject.h"
#include "RecursionLoopGuard.h"

namespace Designer
{

DesignerSession* DesignerSession::s_session = nullptr;

DesignerSession::DesignerSession()
	: m_bActiveSession(false)
	, m_bSessionFinished(false)
{
	GetIEditor()->GetMaterialManager()->AddListener(this);

	// Brush to Designer
	GetIEditor()->GetObjectManager()->RegisterTypeConverter(RUNTIME_CLASS(CBrushObject), "Designer", [](CBaseObject* pObject)
		{
			Converter converter;
			if (!converter.ConvertToDesignerObject())
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "The selected object(s) can't be converted.");
	  });

	// Entity to Designer
	GetIEditor()->GetObjectManager()->RegisterTypeConverter(RUNTIME_CLASS(CEntityObject), "Designer", [](CBaseObject* pObject)
		{
			Converter converter;
			if (!converter.ConvertToDesignerObject())
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "The selected object(s) can't be converted.");
	  });
}

DesignerSession::~DesignerSession()
{
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
}

DesignerSession* DesignerSession::GetInstance()
{
	if (!s_session)
	{
		s_session = new DesignerSession();
	}

	return s_session;
}

Model* DesignerSession::GetModel() const
{
	if (!m_pBaseObject)
		return nullptr;

	Model* pModel;
	if (Designer::GetModel(m_pBaseObject, pModel))
		return pModel;

	DESIGNER_ASSERT(0);
	return nullptr;
}

CBaseObject* DesignerSession::GetBaseObject() const
{
	return m_pBaseObject;
}

MainContext DesignerSession::GetMainContext()
{
	MainContext mc(m_pBaseObject, GetCompiler(), GetModel());
	mc.pSelected = &m_pSelectedElements;
	return mc;
}

ModelCompiler* DesignerSession::GetCompiler() const
{
	DESIGNER_ASSERT(m_pBaseObject);
	if (!m_pBaseObject)
		return nullptr;

	ModelCompiler* pCompiler;
	if (Designer::GetCompiler(m_pBaseObject, pCompiler))
		return pCompiler;

	DESIGNER_ASSERT(0);
	return nullptr;
}

void DesignerSession::SetBaseObject(CBaseObject* pBaseObject)
{
	if (m_pBaseObject == pBaseObject)
		return;
	m_pBaseObject = pBaseObject;
	signalDesignerEvent(eDesignerNotify_ObjectSelect, 0);
}

void DesignerSession::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	bool bExclusiveModeBeforeSave = false;
	ElementSet* pSelected = GetSelectedElements();
	switch (event)
	{
	case eNotify_OnBeginGameMode:
	case eNotify_OnBeginBindViewport:
		gDesignerSettings.bExclusiveMode = false;
		gExclusiveModeSettings.EnableExclusiveMode(gDesignerSettings.bExclusiveMode);
		pSelected->Clear();
		gDesignerSettings.Update(true);
		signalDesignerEvent(eDesignerNotify_SettingsChanged, nullptr);
		break;

	case eNotify_OnBeginLoad:
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneOpen:
		// make sure we clear the cache here
		ClearCache();
	// fallthrough
	case eNotify_OnBeginSceneSave:
		bExclusiveModeBeforeSave = gDesignerSettings.bExclusiveMode;
		if (gDesignerSettings.bExclusiveMode)
		{
			gDesignerSettings.bExclusiveMode = false;
			gExclusiveModeSettings.EnableExclusiveMode(gDesignerSettings.bExclusiveMode);
		}
		break;

	case eNotify_OnEndSceneSave:
		if (bExclusiveModeBeforeSave)
		{
			gDesignerSettings.bExclusiveMode = true;
			gExclusiveModeSettings.EnableExclusiveMode(gDesignerSettings.bExclusiveMode);
			bExclusiveModeBeforeSave = false;
		}
		break;
	case eNotify_OnSelectionChange:
		{
			const CSelectionGroup* sel = GetIEditor()->GetObjectManager()->GetSelection();
			const int count = sel->GetCount();
			bool startSession = count > 0;
			for (int i = 0; i < count; i++)
			{
				CBaseObject* obj = sel->GetObject(i);
				if (!Designer::CanDesignerEditObject(obj))
					startSession = false;
				/*else if(obj->IsKindOf(RUNTIME_CLASS(AreaSolidObject))
				   {
				   //this was on the area solid BeginEditParams(), not sure where to put this now...
				   Designer::GetModel(obj)->SetFlag(GetModel()->GetFlag() | eModelFlag_DisplayBackFace);
				   ApplyPostProcess(MainContext(this, GetCompiler(), GetModel()), ePostProcess_Mesh);
				   }*/
			}

			if (startSession)
				BeginSession();
			else
				EndSession();
		}

	}
}

void DesignerSession::SetCurrentSubMatID(int idx)
{
	//move to session
	m_currentMaterialIntex = idx;
	MaterialChangeEvent evt = { idx };
	signalDesignerEvent(eDesignerNotify_SubMaterialSelectionChanged, &evt);
}

void DesignerSession::ClearCache()
{
	m_pBaseObject = nullptr;
	m_currentMaterialIntex = 0;

	m_pExcludedEdgeManager.Clear();
	m_pSelectedElements.Clear();

	m_pSelectionMesh = nullptr;
}

void DesignerSession::OnDataBaseItemEvent(
  IDataBaseItem* pItem,
  EDataBaseItemEvent event)
{
	DatabaseEvent evt;
	evt.evt = event;
	evt.pItem = pItem;

	signalDesignerEvent(eDesignerNotify_SubMaterialSelectionChanged, &evt);

	auto pObject = GetBaseObject();
	if (pObject)
	{
		if (pObject->GetMaterial() == (IEditorMaterial*)pItem || pObject->GetRenderMaterial() == (IEditorMaterial*)pItem)
			signalDesignerEvent(eDesignerNotify_RefreshSubMaterialList, nullptr);
	}
}

int DesignerSession::GetCurrentSubMatID() const
{
	return m_currentMaterialIntex;
}

ElementSet* DesignerSession::GetSelectedElements()
{
	return &m_pSelectedElements;
}

void DesignerSession::ClearPolygonSelections()
{
	m_pSelectionMesh = nullptr;
}

_smart_ptr<PolygonMesh> DesignerSession::GetSelectionMesh()
{
	return m_pSelectionMesh;
}

void DesignerSession::UpdateSelectionMeshFromSelectedElements(MainContext& mc)
{
	if (!mc.pModel || !mc.pCompiler)
		return;

	std::vector<PolygonPtr> polygonlist;

	ElementSet* pSelectedElement = DesignerSession::GetInstance()->GetSelectedElements();

	for (int k = 0, iElementSize(pSelectedElement->GetCount()); k < iElementSize; ++k)
	{
		if (pSelectedElement->Get(k).IsPolygon())
			polygonlist.push_back(pSelectedElement->Get(k).m_pPolygon);
	}

	if (m_pSelectionMesh == NULL)
		m_pSelectionMesh = new PolygonMesh;

	int renderFlag = mc.pCompiler->GetRenderFlags();
	int viewDist = mc.pCompiler->GetViewDistRatio();
	uint32 minSpec = mc.pObject->GetMinSpec();
	uint32 materialLayerMask = mc.pObject->GetMaterialLayersMask();
	BrushMatrix34 worldTM = mc.pObject->GetWorldTM();

	m_pSelectionMesh->SetPolygons(polygonlist, false, worldTM, renderFlag, viewDist, minSpec, materialLayerMask);
}

void DesignerSession::UpdateSelectionMesh(
  PolygonPtr pPolygon,
  ModelCompiler* pCompiler,
  CBaseObject* pObj,
  bool bForce)
{
	std::vector<PolygonPtr> polygons;
	if (pPolygon)
		polygons.push_back(pPolygon);
	UpdateSelectionMesh(polygons, pCompiler, pObj, bForce);
}

void DesignerSession::UpdateSelectionMesh(
  std::vector<PolygonPtr> polygons,
  ModelCompiler* pCompiler,
  CBaseObject* pObj,
  bool bForce)
{
	if (m_pSelectionMesh == NULL)
		m_pSelectionMesh = new PolygonMesh;

	int renderFlag = pCompiler->GetRenderFlags();
	int viewDist = pCompiler->GetViewDistRatio();
	uint32 minSpec = pObj->GetMinSpec();
	uint32 materialLayerMask = pObj->GetMaterialLayersMask();
	BrushMatrix34 worldTM = pObj->GetWorldTM();

	if (!polygons.empty())
		m_pSelectionMesh->SetPolygons(polygons, bForce, worldTM, renderFlag, viewDist, minSpec, materialLayerMask);
	else
		m_pSelectionMesh->SetPolygon(NULL, bForce, worldTM, renderFlag, viewDist, minSpec, materialLayerMask);
}

void DesignerSession::ReleaseSelectionMesh()
{
	if (m_pSelectionMesh)
	{
		m_pSelectionMesh->ReleaseResources();
		m_pSelectionMesh = nullptr;
	}
}

void DesignerSession::BeginSession()
{
	// This will clear selection caches every time a new object is selected, which
	// is not what we usually want, but will suffice until we rip selection state
	// into the Model itself, rather than the session
	ClearCache();

	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	SetBaseObject(pSelection->GetObject(0));

	if (!m_bActiveSession)
	{
		Model* model;
		model = GetModel();
		model->SetShelf(eShelf_Base);

		if (gDesignerSettings.bExclusiveMode)
		{
			if (model && model->IsEmpty())
			{
				gDesignerSettings.bExclusiveMode = false;
				gDesignerSettings.Update(true);
			}
			else
			{
				gExclusiveModeSettings.EnableExclusiveMode(true);
			}
		}

		signalDesignerEvent(eDesignerNotify_BeginDesignerSession, nullptr);

		m_bActiveSession = true;
	}
}

ExcludedEdgeManager* DesignerSession::GetExcludedEdgeManager()
{
	return &m_pExcludedEdgeManager;
}

std::vector<EDesignerTool> DesignerSession::GetIncompatibleSubtools()
{
	if (!m_pBaseObject)
		return std::vector<EDesignerTool>();

	if (m_pBaseObject->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		DesignerObject* pModelObj = static_cast<DesignerObject*>(m_pBaseObject.get());
		return pModelObj->GetIncompatibleSubtools();
	}
	else if (m_pBaseObject->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
	{
		AreaSolidObject* pArea = static_cast<AreaSolidObject*>(m_pBaseObject.get());
		return pArea->GetIncompatibleSubtools();
	}
	else if (m_pBaseObject->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
	{
		ClipVolumeObject* pVolume = static_cast<ClipVolumeObject*>(m_pBaseObject.get());
		return pVolume->GetIncompatibleSubtools();
	}

	// should never happen but...
	return std::vector<EDesignerTool>();
}

void DesignerSession::EndSession()
{
	RECURSION_GUARD(m_bSessionFinished);
	if (m_bActiveSession)
	{
		// disable at once, this saves us from recursive calls when destroying empty designer objects on tool exit
		m_bActiveSession = false;

		// quit any designer tools if we still have one.
		// usually session ends when we select another object type, so we usually are in
		// object mode tool already but it can also happen when we undo out of the session.
		// In that case, designer won't have data to work with so we need to ensure the tool has quit.
		if (GetDesigner())
		{
			GetIEditor()->SetEditTool(nullptr);
		}

		gExclusiveModeSettings.EnableExclusiveMode(false);
		signalDesignerEvent(eDesignerNotify_EndDesignerSession, nullptr);
		ClearCache();
	}
}

bool DesignerSession::GetIsActive()
{
	return m_bActiveSession;
}

}

