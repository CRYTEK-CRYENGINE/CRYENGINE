// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Runtime/IRuntimeRegistry.h>
#include <CrySchematyc/Utils/GUID.h>

#ifdef RegisterClass
#undef RegisterClass
#endif

namespace Schematyc
{
// Forward declare classes.
class CRuntimeClass;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CRuntimeClass)

class CRuntimeRegistry : public IRuntimeRegistry
{
private:

	typedef std::unordered_map<CryGUID, CRuntimeClassConstPtr> Classes;

public:

	// IRuntimeRegistry
	virtual IRuntimeClassConstPtr GetClass(const CryGUID& guid) const override;
	// ~IRuntimeRegistry

	void                  RegisterClass(const CRuntimeClassConstPtr& pClass);
	void                  ReleaseClass(const CryGUID& guid);
	CRuntimeClassConstPtr GetClassImpl(const CryGUID& guid) const;

	void                  Reset();

private:

	Classes m_classes;
};
} // Schematyc
