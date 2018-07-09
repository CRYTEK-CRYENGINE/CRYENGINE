// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SurfaceTypeValidator.h"
#include <Cry3DEngine/IMaterial.h>
#include <CryAction/IMaterialEffects.h>
#include <CryGame/IGameFramework.h>
#include "Material/Material.h"

#include <CrySystem/ICryLink.h>

void CSurfaceTypeValidator::Validate()
{
	LOADING_TIME_PROFILE_SECTION;
	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	std::set<string> reportedMaterialNames;

	CBaseObjectsArray objects;
	pObjectManager->GetObjects(objects);
	for (CBaseObjectsArray::iterator itObject = objects.begin(); itObject != objects.end(); ++itObject)
	{
		CBaseObject* pObject = *itObject;
		IPhysicalEntity* pPhysicalEntity = pObject ? pObject->GetCollisionEntity() : 0;

		// query part number with GetStatus(pe_status_nparts)
		pe_status_nparts numPartsQuery;
		int numParts = pPhysicalEntity ? pPhysicalEntity->GetStatus(&numPartsQuery) : 0;

		// iterate the parts, query GetParams(pe_params_part) for each part index (0..n-1)
		// (set ipart to the part index and MARK_UNUSED partid before each getstatus call)
		pe_params_part partQuery;
		for (partQuery.ipart = 0; partQuery.ipart < numParts; ++partQuery.ipart)
		{
			MARK_UNUSED partQuery.partid;
			int queryResult = pPhysicalEntity->GetParams(&partQuery);

			// if flags & (geom_colltype0|geom_colltype_player), check nMats entries in pMatMapping. they should contain valid game mat ids
			if (queryResult != 0 && partQuery.flagsAND & (geom_colltype0 | geom_colltype_player))
			{
				CMaterial* pEditorMaterial = pObject ? (CMaterial*)pObject->GetRenderMaterial() : 0;
				IMaterial* pMaterial = pEditorMaterial ? pEditorMaterial->GetMatInfo() : 0;

				char usedSubMaterials[MAX_SUB_MATERIALS];
				memset(usedSubMaterials, 0, sizeof(usedSubMaterials));
				if (pMaterial && reportedMaterialNames.insert(pMaterial->GetName()).second)
				{
					GetUsedSubMaterials(&partQuery, usedSubMaterials);
				}

				int surfaceTypeIDs[MAX_SUB_MATERIALS];
				memset(surfaceTypeIDs, 0, sizeof(surfaceTypeIDs));
				int numSurfaces = pMaterial ? pMaterial->FillSurfaceTypeIds(surfaceTypeIDs) : 0;

				for (int surfaceIDIndex = 0; surfaceIDIndex < numSurfaces; ++surfaceIDIndex)
				{
					string materialSpec;
					materialSpec.Format("%s:%d", pMaterial->GetName(), surfaceIDIndex + 1);
					if (usedSubMaterials[surfaceIDIndex] && surfaceTypeIDs[surfaceIDIndex] <= 0 && reportedMaterialNames.insert(materialSpec).second)
					{
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Physicalized object has material (%s) with invalid surface type. %s", materialSpec.c_str(),
						           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", pObject->GetName()));
					}
				}
			}
		}

		pe_status_placeholder spc;
		pe_action_reset ar;
		if (pPhysicalEntity && pPhysicalEntity->GetStatus(&spc) && spc.pFullEntity)
			pPhysicalEntity->Action(&ar, 1);
	}
}

void CSurfaceTypeValidator::GetUsedSubMaterials(pe_params_part* pPart, char usedSubMaterials[])
{
	phys_geometry* pGeometriesToCheck[2];
	int numGeometriesToCheck = 0;
	if (pPart->pPhysGeom)
		pGeometriesToCheck[numGeometriesToCheck++] = pPart->pPhysGeom;
	if (pPart->pPhysGeomProxy && pPart->pPhysGeomProxy != pPart->pPhysGeom)
		pGeometriesToCheck[numGeometriesToCheck++] = pPart->pPhysGeomProxy;

	for (int geometryToCheck = 0; geometryToCheck < numGeometriesToCheck; ++geometryToCheck)
	{
		phys_geometry* pGeometry = pGeometriesToCheck[geometryToCheck];

		IGeometry* pCollGeometry = pGeometry ? pGeometry->pGeom : 0;
		mesh_data* pMesh = (mesh_data*)(pCollGeometry && pCollGeometry->GetType() == GEOM_TRIMESH ? pCollGeometry->GetData() : 0);

		if (pMesh && pMesh->pMats)
		{
			for (int i = 0; i < pMesh->nTris; ++i)
			{
				const char subMatId = pMesh->pMats[i];
				if (subMatId >= 0 && subMatId < MAX_SUB_MATERIALS)
					usedSubMaterials[subMatId] = 1;
			}
		}
		else
		{
			if (pGeometry)
			{
				const char subMatId = pGeometry->surface_idx;
				if (subMatId >= 0 && subMatId < MAX_SUB_MATERIALS)
					usedSubMaterials[subMatId] = 1;
			}
		}
	}
}

