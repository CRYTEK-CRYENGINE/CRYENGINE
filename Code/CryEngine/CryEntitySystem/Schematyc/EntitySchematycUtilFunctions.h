// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "Entity.h"
#include "EntitySystem.h"
#include <CrySerialization/Forward.h>
#include <CryMath/Cry_Camera.h>

#include  <CrySchematyc/Env/Elements/EnvFunction.h>
#include  <CrySchematyc/Utils/SharedString.h>
#include  <CrySchematyc/IObject.h>
#include  <CrySchematyc/Env/IEnvRegistrar.h>

namespace Schematyc
{
namespace Entity
{
void GetEntityName(ExplicitEntityId entityId, CSharedString& stringOut)
{
	if (const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		stringOut = CSharedString(pEntity->GetName());
	}
}

void GetEntityObjectId(ExplicitEntityId entityId, ObjectId& objectIdOut)
{
	const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId));
	objectIdOut = (pEntity && pEntity->GetSchematycObject()) ? pEntity->GetSchematycObject()->GetId() : ObjectId::Invalid;
}

void FindEntityByName(CSharedString name, ExplicitEntityId& entityIdOut)
{
	const CEntity* pEntity = static_cast<CEntity*>(g_pIEntitySystem->FindEntityByName(name.c_str()));
	entityIdOut = ExplicitEntityId(pEntity ? pEntity->GetId() : INVALID_ENTITYID);
}

void Hide(ExplicitEntityId entityId, bool bHide)
{
	if (CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		pEntity->Hide(bHide);
	}
}

void IsHidden(ExplicitEntityId entityId, bool& bOut)
{
	if (const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		bOut = pEntity->IsHidden();
	}
	else
	{
		bOut = true;
	}
}

void SetTransform(ExplicitEntityId entityId, const CryTransform::CTransform& transform)
{
	if (CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		pEntity->SetWorldTM(transform.ToMatrix34());
	}
}

void GetTransform(ExplicitEntityId entityId, CryTransform::CTransform& transformOut)
{
	if (const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		transformOut = CryTransform::CTransform(pEntity->GetWorldTM());
	}
	else
	{
		transformOut = CryTransform::CTransform();
	}
}

void SetRotation(ExplicitEntityId entityId, const CryTransform::CRotation& rotation)
{
	if (CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		Matrix34 transform = pEntity->GetWorldTM();
		transform.SetRotation33(rotation.ToMatrix33());
		pEntity->SetWorldTM(transform);
	}
}

void GetRotation(ExplicitEntityId entityId, CryTransform::CRotation& rotationOut)
{
	if (const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		rotationOut = CryTransform::CRotation(pEntity->GetWorldRotation());
	}
	else
	{
		rotationOut = CryTransform::CRotation();
	}
}

void SetPosition(ExplicitEntityId entityId, const Vec3& position)
{
	if (CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		Matrix34 transform = pEntity->GetWorldTM();
		transform.SetTranslation(position);
		pEntity->SetWorldTM(transform);
	}
}

void GetPosition(ExplicitEntityId entityId, Vec3& positionOut)
{
	if (const CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(static_cast<EntityId>(entityId)))
	{
		positionOut = pEntity->GetWorldPos();
	}
	else
	{
		positionOut = ZERO;
	}
}

void SpawnEntity(ExplicitEntityId& entityIdOut, Schematyc::EntityClassName className, const CryTransform::CTransform& transform)
{
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = className.value.size() > 0 ? g_pIEntitySystem->GetClassRegistry()->FindClass(className.value) : nullptr;
	spawnParams.vPosition = transform.GetTranslation();
	spawnParams.vScale = transform.GetScale();
	spawnParams.qRotation = transform.GetRotation().ToQuat();

	if (const CEntity* pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(spawnParams)))
	{
		entityIdOut = ExplicitEntityId(pEntity->GetId());
	}
	else
	{
		entityIdOut = ExplicitEntityId(INVALID_ENTITYID);
	}
}

void RemoveEntity(ExplicitEntityId entityId)
{
	g_pIEntitySystem->RemoveEntity(static_cast<EntityId>(entityId));
}

static void RegisterUtilFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(GetTypeDesc<ExplicitEntityId>().GetGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityName, "955ca6c4-5b0a-4150-aaba-79e2939e85f7"_cry_guid, "GetName");
		pFunction->SetDescription("Get name of entity");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindOutput(2, 'name', "Name");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityObjectId, "cdd011ac-14a1-403b-af21-28d9e77e562d"_cry_guid, "GetObjectId");
		pFunction->SetDescription("Get object id from entity");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindOutput(2, 'obj', "ObjectId");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&FindEntityByName, "{823518D1-6FE9-49DA-A522-C1483385B70D}"_cry_guid, "FindByName");
		pFunction->SetDescription("Finds an entity by name");
		pFunction->BindInput(1, 'name', "Name");
		pFunction->BindOutput(2, 'ent', "Entity");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&Hide, "abc4938d-a631-4a36-9f10-22cf6dc9dabd"_cry_guid, "Hide");
		pFunction->SetDescription("Show/hide entity");
		pFunction->SetFlags(EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindInput(2, 'vis', "Hide");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&IsHidden, "5aa5e8f0-b4f4-491d-8074-d8b129500d09"_cry_guid, "IsHidden");
		pFunction->SetDescription("Is entity hidden?");
		pFunction->SetFlags(EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindOutput(2, 'vis', "Visible");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SetTransform, "FA08CEA0-A0C5-4340-9F8A-E38D74488BAF"_cry_guid, "SetTransform");
		pFunction->SetDescription("Set Entity Transformation");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindInput(2, 'tr', "transform");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetTransform, "8A99E1BA-A5CD-4DE8-A19F-D07DF5D3B245"_cry_guid, "GetTransform");
		pFunction->SetDescription("Get Entity Transformation");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindOutput(2, 'tr', "transform");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SetRotation, "{53FDFFFB-A216-4001-BA26-9E81A7D2160D}"_cry_guid, "SetRotation");
		pFunction->SetDescription("Set Entity Rotation");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindInput(2, 'rot', "rotation");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetRotation, "{B03F7198-583E-4C9C-BDC7-92D904920D2C}"_cry_guid, "GetRotation");
		pFunction->SetDescription("Get Entity Rotation");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindOutput(2, 'rot', "rotation");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SetPosition, "{1FD3B799-D029-480A-8D7B-F4EDFFF3D5F9}"_cry_guid, "SetPosition");
		pFunction->SetDescription("Set Entity Position");
		pFunction->BindInput(1, 'ent', "Entity");
		pFunction->BindInput(2, 'pos', "Position");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SpawnEntity, "{7689D503-263E-4FE2-8BF5-3875DEFB7F3C}"_cry_guid, "Spawn");
		pFunction->SetDescription("Spawns an entity instance in the world");
		pFunction->BindInput(2, 'clas', "Entity Class");
		pFunction->BindInput(3, 'tran', "Transform");
		pFunction->BindOutput(1, 'ent', "Entity");
		scope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&RemoveEntity, "{0A8D243F-01BB-4E28-A26E-1D7A83A61603}"_cry_guid, "Remove");
		pFunction->SetDescription("Removes an entity instance from the world");
		pFunction->BindInput(1, 'ent', "Entity");
		scope.Register(pFunction);
	}
}
} // Entity
} // Schematyc
