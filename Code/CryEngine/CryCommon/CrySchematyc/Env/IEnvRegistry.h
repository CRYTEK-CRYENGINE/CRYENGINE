// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Env/Elements/IEnvInterface.h"
#include "CrySchematyc/Network/INetworkSpawnParams.h"
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/TypeUtils.h"

namespace Schematyc
{

// Forward declare interfaces.
struct IEnvAction;
struct IEnvClass;
struct IEnvComponent;
struct IEnvDataType;
struct IEnvFunction;
struct IEnvModule;
struct IEnvPackage;
struct IEnvRegistrar;
struct IEnvSignal;

typedef std::unique_ptr<IEnvPackage>                          IEnvPackagePtr;
typedef std::shared_ptr<IEnvElement>                          IEnvElementPtr;

typedef std::function<EVisitStatus(const IEnvPackage&)>           EnvPackageConstVisitor;
typedef std::function<EVisitStatus(const IEnvModule&)>            EnvModuleConstVisitor;
typedef std::function<EVisitStatus(const IEnvDataType&)>          EnvDataTypeConstVisitor;
typedef std::function<EVisitStatus(const IEnvSignal&)>            EnvSignalConstVisitor;
typedef std::function<EVisitStatus(const IEnvFunction&)>          EnvFunctionConstVisitor;
typedef std::function<EVisitStatus(const IEnvClass&)>             EnvClassConstVisitor;
typedef std::function<EVisitStatus(const IEnvInterface&)>         EnvInterfaceConstVisitor;
typedef std::function<EVisitStatus(const IEnvInterfaceFunction&)> EnvInterfaceFunctionConstVisitor;
typedef std::function<EVisitStatus(const IEnvComponent&)>         EnvComponentConstVisitor;
typedef std::function<EVisitStatus(const IEnvAction&)>            EnvActionConstVisitor;

struct IEnvRegistryListener
{
	virtual ~IEnvRegistryListener() {}
	virtual void OnEnvElementAdd(IEnvElementPtr pElement) = 0;
	virtual void OnEnvElementDelete(IEnvElementPtr pElement) = 0;
};

typedef std::vector<IEnvRegistryListener*> TEnvRegistryListeners;

struct IEnvRegistry
{
	virtual ~IEnvRegistry() {}

	virtual void                         RegisterListener(IEnvRegistryListener* pListener) = 0;
	virtual void                         UnregisterListener(IEnvRegistryListener* pListener) = 0;

	virtual bool                         RegisterPackage(IEnvPackagePtr&& pPackage) = 0;
	virtual void                         DeregisterPackage(const CryGUID& guid) = 0;
	virtual const IEnvPackage*           GetPackage(const CryGUID& guid) const = 0;
	virtual void                         VisitPackages(const EnvPackageConstVisitor& visitor) const = 0;

	virtual const IEnvElement&           GetRoot() const = 0;
	virtual const IEnvElement*           GetElement(const CryGUID& guid) const = 0;

	virtual const IEnvModule*            GetModule(const CryGUID& guid) const = 0;
	virtual void                         VisitModules(const EnvModuleConstVisitor& visitor) const = 0;

	virtual const IEnvDataType*          GetDataType(const CryGUID& guid) const = 0;
	virtual void                         VisitDataTypes(const EnvDataTypeConstVisitor& visitor) const = 0;

	virtual const IEnvSignal*            GetSignal(const CryGUID& guid) const = 0;
	virtual void                         VisitSignals(const EnvSignalConstVisitor& visitor) const = 0;

	virtual const IEnvFunction*          GetFunction(const CryGUID& guid) const = 0;
	virtual void                         VisitFunctions(const EnvFunctionConstVisitor& visitor) const = 0;

	virtual const IEnvClass*             GetClass(const CryGUID& guid) const = 0;
	virtual void                         VisitClasses(const EnvClassConstVisitor& visitor) const = 0;

	virtual const IEnvInterface*         GetInterface(const CryGUID& guid) const = 0;
	virtual void                         VisitInterfaces(const EnvInterfaceConstVisitor& visitor) const = 0;

	virtual const IEnvInterfaceFunction* GetInterfaceFunction(const CryGUID& guid) const = 0;
	virtual void                         VisitInterfaceFunctions(const EnvInterfaceFunctionConstVisitor& visitor) const = 0;

	virtual const IEnvComponent*         GetComponent(const CryGUID& guid) const = 0;
	virtual void                         VisitComponents(const EnvComponentConstVisitor& visitor) const = 0;

	virtual const IEnvAction*            GetAction(const CryGUID& guid) const = 0;
	virtual void                         VisitActions(const EnvActionConstVisitor& visitor) const = 0;

	virtual void                         BlacklistElement(const CryGUID& guid) = 0;
	virtual bool                         IsBlacklistedElement(const CryGUID& guid) const = 0;
};

} // Schematyc
