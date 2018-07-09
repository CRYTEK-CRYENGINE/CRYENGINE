// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryNetwork/IServiceNetwork.h>
#include <CryNetwork/IRemoteCommand.h>
#include "LiveCreateManager.h"
#include "LiveCreateHost.h"

#include "LiveCreate_Objects.h"

#ifndef NO_LIVECREATE

namespace LiveCreate
{

IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetEntityTransform);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EntityUpdate);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EntityDelete);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_ObjectAreaUpdate);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EntityPropertyChange);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_UpdateLightParams);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EntitySetMaterial);

extern bool bIsCameraSyncEnabled;

void CLiveCreateCmd_SetEntityTransform::Execute()
{
	bool bValid = false;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(gEnv->pEntitySystem->FindEntityByGuid(m_guid));
	if (NULL != pEntity)
	{
		if (bIsCameraSyncEnabled)
		{
			pEntity->SetFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
		}

		pEntity->SetPosRotScale(
		  m_position,
		  m_rotation,
		  m_scale,
		  ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL | ENTITY_XFORM_EDITOR);

		bValid = true;
	}

	// a light
	if (0 != (m_flags & eFlag_Light))
	{
		ILightSource* pLight = FindLightSource(m_guid);
		if (NULL != pLight)
		{
			// move the cheap light proxy directly
			Matrix34 localTransform;
			localTransform.SetIdentity();
			localTransform.SetTranslation(m_position);
			pLight->SetMatrix(localTransform);

			bValid = true;
		}
	}
}

void CLiveCreateCmd_EntityUpdate::Execute()
{
	// deserialize xml data
	XmlNodeRef xml = gEnv->pSystem->LoadXmlFromBuffer(m_data.c_str(), m_data.length());
	if (NULL != xml)
	{
		// load or update the entity
		const EntityId entityID = gEnv->pEntitySystem->FindEntityByGuid(m_guid);
		if (0 != entityID)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
			if (NULL != pEntity)
			{
				pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
				gEnv->pEntitySystem->RemoveEntity(entityID);
			}
		}

		// remove the cheap light source
		if (0 != (m_flags & eFlag_Light))
		{
			ILightSource* pLight = FindLightSource(m_guid);
			if (NULL != pLight)
			{
				gEnv->p3DEngine->DeleteLightSource(pLight);
			}
		}

		// Force deletion of removed entities.
		gEnv->pEntitySystem->DeletePendingEntities();

		// deserialize entity
		XmlNodeRef xmlRoot = gEnv->pSystem->CreateXmlNode("Entities");
		xmlRoot->addChild(xml);
		gEnv->pEntitySystem->LoadEntities(xmlRoot, true);
	}
}

void CLiveCreateCmd_EntityDelete::Execute()
{
	// load or update the entity
	const EntityId entityID = gEnv->pEntitySystem->FindEntityByGuid(m_guid);
	if (0 != entityID)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
		if (NULL != pEntity)
		{
			pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
			gEnv->pEntitySystem->RemoveEntity(entityID);
		}
	}

	// Cheap light
	if (0 != (m_flags & eFlag_Light))
	{
		ILightSource* pLight = FindLightSource(m_guid);
		if (NULL != pLight)
		{
			gEnv->p3DEngine->DeleteLightSource(pLight);
		}
	}

	// Force deletion of removed entities.
	gEnv->pEntitySystem->DeletePendingEntities();
};

void CLiveCreateCmd_ObjectAreaUpdate::Execute()
{
	const uint32 kMaxLayers = 1024;

	// get mask for visible layers
	uint8 layerMask[kMaxLayers / 8];
	memset(layerMask, 0, sizeof(layerMask));
	gEnv->pEntitySystem->GetVisibleLayerIDs(layerMask, kMaxLayers);

	// load layer names
	uint16 layerIdTable[kMaxLayers];
	memset(layerIdTable, 0, sizeof(layerIdTable));
	for (uint32 i = 0; i < m_layerNames.size(); ++i)
	{
		const uint16 dataId = m_layerIds[i];
		if (dataId < kMaxLayers)
		{
			const int layerId = gEnv->pEntitySystem->GetLayerId(m_layerNames[i].c_str());
			if (layerId == -1)
			{
				gEnv->pLog->LogWarning("Layer '%s' not found on console", m_layerNames[i].c_str());
			}
			else
			{
				layerIdTable[dataId] = (uint16)layerId;
			}
		}
	}

	// load data using the layer visibility mask so only the visible layers are updated
	CLiveCreateInplaceReader dataReader(&m_data[0], m_data.size());
	gEnv->p3DEngine->LoadInternalState(dataReader, layerMask, layerIdTable);
}

void CLiveCreateCmd_UpdateLightParams::Execute()
{
	ILightSource* pLight = FindLightSource(m_guid);
	if (NULL != pLight)
	{
		SRenderLight params = pLight->GetLightProperties();
		params.m_Flags = m_Flags;
		params.m_fRadius = m_fRadius;
		params.m_Color = m_Color;
		params.m_SpecMult = m_SpecMult;
		params.m_fHDRDynamic = m_fHDRDynamic;
		params.m_nAttenFalloffMax = m_nAttenFalloffMax;
		params.m_nSortPriority = m_nSortPriority;
		params.m_fShadowBias = m_fShadowBias;
		params.m_fShadowSlopeBias = m_fShadowSlopeBias;
		params.m_fShadowResolutionScale = m_fShadowResolutionScale;
		params.m_fShadowUpdateMinRadius = m_fShadowUpdateMinRadius;
		params.m_nShadowMinResolution = m_nShadowMinResolution;
		params.m_nShadowUpdateRatio = m_nShadowUpdateRatio;
		params.m_fBaseRadius = m_fBaseRadius;
		params.m_BaseColor = m_BaseColor;
		params.m_BaseSpecMult = m_BaseSpecMult;
		pLight->SetLightProperties(params);
	}
}

void CLiveCreateCmd_EntityPropertyChange::Execute()
{
	// Load or update the entity
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(gEnv->pEntitySystem->FindEntityByGuid(m_guid));
	if (NULL != pEntity)
	{
		const void* pData = NULL;
		switch (m_type)
		{
		case eType_BOOL:
		case eType_INT:
			{
				pData = &m_valueInt;
				break;
			}

		case eType_VECTOR2:
		case eType_VECTOR:
		case eType_VECTOR4:
		case eType_QUAT:
		case eType_FLOAT:
			{
				pData = &m_valueVec4.x;
				break;
			}

		case eType_STRING:
			{
				pData = &m_valueString;
				break;
			}
		}

		pEntity->HandleVariableChange(m_name.c_str(), pData);
	}
}

void CLiveCreateCmd_EntitySetMaterial::Execute()
{
	// find the entity
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(gEnv->pEntitySystem->FindEntityByGuid(m_guid));
	if (NULL != pEntity)
	{
		// we want to restore original material
		if (m_materialName.empty())
		{
			pEntity->SetMaterial(NULL);
		}
		else
		{
			// load the material
			IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(m_materialName.c_str());
			if (NULL == pMaterial)
			{
				pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialName.c_str());
				if (NULL == pMaterial)
				{
					gEnv->pLog->LogWarning("SetMaterialCommand: Material '%s' not found", m_materialName.c_str());
					return;
				}
			}

			// set the material
			pEntity->SetMaterial(pMaterial);
		}
	}
}

}

#endif
