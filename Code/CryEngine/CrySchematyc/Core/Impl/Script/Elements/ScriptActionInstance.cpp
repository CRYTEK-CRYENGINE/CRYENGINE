// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptActionInstance.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvAction.h>
#include <CrySchematyc/Env/Elements/IEnvInterface.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/SerializationUtils.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

namespace Schematyc
{

CScriptActionInstance::CScriptActionInstance() {}

CScriptActionInstance::CScriptActionInstance(const CryGUID& guid, const char* szName, const CryGUID& actionTypeGUID, const CryGUID& componentInstanceGUID)
	: CScriptElementBase(guid, szName)
	, m_actionTypeGUID(actionTypeGUID)
	, m_componentInstanceGUID(componentInstanceGUID)
{
	RefreshProperties();
}

void CScriptActionInstance::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const {}

void CScriptActionInstance::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_componentInstanceGUID = guidRemapper.Remap(m_componentInstanceGUID);
}

void CScriptActionInstance::ProcessEvent(const SScriptEvent& event) {}

void CScriptActionInstance::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

CryGUID CScriptActionInstance::GetActionTypeGUID() const
{
	return m_actionTypeGUID;
}

CryGUID CScriptActionInstance::GetComponentInstanceGUID() const
{
	return m_componentInstanceGUID;
}

const CClassProperties& CScriptActionInstance::GetProperties() const
{
	return m_properties;
}

void CScriptActionInstance::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_actionTypeGUID, "actionTypeGUID");
	archive(m_componentInstanceGUID, "componentInstanceGUID");
}

void CScriptActionInstance::Load(Serialization::IArchive& archive, const ISerializationContext& context)
{
	RefreshProperties();
	SerializeProperties(archive);
}

void CScriptActionInstance::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_actionTypeGUID, "actionTypeGUID");
	archive(m_componentInstanceGUID, "componentInstanceGUID");
	SerializeProperties(archive);
}

void CScriptActionInstance::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (archive.isOutput())
	{
		const IEnvAction* pEnvAction = gEnv->pSchematyc->GetEnvRegistry().GetAction(m_actionTypeGUID);
		if (pEnvAction)
		{
			string action = pEnvAction->GetName();
			archive(action, "action", "!Action");
		}
	}
	SerializeProperties(archive);
}

void CScriptActionInstance::RefreshProperties()
{
	// #SchematycTODO : Implement!!!
}

void CScriptActionInstance::SerializeProperties(Serialization::IArchive& archive)
{
	// #SchematycTODO : Implement!!!
}

} // Schematyc
