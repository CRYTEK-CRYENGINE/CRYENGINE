// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

namespace Schematyc
{

// Forward declare interfaces.
struct IObjectProperties;
// Forward declare classes.
class CAnyRef;
class CAnyConstRef;
class CClassProperties;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IObjectProperties)

struct IObjectProperties
{
	virtual ~IObjectProperties() {}

	virtual IObjectPropertiesPtr    Clone() const = 0;
	virtual void                    Serialize(Serialization::IArchive& archive) = 0;
	virtual void                    SerializeVariables(Serialization::IArchive& archive) = 0;
	virtual const CClassProperties* GetComponentProperties(const CryGUID& guid) const = 0;
	virtual CClassProperties*       GetComponentProperties(const CryGUID& guid) = 0;
	virtual bool                    ReadVariable(const CAnyRef& value, const CryGUID& guid) const = 0;

	virtual void                    AddComponent(const CryGUID& guid, const char* szName, const CClassProperties& properties) = 0;
	virtual void                    AddVariable(const CryGUID& guid, const char* szName, const CAnyConstRef& value) = 0;

	virtual bool                    HasVariables() const = 0;
};

} // Schematyc
