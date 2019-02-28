// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/BehaviorTree/NodeFactory.h>
#include <CryGame/IGameWebDebug.h>

namespace BehaviorTree
{
class MetaExtensionFactory;
#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
class ExecutionStackFileLogger;
#endif

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE)
	#define DEBUG_MODULAR_BEHAVIOR_TREE_WEB
#endif

class BehaviorTreeManager
	: public IBehaviorTreeManager
#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	  , public IGameWebDebugEntityChannel
#endif
{
public:
	BehaviorTreeManager();
	virtual ~BehaviorTreeManager();

	// IBehaviorTreeManager
	virtual void                                Update() override;
	virtual IMetaExtensionFactory&              GetMetaExtensionFactory() override;
	virtual INodeFactory&                       GetNodeFactory() override;
#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual Serialization::ClassFactory<INode>& GetNodeSerializationFactory() override;
#endif
	virtual void                                LoadFromDiskIntoCache(const char* behaviorTreeName) override;
	virtual bool                                StartModularBehaviorTree(const EntityId entityId, const char* treeName) override;
	virtual bool                                StartModularBehaviorTreeFromXml(const EntityId entityId, const char* treeName, XmlNodeRef treeXml) override;
	virtual void                                StopModularBehaviorTree(const EntityId entityId) override;

	virtual void                                HandleEvent(const EntityId entityId, Event& event) override;

	// Returns blackboard corresponding to behavior tree assigned to Entity with specified entityID.
	virtual BehaviorTree::Blackboard* GetBehaviorTreeBlackboard(const EntityId entityId) override;

	virtual Variables::Collection*              GetBehaviorVariableCollection_Deprecated(const EntityId entityId) const override;
	virtual const Variables::Declarations*      GetBehaviorVariableDeclarations_Deprecated(const EntityId entityId) const override;
	// ~IBehaviorTreeManager

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	// IGameWebDebugEntityChannel
	virtual void Subscribe(const TGameWebDebugClientId clientId, const EntityId entityId) override;
	virtual void Unsubscribe(const TGameWebDebugClientId clientId) override;
	//~IGameWebDebugEntityChannel
#endif

	void                    Reset();

	size_t                  GetTreeInstanceCount() const;
	EntityId                GetTreeInstanceEntityIdByIndex(const size_t index) const;

	BehaviorTreeInstancePtr CreateBehaviorTreeInstanceFromXml(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode);

#ifdef CRYAISYSTEM_DEBUG
	void DrawMemoryInformation();
#endif

private:
	BehaviorTreeInstancePtr CreateBehaviorTreeInstanceFromDiskCache(const char* behaviorTreeName);
	BehaviorTreeInstancePtr LoadFromCache(const char* behaviorTreeName);
	bool                    StartBehaviorInstance(const EntityId entityId, BehaviorTreeInstancePtr instance, const char* treeName);
	void                    StopAllBehaviorTreeInstances();
	BehaviorTreeInstance*   GetBehaviorTree(const EntityId entityId) const;

	bool                    LoadBehaviorTreeTemplate(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode, BehaviorTreeTemplate& behaviorTreeTemplate);

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE)
	void UpdateDebugVisualization(UpdateContext updateContext, const EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance, IEntity* agentEntity);
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
	void UpdateExecutionStackLogging(UpdateContext updateContext, const EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance);
#endif

	std::unique_ptr<NodeFactory>          m_nodeFactory;
	std::unique_ptr<MetaExtensionFactory> m_metaExtensionFactory;

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE_WEB)
	bool DoesEntityWantToDoWebDebugging(const EntityId entityIdToCheckForWebDebugging, TGameWebDebugClientId* pOutClientId) const;
	void UpdateWebDebugChannel(const TGameWebDebugClientId clientId, UpdateContext& updateContext, DebugTree& debugTree, BehaviorTreeInstance& instance, const bool bExecutionError);
#endif

	typedef string                                                                                                              BehaviorTreeName;
	typedef std::unordered_map<BehaviorTreeName, BehaviorTreeTemplatePtr, stl::hash_stricmp<string>, stl::hash_stricmp<string>> BehaviorTreeCache;
	BehaviorTreeCache m_behaviorTreeCache;

	typedef VectorMap<EntityId, BehaviorTreeInstancePtr> Instances;
	Instances m_instances;

	typedef std::vector<EntityId> EntityIdVector;
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	EntityIdVector m_errorStatusTrees;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE_WEB
	typedef VectorMap<TGameWebDebugClientId, EntityId> WebSubscribers;
	WebSubscribers m_webSubscribers;
	bool           m_bRegisteredAsDebugChannel;
#endif

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
	typedef std::shared_ptr<ExecutionStackFileLogger>        ExecutionStackFileLoggerPtr;
	typedef VectorMap<EntityId, ExecutionStackFileLoggerPtr> ExecutionStackFileLoggerInstances;
	ExecutionStackFileLoggerInstances m_executionStackFileLoggerInstances;
#endif
};
}
