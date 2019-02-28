// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GeomEntity.h"
#include "BrushObject.h"

#include "Objects/InspectorWidgetCreator.h"

REGISTER_CLASS_DESC(CGeomEntityClassDesc);

IMPLEMENT_DYNCREATE(CGeomEntity, CEntityObject)

//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::Init(CBaseObject* prev, const string& file)
{
	bool bRes = false;

	//If the previous entity doesn't have the geom variable, remove the variable for this entity too.
	//So both share the same variables, which is important for the init process.
	if (prev != nullptr && m_pVarObject != nullptr && prev->GetVarObject() != nullptr &&  prev->GetVarObject()->FindVariable("Geometry") == nullptr)
	{
		m_pVarObject->DeleteVariable(&mv_geometry);
	}

	if (file.IsEmpty())
	{
		bRes = Super::Init(prev, "");
		SetClass("GeomEntity");
	}
	else
	{
		bRes = Super::Init(prev, "GeomEntity");
		SetClass("GeomEntity");
		SetGeometryFile(file);
		string name = PathUtil::GetFileName(file);
		if (!name.IsEmpty())
			SetUniqName(name);
	}
	m_bSupportsBoxHighlight = false;

	if (m_pEntity != nullptr && m_pVarObject != nullptr)
	{
		if (auto* pGeometryComponent = m_pEntity->GetComponent<IGeometryEntityComponent>())
		{
			m_pVarObject->DeleteVariable(&mv_geometry);
		}
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_geometry, "Geometry", functor(*this, &CGeomEntity::OnGeometryFileChange), IVariable::DT_OBJECT);

	Super::InitVariables();
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CGeomEntity>("Geometry", [](CGeomEntity* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_geometry, ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::SetGeometryFile(const string& filename)
{
	if (m_pEntity == nullptr)
	{
		SpawnEntity();
	}

	if (m_pEntity != nullptr)
	{
		if (auto* pGeometryComponent = m_pEntity->GetComponent<IGeometryEntityComponent>())
		{
			pGeometryComponent->SetGeometry(filename);
			CalcBBox();
			return;
		}
	}

	mv_geometry.Set(filename.c_str());

	if (!m_pEntity)
	{
		return;
	}

	const char* szExt = PathUtil::GetExt(filename);
	if (stricmp(szExt, CRY_SKEL_FILE_EXT) == 0 || stricmp(szExt, CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 || stricmp(szExt, CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
	{
		m_pEntity->LoadCharacter(0, filename, IEntity::EF_AUTO_PHYSICALIZE);
	}
	else
	{
		m_pEntity->LoadGeometry(0, filename, nullptr, IEntity::EF_AUTO_PHYSICALIZE);
	}

	CalcBBox();

	if (!m_bVisible)
	{
		m_pEntity->Hide(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::OnGeometryFileChange(IVariable* pLegacyVar)
{
	string value;
	pLegacyVar->Get(value);

	SetGeometryFile(value);
}

//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::ConvertFromObject(CBaseObject* object)
{
	Super::ConvertFromObject(object);

	if (object->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrushObject = (CBrushObject*)object;

		IStatObj* prefab = pBrushObject->GetIStatObj();
		if (!prefab)
		{
			return false;
		}

		// Copy entity shadow parameters.
		int rndFlags = pBrushObject->GetRenderFlags();

		mv_castShadowMinSpec = (rndFlags & ERF_CASTSHADOWMAPS) ? CONFIG_LOW_SPEC : END_CONFIG_SPEC_ENUM;

		mv_outdoor = (rndFlags & ERF_OUTDOORONLY) != 0;
		mv_ratioLOD = pBrushObject->GetRatioLod();
		mv_ratioViewDist = pBrushObject->GetRatioViewDist();
		mv_recvWind = (rndFlags & ERF_RECVWIND) != 0;
		mv_noDecals = (rndFlags & ERF_NO_DECALNODE_DECALS) != 0;

		SpawnEntity();
		SetGeometryFile(prefab->GetFilePath());
		return true;
	}
	else if (object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pObject = (CEntityObject*)object;
		if (!pObject->GetIEntity())
		{
			return false;
		}

		IStatObj* pStatObj = pObject->GetIEntity()->GetStatObj(0);
		if (!pStatObj)
		{
			return false;
		}

		SpawnEntity();
		SetGeometryFile(pStatObj->GetFilePath());
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::IsSimilarObject(CBaseObject* pObject)
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CGeomEntity* pEntity = (CGeomEntity*)pObject;
		if (GetGeometryFile() == pEntity->GetGeometryFile())
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::GetScriptProperties(XmlNodeRef xmlProperties)
{
	Super::GetScriptProperties(xmlProperties);

	string value = mv_geometry;
	if (value.GetLength() > 0)
	{
		SetGeometryFile(value);
	}
}

void CGeomEntity::InvalidateGeometryFile(const string& file)
{
	string val = mv_geometry;
	if (val == file)
	{
		if (m_pEntity)
		{
			DeleteEntity();
			SpawnEntity();
		}

		CalcBBox();
		InvalidateTM(0);
	}
}

XmlNodeRef CGeomEntity::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef node = Super::Export(levelPath, xmlNode);

	if (node && !m_pEntity->GetComponent<IGeometryEntityComponent>())
	{
		node->setAttr("Geometry", (string)mv_geometry);
	}

	return node;
}

