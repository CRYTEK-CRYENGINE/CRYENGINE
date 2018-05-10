// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #Schematyc : Ditch visit status support or implement correct visitor status handling!!!

#include "StdAfx.h"
#include "ScriptView.h"

#include <CrySchematyc/FundamentalTypes.h>
#include <CrySchematyc/ICore.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvAction.h>
#include <CrySchematyc/Env/Elements/IEnvClass.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>
#include <CrySchematyc/Env/Elements/IEnvDataType.h>
#include <CrySchematyc/Env/Elements/IEnvFunction.h>
#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptActionInstance.h>
#include <CrySchematyc/Script/Elements/IScriptBase.h>
#include <CrySchematyc/Script/Elements/IScriptClass.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Script/Elements/IScriptEnum.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Script/Elements/IScriptModule.h>
#include <CrySchematyc/Script/Elements/IScriptSignal.h>
#include <CrySchematyc/Script/Elements/IScriptState.h>
#include <CrySchematyc/Script/Elements/IScriptStateMachine.h>
#include <CrySchematyc/Script/Elements/IScriptStruct.h>
#include <CrySchematyc/Script/Elements/IScriptTimer.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include "CVars.h"
#include "Services/Log.h"

namespace Schematyc
{
namespace ScriptViewUtils_DEPRECATED
{
enum class EVisitFlags
{
	VisitParents  = BIT(0),
	VisitChildren = BIT(1)
};

typedef CEnumFlags<EVisitFlags> VisitFlags;

inline const IScriptClass* GetScriptBaseClass(const IScriptElement& scriptElement)
{
	SElementId baseClassId;
	auto visitScriptElement = [&baseClassId](const IScriptElement& scriptElement) -> EVisitStatus
	{
		if (scriptElement.GetType() == EScriptElementType::Base)
		{
			baseClassId = DynamicCast<IScriptBase>(scriptElement).GetClassId();
			return EVisitStatus::Stop;
		}
		return EVisitStatus::Continue;
	};
	scriptElement.VisitChildren(visitScriptElement);
	return baseClassId.domain == EDomain::Script ? DynamicCast<IScriptClass>(gEnv->pSchematyc->GetScriptRegistry().GetElement(baseClassId.guid)) : nullptr;
}

// #SchematycTODO : No need to pass element type, this can be retrieved using ELEMENT::ElementType.
template<typename ELEMENT> void VisitScriptElements(const std::function<EVisitStatus(const ELEMENT&)>& visitor, EScriptElementType filterElementType, EScriptElementAccessor accessor, const IScriptElement& scriptScope)
{
	for (const IScriptElement* pScriptElement = scriptScope.GetFirstChild(); pScriptElement; pScriptElement = pScriptElement->GetNextSibling())
	{
		if (pScriptElement->GetAccessor() <= accessor)
		{
			VisitScriptElements<ELEMENT>(visitor, filterElementType, accessor, *pScriptElement);
			if (pScriptElement->GetType() == filterElementType)
			{
				if (visitor(static_cast<const ELEMENT&>(*pScriptElement)) == EVisitStatus::Stop)
				{
					break;
				}
			}
		}
	}
}

// #SchematycTODO : No need to pass element type, this can be retrieved using ELEMENT::ElementType.
template<typename ELEMENT> void VisitScriptElementsInClass(const std::function<EVisitStatus(const ELEMENT&)>& visitor, EScriptElementType elementType, EDomainScope scope, CryGUID scopeGUID)
{
	const IScriptElement* pScriptElement = gEnv->pSchematyc->GetScriptRegistry().GetElement(scopeGUID);
	while (pScriptElement)
	{
		VisitScriptElements<ELEMENT>(visitor, elementType, EScriptElementAccessor::Private, *pScriptElement);
		if (pScriptElement->GetType() != EScriptElementType::Class)
		{
			pScriptElement = pScriptElement->GetParent();
		}
		else
		{
			break;
		}
	}
	if (pScriptElement && ((scope == EDomainScope::Derived) || ((scope == EDomainScope::All))))
	{
		const EScriptElementAccessor accessor = scope == EDomainScope::Derived ? EScriptElementAccessor::Protected : EScriptElementAccessor::Private;
		for (const IScriptClass* pScriptBaseClass = GetScriptBaseClass(*pScriptElement); pScriptBaseClass; pScriptBaseClass = GetScriptBaseClass(*pScriptBaseClass))
		{
			VisitScriptElements<ELEMENT>(visitor, elementType, accessor, *pScriptBaseClass);
		}
	}
}

template<typename ELEMENT> void VisitScriptElementsInModule(const std::function<EVisitStatus(const ELEMENT&)>& visitor, EScriptElementType elementType, CryGUID scopeGUID)
{
	const IScriptElement* pScriptElement = gEnv->pSchematyc->GetScriptRegistry().GetElement(scopeGUID);
	if (pScriptElement)
	{
		VisitScriptElements<ELEMENT>(visitor, elementType, EScriptElementAccessor::Public, *pScriptElement);
	}
}

template<typename ELEMENT> void VisitScriptElementsRecursive(const std::function<EVisitStatus(const ELEMENT&)>& visitor, const IScriptElement& scope, EScriptElementType type, EScriptElementAccessor accessor, const VisitFlags& flags, const IScriptElement* pSkipElement)
{
	for (const IScriptElement* pScriptElement = scope.GetFirstChild(); pScriptElement; pScriptElement = pScriptElement->GetNextSibling())
	{
		if (pScriptElement != pSkipElement)
		{
			const EScriptElementType elementType = pScriptElement->GetType();

			if ((elementType == type) && (pScriptElement->GetAccessor() <= accessor))
			{
				if (visitor(static_cast<const ELEMENT&>(*pScriptElement)) == EVisitStatus::Stop)
				{
					break;
				}
			}

			if (flags.Check(EVisitFlags::VisitChildren))
			{
				VisitScriptElementsRecursive<ELEMENT>(visitor, *pScriptElement, type, accessor, EVisitFlags::VisitChildren, nullptr);
			}

			switch (elementType)
			{
			case EScriptElementType::Base:
				{
					const SElementId baseClassId = DynamicCast<IScriptBase>(pScriptElement)->GetClassId();
					if (baseClassId.domain == EDomain::Script)
					{
						const IScriptElement* pScriptClass = gEnv->pSchematyc->GetScriptRegistry().GetElement(baseClassId.guid);
						if (pScriptClass)
						{
							VisitScriptElementsRecursive<ELEMENT>(visitor, *pScriptClass, type, EScriptElementAccessor::Protected, EVisitFlags::VisitChildren, nullptr);
						}
					}
					break;
				}
			}
		}
	}

	if (flags.Check(EVisitFlags::VisitParents))
	{
		const IScriptElement* pParentScriptElement = scope.GetParent();
		if (pParentScriptElement)
		{
			VisitScriptElementsRecursive<ELEMENT>(visitor, *pParentScriptElement, type, accessor, EVisitFlags::VisitParents, &scope);
		}
	}
}

template<typename ELEMENT> void VisitScriptElements(const std::function<EVisitStatus(const ELEMENT&)>& visitor, EScriptElementType type, const CryGUID& scopeGUID)
{
	const IScriptElement* pScriptScope = gEnv->pSchematyc->GetScriptRegistry().GetElement(scopeGUID);
	if (pScriptScope)
	{
		VisitScriptElementsRecursive<ELEMENT>(visitor, *pScriptScope, type, EScriptElementAccessor::Private, { EVisitFlags::VisitParents, EVisitFlags::VisitChildren }, nullptr);
	}
}

template<typename ELEMENT> void VisitRootScriptElements(const std::function<EVisitStatus(const ELEMENT&)>& visitor, EScriptElementType type)
{
	const IScriptElement& rootElement = gEnv->pSchematyc->GetScriptRegistry().GetRootElement();
	VisitScriptElementsRecursive<ELEMENT>(visitor, rootElement, type, EScriptElementAccessor::Public, {}, nullptr);
}
} // ScriptViewUtils_DEPRECATED

namespace ScriptViewUtils
{
enum class EVisitFlags
{
	VisitParents  = BIT(0),
	VisitChildren = BIT(1)
};

typedef CEnumFlags<EVisitFlags> VisitFlags;

template<typename ELEMENT> void VisitElementsRecursive(const IScriptElement& scope, const std::function<void(const ELEMENT&)>& visitor, EScriptElementAccessor accessor, const VisitFlags& flags, const IScriptElement* pSkipElement)
{
	for (const IScriptElement* pScriptElement = scope.GetFirstChild(); pScriptElement; pScriptElement = pScriptElement->GetNextSibling())
	{
		if (pScriptElement != pSkipElement)
		{
			if (accessor >= pScriptElement->GetAccessor())
			{
				const ELEMENT* pScriptElementImpl = DynamicCast<ELEMENT>(pScriptElement);
				if (pScriptElementImpl)
				{
					visitor(*pScriptElementImpl);
				}

				if (flags.Check(EVisitFlags::VisitChildren))
				{
					VisitElementsRecursive<ELEMENT>(*pScriptElement, visitor, EScriptElementAccessor::Public, EVisitFlags::VisitChildren, nullptr);
				}

				if (accessor >= EScriptElementAccessor::Protected)
				{
					switch (pScriptElement->GetType())
					{
					case EScriptElementType::Base:
						{
							const SElementId baseClassId = DynamicCast<IScriptBase>(pScriptElement)->GetClassId();
							if (baseClassId.domain == EDomain::Script)
							{
								const IScriptElement* pScriptClass = gEnv->pSchematyc->GetScriptRegistry().GetElement(baseClassId.guid);
								if (pScriptClass)
								{
									VisitElementsRecursive<ELEMENT>(*pScriptClass, visitor, EScriptElementAccessor::Protected, EVisitFlags::VisitChildren, nullptr);
								}
							}
							break;
						}
					}
				}
			}
		}
	}

	if (flags.Check(EVisitFlags::VisitParents))
	{
		const IScriptElement* pParentScriptElement = scope.GetParent();
		if (pParentScriptElement)
		{
			VisitElementsRecursive<ELEMENT>(*pParentScriptElement, visitor, accessor, flags, &scope);
		}
	}
}

template<typename ELEMENT> void VisitEnclosedElememts(const CryGUID& scopeGUID, const std::function<void(const ELEMENT&)>& visitor)
{
	const IScriptElement* pScriptScope = gEnv->pSchematyc->GetScriptRegistry().GetElement(scopeGUID);
	if (pScriptScope)
	{
		VisitElementsRecursive<ELEMENT>(*pScriptScope, visitor, EScriptElementAccessor::Private, EVisitFlags::VisitChildren, nullptr);
	}
}

template<typename ELEMENT> void VisitAccesibleElememts(const CryGUID& scopeGUID, const std::function<void(const ELEMENT&)>& visitor)
{
	const IScriptElement* pScriptScope = gEnv->pSchematyc->GetScriptRegistry().GetElement(scopeGUID);
	if (pScriptScope)
	{
		VisitElementsRecursive<ELEMENT>(*pScriptScope, visitor, EScriptElementAccessor::Private, { EVisitFlags::VisitParents, EVisitFlags::VisitChildren }, nullptr);
	}
}
} //ScriptViewUtils

CScriptView::CScriptView(const CryGUID& scopeGUID)
	: m_scopeGUID(scopeGUID)
{}

const CryGUID& CScriptView::GetScopeGUID() const
{
	return m_scopeGUID;
}

const IEnvClass* CScriptView::GetEnvClass() const
{
	IScriptRegistry& scriptRegistry = gEnv->pSchematyc->GetScriptRegistry();
	const IScriptClass* pScriptClass = GetScriptClass();
	SElementId baseClassId;
	while (pScriptClass)
	{
		auto visitScriptElement = [&baseClassId](const IScriptElement& scriptElement) -> EVisitStatus
		{
			if (scriptElement.GetType() == EScriptElementType::Base)
			{
				baseClassId = DynamicCast<IScriptBase>(scriptElement).GetClassId();
				return EVisitStatus::Stop;
			}
			return EVisitStatus::Continue;
		};
		pScriptClass->VisitChildren(visitScriptElement);
		if (baseClassId.domain == EDomain::Script)
		{
			pScriptClass = DynamicCast<IScriptClass>(scriptRegistry.GetElement(baseClassId.guid));
		}
		else
		{
			break;
		}
	}
	return baseClassId.domain == EDomain::Env ? gEnv->pSchematyc->GetEnvRegistry().GetClass(baseClassId.guid) : nullptr;
}

const IScriptClass* CScriptView::GetScriptClass() const
{
	const IScriptElement* pScriptElement = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_scopeGUID);
	while (pScriptElement)
	{
		if (pScriptElement->GetType() == EScriptElementType::Class)
		{
			return DynamicCast<IScriptClass>(pScriptElement);
		}
		else
		{
			pScriptElement = pScriptElement->GetParent();
		}
	}
	return nullptr;
}

void CScriptView::VisitEnvDataTypes(const EnvDataTypeConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor)
	if (visitor)
	{
		//const IEnvClass* pEnvClass = GetEnvClass();
		auto visitEnvDataType = [&visitor /*, &pEnvClass*/](const IEnvDataType& envDataType) -> EVisitStatus
		{
			//if(!pEnvClass || ScriptViewUtils::IsEnvClassUsingNamespace(*pEnvClass, envDataType.GetNamespace()))
			{
				return visitor(envDataType);
			}
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc->GetEnvRegistry().VisitDataTypes(visitEnvDataType);
	}
}

void CScriptView::VisitEnvSignals(const EnvSignalConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor)
	if (visitor)
	{
		//const IEnvSignal* pEnvClass = GetEnvClass();
		auto visitEnvSignal = [&visitor /*, &pEnvClass*/](const IEnvSignal& envSignal) -> EVisitStatus
		{
			//if(!pEnvClass || ScriptViewUtils::IsEnvClassUsingNamespace(*pEnvClass, envSignal.GetNamespace()))
			{
				return visitor(envSignal);
			}
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc->GetEnvRegistry().VisitSignals(visitEnvSignal);
	}
}

void CScriptView::VisitEnvFunctions(const EnvFunctionConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor)
	if (visitor)
	{
		auto visitEnvFunction = [&visitor](const IEnvFunction& envFunction) -> EVisitStatus
		{
			// #SchematycTODO : Filter based on scope!
			return visitor(envFunction);
		};
		gEnv->pSchematyc->GetEnvRegistry().VisitFunctions(visitEnvFunction);
	}
}

void CScriptView::VisitEnvClasses(const EnvClassConstVisitor& visitor) const
{
	gEnv->pSchematyc->GetEnvRegistry().VisitClasses(visitor);
}

void CScriptView::VisitEnvInterfaces(const EnvInterfaceConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor)
	if (visitor)
	{
		const IEnvClass* pEnvClass = GetEnvClass();
		auto visitEnvInterface = [&visitor, &pEnvClass](const IEnvInterface& envInterface) -> EVisitStatus
		{
			if (!pEnvClass /* || ScriptViewUtils::IsEnvClassUsingNamespace(*pEnvClass, envInterface.GetNamespace())*/)
			{
				return visitor(envInterface);
			}
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc->GetEnvRegistry().VisitInterfaces(visitEnvInterface);
	}
}

void CScriptView::VisitEnvComponents(const EnvComponentConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor)
	if (visitor)
	{
		typedef std::vector<const IEnvComponent*> Exclusions;

		Exclusions exclusions;
		auto visitScriptComponentInstances = [&visitor, &exclusions](const IScriptComponentInstance& componentInstance) -> EVisitStatus
		{
			const IEnvComponent* pEnvComponent = gEnv->pSchematyc->GetEnvRegistry().GetComponent(componentInstance.GetTypeGUID());
			if (pEnvComponent && pEnvComponent->GetDesc().GetComponentFlags().Check(IEntityComponent::EFlags::Singleton))
			{
				exclusions.push_back(pEnvComponent);
			}
			return EVisitStatus::Continue;
		};
		VisitScriptComponentInstances(visitScriptComponentInstances, EDomainScope::All);

		const IEnvClass* pEnvClass = GetEnvClass();
		auto visitEnvComponent = [&visitor, &exclusions, &pEnvClass](const IEnvComponent& envComponent) -> EVisitStatus
		{
			if (std::find(exclusions.begin(), exclusions.end(), &envComponent) == exclusions.end())
			{
				//if (!pEnvClass || ScriptViewUtils::IsEnvClassUsingNamespace(*pEnvClass, envComponent.GetNamespace()))
				//{
				return visitor(envComponent);
				//}
			}
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc->GetEnvRegistry().VisitComponents(visitEnvComponent);
	}
}

void CScriptView::VisitEnvActions(const EnvActionConstVisitor& visitor) const
{
	// #SchematycTODO : Filter actions based on env modules and script component instances?

	gEnv->pSchematyc->GetEnvRegistry().VisitActions(visitor);
}

void CScriptView::VisitScriptModules(const ScriptModuleConstVisitor& visitor) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElements<IScriptModule>(visitor, EScriptElementType::Module, m_scopeGUID);
}

void CScriptView::VisitScriptFunctions(const ScriptFunctionConstVisitor& visitor) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElements<IScriptFunction>(visitor, EScriptElementType::Function, m_scopeGUID);
}

void CScriptView::VisitScriptClasses(const ScriptClassConstVisitor& visitor, EDomainScope scope) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElementsInClass<IScriptClass>(visitor, EScriptElementType::Class, scope, m_scopeGUID);   // #SchematycTODO : Replace with VisitScriptElements?
}

//void CScriptView::VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const {}

void CScriptView::VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElementsInClass<IScriptVariable>(visitor, EScriptElementType::Variable, scope, m_scopeGUID);   // #SchematycTODO : Replace with VisitScriptElements?
}

void CScriptView::VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElementsInClass<IScriptComponentInstance>(visitor, EScriptElementType::ComponentInstance, scope, m_scopeGUID);
}

void CScriptView::VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const
{
	ScriptViewUtils_DEPRECATED::VisitScriptElementsInClass<IScriptActionInstance>(visitor, EScriptElementType::ActionInstance, scope, m_scopeGUID);
}

void CScriptView::VisitScriptModuleFunctions(const ScriptModuleFunctionsConstVisitor& visitor) const
{
	auto visitModules = [&, visitor](const IScriptModule& scriptModule) -> EVisitStatus
	{
		ScriptViewUtils_DEPRECATED::VisitScriptElements<IScriptFunction>(visitor, EScriptElementType::Function, scriptModule.GetGUID());
		return EVisitStatus::Continue;
	};

	ScriptViewUtils_DEPRECATED::VisitRootScriptElements<IScriptModule>(visitModules, EScriptElementType::Module);
}

void CScriptView::VisitScriptModuleVariables(const ScriptModuleVariablesConstVisitor& visitor) const
{
	auto visitModules = [&, visitor](const IScriptModule& scriptModule) -> EVisitStatus
	{
		ScriptViewUtils_DEPRECATED::VisitScriptElements<IScriptVariable>(visitor, EScriptElementType::Variable, scriptModule.GetGUID());
		return EVisitStatus::Continue;
	};

	ScriptViewUtils_DEPRECATED::VisitRootScriptElements<IScriptModule>(visitModules, EScriptElementType::Module);
}

void CScriptView::VisitScriptModuleSignals(const ScriptModuleSignalsConstVisitor& visitor) const
{
	auto visitModules = [&, visitor](const IScriptModule& scriptModule) -> EVisitStatus
	{
		ScriptViewUtils_DEPRECATED::VisitScriptElements<IScriptSignal>(visitor, EScriptElementType::Signal, scriptModule.GetGUID());
		return EVisitStatus::Continue;
	};

	ScriptViewUtils_DEPRECATED::VisitRootScriptElements<IScriptModule>(visitModules, EScriptElementType::Module);
}

void CScriptView::VisitEnclosedEnums(const ScriptEnumConstVisitor& visitor) const
{
	ScriptViewUtils::VisitEnclosedElememts<IScriptEnum>(m_scopeGUID, visitor);
}

void CScriptView::VisitAccesibleEnums(const ScriptEnumConstVisitor& visitor) const
{
	ScriptViewUtils::VisitAccesibleElememts<IScriptEnum>(m_scopeGUID, visitor);
}

void CScriptView::VisitEnclosedStructs(const ScriptStructConstVisitor& visitor) const
{
	ScriptViewUtils::VisitEnclosedElememts<IScriptStruct>(m_scopeGUID, visitor);
}

void CScriptView::VisitAccesibleStructs(const ScriptStructConstVisitor& visitor) const
{
	ScriptViewUtils::VisitAccesibleElememts<IScriptStruct>(m_scopeGUID, visitor);
}

void CScriptView::VisitEnclosedSignals(const ScriptSignalConstVisitor& visitor) const
{
	ScriptViewUtils::VisitEnclosedElememts<IScriptSignal>(m_scopeGUID, visitor);
}

void CScriptView::VisitAccesibleSignals(const ScriptSignalConstVisitor& visitor) const
{
	ScriptViewUtils::VisitAccesibleElememts<IScriptSignal>(m_scopeGUID, visitor);
}

void CScriptView::VisitEnclosedStates(const ScriptStateConstVisitor& visitor) const
{
	ScriptViewUtils::VisitEnclosedElememts<IScriptState>(m_scopeGUID, visitor);
}

void CScriptView::VisitAccesibleStates(const ScriptStateConstVisitor& visitor) const
{
	ScriptViewUtils::VisitAccesibleElememts<IScriptState>(m_scopeGUID, visitor);
}

void CScriptView::VisitEnclosedTimers(const ScriptTimerConstVisitor& visitor) const
{
	ScriptViewUtils::VisitEnclosedElememts<IScriptTimer>(m_scopeGUID, visitor);
}

void CScriptView::VisitAccesibleTimers(const ScriptTimerConstVisitor& visitor) const
{
	ScriptViewUtils::VisitAccesibleElememts<IScriptTimer>(m_scopeGUID, visitor);
}

const IScriptStateMachine* CScriptView::GetScriptStateMachine(const CryGUID& guid) const
{
	return nullptr;
}

const IScriptComponentInstance* CScriptView::GetScriptComponentInstance(const CryGUID& guid) const
{
	return DynamicCast<IScriptComponentInstance>(gEnv->pSchematyc->GetScriptRegistry().GetElement(guid));   // #SchematycTODO : Should we be checking to make sure the result is in scope?
}

const IScriptActionInstance* CScriptView::GetScriptActionInstance(const CryGUID& guid) const
{
	return nullptr;
}

const IScriptElement* CScriptView::GetScriptElement(const CryGUID& guid) const
{
	return gEnv->pSchematyc->GetScriptRegistry().GetElement(guid);   // #SchematycTODO : Should we be checking to make sure the result is in scope?
}

bool CScriptView::QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunction& envFunction, EDomainQualifier qualifier, IString& output) const
{
	const IEnvComponent* pEnvComponent = gEnv->pSchematyc->GetEnvRegistry().GetComponent(scriptComponentInstance.GetTypeGUID());
	if (pEnvComponent)
	{
		if (QualifyName(scriptComponentInstance, qualifier, output))
		{
			if (!output.empty())
			{
				output.append("::");
			}
			output.append(envFunction.GetName());
			return true;
		}
	}
	return false;
}

bool CScriptView::QualifyName(const IEnvClass& envClass, IString& output) const
{
	output.clear();
	//output.append(envClass.GetNamespace());
	//output.append("::");
	output.append(envClass.GetName());
	return true;
}

bool CScriptView::QualifyName(const IEnvInterface& envInterface, IString& output) const
{
	output.clear();
	//output.append(envInterface.GetNamespace());
	//output.append("::");
	output.append(envInterface.GetName());
	return true;
}

bool CScriptView::QualifyName(const IEnvInterfaceFunction& envInterfaceFunction, IString& output) const
{
	// #SchematycTODO : Create a recursive function for qualifying all environment element type names.
	const IEnvInterface* pEnvInterface = DynamicCast<IEnvInterface>(envInterfaceFunction.GetParent());
	if (pEnvInterface)
	{
		if (QualifyName(*pEnvInterface, output))
		{
			output.append("::");
			output.append(envInterfaceFunction.GetName());
			return true;
		}
	}
	return false;
}

bool CScriptView::QualifyName(const IEnvComponent& envComponent, IString& output) const
{
	output.clear();
	//output.append(envComponent.GetNamespace());
	//output.append("::");
	output.append(envComponent.GetName());
	return true;
}

void CScriptView::QualifyName(const IEnvElement& envElement, IString& output) const
{
	output.assign(envElement.GetName());
	for (const IEnvElement* pEnvScope = envElement.GetParent(); pEnvScope && (pEnvScope->GetType() != EEnvElementType::Root); pEnvScope = pEnvScope->GetParent())
	{
		output.insert(0, "::");
		output.insert(0, pEnvScope->GetName());
	}
}

bool CScriptView::QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, IString& output) const
{
	const IScriptElement* pScriptScope = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_scopeGUID);
	if (pScriptScope)
	{
		return QualifyScriptElementName(*pScriptScope, scriptElement, qualifier, output);
	}
	return false;
}
} // Schematyc
