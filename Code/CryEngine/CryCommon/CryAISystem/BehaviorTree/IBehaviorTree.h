// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define DEBUG_MODULAR_BEHAVIOR_TREE
	#define USING_BEHAVIOR_TREE_EDITOR
#endif

#ifdef USING_BEHAVIOR_TREE_EDITOR
	#define USING_BEHAVIOR_TREE_SERIALIZATION
	#define USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	#define USING_BEHAVIOR_TREE_COMMENTS
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	#define USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	#define USING_BEHAVIOR_TREE_LOG
	#define USING_BEHAVIOR_TREE_VISUALIZER
	#define USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	#define USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING
	#define USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
//#  define USING_BEHAVIOR_TREE_DEBUG_MEMORY_USAGE
#endif

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE) || defined(USING_BEHAVIOR_TREE_EDITOR)
	#define STORE_BLACKBOARD_VARIABLE_NAMES
#endif

#include <CryCore/Containers/VectorMap.h>
#include <CryCore/Containers/VariableCollection.h>
#include <CryAISystem/BehaviorTree/TimestampCollection.h>
#include <CrySerialization/IArchive.h>

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION

	#include <CrySerialization/IClassFactory.h>
	#include <CrySerialization/Forward.h>

namespace BehaviorTree
{
struct INode;
typedef Serialization::ClassFactory<INode> NodeSerializationClassFactory;
}

#endif

struct IAISignalExtraData;

namespace BehaviorTree
{
struct INodeFactory;
class EventDispatcher;

enum Status
{
	Invalid,
	Success,
	Failure,
	Running
};

enum LoadResult
{
	LoadFailure,
	LoadSuccess,
};

#if defined(USING_BEHAVIOR_TREE_EVENT_DEBUGGING) || defined(USING_BEHAVIOR_TREE_SERIALIZATION)
	#define STORE_EVENT_NAME
#endif

class Event
{
public:
	Event()
		: m_nameCRC32(0)
		, m_pData(0)
#ifdef STORE_EVENT_NAME
		, m_name()
#endif
	{
	}

	Event(uint32 nameCRC32, const void* pData = 0)
		: m_nameCRC32(nameCRC32)
		, m_pData(pData)
#ifdef STORE_EVENT_NAME
		, m_name()
#endif
	{
	}

#ifdef STORE_EVENT_NAME
	Event(uint32 nameCRC32, const char* name, const void* pData = 0)
		: m_nameCRC32(nameCRC32)
		, m_name(name)
		, m_pData(pData)
	{
	}
#endif

	Event(const char* name, const void* pData = 0)
		: m_pData(pData)
#ifdef STORE_EVENT_NAME
		, m_name(name)
#endif
	{
		m_nameCRC32 = CCrc32::Compute(name);
	}

	bool operator==(const Event& rhs) const
	{
		return m_nameCRC32 == rhs.m_nameCRC32;
	}

	bool operator<(const Event& rhs) const
	{
		return m_nameCRC32 < rhs.m_nameCRC32;
	}

#ifdef STORE_EVENT_NAME
	const char* GetName() const
	{
		return m_name.c_str();
	}
#endif

	uint32 GetCRC() const
	{
		return m_nameCRC32;
	}

	const void* GetData() const
	{
		return m_pData;
	}

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive)
	{
		archive(m_name, "name", "^Name");
	}
#endif

private:
	uint32      m_nameCRC32;
	const void* m_pData;

#ifdef STORE_EVENT_NAME
	string m_name;
#endif
};

struct LoadContext
{
	LoadContext(
	  INodeFactory& _nodeFactory,
	  const char* _treeName,
	  const Variables::Declarations& _variableDeclarations
	  )
		: nodeFactory(_nodeFactory)
		, treeName(_treeName)
		, variableDeclarations(_variableDeclarations)
	{
	}

	INodeFactory&                  nodeFactory;
	const char*                    treeName;
	const Variables::Declarations& variableDeclarations;

	// Warning! If you're thinking about adding the entity id to the
	// LoadContext you need to keep one thing in mind:
	// The modular behavior tree will be loaded up and constructed before
	// the connection between the entity and the ai object has been made.
};

struct INode;
struct DebugNode;

typedef std::shared_ptr<DebugNode> DebugNodePtr;

struct DebugNode
{
	typedef DynArray<DebugNodePtr> Children;

	const INode* node;
	Children     children;

	DebugNode(const INode* _node) { node = _node; }
};

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
class DebugTree
{
public:
	void Push(const INode* node)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		DebugNodePtr debugNode(new DebugNode(node));

		if (!m_firstDebugNode)
			m_firstDebugNode = debugNode;

		if (!m_debugNodeStack.empty())
			m_debugNodeStack.back()->children.push_back(debugNode);

		m_debugNodeStack.push_back(debugNode);
	}

	void Pop(Status s)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		IF_UNLIKELY (s == Failure || s == Success)
		{
			m_succeededAndFailedNodes.push_back(m_debugNodeStack.back());
		}

		m_debugNodeStack.pop_back();

		if (s != Running)
		{
			if (!m_debugNodeStack.empty())
				m_debugNodeStack.back()->children.pop_back();
			else
				m_firstDebugNode.reset();
		}
	}

	DebugNodePtr GetFirstNode() const
	{
		return m_firstDebugNode;
	}

	const DynArray<DebugNodePtr>& GetSucceededAndFailedNodes() const
	{
		return m_succeededAndFailedNodes;
	}

private:
	DebugNodePtr           m_firstDebugNode;
	DynArray<DebugNodePtr> m_debugNodeStack;
	DynArray<DebugNodePtr> m_succeededAndFailedNodes;
};
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

typedef uint32 NodeID;

struct BehaviorVariablesContext
{
	BehaviorVariablesContext(Variables::Collection& _collection, const Variables::Declarations& _declarations, bool _variablesWereChangedBeforeCurrentTick)
		: collection(_collection)
		, declarations(_declarations)
		, variablesWereChangedBeforeCurrentTick(_variablesWereChangedBeforeCurrentTick)
	{
	}

	bool VariablesChangedSinceLastTick() const
	{
		return variablesWereChangedBeforeCurrentTick || collection.Changed();
	}

	Variables::Collection&         collection;
	const Variables::Declarations& declarations;
	const bool                     variablesWereChangedBeforeCurrentTick;
};

#ifdef USING_BEHAVIOR_TREE_LOG
class MessageQueue
{
public:
	typedef std::deque<string> Messages;

	void AddMessage(const char* message)
	{
		if (m_messages.size() + 1 > 20)
			m_messages.pop_front();

		m_messages.push_back(message);
	}

	const Messages& GetMessages() const
	{
		return m_messages;
	}

	void Clear()
	{
		m_messages.clear();
	}

private:
	Messages m_messages;
};
#endif // USING_BEHAVIOR_TREE_LOG

//////////////////////////////////////////////////////////////////////////

struct IBlackboardVariable
{
public:
	virtual ~IBlackboardVariable() {}

	template<typename Type>
	bool SetData(const Type& data)
	{
		if (Serialization::TypeID::get<Type>() != GetDataTypeId())
			return false;

		*static_cast<Type*>(GetDataVoidPointer()) = data;
		return true;
	}

	template<typename Type>
	bool GetData(Type& data) const
	{
		if (Serialization::TypeID::get<Type>() != GetDataTypeId())
			return false;

		data = *static_cast<const Type*>(GetDataVoidPointer());
		return true;
	}

	virtual Serialization::TypeID GetDataTypeId() const = 0;

private:
	virtual void*       GetDataVoidPointer() = 0;
	virtual const void* GetDataVoidPointer() const = 0;
};

DECLARE_SHARED_POINTERS(IBlackboardVariable);

template<class Type = void>
class BlackboardVariable : public IBlackboardVariable
{
public:
	BlackboardVariable(Type value)
	{
		m_value = value;
		m_typeId = Serialization::TypeID::get<Type>();
	}

	virtual Serialization::TypeID GetDataTypeId() const override { return m_typeId; }

private:
	BlackboardVariable() {};

	virtual void*       GetDataVoidPointer() override       { return &m_value; }
	virtual const void* GetDataVoidPointer() const override { return &m_value; }

	Serialization::TypeID m_typeId;
	Type                  m_value;
};

struct BlackboardVariableId
{
	BlackboardVariableId()
		: id(0)
	{};

	BlackboardVariableId(const char* _name)
	{
		id = GetIdValueFromName(_name);
#ifdef STORE_BLACKBOARD_VARIABLE_NAMES
		name = _name;
#endif
	};

	operator uint32() const { return id; };

	static inline uint32 GetIdValueFromName(const char* name)
	{
		return CCrc32::Compute(name);
	}

	uint32 id;
#ifdef STORE_BLACKBOARD_VARIABLE_NAMES
	string name;
#endif

};

class Blackboard
{
public:
	template<typename Type>
	bool SetVariable(BlackboardVariableId id, const Type& value)
	{
		BlackboardVariableArray::iterator blackboardVariableIt = std::find_if(m_blackboardVariableArray.begin(), m_blackboardVariableArray.end(), FindBlackboardVariable(id));

		if (blackboardVariableIt != m_blackboardVariableArray.end())
		{
			return blackboardVariableIt->second->SetData(value);
		}

		IBlackboardVariablePtr blackboardVariablePtr;
		blackboardVariablePtr.reset(new BlackboardVariable<Type>(value));
		m_blackboardVariableArray.push_back(std::make_pair(id, blackboardVariablePtr));
		return true;
	}

	template<typename Type>
	bool GetVariable(BlackboardVariableId id, Type& variable) const
	{
		BlackboardVariableArray::const_iterator blackboardVariableIt = std::find_if(m_blackboardVariableArray.begin(), m_blackboardVariableArray.end(), FindBlackboardVariable(id));

		if (blackboardVariableIt != m_blackboardVariableArray.end())
		{
			return blackboardVariableIt->second->GetData(variable);
		}

		return false;
	}

	typedef std::pair<BlackboardVariableId, IBlackboardVariablePtr> BlackboardVariableIdAndPtr;
	typedef DynArray<BlackboardVariableIdAndPtr>                    BlackboardVariableArray;

	const BlackboardVariableArray& GetBlackboardVariableArray() const { return m_blackboardVariableArray; }

private:

	struct FindBlackboardVariable
	{
		inline FindBlackboardVariable(BlackboardVariableId _id)
			: id(_id)
		{}

		inline bool operator()(const BlackboardVariableIdAndPtr& variableIdAndPtr) const
		{
			return variableIdAndPtr.first == id;
		}

		BlackboardVariableId id;
	};

	BlackboardVariableArray m_blackboardVariableArray;    //!< If using regular std::find proves too slow we could optimize by sorting elements.
};

//////////////////////////////////////////////////////////////////////////

//! 'Behavior Tree Template Meta Extensions' allow you to add custom data to the behavior tree template document for serialization.
//! This custom data will then be serialized as being a part of the document. This information can 'anything': it can be something
//! specific for your particular game, or it can also be information that is not used during the game at all but is used, for
//! example, the behavior tree editor to improve the work flow for working with the document.
struct IMetaExtension
{
	virtual ~IMetaExtension() {}
	virtual const char*           GetName() const = 0;
	virtual const char*           GetLabel() const = 0;
	virtual Serialization::TypeID GetTypeID() const = 0;
	virtual LoadResult            LoadFromXml(const XmlNodeRef& xml) = 0;
#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual bool                  CreateXmlDescription(XmlNodeRef& xml) const = 0;
#endif
#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) = 0;
#endif
};

DECLARE_SHARED_POINTERS(IMetaExtension);

typedef DynArray<IMetaExtensionPtr> IMetaExtensionPtrArray;

//! We store all the meta extensions for a behavior tree template in here.
class MetaExtensionTable
{
public:

	template<typename TYPE> TYPE* QueryMetaExtension()
	{
		const size_t index = IndexOfMetaExtension(Serialization::TypeID::get<TYPE>());
		return static_cast<TYPE*>(index < m_extensions.size() ? m_extensions[index].get() : nullptr);
	}

	template<typename TYPE> const TYPE* QueryMetaExtension() const
	{
		const size_t index = IndexOfMetaExtension(Serialization::TypeID::get<TYPE>());
		return static_cast<TYPE*>(index < m_extensions.size() ? m_extensions[index].get() : nullptr);
	}

	bool LoadFromXml(const XmlNodeRef& xml)
	{
		for (IMetaExtensionPtr& pExtension : m_extensions)
		{
			IMetaExtension& extension = *pExtension;
			if (XmlNodeRef extensionXml = xml->findChild(extension.GetName()))
			{
				if (extension.LoadFromXml(extensionXml) == LoadFailure)
				{
					return false;
				}
			}
		}
		return true;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION

	XmlNodeRef CreateXmlDescription() const
	{
		XmlNodeRef xml = GetISystem()->CreateXmlNode("MetaExtensions");
		for (const IMetaExtensionPtr& pExtension : m_extensions)
		{
			const IMetaExtension& extension = *pExtension;
			if (XmlNodeRef extensionXml = GetISystem()->CreateXmlNode(extension.GetName()))
			{
				if (extension.CreateXmlDescription(extensionXml))
				{
					xml->addChild(extensionXml);
				}
				else
				{
					return XmlNodeRef();
				}
			}
		}
		return xml;
	}

#endif // USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION

	void Serialize(Serialization::IArchive& archive)
	{
		for (IMetaExtensionPtr& pMetaExtension : m_extensions)
		{
			IMetaExtension& extension = *pMetaExtension;
			archive(extension, extension.GetName(), extension.GetLabel());
		}
	}

#endif

protected:

	friend class MetaExtensionFactory;

	void AddMetaExtension(IMetaExtensionPtr pExtension)
	{
		if (IndexOfMetaExtension(pExtension->GetTypeID()) != m_extensions.size())
		{
			CryFatalError("Behavior tree meta extension already registered!");
		}

		m_extensions.push_back(pExtension);
	}

private:

	size_t IndexOfMetaExtension(const Serialization::TypeID& typeID) const
	{
		// TODO : Consider sorting extensions by type id to improve performance of queries.
		for (IMetaExtensionPtrArray::const_iterator iter = m_extensions.begin(), end = m_extensions.end(); iter != end; ++iter)
		{
			if ((*iter)->GetTypeID() == typeID)
			{
				return iter - m_extensions.begin();
			}
		}
		return m_extensions.size();
	}

private:

	IMetaExtensionPtrArray m_extensions;
};

struct UpdateContext
{
	UpdateContext(
	  const EntityId _id
	  , IEntity& _entity
	  , BehaviorVariablesContext& _variables
	  , TimestampCollection& _timestamps
	  , Blackboard& _blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
	  , MessageQueue& _behaviorLog
#endif // USING_BEHAVIOR_TREE_LOG
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	  , DebugTree* _debugTree = NULL
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	  )
		: entityId(_id)
		, entity(_entity)
		, runtimeData(NULL)
		, variables(_variables)
		, timestamps(_timestamps)
		, blackboard(_blackboard)
#ifdef USING_BEHAVIOR_TREE_LOG
		, behaviorLog(_behaviorLog)
#endif // USING_BEHAVIOR_TREE_LOG
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		, debugTree(_debugTree)
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
	{
	}

	EntityId                  entityId;
	IEntity&                  entity;
	void*                     runtimeData;
	BehaviorVariablesContext& variables;
	TimestampCollection&      timestamps;
	Blackboard&               blackboard;

#ifdef USING_BEHAVIOR_TREE_LOG
	MessageQueue& behaviorLog;
#endif // USING_BEHAVIOR_TREE_LOG

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	DebugTree* debugTree;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

};

struct EventContext
{
	EventContext(const EntityId _id)
		: entityId(_id)
		, runtimeData(NULL)
	{
	}

	EntityId entityId;
	void*    runtimeData;
};

struct INode
{
	virtual ~INode() {}

	//! When you "tick" a node the following rules apply:
	//! 1. If the node was not previously running the node will first be
	//!  initialized and then immediately updated.
	//! 2. If the node was previously running, but no longer is, it will be
	//!  terminated.
	virtual Status Tick(const UpdateContext& context) = 0;

	//! Call this to explicitly terminate a node.
	//! The node will itself take care of cleaning things up.
	//! It's safe to call Terminate on an already terminated node, although it is of course redundant.
	virtual void Terminate(const UpdateContext& context) = 0;

	//! Load up a behavior tree node with information from an xml node.
	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context) = 0;

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() = 0;
#endif

	//! Send an event to the node.
	//! The event will be dispatched to the correct HandleEvent method with validated runtime data.
	//! Never override this!
	virtual void SendEvent(const EventContext& context, const Event& event) = 0;

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) = 0;
#endif
};

DECLARE_SHARED_POINTERS(INode);

//! This is the recipe for a behavior tree.
//! The information in this template should be considered to be immutable (read-only).
//! You can create a behavior tree instance from a template.
struct BehaviorTreeTemplate
{
	BehaviorTreeTemplate() {}

	BehaviorTreeTemplate(
	  INodePtr& _rootNode,
	  TimestampCollection& _timestampCollection,
	  Variables::Declarations& _variableDeclarations,
	  Variables::SignalHandler& _signals)
		: rootNode(_rootNode)
		, defaultTimestampCollection(_timestampCollection)
		, variableDeclarations(_variableDeclarations)
		, signalHandler(_signals)
	{
	}

	MetaExtensionTable       metaExtensionTable;
	INodePtr                 rootNode;
	TimestampCollection      defaultTimestampCollection;
	Variables::Declarations  variableDeclarations;
	Variables::SignalHandler signalHandler;

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE)
	CryFixedStringT<64> mbtFilename;
#endif
};

DECLARE_SHARED_POINTERS(BehaviorTreeTemplate);

//! This contains all the data for a behavior tree that can be modified during runtime.
//! The behavior tree instance is created from a behavior tree template.
struct BehaviorTreeInstance
{
	BehaviorTreeInstance() {}

	BehaviorTreeInstance(
	  const TimestampCollection& _timestampCollection,
	  const Variables::Collection& _variables,
	  BehaviorTreeTemplatePtr _behaviorTreeTemplate,
	  INodeFactory& nodeFactory)
		: timestampCollection(_timestampCollection)
		, variables(_variables)
		, behaviorTreeTemplate(_behaviorTreeTemplate)
	{
	}

	TimestampCollection           timestampCollection;
	Variables::Collection         variables;
	const BehaviorTreeTemplatePtr behaviorTreeTemplate;
	Blackboard                    blackboard;

#ifdef USING_BEHAVIOR_TREE_LOG
	MessageQueue behaviorLog;
#endif // USING_BEHAVIOR_TREE_LOG

#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	MessageQueue eventsLog;
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
};

DECLARE_SHARED_POINTERS(BehaviorTreeInstance);

struct IBehaviorTreeManager
{
	virtual ~IBehaviorTreeManager() {}

	virtual void Update() = 0;

	virtual struct IMetaExtensionFactory&  GetMetaExtensionFactory() = 0;
	virtual struct INodeFactory&           GetNodeFactory() = 0;
#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual NodeSerializationClassFactory& GetNodeSerializationFactory() = 0;
#endif
	virtual void                           LoadFromDiskIntoCache(const char* behaviorTreeName) = 0;
	virtual bool StartModularBehaviorTree(const EntityId entityId, const char* treeName) = 0;
	virtual bool StartModularBehaviorTreeFromXml(const EntityId entityId, const char* treeName, XmlNodeRef treeXml) = 0;
	virtual void StopModularBehaviorTree(const EntityId entityId) = 0;

	virtual void HandleEvent(const EntityId entityId, Event& event) = 0;

	virtual BehaviorTree::Blackboard* GetBehaviorTreeBlackboard(const EntityId entityId) = 0;

	//! Todo: Remove these functions as the behavior variables are a internal concept of the behavior tree and should not be directly accessed from the outside.
	//! Instead of changing directly the internal state of the variable, the external systems should inform the tree of a
	//! particular event and let the behavior tree decide if and what variables should be changed.
	virtual Variables::Collection*         GetBehaviorVariableCollection_Deprecated(const EntityId entityId) const = 0;
	virtual const Variables::Declarations* GetBehaviorVariableDeclarations_Deprecated(const EntityId entityId) const = 0;
};

typedef uint64 RuntimeDataID;

inline RuntimeDataID MakeRuntimeDataID(const EntityId entityID, const NodeID nodeID)
{
	static_assert(sizeof(entityID) == 4, "Expected entity id to be 4 bytes");
	static_assert(sizeof(nodeID) == 4, "Expected node id to be 4 bytes");
	static_assert(sizeof(RuntimeDataID) == 8, "Expected runtime data id to be 8 bytes");

	const RuntimeDataID runtimeDataID = (uint64)entityID | (((uint64)nodeID) << 32);
	return runtimeDataID;
}

typedef IMetaExtensionPtr (* MetaExtensionCreator)();

struct IMetaExtensionFactory
{
	virtual ~IMetaExtensionFactory() {}
	virtual void RegisterMetaExtensionCreator(MetaExtensionCreator metaExtensionCreator) = 0;
	virtual void CreateMetaExtensions(MetaExtensionTable& metaExtensionTable) = 0;
};

template<typename META_EXTENSION_TYPE> IMetaExtensionPtr CreateMetaExtension()
{
	return IMetaExtensionPtr(new META_EXTENSION_TYPE);
}

struct INodeCreator
{
	virtual ~INodeCreator() {}
	virtual INodePtr    Create() = 0;
	virtual void        Trim() = 0;
	virtual const char* GetTypeName() const = 0;
	virtual size_t      GetNodeClassSize() const = 0;
	virtual size_t      GetSizeOfImmutableDataForAllAllocatedNodes() const = 0;
	virtual size_t      GetSizeOfRuntimeDataForAllAllocatedNodes() const = 0;
	virtual void*       AllocateRuntimeData(const RuntimeDataID runtimeDataID) = 0;
	virtual void*       GetRuntimeData(const RuntimeDataID runtimeDataID) const = 0;
	virtual void        FreeRuntimeData(const RuntimeDataID runtimeDataID) = 0;
	virtual void        SetNodeFactory(INodeFactory* nodeFactory) = 0;
};

struct INodeFactory
{
	virtual ~INodeFactory() {}
	virtual INodePtr CreateNodeOfType(const char* typeName) = 0;
	virtual INodePtr CreateNodeFromXml(const XmlNodeRef& xml, const LoadContext& context) = 0;
	virtual void     RegisterNodeCreator(struct INodeCreator* nodeCreator) = 0;
	virtual size_t   GetSizeOfImmutableDataForAllAllocatedNodes() const = 0;
	virtual size_t   GetSizeOfRuntimeDataForAllAllocatedNodes() const = 0;
	virtual void*    AllocateRuntimeDataMemory(const size_t size) = 0;
	virtual void     FreeRuntimeDataMemory(void* pointer) = 0;
	virtual void*    AllocateNodeMemory(const size_t size) = 0;
	virtual void     FreeNodeMemory(void* pointer) = 0;
};

template<typename NodeType, typename NodeCreatorType>
class NodeDeleter
{
public:
	NodeDeleter(INodeFactory& nodeFactory, NodeCreatorType& creator)
		: m_nodeFactory(nodeFactory), m_creator(creator)
	{
	}

	void operator()(NodeType* node)
	{
		node->~NodeType();
		m_nodeFactory.FreeNodeMemory(node);
		--m_creator.m_nodeCount;
	}

private:
	INodeFactory&    m_nodeFactory;
	NodeCreatorType& m_creator;
};

template<typename NodeType> class NodeCreator : public INodeCreator
{
	typedef NodeCreator<NodeType>           ThisNodeCreatorType;
	typedef typename NodeType::RuntimeData  RuntimeDataType;
	typedef VectorMap<RuntimeDataID, void*> RuntimeDataCollection;

	friend class NodeDeleter<NodeType, ThisNodeCreatorType>;

public:
	NodeCreator(const char* typeName)
		: m_typeName(typeName)
		, m_nodeCount(0)
	{
	}

	virtual INodePtr Create() override
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Modular Behavior Tree Node Factory: %s", m_typeName);

		assert(m_nodeFactory != NULL);

		void* const pointer = m_nodeFactory->AllocateNodeMemory(sizeof(NodeType));

		if (pointer == NULL)
		{
			CryFatalError("Could not allocate the memory needed for a '%s' node.", GetTypeName());
		}

		++m_nodeCount;

		return INodePtr(new(pointer) NodeType(), NodeDeleter<NodeType, ThisNodeCreatorType>(*m_nodeFactory, *this));
	}

	virtual void Trim() override
	{
		assert(m_runtimeDataCollection.empty());
		if (m_runtimeDataCollection.empty())
			stl::free_container(m_runtimeDataCollection);
		else
			gEnv->pLog->LogError("Failed to free runtime data collection. There were still %" PRISIZE_T " runtime data elements in '%s' node creator.", m_runtimeDataCollection.size(), m_typeName);
	}

	virtual const char* GetTypeName() const override
	{
		return m_typeName;
	}

	virtual size_t GetNodeClassSize() const override
	{
		return sizeof(NodeType);
	}

	virtual size_t GetSizeOfImmutableDataForAllAllocatedNodes() const override
	{
		return m_nodeCount * (sizeof(NodeType) + sizeof(INodePtr));
	}

	virtual size_t GetSizeOfRuntimeDataForAllAllocatedNodes() const override
	{
		return m_runtimeDataCollection.size() * sizeof(RuntimeDataType);
	}

	virtual void* AllocateRuntimeData(const RuntimeDataID runtimeDataID) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		void* pointer = m_nodeFactory->AllocateRuntimeDataMemory(sizeof(RuntimeDataType));
		RuntimeDataType* runtimeData = new(pointer) RuntimeDataType;
		assert(runtimeData != NULL);
		m_runtimeDataCollection.insert(std::make_pair(runtimeDataID, reinterpret_cast<void*>(runtimeData)));
		return runtimeData;
	}

	virtual void* GetRuntimeData(const RuntimeDataID runtimeDataID) const override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		typename RuntimeDataCollection::const_iterator it = m_runtimeDataCollection.find(runtimeDataID);
		if (it != m_runtimeDataCollection.end())
		{
			return it->second;
		}

		return NULL;
	}

	virtual void FreeRuntimeData(const RuntimeDataID runtimeDataID) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		typename RuntimeDataCollection::iterator it = m_runtimeDataCollection.find(runtimeDataID);
		assert(it != m_runtimeDataCollection.end());
		if (it != m_runtimeDataCollection.end())
		{
			RuntimeDataType* runtimeData = reinterpret_cast<RuntimeDataType*>(it->second);
			runtimeData->~RuntimeDataType();
			m_nodeFactory->FreeRuntimeDataMemory(runtimeData);
			m_runtimeDataCollection.erase(it);
		}
	}

	virtual void SetNodeFactory(INodeFactory* nodeFactory) override
	{
		m_nodeFactory = nodeFactory;
	}

private:
	const char*           m_typeName;
	INodeFactory*         m_nodeFactory;
	RuntimeDataCollection m_runtimeDataCollection;
	size_t                m_nodeCount;
};

//! Register your meta extension creator with the passed in manager.
#define REGISTER_BEHAVIOR_TREE_META_EXTENSION(manager, metaExtensiontype)                                     \
  {                                                                                                           \
    manager.GetMetaExtensionFactory().RegisterMetaExtensionCreator(&CreateMetaExtension<metaExtensiontype> ); \
  }

//! Register your node creator with the passed in manager.
//! You need to have made your creator available via GenerateBehaviorTreeNodeCreator.
#define REGISTER_BEHAVIOR_TREE_NODE(manager, nodetype)                      \
  {                                                                         \
    static NodeCreator<nodetype> nodetype ## NodeCreator( # nodetype);      \
    manager.GetNodeFactory().RegisterNodeCreator(&nodetype ## NodeCreator); \
  }

#define REGISTER_BEHAVIOR_TREE_NODE_WITH_NAME(manager, nodetype, nodename) \
  { \
    static NodeCreator<nodetype> nodetype##NodeCreator(nodename); \
    manager.GetNodeFactory().RegisterNodeCreator(&nodetype##NodeCreator); \
  }

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	#define REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, nodetype, label, color)                                 \
	  {                                                                                                                     \
	    REGISTER_BEHAVIOR_TREE_NODE(manager, nodetype);                                                                     \
	    SERIALIZATION_CLASS_NAME_FOR_FACTORY(manager.GetNodeSerializationFactory(), INode, nodetype, # nodetype, label);    \
	    SERIALIZATION_CLASS_ANNOTATION_FOR_FACTORY(manager.GetNodeSerializationFactory(), INode, nodetype, "color", color); \
	  }
#else
	#define REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, nodetype, label, color) \
	  {                                                                                     \
	    REGISTER_BEHAVIOR_TREE_NODE(manager, nodetype);                                     \
	  }
#endif

}

//! \endcond