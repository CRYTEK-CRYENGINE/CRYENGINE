// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CryCore/BoostHelpers.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
struct ISignal;
struct ILibAbstractInterface;
struct ILibAbstractInterfaceFunction;
struct ILibClass;
struct ILib;

DECLARE_SHARED_POINTERS(ISignal)
DECLARE_SHARED_POINTERS(ILibAbstractInterface)
DECLARE_SHARED_POINTERS(ILibAbstractInterfaceFunction)
DECLARE_SHARED_POINTERS(ILibClass)
DECLARE_SHARED_POINTERS(ILib)

typedef TemplateUtils::CDelegate<EVisitStatus(const ISignalConstPtr&)>                       LibSignalVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibAbstractInterfaceConstPtr&)>         ILibAbstractInterfaceVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibAbstractInterfaceFunctionConstPtr&)> ILibAbstractInterfaceFunctionVisitor;
typedef TemplateUtils::CDelegate<EVisitStatus(const ILibClassConstPtr&)>                     ILibClassVisitor;

typedef TemplateUtils::CSignalv2<void (const ILibClassConstPtr&)>                            LibClassRegistrationSignal;

struct SLibRegistrySignals
{
	LibClassRegistrationSignal classRegistration;
};

struct ILibRegistry
{
	virtual ~ILibRegistry() {}

	virtual ISignalConstPtr                       GetSignal(const SGUID& guid) const = 0;
	virtual void                                  VisitSignals(const LibSignalVisitor& visitor) const = 0;
	virtual ILibAbstractInterfaceConstPtr         GetAbstractInterface(const SGUID& guid) const = 0;
	virtual void                                  VisitAbstractInterfaces(const ILibAbstractInterfaceVisitor& visitor) = 0;
	virtual ILibAbstractInterfaceFunctionConstPtr GetAbstractInterfaceFunction(const SGUID& guid) const = 0;
	virtual void                                  VisitAbstractInterfaceFunctions(const ILibAbstractInterfaceFunctionVisitor& visitor) = 0;
	virtual SGUID                                 FindClass(const char* name) const = 0;
	virtual ILibClassConstPtr                     GetClass(const SGUID& guid) const = 0;
	virtual void                                  VisitClasses(const ILibClassVisitor& visitor) const = 0;
	virtual ILibPtr                               GetLib(const char* name) = 0;
	virtual bool                                  RegisterLib(const ILibPtr& pLib) = 0;
	virtual SLibRegistrySignals&                  Signals() = 0;
};
}
