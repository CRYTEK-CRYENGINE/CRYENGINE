// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvRegistry.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc/Env/Elements/IEnvAction.h>
#include <CrySchematyc/Env/Elements/IEnvClass.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>
#include <CrySchematyc/Env/Elements/IEnvDataType.h>
#include <CrySchematyc/Env/Elements/IEnvInterface.h>
#include <CrySchematyc/Env/Elements/IEnvFunction.h>
#include <CrySchematyc/Env/Elements/IEnvModule.h>
#include <CrySchematyc/Env/Elements/IEnvSignal.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/StackString.h>

namespace Schematyc
{
class CEnvPackageElementRegistrar : public IEnvElementRegistrar, public IEnvRegistrar
{
public:

	inline CEnvPackageElementRegistrar(EnvPackageElements& elements)
		: m_elements(elements)
	{}

	// IEnvElementRegistrar

	virtual void Register(const IEnvElementPtr& pElement, const CryGUID& scopeGUID) override
	{
		SCHEMATYC_CORE_ASSERT(pElement);
		if (pElement)
		{
			m_elements.emplace_back(pElement->GetGUID(), pElement, scopeGUID);
		}
	}

	// ~IEnvElementRegistrar

	// IEnvRegistrar

	virtual CEnvRegistrationScope RootScope() override
	{
		return CEnvRegistrationScope(*this, CryGUID());
	}

	virtual CEnvRegistrationScope Scope(const CryGUID& scopeGUID) override
	{
		return CEnvRegistrationScope(*this, scopeGUID);
	}

	// ~IEnvRegistrar

private:

	EnvPackageElements& m_elements;
};

CEnvRoot::CEnvRoot()
	: CEnvElementBase(CryGUID(), "Root", SCHEMATYC_SOURCE_FILE_INFO)
{}

bool CEnvRoot::IsValidScope(IEnvElement& scope) const
{
	return false;
}

bool CEnvRegistry::RegisterPackage(IEnvPackagePtr&& pPackage)
{
	SCHEMATYC_CORE_ASSERT(pPackage);
	if (!pPackage)
	{
		return false;
	}

	const CryGUID guid = pPackage->GetGUID();
	EnvPackageCallback callback = pPackage->GetCallback();

	SCHEMATYC_CORE_ASSERT(!GUID::IsEmpty(guid) && callback);
	if (GUID::IsEmpty(guid) || !callback)
	{
		return false;
	}

	const bool bUniqueGUID = !GetPackage(guid);
	SCHEMATYC_CORE_ASSERT(bUniqueGUID);
	if (!bUniqueGUID)
	{
		return false;
	}

	SCHEMATYC_CORE_COMMENT("Registering package: name = %s", pPackage->GetName());

	auto emplaceResult = m_packages.emplace((CryGUID)guid, SPackageInfo{ std::forward<IEnvPackagePtr>(pPackage) });
	SPackageInfo& packageInfo = emplaceResult.first->second;
	packageInfo.elements.reserve(100);

	CEnvPackageElementRegistrar packageElementRegistrar(packageInfo.elements);
	callback(packageElementRegistrar);

	if (RegisterPackageElements(packageInfo.elements))
	{
		return true;
	}
	else
	{
		ReleasePackageElements(packageInfo.elements);
		m_packages.erase(emplaceResult.first);

		SCHEMATYC_CORE_CRITICAL_ERROR("Failed to register package!");
		return false;
	}
}

void CEnvRegistry::DeregisterPackage(const CryGUID& guid)
{
	auto packageIt = m_packages.find(guid);
	if (packageIt != m_packages.end())
	{
		ReleasePackageElements(packageIt->second.elements);
		m_packages.erase(packageIt);
	}
}

const IEnvPackage* CEnvRegistry::GetPackage(const CryGUID& guid) const
{
	Packages::const_iterator itPackage = m_packages.find(guid);
	return itPackage != m_packages.end() ? itPackage->second.pPackage.get() : nullptr;
}

void CEnvRegistry::VisitPackages(const EnvPackageConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Packages::value_type& package : m_packages)
		{
			if (visitor(*package.second.pPackage) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvElement& CEnvRegistry::GetRoot() const
{
	return m_root;
}

const IEnvElement* CEnvRegistry::GetElement(const CryGUID& guid) const
{
	Elements::const_iterator itElement = m_elements.find(guid);
	return itElement != m_elements.end() ? itElement->second.get() : nullptr;
}

const IEnvModule* CEnvRegistry::GetModule(const CryGUID& guid) const
{
	Modules::const_iterator itModule = m_modules.find(guid);
	return itModule != m_modules.end() ? itModule->second.get() : nullptr;
}

void CEnvRegistry::VisitModules(const EnvModuleConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Modules::value_type& module : m_modules)
		{
			if (visitor(*module.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvDataType* CEnvRegistry::GetDataType(const CryGUID& guid) const
{
	DataTypes::const_iterator itDataType = m_dataTypes.find(guid);
	return itDataType != m_dataTypes.end() ? itDataType->second.get() : nullptr;
}

void CEnvRegistry::VisitDataTypes(const EnvDataTypeConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const DataTypes::value_type& dataType : m_dataTypes)
		{
			if (visitor(*dataType.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

void CEnvRegistry::RegisterListener(IEnvRegistryListener* pListener)
{
	m_listeners.push_back(pListener);
}

void CEnvRegistry::UnregisterListener(IEnvRegistryListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

const IEnvSignal* CEnvRegistry::GetSignal(const CryGUID& guid) const
{
	Signals::const_iterator itSignal = m_signals.find(guid);
	return itSignal != m_signals.end() ? itSignal->second.get() : nullptr;
}

void CEnvRegistry::VisitSignals(const EnvSignalConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Signals::value_type& signal : m_signals)
		{
			if (visitor(*signal.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvFunction* CEnvRegistry::GetFunction(const CryGUID& guid) const
{
	Functions::const_iterator itFunction = m_functions.find(guid);
	return itFunction != m_functions.end() ? itFunction->second.get() : nullptr;
}

void CEnvRegistry::VisitFunctions(const EnvFunctionConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Functions::value_type& function : m_functions)
		{
			if (visitor(*function.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvClass* CEnvRegistry::GetClass(const CryGUID& guid) const
{
	Classes::const_iterator itClass = m_classes.find(guid);
	return itClass != m_classes.end() ? itClass->second.get() : nullptr;
}

void CEnvRegistry::VisitClasses(const EnvClassConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Classes::value_type& _class : m_classes)
		{
			if (visitor(*_class.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvInterface* CEnvRegistry::GetInterface(const CryGUID& guid) const
{
	Interfaces::const_iterator itInterface = m_interfaces.find(guid);
	return itInterface != m_interfaces.end() ? itInterface->second.get() : nullptr;
}

void CEnvRegistry::VisitInterfaces(const EnvInterfaceConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Interfaces::value_type& envInterface : m_interfaces)
		{
			if (visitor(*envInterface.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvInterfaceFunction* CEnvRegistry::GetInterfaceFunction(const CryGUID& guid) const
{
	InterfaceFunctions::const_iterator itFunction = m_interfaceFunctions.find(guid);
	return itFunction != m_interfaceFunctions.end() ? itFunction->second.get() : nullptr;
}

void CEnvRegistry::VisitInterfaceFunctions(const EnvInterfaceFunctionConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const InterfaceFunctions::value_type& function : m_interfaceFunctions)
		{
			if (visitor(*function.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvComponent* CEnvRegistry::GetComponent(const CryGUID& guid) const
{
	Components::const_iterator itComponent = m_components.find(guid);
	return itComponent != m_components.end() ? itComponent->second.get() : nullptr;
}

void CEnvRegistry::VisitComponents(const EnvComponentConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Components::value_type& component : m_components)
		{
			if (visitor(*component.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

const IEnvAction* CEnvRegistry::GetAction(const CryGUID& guid) const
{
	Actions::const_iterator itAction = m_actions.find(guid);
	return itAction != m_actions.end() ? itAction->second.get() : nullptr;
}

void CEnvRegistry::VisitActions(const EnvActionConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Actions::value_type& action : m_actions)
		{
			if (visitor(*action.second) == EVisitStatus::Stop)
			{
				break;
			}
		}
	}
}

void CEnvRegistry::BlacklistElement(const CryGUID& guid)
{
	m_blacklist.insert(guid);
}

bool CEnvRegistry::IsBlacklistedElement(const CryGUID& guid) const
{
	return m_blacklist.find(guid) != m_blacklist.end();
}

void CEnvRegistry::Refresh()
{
	m_actions.clear();
	m_components.clear();
	m_interfaceFunctions.clear();
	m_interfaces.clear();
	m_classes.clear();
	m_functions.clear();
	m_signals.clear();
	m_dataTypes.clear();
	m_modules.clear();

	m_elements.clear();

	for (const Packages::value_type& package : m_packages)
	{
		// #SchematycTODO : Re-register package elements!!!
	}
}

bool CEnvRegistry::RegisterPackageElements(const EnvPackageElements& packageElements)
{
	m_elements.reserve(m_elements.size() + packageElements.size());

	bool bError = false;

	for (const SEnvPackageElement& packageElement : packageElements)
	{
		if (IEnvElement* pExistingElement = GetElement(packageElement.elementGUID))
		{
			CStackString temp;
			GUID::ToString(temp, packageElement.elementGUID);
			SCHEMATYC_CORE_CRITICAL_ERROR("Duplicate element guid: element = %s, guid = %s - Conflicted with existing element = %s!", packageElement.pElement->GetName(), temp.c_str(), pExistingElement->GetName());
			bError = true;
		}

		m_elements.insert(Elements::value_type(packageElement.elementGUID, packageElement.pElement));

		for (auto listeners : m_listeners)
		{
			listeners->OnEnvElementAdd(packageElement.pElement);
		}

		switch (packageElement.pElement->GetType())
		{
		case EEnvElementType::Module:
			{
				m_modules.insert(Modules::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvModule>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::DataType:
			{
				m_dataTypes.insert(DataTypes::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvDataType>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Signal:
			{
				m_signals.insert(Signals::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvSignal>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Function:
			{
				m_functions.insert(Functions::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvFunction>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Class:
			{
				m_classes.insert(Classes::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvClass>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Interface:
			{
				m_interfaces.insert(Interfaces::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvInterface>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::InterfaceFunction:
			{
				m_interfaceFunctions.insert(InterfaceFunctions::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvInterfaceFunction>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Component:
			{
				m_components.insert(Components::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvComponent>(packageElement.pElement)));
				break;
			}
		case EEnvElementType::Action:
			{
				m_actions.insert(Actions::value_type(packageElement.elementGUID, std::static_pointer_cast<IEnvAction>(packageElement.pElement)));
				break;
			}
		}
	}

	for (const SEnvPackageElement& packageElement : packageElements)
	{
		IEnvElement* pScope = nullptr;
		if (!GUID::IsEmpty(packageElement.scopeGUID))
		{
			pScope = GetElement(packageElement.scopeGUID);
		}
		else
		{
			pScope = &m_root;
		}

		if (pScope)
		{
			const char* szElementName = packageElement.pElement->GetName();
			auto matchElementName = [szElementName](const IEnvElement& childElement) -> EVisitStatus
			{
				return strcmp(childElement.GetName(), szElementName) ? EVisitStatus::Continue : EVisitStatus::Stop;
			};
			if (pScope->VisitChildren(matchElementName) == EVisitStatus::Stop)
			{
				SCHEMATYC_CORE_WARNING("Duplicate element name: element = %s, scope = %s", szElementName, pScope->GetName());
			}
		}

		if (!pScope || !pScope->AttachChild(*packageElement.pElement))
		{
			CStackString temp;
			GUID::ToString(temp, packageElement.elementGUID);
			SCHEMATYC_CORE_CRITICAL_ERROR("Invalid element scope: element = %s, scope = %s", packageElement.pElement->GetName(), temp.c_str());
			bError = true;
		}
	}

	if(!ValidateComponentDependencies())
	{
		bError = true;
	}

	return !bError;
}

void CEnvRegistry::ReleasePackageElements(const EnvPackageElements& packageElements)
{
	for (const SEnvPackageElement& packageElement : packageElements)
	{
		IEnvElement* pScope = packageElement.pElement->GetParent();
		if (pScope)
		{
			pScope->DetachChild(*packageElement.pElement);
		}

		for (auto listeners : m_listeners)
		{
			listeners->OnEnvElementDelete(packageElement.pElement);
		}

		switch (packageElement.pElement->GetType())
		{
		case EEnvElementType::Module:
			{
				m_modules.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::DataType:
			{
				m_dataTypes.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Signal:
			{
				m_signals.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Function:
			{
				m_functions.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Class:
			{
				m_classes.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Interface:
			{
				m_interfaces.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::InterfaceFunction:
			{
				m_interfaceFunctions.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Component:
			{
				m_components.erase(packageElement.elementGUID);
				break;
			}
		case EEnvElementType::Action:
			{
				m_actions.erase(packageElement.elementGUID);
				break;
			}
		}

		m_elements.erase(packageElement.elementGUID);
	}
}

bool CEnvRegistry::ValidateComponentDependencies() const
{
	bool bError = false;

	for (const Components::value_type& component : m_components)
	{
		const CEntityComponentClassDesc& componentDesc = component.second->GetDesc();
		for(const SEntityComponentRequirements& interaction : componentDesc.GetComponentInteractions())
		{
			if (interaction.type != SEntityComponentRequirements::EType::HardDependency)
			{
				continue;
			}

			const IEnvComponent* pDependencyComponent = GetComponent(interaction.guid);
			if (pDependencyComponent)
			{
				const CEntityComponentClassDesc& dependencyComponentDesc = pDependencyComponent->GetDesc();
				if (!dependencyComponentDesc.GetComponentFlags().Check(IEntityComponent::EFlags::Singleton))
				{
					SCHEMATYC_CORE_CRITICAL_ERROR("Non-singleton component detected as dependency: component = %s, dependency = %s", component.second->GetName(), pDependencyComponent->GetName());
					bError = true;
				}
				if (dependencyComponentDesc.DependsOn(component.first))
				{
					SCHEMATYC_CORE_CRITICAL_ERROR("Circular dependency detected between components: component = %s, dependency = %s", component.second->GetName(), pDependencyComponent->GetName());
					bError = true;
				}
			}
			else
			{
				CStackString temp;
				GUID::ToString(temp, interaction.guid);
				SCHEMATYC_CORE_CRITICAL_ERROR("Unable to resolve component dependency: component = %s, dependency = %s", component.second->GetName(), temp.c_str());
				bError = true;
			}
		}
	}

	return !bError;
}

IEnvElement* CEnvRegistry::GetElement(const CryGUID& guid)
{
	Elements::iterator itElement = m_elements.find(guid);
	return itElement != m_elements.end() ? itElement->second.get() : nullptr;
}
} // Schematyc
