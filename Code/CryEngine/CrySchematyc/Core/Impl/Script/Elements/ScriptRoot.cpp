// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptRoot.h"

namespace Schematyc
{
CScriptRoot::CScriptRoot()
	: CScriptElementBase(CryGUID(), "Root", EScriptElementFlags::FixedName)
{}

void CScriptRoot::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const {}

void CScriptRoot::RemapDependencies(IGUIDRemapper& guidRemapper)                                                        {}

void CScriptRoot::ProcessEvent(const SScriptEvent& event)                                                               {}

void CScriptRoot::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}
} // Schematyc
