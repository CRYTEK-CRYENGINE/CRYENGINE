// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Rename script utils!!!
// #SchematycTODO : Clean up or deprecate collection utilities?
// #SchematycTODO : Should collectors reserve memory before storing results?
// #SchematycTODO : Replace true/false for EVisitFlags!

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

#include "CrySchematyc2/IFoundation.h"
#include "CrySchematyc2/ILibRegistry.h"
#include "CrySchematyc2/Env/IEnvRegistry.h"
#include "CrySchematyc2/Script/IScriptFile.h"
#include "CrySchematyc2/Script/IScriptGraph.h"
#include "CrySchematyc2/Script/IScriptRegistry.h"
#include "CrySchematyc2/Services/ILog.h"

namespace Schematyc2
{
	typedef std::vector<SGUID> GUIDVector;
	
	typedef std::vector<IScriptGroup*>                                 TScriptGroupVector;
	typedef std::vector<const IScriptGroup*>                           TScriptGroupConstVector;
	typedef std::vector<IScriptSignal*>                                TScriptSignalVector;
	typedef std::vector<const IScriptSignal*>                          TScriptSignalConstVector;
	typedef std::vector<IScriptAbstractInterface*>                     TScriptAbstractInterfaceVector;
	typedef std::vector<const IScriptAbstractInterface*>               TScriptAbstractInterfaceConstVector;
	typedef std::vector<IScriptAbstractInterfaceFunction*>             TScriptAbstractInterfaceFunctionVector;
	typedef std::vector<const IScriptAbstractInterfaceFunction*>       TScriptAbstractInterfaceFunctionConstVector;
	typedef std::vector<IScriptAbstractInterfaceTask*>                 TScriptAbstractInterfaceTaskVector;
	typedef std::vector<const IScriptAbstractInterfaceTask*>           TScriptAbstractInterfaceTaskConstVector;
	typedef std::vector<IScriptClass*>                                 TScriptClassVector;
	typedef std::vector<const IScriptClass*>                           TScriptClassConstVector;
	typedef std::vector<IScriptStateMachine*>                          TScriptStateMachineVector;
	typedef std::vector<const IScriptStateMachine*>                    TScriptStateMachineConstVector;
	typedef std::vector<IScriptState*>                                 TScriptStateVector;
	typedef std::vector<const IScriptState*>                           TScriptStateConstVector;
	typedef std::vector<IScriptVariable*>                              TScriptVariableVector;
	typedef std::vector<const IScriptVariable*>                        TScriptVariableConstVector;
	typedef std::vector<IScriptProperty*>                              TScriptPropertyVector;
	typedef std::vector<const IScriptProperty*>                        TScriptPropertyConstVector;
	typedef std::vector<IScriptContainer*>                             TScriptContainerVector;
	typedef std::vector<const IScriptContainer*>                       TScriptContainerConstVector;
	typedef std::vector<IScriptTimer*>                                 TScriptTimerVector;
	typedef std::vector<const IScriptTimer*>                           TScriptTimerConstVector;
	typedef std::vector<IScriptAbstractInterfaceImplementation*>       TScriptAbstractInterfaceImplementationVector;
	typedef std::vector<const IScriptAbstractInterfaceImplementation*> TScriptAbstractInterfaceImplementationConstVector;
	typedef std::vector<IScriptComponentInstance*>                     TScriptComponentInstanceVector;
	typedef std::vector<const IScriptComponentInstance*>               TScriptComponentInstanceConstVector;
	typedef std::vector<IScriptActionInstance*>                        TScriptActionInstanceVector;
	typedef std::vector<const IScriptActionInstance*>                  TScriptActionInstanceConstVector;
	typedef std::vector<IScriptGraphNode*>                             TScriptGraphNodeVector;
	typedef std::vector<const IScriptGraphNode*>                       TScriptGraphNodeConstVector;
	typedef std::vector<IScriptGraphLink*>                             TScriptGraphLinkVector;
	typedef std::vector<const IScriptGraphLink*>                       TScriptGraphLinkConstVector;
	typedef std::vector<IDocGraph*>                                    TDocGraphVector;
	typedef std::vector<const IDocGraph*>                              TDocGraphConstVector;

	namespace DocUtils
	{
		// Count instances of element type at a given scope.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline uint32 Count(const IScriptFile& file, const SGUID& scopeGUID, EScriptElementType elementType, EVisitFlags visitFlags)
		{
			uint32 count = 0;
			auto visitElement = [elementType, &count] (const IScriptElement& element) -> EVisitStatus
			{
				if(element.GetElementType() == elementType)
				{
					++ count;
				}
				return EVisitStatus::Continue;
			};
			file.VisitElements(ScriptElementConstVisitor::FromLambdaFunction(visitElement), scopeGUID, (visitFlags & EVisitFlags::RecurseHierarchy) != 0);
			return count;
		}

		// Empty filter.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		template <typename TYPE> struct SEmptyFilter
		{
			inline bool operator () (const TYPE&) const
			{
				return true;
			}
		};

		// Utility class for collecting elements from a file.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		template <typename CONTAINER, typename ELEMENT, typename FILTER = SEmptyFilter<ELEMENT> > class CElementCollector
		{
		public:

			typedef std::vector<ELEMENT*> Elements;

			inline CElementCollector(Elements& elements, const FILTER& filter = FILTER())
				: m_elements(elements)
				, m_filter(filter)
			{}

			inline EVisitStatus Collect(ELEMENT& element)
			{
				if(m_filter(element))
				{
					m_elements.push_back(&element);
				}
				return EVisitStatus::Continue;
			}

		private:

			Elements& m_elements;
			FILTER    m_filter;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGroups(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptGroupVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptGroup> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitGroups(ScriptGroupVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGroups(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptGroupConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptGroup> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitGroups(ScriptGroupConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}
		
		//////////////////////////////////////////////////////////////////////////
		inline void CollectSignals(IScriptFile& file, TScriptSignalVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptSignal> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitSignals(ScriptSignalVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), SGUID(), true);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectSignals(const IScriptFile& file, TScriptSignalConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptSignal> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitSignals(ScriptSignalConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), SGUID(), true);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaces(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptAbstractInterface> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaces(ScriptAbstractInterfaceVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaces(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptAbstractInterface> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaces(ScriptAbstractInterfaceConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaceFunctions(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceFunctionVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptAbstractInterfaceFunction> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaceFunctions(ScriptAbstractInterfaceFunctionVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaceFunctions(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceFunctionConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptAbstractInterfaceFunction> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaceFunctions(ScriptAbstractInterfaceFunctionConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectClasses(IScriptFile& file, TScriptClassVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptClass> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitClasses(ScriptClassVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), SGUID(), true);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectClasses(const IScriptFile& file, TScriptClassConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptClass> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitClasses(ScriptClassConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), SGUID(), true);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectStateMachines(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptStateMachineVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptStateMachine> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitStateMachines(ScriptStateMachineVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectStateMachines(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptStateMachineConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptStateMachine> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitStateMachines(ScriptStateMachineConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectStates(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptStateVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptState> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitStates(ScriptStateVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectStates(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptStateConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptState> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitStates(ScriptStateConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectVariables(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptVariableVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptVariable> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitVariables(ScriptVariableVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectVariables(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptVariableConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptVariable> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitVariables(ScriptVariableConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectProperties(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptPropertyVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptProperty> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitProperties(ScriptPropertyVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectProperties(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptPropertyConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptProperty> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitProperties(ScriptPropertyConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectContainers(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptContainerVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptContainer> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitContainers(ScriptContainerVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectContainers(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptContainerConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptContainer> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitContainers(ScriptContainerConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectTimers(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptTimerVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptTimer> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitTimers(ScriptTimerVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectTimers(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptTimerConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptTimer> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitTimers(ScriptTimerConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaceImplementations(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceImplementationVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptAbstractInterfaceImplementation> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaceImplementations(ScriptAbstractInterfaceImplementationVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectAbstractInterfaceImplementations(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptAbstractInterfaceImplementationConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptAbstractInterfaceImplementation> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitAbstractInterfaceImplementations(ScriptAbstractInterfaceImplementationConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectComponentInstances(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptComponentInstanceVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptComponentInstance> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitComponentInstances(ScriptComponentInstanceVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectComponentInstances(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptComponentInstanceConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptComponentInstance> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectActionInstances(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptActionInstanceVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptActionInstance> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitActionInstances(ScriptActionInstanceVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectActionInstances(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TScriptActionInstanceConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptActionInstance> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitActionInstances(ScriptActionInstanceConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphs(IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TDocGraphVector& results)
		{
			typedef CElementCollector<IScriptFile, IDocGraph> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitGraphs(DocGraphVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphs(const IScriptFile& file, const SGUID& scopeGUID, bool bRecurseHierarchy, TDocGraphConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IDocGraph> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			file.VisitGraphs(DocGraphConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector), scopeGUID, bRecurseHierarchy);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodes(IScriptGraph& graph, TScriptGraphNodeVector& results)
		{
			typedef CElementCollector<IScriptFile, IScriptGraphNode> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			graph.VisitNodes(ScriptGraphNodeVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodes(const IScriptGraph& graph, TScriptGraphNodeConstVector& results)
		{
			typedef CElementCollector<IScriptFile, const IScriptGraphNode> Collector;

			LOADING_TIME_PROFILE_SECTION;

			Collector collector(results);
			graph.VisitNodes(ScriptGraphNodeConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		struct SGraphNodeClassFilter
		{
			inline SGraphNodeClassFilter(EScriptGraphNodeType _type)
				: type(_type)
			{}

			inline bool operator () (IScriptGraphNode& graphNode)
			{
				return graphNode.GetType() == type;
			}

			inline bool operator () (const IScriptGraphNode& graphNode)
			{
				return graphNode.GetType() == type;
			}

			EScriptGraphNodeType type;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodes(IScriptGraph& graph, EScriptGraphNodeType type, TScriptGraphNodeVector& results)
		{
			typedef CElementCollector<IScriptGraph, IScriptGraphNode, SGraphNodeClassFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			const SGraphNodeClassFilter filter(type);
			Collector                   collector(results, filter);
			graph.VisitNodes(ScriptGraphNodeVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodes(const IScriptGraph& graph, EScriptGraphNodeType type, TScriptGraphNodeConstVector& results)
		{
			typedef CElementCollector<IScriptGraph, const IScriptGraphNode, SGraphNodeClassFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			const SGraphNodeClassFilter filter(type);
			Collector                   collector(results, filter);
			graph.VisitNodes(ScriptGraphNodeConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		struct SGraphNodeInputLinkFilter
		{
			inline SGraphNodeInputLinkFilter(const SGUID& _dstNodeGUID)
				: dstNodeGUID(_dstNodeGUID)
			{}

			inline bool operator () (IScriptGraphLink& graphLink)
			{
				return graphLink.GetDstNodeGUID() == dstNodeGUID;
			}

			inline bool operator () (const IScriptGraphLink& graphLink)
			{
				return graphLink.GetDstNodeGUID() == dstNodeGUID;
			}

			const SGUID dstNodeGUID;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeInputLinks(IScriptGraph& graph, const SGUID& dstNodeGUID, TScriptGraphLinkVector& results)
		{
			typedef CElementCollector<IScriptGraph, IScriptGraphLink, SGraphNodeInputLinkFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			const SGraphNodeInputLinkFilter filter(dstNodeGUID);
			Collector                       collector(results, filter);
			graph.VisitLinks(ScriptGraphLinkVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeInputLinks(const IScriptGraph& graph, const SGUID& dstNodeGUID, TScriptGraphLinkConstVector& results)
		{
			typedef CElementCollector<IScriptGraph, const IScriptGraphLink, SGraphNodeInputLinkFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			const SGraphNodeInputLinkFilter filter(dstNodeGUID);
			Collector                       collector(results, filter);
			graph.VisitLinks(ScriptGraphLinkConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
		}

		//////////////////////////////////////////////////////////////////////////
		struct SGraphNodeInputLinkNameFilter
		{
			inline SGraphNodeInputLinkNameFilter(const SGUID& _dstNodeGUID, const char* _szDstInputName)
				: dstNodeGUID(_dstNodeGUID)
				, szDstInputName(_szDstInputName)
			{}

			inline bool operator () (IScriptGraphLink& graphLink)
			{
				return (graphLink.GetDstNodeGUID() == dstNodeGUID) && (strcmp(graphLink.GetDstInputName(), szDstInputName) == 0);
			}

			inline bool operator () (const IScriptGraphLink& graphLink)
			{
				return (graphLink.GetDstNodeGUID() == dstNodeGUID) && (strcmp(graphLink.GetDstInputName(), szDstInputName) == 0);
			}

			const SGUID dstNodeGUID;
			const char* szDstInputName;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeInputLinks(IScriptGraph& graph, const SGUID& dstNodeGUID, const char* szDstInputName, TScriptGraphLinkVector& results)
		{
			typedef CElementCollector<IScriptGraph, IScriptGraphLink, SGraphNodeInputLinkNameFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			SCHEMATYC2_SYSTEM_ASSERT(szDstInputName);
			if(szDstInputName)
			{
				const SGraphNodeInputLinkNameFilter filter(dstNodeGUID, szDstInputName);
				Collector                           collector(results, filter);
				graph.VisitLinks(ScriptGraphLinkVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeInputLinks(const IScriptGraph& graph, const SGUID& dstNodeGUID, const char* szDstInputName, TScriptGraphLinkConstVector& results)
		{
			typedef CElementCollector<IScriptGraph, const IScriptGraphLink, SGraphNodeInputLinkNameFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			SCHEMATYC2_SYSTEM_ASSERT(szDstInputName);
			if(szDstInputName)
			{
				const SGraphNodeInputLinkNameFilter filter(dstNodeGUID, szDstInputName);
				Collector                           collector(results, filter);
				graph.VisitLinks(ScriptGraphLinkConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		struct SGraphNodeOutputLinkNameFilter
		{
			inline SGraphNodeOutputLinkNameFilter(const SGUID& _srcNodeGUID, const char* _szSrcOutputName)
				: srcNodeGUID(_srcNodeGUID)
				, szSrcOutputName(_szSrcOutputName)
			{}

			inline bool operator () (IScriptGraphLink& graphLink)
			{
				return (graphLink.GetSrcNodeGUID() == srcNodeGUID) && (strcmp(graphLink.GetSrcOutputName(), szSrcOutputName) == 0);
			}

			inline bool operator () (const IScriptGraphLink& graphLink)
			{
				return (graphLink.GetSrcNodeGUID() == srcNodeGUID) && (strcmp(graphLink.GetSrcOutputName(), szSrcOutputName) == 0);
			}

			const SGUID srcNodeGUID;
			const char* szSrcOutputName;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeOutputLinks(IScriptGraph& graph, const SGUID& srcNodeGUID, const char* szSrcOutputName, TScriptGraphLinkVector& results)
		{
			typedef CElementCollector<IScriptGraph, IScriptGraphLink, SGraphNodeOutputLinkNameFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			SCHEMATYC2_SYSTEM_ASSERT(szSrcOutputName);
			if(szSrcOutputName)
			{
				const SGraphNodeOutputLinkNameFilter filter(srcNodeGUID, szSrcOutputName);
				Collector                            collector(results, filter);
				graph.VisitLinks(ScriptGraphLinkVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline void CollectGraphNodeOutputLinks(const IScriptGraph& graph, const SGUID& srcNodeGUID, const char* szSrcOutputName, TScriptGraphLinkConstVector& results)
		{
			typedef CElementCollector<IScriptGraph, const IScriptGraphLink, SGraphNodeOutputLinkNameFilter> Collector;

			LOADING_TIME_PROFILE_SECTION;

			SCHEMATYC2_SYSTEM_ASSERT(szSrcOutputName);
			if(szSrcOutputName)
			{
				const SGraphNodeOutputLinkNameFilter filter(srcNodeGUID, szSrcOutputName);
				Collector                            collector(results, filter);
				graph.VisitLinks(ScriptGraphLinkConstVisitor::FromMemberFunction<Collector, &Collector::Collect>(collector));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline void SortProperties(TScriptPropertyVector& properties)
		{
			auto compareProperties = [] (const IScriptProperty* lhs, const IScriptProperty* rhs)
			{
				return strcmp(lhs->GetName(), rhs->GetName()) < 0;
			};
			std::sort(properties.begin(), properties.end(), compareProperties);
		}

		//////////////////////////////////////////////////////////////////////////
		inline void SortProperties(TScriptPropertyConstVector& properties)
		{
			auto compareProperties = [] (const IScriptProperty* lhs, const IScriptProperty* rhs)
			{
				return strcmp(lhs->GetName(), rhs->GetName()) < 0;
			};
			std::sort(properties.begin(), properties.end(), compareProperties);
		}

		//////////////////////////////////////////////////////////////////////////
		inline const IScriptClass* FindOwnerClass(const IScriptFile& file, const SGUID& scopeGUID)
		{
			const IScriptElement* pElement = file.GetElement(scopeGUID);
			if(pElement)
			{
				if(pElement->GetElementType() == EScriptElementType::Class)
				{
					return static_cast<const IScriptClass*>(pElement);
				}
				else
				{
					return FindOwnerClass(file, pElement->GetScopeGUID());
				}
			}
			return nullptr;
		}

		//////////////////////////////////////////////////////////////////////////
		inline const IScriptStateMachine* FindOwnerStateMachine(const IScriptFile& file, const SGUID& scopeGUID)
		{
			const IScriptElement* pElement = file.GetElement(scopeGUID);
			if(pElement)
			{
				if(pElement->GetElementType() == EScriptElementType::StateMachine)
				{
					return static_cast<const IScriptStateMachine*>(pElement);
				}
				else
				{
					return FindOwnerStateMachine(file, pElement->GetScopeGUID());
				}
			}
			return nullptr;
		}

		//////////////////////////////////////////////////////////////////////////
		inline const IScriptState* FindOwnerState(const IScriptFile& file, const SGUID& scopeGUID)
		{
			const IScriptElement* pElement = file.GetElement(scopeGUID);
			if(pElement)
			{
				if(pElement->GetElementType() == EScriptElementType::State)
				{
					return static_cast<const IScriptState*>(pElement);
				}
				else
				{
					return FindOwnerState(file, pElement->GetScopeGUID());
				}
			}
			return nullptr;
		}

		//////////////////////////////////////////////////////////////////////////
		inline const IDocGraph* FindOwnerGraph(const IScriptFile& file, const SGUID& scopeGUID)
		{
			const IScriptElement* pElement = file.GetElement(scopeGUID);
			if(pElement)
			{
				if(pElement->GetElementType() == EScriptElementType::Graph)
				{
					return static_cast<const IDocGraph*>(pElement);
				}
				else
				{
					return FindOwnerGraph(file, pElement->GetScopeGUID());
				}
			}
			return nullptr;
		}

		//////////////////////////////////////////////////////////////////////////
		inline void GetFullElementName(const IScriptFile& file, const IScriptElement& element, stack_string& output, EScriptElementType ownerElementType = EScriptElementType::None)
		{
			output = element.GetName();
			for(const IScriptElement* pScopeElement = file.GetElement(element.GetScopeGUID()); pScopeElement && (pScopeElement->GetElementType() != ownerElementType); pScopeElement = file.GetElement(pScopeElement->GetScopeGUID()))
			{
				const char* szName = pScopeElement->GetName();
				if(szName[0])
				{
					if(!output.empty())
					{
						output.insert(0, "::");
					}
					output.insert(0, pScopeElement->GetName());
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline void GetFullElementName(const IScriptFile& file, const SGUID& guid, stack_string& output, EScriptElementType ownerElementType = EScriptElementType::None)
		{
			const IScriptElement* pElement = file.GetElement(guid);
			if(pElement)
			{
				GetFullElementName(file, *pElement, output, ownerElementType);
			}
			else
			{
				output.clear();
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsElementInScope(const IScriptFile& file, const SGUID& scopeGUID, const SGUID& elementScopeGUID)
		{
			if(!elementScopeGUID)
			{
				return true;
			}
			else
			{
				for(const IScriptElement* pScopeElement = file.GetElement(scopeGUID); pScopeElement; pScopeElement = file.GetElement(pScopeElement->GetScopeGUID()))
				{
					if(pScopeElement->GetGUID() == elementScopeGUID)
					{
						return true;
					}
					TScriptGroupConstVector groups;
					CollectGroups(file, SGUID(), true, groups);
					for(TScriptGroupConstVector::const_iterator itGroup = groups.begin(), itEndGroup = groups.end(); itGroup != itEndGroup; ++ itGroup)
					{
						if((*itGroup)->GetGUID() == elementScopeGUID)
						{
							return true;
						}
					}
				}
				TScriptGroupConstVector groups;
				CollectGroups(file, SGUID(), true, groups);
				for(TScriptGroupConstVector::const_iterator itGroup = groups.begin(), itEndGroup = groups.end(); itGroup != itEndGroup; ++ itGroup)
				{
					if((*itGroup)->GetGUID() == elementScopeGUID)
					{
						return true;
					}
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsComponentInScope(const IScriptFile& file, const SGUID& scopeGUID, const SGUID& componentGUID)
		{
			const IScriptClass* pClass = FindOwnerClass(file, scopeGUID);
			if(pClass)
			{
				IFoundationConstPtr pFoundation = gEnv->pSchematyc2->GetEnvRegistry().GetFoundation(pClass->GetFoundationGUID());
				if(pFoundation)
				{
					if(FoundationUtils::FindFoundationComponent(*pFoundation, componentGUID) != INVALID_INDEX)
					{
						return true;
					}
				}

				const SGUID classGUID = pClass->GetGUID();
				bool        bResult = false;
				auto visitComponentInstance = [&file, &componentGUID, &classGUID, &bResult] (const IScriptComponentInstance& componentInstance) -> EVisitStatus
				{
					if(componentInstance.GetComponentGUID() == componentGUID)
					{
						for(const IScriptElement* pElement = &componentInstance; pElement; pElement = file.GetElement(pElement->GetScopeGUID()))
						{
							if(pElement->GetScopeGUID() == classGUID)
							{
								bResult = true;
								return EVisitStatus::Stop;
							}
						}
					}
					return EVisitStatus::Continue;
				};
				file.VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitComponentInstance));
				return bResult;
			}
			return false;
		}

		namespace Private
		{
			//////////////////////////////////////////////////////////////////////////
			inline bool IsActionInScopeOrGroup(const IScriptFile& file, const SGUID& scopeGUID, const SGUID& actionGUID)
			{
				TScriptActionInstanceConstVector actionInstances;
				CollectActionInstances(file, scopeGUID, false, actionInstances);
				for(TScriptActionInstanceConstVector::const_iterator iActionInstance = actionInstances.begin(), iEndActionInstance = actionInstances.end(); iActionInstance != iEndActionInstance; ++ iActionInstance)
				{
					if((*iActionInstance)->GetActionGUID() == actionGUID)
					{
						return true;
					}
				}
				TScriptGroupConstVector	groups;
				CollectGroups(file, scopeGUID, false, groups);
				for(TScriptGroupConstVector::const_iterator iGroup = groups.begin(), iEndGroup = groups.end(); iGroup != iEndGroup; ++ iGroup)
				{
					if(IsActionInScopeOrGroup(file, (*iGroup)->GetGUID(), actionGUID))
					{
						return true;
					}
				}
				return false;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsActionInScope(const IScriptFile& file, const SGUID& scopeGUID, const SGUID& actionGUID)
		{
			for(const IScriptElement* pScopeElement = file.GetElement(scopeGUID); pScopeElement; pScopeElement = file.GetElement(pScopeElement->GetScopeGUID()))
			{
				if(Private::IsActionInScopeOrGroup(file, pScopeElement->GetGUID(), actionGUID))
				{
					return true;
				}
				else if(pScopeElement->GetElementType() == EScriptElementType::Class)
				{
					return false;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsElementAvailableInScope(const IScriptFile& file, const SGUID& scopeGUID, const char* szNamespace)
		{
			const IScriptClass* pClass = FindOwnerClass(file, scopeGUID);
			if(pClass)
			{
				IFoundationConstPtr	pFoundation = gEnv->pSchematyc2->GetEnvRegistry().GetFoundation(pClass->GetFoundationGUID());
				return pFoundation && (FoundationUtils::FindFoundationNamespace(*pFoundation, szNamespace) != INVALID_INDEX);
			}
			return false;
		}

		enum class EEnvElementType // #SchematycTODO : Is this enum really necessary? It seems we only use IsElementAvailableInScope to check for actions and components.
		{
			Component,
			Action,
			Other
		};

		//////////////////////////////////////////////////////////////////////////
		inline bool IsElementAvailableInScope(const IScriptFile& file, const SGUID& scopeGUID, EEnvElementType ownerType, const SGUID& ownerGUID, const char* szNamespace)
		{
			switch(ownerType)
			{
			case EEnvElementType::Component:
				{
					return IsComponentInScope(file, scopeGUID, ownerGUID);
				}
			case EEnvElementType::Action:
				{
					return IsActionInScope(file, scopeGUID, ownerGUID);
				}
			case EEnvElementType::Other:
				{
					return IsElementAvailableInScope(file, scopeGUID, szNamespace); 
				}
			default:
				{
					return false;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// #SchematycTODO : Remove!!!
		inline bool IsSignalAvailableInScope(const IScriptFile& file, const SGUID& scopeGUID, const SGUID& ownerGUID, const char* szNamespace)
		{
			if(ownerGUID)
			{
				return IsComponentInScope(file, scopeGUID, ownerGUID) || IsActionInScope(file, scopeGUID, ownerGUID);
			}
			else
			{
				return IsElementAvailableInScope(file, scopeGUID, szNamespace);
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsPartnerStateMachine(const IScriptFile& file, const SGUID& stateMachineGUID)
		{
			const IScriptClass* pClass = FindOwnerClass(file, stateMachineGUID);
			if(pClass)
			{
				TScriptStateMachineConstVector	stateMachines;
				CollectStateMachines(file, pClass->GetGUID(), true, stateMachines);
				for(TScriptStateMachineConstVector::const_iterator iStateMachine = stateMachines.begin(), iEndStateMachine = stateMachines.end(); iStateMachine != iEndStateMachine; ++ iStateMachine)
				{
					if((*iStateMachine)->GetPartnerGUID() == stateMachineGUID)
					{
						return true;
					}
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsPartnerState(const IScriptFile& file, const SGUID& stateGUID)
		{
			const IScriptClass* pClass = FindOwnerClass(file, stateGUID);
			if(pClass)
			{
				TScriptStateConstVector	states;
				CollectStates(file, pClass->GetGUID(), true, states);
				for(TScriptStateConstVector::const_iterator iState = states.begin(), iEndState = states.end(); iState != iEndState; ++ iState)
				{
					if((*iState)->GetPartnerGUID() == stateGUID)
					{
						return true;
					}
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsActionAvailableInScope(const IScriptFile& file, const SGUID& scopeGUID, const IActionFactory& actionFactory)
		{
			const SGUID	componentGUID = actionFactory.GetComponentGUID();
			if(componentGUID)
			{
				return IsComponentInScope(file, scopeGUID, componentGUID);
			}
			else
			{
				return IsElementAvailableInScope(file, scopeGUID, actionFactory.GetNamespace());
			}
		}

		//////////////////////////////////////////////////////////////////////////
		inline size_t FindGraphInput(const IDocGraph& graph, const char* name)
		{
			SCHEMATYC2_SYSTEM_ASSERT(name);
			if(name)
			{
				for(size_t iInput = 0, inputCount = graph.GetInputCount(); iInput < inputCount; ++ iInput)
				{
					if(strcmp(graph.GetInputName(iInput), name) == 0)
					{
						return iInput;
					}
				}
			}
			return INVALID_INDEX;
		}

		//////////////////////////////////////////////////////////////////////////
		inline size_t FindGraphOutput(const IDocGraph& graph, const char* name)
		{
			SCHEMATYC2_SYSTEM_ASSERT(name);
			if(name)
			{
				for(size_t iOutput = 0, outputCount = graph.GetOutputCount(); iOutput < outputCount; ++ iOutput)
				{
					if(strcmp(graph.GetOutputName(iOutput), name) == 0)
					{
						return iOutput;
					}
				}
			}
			return INVALID_INDEX;
		}

		//////////////////////////////////////////////////////////////////////////
		inline size_t FindGraphNodeInput(const IScriptGraphNode& graphNode, const char* name)
		{
			SCHEMATYC2_SYSTEM_ASSERT(name);
			if(name)
			{
				for(size_t iInput = 0, inputCount = graphNode.GetInputCount(); iInput < inputCount; ++ iInput)
				{
					if(strcmp(graphNode.GetInputName(iInput), name) == 0)
					{
						return iInput;
					}
				}
			}
			return INVALID_INDEX;
		}

		//////////////////////////////////////////////////////////////////////////
		inline size_t FindGraphNodeOutput(const IScriptGraphNode& graphNode, const char* name)
		{
			SCHEMATYC2_SYSTEM_ASSERT(name);
			if(name)
			{
				for(size_t iOutput = 0, outputCount = graphNode.GetOutputCount(); iOutput < outputCount; ++ iOutput)
				{
					if(strcmp(graphNode.GetOutputName(iOutput), name) == 0)
					{
						return iOutput;
					}
				}
			}
			return INVALID_INDEX;
		}

		//////////////////////////////////////////////////////////////////////////
		inline void UnrollGraphSequenceRecursive(const IScriptGraph& graph, const IScriptGraphNode& node, size_t iActiveInput, size_t iActiveOutput, const TemplateUtils::CDelegate<void (const IScriptGraphNode&, EDocGraphSequenceStep, size_t)>& callback, GUIDVector& processedNodes)
		{
			SCHEMATYC2_SYSTEM_ASSERT(callback);
			if(callback)
			{
				for(size_t iDataInput = 0, inputCount = node.GetInputCount(); iDataInput < inputCount; ++ iDataInput)
				{
					if((node.GetInputFlags(iDataInput) & EScriptGraphPortFlags::Execute) == 0)
					{
						TScriptGraphLinkConstVector inputLinks;
						CollectGraphNodeInputLinks(graph, node.GetGUID(), node.GetInputName(iDataInput), inputLinks);
						for(TScriptGraphLinkConstVector::const_iterator iInputLink = inputLinks.begin(), iEndInputLink = inputLinks.end(); iInputLink != iEndInputLink; ++ iInputLink)
						{
							const IScriptGraphLink& inputLink = *(*iInputLink);
							const IScriptGraphNode* pPrevNode = graph.GetNode(inputLink.GetSrcNodeGUID());
							if(pPrevNode)
							{
								const size_t	iPrevNodeOutput = FindGraphNodeOutput(*pPrevNode, inputLink.GetSrcOutputName());
								if((pPrevNode->GetOutputFlags(iPrevNodeOutput) & EScriptGraphPortFlags::Pull) != 0)
								{
									callback(*pPrevNode, EDocGraphSequenceStep::PullOutput, FindGraphNodeOutput(*pPrevNode, inputLink.GetSrcOutputName()));
								}
							}
						}
					}
				}
				callback(node, EDocGraphSequenceStep::BeginInput, iActiveInput);
				const SGUID	nodeGUID = node.GetGUID();
				//if(std::find(processedNodes.begin(), processedNodes.end(), nodeGUID) == processedNodes.end())
				////////////////////////////////////////////////////////////////////////////////////////////////////
				if(std::find(processedNodes.begin(), processedNodes.end(), nodeGUID) == processedNodes.end())
				{
					processedNodes.push_back(nodeGUID);
				}
				////////////////////////////////////////////////////////////////////////////////////////////////////
				{
					//processedNodes.push_back(nodeGUID);
					if(iActiveOutput != INVALID_INDEX)
					{
						callback(node, EDocGraphSequenceStep::BeginOutput, iActiveOutput);
						TScriptGraphLinkConstVector outputLinks;
						CollectGraphNodeOutputLinks(graph, node.GetGUID(), node.GetOutputName(iActiveOutput), outputLinks);
						for(TScriptGraphLinkConstVector::const_iterator iOutputLink = outputLinks.begin(), iEndOutputLink = outputLinks.end(); iOutputLink != iEndOutputLink; ++ iOutputLink)
						{
							const IScriptGraphLink& outputLink = *(*iOutputLink);
							const IScriptGraphNode* pNextNode = graph.GetNode(outputLink.GetDstNodeGUID());
							if(pNextNode)
							{
								UnrollGraphSequenceRecursive(graph, *pNextNode, FindGraphNodeInput(*pNextNode, outputLink.GetDstInputName()), INVALID_INDEX, callback, processedNodes);
							}
						}
						callback(node, EDocGraphSequenceStep::EndOutput, iActiveInput);
					}
					else if((node.GetInputFlags(iActiveInput) & EScriptGraphPortFlags::EndSequence) == 0)
					{
						for(size_t iOutput = 0, outputCount = node.GetOutputCount(); iOutput < outputCount; ++ iOutput)
						{
							if((node.GetOutputFlags(iOutput) & EScriptGraphPortFlags::Execute) != 0)
							{
								callback(node, EDocGraphSequenceStep::BeginOutput, iOutput);
								TScriptGraphLinkConstVector	outputLinks;
								CollectGraphNodeOutputLinks(graph, node.GetGUID(), node.GetOutputName(iOutput), outputLinks);
								for(TScriptGraphLinkConstVector::const_iterator iOutputLink = outputLinks.begin(), iEndOutputLink = outputLinks.end(); iOutputLink != iEndOutputLink; ++ iOutputLink)
								{
									const IScriptGraphLink& outputLink = *(*iOutputLink);
									const IScriptGraphNode* pNextNode = graph.GetNode(outputLink.GetDstNodeGUID());
									if(pNextNode)
									{
										UnrollGraphSequenceRecursive(graph, *pNextNode, FindGraphNodeInput(*pNextNode, outputLink.GetDstInputName()), INVALID_INDEX, callback, processedNodes);
									}
								}
								callback(node, EDocGraphSequenceStep::EndOutput, iOutput);
							}
						}
					}
					callback(node, EDocGraphSequenceStep::EndInput, iActiveInput);
				}
			}
		}

		typedef TemplateUtils::CDelegate<void (const IScriptGraphNode&, EDocGraphSequenceStep, size_t)> GraphSequenceCallback;

		//////////////////////////////////////////////////////////////////////////
		inline void UnrollGraphSequence(const IScriptGraph& graph, const IScriptGraphNode& graphNode, size_t iActiveOutput, const GraphSequenceCallback& callback)
		{
			SCHEMATYC2_SYSTEM_ASSERT(callback);
			if(callback)
			{
				GUIDVector	processedNodes;
				callback(graphNode, EDocGraphSequenceStep::BeginSequence, iActiveOutput);
				UnrollGraphSequenceRecursive(graph, graphNode, INVALID_INDEX, iActiveOutput, callback, processedNodes);
				callback(graphNode, EDocGraphSequenceStep::EndSequence, iActiveOutput);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// #SchematycTODO : Replace with callback version above!
		inline void UnrollGraphSequenceRecursive_Deprecated(const IScriptGraph& graph, const SGUID& nodeGUID, GUIDVector& results)
		{
			const IScriptGraphNode* pGraphNode = graph.GetNode(nodeGUID);
			if(pGraphNode)
			{
				if(std::find(results.begin(), results.end(), nodeGUID) == results.end())
				{
					for(size_t iInput = 0, inputCount = pGraphNode->GetInputCount(); iInput < inputCount; ++ iInput)
					{
						const char*	inputName = pGraphNode->GetInputName(iInput);
						for(size_t iLink = 0, linkCount = graph.GetLinkCount(); iLink < linkCount; ++ iLink)
						{
							const IScriptGraphLink* pGraphLink = graph.GetLink(iLink);
							if((pGraphLink->GetDstNodeGUID() == nodeGUID) && (strcmp(pGraphLink->GetDstInputName(), inputName) == 0))
							{
								const SGUID             prevNodeGUID = pGraphLink->GetSrcNodeGUID();
								const IScriptGraphNode* pPrevGraphNode = graph.GetNode(prevNodeGUID);
								if(pPrevGraphNode)
								{
									const size_t	iPrevNodeOutput = FindGraphNodeOutput(*pPrevGraphNode, pGraphLink->GetSrcOutputName());
									if((pPrevGraphNode->GetOutputFlags(iPrevNodeOutput) & EScriptGraphPortFlags::Pull) != 0)
									{
										results.push_back(prevNodeGUID);
									}
								}
							}
						}
					}

					if(std::find(results.rbegin(), results.rend(), nodeGUID) == results.rend())
					{
						results.push_back(nodeGUID);
					}

					TScriptGraphLinkConstVector	activeOutputLinks;
					for(size_t iGraphNodeOutput = 0, graphNodeOutputCount = pGraphNode->GetOutputCount(); iGraphNodeOutput < graphNodeOutputCount; ++ iGraphNodeOutput)
					{
						if((pGraphNode->GetOutputFlags(iGraphNodeOutput) & EScriptGraphPortFlags::Execute) != 0)
						{
							const char*	outputName = pGraphNode->GetOutputName(iGraphNodeOutput);
							for(size_t iLink = 0, linkCount = graph.GetLinkCount(); iLink < linkCount; ++ iLink)
							{
								const IScriptGraphLink* pGraphLink = graph.GetLink(iLink);
								if((pGraphLink->GetSrcNodeGUID() == nodeGUID) && (strcmp(pGraphLink->GetSrcOutputName(), outputName) == 0))
								{
									activeOutputLinks.push_back(pGraphLink);
								}
							}
						}
					}

					for(TScriptGraphLinkConstVector::const_iterator iActiveOutputLink = activeOutputLinks.begin(), iEndActiveOutputLink = activeOutputLinks.end(); iActiveOutputLink != iEndActiveOutputLink; ++ iActiveOutputLink)
					{
						UnrollGraphSequenceRecursive_Deprecated(graph, (*iActiveOutputLink)->GetDstNodeGUID(), results);
					}
				}
			}
		}

		inline bool IsPrecedingNodeInGraphSequence(const IScriptGraph& graph, const IScriptGraphNode& sequenceNode, const SGUID& nodeGUID)
		{
			for(size_t iSequenceNodeInput = 0, sequenceNodeInputCount = sequenceNode.GetInputCount(); iSequenceNodeInput < sequenceNodeInputCount; ++ iSequenceNodeInput)
			{
				if((sequenceNode.GetInputFlags(iSequenceNodeInput) & EScriptGraphPortFlags::Execute) != 0)
				{
					TScriptGraphLinkConstVector inputLinks;
					CollectGraphNodeInputLinks(graph, sequenceNode.GetGUID(), sequenceNode.GetInputName(iSequenceNodeInput), inputLinks);
					for(const IScriptGraphLink* pLink : inputLinks)
					{
						const SGUID             srcNodeGUID = pLink->GetSrcNodeGUID();
						const IScriptGraphNode* pSrcNode = graph.GetNode(srcNodeGUID);
						if(pSrcNode)
						{
							const size_t iSrcNodeOutput = FindGraphNodeOutput(*pSrcNode, pLink->GetSrcOutputName());
							if((pSrcNode->GetOutputFlags(iSrcNodeOutput) & EScriptGraphPortFlags::BeginSequence) == 0)
							{
								if((srcNodeGUID == nodeGUID) || IsPrecedingNodeInGraphSequence(graph, *pSrcNode, nodeGUID))
								{
									return true;
								}
							}
						}
					}
				}
			}
			return false;
		}
	}

	namespace ScriptIncludeRecursionUtils	
	{
		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptElement&)> ElementVisitor;

		namespace Private
		{
			class CElementVisitWrapper
			{
			public:

				inline CElementVisitWrapper(const IScriptFile& file, const ElementVisitor& visitor, const SGUID& scopeGUID, EVisitFlags visitFlags)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_visitFlags(visitFlags)
				{
					m_file.VisitElements(ScriptElementConstVisitor::FromMemberFunction<CElementVisitWrapper, &CElementVisitWrapper::VisitElement>(*this), m_scopeGUID, (m_visitFlags & EVisitFlags::RecurseHierarchy) != 0);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CElementVisitWrapper, &CElementVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitElement(const IScriptElement& element)
				{
					m_visitor(m_file, element);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CElementVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_visitFlags);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				ElementVisitor     m_visitor;
				SGUID              m_scopeGUID;
				EVisitFlags        m_visitFlags;
			};
		}

		inline void VisitElements(const IScriptFile& file, const ElementVisitor& visitor, const SGUID& scopeGUID, EVisitFlags visitFlags)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CElementVisitWrapper(file, visitor, scopeGUID, visitFlags);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptElement*> GetElementResult;

		namespace Private
		{
			class CGetElementWrapper
			{
			public:

				inline CGetElementWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptElement* pElement = m_file.GetElement(m_guid);
					if(pElement)
					{
						m_result.first  = &m_file;
						m_result.second = pElement;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetElementWrapper, &CGetElementWrapper::VisitInclude>(*this));
					}
				}

				inline GetElementResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetElementWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				SGUID              m_guid;
				GetElementResult   m_result;
			};
		}

		inline GetElementResult GetElement(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetElementWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptInclude&)> IncludeVisitor;

		namespace Private
		{
			class CIncludeVisitWrapper
			{
			public:

				inline CIncludeVisitWrapper(const IScriptFile& file, const IncludeVisitor& visitor)
					: m_file(file)
					, m_visitor(visitor)
				{
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CIncludeVisitWrapper, &CIncludeVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					m_visitor(m_file, include);
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CIncludeVisitWrapper(*pIncludeFile, m_visitor);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				IncludeVisitor     m_visitor;
			};
		}

		inline void VisitIncludes(const IScriptFile& file, const IncludeVisitor& visitor)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CIncludeVisitWrapper(file, visitor);
			}
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptEnumeration&)> EnumerationVisitor;

		namespace Private
		{
			class CEnumerationVisitWrapper
			{
			public:

				inline CEnumerationVisitWrapper(const IScriptFile& file, const EnumerationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_bRecurseHierarchy(bRecurseHierarchy)
				{
					m_file.VisitEnumerations(ScriptEnumerationConstVisitor::FromMemberFunction<CEnumerationVisitWrapper, &CEnumerationVisitWrapper::VisitEnumeration>(*this), m_scopeGUID, m_bRecurseHierarchy);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CEnumerationVisitWrapper, &CEnumerationVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitEnumeration(const IScriptEnumeration& enumeration)
				{
					m_visitor(m_file, enumeration);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CEnumerationVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_bRecurseHierarchy);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				EnumerationVisitor m_visitor;
				SGUID              m_scopeGUID;
				bool               m_bRecurseHierarchy;
			};
		}

		inline void VisitEnumerations(const IScriptFile& file, const EnumerationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CEnumerationVisitWrapper(file, visitor, scopeGUID, bRecurseHierarchy);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptEnumeration*> TGetEnumerationResult;

		namespace Private
		{
			class CGetEnumerationWrapper
			{
			public:

				inline CGetEnumerationWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptEnumeration* pEnumeration = m_file.GetEnumeration(m_guid);
					if(pEnumeration)
					{
						m_result.first  = &m_file;
						m_result.second = pEnumeration;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetEnumerationWrapper, &CGetEnumerationWrapper::VisitInclude>(*this));
					}
				}

				inline TGetEnumerationResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetEnumerationWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&    m_file;
				SGUID                 m_guid;
				TGetEnumerationResult m_result;
			};
		}

		inline TGetEnumerationResult GetEnumeration(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetEnumerationWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptStructure&)> StructureVisitor;

		namespace Private
		{
			class CStructureVisitWrapper
			{
			public:

				inline CStructureVisitWrapper(const IScriptFile& file, const StructureVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_bRecurseHierarchy(bRecurseHierarchy)
				{
					m_file.VisitStructures(ScriptStructureConstVisitor::FromMemberFunction<CStructureVisitWrapper, &CStructureVisitWrapper::VisitStructure>(*this), m_scopeGUID, m_bRecurseHierarchy);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CStructureVisitWrapper, &CStructureVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitStructure(const IScriptStructure& structure)
				{
					m_visitor(m_file, structure);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CStructureVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_bRecurseHierarchy);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				StructureVisitor   m_visitor;
				SGUID              m_scopeGUID;
				bool               m_bRecurseHierarchy;
			};
		}

		inline void VisitStructures(const IScriptFile& file, const StructureVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CStructureVisitWrapper(file, visitor, scopeGUID, bRecurseHierarchy);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptStructure*> TGetStructureResult;

		namespace Private
		{
			class CGetStructureWrapper
			{
			public:

				inline CGetStructureWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptStructure* pStructure = m_file.GetStructure(m_guid);
					if(pStructure)
					{
						m_result.first  = &m_file;
						m_result.second = pStructure;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetStructureWrapper, &CGetStructureWrapper::VisitInclude>(*this));
					}
				}

				inline TGetStructureResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetStructureWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&  m_file;
				SGUID               m_guid;
				TGetStructureResult m_result;
			};
		}

		inline TGetStructureResult GetStructure(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetStructureWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptSignal&)> SignalVisitor;

		namespace Private
		{
			class CSignalVisitWrapper
			{
			public:

				inline CSignalVisitWrapper(const IScriptFile& file, const SignalVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_bRecurseHierarchy(bRecurseHierarchy)
				{
					m_file.VisitSignals(ScriptSignalConstVisitor::FromMemberFunction<CSignalVisitWrapper, &CSignalVisitWrapper::VisitSignal>(*this), m_scopeGUID, m_bRecurseHierarchy);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CSignalVisitWrapper, &CSignalVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitSignal(const IScriptSignal& signal)
				{
					m_visitor(m_file, signal);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CSignalVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_bRecurseHierarchy);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				SignalVisitor      m_visitor;
				SGUID              m_scopeGUID;
				bool               m_bRecurseHierarchy;
			};
		}

		inline void VisitSignals(const IScriptFile& file, const SignalVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CSignalVisitWrapper(file, visitor, scopeGUID, bRecurseHierarchy);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptSignal*> TGetSignalResult;

		namespace Private
		{
			class CGetSignalWrapper
			{
			public:

				inline CGetSignalWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptSignal* pSignal = m_file.GetSignal(m_guid);
					if(pSignal)
					{
						m_result.first  = &m_file;
						m_result.second = pSignal;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetSignalWrapper, &CGetSignalWrapper::VisitInclude>(*this));
					}
				}

				inline TGetSignalResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetSignalWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				SGUID              m_guid;
				TGetSignalResult   m_result;
			};
		}

		inline TGetSignalResult GetSignal(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetSignalWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptAbstractInterface&)> AbstractInterfaceVisitor;

		namespace Private
		{
			class CAbstractInterfaceVisitWrapper
			{
			public:

				inline CAbstractInterfaceVisitWrapper(const IScriptFile& file, const AbstractInterfaceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_bRecurseHierarchy(bRecurseHierarchy)
				{
					m_file.VisitAbstractInterfaces(ScriptAbstractInterfaceConstVisitor::FromMemberFunction<CAbstractInterfaceVisitWrapper, &CAbstractInterfaceVisitWrapper::VisitAbstractInterface>(*this), m_scopeGUID, m_bRecurseHierarchy);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CAbstractInterfaceVisitWrapper, &CAbstractInterfaceVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitAbstractInterface(const IScriptAbstractInterface& abstractInterface)
				{
					m_visitor(m_file, abstractInterface);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CAbstractInterfaceVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_bRecurseHierarchy);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&       m_file;
				AbstractInterfaceVisitor m_visitor;
				SGUID                    m_scopeGUID;
				bool                     m_bRecurseHierarchy;
			};
		}

		inline void VisitAbstractInterfaces(const IScriptFile& file, const AbstractInterfaceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CAbstractInterfaceVisitWrapper(file, visitor, scopeGUID, bRecurseHierarchy);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptAbstractInterface*> TGetAbstractInterfaceResult;

		namespace Private
		{
			class CGetAbstractInterfaceWrapper
			{
			public:

				inline CGetAbstractInterfaceWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptAbstractInterface* pAbstractInterface = m_file.GetAbstractInterface(m_guid);
					if(pAbstractInterface)
					{
						m_result.first  = &m_file;
						m_result.second = pAbstractInterface;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetAbstractInterfaceWrapper, &CGetAbstractInterfaceWrapper::VisitInclude>(*this));
					}
				}

				inline TGetAbstractInterfaceResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetAbstractInterfaceWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&          m_file;
				SGUID                       m_guid;
				TGetAbstractInterfaceResult m_result;
			};
		}

		inline TGetAbstractInterfaceResult GetAbstractInterface(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetAbstractInterfaceWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptAbstractInterfaceFunction&)> AbstractInterfaceFunctionVisitor;

		namespace Private
		{
			class CAbstractInterfaceFunctionVisitWrapper
			{
			public:

				inline CAbstractInterfaceFunctionVisitWrapper(const IScriptFile& file, const AbstractInterfaceFunctionVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_bRecurseHierarchy(bRecurseHierarchy)
				{
					m_file.VisitAbstractInterfaceFunctions(ScriptAbstractInterfaceFunctionConstVisitor::FromMemberFunction<CAbstractInterfaceFunctionVisitWrapper, &CAbstractInterfaceFunctionVisitWrapper::VisitAbstractInterfaceFunction>(*this), m_scopeGUID, m_bRecurseHierarchy);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CAbstractInterfaceFunctionVisitWrapper, &CAbstractInterfaceFunctionVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitAbstractInterfaceFunction(const IScriptAbstractInterfaceFunction& abstractInterfaceFunction)
				{
					m_visitor(m_file, abstractInterfaceFunction);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CAbstractInterfaceFunctionVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_bRecurseHierarchy);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&               m_file;
				AbstractInterfaceFunctionVisitor m_visitor;
				SGUID                            m_scopeGUID;
				bool                             m_bRecurseHierarchy;
			};
		}

		inline void VisitAbstractInterfaceFunctions(const IScriptFile& file, const AbstractInterfaceFunctionVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CAbstractInterfaceFunctionVisitWrapper(file, visitor, scopeGUID, bRecurseHierarchy);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptAbstractInterfaceFunction*> TGetAbstractInterfaceFunctionResult;

		namespace Private
		{
			class CGetAbstractInterfaceFunctionWrapper
			{
			public:

				inline CGetAbstractInterfaceFunctionWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptAbstractInterfaceFunction* pAbstractInterfaceFunction = m_file.GetAbstractInterfaceFunction(m_guid);
					if(pAbstractInterfaceFunction)
					{
						m_result.first  = &m_file;
						m_result.second = pAbstractInterfaceFunction;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetAbstractInterfaceFunctionWrapper, &CGetAbstractInterfaceFunctionWrapper::VisitInclude>(*this));
					}
				}

				inline TGetAbstractInterfaceFunctionResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetAbstractInterfaceFunctionWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile&                  m_file;
				SGUID                               m_guid;
				TGetAbstractInterfaceFunctionResult m_result;
			};
		}

		inline TGetAbstractInterfaceFunctionResult GetAbstractInterfaceFunction(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetAbstractInterfaceFunctionWrapper(file, guid).GetResult();
		}

		typedef TemplateUtils::CDelegate<void (const IScriptFile&, const IScriptClass&)> ClassVisitor;

		namespace Private
		{
			class CClassVisitWrapper
			{
			public:

				inline CClassVisitWrapper(const IScriptFile& file, const ClassVisitor& visitor, const SGUID& scopeGUID, EVisitFlags visitFlags)
					: m_file(file)
					, m_visitor(visitor)
					, m_scopeGUID(scopeGUID)
					, m_visitFlags(visitFlags)
				{
					m_file.VisitClasses(ScriptClassConstVisitor::FromMemberFunction<CClassVisitWrapper, &CClassVisitWrapper::VisitClass>(*this), m_scopeGUID, (m_visitFlags & EVisitFlags::RecurseHierarchy) != 0);
					m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CClassVisitWrapper, &CClassVisitWrapper::VisitInclude>(*this));
				}

				inline EVisitStatus VisitClass(const IScriptClass& _class)
				{
					m_visitor(m_file, _class);
					return EVisitStatus::Continue;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						CClassVisitWrapper(*pIncludeFile, m_visitor, m_scopeGUID, m_visitFlags);
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				ClassVisitor       m_visitor;
				SGUID              m_scopeGUID;
				EVisitFlags        m_visitFlags;
			};
		}

		inline void VisitClasses(const IScriptFile& file, const ClassVisitor& visitor, const SGUID& scopeGUID, EVisitFlags visitFlags)
		{
			SCHEMATYC2_SYSTEM_ASSERT(visitor);
			if(visitor)
			{
				Private::CClassVisitWrapper(file, visitor, scopeGUID, visitFlags);
			}
		}

		typedef std::pair<const IScriptFile*, const IScriptClass*> GetClassResult;

		namespace Private
		{
			class CGetClassWrapper
			{
			public:

				inline CGetClassWrapper(const IScriptFile& file, const SGUID& guid)
					: m_file(file)
					, m_guid(guid)
				{
					const IScriptClass* pClass = m_file.GetClass(m_guid);
					if(pClass)
					{
						m_result.first  = &m_file;
						m_result.second = pClass;
					}
					else
					{
						m_file.VisitIncludes(ScriptIncludeConstVisitor::FromMemberFunction<CGetClassWrapper, &CGetClassWrapper::VisitInclude>(*this));
					}
				}

				inline GetClassResult GetResult() const
				{
					return m_result;
				}

				inline EVisitStatus VisitInclude(const IScriptInclude& include)
				{
					const IScriptFile* pIncludeFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(include.GetRefGUID());
					if(pIncludeFile)
					{
						m_result = CGetClassWrapper(*pIncludeFile, m_guid).GetResult();
						if(m_result.second)
						{
							return EVisitStatus::Stop;
						}
					}
					return EVisitStatus::Continue;
				}

			private:

				const IScriptFile& m_file;
				SGUID              m_guid;
				GetClassResult     m_result;
			};
		}

		inline GetClassResult GetClass(const IScriptFile& file, const SGUID& guid)
		{
			return Private::CGetClassWrapper(file, guid).GetResult();
		}
	}
}
