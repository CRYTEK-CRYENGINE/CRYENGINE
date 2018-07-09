// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptInterfaceImpl.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvInterface.h>
#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Script/Elements/IScriptInterface.h>
#include <CrySchematyc/Script/Elements/IScriptInterfaceFunction.h>
#include <CrySchematyc/Script/Elements/IScriptInterfaceTask.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/SerializationUtils.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

namespace Schematyc
{
CScriptInterfaceImpl::CScriptInterfaceImpl() {}

CScriptInterfaceImpl::CScriptInterfaceImpl(const CryGUID& guid, EDomain domain, const CryGUID& refGUID)
	: CScriptElementBase(guid, nullptr, EScriptElementFlags::FixedName)
	, m_domain(domain)
	, m_refGUID(refGUID)
{}

void CScriptInterfaceImpl::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const
{
	SCHEMATYC_CORE_ASSERT(enumerator);
	if (enumerator)
	{
		if (m_domain == EDomain::Script)
		{
			enumerator(m_refGUID);
		}
	}
}

void CScriptInterfaceImpl::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_refGUID = guidRemapper.Remap(m_refGUID);
}

void CScriptInterfaceImpl::ProcessEvent(const SScriptEvent& event)
{
	if ((event.id == EScriptEventId::EditorAdd) || (event.id == EScriptEventId::EditorFixUp) || (event.id == EScriptEventId::EditorPaste) || (event.id == EScriptEventId::EditorDependencyModified))
	{
		switch (m_domain)
		{
		case EDomain::Env:
			{
				const IEnvInterface* pEnvInterface = gEnv->pSchematyc->GetEnvRegistry().GetInterface(m_refGUID);
				if (pEnvInterface)
				{
					CScriptElementBase::SetName(pEnvInterface->GetName());
					RefreshEnvInterfaceFunctions(*pEnvInterface);
				}
				break;
			}
		case EDomain::Script:
			{
				/*ScriptIncludeRecursionUtils::TGetInterfaceResult scriptInterface = ScriptIncludeRecursionUtils::GetInterface(CScriptElementBase::GetFile(), m_refGUID);
				   if(scriptInterface.second)
				   {
				   CScriptElementBase::SetName(scriptInterface.second->GetName());
				   RefreshScriptInterfaceFunctions(*scriptInterface.first);
				   RefreshScriptInterfaceTasks(*scriptInterface.first);
				   }*/
				break;
			}
		}
	}
}

void CScriptInterfaceImpl::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

EDomain CScriptInterfaceImpl::GetDomain() const
{
	return m_domain;
}

CryGUID CScriptInterfaceImpl::GetRefGUID() const
{
	return m_refGUID;
}

void CScriptInterfaceImpl::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_domain, "m_domain");
	archive(m_refGUID, "m_refGUID");
}

void CScriptInterfaceImpl::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_domain, "m_domain");
	archive(m_refGUID, "m_refGUID");
}

void CScriptInterfaceImpl::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (!GUID::IsEmpty(m_refGUID))
	{
		switch (m_domain)
		{
		case EDomain::Env:
			{
				const IEnvInterface* pEnvInterface = gEnv->pSchematyc->GetEnvRegistry().GetInterface(m_refGUID);
				if (!pEnvInterface)
				{
					archive.error(*this, "Unable to retrieve interface!");
				}
				break;
			}
		case EDomain::Script:
			{
				/*ScriptIncludeRecursionUtils::TGetInterfaceResult scriptInterface = ScriptIncludeRecursionUtils::GetInterface(CScriptElementBase::GetFile(), m_refGUID);
				   if(!scriptInterface.second)
				   {
				   archive.error(*this, "Unable to retrieve interface!");
				   }*/
				break;
			}
		}
	}
}

void CScriptInterfaceImpl::RefreshEnvInterfaceFunctions(const IEnvInterface& envInterface)
{
	/*
	   // Collect graphs.
	   IScriptFile&    file = CScriptElementBase::GetFile();
	   const CryGUID     guid = CScriptElementBase::GetGUID();
	   TDocGraphVector graphs;
	   DocUtils::CollectGraphs(file, guid, false, graphs);
	   // Iterate through all functions in interface and create corresponding graphs (if they don't already exist).
	   for(const IEnvElement* pEnvInterfaceChild = envInterface.GetFirstChild(); pEnvInterfaceChild; pEnvInterfaceChild = pEnvInterfaceChild->GetNextSibling())
	   {
	   if(pEnvInterfaceChild->GetType() == EEnvElementType::InterfaceFunction)
	   {
	    const IEnvInterfaceFunction& envInterfaceFunction = EnvElement::Cast<const IEnvInterfaceFunction>(*pEnvInterfaceChild);
	    const CryGUID                  functionGUID = envInterfaceFunction.GetGUID();
	    bool                         bGraphExists = false;
	    for(const IDocGraph* pGraph : graphs)
	    {
	      if(pGraph->GetContextGUID() == functionGUID)
	      {
	        bGraphExists = true;
	        break;
	      }
	    }
	    if(!bGraphExists)
	    {
	      IDocGraph* pGraph = file.AddGraph(SScriptGraphParams(guid, envInterfaceFunction.GetName(), EScriptGraphType::InterfaceFunction, functionGUID));
	      CRY_ASSERT(pGraph);
	      if(pGraph)
	      {
	        pGraph->AddNode(EScriptGraphNodeType::Begin, CryGUID(), CryGUID(), Vec2(0.0f, 0.0f));	// #SchematycTODO : This should be handled by the graph itself!!!
	        graphs.push_back(pGraph);
	      }
	    }
	   }
	   }
	 */
}

/*void CScriptInterfaceImpl::RefreshScriptInterfaceFunctions(const IScriptFile& interfaceFile)
   {
   typedef std::map<CryGUID, IDocGraph*> FunctionGraphs;

   IScriptFile&   file = CScriptElementBase::GetFile();
   FunctionGraphs functionGraphs;

   auto visitGraph = [&functionGraphs] (IDocGraph& graph) -> EVisitStatus
   {
    if(graph.GetType() == EScriptGraphType::InterfaceFunction)
    {
      functionGraphs.insert(FunctionGraphs::value_type(graph.GetContextGUID(), &graph));
    }
    return EVisitStatus::Continue;
   };
   file.VisitGraphs(visitGraph, CScriptElementBase::GetGUID(), false);

   auto visitInterfaceFunction = [this, &file, &functionGraphs] (const IScriptInterfaceFunction& function) -> EVisitStatus
   {
    const CryGUID              functionGUID = function.GetGUID();
    FunctionGraphs::iterator itFunctionGraph = functionGraphs.find(functionGUID);
    if(itFunctionGraph == functionGraphs.end())
    {
      IDocGraph* pGraph = file.AddGraph(SScriptGraphParams(CScriptElementBase::GetGUID(), function.GetName(), EScriptGraphType::InterfaceFunction, functionGUID));
      CRY_ASSERT(pGraph);
      if(pGraph)
      {
        pGraph->AddNode(EScriptGraphNodeType::Begin, CryGUID(), CryGUID(), Vec2(0.0f, 0.0f)); // #SchematycTODO : This should be handled by the graph itself!!!
      }
    }
    else
    {
      functionGraphs.erase(itFunctionGraph);
    }
    return EVisitStatus::Continue;
   };
   interfaceFile.VisitInterfaceFunctions(visitInterfaceFunction, m_refGUID, false);

   for(FunctionGraphs::value_type& functionGraph : functionGraphs)
   {
    functionGraph.second->SetFlags(EScriptElementFlags::Discard);
   }
   }*/

/*void CScriptInterfaceImpl::RefreshScriptInterfaceTasks(const IScriptFile& interfaceFile)
   {
   typedef std::unordered_map<CryGUID, IScriptStateMachine*> TaskStateMachines;

   IScriptFile&      file = CScriptElementBase::GetFile();
   TaskStateMachines taskStateMachines;

   auto visitStateMachine = [&taskStateMachines] (IScriptStateMachine& stateMachine) -> EVisitStatus
   {
    if(stateMachine.GetLifetime() == EScriptStateMachineLifetime::Task)
    {
      taskStateMachines.insert(TaskStateMachines::value_type(stateMachine.GetContextGUID(), &stateMachine));
    }
    return EVisitStatus::Continue;
   };
   file.VisitStateMachines(visitStateMachine, CScriptElementBase::GetGUID(), false);

   auto visitInterfaceTask = [this, &interfaceFile, &file, &taskStateMachines] (const IScriptInterfaceTask& task) -> EVisitStatus
   {
    const CryGUID                 taskGUID = task.GetGUID();
    TaskStateMachines::iterator itTaskStateMachine = taskStateMachines.find(taskGUID);
    if(itTaskStateMachine == taskStateMachines.end())
    {
      file.AddStateMachine(CScriptElementBase::GetGUID(), task.GetName(), EScriptStateMachineLifetime::Task, taskGUID, CryGUID());
    }
    else
    {
      taskStateMachines.erase(itTaskStateMachine);
    }
    RefreshScriptInterfaceTaskPropertiess(interfaceFile, taskGUID);
    return EVisitStatus::Continue;
   };
   interfaceFile.VisitInterfaceTasks(visitInterfaceTask, m_refGUID, false);

   for(TaskStateMachines::value_type& taskStateMachine : taskStateMachines)
   {
    taskStateMachine.second->SetFlags(EScriptElementFlags::Discard);
   }
   }*/

/*void CScriptInterfaceImpl::RefreshScriptInterfaceTaskPropertiess(const IScriptFile& interfaceFile, const CryGUID& taskGUID)
   {
   }*/

} // Schematyc
