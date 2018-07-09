// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_det_tex.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain detail textures
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "3dEngine.h"

void CTerrain::SetDetailLayerProperties(int nId, float fScaleX, float fScaleY,
                                        uint8 ucProjAxis, const char* szSurfName,
                                        const PodArray<int>& lstnVegetationGroups, IMaterial* pMat)
{
	if (nId >= 0 && nId < SRangeInfo::e_max_surface_types)
	{
		const int nNameLength = strlen(szSurfName);
		const int nDestNameLength = sizeof(m_SSurfaceType[nId].szName) - 1;
		if (nNameLength > nDestNameLength)
			Error("CTerrain::SetDetailLayerProperties: attempt to assign too long surface type name (%s)", szSurfName);
		cry_strcpy(m_SSurfaceType[nId].szName, szSurfName);
		m_SSurfaceType[nId].fScale = fScaleX;
		m_SSurfaceType[nId].ucDefProjAxis = ucProjAxis;
		m_SSurfaceType[nId].ucThisSurfaceTypeId = nId;
		m_SSurfaceType[nId].lstnVegetationGroups.Reset();
		m_SSurfaceType[nId].lstnVegetationGroups.AddList(lstnVegetationGroups);
		m_SSurfaceType[nId].pLayerMat = (CMatInfo*)pMat;
#ifndef SW_STRIP_LOADING_MSG
		if (m_SSurfaceType[nId].pLayerMat && !m_bEditor)
			CTerrain::Get3DEngine()->PrintMessage("  Layer %d - %s has material %s", nId, szSurfName, pMat->GetName());
#endif
	}
	else
	{
		Warning("CTerrain::SetDetailTextures: LayerId is out of range: %d: %s", nId, szSurfName);
		assert(!"CTerrain::SetDetailTextures: LayerId is out of range");
	}
}

void CTerrain::LoadSurfaceTypesFromXML(XmlNodeRef pDetTexTagList)
{
	LOADING_TIME_PROFILE_SECTION;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Terrain, 0, "Surface types");

#ifndef SW_STRIP_LOADING_MSG
	CTerrain::Get3DEngine()->PrintMessage("Loading terrain layers ...");
#endif

	if (!pDetTexTagList)
		return;

	m_bProcVegetationInUse = false;

	for (int nId = 0; nId < pDetTexTagList->getChildCount() && nId < SRangeInfo::e_max_surface_types; nId++)
	{
		XmlNodeRef pDetLayer = pDetTexTagList->getChild(nId);
		IMaterialManager* pMatMan = Get3DEngine()->GetMaterialManager();
		const char* pMatName = pDetLayer->getAttr("DetailMaterial");
		_smart_ptr<IMaterial> pMat = pMatName[0] ? pMatMan->LoadMaterial(pMatName) : NULL;

		// material diffuse texture may be needed to generate terrain base texture (on CPU)
		if (gEnv->IsEditor() && pMat)
			pMat->SetKeepLowResSysCopyForDiffTex();

		float fScaleX = 1.f;
		pDetLayer->getAttr("DetailScaleX", fScaleX);
		float fScaleY = 1.f;
		pDetLayer->getAttr("DetailScaleY", fScaleY);
		uint8 projAxis = pDetLayer->getAttr("ProjAxis")[0];

		if (!pMat || pMat == pMatMan->GetDefaultMaterial())
		{
			Error("CTerrain::LoadSurfaceTypesFromXML: Error loading material: %s", pMatName);
			pMat = pMatMan->GetDefaultTerrainLayerMaterial();
		}

		if (CMatInfo* pMatInfo = static_cast<CMatInfo*>(pMat.get()))
		{
			pMatInfo->m_fDefautMappingScale = fScaleX;
			Get3DEngine()->InitMaterialDefautMappingAxis(pMatInfo);
		}

		PodArray<int> lstnVegetationGroups;
		for (int nGroup = 0; nGroup < pDetLayer->getChildCount(); nGroup++)
		{
			XmlNodeRef pVegetationGroup = pDetLayer->getChild(nGroup);
			int nVegetationGroupId = -1;
			if (pVegetationGroup->isTag("VegetationGroup") && pVegetationGroup->getAttr("Id", nVegetationGroupId))
			{
				lstnVegetationGroups.Add(nVegetationGroupId);
				m_bProcVegetationInUse = true;
			}
		}

		SetDetailLayerProperties(nId, fScaleX, fScaleY, projAxis, pDetLayer->getAttr("Name"), lstnVegetationGroups, pMat);
	}
}

void CTerrain::UpdateSurfaceTypes()
{
	if (GetParentNode())
		GetParentNode()->UpdateDetailLayersInfo(true);
}
