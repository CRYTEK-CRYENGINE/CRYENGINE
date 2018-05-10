// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "BehaviorTreeManager.h"
#include <CryAISystem/BehaviorTree/XmlLoader.h>
#include <CryAISystem/BehaviorTree/NodeFactory.h>
#include "BehaviorTree/BehaviorTreeMetaExtensions.h"

#ifdef USING_BEHAVIOR_TREE_VISUALIZER
	#include "BehaviorTree/TreeVisualizer.h"
#endif // USING_BEHAVIOR_TREE_VISUALIZER

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
	#include "ExecutionStackFileLogger.h"
#endif

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	#include <CryAISystem/IAIDebugRenderer.h>
#endif

#include "PipeUser.h"

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	#include <CrySerialization/ClassFactory.h>
#endif

#if defined (DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	#include <CryGame/IGameFramework.h>
	#include <CryGame/IGame.h>
	#include <CrySerialization/IArchiveHost.h>
#endif

namespace
{
static CAIActor* GetAIActor(EntityId entityId)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
	IAIObject* aiObject = entity ? entity->GetAI() : NULL;
	return aiObject ? aiObject->CastToCAIActor() : NULL;
}

static CPipeUser* GetPipeUser(EntityId entityId)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
	IAIObject* aiObject = entity ? entity->GetAI() : NULL;
	return aiObject ? aiObject->CastToCPipeUser() : NULL;
}

#if defined (DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
IGameWebDebugService* GetIGameWebDebugService()
{
	if (gEnv && gEnv->pGameFramework != nullptr)
	{
		if (IGame* pGame = gEnv->pGameFramework->GetIGame())
		{
			return pGame->GetIWebDebugService();
		}
	}

	return nullptr;
}
#endif
}

namespace BehaviorTree
{
#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
NodeFactory::BehaviorTreeBucketAllocator NodeFactory::s_bucketAllocator(NULL, false);
#else
NodeFactory::BehaviorTreeBucketAllocator NodeFactory::s_bucketAllocator;
#endif

BehaviorTreeManager::BehaviorTreeManager()
#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	: m_bRegisteredAsDebugChannel(false)
#endif
{
	m_metaExtensionFactory.reset(new BehaviorTree::MetaExtensionFactory);
	m_nodeFactory.reset(new NodeFactory);
}

BehaviorTreeManager::~BehaviorTreeManager()
{
#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	if (m_bRegisteredAsDebugChannel)
	{
		IGameWebDebugService* pWebDebugService = GetIGameWebDebugService();
		if (pWebDebugService)
		{
			pWebDebugService->RemoveChannel(this);
			m_bRegisteredAsDebugChannel = false;
		}
	}
#endif
}

IMetaExtensionFactory& BehaviorTreeManager::GetMetaExtensionFactory()
{
	assert(m_metaExtensionFactory.get());
	return *m_metaExtensionFactory.get();
}

INodeFactory& BehaviorTreeManager::GetNodeFactory()
{
	assert(m_nodeFactory.get());
	return *m_nodeFactory.get();
}

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
SERIALIZATION_CLASS_NULL(INode, "[ Node ]");

Serialization::ClassFactory<INode>& BehaviorTreeManager::GetNodeSerializationFactory()
{
	return Serialization::ClassFactory<INode>::the();
}
#endif

BehaviorTreeInstancePtr BehaviorTreeManager::CreateBehaviorTreeInstanceFromDiskCache(const char* behaviorTreeName)
{
	BehaviorTreeInstancePtr instance = LoadFromCache(behaviorTreeName);

	if (instance.get())
		return instance;

	// In the editor the behavior trees are not cached up front so we
	// need to load the tree into the cache if it wasn't already there.
	LoadFromDiskIntoCache(behaviorTreeName);
	return LoadFromCache(behaviorTreeName);
}

BehaviorTreeInstancePtr BehaviorTreeManager::CreateBehaviorTreeInstanceFromXml(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode)
{
	BehaviorTreeTemplatePtr behaviorTreeTemplate = BehaviorTreeTemplatePtr(new BehaviorTreeTemplate());
	if (LoadBehaviorTreeTemplate(behaviorTreeName, behaviorTreeXmlNode, *(behaviorTreeTemplate.get())))
	{
		return BehaviorTreeInstancePtr(new BehaviorTreeInstance(
		                                 behaviorTreeTemplate->defaultTimestampCollection,
		                                 behaviorTreeTemplate->variableDeclarations.GetDefaults(),
		                                 behaviorTreeTemplate,
		                                 GetNodeFactory()
		                                 ));
	}

	return BehaviorTreeInstancePtr();
}

BehaviorTreeInstancePtr BehaviorTreeManager::LoadFromCache(const char* behaviorTreeName)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Load Modular Behavior Tree From Cache: %s", behaviorTreeName);

	BehaviorTreeCache::iterator findResult = m_behaviorTreeCache.find(behaviorTreeName);
	if (findResult != m_behaviorTreeCache.end())
	{
		BehaviorTreeTemplatePtr behaviorTreeTemplate = findResult->second;
		return BehaviorTreeInstancePtr(new BehaviorTreeInstance(
		                                 behaviorTreeTemplate->defaultTimestampCollection,
		                                 behaviorTreeTemplate->variableDeclarations.GetDefaults(),
		                                 behaviorTreeTemplate,
		                                 GetNodeFactory()));
	}

	return BehaviorTreeInstancePtr();
}

bool BehaviorTreeManager::LoadBehaviorTreeTemplate(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode, BehaviorTreeTemplate& behaviorTreeTemplate)
{
#if defined(DEBUG_MODULAR_BEHAVIOR_TREE)
	behaviorTreeTemplate.mbtFilename = behaviorTreeName;
#endif

	GetMetaExtensionFactory().CreateMetaExtensions(behaviorTreeTemplate.metaExtensionTable);

	if (XmlNodeRef metaExtensionsXml = behaviorTreeXmlNode->findChild("MetaExtensions"))
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "MetaExtensions");
		behaviorTreeTemplate.metaExtensionTable.LoadFromXml(metaExtensionsXml);
	}

	if (XmlNodeRef timestampsXml = behaviorTreeXmlNode->findChild("Timestamps"))
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Timestamps");
		behaviorTreeTemplate.defaultTimestampCollection.LoadFromXml(timestampsXml);
	}

	if (XmlNodeRef variablesXml = behaviorTreeXmlNode->findChild("Variables"))
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Variables");
		behaviorTreeTemplate.variableDeclarations.LoadFromXML(variablesXml, behaviorTreeName);
	}

	if (XmlNodeRef signalsXml = behaviorTreeXmlNode->findChild("SignalVariables"))
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Signals");
		behaviorTreeTemplate.signalHandler.LoadFromXML(behaviorTreeTemplate.variableDeclarations, signalsXml, behaviorTreeName);
	}

	LoadContext context(GetNodeFactory(), behaviorTreeName, behaviorTreeTemplate.variableDeclarations);
	behaviorTreeTemplate.rootNode = XmlLoader().CreateBehaviorTreeRootNodeFromBehaviorTreeXml(behaviorTreeXmlNode, context);

	if (!behaviorTreeTemplate.rootNode)
		return false;

	return true;
}

void BehaviorTreeManager::LoadFromDiskIntoCache(const char* behaviorTreeName)
{
	BehaviorTreeCache::iterator findResult = m_behaviorTreeCache.find(behaviorTreeName);
	if (findResult != m_behaviorTreeCache.end())
		return;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Load Modular Behavior Tree From Disk: %s", behaviorTreeName);

	XmlNodeRef behaviorTreeXmlNode = XmlLoader().LoadBehaviorTreeXmlFile("Scripts/AI/BehaviorTrees/", behaviorTreeName);

	if (!behaviorTreeXmlNode)
		behaviorTreeXmlNode = XmlLoader().LoadBehaviorTreeXmlFile("libs/ai/behavior_trees/", behaviorTreeName);

	if (behaviorTreeXmlNode)
	{
		BehaviorTreeTemplatePtr behaviorTreeTemplate = BehaviorTreeTemplatePtr(new BehaviorTreeTemplate());
		if (LoadBehaviorTreeTemplate(behaviorTreeName, behaviorTreeXmlNode, *(behaviorTreeTemplate.get())))
		{
			m_behaviorTreeCache.insert(BehaviorTreeCache::value_type(behaviorTreeName, behaviorTreeTemplate));
		}
		else
		{
			gEnv->pLog->LogError("Modular behavior tree '%s' failed to load.", behaviorTreeName);
		}
	}
	else
	{
		gEnv->pLog->LogError("Failed to load behavior tree '%s'. Could not load the '%s.xml' file from the 'Scripts/AI/BehaviorTrees/' or 'libs/ai/behavior_trees/' folders", behaviorTreeName, behaviorTreeName);
	}
}

void BehaviorTreeManager::Reset()
{
	StopAllBehaviorTreeInstances();
	m_behaviorTreeCache.clear();
	m_nodeFactory->TrimNodeCreators();
	m_nodeFactory->CleanUpBucketAllocator();
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	m_errorStatusTrees.clear();
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	if (!m_bRegisteredAsDebugChannel)
	{
		IGameWebDebugService* pWebDebugService = GetIGameWebDebugService();
		if (pWebDebugService)
		{
			pWebDebugService->AddChannel(this);
			m_bRegisteredAsDebugChannel = true;
		}
	}
#endif
}

void BehaviorTreeManager::StopAllBehaviorTreeInstances()
{
	Instances::iterator it = m_instances.begin();
	Instances::iterator end = m_instances.end();
	for (; it != end; ++it)
	{
		BehaviorTreeInstance& instance = *(it->second.get());
		const EntityId entityId = it->first;

		BehaviorVariablesContext variables(
		  instance.variables
		  , instance.behaviorTreeTemplate->variableDeclarations
		  , instance.variables.Changed()
		  );

		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		assert(entity);

		UpdateContext context(
		  entityId
		  , *entity
		  , variables
		  , instance.timestampCollection
		  , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
		  , instance.behaviorLog
#endif
		  );

		instance.behaviorTreeTemplate->rootNode->Terminate(context);
	}

	m_instances.clear();
}

#ifdef CRYAISYSTEM_DEBUG
void BehaviorTreeManager::DrawMemoryInformation()
{
	const float fontSize = 1.25f;
	const float lineHeight = 11.5f * fontSize;
	const float x = 10.0f;
	float y = 30.0f;

	IAIDebugRenderer* renderer = gEnv->pAISystem->GetAIDebugRenderer();

	const size_t immutableSize = m_nodeFactory->GetSizeOfImmutableDataForAllAllocatedNodes();
	const size_t runtimeSize = m_nodeFactory->GetSizeOfRuntimeDataForAllAllocatedNodes();

	renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "MODULAR BEHAVIOR TREE STATISTICS");
	y += lineHeight;
	renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "Immutable: %" PRISIZE_T " bytes", immutableSize);
	y += lineHeight;
	renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "Runtime: %" PRISIZE_T " bytes", runtimeSize);
	y += lineHeight;
}
#endif

bool BehaviorTreeManager::StartModularBehaviorTree(const EntityId entityId, const char* treeName)
{
	BehaviorTreeInstancePtr instance = CreateBehaviorTreeInstanceFromDiskCache(treeName);
	return StartBehaviorInstance(entityId, instance, treeName);
}

bool BehaviorTreeManager::StartModularBehaviorTreeFromXml(const EntityId entityId, const char* treeName, XmlNodeRef treeXml)
{
	BehaviorTreeInstancePtr instance = CreateBehaviorTreeInstanceFromXml(treeName, treeXml);
	return StartBehaviorInstance(entityId, instance, treeName);
}

bool BehaviorTreeManager::StartBehaviorInstance(const EntityId entityId, BehaviorTreeInstancePtr instance, const char* treeName)
{
	StopModularBehaviorTree(entityId);
	if (instance.get())
	{
		m_instances.insert(std::make_pair(entityId, instance));
#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
		m_executionStackFileLoggerInstances.insert(std::make_pair(entityId, ExecutionStackFileLoggerPtr(new ExecutionStackFileLogger(entityId))));
#endif
		return true;
	}
	else
	{
		gEnv->pLog->LogError("Failed to start modular behavior tree '%s'.", treeName);
		return false;
	}
}

void BehaviorTreeManager::StopModularBehaviorTree(const EntityId entityId)
{
	Instances::iterator it = m_instances.find(entityId);
	if (it != m_instances.end())
	{
		BehaviorTreeInstance& instance = *(it->second.get());

		BehaviorVariablesContext variables(
		  instance.variables
		  , instance.behaviorTreeTemplate->variableDeclarations
		  , instance.variables.Changed()
		  );

		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		assert(entity);

		UpdateContext context(
		  entityId
		  , *entity
		  , variables
		  , instance.timestampCollection
		  , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
		  , instance.behaviorLog
#endif
		  );

		instance.behaviorTreeTemplate->rootNode->Terminate(context);
		m_instances.erase(it);
	}

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
	m_executionStackFileLoggerInstances.erase(entityId);
#endif
}

void BehaviorTreeManager::Update()
{
	EntityIdVector newErrorStatusTree;

	Instances::iterator it = m_instances.begin();
	Instances::iterator end = m_instances.end();

	for (; it != end; ++it)
	{
		const EntityId entityId = it->first;

		IEntity* agentEntity = gEnv->pEntitySystem->GetEntity(entityId);
		assert(agentEntity);
		if (!agentEntity)
			continue;

		CPipeUser* pipeUser = GetPipeUser(entityId);
		if (pipeUser && (!pipeUser->IsEnabled() || pipeUser->IsPaused()))
			continue;

		BehaviorTreeInstance& instance = *(it->second.get());

		BehaviorVariablesContext variables(instance.variables, instance.behaviorTreeTemplate->variableDeclarations, instance.variables.Changed());
		instance.variables.ResetChanged();

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		const bool debugThisAgent = (GetAISystem()->GetAgentDebugTarget() == entityId);
		DebugTree debugTree;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE_WEB
		TGameWebDebugClientId webDebugClientId;
		const bool webDebugThisAgent = DoesEntityWantToDoWebDebugging(entityId, &webDebugClientId);
#endif // DEBUG_MODULAR_BEHAVIOR_TREE_WEB

		UpdateContext updateContext(
		  entityId
		  , *agentEntity
		  , variables
		  , instance.timestampCollection
		  , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
		  , instance.behaviorLog
#endif // USING_BEHAVIOR_TREE_LOG
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
#	ifdef DEBUG_MODULAR_BEHAVIOR_TREE_WEB
		  , (debugThisAgent || webDebugThisAgent) ? &debugTree : nullptr
#	else
		  , debugThisAgent ? &debugTree : nullptr
#	endif // DEBUG_MODULAR_BEHAVIOR_TREE_WEB
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
		  );

		const Status behaviorStatus = instance.behaviorTreeTemplate->rootNode->Tick(updateContext);
		const bool bExecutionError = (behaviorStatus == Success) || (behaviorStatus == Failure);

		IF_UNLIKELY (bExecutionError)
		{
			string message;
			message.Format("Modular Behavior Tree Error Status: The root node for entity '%s' %s. Having the root succeed or fail is considered undefined behavior and the tree should be designed in a way that the root node is always running.", agentEntity ? agentEntity->GetName() : "", (behaviorStatus == Success ? "SUCCEEDED" : "FAILED"));

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
			DynArray<DebugNodePtr>::const_iterator it = debugTree.GetSucceededAndFailedNodes().begin();
			DynArray<DebugNodePtr>::const_iterator end = debugTree.GetSucceededAndFailedNodes().end();
			for (; it != end; ++it)
			{
				const BehaviorTree::Node* node = static_cast<const BehaviorTree::Node*>((*it)->node);
				message.append(stack_string().Format(" (%d) %s.", node->GetXmlLine(), node->GetCreator()->GetTypeName()).c_str());
			}
#endif  // DEBUG_MODULAR_BEHAVIOR_TREE

			gEnv->pLog->LogError("%s", message.c_str());

			newErrorStatusTree.push_back(entityId);

			continue;
		}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		if (debugThisAgent)
		{
			UpdateDebugVisualization(updateContext, entityId, debugTree, instance, agentEntity);
		}

		if (gAIEnv.CVars.LogModularBehaviorTreeExecutionStacks == 1 && debugThisAgent || gAIEnv.CVars.LogModularBehaviorTreeExecutionStacks == 2)
		{
			UpdateExecutionStackLogging(updateContext, entityId, debugTree, instance);
		}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE_WEB
		if (webDebugThisAgent)
		{
			UpdateWebDebugChannel(webDebugClientId, updateContext, debugTree, instance, bExecutionError);
		}
#endif
	}

	for (EntityIdVector::iterator it = newErrorStatusTree.begin(), end = newErrorStatusTree.end(); it != end; ++it)
	{
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		m_errorStatusTrees.push_back(*it);
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		StopModularBehaviorTree(*it);
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	if (gAIEnv.CVars.DebugDraw > 0)
	{
		for (EntityIdVector::iterator it = m_errorStatusTrees.begin(), end = m_errorStatusTrees.end(); it != end; ++it)
		{
			if (IEntity* entity = gEnv->pEntitySystem->GetEntity(*it))
			{
				const float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
				const Vec3 position = entity->GetPos() + Vec3(0.0f, 0.0f, 2.0f);
				IRenderAuxText::DrawLabelEx(position, 1.5f, color, true, true, "Behavior tree error.");
			}
		}
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

}

size_t BehaviorTreeManager::GetTreeInstanceCount() const
{
	return m_instances.size();
}

EntityId BehaviorTreeManager::GetTreeInstanceEntityIdByIndex(const size_t index) const
{
	assert(index < m_instances.size());
	Instances::const_iterator it = m_instances.begin();
	std::advance(it, index);
	return it->first;
}

BehaviorTreeInstance* BehaviorTreeManager::GetBehaviorTree(const EntityId entityId) const
{
	Instances::const_iterator it = m_instances.find(entityId);
	if (it != m_instances.end())
		return it->second.get();
	return nullptr;
}

BehaviorTree::Blackboard* BehaviorTreeManager::GetBehaviorTreeBlackboard(const EntityId entityId)
{
	BehaviorTreeInstance* pBTInstance = GetBehaviorTree(entityId);
	return pBTInstance ? &pBTInstance->blackboard : nullptr;
}

Variables::Collection* BehaviorTreeManager::GetBehaviorVariableCollection_Deprecated(const EntityId entityId) const
{
	if (BehaviorTreeInstance* instance = GetBehaviorTree(entityId))
		return &instance->variables;

	return NULL;
}

const Variables::Declarations* BehaviorTreeManager::GetBehaviorVariableDeclarations_Deprecated(const EntityId entityId) const
{
	if (BehaviorTreeInstance* instance = GetBehaviorTree(entityId))
		return &instance->behaviorTreeTemplate->variableDeclarations;

	return NULL;
}

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
void BehaviorTreeManager::Subscribe(const TGameWebDebugClientId clientId, const EntityId entityId)
{
	m_webSubscribers[clientId] = entityId;
}

void BehaviorTreeManager::Unsubscribe(const TGameWebDebugClientId clientId)
{
	m_webSubscribers.erase(clientId);
}
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
void BehaviorTreeManager::UpdateDebugVisualization(UpdateContext updateContext, const EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance, IEntity* agentEntity)
{
	#ifdef USING_BEHAVIOR_TREE_VISUALIZER
	BehaviorTree::TreeVisualizer treeVisualizer(updateContext);
	treeVisualizer.Draw(
	  debugTree
	  , instance.behaviorTreeTemplate->mbtFilename
	  , agentEntity->GetName()
		#ifdef USING_BEHAVIOR_TREE_LOG
	  , instance.behaviorLog
		#endif // USING_BEHAVIOR_TREE_LOG
	  , instance.timestampCollection
	  , instance.blackboard
		#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	  , instance.eventsLog
		#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	  );
	#endif // USING_BEHAVIOR_TREE_VISUALIZER

	#ifdef DEBUG_VARIABLE_COLLECTION
	Variables::DebugHelper::DebugDraw(true, instance.variables, instance.behaviorTreeTemplate->variableDeclarations);
	#endif // DEBUG_VARIABLE_COLLECTION

}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
void BehaviorTreeManager::UpdateExecutionStackLogging(UpdateContext updateContext, const EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance)
{
	ExecutionStackFileLoggerInstances::const_iterator itTreeHistory = m_executionStackFileLoggerInstances.find(entityId);
	if (itTreeHistory != m_executionStackFileLoggerInstances.end())
	{
		ExecutionStackFileLogger* logger = itTreeHistory->second.get();
		logger->LogDebugTree(debugTree, updateContext, instance);
	}
}
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE_WEB
bool BehaviorTreeManager::DoesEntityWantToDoWebDebugging(const EntityId entityIdToCheckForWebDebugging, TGameWebDebugClientId* pOutClientId) const
{
	if (m_bRegisteredAsDebugChannel)
	{
		for (WebSubscribers::const_iterator it = m_webSubscribers.begin(); it != m_webSubscribers.end(); ++it)
		{
			if (it->second == entityIdToCheckForWebDebugging)
			{
				if (pOutClientId)
				{
					*pOutClientId = it->first;
				}
				return true;
			}
		}
	}

	if (pOutClientId)
	{
		*pOutClientId = GAME_WEBDEBUG_INVALID_CLIENT_ID;
	}
	return false;
}

void BehaviorTreeManager::UpdateWebDebugChannel(const TGameWebDebugClientId clientId, UpdateContext& updateContext, DebugTree& debugTree, BehaviorTreeInstance& instance, const bool bExecutionError)
{
	IGameWebDebugService* pWebDebugService = GetIGameWebDebugService();
	if (pWebDebugService)
	{
		DebugTreeSerializer treeSerializer(instance, debugTree, updateContext, bExecutionError);
		DynArray<char> messageBuffer;
		Serialization::SaveJsonBuffer(messageBuffer, treeSerializer);

		IGameWebDebugService::SMessage message;
		message.clientId = clientId;
		message.pData = messageBuffer.data();
		message.dataSize = messageBuffer.size();

		pWebDebugService->Publish(message);
	}
}
#endif

void BehaviorTreeManager::HandleEvent(const EntityId entityId, Event& event)
{
	assert(entityId);

	if (BehaviorTreeInstance* behaviorTreeInstance = GetBehaviorTree(entityId))
	{
		behaviorTreeInstance->behaviorTreeTemplate->signalHandler.ProcessSignal(event.GetCRC(), behaviorTreeInstance->variables);
		behaviorTreeInstance->timestampCollection.HandleEvent(event.GetCRC());
		BehaviorTree::EventContext context(entityId);
		behaviorTreeInstance->behaviorTreeTemplate->rootNode->SendEvent(context, event);
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
		behaviorTreeInstance->eventsLog.AddMessage(event.GetName());
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	}
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	else
	{
		const char* entityName = "";
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId))
			entityName = entity->GetName();

		stack_string errorText;
		errorText.Format("BehaviorTreeManager: The event '%s' was not handled because the entity '%s' is not running a behavior tree.", event.GetName(), entityName);
		gEnv->pLog->LogError(errorText.c_str());
	}
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
}
}
