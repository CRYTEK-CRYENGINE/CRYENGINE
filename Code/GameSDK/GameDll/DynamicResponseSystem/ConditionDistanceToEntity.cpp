// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConditionDistanceToEntity.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CrySerialization/IArchive.h>
#include <cmath>

CConditionDistanceToEntity::CConditionDistanceToEntity()
	: m_squaredDistance(100.0f)
	, m_entityName()
{
}

//--------------------------------------------------------------------------------------------------
CConditionDistanceToEntity::CConditionDistanceToEntity(const string& actorName)
	: m_squaredDistance(100.0f)
	, m_entityName(actorName)
{
}

//--------------------------------------------------------------------------------------------------
CConditionDistanceToEntity::~CConditionDistanceToEntity()
{
}

//--------------------------------------------------------------------------------------------------
bool CConditionDistanceToEntity::IsMet(DRS::IResponseInstance* pResponseInstance)
{
	IEntity* pTargetEntity = gEnv->pEntitySystem->FindEntityByName(m_entityName.c_str());
	if (pTargetEntity)
	{
		IEntity* pSourceEntity = pResponseInstance->GetCurrentActor()->GetLinkedEntity();
		if (pSourceEntity)
		{
			return pTargetEntity->GetWorldPos().GetSquaredDistance(pSourceEntity->GetWorldPos()) < m_squaredDistance;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
void CConditionDistanceToEntity::Serialize(Serialization::IArchive& ar)
{
	float distance = sqrt(m_squaredDistance);
	ar(distance, "Distance", "^> Distance");
	m_squaredDistance = distance * distance;
	ar(m_entityName, "EntityName", "^EntityName");
}

//--------------------------------------------------------------------------------------------------
string CConditionDistanceToEntity::GetVerboseInfo() const
{
	return m_entityName + "' < than " + CryStringUtils::toString(sqrt(m_squaredDistance)).c_str();
}
