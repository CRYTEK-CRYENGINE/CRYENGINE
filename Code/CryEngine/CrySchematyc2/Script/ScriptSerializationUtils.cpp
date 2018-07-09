// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptSerializationUtils.h"

#include <CrySchematyc2/Serialization/SerializationUtils.h>

#include "Deprecated/DocLogicGraph.h"
#include "Deprecated/DocTransitionGraph.h"
#include "Script/ScriptUserDocumentation.h"
#include "Script/Elements/ScriptAbstractInterface.h"
#include "Script/Elements/ScriptAbstractInterfaceFunction.h"
#include "Script/Elements/ScriptAbstractInterfaceImplementation.h"
#include "Script/Elements/ScriptAbstractInterfaceTask.h"
#include "Script/Elements/ScriptActionInstance.h"
#include "Script/Elements/ScriptClass.h"
#include "Script/Elements/ScriptClassBase.h"
#include "Script/Elements/ScriptComponentInstance.h"
#include "Script/Elements/ScriptContainer.h"
#include "Script/Elements/ScriptEnumeration.h"
#include "Script/Elements/ScriptFunction.h"
#include "Script/Elements/ScriptGroup.h"
#include "Script/Elements/ScriptInclude.h"
#include "Script/Elements/ScriptModule.h"
#include "Script/Elements/ScriptProperty.h"
#include "Script/Elements/ScriptSignal.h"
#include "Script/Elements/ScriptState.h"
#include "Script/Elements/ScriptStateMachine.h"
#include "Script/Elements/ScriptStructure.h"
#include "Script/Elements/ScriptTimer.h"
#include "Script/Elements/ScriptVariable.h"
#include "Serialization/SerializationContext.h"

namespace Schematyc2
{
	namespace DocSerializationUtils
	{
		CDocGraphFactory      g_docGraphFactory; // Move to IFramework?
		CScriptElementFactory g_scriptElementFactory; // Move to IFramework?

		struct SGraphHeader
		{
			inline SGraphHeader()
				: type(EScriptGraphType::Unknown)
			{}

			inline SGraphHeader(const SGUID& _guid, const SGUID& _scopeGUID, const char* _szName, EScriptGraphType _type, const SGUID& _contextGUID)
				: guid(_guid)
				, scopeGUID(_scopeGUID)
				, name(_szName)
				, type(_type)
				, contextGUID(_contextGUID)
			{}

			void Serialize(Serialization::IArchive& arhcive)
			{
				arhcive(guid, "guid");
				arhcive(scopeGUID, "scope_guid");
				arhcive(name, "name");
				arhcive(type, "type");
				arhcive(contextGUID, "context_guid");
			}

			SGUID            guid;
			SGUID            scopeGUID;
			string           name;
			EScriptGraphType type;
			SGUID            contextGUID;
		};

		bool Serialize(Serialization::IArchive& archive, CInputBlock::SElement& value, const char* szName, const char* szLabel)
		{
			if(archive.isInput())
			{
				value.ptr = g_scriptElementFactory.CreateElement(archive, szName);
				if(value.ptr)
				{
					if(value.ptr->GetElementType() == EScriptElementType::Graph)
					{
						archive(ArchiveBlock(value.blackBox, "detail"), szName, szLabel);
					}
					else
					{
						archive(value.blackBox, szName, szLabel);
					}
					return true;
				}
			}
			return false;
		}

		IDocGraphPtr CDocGraphFactory::CreateGraph(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptGraphType type, const SGUID& contextGUID)
		{
			switch(type)
			{
			case EScriptGraphType::AbstractInterfaceFunction:
			case EScriptGraphType::Function:
			case EScriptGraphType::Condition:
			case EScriptGraphType::Constructor:
			case EScriptGraphType::Destructor:
			case EScriptGraphType::SignalReceiver:
				{
					return IDocGraphPtr(new CDocLogicGraph(file, guid, scopeGUID, szName, type, contextGUID));
				}
			case EScriptGraphType::Transition:
				{
					return IDocGraphPtr(new CDocTransitionGraph(file, guid, scopeGUID, szName, type, contextGUID));
				}
			default:
				{
					return IDocGraphPtr();
				}
			}
		}

		CScriptElementFactory::CScriptElementFactory()
			: m_defaultElementType(EScriptElementType::None)
		{}

		void CScriptElementFactory::SetDefaultElementType(EScriptElementType defaultElementType) 
		{
			m_defaultElementType = defaultElementType;
		}

		IScriptElementPtr CScriptElementFactory::CreateElement(IScriptFile& file, EScriptElementType elementType)
		{
			switch(elementType)
			{
			case EScriptElementType::Module:
				{
					return std::make_shared<CScriptModule>(file);
				}
			case EScriptElementType::Include:
				{
					return std::make_shared<CScriptInclude>(file);
				}
			case EScriptElementType::Group:
				{
					return std::make_shared<CScriptGroup>(file);
				}
			case EScriptElementType::Enumeration:
				{
					return std::make_shared<CScriptEnumeration>(file);
				}
			case EScriptElementType::Structure:
				{
					return std::make_shared<CScriptStructure>(file);
				}
			case EScriptElementType::Signal:
				{
					return std::make_shared<CScriptSignal>(file);
				}
			case EScriptElementType::Function:
				{
					return std::make_shared<CScriptFunction>(file);
				}
			case EScriptElementType::AbstractInterface:
				{
					return std::make_shared<CScriptAbstractInterface>(file);
				}
			case EScriptElementType::AbstractInterfaceFunction:
				{
					return std::make_shared<CScriptAbstractInterfaceFunction>(file);
				}
			case EScriptElementType::AbstractInterfaceTask:
				{
					return std::make_shared<CScriptAbstractInterfaceTask>(file);
				}
			case EScriptElementType::Class:
				{
					return std::make_shared<CScriptClass>(file);
				}
			case EScriptElementType::ClassBase:
				{
					return std::make_shared<CScriptClassBase>(file);
				}
			case EScriptElementType::StateMachine:
				{
					return std::make_shared<CScriptStateMachine>(file);
				}
			case EScriptElementType::State:
				{
					return std::make_shared<CScriptState>(file);
				}
			case EScriptElementType::Variable:
				{
					return std::make_shared<CScriptVariable>(file);
				}
			case EScriptElementType::Property:
				{
					return std::make_shared<CScriptProperty>(file);
				}
			case EScriptElementType::Container:
				{
					return std::make_shared<CScriptContainer>(file);
				}
			case EScriptElementType::Timer:
				{
					return std::make_shared<CScriptTimer>(file);
				}
			case EScriptElementType::AbstractInterfaceImplementation:
				{
					return std::make_shared<CScriptAbstractInterfaceImplementation>(file);
				}
			case EScriptElementType::ComponentInstance:
				{
					return std::make_shared<CScriptComponentInstance>(file);
				}
			case EScriptElementType::ActionInstance:
				{
					return std::make_shared<CScriptActionInstance>(file);
				}
			}
			return IScriptElementPtr();
		}

		IScriptElementPtr CScriptElementFactory::CreateElement(Serialization::IArchive& archive, const char* szName)
		{
			IScriptFile* pFile = SerializationContext::GetScriptFile(archive);
			if(pFile)
			{
				EScriptElementType elementType = EScriptElementType::None;
				archive(ArchiveBlock(elementType, "elementType"), "Element");
				if(elementType == EScriptElementType::None)
				{
					elementType = m_defaultElementType;
				}
				else if((elementType == EScriptElementType::Module) && (m_defaultElementType == EScriptElementType::Group)) // Fix for broken data in old script files.
				{
					elementType = m_defaultElementType;
				}

				if(elementType == EScriptElementType::Graph)
				{
					// #SchematycTODO : This is a bit of a hack to maintain compatibility with old files. We should update the format to combine header and detail once graphs become optional element extensions.
					SGraphHeader graphHeader;
					archive(ArchiveBlock(graphHeader, "header"), szName);
					IDocGraphPtr pGraph = g_docGraphFactory.CreateGraph(*pFile, graphHeader.guid, graphHeader.scopeGUID, graphHeader.name.c_str(), graphHeader.type, graphHeader.contextGUID);
					if(pGraph)
					{
						archive(ArchiveBlock(*pGraph, "detail"), szName);
					}
					return pGraph;
				}
				else
				{
					IScriptElementPtr pElement = CreateElement(*pFile, elementType);
					if(pElement)
					{
						archive(*pElement, szName);
					}
					return pElement;
				}
			}
			return IScriptElementPtr();
		}

		CInputBlock::SElement::SElement()
			: serializationPass(ESerializationPass::Load)
			, sortPriority(0)
		{}

		void CInputBlock::SElement::Serialize(Serialization::IArchive& archive)
		{
			if(ptr)
			{
				SCHEMATYC2_SYSTEM_ASSERT(serializationPass <= ESerializationPass::PostLoad)
				CSerializationContext serializationContext(SSerializationContextParams(archive, &ptr->GetFile(), serializationPass));
				ptr->Serialize(archive);
				serializationPass = static_cast<ESerializationPass>(static_cast<std::underlying_type<ESerializationPass>::type>(serializationPass) + 1);
			}
		}

		CInputBlock::CInputBlock(IScriptFile& file)
			: m_file(file)
		{}

		void CInputBlock::Serialize(Serialization::IArchive& archive)
		{
			if(archive.isInput())
			{
				Elements includes;
				Elements groups;
				Elements enumerations;
				Elements structures;
				Elements signals;
				Elements abstractInterfaces;
				Elements abstractInterfaceFunctions;
				Elements abstractInterfaceTasks;
				Elements classes;
				Elements classBases;
				Elements stateMachines;
				Elements states;
				Elements variables;
				Elements properties;
				Elements containers;
				Elements timers;
				Elements abstractInterfaceImplementations;
				Elements componentInstances;
				Elements actionInstances;
				Elements graphs;

				CSerializationContext serializationContext(SSerializationContextParams(archive, &m_file, ESerializationPass::PreLoad));

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Include);
				archive(includes, "includes");
				EraseEmptyElements(includes);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Group);
				archive(groups, "groups");
				EraseEmptyElements(groups);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Enumeration);
				archive(enumerations, "enumerations");
				EraseEmptyElements(enumerations);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Structure);
				archive(structures, "structures");
				EraseEmptyElements(structures);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Signal);
				archive(signals, "signals");
				EraseEmptyElements(signals);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::AbstractInterface);
				archive(abstractInterfaces, "abstract_interfaces");
				EraseEmptyElements(abstractInterfaces);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::AbstractInterfaceFunction);
				archive(abstractInterfaceFunctions, "abstract_interface_functions");
				EraseEmptyElements(abstractInterfaceFunctions);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::AbstractInterfaceTask);
				archive(abstractInterfaceTasks, "abstract_interface_tasks");
				EraseEmptyElements(abstractInterfaceTasks);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Class);
				archive(classes, "schemas");
				EraseEmptyElements(classes);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::ClassBase);
				archive(classBases, "schema_bases");
				EraseEmptyElements(classBases);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::StateMachine);
				archive(stateMachines, "state_machines");
				EraseEmptyElements(stateMachines);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::State);
				archive(states, "states");
				EraseEmptyElements(states);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Variable);
				archive(variables, "variables");
				EraseEmptyElements(variables);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Property);
				archive(properties, "properties");
				EraseEmptyElements(properties);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Container);
				archive(containers, "containers");
				EraseEmptyElements(containers);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Timer);
				archive(timers, "timers");
				EraseEmptyElements(timers);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::AbstractInterfaceImplementation);
				archive(abstractInterfaceImplementations, "abstract_interface_implementations");
				EraseEmptyElements(abstractInterfaceImplementations);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::ComponentInstance);
				archive(componentInstances, "component_instances");
				EraseEmptyElements(componentInstances);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::ActionInstance);
				archive(actionInstances, "action_instances");
				EraseEmptyElements(actionInstances);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Graph);
				archive(graphs, "graphs");
				EraseEmptyElements(graphs);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::None);

				archive(m_elements, "elements");
				EraseEmptyElements(stateMachines);

				g_scriptElementFactory.SetDefaultElementType(EScriptElementType::Include); // Workround for include files that are loaded on request. Once pre-pass is implemented this should be set back to EDocElementType::None.

				size_t elementCount = m_elements.size();
				elementCount += includes.size();
				elementCount += groups.size();
				elementCount += enumerations.size();
				elementCount += structures.size();
				elementCount += signals.size();
				elementCount += abstractInterfaces.size();
				elementCount += abstractInterfaceFunctions.size();
				elementCount += abstractInterfaceTasks.size();
				elementCount += classes.size();
				elementCount += classBases.size();
				elementCount += stateMachines.size();
				elementCount += states.size();
				elementCount += variables.size();
				elementCount += properties.size();
				elementCount += containers.size();
				elementCount += timers.size();
				elementCount += abstractInterfaceImplementations.size();
				elementCount += componentInstances.size();
				elementCount += actionInstances.size();
				elementCount += graphs.size();
				m_elements.reserve(elementCount);

				AppendElements(m_elements, includes);
				AppendElements(m_elements, groups);
				AppendElements(m_elements, enumerations);
				AppendElements(m_elements, structures);
				AppendElements(m_elements, signals);
				AppendElements(m_elements, abstractInterfaces);
				AppendElements(m_elements, abstractInterfaceFunctions);
				AppendElements(m_elements, abstractInterfaceTasks);
				AppendElements(m_elements, classes);
				AppendElements(m_elements, classBases);
				AppendElements(m_elements, stateMachines);
				AppendElements(m_elements, states);
				AppendElements(m_elements, variables);
				AppendElements(m_elements, properties);
				AppendElements(m_elements, containers);
				AppendElements(m_elements, timers);
				AppendElements(m_elements, abstractInterfaceImplementations);
				AppendElements(m_elements, componentInstances);
				AppendElements(m_elements, actionInstances);
				AppendElements(m_elements, graphs);
			}
		}

		void CInputBlock::BuildElementHierarchy(IScriptElement& parent)
		{
			typedef std::unordered_map<SGUID, IScriptElement*> ElementsByGUID;

			ElementsByGUID elementsByGUID;
			for(const SElement& element : m_elements)
			{
				elementsByGUID.insert(ElementsByGUID::value_type(element.ptr->GetGUID(), element.ptr.get()));
			}

			for(const SElement& element : m_elements)
			{
				const SGUID scopeGUID = element.ptr->GetScopeGUID();
				if(scopeGUID == parent.GetGUID())
				{
					parent.AttachChild(*element.ptr);
				}
				else
				{
					ElementsByGUID::const_iterator itParentElement = elementsByGUID.find(scopeGUID);
					if(itParentElement != elementsByGUID.end())
					{
						itParentElement->second->AttachChild(*element.ptr);
					}
					else
					{
						SCHEMATYC2_SYSTEM_WARNING("Failed to find parent element!");
					}
				}
			}
		}

		void CInputBlock::SortElementsByDependency()
		{
			SortElementsByDependency(m_elements);
			VerifyElementDependencies(m_elements);
		}

		CInputBlock::Elements& CInputBlock::GetElements()
		{
			return m_elements;
		}

		void CInputBlock::AppendElements(Elements& dst, const Elements& src) const
		{
			for(const SElement& element : src)
			{
				dst.push_back(element);
			}
		}

		void CInputBlock::EraseEmptyElements(Elements &elements) const
		{
			size_t elementCount = elements.size();
			for(size_t elementIdx = 0; elementIdx < elementCount; )
			{
				if(!elements[elementIdx].ptr)
				{
					if(elementIdx < (elementCount - 1))
					{
						elements[elementIdx] = elements[elementCount - 1];
					}
					-- elementCount;
				}
				else
				{
					++ elementIdx;
				}
			}
			elements.resize(elementCount);
		}

		void CInputBlock::SortElementsByDependency(Elements &elements)
		{
			typedef std::unordered_map<SGUID, size_t> DependencyMap;

			// Give include elements maximum sort priority to ensure they are loaded first.
			// This is a temporary workaround until a proper 'include' step can be implemented.
			for(SElement& element : m_elements)
			{
				if(element.ptr->GetElementType() == EScriptElementType::Include)
				{
					element.sortPriority = ~0;
				}
			}

			DependencyMap dependencyMap;
			const size_t  elementCount = elements.size();
			dependencyMap.reserve(elementCount);
			for(size_t elementIdx = 0; elementIdx < elementCount; ++ elementIdx)
			{
				dependencyMap.insert(DependencyMap::value_type(elements[elementIdx].ptr->GetGUID(), elementIdx));
			}

			for(SElement& element : elements)
			{
				auto visitElementDependency = [&elements, &dependencyMap, &element] (const SGUID& guid)
				{
					DependencyMap::const_iterator itDependency = dependencyMap.find(guid);
					if(itDependency != dependencyMap.end())
					{
						element.dependencyIndices.push_back(itDependency->second);
					}
				};
				element.ptr->EnumerateDependencies(ScriptDependancyEnumerator::FromLambdaFunction(visitElementDependency));
			}

			size_t recursiveDependencyCount = 0;
			for(size_t elementIdx = 0; elementIdx < elementCount; ++ elementIdx)
			{
				const SElement& element = m_elements[elementIdx];
				for(const size_t& dependencyIdx : element.dependencyIndices)
				{
					const SElement& dependencyElement = elements[dependencyIdx];
					if(std::find(dependencyElement.dependencyIndices.begin(), dependencyElement.dependencyIndices.end(), elementIdx) != dependencyElement.dependencyIndices.end())
					{
						++ recursiveDependencyCount;
					}
				}
			}
			if(recursiveDependencyCount)
			{
				SCHEMATYC2_SYSTEM_CRITICAL_ERROR("%d recursive dependencies detected!", recursiveDependencyCount);
			}

			bool bShuffle;
			do
			{
				bShuffle = false;
				for(const SElement& element : elements)
				{
					for(const size_t& dependencyIdx : element.dependencyIndices)
					{
						SElement& dependencyElement = elements[dependencyIdx];
						if(dependencyElement.sortPriority <= element.sortPriority)
						{
							dependencyElement.sortPriority = element.sortPriority + 1;
							bShuffle                       = true;
						}
					}
				}
			} while(bShuffle);

			auto sortElements = [] (const SElement& lhs, const SElement& rhs) -> bool
			{
				return lhs.sortPriority > rhs.sortPriority;
			};
			std::sort(elements.begin(), elements.end(), sortElements);
		}

		void CInputBlock::VerifyElementDependencies(const Elements &elements)
		{
			typedef std::unordered_set<SGUID> Dependencies;

			Dependencies dependencies;
			dependencies.reserve(elements.size());
			for(const SElement& element : elements)
			{
				dependencies.insert(element.ptr->GetGUID());
			}

			uint32 errorCount = 0;
			for(const SElement& element : elements)
			{
				auto visitElementDependency = [&dependencies, &errorCount] (const SGUID& guid)
				{
					if(dependencies.find(guid) != dependencies.end())
					{
						++ errorCount;
					}
				};
				element.ptr->EnumerateDependencies(ScriptDependancyEnumerator::FromLambdaFunction(visitElementDependency));
				dependencies.erase(element.ptr->GetGUID());
			}

			SCHEMATYC2_SYSTEM_ASSERT(!errorCount);
		}
	}
}
