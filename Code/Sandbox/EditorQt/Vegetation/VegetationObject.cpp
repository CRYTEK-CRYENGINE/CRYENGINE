// StaticObjParam.cpp: implementation of the CVegetationObject class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VegetationMap.h"
#include "VegetationSelectTool.h"
#include "Material/MaterialManager.h"

#include "Terrain/Heightmap.h"
#include "Terrain/SurfaceType.h"
#include "Terrain/TerrainManager.h"

#include <Cry3DEngine/I3DEngine.h>

#include "VegetationObject.h"

#include "Util/BoostPythonHelpers.h"

#include <EditorFramework/Editor.h>
#include <EditorFramework/Preferences.h>
#include <Preferences/GeneralPreferences.h>
#include <Preferences/ViewportPreferences.h>

#include "CryEditDoc.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVegetationObject::CVegetationObject(int id)
{
	m_id = id;

	m_statObj = 0;
	m_objectSize = 1;
	m_numInstances = 0;
	m_bSelected = false;
	m_bHidden = false;
	m_index = 0;
	m_bInUse = true;

	m_bVarIgnoreChange = false;

	m_group = _T("Default");

	// Int vars.
	mv_size = 1.0f;
	mv_size->SetLimits(0.1f, 100.0f, 0.0f, true, true);

	mv_hmin = GetIEditorImpl()->GetHeightmap()->GetWaterLevel();
	mv_hmin->SetLimits(0.0f, 4096.0f, 0.0f, true, true);
	mv_hmax = 4096.0f;
	mv_hmax->SetLimits(0.0f, 4096.0f, 0.0f, true, true);

	// Limit the slope values from 0.0 to slightly less than 90. We later calculate the slope as
	// tan(angle) and tan(90) will result in disaster
	const float slopeLimitDeg = 90.f - 0.01;
	mv_slope_min = 0.0f;
	mv_slope_min->SetLimits(0.0f, slopeLimitDeg, 0.0f, true, true);
	mv_slope_max = slopeLimitDeg;
	mv_slope_max->SetLimits(0.0f, slopeLimitDeg, 0.0f, true, true);

	mv_stiffness = 0.5f;
	mv_damping = 2.5f;
	mv_variance = 0.6f;
	mv_airResistance = 1.f;

	mv_density = 1;
	mv_density->SetLimits(0.5f, 100, 0.0f, true, false);
	mv_bending = 0.0f;
	mv_bending->SetLimits(0, 100, 0.0f, true, false);
	mv_sizevar = 0;
	mv_sizevar->SetLimits(0, 100, 0.0f, true, false);
	mv_rotationRangeToTerrainNormal = 0;
	mv_rotationRangeToTerrainNormal->SetLimits(0, 100, 0.0f, true, false);

	mv_alignToTerrain = false;
	mv_alignToTerrainCoefficient = 0.f;
	mv_alignToTerrainCoefficient->SetLimits(0.0f, 100.0f, 0.0f, true, true);
	mv_castShadows = false;
	mv_castShadowMinSpec = CONFIG_LOW_SPEC;
	mv_giMode = true;
	mv_instancing = false;
	mv_DynamicDistanceShadows = false;
	mv_hideable = 0;
	//mv_hideableSecondary = false;
	mv_playerHideable = 0;
	mv_growOn = 0;
	mv_pickable = false;
	mv_aiRadius = -1.0f;
	mv_SpriteDistRatio = 1;
	mv_LodDistRatio = 1;
	mv_MaxViewDistRatio = 1;
	mv_ShadowDistRatio = 1;
	mv_UseSprites = false;
	mv_layerFrozen = false;
	mv_minSpec = 0;
	mv_allowIndoor = false;
	mv_autoMerged = false;

	m_guid = CryGUID::Create();

	mv_hideable.AddEnumItem("None", 0);
	mv_hideable.AddEnumItem("Hideable", 1);
	mv_hideable.AddEnumItem("Secondary", 2);

	mv_playerHideable.AddEnumItem("None", IStatInstGroup::ePlayerHideable_None);
	mv_playerHideable.AddEnumItem("Standing", IStatInstGroup::ePlayerHideable_High);
	mv_playerHideable.AddEnumItem("Crouching", IStatInstGroup::ePlayerHideable_Mid);
	mv_playerHideable.AddEnumItem("Proned", IStatInstGroup::ePlayerHideable_Low);

	mv_growOn->AddEnumItem("Terrain Only", eGrowOn_Terrain);
	mv_growOn->AddEnumItem("Brushes Only", eGrowOn_Brushes);
	mv_growOn->AddEnumItem("Terrain and Brushes", eGrowOn_Both);

	mv_minSpec.AddEnumItem("All", 0);
	mv_minSpec.AddEnumItem("Low", CONFIG_LOW_SPEC);
	mv_minSpec.AddEnumItem("Medium", CONFIG_MEDIUM_SPEC);
	mv_minSpec.AddEnumItem("High", CONFIG_HIGH_SPEC);
	mv_minSpec.AddEnumItem("VeryHigh", CONFIG_VERYHIGH_SPEC);
	mv_minSpec.AddEnumItem("Detail", CONFIG_DETAIL_SPEC);

	mv_castShadowMinSpec.AddEnumItem("Never", END_CONFIG_SPEC_ENUM);
	mv_castShadowMinSpec.AddEnumItem("Low", CONFIG_LOW_SPEC);
	mv_castShadowMinSpec.AddEnumItem("Medium", CONFIG_MEDIUM_SPEC);
	mv_castShadowMinSpec.AddEnumItem("High", CONFIG_HIGH_SPEC);
	mv_castShadowMinSpec.AddEnumItem("VeryHigh", CONFIG_VERYHIGH_SPEC);

	mv_castShadows->SetFlags(mv_castShadows->GetFlags() | IVariable::UI_INVISIBLE);
	mv_castShadowMinSpec->SetFlags(mv_castShadowMinSpec->GetFlags() | IVariable::UI_UNSORTED);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_size, "Size", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_sizevar, "SizeVar", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_rotation, "RandomRotation", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_rotationRangeToTerrainNormal, "RotationRangeToTerrainNormal", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_alignToTerrain, "AlignToTerrainHeight", functor(*this, &CVegetationObject::OnVarChange));
	mv_alignToTerrain->SetFlags(mv_alignToTerrain->GetFlags() | IVariable::UI_INVISIBLE);
	m_pVarObject->AddVariable(mv_alignToTerrainCoefficient, "AlignToTerrainNormals", functor(*this, &CVegetationObject::OnVarChange), IVariable::DT_PERCENT);
	m_pVarObject->AddVariable(mv_useTerrainColor, "UseTerrainColor", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_allowIndoor, "AllowIndoor", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_bending, "Bending", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_hideable, "Hideable", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_playerHideable, "PlayerHideable", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_materialGroundDecal, "GroundDecalMaterial", functor(*this, &CVegetationObject::OnMaterialChange), IVariable::DT_MATERIAL);
	m_pVarObject->AddVariable(mv_growOn, "GrowOn", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_autoMerged, "AutoMerged", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_stiffness, "Stiffness", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_damping, "Damping", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_variance, "Variance", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_airResistance, "AirResistance", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_pickable, "Pickable", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_aiRadius, "AIRadius", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_density, "Density", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_hmin, VEGETATION_ELEVATION_MIN, functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_hmax, VEGETATION_ELEVATION_MAX, functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_slope_min, VEGETATION_SLOPE_MIN, functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_slope_max, VEGETATION_SLOPE_MAX, functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_castShadows, "CastShadow", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_castShadowMinSpec, "CastShadowMinSpec", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_DynamicDistanceShadows, "DynamicDistanceShadows", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_giMode, "GlobalIllumination", "Global Illumination", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_instancing, "Instancing", "UseInstancing", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_SpriteDistRatio, "SpriteDistRatio", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_LodDistRatio, "LodDistRatio", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_MaxViewDistRatio, "MaxViewDistRatio", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_material, "Material", functor(*this, &CVegetationObject::OnMaterialChange), IVariable::DT_MATERIAL);
	m_pVarObject->AddVariable(mv_UseSprites, "UseSprites", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_minSpec, "MinSpec", functor(*this, &CVegetationObject::OnVarChange));

	m_pVarObject->AddVariable(mv_layerFrozen, "Frozen", "Layer_Frozen", functor(*this, &CVegetationObject::OnVarChange));
	m_pVarObject->AddVariable(mv_layerWet, "Layer_Wet", "Layer_Wet", functor(*this, &CVegetationObject::OnVarChange));

	m_pVarObject->AddVariable(mv_fileName, "Object", "Object", functor(*this, &CVegetationObject::OnFileNameChange), IVariable::DT_OBJECT);

	m_pVarObject->AddVariable(mv_layerVars, "UseOnTerrainLayers", "Use On Terrain Layers");
}

CVegetationObject::~CVegetationObject()
{
	if (m_statObj)
	{
		m_statObj->Release();
	}

	I3DEngine* const p3dEngine = GetIEditorImpl()->GetSystem()->GetI3DEngine();

	if (m_id >= 0)
	{
		IStatInstGroup grp;
		p3dEngine->SetStatInstGroup(m_id, grp);
	}
	if (m_pMaterial)
	{
		m_pMaterial = NULL;
	}

}

void CVegetationObject::SetFileName(const string& strFileName)
{
	mv_fileName->Set(strFileName);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetGroup(const string& group)
{
	m_group = group;
};

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::UnloadObject()
{
	if (m_statObj)
	{
		m_statObj->Release();
	}
	m_statObj = 0;
	m_objectSize = 1;

	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::LoadObject()
{
	m_objectSize = 1;
	string filename;
	mv_fileName->Get(filename);
	if (m_statObj == 0 && !filename.IsEmpty())
	{
		if (m_statObj)
		{
			m_statObj->Release();
			m_statObj = 0;
		}
		m_statObj = GetIEditorImpl()->GetSystem()->GetI3DEngine()->LoadStatObj(filename, NULL, NULL, false);
		if (m_statObj)
		{
			m_statObj->AddRef();
			Vec3 min = m_statObj->GetBoxMin();
			Vec3 max = m_statObj->GetBoxMax();
			m_objectSize = __max(max.x - min.x, max.y - min.y);
		}
	}
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetHidden(bool bHidden)
{
	m_bHidden = bHidden;
	SetInUse(!bHidden);

	GetIEditorImpl()->SetModifiedFlag();
	/*
	   for (int i = 0; i < GetObjectCount(); i++)
	   {
	   CVegetationObject *obj = GetObject(i);
	   obj->SetInUse( !bHidden );
	   }
	 */
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::CopyFrom(const CVegetationObject& o)
{
	if (m_pVarObject != nullptr)
	{
		m_pVarObject->CopyVariableValues(const_cast<CVegetationObject*>(&o)->m_pVarObject.get());
	}

	m_bInUse = o.m_bInUse;
	m_bHidden = o.m_bHidden;
	m_group = o.m_group;

	if (m_statObj == 0)
		LoadObject();

	GetIEditorImpl()->SetModifiedFlag();
	SetEngineParams();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnVarChange(IVariable* var)
{
	if (m_bVarIgnoreChange)
		return;

	SetEngineParams();
	GetIEditorImpl()->SetModifiedFlag();

	if (var == mv_hideable.GetVar() || var == mv_alignToTerrainCoefficient.GetVar() || var == mv_autoMerged.GetVar() || mv_autoMerged || var == mv_instancing.GetVar())
	{
		// Reposition this object on vegetation map.
		GetIEditorImpl()->GetVegetationMap()->RepositionObject(this);
		CEditTool* pTool = GetIEditorImpl()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
		{
			auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
			pVegetationTool->UpdateTransformManipulator();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnTerrainLayerVarChange(IVariable* pTerrainLayerVariable)
{
	if (m_bVarIgnoreChange || !pTerrainLayerVariable)
	{
		return;
	}

	auto layerName = pTerrainLayerVariable->GetName();
	auto bValue = false;
	pTerrainLayerVariable->Get(bValue);

	if (bValue)
	{
		stl::push_back_unique(m_terrainLayers, layerName);
	}
	else
	{
		stl::find_and_erase(m_terrainLayers, layerName);
	}

	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();

	SetEngineParams();
	GetIEditorImpl()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::UpdateMaterial()
{
	// Update CGF material
	{
		CMaterial* pMaterial = NULL;
		string mtlName = mv_material;
		if (!mtlName.IsEmpty())
			pMaterial = (CMaterial*)GetIEditorImpl()->GetMaterialManager()->LoadMaterial(mtlName);

		if (pMaterial != m_pMaterial)
			m_pMaterial = pMaterial;
	}

	// Update ground decal material
	{
		CMaterial* pMaterial = NULL;
		string mtlName = mv_materialGroundDecal;
		if (!mtlName.IsEmpty())
			pMaterial = (CMaterial*)GetIEditorImpl()->GetMaterialManager()->LoadMaterial(mtlName);

		if (pMaterial != m_pMaterialGroundDecal)
		{
			m_pMaterialGroundDecal = pMaterial;
			GetIEditorImpl()->GetVegetationMap()->PlaceObjectsOnTerrain();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnMaterialChange(IVariable* var)
{
	if (m_bVarIgnoreChange)
		return;

	UpdateMaterial();
	SetEngineParams();
	GetIEditorImpl()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnFileNameChange(IVariable* var)
{
	if (m_bVarIgnoreChange)
		return;

	UnloadObject();
	LoadObject();
	GetIEditorImpl()->SetModifiedFlag();
	GetIEditorImpl()->GetVegetationMap()->RepositionObject(this);
}
//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetEngineParams()
{
	I3DEngine* engine = GetIEditorImpl()->GetSystem()->GetI3DEngine();
	if (!engine)
		return;

	if (!engine->GetITerrain())
	{
		return;
	}

	IStatInstGroup grp;
	if (!m_bHidden)
	{
		if (m_numInstances > 0 || !m_terrainLayers.empty())
		{
			// Only set object when vegetation actually used in level.
			grp.pStatObj = m_statObj;
		}
	}
	grp.bHideability = mv_hideable == 1;
	grp.bHideabilitySecondary = mv_hideable == 2;
	grp.bPickable = mv_pickable;
	grp.fBending = mv_bending;
	grp.nCastShadowMinSpec = mv_castShadowMinSpec;
	grp.bGIMode = mv_giMode;
	grp.bInstancing = mv_instancing;
	grp.bDynamicDistanceShadows = mv_DynamicDistanceShadows;
	grp.fSpriteDistRatio = mv_SpriteDistRatio;
	grp.fLodDistRatio = mv_LodDistRatio;
	grp.fShadowDistRatio = mv_ShadowDistRatio;
	grp.fMaxViewDistRatio = mv_MaxViewDistRatio;
	grp.fBrightness = 1.f; // not used
	grp.bAutoMerged = mv_autoMerged;
	grp.fStiffness = mv_stiffness;
	grp.fDamping = mv_damping;
	grp.fVariance = mv_variance;
	grp.fAirResistance = mv_airResistance;

	grp.nPlayerHideable = mv_playerHideable;

	int nMinSpec = mv_minSpec;
	grp.minConfigSpec = (ESystemConfigSpec)nMinSpec;
	grp.pMaterial = 0;
	grp.nMaterialLayers = 0;

	if (m_pMaterial)
	{
		grp.pMaterial = m_pMaterial->GetMatInfo();
	}

	// Set material layer flags
	uint8 nMaterialLayersFlags = 0;

	// Activate frozen layer
	if (mv_layerFrozen)
	{
		nMaterialLayersFlags |= MTL_LAYER_FROZEN;
		// temporary fix: disable bending atm for material layers
		grp.fBending = 0.0f;
	}
	if (mv_layerWet)
	{
		nMaterialLayersFlags |= MTL_LAYER_WET;
	}

	grp.nMaterialLayers = nMaterialLayersFlags;
	grp.bUseSprites = mv_UseSprites;
	grp.bRandomRotation = mv_rotation;
	grp.nRotationRangeToTerrainNormal = mv_rotationRangeToTerrainNormal;
	grp.bUseTerrainColor = mv_useTerrainColor;
	grp.bAllowIndoor = mv_allowIndoor;
	// mv_alignToTerrainCoefficient is percentage float it has to be scaled
	grp.fAlignToTerrainCoefficient = mv_alignToTerrainCoefficient / 100.0f;

	// pass parameters for procedural objects placement
	grp.fDensity = mv_density;
	grp.fElevationMax = mv_hmax;
	grp.fElevationMin = mv_hmin;
	grp.fSize = mv_size;
	grp.fSizeVar = mv_sizevar;
	grp.fSlopeMax = mv_slope_max;
	grp.fSlopeMin = mv_slope_min;

	if (mv_hideable == 2)
	{
		grp.bHideability = false;
		grp.bHideabilitySecondary = true;
	}
	else if (mv_hideable == 1)
	{
		grp.bHideability = true;
		grp.bHideabilitySecondary = false;
	}
	else
	{
		grp.bHideability = false;
		grp.bHideabilitySecondary = false;
	}

	grp.nID = m_id;

	engine->SetStatInstGroup(m_id, grp);

	float r = mv_aiRadius;
	if (m_statObj)
		GetIEditorImpl()->GetVegetationMap()->SetAIRadius(m_statObj, r);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::Serialize(XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		m_bVarIgnoreChange = true;
		if (m_pVarObject != nullptr)
		{
			m_pVarObject->Serialize(node, bLoading);
		}
		m_bVarIgnoreChange = false;

		// Loading
		string fileName;
		node->getAttr("FileName", fileName);
		fileName = PathUtil::ToUnixPath((const char*)fileName).c_str();
		node->getAttr("GUID", m_guid);
		node->getAttr("Hidden", m_bHidden);
		node->getAttr("Category", m_group);

		m_terrainLayers.clear();
		XmlNodeRef layers = node->findChild("TerrainLayers");
		if (layers)
		{
			for (int i = 0; i < layers->getChildCount(); i++)
			{
				string name;
				XmlNodeRef layer = layers->getChild(i);
				if (layer->getAttr("Name", name) && !name.IsEmpty())
					m_terrainLayers.push_back(name);
			}
		}

		SetFileName(fileName);

		UnloadObject();
		LoadObject();
		UpdateMaterial();
		SetEngineParams();

		if ((mv_castShadowMinSpec == END_CONFIG_SPEC_ENUM) && mv_castShadows) // backwards compatibility check
		{
			mv_castShadowMinSpec = CONFIG_LOW_SPEC;
			mv_castShadows = false;
		}
		if (mv_alignToTerrain) // backwards compatibility check
		{
			mv_alignToTerrainCoefficient = 1.f;
			mv_alignToTerrain = false;
		}
	}
	else
	{
		// Do not serialize layer variables, behaviour of old mfc editor
		CVarBlock noLayerVariables;
		if (m_pVarObject != nullptr)
		{
			for (int i = 0; i < m_pVarObject->GetNumVariables(); ++i)
			{
				auto pVariable = m_pVarObject->GetVariable(i);
				if (pVariable == &mv_layerVars)
				{
					continue;
				}
				noLayerVariables.AddVariable(pVariable->Clone(true));
			}
		}
		noLayerVariables.Serialize(node, bLoading);

		// Save.
		node->setAttr("Id", m_id);
		node->setAttr("FileName", GetFileName());
		node->setAttr("GUID", m_guid);
		node->setAttr("Hidden", m_bHidden);
		node->setAttr("Index", m_index);
		node->setAttr("Category", m_group);

		if (!m_terrainLayers.empty())
		{
			XmlNodeRef layers = node->newChild("TerrainLayers");
			for (int i = 0; i < m_terrainLayers.size(); i++)
			{
				XmlNodeRef layer = layers->newChild("Layer");
				layer->setAttr("Name", m_terrainLayers[i]);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationObject::IsUsedOnTerrainLayer(const string& layer)
{
	return stl::find(m_terrainLayers, layer);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::UpdateTerrainLayerVariables()
{
	// clear old terrain layer variables
	mv_layerVars.DeleteAllVariables();
	const auto surfaceTypeCount = GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypeCount();
	for (int i = 0; i < surfaceTypeCount; ++i)
	{
		auto pType = GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypePtr(i);
		if (!pType)
		{
			continue;
		}

		auto pTerrainLayerBoolVar = new CVariable<bool>;
		pTerrainLayerBoolVar->SetName(pType->GetName());
		auto bValue = stl::find(m_terrainLayers, pType->GetName());
		pTerrainLayerBoolVar->Set(bValue);
		pTerrainLayerBoolVar->AddOnSetCallback(functor(*this, &CVegetationObject::OnTerrainLayerVarChange));

		mv_layerVars.AddVariable(pTerrainLayerBoolVar);
	}
}

//////////////////////////////////////////////////////////////////////////
int CVegetationObject::GetTextureMemUsage(ICrySizer* pSizer)
{
	int nSize = 0;
	if (m_statObj != NULL && m_statObj->GetRenderMesh() != NULL)
	{
		IMaterial* pMtl = (m_pMaterial) ? m_pMaterial->GetMatInfo() : NULL;
		if (!pMtl)
			pMtl = m_statObj->GetMaterial();
		if (pMtl)
			nSize = m_statObj->GetRenderMesh()->GetTextureMemoryUsage(pMtl, pSizer);

	}
	return nSize;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetSelected(bool bSelected)
{
	bool bSendEvent = bSelected != m_bSelected;
	m_bSelected = bSelected;
	if (bSendEvent)
	{
		GetIEditorImpl()->Notify(eNotify_OnVegetationObjectSelection);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::OnConfigSpecChange()
{
	bool bHiddenBySpec = false;
	int nMinSpec = mv_minSpec;

	if (gViewportPreferences.applyConfigSpec)
	{
		auto configSpec = GetIEditorImpl()->GetEditorConfigSpec();
		if (nMinSpec == CONFIG_DETAIL_SPEC && configSpec == CONFIG_LOW_SPEC)
			bHiddenBySpec = true;
		if (nMinSpec != 0 && configSpec != 0 && nMinSpec > configSpec)
			bHiddenBySpec = true;
	}

	// Hide/unhide object depending if it`s needed for this spec.
	if (bHiddenBySpec && !IsHidden())
		GetIEditorImpl()->GetVegetationMap()->HideObject(this, true);
	else if (!bHiddenBySpec && IsHidden())
		GetIEditorImpl()->GetVegetationMap()->HideObject(this, false);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationObject::SetNumInstances(int numInstances)
{
	if (m_numInstances == 0 && numInstances > 0)
	{
		m_numInstances = numInstances;
		// Object is really used.
		SetEngineParams();
	}
	else
	{
		m_numInstances = numInstances;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVegetationObject::IsHidden() const
{
	if (m_bHidden)
	{
		return true;
	}

	ICVar* piVariable = gEnv->pConsole->GetCVar("e_Vegetation");
	if (piVariable)
	{
		return (piVariable->GetIVal() != 0) ? FALSE : TRUE;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
namespace
{
// Handle all Vegetation Object Getters
//	-> Single Object by Name or ID
//	-> All Vegetation Objects by Name
//	-> All Vegetation Objects loaded.
boost::python::list PyGetVegetation(const string& vegetationName = "", bool loadedOnly = false)
{
	boost::python::list result;
	CVegetationMap* pVegMap = GetIEditorImpl()->GetVegetationMap();

	if (vegetationName.IsEmpty())
	{
		const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();

		if (pSel->GetCount() == 0)
		{
			for (int i = 0; i < pVegMap->GetObjectCount(); i++)
			{
				if (loadedOnly && pVegMap->GetObject(i)->IsHidden())
					continue;

				result.append(PyScript::CreatePyGameVegetation(pVegMap->GetObject(i)));
			}
		}
		else
		{
			for (int i = 0; i < pSel->GetCount(); i++)
			{
				if (pVegMap->GetObject(i)->IsSelected())
					result.append(PyScript::CreatePyGameVegetation(pVegMap->GetObject(i)));
			}
		}
	}
	else
	{
		for (int i = 0; i < pVegMap->GetObjectCount(); i++)
		{
			if (strcmp(vegetationName, pVegMap->GetObject(i)->GetFileName()) == 0)
			{
				if (loadedOnly && pVegMap->GetObject(i)->IsHidden())
					continue;

				result.append(PyScript::CreatePyGameVegetation(pVegMap->GetObject(i)));
			}
		}
	}

	return result;
}
}

BOOST_PYTHON_FUNCTION_OVERLOADS(pyGetVegetationOverload, PyGetVegetation, 0, 2);
REGISTER_PYTHON_OVERLOAD_COMMAND(PyGetVegetation, vegetation, get_vegetation, pyGetVegetationOverload,
                                 "Get all, selected, specific name, loaded vegetation objects in the current level.",
                                 "general.get_vegetation(str vegetationName=\'\', bool loadedOnly=False)");

