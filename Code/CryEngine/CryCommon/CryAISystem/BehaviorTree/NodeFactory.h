// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Node.h"
#include <CryCore/Containers/VectorMap.h>
#include "IBehaviorTree.h"
#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
	#include <CryMemory/BucketAllocatorImpl.h>
#endif

namespace BehaviorTree
{
class NodeFactory : public INodeFactory
{
public:
	NodeFactory() : m_nextNodeID(0)
	{
#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
		s_bucketAllocator.EnableExpandCleanups(false);
#endif
	}

	void CleanUpBucketAllocator()
	{
		s_bucketAllocator.cleanup();
	}

	virtual INodePtr CreateNodeOfType(const char* typeName) override
	{
		INodePtr node;

		NodeCreators::iterator nodeCreatorIt = m_nodeCreators.find(stack_string(typeName));
		if (nodeCreatorIt != m_nodeCreators.end())
		{
			INodeCreator* creator = nodeCreatorIt->second;
			node = creator->Create();

			static_cast<Node*>(node.get())->m_id = m_nextNodeID++;
			static_cast<Node*>(node.get())->SetCreator(creator);
		}

		if (!node)
		{
			gEnv->pLog->LogError("Failed to create behavior tree node of type '%s'.", typeName);
		}

		return node;
	}

	virtual INodePtr CreateNodeFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		INodePtr node = context.nodeFactory.CreateNodeOfType(xml->getTag());

		if (node)
		{
			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "CreateNodeFromXml: %s", xml->getTag());

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
			static_cast<Node*>(node.get())->SetXmlLine(xml->getLine());
#endif

			if (node->LoadFromXml(xml, context) == LoadSuccess)
			{
				return node;
			}
		}

		return INodePtr();
	}

	virtual void RegisterNodeCreator(INodeCreator* nodeCreator) override
	{
		nodeCreator->SetNodeFactory(this);
		m_nodeCreators.insert(std::make_pair(stack_string(nodeCreator->GetTypeName()), nodeCreator));

#ifdef USING_BEHAVIOR_TREE_DEBUG_MEMORY_USAGE
		gEnv->pLog->Log("Modular behavior tree node '%s' has a class size of %" PRISIZE_T " bytes.", nodeCreator->GetTypeName(), nodeCreator->GetNodeClassSize());
#endif
	}

	virtual void TrimNodeCreators()
	{
		NodeCreators::const_iterator it = m_nodeCreators.begin();
		NodeCreators::const_iterator end = m_nodeCreators.end();

		for (; it != end; ++it)
		{
			INodeCreator* creator = it->second;
			creator->Trim();
		}
	}

	virtual size_t GetSizeOfImmutableDataForAllAllocatedNodes() const override
	{
		size_t total = 0;

		NodeCreators::const_iterator it = m_nodeCreators.begin();
		NodeCreators::const_iterator end = m_nodeCreators.end();

		for (; it != end; ++it)
		{
			INodeCreator* creator = it->second;
			total += creator->GetSizeOfImmutableDataForAllAllocatedNodes();
		}

		return total;
	}

	virtual size_t GetSizeOfRuntimeDataForAllAllocatedNodes() const override
	{
		size_t total = 0;

		NodeCreators::const_iterator it = m_nodeCreators.begin();
		NodeCreators::const_iterator end = m_nodeCreators.end();

		for (; it != end; ++it)
		{
			INodeCreator* creator = it->second;
			total += creator->GetSizeOfRuntimeDataForAllAllocatedNodes();
		}

		return total;
	}

	virtual void* AllocateRuntimeDataMemory(const size_t size) override
	{
		return s_bucketAllocator.allocate(size);
	}

	virtual void FreeRuntimeDataMemory(void* pointer) override
	{
		assert(s_bucketAllocator.IsInAddressRange(pointer));

		if (s_bucketAllocator.IsInAddressRange(pointer))
		{
			s_bucketAllocator.deallocate(pointer);
		}
	}

	//! This will be called while loading a level or jumping into game in the editor.
	//! The memory will remain until the level is unloaded or we exit the game in the editor.
	virtual void* AllocateNodeMemory(const size_t size) override
	{
		return new char[size];
	}

	//! This will be called while unloading a level or exiting game in the editor.
	virtual void FreeNodeMemory(void* pointer) override
	{
		delete[] (char*)pointer;
	}

private:
	typedef VectorMap<stack_string, INodeCreator*>                                                                                 NodeCreators;
#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
	typedef BucketAllocator<BucketAllocatorDetail::DefaultTraits<(2*1024*1024), BucketAllocatorDetail::SyncPolicyUnlocked, false>> BehaviorTreeBucketAllocator;
#else
	typedef node_alloc<eCryDefaultMalloc, true, 2*1024*1024>                                                                       BehaviorTreeBucketAllocator;
#endif

	NodeCreators                       m_nodeCreators;
	NodeID                             m_nextNodeID;
	static BehaviorTreeBucketAllocator s_bucketAllocator;
};
}
