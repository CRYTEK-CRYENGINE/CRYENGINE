// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
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
CSharedString GetEntityName(ExplicitEntityId entityId)
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId));
	return pEntity ? CSharedString(pEntity->GetName()) : CSharedString();
}

ObjectId GetEntityObjectId(ExplicitEntityId entityId)
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId));
	return (pEntity && pEntity->GetSchematycObject()) ? pEntity->GetSchematycObject()->GetId() : ObjectId::Invalid;
}

static void RegisterUtilFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityName, "955ca6c4-5b0a-4150-aaba-79e2939e85f7"_cry_guid, "GetEntityName");
		pFunction->SetDescription("Get name of entity");
		pFunction->BindInput(1, 'ent', "EntityId");
		pFunction->BindOutput(0, 'name', "Name");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityObjectId, "cdd011ac-14a1-403b-af21-28d9e77e562d"_cry_guid, "GetEntityObjectId");
		pFunction->SetDescription("Get object id from entity");
		pFunction->BindInput(1, 'ent', "EntityId");
		pFunction->BindOutput(0, 'obj', "ObjectId");
		scope.Register(pFunction);
	}
}
} // Entity
} // Schematyc