// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BrushObject.h"

#include <Preferences/ViewportPreferences.h>
#include "Viewport.h"

#include "EntityObject.h"
#include "Geometry/EdMesh.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "EditMode/SubObjectSelectionReferenceFrameCalculator.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"

#include <Serialization/Decorators/EditToolButton.h>
#include <Serialization/Decorators/EditorActionButton.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryEntitySystem/IBreakableManager.h>
#include <CrySystem/ICryLink.h>
#include "FilePathUtil.h"

#include <cctype> // std::isdigit

#define MIN_BOUNDS_SIZE 0.01f

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
REGISTER_CLASS_DESC(CBrushObjectClassDesc);

IMPLEMENT_DYNCREATE(CBrushObject, CBaseObject)

//////////////////////////////////////////////////////////////////////////
CBrushObject::CBrushObject()
{
	m_pGeometry = 0;
	m_pRenderNode = 0;

	m_renderFlags = 0;
	m_bNotSharedGeom = false;
	m_bSupportsBoxHighlight = false;
	m_bbox.Reset();

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_geometryFile, "Prefab", "Geometry", functor(*this, &CBrushObject::OnGeometryChange), IVariable::DT_OBJECT);

	UseMaterialLayersMask(true);

	// Init Variables.
	mv_outdoor = false;
	mv_castShadowMaps = true;
	mv_giMode = true;
	mv_rainOccluder = true;
	mv_registerByBBox = false;
	mv_dynamicDistanceShadows = false;
	mv_hideable = 0;
	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_excludeFromTriangulation = false;
	mv_noDynWater = false;
	mv_aiRadius = -1.0f;
	mv_noDecals = false;
	mv_recvWind = false;
	mv_Occluder = false;
	mv_drawLast = false;
	mv_shadowLodBias = 0;

	static string sVarName_OutdoorOnly = "IgnoreVisareas";
	//	static string sVarName_CastShadows = "CastShadows";
	//	static string sVarName_CastShadows2 = _T("CastShadowVolume");
	//static string sVarName_SelfShadowing = "SelfShadowing";
	static string sVarName_CastShadowMaps = "CastShadowMaps";
	static string sVarName_GIMode = "GIMode";
	static string sVarName_RainOccluder = "RainOccluder";
	static string sVarName_RegisterByBBox = "SupportSecondVisarea";
	static string sVarName_DynamicDistanceShadows = "DynamicDistanceShadows";
	static string sVarName_ReceiveLightmap = "ReceiveRAMmap";
	static string sVarName_Hideable = "Hideable";
	static string sVarName_HideableSecondary = "HideableSecondary";
	static string sVarName_LodRatio = "LodRatio";
	static string sVarName_ViewDistRatio = "ViewDistRatio";
	static string sVarName_NotTriangulate = "NotTriangulate";
	static string sVarName_NoDynWater = "NoDynamicWater";
	static string sVarName_AIRadius = "AIRadius";
	static string sVarName_LightmapQuality = "RAMmapQuality";
	static string sVarName_NoDecals = "NoStaticDecals";
	static string sVarName_Frozen = "Frozen";
	static string sVarName_NoAmbShadowCaster = "NoAmnbShadowCaster";
	static string sVarName_RecvWind = "RecvWind";
	static string sVarName_IntegrQuality = "IntegrationQuality";
	static string sVarName_Occluder = "Occluder";
	static string sVarName_DrawLast = "DrawLast";
	static string sVarName_ShadowLodBias = "ShadowLodBias";

	CVarEnumList<int>* pHideModeList = new CVarEnumList<int>;
	pHideModeList->AddItem("None", 0);
	pHideModeList->AddItem("Hideable", 1);
	pHideModeList->AddItem("Secondary", 2);
	mv_hideable.SetEnumList(pHideModeList);

	m_pVarObject->ReserveNumVariables(16);

	// Add Collision filtering variables
	m_collisionFiltering.AddToObject(m_pVarObject.get(), functor(*this, &CBrushObject::OnRenderVarChange));

	m_pVarObject->AddVariable(mv_outdoor, sVarName_OutdoorOnly, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_castShadowMaps, sVarName_CastShadowMaps, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_giMode, sVarName_GIMode, "Global Illumination", functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_rainOccluder, sVarName_RainOccluder, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_registerByBBox, sVarName_RegisterByBBox, functor(*this, &CBrushObject::OnRenderVarChange));
	
	m_pVarObject->AddVariable(mv_dynamicDistanceShadows, sVarName_DynamicDistanceShadows, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_hideable, sVarName_Hideable, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_ratioLOD, sVarName_LodRatio, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_ratioViewDist, sVarName_ViewDistRatio, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_excludeFromTriangulation, sVarName_NotTriangulate, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_noDynWater, sVarName_NoDynWater, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_aiRadius, sVarName_AIRadius, functor(*this, &CBrushObject::OnAIRadiusVarChange));
	m_pVarObject->AddVariable(mv_noDecals, sVarName_NoDecals, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_recvWind, sVarName_RecvWind, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_Occluder, sVarName_Occluder, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_drawLast, sVarName_DrawLast, functor(*this, &CBrushObject::OnRenderVarChange));
	m_pVarObject->AddVariable(mv_shadowLodBias, sVarName_ShadowLodBias, functor(*this, &CBrushObject::OnRenderVarChange));

	mv_ratioLOD.SetLimits(0, 255);
	mv_ratioViewDist.SetLimits(0, 255);

	m_bIgnoreNodeUpdate = false;
	m_RePhysicalizeOnVisible = false;

	SetFlags(OBJFLAG_SHOW_ICONONTOP);
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	FreeGameData();

	CBaseObject::Done();
}

bool CBrushObject::IncludeForGI()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::FreeGameData()
{
	LOADING_TIME_PROFILE_SECTION;
	if (m_pRenderNode)
	{
		GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}
	// Release Mesh.
	if (m_pGeometry)
		m_pGeometry->RemoveUser();
	m_pGeometry = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(255, 255, 255));

	if (IsCreateGameObjects())
	{
		if (prev)
		{
			CBrushObject* brushObj = (CBrushObject*)prev;
		}
		else if (!file.IsEmpty())
		{
			// Create brush from geometry.
			mv_geometryFile = file;

			string name = PathUtil::GetFileName(file);
			SetUniqName(name);
		}
		//m_indoor->AddBrush( this );
	}

	// Must be after SetBrush call.
	bool res = CBaseObject::Init(prev, file);

	if (prev)
	{
		CBrushObject* brushObj = (CBrushObject*)prev;
		m_bbox = brushObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		uint32 ForceID = GetObjectManager()->ForceID();
		m_pRenderNode = GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_Brush);
		if (ForceID)
		{
			m_pRenderNode->SetEditorObjectId(ForceID);
		}
		else
		{
			// keep high part of CryGUID (stays compatible with old GUID.Data1)
			m_pRenderNode->SetEditorObjectId((GetId().hipart >> 32));
		}
		UpdateEngineNode();
	}

	return true;
}

void CBrushObject::UpdateHighlightPassState(bool bSelected, bool bHighlighted)
{
	if (m_pRenderNode)
	{
		m_pRenderNode->SetEditorObjectInfo(bSelected, bHighlighted);
	}
}
//////////////////////////////////////////////////////////////////////////
void CBrushObject::ApplyStatObjProperties()
{
	if (m_pGeometry)
	{
		const char* const szProperties = m_pGeometry->GetIStatObj()->GetProperties();

		ParsePropertyString(szProperties, "\n\r", [this](const char* pKey, size_t keyLength, const char* pValue, size_t valueLength)
		{
			// TODO: Consider automatic iteration over m_pVarObject.
			if (strncmp("cast_shadow_maps", pKey, keyLength) == 0)
			{
				mv_castShadowMaps = !valueLength || strncmp("true", pValue, valueLength) == 0 || strncmp("1", pValue, valueLength) == 0;
			}
			else if (valueLength && std::isdigit(*pValue))
			{
				if (strncmp("lod_ratio", pKey, keyLength) == 0)
				{
					mv_ratioLOD = (float)atof(pValue);
				}
				else if (strncmp("view_dist_ratio", pKey, keyLength) == 0)
				{
					mv_ratioViewDist = (float)atof(pValue);
				}
			}
		});
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnFileChange(string filename)
{
	CUndo undo("Brush Prefab Modify");
	StoreUndo("Brush Prefab Modify");
	mv_geometryFile = filename;

	// Update variables in UI.
	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetSelected(bool bSelect)
{
	CBaseObject::SetSelected(bSelect);

	if (m_pRenderNode)
	{
		if (bSelect)
			m_renderFlags |= ERF_SELECTED;
		else
			m_renderFlags &= ~ERF_SELECTED;

		m_pRenderNode->SetRndFlags(m_renderFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::GetLocalBounds(AABB& box)
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Display(CObjectRenderHelper& objRenderHelper)
{
	if (!gViewportDebugPreferences.showBrushObjectHelper)
		return;

	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!m_pGeometry)
	{
		static int nagCount = 0;
		if (nagCount < 100)
		{
			const string& geomName = mv_geometryFile;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Brush '%s' (%s) does not have geometry!", (const char*)GetName(), (const char*)geomName);
			nagCount++;
		}
	}

	if (dc.flags & DISPLAY_2D)
	{
		int flags = 0;

		if (IsSelected())
		{
			dc.SetLineWidth(2);

			flags = 1;
			dc.SetColor(RGB(225, 0, 0));
		}
		else
		{
			flags = 0;
			dc.SetColor(GetColor());
		}

		dc.PushMatrix(GetWorldTM());
		dc.DrawWireBox(m_bbox.min, m_bbox.max);

		dc.PopMatrix();

		if (IsSelected())
			dc.SetLineWidth(0);

		if (m_pGeometry)
			m_pGeometry->Display(dc);

		return;
	}

	if (m_pGeometry)
	{
		if (!IsHighlighted())
		{
			m_pGeometry->Display(dc);
		}
	}

	if (IsSelected() && m_pRenderNode)
	{
		Vec3 scaleVec = GetScale();
		float scale = 0.5f * (scaleVec.x + scaleVec.y);
		float aiRadius = mv_aiRadius * scale;
		if (aiRadius > 0.0f)
		{
			dc.SetColor(1, 0, 1.0f, 0.7f);
			dc.DrawTerrainCircle(GetPos(), aiRadius, 0.1f);
		}
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CBrushObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;
	m_bIgnoreNodeUpdate = true;
	CBaseObject::Serialize(ar);
	m_bIgnoreNodeUpdate = false;
	if (ar.bLoading)
	{
		ar.node->getAttr("NotSharedGeometry", m_bNotSharedGeom);
		if (!m_pGeometry)
		{
			string mesh = mv_geometryFile;
			if (!mesh.IsEmpty())
				CreateBrushFromMesh(mesh);
		}

		UpdateEngineNode();
	}
	else
	{
		ar.node->setAttr("RndFlags", ERF_GET_WRITABLE(m_renderFlags));
		if (m_bNotSharedGeom)
			ar.node->setAttr("NotSharedGeometry", m_bNotSharedGeom);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::HitTest(HitContext& hc)
{
	if (CheckFlags(OBJFLAG_SUBOBJ_EDITING))
	{
		if (m_pGeometry)
		{
			bool bHit = m_pGeometry->HitTest(hc);
			if (bHit)
				hc.object = this;
			return bHit;
		}
		return false;
	}

	Vec3 pnt;

	Vec3 raySrc = hc.raySrc;
	Vec3 rayDir = hc.rayDir;
	WorldToLocalRay(raySrc, rayDir);

	if (Intersect::Ray_AABB(raySrc, rayDir, m_bbox, pnt))
	{
		if (hc.b2DViewport)
		{
			// World space distance.
			hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(pnt));
			hc.object = this;
			return true;
		}

		IPhysicalEntity* physics = 0;

		if (m_pRenderNode)
		{
			IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj();
			if (pStatObj)
			{
				SRayHitInfo hi;
				hi.inReferencePoint = raySrc;
				hi.inRay = Ray(raySrc, rayDir);
				if (pStatObj->RayIntersection(hi))
				{
					// World space distance.
					Vec3 worldHitPos = GetWorldTM().TransformPoint(hi.vHitPos);
					hc.dist = hc.raySrc.GetDistance(worldHitPos);
					hc.object = this;
					return true;
				}
				return false;
			}

			physics = m_pRenderNode->GetPhysics();
			if (physics)
			{
				if (physics->GetStatus(&pe_status_nparts()) == 0)
					physics = 0;
			}

		}

		if (physics)
		{
			Vec3r origin = hc.raySrc;
			Vec3r dir = hc.rayDir * 10000.0f;
			ray_hit hit;
			int col = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld()->RayTraceEntity(physics, origin, dir, &hit, 0, geom_collides);
			if (col > 0)
			{
				// World space distance.
				hc.dist = hit.dist;
				hc.object = this;
				return true;
			}
		}

		hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(pnt));
		hc.object = this;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
int CBrushObject::HitTestAxis(HitContext& hc)
{
	//@HACK Temporary hack.
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//! Invalidates cached transformation matrix.
void CBrushObject::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (!(nWhyFlags & eObjectUpdateFlags_RestoreUndo)) // Can skip updating game object when restoring undo.
	{
		if (m_pRenderNode)
			UpdateEngineNode(true);
	}

	m_invertTM = GetWorldTM();
	m_invertTM.Invert();

	// Make sure that if the brush is not visible while its transform is changing that we rephysicalize
	// once it's visible again
	if (!IsVisible() && m_pRenderNode && m_pRenderNode->GetPhysics())
	{
		m_RePhysicalizeOnVisible = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnEvent(ObjectEvent event)
{
	switch (event)
	{
	case EVENT_FREE_GAME_DATA:
		FreeGameData();
		return;
	case EVENT_PRE_EXPORT:
		{
			if (m_pRenderNode)
				m_pRenderNode->SetCollisionClassIndex(m_collisionFiltering.GetCollisionClassExportId());
		}
		return;
	}
	__super::OnEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::WorldToLocalRay(Vec3& raySrc, Vec3& rayDir)
{
	raySrc = m_invertTM.TransformPoint(raySrc);
	rayDir = m_invertTM.TransformVector(rayDir).GetNormalized();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnGeometryChange(IVariable* var)
{
	// Load new prefab model.
	string objName = mv_geometryFile;

	CreateBrushFromMesh(objName);
	InvalidateTM(0);

	SetModified(false, true);
}

//////////////////////////////////////////////////////////////////////////
static bool IsBrushFilename(const char* filename)
{
	if ((filename == 0) || (filename[0] == 0))
	{
		return false;
	}

	const char* const ext = CryGetExt(filename);

	if ((stricmp(ext, CRY_GEOMETRY_FILE_EXT) == 0) ||
	    (stricmp(ext, CRY_ANIM_GEOMETRY_FILE_EXT) == 0))
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::CreateBrushFromMesh(const char* meshFilename)
{
	if (m_pGeometry)
	{
		if (m_pGeometry->IsSameObject(meshFilename))
		{
			return;
		}

		m_pGeometry->RemoveUser();
	}

	if (meshFilename && meshFilename[0] && (!IsBrushFilename(meshFilename)))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "File %s cannot be used for brush geometry (unsupported file extension)", meshFilename);
		m_pGeometry = 0;
	}
	else
	{
		m_pGeometry = CEdMesh::LoadMesh(meshFilename);
	}

	if (m_pGeometry)
	{
		m_pGeometry->AddUser();
	}

	if (m_pGeometry)
	{
		ApplyStatObjProperties();
		UpdateEngineNode();
	}
	else if (m_pRenderNode)
	{
		// Remove this object from engine node.
		m_pRenderNode->SetEntityStatObj(0, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::ReloadGeometry()
{
	if (m_pGeometry)
	{
		m_pGeometry->ReloadGeometry();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnAIRadiusVarChange(IVariable* var)
{
	if (m_bIgnoreNodeUpdate)
		return;

	if (m_pRenderNode)
	{
		IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj();
		if (pStatObj)
		{
			// update all other objects that share pStatObj
			CBaseObjectsArray objects;
			GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
			for (unsigned i = 0; i < objects.size(); ++i)
			{
				CBaseObject* pBase = objects[i];
				if (pBase->GetType() == OBJTYPE_BRUSH)
				{
					CBrushObject* obj = (CBrushObject*)pBase;
					if (obj->m_pRenderNode && obj->m_pRenderNode->GetEntityStatObj() == pStatObj)
					{
						obj->mv_aiRadius = mv_aiRadius;
					}
				}
			}
		}
	}
	UpdateEngineNode();
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnRenderVarChange(IVariable* var)
{
	UpdateEngineNode();
	UpdatePrefab();

	// cached shadows if dynamic distance shadow flag has changed
	if (var == &mv_dynamicDistanceShadows)
		gEnv->p3DEngine->SetRecomputeCachedShadows();
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CBrushObject::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_pRenderNode)
		return m_pRenderNode->GetPhysics();
	return 0;
}

void CBrushObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CBrushObject>("Brush", [](CBrushObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		const string geometryFile = pObject->mv_geometryFile;

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_geometryFile, ar);

		// If a new geometry file is assigned it can set new default values. Do not overwrite them with old values from the UI.
		if (ar.isOutput() || geometryFile == (string)pObject->mv_geometryFile)
		{
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_outdoor, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_castShadowMaps, ar);
			pObject->m_pVarObject->SerializeVariable(pObject->mv_giMode.GetVar(), ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_dynamicDistanceShadows, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_rainOccluder, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_registerByBBox, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_hideable, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioLOD, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioViewDist, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_excludeFromTriangulation, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_noDynWater, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_aiRadius, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_noDecals, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_recvWind, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_Occluder, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_drawLast, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_shadowLodBias, ar);
		}

		if (ar.openBlock("cgf", "<CGF"))
		{
			ar(Serialization::ActionButton(std::bind(&CBrushObject::ReloadGeometry, pObject)), "reload_geometry", "^Reload CGF");
			ar(Serialization::ActionButton(std::bind(&CBrushObject::SaveToCGF, pObject)), "save_cgf", "^Save CGF");
			ar.closeBlock();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::ConvertFromObject(CBaseObject* object)
{
	__super::ConvertFromObject(object);
	if (object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* entity = (CEntityObject*)object;
		IEntity* pIEntity = entity->GetIEntity();
		if (!pIEntity)
			return false;

		IStatObj* pStatObj = pIEntity->GetStatObj(0);
		if (!pStatObj)
		{
			// Try to get a non-empty stat object with the largest radius.
			for (size_t i = 0, slotCount = pIEntity->GetSlotCount(); i < slotCount; ++i)
			{
				IStatObj* pCurrStatObj = pIEntity->GetStatObj(i);
				if (pCurrStatObj && (!pStatObj || (pStatObj->GetRadiusSqr() < pCurrStatObj->GetRadiusSqr())))
				{
					pStatObj = pCurrStatObj;
				}
			}
		}

		if (!pStatObj)
		{
			return false;
		}

		// Copy entity shadow parameters.
		//mv_selfShadowing = entity->IsSelfShadowing();
		mv_castShadowMaps = (entity->GetCastShadowMinSpec() < END_CONFIG_SPEC_ENUM) ? true : false;
		mv_giMode = (entity->GetGIMode() != 0);
		mv_rainOccluder = true;
		mv_registerByBBox = false;
		mv_ratioLOD = entity->GetRatioLod();
		mv_ratioViewDist = entity->GetRatioViewDist();
		mv_recvWind = false;

		mv_geometryFile = pStatObj->GetFilePath();
		return true;
	}
	/*
	   if (object->IsKindOf(RUNTIME_CLASS(CStaticObject)))
	   {
	   CStaticObject *pStatObject = (CStaticObject*)object;
	   mv_hideable = pStatObject->IsHideable();
	   }
	 */
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::UpdateEngineNode(bool bOnlyTransform)
{
	if (m_bIgnoreNodeUpdate)
		return;

	if (!m_pRenderNode)
		return;

	//////////////////////////////////////////////////////////////////////////
	// Set brush render flags.
	//////////////////////////////////////////////////////////////////////////
	m_renderFlags = 0;
	if (mv_outdoor)
		m_renderFlags |= ERF_OUTDOORONLY;
	//	if (mv_castShadows)
	//	m_renderFlags |= ERF_CASTSHADOWVOLUME;
	//if (mv_selfShadowing)
	//m_renderFlags |= ERF_SELFSHADOW;
	if (mv_castShadowMaps)
		m_renderFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
	if (mv_rainOccluder)
		m_renderFlags |= ERF_RAIN_OCCLUDER;
	if (mv_registerByBBox)
		m_renderFlags |= ERF_REGISTER_BY_BBOX;
	if (mv_dynamicDistanceShadows)
		m_renderFlags |= ERF_DYNAMIC_DISTANCESHADOWS;
	//	if (mv_recvLightmap)
	//	m_renderFlags |= ERF_USERAMMAPS;
	if (IsHidden() || IsHiddenBySpec())
		m_renderFlags |= ERF_HIDDEN;
	if (mv_hideable == 1)
		m_renderFlags |= ERF_HIDABLE;
	if (mv_hideable == 2)
		m_renderFlags |= ERF_HIDABLE_SECONDARY;
	//	if (mv_hideableSecondary)
	//		m_renderFlags |= ERF_HIDABLE_SECONDARY;
	if (mv_excludeFromTriangulation)
		m_renderFlags |= ERF_EXCLUDE_FROM_TRIANGULATION;
	if (mv_noDynWater)
		m_renderFlags |= ERF_NODYNWATER;
	if (mv_noDecals)
		m_renderFlags |= ERF_NO_DECALNODE_DECALS;
	if (mv_recvWind)
		m_renderFlags |= ERF_RECVWIND;
	if (mv_Occluder)
		m_renderFlags |= ERF_GOOD_OCCLUDER;
	((IBrush*)m_pRenderNode)->SetDrawLast(mv_drawLast);
	if (m_pRenderNode->GetRndFlags() & ERF_COLLISION_PROXY)
		m_renderFlags |= ERF_COLLISION_PROXY;
	if (m_pRenderNode->GetRndFlags() & ERF_RAYCAST_PROXY)
		m_renderFlags |= ERF_RAYCAST_PROXY;
	if (IsSelected())
		m_renderFlags |= ERF_SELECTED;
	if (mv_giMode)
		m_renderFlags |= ERF_GI_MODE_BIT0;

	int flags = GetRenderFlags();

	m_pRenderNode->SetRndFlags(m_renderFlags);

	m_pRenderNode->SetMinSpec(GetMinSpec());
	m_pRenderNode->SetViewDistRatio(mv_ratioViewDist);
	m_pRenderNode->SetLodRatio(mv_ratioLOD);
	m_pRenderNode->SetShadowLodBias(mv_shadowLodBias);

	m_renderFlags = m_pRenderNode->GetRndFlags();

	m_pRenderNode->SetMaterialLayers(GetMaterialLayersMask());

	if (m_pGeometry)
	{
		m_pGeometry->GetBounds(m_bbox);

		Matrix34A tm = GetWorldTM();
		m_pRenderNode->SetEntityStatObj(m_pGeometry->GetIStatObj(), &tm);
	}

	IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj();
	if (pStatObj)
	{
		float r = mv_aiRadius;
		pStatObj->SetAIVegetationRadius(r);
	}

	// Fast exit if only transformation needs to be changed.
	if (bOnlyTransform)
	{
		return;
	}

	if (GetMaterial())
	{
		((CMaterial*)GetMaterial())->AssignToEntity(m_pRenderNode);
	}
	else
	{
		// Reset all material settings for this node.
		m_pRenderNode->SetMaterial(0);
	}

	m_pRenderNode->SetLayerId(0);

	// Apply collision class filtering
	m_collisionFiltering.ApplyToPhysicalEntity(m_pRenderNode ? m_pRenderNode->GetPhysics() : 0);

	// Setting the objects layer modified, this is to ensure that if this object is embedded in a prefab that it is properly saved.
	this->SetLayerModified();

	return;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
IStatObj* CBrushObject::GetIStatObj()
{
	if (m_pRenderNode)
	{
		IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj();
		if (pStatObj)
		{
			if (pStatObj->GetIndexedMesh(true))
				return pStatObj;
		}
	}
	if (m_pGeometry == NULL)
		return NULL;
	return m_pGeometry->GetIStatObj();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	__super::SetMinSpec(nSpec, bSetChildren);
	if (m_pRenderNode)
	{
		m_pRenderNode->SetMinSpec(GetMinSpec());
		m_renderFlags = m_pRenderNode->GetRndFlags();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::UpdateVisibility(bool visible)
{
	if (visible == CheckFlags(OBJFLAG_INVISIBLE) ||
	    m_pRenderNode && (bool(m_renderFlags & ERF_HIDDEN) == (visible && !IsHiddenBySpec())) // force update if spec changed
	    )
	{
		CBaseObject::UpdateVisibility(visible);
		if (m_pRenderNode)
		{
			if (!visible || IsHiddenBySpec())
				m_renderFlags |= ERF_HIDDEN;
			else
				m_renderFlags &= ~ERF_HIDDEN;

			m_pRenderNode->SetRndFlags(m_renderFlags);
		}
	}

	// This is easily triggered when a level is loaded, and said level has invisible prefabs.
	// brushes within the prefab are transformed and not rephysicalized. If we don't rephysicalize
	// when the brush is made visible then its physics proxies will most likely be incorrect
	if (visible && m_RePhysicalizeOnVisible && m_pRenderNode)
	{
		m_pRenderNode->Dephysicalize();
		m_pRenderNode->Physicalize();
		m_RePhysicalizeOnVisible = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMaterial(IEditorMaterial* mtl)
{
	CBaseObject::SetMaterial(mtl);
	if (m_pRenderNode)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
IEditorMaterial* CBrushObject::GetRenderMaterial() const
{
	if (GetMaterial())
		return GetMaterial();
	if (m_pGeometry && m_pGeometry->GetIStatObj())
		return GetIEditorImpl()->GetMaterialManager()->FromIMaterial(m_pGeometry->GetIStatObj()->GetMaterial());
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	CBaseObject::SetMaterialLayersMask(nLayersMask);
	UpdateEngineNode(false);
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Validate()
{
	CBaseObject::Validate();

	if (!m_pGeometry)
	{
		string file = mv_geometryFile;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No Geometry %s for Brush %s %s", (const char*)file, (const char*)GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}
	else if (m_pGeometry->IsDefaultObject())
	{
		string file = mv_geometryFile;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Geometry file %s for Brush %s Failed to Load %s", (const char*)file, (const char*)GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}
}

string CBrushObject::GetAssetPath() const
{
	return mv_geometryFile;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::GatherUsedResources(CUsedResources& resources)
{
	string geomFile = mv_geometryFile;
	if (!geomFile.IsEmpty())
	{
		resources.Add(geomFile);
	}
	if (m_pGeometry && m_pGeometry->GetIStatObj())
	{
		CMaterialManager::GatherResources(m_pGeometry->GetIStatObj()->GetMaterial(), resources);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::IsSimilarObject(CBaseObject* pObject)
{
	if (pObject->GetClassDesc() == GetClassDesc() && GetRuntimeClass() == pObject->GetRuntimeClass())
	{
		CBrushObject* pBrush = (CBrushObject*)pObject;
		if ((string)mv_geometryFile == (string)pBrush->mv_geometryFile)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::StartSubObjSelection(int elemType)
{
	bool bStarted = false;
	if (m_pGeometry)
	{
		if (m_pGeometry->GetUserCount() > 1)
		{
			string str;
			str.Format("Geometry File %s is used by multiple objects.\r\nDo you want to make an unique copy of this geometry?",
			           (const char*)m_pGeometry->GetFilename());
			int res = MessageBox(AfxGetMainWnd()->GetSafeHwnd(), str, "Warning", MB_ICONQUESTION | MB_YESNOCANCEL);
			if (res == IDCANCEL)
				return false;
			if (res == IDYES)
			{
				// Must make a copy of the geometry.
				m_pGeometry = (CEdMesh*)m_pGeometry->Clone();
				string levelPath = PathUtil::AddBackslash(GetIEditorImpl()->GetLevelFolder().GetString());
				string filename = levelPath + "Objects\\" + GetName() + "." + CRY_GEOMETRY_FILE_EXT;
				filename = PathUtil::MakeGamePath(filename.GetString());
				m_pGeometry->SetFilename(filename);
				mv_geometryFile = filename;

				UpdateEngineNode();
				m_bNotSharedGeom = true;
			}
		}
		bStarted = m_pGeometry->StartSubObjSelection(GetWorldTM(), elemType, 0);
	}
	if (bStarted)
		SetFlags(OBJFLAG_SUBOBJ_EDITING);
	return bStarted;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::EndSubObjectSelection()
{
	ClearFlags(OBJFLAG_SUBOBJ_EDITING);
	if (m_pGeometry)
	{
		m_pGeometry->EndSubObjSelection();
		UpdateEngineNode(true);
		if (m_pRenderNode)
			m_pRenderNode->Physicalize();
		m_pGeometry->GetBounds(m_bbox);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::CalculateSubObjectSelectionReferenceFrame(SubObjectSelectionReferenceFrameCalculator* pCalculator)
{
	if (m_pGeometry)
	{
		Matrix34 refFrame;
		bool bAnySelected = m_pGeometry->GetSelectionReferenceFrame(refFrame);
		refFrame = this->GetWorldTM() * refFrame;
		pCalculator->SetExplicitFrame(bAnySelected, refFrame);
	}
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* CBrushObject::GetGeometry()
{
	// Return our geometry.
	return m_pGeometry;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SaveToCGF()
{
	string filename;
	if (CFileUtil::SelectSaveFile("CGF Files (*.cgf)|*.cgf", CRY_GEOMETRY_FILE_EXT, PathUtil::Make(PathUtil::GetGameFolder(), "objects").c_str(), filename))
	{
		filename = PathUtil::AbsolutePathToCryPakPath(filename.GetString());
		if (!filename.IsEmpty())
		{
			if (m_pGeometry)
			{
				if (GetMaterial())
					m_pGeometry->SaveToCGF(filename, NULL, GetMaterial()->GetMatInfo());
				else
					m_pGeometry->SaveToCGF(filename);
			}
			mv_geometryFile = PathUtil::MakeGamePath(filename.GetString()).c_str();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::GetVerticesInWorld(std::vector<Vec3>& vertices) const
{
	vertices.clear();
	if (m_pGeometry == NULL)
		return;
	IIndexedMesh* pIndexedMesh = m_pGeometry->GetIndexedMesh();
	if (pIndexedMesh == NULL)
		return;

	IIndexedMesh::SMeshDescription meshDesc;
	pIndexedMesh->GetMeshDescription(meshDesc);

	vertices.reserve(meshDesc.m_nVertCount);

	const Matrix34 tm = GetWorldTM();

	if (meshDesc.m_pVerts)
	{
		for (int v = 0; v < meshDesc.m_nVertCount; ++v)
		{
			vertices.push_back(tm.TransformPoint(meshDesc.m_pVerts[v]));
		}
	}
	else
	{
		for (int v = 0; v < meshDesc.m_nVertCount; ++v)
		{
			vertices.push_back(tm.TransformPoint(meshDesc.m_pVertsF16[v].ToVec3()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::InvalidateGeometryFile(const string& gamePath)
{
	if ((const string&)mv_geometryFile == gamePath)
	{
		if (m_pGeometry)
		{
			m_pGeometry->GetBounds(m_bbox);

			// Create a new render node, since a render node will not re-physicalize unless the stat obj changes.
			{
				if (m_pRenderNode)
				{
					GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
					m_pRenderNode = 0;
				}
				CreateGameObject();
				InvalidateTM(0);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CBrushObject::GetMouseOverStatisticsText() const
{
	string triangleCountText;
	string lodText;

	if (!m_pGeometry)
	{
		return triangleCountText;
	}

	IStatObj* pStatObj = m_pGeometry->GetIStatObj();

	if (pStatObj)
	{
		IStatObj::SStatistics stats;
		pStatObj->GetStatistics(stats);

		for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
		{
			if (stats.nIndicesPerLod[i] > 0)
			{
				lodText.Format("\n  LOD%d:   ", i);
				triangleCountText = triangleCountText + lodText +
				                    FormatWithThousandsSeperator(stats.nIndicesPerLod[i] / 3) + " Tris   "
				                    + FormatWithThousandsSeperator(stats.nVerticesPerLod[i]) + " Verts";
			}
		}
	}

	return triangleCountText;
}

