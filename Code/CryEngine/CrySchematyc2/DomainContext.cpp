// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DomainContext.h"

#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Env/IEnvFunctionDescriptor.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptFile.h>
#include <CrySchematyc2/Script/IScriptGraph.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/Services/ILog.h>

#include "CVars.h"
#include "Services/Log.h"

namespace Schematyc2
{
	namespace DomainContextUtils
	{
		typedef std::vector<const IScriptElement*> ScriptElementAncestors;

		inline bool IsEnvFoundationUsingNamespace(const IFoundation& envFoundation, const char* szNamespace)
		{
			CRY_ASSERT(szNamespace);
			if(szNamespace)
			{
				for(size_t namespaceIdx = 0, namespaceCount = envFoundation.GetNamespaceCount(); namespaceIdx < namespaceCount; ++ namespaceIdx)
				{
					if(strcmp(envFoundation.GetNamespace(namespaceIdx), szNamespace) == 0)
					{
						return true;
					}
				}
			}
			return false;
		}

		inline const IScriptClass* GetScriptClass(const IScriptFile& file, const SGUID& guid)
		{
			const IScriptClass* pScriptClass = file.GetClass(guid);
			if(!pScriptClass)
			{
				auto visitScriptInclude = [&guid, &pScriptClass] (const IScriptInclude& scriptInclude) -> EVisitStatus
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(scriptInclude.GetRefGUID());
					CRY_ASSERT(pIncludeFile);
					if(pIncludeFile)
					{
						pScriptClass = GetScriptClass(*pIncludeFile, guid);
						if(pScriptClass)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				};
				file.VisitIncludes(ScriptIncludeConstVisitor::FromLambdaFunction(visitScriptInclude));
			}
			return pScriptClass;
		}

		inline const IScriptClass* GetScriptBaseClass(const IScriptFile& file, const SGUID& scopeGUID)
		{
			const IScriptClassBase* pScriptClassBase = nullptr;
			auto visitScriptClassBase = [&pScriptClassBase] (const IScriptClassBase& scriptClassBase) -> EVisitStatus
			{
				pScriptClassBase = &scriptClassBase;
				return EVisitStatus::Stop;
			};
			file.VisitClassBases(ScriptClassBaseConstVisitor::FromLambdaFunction(visitScriptClassBase), scopeGUID, false);
			if(pScriptClassBase)
			{
				return GetScriptClass(file, pScriptClassBase->GetRefGUID());
			}
			return nullptr;
		}

		template <typename ELEMENT> void VisitScriptElements(const TemplateUtils::CDelegate<EVisitStatus (const ELEMENT&)>& visitor, EScriptElementType elementType, EAccessor accessor, const IScriptFile& file, const SGUID& scopeGUID)
		{
			auto visitScriptElement = [&visitor, elementType, accessor, &file, &scopeGUID] (const IScriptElement& scriptElement) -> EVisitStatus
			{
				const EScriptElementType _elementType = scriptElement.GetElementType();
				if(_elementType == EScriptElementType::Group)
				{
					VisitScriptElements<ELEMENT>(visitor, elementType, accessor, file, scriptElement.GetGUID());
				}
				else if(_elementType == elementType)
				{
					if(scriptElement.GetAccessor() <= accessor)
					{
						visitor(static_cast<const ELEMENT&>(scriptElement));
					}
				}
				return EVisitStatus::Continue;
			};
			file.VisitElements(ScriptElementConstVisitor::FromLambdaFunction(visitScriptElement), scopeGUID, false);
		}

		template <typename ELEMENT> void VisitScriptElementsInClass(const TemplateUtils::CDelegate<EVisitStatus (const ELEMENT&)>& visitor, EScriptElementType elementType, EDomainScope scope, const IScriptFile& file, SGUID scopeGUID)
		{
			const IScriptElement* pScriptElement = file.GetElement(scopeGUID);
			while(pScriptElement)
			{
				DomainContextUtils::VisitScriptElements<ELEMENT>(visitor, elementType, EAccessor::Private, file, scopeGUID);
				if(pScriptElement->GetElementType() != EScriptElementType::Class)
				{
					scopeGUID      = pScriptElement->GetScopeGUID();
					pScriptElement = file.GetElement(scopeGUID);
				}
				else
				{
					break;
				}
			}
			if((scope == EDomainScope::Derived) || ((scope == EDomainScope::All)))
			{
				const EAccessor accessor = scope == EDomainScope::Derived ? EAccessor::Protected : EAccessor::Private;
				for(const IScriptClass* pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(file, scopeGUID); pScriptBaseClass; pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID()))
				{
					DomainContextUtils::VisitScriptElements<ELEMENT>(visitor, elementType, accessor, pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID());
				}
			}
		}

		inline const IScriptElement* GetScriptElement(const IScriptFile& file, const SGUID& guid, EScriptElementType elementType)
		{
			const IScriptElement* pScriptElement = file.GetElement(guid, elementType);
			if(!pScriptElement)
			{
				auto visitScriptInclude = [&file, &guid, elementType, &pScriptElement] (const IScriptInclude& scriptInclude) -> EVisitStatus
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(scriptInclude.GetRefGUID());
					CRY_ASSERT(pIncludeFile);
					if(pIncludeFile)
					{
						pScriptElement = GetScriptElement(*pIncludeFile, guid, elementType);
						if(pScriptElement)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				};
				file.VisitIncludes(ScriptIncludeConstVisitor::FromLambdaFunction(visitScriptInclude));
			}
			return pScriptElement;
		}

		inline void GetScriptElementAncestors(const IScriptElement& scriptElement, ScriptElementAncestors& ancestors)
		{
			ancestors.reserve(16);
			ancestors.push_back(&scriptElement);
			for(const IScriptElement* pScriptElement = scriptElement.GetFile().GetElement(scriptElement.GetScopeGUID()); pScriptElement; pScriptElement = pScriptElement->GetFile().GetElement(pScriptElement->GetScopeGUID()))
			{
				ancestors.push_back(pScriptElement);
			}
		}

		inline const IScriptElement* FindFirstCommonScriptElementAncestor(const IScriptElement& lhsScriptElement, const IScriptElement& rhsScriptElement)
		{
			ScriptElementAncestors lhsScriptElementAncestors;
			ScriptElementAncestors rhsScriptElementAncestors;
			GetScriptElementAncestors(lhsScriptElement, lhsScriptElementAncestors);
			GetScriptElementAncestors(rhsScriptElement, rhsScriptElementAncestors);
			for(const IScriptElement* pScriptElementAncestor : lhsScriptElementAncestors)
			{
				if(std::find(rhsScriptElementAncestors.begin(), rhsScriptElementAncestors.end(), pScriptElementAncestor) != rhsScriptElementAncestors.end())
				{
					return pScriptElementAncestor;
				}
			}
			return nullptr;
		}

		inline bool QualifyScriptElementName(const IScriptElement& scriptScopeElement, const IScriptElement& scriptElement, EDomainQualifier qualifier, stack_string& output)
		{
			if(&scriptScopeElement == &scriptElement)
			{
				return true;
			}

			output = scriptElement.GetName();
			switch(qualifier)
			{
			case EDomainQualifier::Global:
				{
					for(const IScriptElement* pScriptScopeElement = scriptElement.GetFile().GetElement(scriptElement.GetScopeGUID()); pScriptScopeElement; pScriptScopeElement = pScriptScopeElement->GetFile().GetElement(pScriptScopeElement->GetScopeGUID()))
					{
						output.insert(0, "::");
						output.insert(0, pScriptScopeElement->GetName());
					}
					return true;
				}
			case EDomainQualifier::Local:
				{
					const IScriptElement* pFirstCommonScriptElementAncestor = FindFirstCommonScriptElementAncestor(scriptScopeElement, scriptElement);
					for(const IScriptElement* pScriptScopeElement = scriptElement.GetFile().GetElement(scriptElement.GetScopeGUID()); pScriptScopeElement != pFirstCommonScriptElementAncestor; pScriptScopeElement = pScriptScopeElement->GetFile().GetElement(pScriptScopeElement->GetScopeGUID()))
					{
						output.insert(0, "::");
						output.insert(0, pScriptScopeElement->GetName());
					}
					return true;
				}
			default:
				{
					return false;
				}
			}
		}
	}

	CDomainContext::CDomainContext(const SDomainContextScope& scope)
		: m_scope(scope)
	{}

	const SDomainContextScope& CDomainContext::GetScope() const
	{
		return m_scope;
	}

	IFoundationConstPtr CDomainContext::GetEnvFoundation() const
	{
		const IScriptClass* pScriptClass = GetScriptClass();
		if(pScriptClass)
		{
			SGUID foundationGUID = pScriptClass->GetFoundationGUID();
			if(!foundationGUID)
			{
				for(const IScriptClass* pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptClass->GetFile(), pScriptClass->GetGUID()); pScriptBaseClass; pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID()))
				{
					foundationGUID = pScriptBaseClass->GetFoundationGUID();
					if(foundationGUID)
					{
						break;
					}
				}
			}
			if(foundationGUID)
			{
				return gEnv->pSchematyc2->GetEnvRegistry().GetFoundation(foundationGUID);
			}
		}
		return IFoundationConstPtr();
	}

	const IScriptClass* CDomainContext::GetScriptClass() const
	{
		if(m_scope.pScriptFile)
		{
			const IScriptElement* pScriptElement = m_scope.pScriptFile->GetElement(m_scope.guid);
			while(pScriptElement)
			{
				if(pScriptElement->GetElementType() == EScriptElementType::Class)
				{
					return static_cast<const IScriptClass*>(pScriptElement);
				}
				else
				{
					pScriptElement = m_scope.pScriptFile->GetElement(pScriptElement->GetScopeGUID());
				}
			}
		}
		return nullptr;
	}

	void CDomainContext::VisitEnvFunctions(const EnvFunctionVisitor& visitor) const
	{
		// #SchematycTODO : Implement me!!!
	}

	void CDomainContext::VisitEnvGlobalFunctions(const EnvGlobalFunctionVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor)
		if(visitor)
		{
			IFoundationConstPtr pEnvFoundation = GetEnvFoundation();
			auto visitEnvGlobalFunction = [&visitor, &pEnvFoundation] (const IGlobalFunctionConstPtr& pFunction) -> EVisitStatus
			{
				if(!pEnvFoundation || DomainContextUtils::IsEnvFoundationUsingNamespace(*pEnvFoundation, pFunction->GetNamespace()))
				{
					visitor(pFunction);
				}
				return EVisitStatus::Continue;
			};
			gEnv->pSchematyc2->GetEnvRegistry().VisitGlobalFunctions(EnvGlobalFunctionVisitor::FromLambdaFunction(visitEnvGlobalFunction));
		}
	}

	void CDomainContext::VisitEnvAbstractInterfaces(const EnvAbstractInterfaceVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor)
		if(visitor)
		{
			IFoundationConstPtr pEnvFoundation = GetEnvFoundation();
			auto visitEnvAbstractInterface = [&visitor, &pEnvFoundation] (const IAbstractInterfaceConstPtr& pAbstractInterface) -> EVisitStatus
			{
				if(!pEnvFoundation || DomainContextUtils::IsEnvFoundationUsingNamespace(*pEnvFoundation, pAbstractInterface->GetNamespace()))
				{
					visitor(pAbstractInterface);
				}
				return EVisitStatus::Continue;
			};
			gEnv->pSchematyc2->GetEnvRegistry().VisitAbstractInterfaces(EnvAbstractInterfaceVisitor::FromLambdaFunction(visitEnvAbstractInterface));
		}
	}

	void CDomainContext::VisitEnvComponentFactories(const EnvComponentFactoryVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor)
		if(visitor)
		{
			typedef std::vector<const IComponentFactory*> Exclusions;

			Exclusions exclusions;
			auto visitScriptComponentInstances = [&visitor, &exclusions] (const IScriptComponentInstance& componentInstance) -> EVisitStatus
			{
				IComponentFactoryConstPtr pComponentFactory = gEnv->pSchematyc2->GetEnvRegistry().GetComponentFactory(componentInstance.GetComponentGUID());
				if(pComponentFactory && ((pComponentFactory->GetFlags() & EComponentFlags::Singleton) != 0))
				{
					exclusions.push_back(pComponentFactory.get());
				}
				return EVisitStatus::Continue;
			};
			VisitScriptComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitScriptComponentInstances), EDomainScope::All);

			IFoundationConstPtr pEnvFoundation = GetEnvFoundation();
			auto visitEnvComponentFactory = [&visitor, &exclusions, &pEnvFoundation] (const IComponentFactoryConstPtr& pComponentFactory) -> EVisitStatus
			{
				if(std::find(exclusions.begin(), exclusions.end(), pComponentFactory.get()) == exclusions.end())
				{
					if(!pEnvFoundation || DomainContextUtils::IsEnvFoundationUsingNamespace(*pEnvFoundation, pComponentFactory->GetNamespace()))
					{
						visitor(pComponentFactory);
					}
				}
				return EVisitStatus::Continue;
			};
			gEnv->pSchematyc2->GetEnvRegistry().VisitComponentFactories(EnvComponentFactoryVisitor::FromLambdaFunction(visitEnvComponentFactory));
		}
	}

	void CDomainContext::VisitEnvComponentMemberFunctions(const EnvComponentMemberFunctionVisitor& visitor) const
	{
		gEnv->pSchematyc2->GetEnvRegistry().VisitComponentMemberFunctions(visitor);
	}

	void CDomainContext::VisitEnvActionMemberFunctions(const EnvActionMemberFunctionVisitor& visitor) const
	{
		gEnv->pSchematyc2->GetEnvRegistry().VisitActionMemberFunctions(visitor);
	}

	void CDomainContext::VisitScriptEnumerations(const ScriptEnumerationConstVisitor& visitor, EDomainScope scope) const
	{
		if(m_scope.pScriptFile)
		{
			DomainContextUtils::VisitScriptElementsInClass<IScriptEnumeration>(visitor, EScriptElementType::Enumeration, scope, *m_scope.pScriptFile, m_scope.guid);
		}
	}

	void CDomainContext::VisitScriptStates(const ScriptStateConstVisitor& visitor, EDomainScope scope) const
	{
		if(m_scope.pScriptFile)
		{
			DomainContextUtils::VisitScriptElementsInClass<IScriptState>(visitor, EScriptElementType::State, scope, *m_scope.pScriptFile, m_scope.guid);
		}
	}

	void CDomainContext::VisitScriptStateMachines(const ScriptStateMachineConstVisitor& visitor, EDomainScope scope) const
	{
		if(m_scope.pScriptFile)
		{
			DomainContextUtils::VisitScriptElementsInClass<IScriptStateMachine>(visitor, EScriptElementType::StateMachine, scope, *m_scope.pScriptFile, m_scope.guid);
		}
	}

	void CDomainContext::VisitScriptVariables(const ScriptVariableConstVisitor& visitor, EDomainScope scope) const
	{
		if(m_scope.pScriptFile)
		{
			DomainContextUtils::VisitScriptElementsInClass<IScriptVariable>(visitor, EScriptElementType::Variable, scope, *m_scope.pScriptFile, m_scope.guid);
		}
	}

	void CDomainContext::VisitScriptProperties(const ScriptPropertyConstVisitor& visitor, EDomainScope scope) const
	{
		if(m_scope.pScriptFile)
		{
			DomainContextUtils::VisitScriptElementsInClass<IScriptProperty>(visitor, EScriptElementType::Property, scope, *m_scope.pScriptFile, m_scope.guid);
		}
	}

	void CDomainContext::VisitScriptComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, EDomainScope scope) const
	{
		// #SchematycTODO : Use DomainContextUtils::VisitScriptElementsInClass?

		const IScriptClass* pScriptClass = GetScriptClass();
		if(pScriptClass)
		{
			SGUID guid = pScriptClass->GetGUID();
			pScriptClass->GetFile().VisitComponentInstances(visitor, guid, true);
			if(scope == EDomainScope::All)
			{
				for(const IScriptClass* pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(*m_scope.pScriptFile, guid); pScriptBaseClass; pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID()))
				{
					DomainContextUtils::VisitScriptElements<IScriptComponentInstance>(visitor, EScriptElementType::ComponentInstance, EAccessor::Private, pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID());
				}
			}
		}
	}

	void CDomainContext::VisitScriptActionInstances(const ScriptActionInstanceConstVisitor& visitor, EDomainScope scope) const
	{
		// #SchematycTODO : Use DomainContextUtils::VisitScriptElementsInClass?

		if(m_scope.pScriptFile)
		{
			SGUID                 guid = m_scope.guid;
			const IScriptElement* pScriptElement = m_scope.pScriptFile->GetElement(guid);
			while(pScriptElement)
			{
				DomainContextUtils::VisitScriptElements<IScriptActionInstance>(visitor, EScriptElementType::ActionInstance, EAccessor::Private, *m_scope.pScriptFile, guid);
				if(pScriptElement->GetElementType() != EScriptElementType::Class)
				{
					guid           = pScriptElement->GetScopeGUID();
					pScriptElement = m_scope.pScriptFile->GetElement(guid);
				}
				else
				{
					break;
				}
			}
			if(scope == EDomainScope::All)
			{
				for(const IScriptClass* pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(*m_scope.pScriptFile, guid); pScriptBaseClass; pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID()))
				{
					DomainContextUtils::VisitScriptElements<IScriptActionInstance>(visitor, EScriptElementType::ActionInstance, EAccessor::Private, pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID());
				}
			}
		}
	}

	void CDomainContext::VisitDocGraphs(const DocGraphConstVisitor& visitor, EDomainScope scope) const
	{
		// #SchematycTODO : Use DomainContextUtils::VisitScriptElementsInClass?

		if(m_scope.pScriptFile)
		{
			SGUID                 guid = m_scope.guid;
			const IScriptElement* pScriptElement = m_scope.pScriptFile->GetElement(guid);
			while(pScriptElement)
			{
				DomainContextUtils::VisitScriptElements<IDocGraph>(visitor, EScriptElementType::Graph, EAccessor::Private, *m_scope.pScriptFile, guid);
				if(pScriptElement->GetElementType() != EScriptElementType::Class)
				{
					guid           = pScriptElement->GetScopeGUID();
					pScriptElement = m_scope.pScriptFile->GetElement(guid);
				}
				else
				{
					break;
				}
			}
			if((scope == EDomainScope::Derived) || ((scope == EDomainScope::All)))
			{
				const EAccessor accessor = scope == EDomainScope::Derived ? EAccessor::Protected : EAccessor::Private;
				for(const IScriptClass* pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(*m_scope.pScriptFile, guid); pScriptBaseClass; pScriptBaseClass = DomainContextUtils::GetScriptBaseClass(pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID()))
				{
					DomainContextUtils::VisitScriptElements<IDocGraph>(visitor, EScriptElementType::Graph, accessor, pScriptBaseClass->GetFile(), pScriptBaseClass->GetGUID());
				}
			}
		}
	}

	const IScriptStateMachine* CDomainContext::GetScriptStateMachine(const SGUID& guid) const
	{
		return m_scope.pScriptFile ? static_cast<const IScriptStateMachine*>(DomainContextUtils::GetScriptElement(*m_scope.pScriptFile, guid, EScriptElementType::StateMachine)) : nullptr;
	}

	const IScriptComponentInstance* CDomainContext::GetScriptComponentInstance(const SGUID& guid) const
	{
		return m_scope.pScriptFile ? static_cast<const IScriptComponentInstance*>(DomainContextUtils::GetScriptElement(*m_scope.pScriptFile, guid, EScriptElementType::ComponentInstance)) : nullptr;
	}

	const IScriptActionInstance* CDomainContext::GetScriptActionInstance(const SGUID& guid) const
	{
		return m_scope.pScriptFile ? static_cast<const IScriptActionInstance*>(DomainContextUtils::GetScriptElement(*m_scope.pScriptFile, guid, EScriptElementType::ActionInstance)) : nullptr;
	}

	const IDocGraph* CDomainContext::GetDocGraph(const SGUID& guid) const
	{
		return m_scope.pScriptFile ? static_cast<const IDocGraph*>(DomainContextUtils::GetScriptElement(*m_scope.pScriptFile, guid, EScriptElementType::Graph)) : nullptr;
	}

	bool CDomainContext::QualifyName(const IGlobalFunction& envGlobalFunction, stack_string& output) const
	{
		output.clear();
		output.append(envGlobalFunction.GetNamespace());
		output.append("::");
		output.append(envGlobalFunction.GetName());
		return true;
	}

	bool CDomainContext::QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IEnvFunctionDescriptor& envFunctionDescriptor, EDomainQualifier qualifier, stack_string& output) const
	{
		IComponentFactoryConstPtr pEnvComponentFactory = gEnv->pSchematyc2->GetEnvRegistry().GetComponentFactory(scriptComponentInstance.GetComponentGUID());
		if(pEnvComponentFactory)
		{
			if((pEnvComponentFactory->GetFlags() & EComponentFlags::HideInEditor) != 0)
			{
				if(QualifyName(*pEnvComponentFactory, output))
				{
					if(!output.empty())
					{
						output.append("::");
					}
					output.append(envFunctionDescriptor.GetName());
					return true;
				}
			}
			else
			{
				if(QualifyName(scriptComponentInstance, qualifier, output))
				{
					if(!output.empty())
					{
						output.append("::");
					}
					output.append(envFunctionDescriptor.GetName());
					return true;
				}
			}
		}
		return false;
	}

	bool CDomainContext::QualifyName(const IAbstractInterface& envAbstractInterface, stack_string& output) const
	{
		output.clear();
		output.append(envAbstractInterface.GetNamespace());
		output.append("::");
		output.append(envAbstractInterface.GetName());
		return true;
	}

	bool CDomainContext::QualifyName(const IAbstractInterfaceFunction& envAbstractInterfaceFunction, stack_string& output) const
	{
		IAbstractInterfaceConstPtr pEnvAbstractInterface = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterface(envAbstractInterfaceFunction.GetOwnerGUID());
		if(pEnvAbstractInterface)
		{
			if(QualifyName(*pEnvAbstractInterface, output))
			{
				output.append("::");
				output.append(envAbstractInterfaceFunction.GetName());
				return true;
			}
		}
		return false;
	}

	bool CDomainContext::QualifyName(const IComponentFactory& envComponentFactory, stack_string& output) const
	{
		output.clear();
		output.append(envComponentFactory.GetNamespace());
		output.append("::");
		output.append(envComponentFactory.GetName());
		return true;
	}

	bool CDomainContext::QualifyName(const IScriptComponentInstance& scriptComponentInstance, const IComponentMemberFunction& envComponentMemberFunction, EDomainQualifier qualifier, stack_string& output) const
	{
		IComponentFactoryConstPtr pEnvComponentFactory = gEnv->pSchematyc2->GetEnvRegistry().GetComponentFactory(scriptComponentInstance.GetComponentGUID());
		if(pEnvComponentFactory)
		{
			if((pEnvComponentFactory->GetFlags() & EComponentFlags::HideInEditor) != 0)
			{
				if(QualifyName(*pEnvComponentFactory, output))
				{
					output.append("::");
					output.append(envComponentMemberFunction.GetName());
					return true;
				}
			}
			else
			{
				if(QualifyName(scriptComponentInstance, qualifier, output))
				{
					output.append("::");
					output.append(envComponentMemberFunction.GetName());
					return true;
				}
			}
		}
		return false;
	}

	bool CDomainContext::QualifyName(const IScriptActionInstance& scriptActionInstance, const IActionMemberFunction& envActionMemberFunction, EDomainQualifier qualifier, stack_string& output) const
	{
		if(QualifyName(scriptActionInstance, qualifier, output))
		{
			output.append("::");
			output.append(envActionMemberFunction.GetName());
			return true;
		}
		return false;
	}

	bool CDomainContext::QualifyName(const IScriptElement& scriptElement, EDomainQualifier qualifier, stack_string& output) const
	{
		if(m_scope.pScriptFile)
		{
			const IScriptElement* pScriptScopeElement = m_scope.pScriptFile->GetElement(m_scope.guid);
			if(pScriptScopeElement)
			{
				return DomainContextUtils::QualifyScriptElementName(*pScriptScopeElement, scriptElement, qualifier, output);
			}
		}
		return false;
	}
}
