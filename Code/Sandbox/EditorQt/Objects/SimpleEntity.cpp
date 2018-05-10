// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SimpleEntity.h"
#include "BrushObject.h"

REGISTER_CLASS_DESC(CSimpleEntityClassDesc);

IMPLEMENT_DYNCREATE(CSimpleEntity, CEntityObject)

//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::Init(CBaseObject* prev, const string& file)
{
	bool bRes = false;
	if (file.IsEmpty())
	{
		bRes = CEntityObject::Init(prev, "");
	}
	else
	{
		bRes = CEntityObject::Init(prev, "BasicEntity");
		SetClass(m_entityClass);
		SetGeometryFile(file);
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CSimpleEntity::SetGeometryFile(const string& filename)
{
	if (m_pLuaProperties)
	{
		IVariable* pModelVar = m_pLuaProperties->FindVariable("object_Model");
		if (pModelVar)
		{
			pModelVar->Set(filename);
			SetModified(false, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CSimpleEntity::GetGeometryFile() const
{
	string filename;
	if (m_pLuaProperties)
	{
		IVariable* pModelVar = m_pLuaProperties->FindVariable("object_Model");
		if (pModelVar)
		{
			pModelVar->Get(filename);
		}
	}
	return filename;
}

//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::ConvertFromObject(CBaseObject* object)
{
	__super::ConvertFromObject(object);

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

		SetClass("BasicEntity");
		SpawnEntity();
		SetGeometryFile(prefab->GetFilePath());
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////
void CSimpleEntity::OnFileChange(string filename)
{
	CUndo undo("Entity Geom Modify");
	StoreUndo("Entity Geom Modify");
	SetGeometryFile(filename);
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CSimpleEntity::Validate()
{
	CEntityObject::Validate();

	// Checks if object loaded.
}

//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::IsSimilarObject(CBaseObject* pObject)
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CSimpleEntity* pEntity = (CSimpleEntity*)pObject;
		if (GetGeometryFile() == pEntity->GetGeometryFile())
		{
			return true;
		}
	}
	return false;
}

