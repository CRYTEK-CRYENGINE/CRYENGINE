// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Env/EnvElementBase.h>
#include <CrySchematyc/Env/IEnvPackage.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/IEnvRegistry.h>

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvAction;
struct IEnvClass;
struct IEnvComponent;
struct IEnvDataType;
struct IEnvElement;
struct IEnvFunction;
struct IEnvInterface;
struct IEnvInterfaceFunction;
struct IEnvModule;
struct IEnvSignal;

// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IEnvAction)
DECLARE_SHARED_POINTERS(IEnvClass)
DECLARE_SHARED_POINTERS(IEnvComponent)
DECLARE_SHARED_POINTERS(IEnvDataType)
DECLARE_SHARED_POINTERS(IEnvElement)
DECLARE_SHARED_POINTERS(IEnvFunction)
DECLARE_SHARED_POINTERS(IEnvInterface)
DECLARE_SHARED_POINTERS(IEnvInterfaceFunction)
DECLARE_SHARED_POINTERS(IEnvModule)
DECLARE_SHARED_POINTERS(IEnvSignal)

struct SEnvPackageElement
{
	inline SEnvPackageElement(const CryGUID& _elementGUID, const IEnvElementPtr& _pElement, const CryGUID& _scopeGUID)
		: elementGUID(_elementGUID)
		, pElement(_pElement)
		, scopeGUID(_scopeGUID)
	{}

	CryGUID          elementGUID;
	IEnvElementPtr   pElement;
	CryGUID          scopeGUID;
};

typedef std::vector<SEnvPackageElement> EnvPackageElements;

struct IEnvRoot : public IEnvElementBase<EEnvElementType::Root>
{
	virtual ~IEnvRoot() {}
};

class CEnvRoot : public CEnvElementBase<IEnvRoot>
{
public:

	CEnvRoot();

	// IEnvElement
	virtual bool IsValidScope(IEnvElement& scope) const override;
	// ~IEnvElement
};

class CEnvRegistry : public IEnvRegistry
{
private:

	struct SPackageInfo
	{
		IEnvPackagePtr pPackage;
		EnvPackageElements elements;
	};

	typedef std::unordered_map<CryGUID, SPackageInfo>                  Packages;
	typedef std::unordered_map<CryGUID, IEnvElementPtr>                Elements;
	typedef std::unordered_map<CryGUID, IEnvModulePtr>                 Modules;
	typedef std::unordered_map<CryGUID, IEnvDataTypePtr>               DataTypes;
	typedef std::unordered_map<CryGUID, IEnvSignalPtr>                 Signals;
	typedef std::unordered_map<CryGUID, IEnvFunctionPtr>               Functions;
	typedef std::unordered_map<CryGUID, IEnvClassPtr>                  Classes;
	typedef std::unordered_map<CryGUID, IEnvInterfacePtr>              Interfaces;
	typedef std::unordered_map<CryGUID, IEnvInterfaceFunctionConstPtr> InterfaceFunctions;
	typedef std::unordered_map<CryGUID, IEnvComponentPtr>              Components;
	typedef std::unordered_map<CryGUID, IEnvActionPtr>                 Actions;
	typedef std::unordered_set<CryGUID>                                Blacklist;

public:

	// IEnvRegistry

	virtual bool                         RegisterPackage(IEnvPackagePtr&& pPackage) override;
	virtual void                         DeregisterPackage(const CryGUID& guid) override;
	virtual const IEnvPackage*           GetPackage(const CryGUID& guid) const override;
	virtual void                         VisitPackages(const EnvPackageConstVisitor& visitor) const override;

	virtual void                         RegisterListener(IEnvRegistryListener* pListener) override;
	virtual void                         UnregisterListener(IEnvRegistryListener* pListener) override;

	virtual const IEnvElement&           GetRoot() const override;
	virtual const IEnvElement*           GetElement(const CryGUID& guid) const override;

	virtual const IEnvModule*            GetModule(const CryGUID& guid) const override;
	virtual void                         VisitModules(const EnvModuleConstVisitor& visitor) const override;

	virtual const IEnvDataType*          GetDataType(const CryGUID& guid) const override;
	virtual void                         VisitDataTypes(const EnvDataTypeConstVisitor& visitor) const override;

	virtual const IEnvSignal*            GetSignal(const CryGUID& guid) const override;
	virtual void                         VisitSignals(const EnvSignalConstVisitor& visitor) const override;

	virtual const IEnvFunction*          GetFunction(const CryGUID& guid) const override;
	virtual void                         VisitFunctions(const EnvFunctionConstVisitor& visitor) const override;

	virtual const IEnvClass*             GetClass(const CryGUID& guid) const override;
	virtual void                         VisitClasses(const EnvClassConstVisitor& visitor) const override;

	virtual const IEnvInterface*         GetInterface(const CryGUID& guid) const override;
	virtual void                         VisitInterfaces(const EnvInterfaceConstVisitor& visitor) const override;

	virtual const IEnvInterfaceFunction* GetInterfaceFunction(const CryGUID& guid) const override;
	virtual void                         VisitInterfaceFunctions(const EnvInterfaceFunctionConstVisitor& visitor) const override;

	virtual const IEnvComponent*         GetComponent(const CryGUID& guid) const override;
	virtual void                         VisitComponents(const EnvComponentConstVisitor& visitor) const override;

	virtual const IEnvAction*            GetAction(const CryGUID& guid) const override;
	virtual void                         VisitActions(const EnvActionConstVisitor& visitor) const override;

	virtual void                         BlacklistElement(const CryGUID& guid) override;
	virtual bool                         IsBlacklistedElement(const CryGUID& guid) const override;

	// ~IEnvRegistry

	void Refresh();

private:

	bool         RegisterPackageElements(const EnvPackageElements& packageElements);
	void         ReleasePackageElements(const EnvPackageElements& packageElements);

	bool         ValidateComponentDependencies() const;

	IEnvElement* GetElement(const CryGUID& guid);

private:

	TEnvRegistryListeners m_listeners;

	Packages           m_packages;

	CEnvRoot           m_root;
	Elements           m_elements;

	Modules            m_modules;
	DataTypes          m_dataTypes;
	Signals            m_signals;
	Functions          m_functions;
	Classes            m_classes;
	Interfaces         m_interfaces;
	InterfaceFunctions m_interfaceFunctions;
	Components         m_components;
	Actions            m_actions;

	Blacklist          m_blacklist;
};
} // Schematyc
