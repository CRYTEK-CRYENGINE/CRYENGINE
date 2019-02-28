// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FlowSystem.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#include "FlowGraph.h"
#include "Nodes/FlowLogNode.h"
#include "Nodes/FlowStartNode.h"
#include "Nodes/FlowTrackEventNode.h"
#include "Nodes/FlowDelayNode.h"

#include "Nodes/FlowScriptedNode.h"
#include "Nodes/FlowCompositeNode.h"
#include "Nodes/FlowEntityNode.h"

#include "Inspectors/FlowInspectorDefault.h"
#include "Modules/ModuleManager.h"

#include <CrySystem/ITimer.h>

#include <CryString/CryPath.h>

#include "../GameTokens/GameTokenSystem.h"

#define BLACKLIST_FILE_PATH     "Libs/FlowNodes/FlownodeBlacklist.xml"
#define ENTITY_NODE_PREFIX      "entity:"

#define GRAPH_RESERVED_CAPACITY 8

#ifndef _RELEASE
	#define SHOW_FG_CRITICAL_LOADING_ERROR_ON_SCREEN
#endif

#ifndef _RELEASE
CFlowSystem::TSFGProfile CFlowSystem::FGProfile;
#endif //_RELEASE

template<class T>
class CAutoFlowFactory : public IFlowNodeFactory
{
public:
	CAutoFlowFactory() {}
	IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo) { return new T(pActInfo); }
	void         GetMemoryUsage(ICrySizer* s) const
	{
		SIZER_SUBCOMPONENT_NAME(s, "CAutoFlowFactory");
		s->Add(*this);
	}

	void Reset() {}
};

template<class T>
class CSingletonFlowFactory : public IFlowNodeFactory
{
public:
	CSingletonFlowFactory() { m_pInstance = new T(); }
	void GetMemoryUsage(ICrySizer* s) const
	{
		SIZER_SUBCOMPONENT_NAME(s, "CSingletonFlowFactory");
		s->Add(*this);
		s->AddObject(m_pInstance);
	}
	IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo)
	{
		return m_pInstance;
	}

	void Reset() {}

private:
	IFlowNodePtr m_pInstance;
};

// FlowSystem Container
void CFlowSystemContainer::AddItem(TFlowInputData item)
{
	m_container.push_back(item);
}

void CFlowSystemContainer::AddItemUnique(TFlowInputData item)
{
	if (std::find(m_container.begin(), m_container.end(), item) != m_container.end())
	{
		m_container.push_back(item);
	}
}

void CFlowSystemContainer::RemoveItem(TFlowInputData item)
{
	m_container.erase(std::remove(m_container.begin(), m_container.end(), item), m_container.end());
}

TFlowInputData CFlowSystemContainer::GetItem(int i)
{
	return m_container[i];
}

void CFlowSystemContainer::RemoveItemAt(int i)
{
	m_container.erase(m_container.begin() + i);
}

void CFlowSystemContainer::Clear()
{
	m_container.clear();
}

int CFlowSystemContainer::GetItemCount() const
{
	return m_container.size();
}

void CFlowSystemContainer::GetMemoryUsage(ICrySizer* s) const
{
	if (GetItemCount() > 0)
		s->Add(&m_container[0], m_container.capacity());

	s->Add(m_container);
}

void CFlowSystemContainer::Serialize(TSerialize ser)
{
	int count = m_container.size();

	ser.Value("count", count);

	if (ser.IsWriting())
	{
		for (int i = 0; i < count; i++)
		{
			ser.BeginGroup("ContainerInfo");
			m_container[i].Serialize(ser);
			ser.EndGroup();
		}
	}
	else
	{
		for (int i = 0; i < count; i++)
		{
			ser.BeginGroup("ContainerInfo");
			TFlowInputData data;
			data.Serialize(ser);
			m_container.push_back(data);
			ser.EndGroup();
		}
	}
}

CFlowSystem::CFlowSystem()
	: m_bInspectingEnabled(false)
	, m_needToUpdateForwardings(false)
	, m_criticalLoadingErrorHappened(false)
	, m_graphs(GRAPH_RESERVED_CAPACITY)
	, m_nextFlowGraphId(0)
	, m_pModuleManager(NULL)
	, m_blacklistNode(NULL)
	, m_nextNodeTypeID(InvalidFlowNodeTypeId)
	, m_bRegisteredDefaultNodes(false)
{
	LOADING_TIME_PROFILE_SECTION;

	LoadBlacklistedFlownodeXML();

	m_pGameTokenSystem = new CGameTokenSystem;
	gEnv->pEntitySystem->GetClassRegistry()->RegisterListener(this);
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CFlowSystem");
}

void CFlowSystem::PreInit()
{
	LOADING_TIME_PROFILE_SECTION;

	m_pModuleManager = new CFlowGraphModuleManager();
	RegisterAllNodeTypes();

	m_pDefaultInspector = new CFlowInspectorDefault(this);
	RegisterInspector(m_pDefaultInspector, 0);
	EnableInspecting(false);
}

CFlowSystem::~CFlowSystem()
{
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	gEnv->pEntitySystem->GetClassRegistry()->UnregisterListener(this);

	for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
	{
		itGraph->NotifyFlowSystemDestroyed();
	}
	if (m_pGameTokenSystem)
	{
		delete m_pGameTokenSystem;
		m_pGameTokenSystem = nullptr;
	}
	Shutdown();

	// FlowSystem is not usable anymore
	gEnv->pFlowSystem = nullptr;
}

void CFlowSystem::Release()
{
	UnregisterInspector(m_pDefaultInspector, 0);
	delete this;
}

IFlowGraphPtr CFlowSystem::CreateFlowGraph()
{
	return new CFlowGraph(this);
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::ReloadAllNodeTypes()
{
	if (gEnv->IsEditor())
	{
		RegisterAllNodeTypes();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterAllNodeTypes()
{
	// clear node types
	m_typeNameToIdMap.clear();
	m_typeRegistryVec.clear();

	// reset TypeIDs
	m_freeNodeTypeIDs.clear();
	m_nextNodeTypeID = InvalidFlowNodeTypeId;

	// register all types
	TFlowNodeTypeId typeId = RegisterType("InvalidType", 0);
	assert(typeId == InvalidFlowNodeTypeId);
	RegisterType("Debug:Log", new CSingletonFlowFactory<CFlowLogNode>());
	RegisterType("Game:Start", new CAutoFlowFactory<CFlowStartNode>());
	RegisterType("TrackEvent", new CAutoFlowFactory<CFlowTrackEventNode>());

	LoadExtensions("Libs/FlowNodes");

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_REGISTER_FLOWNODES, 0, 0);

	m_bRegisteredDefaultNodes = true;
}

void CFlowSystem::LoadExtensions(string path)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	path.TrimRight("/\\");
	string search = path + "/*.node";
	intptr_t handle = pCryPak->FindFirst(search.c_str(), &fd);
	if (handle != -1)
	{
		int res = 0;

		TPendingComposites pendingComposites;

		do
		{
			cry_strcpy(filename, path.c_str());
			cry_strcat(filename, "/");
			cry_strcat(filename, fd.name);

			XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename);
			if (root)
				LoadExtensionFromXml(root, &pendingComposites);

			res = pCryPak->FindNext(handle, &fd);
		}
		while (res >= 0);

		LoadComposites(&pendingComposites);

		pCryPak->FindClose(handle);
	}
}

void CFlowSystem::LoadExtensionFromXml(XmlNodeRef nodeParent, TPendingComposites* pComposites)
{
	XmlNodeRef node;
	int nChildren = nodeParent->getChildCount();
	int iChild;

	// run for nodeParent, and any children it contains
	for (iChild = -1, node = nodeParent;; )
	{
		const char* tag = node->getTag();
		if (!tag || !*tag)
			return;

		// Multiple components may be contained within a <NodeList> </NodeList> pair
		if ((0 == strcmp("NodeList", tag)) || (0 == strcmp("/NodeList", tag)))
		{
			// a list node, do nothing.
			// if root level, will advance to child below
		}
		else
		{
			const char* name = node->getAttr("name");
			if (!name || !*name)
				return;

			if (0 == strcmp("Script", tag))
			{
				const char* path = node->getAttr("script");
				if (!path || !*path)
					return;
				CFlowScriptedNodeFactoryPtr pFactory = new CFlowScriptedNodeFactory();
				if (pFactory->Init(path, name))
					RegisterType(name, &*pFactory);
			}
			else if (0 == strcmp("SimpleScript", tag))
			{
				const char* path = node->getAttr("script");
				if (!path || !*path)
					return;
				CFlowSimpleScriptedNodeFactoryPtr pFactory = new CFlowSimpleScriptedNodeFactory();
				if (pFactory->Init(path, name))
					RegisterType(name, &*pFactory);
			}
			else if (0 == strcmp("Composite", tag))
			{
				pComposites->push(node);
			}
		}

		if (++iChild >= nChildren)
			break;
		node = nodeParent->getChild(iChild);
	}
}

void CFlowSystem::LoadComposites(TPendingComposites* pComposites)
{
	size_t failCount = 0;
	while (!pComposites->empty() && failCount < pComposites->size())
	{
		CFlowCompositeNodeFactoryPtr pFactory = new CFlowCompositeNodeFactory();
		switch (pFactory->Init(pComposites->front(), this))
		{
		case CFlowCompositeNodeFactory::eIR_Failed:
			GameWarning("Failed to load composite due to invalid data: %s", pComposites->front()->getAttr("name"));
			pComposites->pop();
			failCount = 0;
			break;
		case CFlowCompositeNodeFactory::eIR_Ok:
			RegisterType(pComposites->front()->getAttr("name"), &*pFactory);
			pComposites->pop();
			failCount = 0;
			break;
		case CFlowCompositeNodeFactory::eIR_NotYet:
			pComposites->push(pComposites->front());
			pComposites->pop();
			failCount++;
			break;
		}
	}

	while (!pComposites->empty())
	{
		GameWarning("Failed to load composite due to failed dependency: %s", pComposites->front()->getAttr("name"));
		pComposites->pop();
	}
}

class CFlowSystem::CNodeTypeIterator : public IFlowNodeTypeIterator
{
public:
	CNodeTypeIterator(CFlowSystem* pImpl) : m_nRefs(0), m_pImpl(pImpl), m_iter(pImpl->m_typeRegistryVec.begin())
	{
		assert(m_iter != m_pImpl->m_typeRegistryVec.end());
		++m_iter;
		m_id = InvalidFlowNodeTypeId;
		assert(m_id == 0);
	}
	void AddRef()
	{
		++m_nRefs;
	}
	void Release()
	{
		if (0 == --m_nRefs)
			delete this;
	}
	bool Next(SNodeType& nodeType)
	{
		while (m_iter != m_pImpl->m_typeRegistryVec.end() && !stricmp(m_iter->name, ""))
		{
			++m_id;
			++m_iter;
		}

		if (m_iter == m_pImpl->m_typeRegistryVec.end())
			return false;

		nodeType.typeId = ++m_id;
		nodeType.typeName = m_iter->name;
		++m_iter;
		return true;
	}
private:
	int                              m_nRefs;
	TFlowNodeTypeId                  m_id;
	CFlowSystem*                     m_pImpl;
	std::vector<STypeInfo>::iterator m_iter;
};

IFlowNodeTypeIteratorPtr CFlowSystem::CreateNodeTypeIterator()
{
	return new CNodeTypeIterator(this);
}

TFlowNodeTypeId CFlowSystem::GenerateNodeTypeID()
{
	if (!m_freeNodeTypeIDs.empty())
	{
		TFlowNodeTypeId typeID = m_freeNodeTypeIDs.back();
		m_freeNodeTypeIDs.pop_back();
		return typeID;
	}

	if (m_nextNodeTypeID > CFlowData::TYPE_MAX_COUNT)
	{
		CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "CFlowSystem::GenerateNodeTypeID: Reached maximum amount of NodeTypes: Limit=%d", CFlowData::TYPE_MAX_COUNT);
		return InvalidFlowNodeTypeId;
	}

	return m_nextNodeTypeID++;
}

TFlowNodeTypeId CFlowSystem::RegisterType(const char* type, IFlowNodeFactoryPtr factory)
{
	if (!BlacklistedFlownode(&type))
		return InvalidFlowNodeTypeId;

	string typeName = type;
	const TTypeNameToIdMap::const_iterator iter = m_typeNameToIdMap.find(typeName);
	if (iter == m_typeNameToIdMap.end())
	{
		const TFlowNodeTypeId nTypeId = GenerateNodeTypeID();
		if ((nTypeId == InvalidFlowNodeTypeId) && typeName.compareNoCase("InvalidType"))
			return nTypeId;

		m_typeNameToIdMap[typeName] = nTypeId;
		STypeInfo typeInfo(typeName, factory);

		if (nTypeId >= m_typeRegistryVec.size())
			m_typeRegistryVec.push_back(typeInfo);
		else
			m_typeRegistryVec[nTypeId] = typeInfo;

		return nTypeId;
	}
	else
	{
		// overriding
		TFlowNodeTypeId nTypeId = iter->second;
		STypeInfo& typeInfo = m_typeRegistryVec[nTypeId];

		if (!typeInfo.factory->AllowOverride())
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "CFlowSystem::RegisterType: Type '%s' Id=%u already registered. Overriding not allowed by node factory.", type, nTypeId);
			return InvalidFlowNodeTypeId;
		}

		assert(nTypeId < m_typeRegistryVec.size());
		typeInfo.factory = factory;
		return nTypeId;
	}
}

bool CFlowSystem::UnregisterType(const char* typeName)
{
	const TTypeNameToIdMap::iterator iter = m_typeNameToIdMap.find(typeName);

	if (iter != m_typeNameToIdMap.end())
	{
		const TFlowNodeTypeId typeID = iter->second;
		m_freeNodeTypeIDs.push_back(typeID);
		m_typeRegistryVec[typeID] = STypeInfo();
		m_typeNameToIdMap.erase(iter);

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
	{
		// Registered entity types requires loading lua scripts, and so should be done after most game is initialized
		RegisterEntityTypes();
	}
	break;
	}
}

IFlowNodePtr CFlowSystem::CreateNodeOfType(IFlowNode::SActivationInfo* pActInfo, TFlowNodeTypeId typeId)
{
	assert(typeId < m_typeRegistryVec.size());
	if (typeId >= m_typeRegistryVec.size())
		return 0;

	const STypeInfo& type = m_typeRegistryVec[typeId];
	IFlowNodeFactory* pFactory = type.factory;
	if (pFactory)
		return pFactory->Create(pActInfo);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Update()
{
#ifdef SHOW_FG_CRITICAL_LOADING_ERROR_ON_SCREEN
	if (m_criticalLoadingErrorHappened && !gEnv->IsEditor() && gEnv->pRenderer)
	{
		IRenderAuxText::Draw2dLabel(10, 30, 2, Col_Red, false, "CRITICAL ERROR. SOME FLOWGRAPHS HAVE BEEN DISCARDED");
		IRenderAuxText::Draw2dLabel(10, 50, 2, Col_Red, false, "LEVEL LOGIC COULD BE DAMAGED. check log for more info.");
	}
#endif

	{
		CRY_PROFILE_REGION(PROFILE_ACTION, "CFlowSystem::Update()");
		if (m_cVars.m_enableUpdates == 0)
		{
			/*
			   IRenderer * pRend = gEnv->pRenderer;
			   float white[4] = {1,1,1,1};
			   pRend->Draw2dLabel( 10, 100, 2, white, false, "FlowGraphSystem Disabled");
			 */
			return;
		}

		if (m_bInspectingEnabled)
		{
			// call pre updates

			// 1. system inspectors
			std::for_each(m_systemInspectors.begin(), m_systemInspectors.end(), std::bind2nd(std::mem_fun(&IFlowGraphInspector::PreUpdate), (IFlowGraph*) 0));

			// 2. graph inspectors TODO: optimize not to go over all graphs ;-)
			for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
			{
				const std::vector<IFlowGraphInspectorPtr>& graphInspectors(itGraph->GetInspectors());
				std::for_each(graphInspectors.begin(), graphInspectors.end(), std::bind2nd(std::mem_fun(&IFlowGraphInspector::PreUpdate), *itGraph));
			}
		}

		UpdateGraphs();

		if (m_bInspectingEnabled)
		{
			// call post updates

			// 1. system inspectors
			std::for_each(m_systemInspectors.begin(), m_systemInspectors.end(), std::bind2nd(std::mem_fun(&IFlowGraphInspector::PostUpdate), (IFlowGraph*) 0));

			// 2. graph inspectors TODO: optimize not to go over all graphs ;-)
			for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
			{
				const std::vector<IFlowGraphInspectorPtr>& graphInspectors(itGraph->GetInspectors());
				std::for_each(graphInspectors.begin(), graphInspectors.end(), std::bind2nd(std::mem_fun(&IFlowGraphInspector::PostUpdate), *itGraph));
			}
		}
	}

	// end of flow system update: remove module instances which are no longer needed
	m_pModuleManager->RemoveCompletedModuleInstances();
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::UpdateGraphs()
{
	// Determine if graphs should be updated (Debug control)
	bool bUpdateGraphs = true;
	PREFAST_SUPPRESS_WARNING(6237);
	if (gEnv->IsEditing() && gEnv->pGameFramework->IsGameStarted())
	{
		if (m_cVars.m_enableFlowgraphNodeDebugging == 2)
		{
			if (m_cVars.m_debugNextStep == 0)
				bUpdateGraphs = false;
			else
				m_cVars.m_debugNextStep = 0;
		}
	}

	if (bUpdateGraphs)
	{
		if (m_needToUpdateForwardings)
		{
			for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
			{
				itGraph->UpdateForwardings();
			}
			m_needToUpdateForwardings = false;
		}

		for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
		{
			itGraph->Update();
		}
	}

#ifndef _RELEASE
	if (m_cVars.m_profile != 0)
	{
		float white[4] = { 1, 1, 1, 1 };
		IRenderAuxText::Draw2dLabel(10, 100, 2, white, false, "Number of Flow Graphs Updated: %d", FGProfile.graphsUpdated);
		IRenderAuxText::Draw2dLabel(10, 120, 2, white, false, "Number of Flow Graph Nodes Updated: %d", FGProfile.nodeUpdates);
		IRenderAuxText::Draw2dLabel(10, 140, 2, white, false, "Number of Flow Graph Nodes Activated: %d", FGProfile.nodeActivations);
	}
	FGProfile.Reset();
#endif //_RELEASE
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Reset(bool unload)
{
	m_needToUpdateForwardings = true; // does not hurt, and it prevents a potentially bad situation in save/load.
	if (!unload)
	{
		for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
		{
			itGraph->InitializeValues();
		}
	}
	else
	{
		assert(m_graphs.Empty());
		m_graphs.Clear(true);
		for (std::vector<STypeInfo>::iterator it = m_typeRegistryVec.begin(), itEnd = m_typeRegistryVec.end(); it != itEnd; ++it)
		{
			if (it->factory.get())
			{
				it->factory->Reset();
			}
		}
	}

	// Clean up the containers
	m_FlowSystemContainers.clear();
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Init()
{
	if (gEnv->pEntitySystem)
		gEnv->pEntitySystem->AddSink(this, IEntitySystem::OnReused | IEntitySystem::OnSpawn);
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Shutdown()
{
	if (gEnv->pEntitySystem)
	{
		gEnv->pEntitySystem->RemoveSink(this);
	}

	if (m_pModuleManager != nullptr)
	{
		m_pModuleManager->Shutdown();
		delete m_pModuleManager;
		m_pModuleManager = nullptr;
	}

	m_typeRegistryVec.clear();
	m_typeNameToIdMap.clear();
	m_systemInspectors.clear();
	m_freeNodeTypeIDs.clear();
	m_FlowSystemContainers.clear();
}

//////////////////////////////////////////////////////////////////////////
TFlowGraphId CFlowSystem::RegisterGraph(CFlowGraphBase* pGraph, const char* debugName)
{
	m_graphs.Add(pGraph, debugName);
	return m_nextFlowGraphId++;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::UnregisterGraph(CFlowGraphBase* pGraph)
{
	m_graphs.Remove(pGraph);
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::LoadBlacklistedFlownodeXML()
{
	if (!m_blacklistNode)
	{
		const string filename = BLACKLIST_FILE_PATH;
		if (gEnv->pCryPak->IsFileExist(BLACKLIST_FILE_PATH))
		{
			m_blacklistNode = gEnv->pSystem->LoadXmlFromFile(filename);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowSystem::BlacklistedFlownode(const char** nodeName)
{
	if (m_blacklistNode)
	{
		int childCount = m_blacklistNode->getChildCount();
		const char* attrKey;
		const char* attrValue;
		for (int n = 0; n < childCount; ++n)
		{
			XmlNodeRef child = m_blacklistNode->getChild(n);

			if (!child->getAttributeByIndex(0, &attrKey, &attrValue))
				continue;

			if (!stricmp(attrValue, *nodeName))
			{
				//replace name
				int numAttr = child->getNumAttributes();
				if (numAttr == 2 && child->getAttributeByIndex(1, &attrKey, &attrValue))
				{
					*nodeName = attrValue;
					break;
				}

				//remove class
				return false;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterAutoTypes()
{
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterEntityTypes()
{
	// Register all entity class from entities registry.
	// Each entity class is registered as a flow type entity:ClassName, ex: entity:ProximityTrigger
	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass* pEntityClass = 0;
	pClassRegistry->IteratorMoveFirst();
	string entityPrefixStr = ENTITY_NODE_PREFIX;
	while (pEntityClass = pClassRegistry->IteratorNext())
	{
		string classname = entityPrefixStr + pEntityClass->GetName();
		INDENT_LOG_DURING_SCOPE(true, "Flow system is registering entity type '%s'", classname.c_str());

		// if the entity lua script does not have input/outputs defined, and there is already an FG node defined for that entity in c++, do not register the empty lua one
		if (pEntityClass->GetEventCount() == 0 && GetTypeId(classname) != InvalidFlowNodeTypeId)
			continue;

		RegisterType(classname, new CFlowEntityClass(pEntityClass));
	}
}

//////////////////////////////////////////////////////////////////////////
const CFlowSystem::STypeInfo& CFlowSystem::GetTypeInfo(TFlowNodeTypeId typeId) const
{
	assert(typeId < m_typeRegistryVec.size());
	if (typeId < m_typeRegistryVec.size())
		return m_typeRegistryVec[typeId];
	return m_typeRegistryVec[InvalidFlowNodeTypeId];
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowSystem::GetTypeName(TFlowNodeTypeId typeId)
{
	assert(typeId < m_typeRegistryVec.size());
	if (typeId < m_typeRegistryVec.size())
		return m_typeRegistryVec[typeId].name.c_str();
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeTypeId CFlowSystem::GetTypeId(const char* typeName)
{
	return stl::find_in_map(m_typeNameToIdMap, CONST_TEMP_STRING(typeName), InvalidFlowNodeTypeId);
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph)
{
	if (pGraph == 0)
	{
		stl::push_back_unique(m_systemInspectors, pInspector);
	}
	else
	{
		CFlowGraphBase* pCGraph = (CFlowGraphBase*)pGraph.get();
		if (pCGraph && m_graphs.Contains(pCGraph))
			pCGraph->RegisterInspector(pInspector);
		else
			GameWarning("CFlowSystem::RegisterInspector: Unknown graph 0x%p", (IFlowGraph*)pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::UnregisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph)
{
	if (pGraph == 0)
	{
		stl::find_and_erase(m_systemInspectors, pInspector);
	}
	else
	{
		CFlowGraphBase* pCGraph = (CFlowGraphBase*)pGraph.get();
		if (pCGraph && m_graphs.Contains(pCGraph))
			pCGraph->UnregisterInspector(pInspector);
		else
			GameWarning("CFlowSystem::UnregisterInspector: Unknown graph 0x%p", (IFlowGraph*)pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::NotifyFlow(CFlowGraphBase* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	if (!m_bInspectingEnabled) return;

	if (!m_systemInspectors.empty())
	{
		std::vector<IFlowGraphInspectorPtr>::iterator iter(m_systemInspectors.begin());
		while (iter != m_systemInspectors.end())
		{
			(*iter)->NotifyFlow(pGraph, from, to);
			++iter;
		}
	}
	const std::vector<IFlowGraphInspectorPtr>& graphInspectors(pGraph->GetInspectors());
	if (!graphInspectors.empty())
	{
		std::vector<IFlowGraphInspectorPtr>::const_iterator iter(graphInspectors.begin());
		while (iter != graphInspectors.end())
		{
			(*iter)->NotifyFlow(pGraph, from, to);
			++iter;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::NotifyProcessEvent(CFlowGraphBase* pGraph, IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo, IFlowNode* pImpl)
{
	if (!m_bInspectingEnabled) return;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::OnEntityClassRegistryEvent(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass)
{
	string entityPrefixStr = ENTITY_NODE_PREFIX;
	string className = entityPrefixStr + pEntityClass->GetName();

	switch (event)
	{
	case ECRE_CLASS_MODIFIED:
	case ECRE_CLASS_REGISTERED:
		{
			INDENT_LOG_DURING_SCOPE(true, "Flow system is registering entity type '%s'", className.c_str());
			IEntityClass* pClass = const_cast<IEntityClass*>(pEntityClass);
			
			// if the entity lua script does not have input/outputs defined, and there is already an FG node defined for that entity in c++, do not register the empty lua one
			if (pClass->GetEventCount() == 0 && GetTypeId(className) != InvalidFlowNodeTypeId)
				return;

			RegisterType(className, new CFlowEntityClass(pClass));
			break;
		}

	case ECRE_CLASS_UNREGISTERED:
		{
		INDENT_LOG_DURING_SCOPE(true, "Flow system is unregistering entity type '%s'", classname.c_str());
		
		UnregisterType(className);
		break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFlowGraph* CFlowSystem::GetGraphById(TFlowGraphId graphId)
{
	for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
	{
		CFlowGraphBase* pFlowGraph = *itGraph;

		if (pFlowGraph->GetGraphId() == graphId)
		{
			return pFlowGraph;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "FlowSystem");
	pSizer->AddObject(this, sizeof(*this));

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Factories");
		{
			SIZER_SUBCOMPONENT_NAME(pSizer, "FactoriesLookup");
			pSizer->AddObject(m_typeNameToIdMap);
			pSizer->AddObject(m_typeRegistryVec);
		}
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Inspectors");
		pSizer->AddObject(m_systemInspectors);
	}

	if (!m_graphs.Empty())
	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Graphs");
		pSizer->AddObject(&m_graphs, m_graphs.MemSize());
	}

	if (!m_FlowSystemContainers.empty())
	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Containers");
		for (TFlowSystemContainerMap::const_iterator it = m_FlowSystemContainers.begin(); it != m_FlowSystemContainers.end(); ++it)
		{
			(*it).second->GetMemoryUsage(pSizer);
		}

		pSizer->AddObject(&(*m_FlowSystemContainers.begin()), m_FlowSystemContainers.size() * (sizeof(CFlowSystemContainer) + sizeof(TFlowSystemContainerId)));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::OnReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
	{
		itGraph->OnEntityReused(pEntity, params);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::OnEntityIdChanged(EntityId oldId, EntityId newId)
{
	for (TGraphs::Notifier itGraph(m_graphs); itGraph.IsValid(); itGraph.Next())
	{
		itGraph->OnEntityIdChanged(oldId, newId);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowSystem::CreateContainer(TFlowSystemContainerId id)
{
	if (m_FlowSystemContainers.find(id) != m_FlowSystemContainers.end())
	{
		return false;
	}

	IFlowSystemContainerPtr container(new CFlowSystemContainer);
	m_FlowSystemContainers.insert(std::make_pair(id, container));
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::DeleteContainer(TFlowSystemContainerId id)
{
	std::map<TFlowSystemContainerId, IFlowSystemContainerPtr>::iterator it = m_FlowSystemContainers.find(id);
	if (it != m_FlowSystemContainers.end())
	{
		m_FlowSystemContainers.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////
IFlowSystemContainerPtr CFlowSystem::GetContainer(TFlowSystemContainerId id)
{
	return m_FlowSystemContainers[id];
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	// this should be a more generic test: maybe an entity flag "CAN_FORWARD_FLOWGRAPHS", or something similar
	if ((pEntity->GetFlags() & ENTITY_FLAG_HAS_AI) != 0)
	{
		m_needToUpdateForwardings = true;
	}
}

//////////////////////////////////////////////////////////////////////////
IFlowGraphModuleManager* CFlowSystem::GetIModuleManager()
{
	return m_pModuleManager;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Serialize(TSerialize ser)
{
	int count = m_FlowSystemContainers.size();
	ser.Value("ContainerCount", count);

	if (ser.IsWriting())
	{
		TFlowSystemContainerMap::iterator it = m_FlowSystemContainers.begin();
		while (it != m_FlowSystemContainers.end())
		{
			ser.BeginGroup("Containers");
			TFlowSystemContainerId id = (*it).first;
			ser.Value("key", id);
			(*it).second->Serialize(ser);
			ser.EndGroup();
			++it;
		}
	}
	else
	{
		int id;
		for (int i = 0; i < count; i++)
		{
			ser.BeginGroup("Containers");
			ser.Value("key", id);
			if (CreateContainer(id))
			{
				IFlowSystemContainerPtr container = GetContainer(id);
				if (container)
				{
					container->Serialize(ser);
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_ERROR, "CFlowSystem::Serialize: Could not create or get container with ID from save file - container not restored");
				}
			}
			ser.EndGroup();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IGameTokenSystem* CFlowSystem::GetIGameTokenSystem()
{
	return m_pGameTokenSystem;
}
