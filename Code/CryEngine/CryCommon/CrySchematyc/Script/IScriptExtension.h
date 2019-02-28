// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "CrySchematyc/Script/ScriptDependencyEnumerator.h"

namespace Schematyc
{
// Forward declare structures.

struct SScriptEvent;
// Froward declare interfaces.
struct IGUIDRemapper;
struct IScriptElement;

enum EScriptExtensionType
{
	Graph
};

struct IScriptExtension
{
	virtual ~IScriptExtension() {}

	virtual EScriptExtensionType  GetExtensionType() const = 0;
	virtual IScriptElement&       GetElement() = 0;
	virtual const IScriptElement& GetElement() const = 0;
	virtual void                  EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const = 0;
	virtual void                  RemapDependencies(IGUIDRemapper& guidRemapper) = 0;
	virtual void                  ProcessEvent(const SScriptEvent& event) = 0;
	virtual void                  Serialize(Serialization::IArchive& archive) = 0;
};

template<EScriptExtensionType EXTENSION_TYPE> struct IScriptExtensionBase : public IScriptExtension
{
	static const EScriptExtensionType ExtensionType = EXTENSION_TYPE;

	// IScriptExtension

	virtual EScriptExtensionType GetExtensionType() const override
	{
		return ExtensionType;
	}

	// ~IScriptExtension
};

namespace ScriptExtension
{
template<typename TYPE> inline TYPE& Cast(IScriptExtension& scriptExtension)
{
	SCHEMATYC_CORE_ASSERT(scriptExtension.GetExtensionType() == TYPE::ExtensionType);
	return static_cast<TYPE&>(scriptExtension);
}

template<typename TYPE> inline const TYPE& Cast(const IScriptExtension& scriptExtension)
{
	SCHEMATYC_CORE_ASSERT(scriptExtension.GetExtensionType() == TYPE::ExtensionType);
	return static_cast<const TYPE&>(scriptExtension);
}

template<typename TYPE> inline TYPE* Cast(IScriptExtension* pScriptExtension)
{
	return pScriptExtension && (pScriptExtension->GetExtensionType() == TYPE::ExtensionType) ? static_cast<TYPE*>(pScriptExtension) : nullptr;
}

template<typename TYPE> inline const TYPE* Cast(const IScriptExtension* pScriptExtension)
{
	return pScriptExtension && (pScriptExtension->GetExtensionType() == TYPE::ExtensionType) ? static_cast<TYPE*>(pScriptExtension) : nullptr;
}
}

struct IScriptExtensionMap
{
	virtual ~IScriptExtensionMap() {}

	virtual IScriptExtension*            QueryExtension(EScriptExtensionType type) = 0;
	virtual const IScriptExtension*      QueryExtension(EScriptExtensionType type) const = 0;
	virtual void                         EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const = 0;
	virtual void                         ProcessEvent(const SScriptEvent& event) = 0;
	virtual void                         Serialize(Serialization::IArchive& archive) = 0;
	virtual void                         RemapDependencies(IGUIDRemapper& guidRemapper) = 0;

	template<typename TYPE> inline TYPE* QueryExtension()
	{
		return static_cast<TYPE*>(QueryExtension(TYPE::ExtensionType));
	}

	template<typename TYPE> inline const TYPE* QueryExtension() const
	{
		return static_cast<const TYPE*>(QueryExtension(TYPE::ExtensionType));
	}
};
} // Schematyc
