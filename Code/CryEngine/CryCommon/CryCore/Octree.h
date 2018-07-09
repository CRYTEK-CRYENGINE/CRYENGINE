// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_GeoOverlap.h>

namespace OctreeUtils
{
enum EOctant
{
	// m => minus == 0, p => plus == 1
	// X is Least Significant Bit, Z is Most Significant Bit
	eOctant_XmYmZm = 0,
	eOctant_XpYmZm = 1,
	eOctant_XmYpZm = 2,
	eOctant_XpYpZm = 3,
	eOctant_XmYmZp = 4,
	eOctant_XpYmZp = 5,
	eOctant_XmYpZp = 6,
	eOctant_XpYpZp = 7,

	eOctant_Count
};

inline AABB MakeCollapsedAABB()                        { return AABB(Vec3(FLT_MAX, FLT_MAX, FLT_MAX), Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX)); }
inline void RequestXMinCollapsed(Vec3& inOutReference) { inOutReference.x = -FLT_MAX; }
inline void RequestXMaxCollapsed(Vec3& inOutReference) { inOutReference.x = FLT_MAX; }
inline void RequestYMinCollapsed(Vec3& inOutReference) { inOutReference.y = -FLT_MAX; }
inline void RequestYMaxCollapsed(Vec3& inOutReference) { inOutReference.y = FLT_MAX; }
inline void RequestZMinCollapsed(Vec3& inOutReference) { inOutReference.z = -FLT_MAX; }
inline void RequestZMaxCollapsed(Vec3& inOutReference) { inOutReference.z = FLT_MAX; }
inline bool IsCollapsed(const AABB& domain)            { return domain.min.x > domain.max.x; }

struct SIntersectAll
{
};

struct SBasicIntersections
{
	struct SUserDataPtr
	{
		template<typename T>
		inline SUserDataPtr(const T*) {}
	};
	inline bool Intersects(const AABB& aabb, const SIntersectAll&, const SUserDataPtr&) const { return true; }
	inline bool Intersects(const AABB& aabb, const Vec3& v, const SUserDataPtr&) const        { return Overlap::Point_AABB(v, aabb); }
	inline bool Intersects(const AABB& aabb, const Sphere& s, const SUserDataPtr&) const      { return Overlap::Sphere_AABB(s, aabb); }
	inline bool Intersects(const AABB& aabb, const AABB& box, const SUserDataPtr&) const      { return Overlap::AABB_AABB(box, aabb); }
};
}

/*
 * The Octree implemented here is a just container for whichever primitive you want to store. It is actually even made to handle the storage
 * of partial or incomplete primitives (for instance, just a triangle index instead of the whole triangle).
 * To make that possible, you'll need to provide the Octree template with your own custom primitive container (TPrimitiveContainer) and
 * possibly also a type that will contain the data to make sense out your incomplete primitives (TUserData). In the aforementioned case with
 * triangle index, your UserData might well be the container for the actual triangles.
 * Delegating the knowledge about the actual content implies a few requirements for the interface of the TPrimitiveContainer that you'll
 * need to fullfill :
 *
 * -> Constructor accepting a pointer to your TUserData.
 *
 * -> Move (or copy) constructor if you plan to call Solidify on your Octree.
 *
 * -> Intersection tests to find which leaf (leaves ?) should contain your primitive are handled by implementing as many Intersects() methods
 *    as primitives you'll want to support. Note by the way that OctreeUtils::SBasicIntersections already offers an implementation of that
 *    function for a few basic types so you don't need to reimplement them for each and every container. Here is the full prototype of a
 *    typical Intersects() method :
 *  bool Intersects(const AABB& aabb, const TPrimitiveType& primitive, const TUserData* pUserData) const;
 *
 * -> Unsurprisingly, for the insertion of primitives, you'll also need to provide a Push() method for each primitive type that you'll want
 *    your container to hold. These methods should have the following prototype
 *  void Push(const TPrimitiveType& primitive);
 *
 * -> The Octree doesn't know *when* to split a container so you need to implement a function that decides whether it's worth splitting and
 *    where to split. This is done by implementing a ShouldSplit function in your primitive container with the following prototype :
 *  bool ShouldSplit(Vec3& outSplitReference, const size_t nodeDepth, const AABB& domain, TUserData* pUserData) const;
 *
 * -> The Octree doesn't know *how* to split your container so you need to implement a function to split the content of your container into the
 *    octants. This function is expected to have the following prototype :
 *  void Split(TPrimitiveContainer* (&octantContainers)[OctreeUtils::eOctant_Count]
 *            , const AABB (&octantAABBs)[OctreeUtils::eOctant_Count],const Vec3& splitReference, TUserData* pUserData);
 *
 * -> The Split function isn't forced to clear the data, so expect a call to FreeMemory() right after the split has been done :
 *  void FreeMemory();
 *
 * -> If your Octree isn't going to hold a purely static set of primitives, you'll have to properly implement the Remove() and Replace() methods
 *    You can leave them empty otherwise. Here are their respective prototypes
 *  void Remove(const TPrimitive& primitive);
 *  void Replace(const TPrimitive& fromPrimitive, const TPrimitive& toPrimitive);
 *
 *
 * Given the rather large list of requirements for the TPrimitiveContainer, here is a small container for Vec3s that fullfills all of them.
 * This should be a good starting point for your own implementation :

   struct SVec3Container
   : OctreeUtils::SBasicIntersections
   {
   typedef std::vector< Vec3 > TPoints;
   using OctreeUtils::SBasicIntersections::Intersects;

   inline SVec3Container(void*) {}
   inline SVec3Container(SVec3Container&& other) : content(std::move(other.content)) {}
   inline void Push(const Vec3& v)                         { content.push_back(v); }
   inline void Remove(const Vec3& v)                       { content.erase(std::find(content.begin(), content.end(), v)); }
   inline void Replace(const Vec3& vFrom, const Vec3& vTo) { *std::find(content.begin(), content.end(), vFrom) = vTo; }
   inline void FreeMemory()                                  { TPoints().swap(content); }
   void Split(SVec3Container* (&octantContainers)[OctreeUtils::eOctant_Count], const AABB (&octantAABBs)[OctreeUtils::eOctant_Count],const Vec3& reference, void* pUserData);
   bool ShouldSplit(Vec3& outSplitReference, const size_t nodeDepth, const AABB& domain, void* pUserData) const;

   TPoints content;
   };

   void SVec3Container::Split(SVec3Container* (&octantContainers)[OctreeUtils::eOctant_Count], const AABB (&octantsBoxes)[OctreeUtils::eOctant_Count], const Vec3& reference, void* pUserData)
   {
   (void)octantsBoxes;
   (void)pUserData;
   for (TPoints::const_iterator it = content.begin(), itEnd = content.end(); it != itEnd; ++it)
   {
    const Vec3& diff = *it - reference;
    const int octant = ((diff.x >= 0.0f) ? 1 : 0) | ((diff.y >= 0.0f) ? 2 : 0) | ((diff.z >= 0.0f) ? 4 : 0);
    octantContainers[octant]->content.push_back(*it);
   }
   }

   bool SVec3Container::ShouldSplit(Vec3& outSplitReference, const size_t nodeDepth, const AABB& domain, void* pUserData) const
   {
   (void)nodeDepth;
   (void)pUserData;
   static const size_t NMaxPrimitives = 32;
   bool result = content.size() > NMaxPrimitives;
   if (result)
   {
    outSplitReference = domain.GetCenter();
   }
   return result;
   }

 */

template<typename TPrimitiveContainer, typename TUserData>
class COctree
{
public:
	struct Node;
	typedef std::vector<Node*> TChildrenChunksPtrs;
	typedef size_t             TNodeId;
	static const TNodeId s_invalidNodeId = -1;
	inline static TNodeId NodeToId(Node* pNode, const TChildrenChunksPtrs& nodeChunkPtrs);

	//! \note SplitDomains will output collapsed AABBs for some octants when the reference is outside of the inDomain.
	inline static void SplitDomains(AABB(&outDomains)[OctreeUtils::eOctant_Count], const AABB &inDomain, const Vec3 &reference);

	struct SConstLeafInfo
	{
		SConstLeafInfo() : pNode(nullptr), depth(-1) {}
		SConstLeafInfo(const Node* pNode, size_t depth) : pNode(pNode), depth(depth) {}
		const Node* pNode;
		size_t      depth;
	};

	struct SLeafInfo
	{
		SLeafInfo() : pNode(nullptr), depth(-1) {}
		SLeafInfo(Node* pNode, size_t depth) : pNode(pNode), depth(depth) {}
		inline bool operator==(const SLeafInfo& other) const { return pNode == other.pNode; }
		Node*  pNode;
		size_t depth;
	};

	typedef std::vector<SConstLeafInfo> TConstLeaves;
	typedef std::vector<SLeafInfo>      TLeaves;

	//! Construct an octree for a limited domain. The given pUserData will be transmitted to the primitive containers on all relevant calls.
	//! If preAllocatedNodeCount is non-zero, you'll need to initialize each node individually through calls to ConstructPreAllocatedNode().
	COctree(const AABB& domain, TUserData* pUserData, size_t preAllocatedNodeCount = 0);
	~COctree();

	//! PushInitialPrimitive will store the given primitive in the root node.
	//! This function should probably be your main way to insert primitives when building an octree from a large set of primitives.
	//! When done with inserting all primitives, you can force an evaluation of the splitting through a call to ForceEvaluateSplitting().
	template<typename T>
	void PushInitialPrimitive(const T& primitive);

	//! Forces the evaluation for every leaf whether that leaf should be split or not.
	//! Call this after populating the octree with new primitives through interfaces that didn't allow splitting.
	void ForceEvaluateSplitting();

	//! Forces the evaluation for every given leaf whether that leaf should be split or not.
	void ForceEvaluateSplitting(const TLeaves& leaves);

	//! After having constructed an octree with a given amount of pre-allocated nodes, it is expected that you construct each node through this function.
	Node*        ConstructPreAllocatedNode(TNodeId nodeId, TNodeId firstChildId, const AABB& childDomain);

	inline Node* GetRoot() { return &m_root; }

	template<typename TFunc>
	inline void ForEachNode_RootToLeaves(const TFunc& func)
	{
		ForEachNode_RootToLeaves(func, &m_root, 0);
	}

	template<typename TFunc>
	inline void ForEachNode_LeavesToRoot(const TFunc& func)
	{
		ForEachNode_LeavesToRoot(func, &m_root, 0);
	}

	template<typename T>
	void FindIntersectingLeaves(TLeaves& outNodes, const T& primitive);

	template<typename T>
	void FindIntersectingLeaves(TConstLeaves& outNodes, const T& primitive) const;

	//! Insert a primitive into the proper leaf (or leaves if it overlaps multiple).
	//! \param bAllowSplitting If this is set to true (default value) then nodes receiving a Push call will also be evaluated for splitting.
	template<typename T>
	void PushInPlace(const T& primitive, bool bAllowSplitting = true);

	//! Remove a given primitive. Note : Nodes will never be merged as a result of removing primitives.
	template<typename T>
	void Remove(const T& primitive);

	//! Move from primitiveFrom to primitiveTo. Impacted nodes will receive a call to either Push, Remove or Replace, depending on what's best suiting.
	//! \param bAllowSplitting If this is set to true (default value) then nodes receiving a Push call will also be evaluated for splitting.
	template<typename T>
	void Move(const T& primitiveFrom, const T& primitiveTo, bool bAllowSplitting = true);

	//! Reallocate all nodes in one batch to keep memory contiguous. You'll probably only want to use that when your octree is filled with static data.
	void DefragmentNodes();

private:
	template<typename T>
	void FindIntersectingLeaves(TLeaves& outNodes1, TLeaves& outNodes2, TLeaves& outNodesBoth, const T& primitive1, const T& primitive2);

	template<typename T>
	void FindIntersectingLeavesInChunk(TLeaves& outNodes, const T& primitive, Node* pNodeChunk, size_t depth);

	template<typename T>
	void FindIntersectingLeavesInChunk(TLeaves& outNodes1, TLeaves& outNodes2, TLeaves& outNodesBoth, const T& primitive1, const T& primitive2, Node* pNodeChunk, size_t depth);

	template<typename T>
	void FindIntersectingLeavesInChunk(TConstLeaves& outNodes, const T& primitive, Node* pNodeChunk, size_t depth) const;

	template<typename TFunc>
	void ForEachNode_RootToLeaves(const TFunc& func, Node* pNode, size_t depth);

	template<typename TFunc>
	void ForEachNode_LeavesToRoot(const TFunc& func, Node* pNode, size_t depth);

	void EvaluateSplitting(const SLeafInfo leaf);

	void DestroyAllChildrenNodes();

	Node       m_root;
	TUserData* m_pUserData;
	Node*      m_pBufferStart;
	Node*      m_pBufferEnd;
};

template<typename TPrimitiveContainer, typename TUserData>
struct COctree<TPrimitiveContainer, TUserData >::Node
{
	inline Node(const AABB &domain, TUserData * pUserData);
	inline Node(Node && other, Node * newChildrenChunk);
	inline ~Node();
	inline void BufferFree(Node* pBufferStart, Node* pBufferEnd)                               { pChildren = ((pChildren >= pBufferStart) && (pChildren < pBufferEnd)) ? nullptr : pChildren; }
	inline bool IsLeaf() const                                                                 { return pChildren == nullptr; }
	inline bool IsCollapsed() const                                                            { return OctreeUtils::IsCollapsed(domain); }
	inline bool ShouldSplit(Vec3& outSplitReference, size_t depth, TUserData* pUserData) const { return primitiveContainer.ShouldSplit(outSplitReference, depth, domain, pUserData); }
	void        Split(const Vec3& reference, TUserData* pUserData);
	void        FlattenChildrenChunksInto(TChildrenChunksPtrs& out) const;
	void        DestroyChildren();

	AABB                domain;
	TPrimitiveContainer primitiveContainer;
	Node*               pChildren;

private:
	Node();                                    //!< Disabled as we don't want it.
	Node(const Node &);                        //!< Disabled as we don't want it.
	const Node&      operator=(const Node&);   //!< Disabled as we don't want it.
};

template<typename TPrimitiveContainer, typename TUserData>
COctree<TPrimitiveContainer, TUserData>::Node::Node(const AABB& domain, TUserData* pUserData)
	: domain(domain)
	, primitiveContainer(pUserData)
	, pChildren(nullptr)
{
}

template<typename TPrimitiveContainer, typename TUserData>
COctree<TPrimitiveContainer, TUserData>::Node::Node(Node&& other, Node* newChildrenChunk)
	: domain(other.domain)
	, primitiveContainer(std::move(other.primitiveContainer))
	, pChildren(newChildrenChunk)
{
}

template<typename TPrimitiveContainer, typename TUserData>
COctree<TPrimitiveContainer, TUserData>::Node::~Node()
{
	DestroyChildren();
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData>::Node::DestroyChildren()
{
	if (pChildren)
	{
		for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
		{
			pChildren[i].~Node();
		}
		free(pChildren);
		pChildren = nullptr;
	}
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData>::Node::Split(const Vec3 &reference, TUserData * pUserData)
{
	CRY_ASSERT_MESSAGE(IsLeaf(), "Cannot split a non-leaf node from the octree.");
	pChildren = reinterpret_cast<Node*>(malloc(sizeof(Node) * OctreeUtils::eOctant_Count));
	TPrimitiveContainer* octants[OctreeUtils::eOctant_Count] = { nullptr };
	AABB octantDomains[OctreeUtils::eOctant_Count];
	SplitDomains(octantDomains, domain, reference);
	for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
	{
		new(&pChildren[i])Node(octantDomains[i], pUserData);
		octants[i] = &pChildren[i].primitiveContainer;
	}
	primitiveContainer.Split(octants, octantDomains, reference, pUserData);
	primitiveContainer.FreeMemory();
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData>::Node::FlattenChildrenChunksInto(TChildrenChunksPtrs & out) const
{
	if (pChildren)
	{
		out.push_back(pChildren);
		for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
		{
			pChildren[i].FlattenChildrenChunksInto(out);
		}
	}
}

template<typename TPrimitiveContainer, typename TUserData>
COctree<TPrimitiveContainer, TUserData>::COctree(const AABB& domain, TUserData* pUserData, size_t preAllocatedNodeCount)
	: m_root(domain, pUserData)
	, m_pUserData(pUserData)
	, m_pBufferStart(nullptr)
	, m_pBufferEnd(nullptr)
{
	if (preAllocatedNodeCount > 0)
	{
		m_pBufferStart = reinterpret_cast<Node*>(malloc(preAllocatedNodeCount * sizeof(Node)));
		m_pBufferEnd = m_pBufferStart + preAllocatedNodeCount;

		m_root.pChildren = m_pBufferStart;
	}
}

template<typename TPrimitiveContainer, typename TUserData>
COctree<TPrimitiveContainer, TUserData>::~COctree()
{
	DestroyAllChildrenNodes();
}

template<typename TPrimitiveContainer, typename TUserData>
typename COctree<TPrimitiveContainer, TUserData>::Node * COctree<TPrimitiveContainer, TUserData>::ConstructPreAllocatedNode(TNodeId nodeId, TNodeId firstChildId, const AABB &childDomain)
{
	Node* pNode = &m_pBufferStart[nodeId];
	new(pNode) Node(childDomain, m_pUserData);

	if (firstChildId != s_invalidNodeId)
	{
		pNode->pChildren = &m_pBufferStart[firstChildId];
	}

	return pNode;
}

template<typename TPrimitiveContainer, typename TUserData>
inline typename COctree<TPrimitiveContainer, TUserData>::TNodeId COctree<TPrimitiveContainer, TUserData >::NodeToId(Node* pNode, const TChildrenChunksPtrs& nodeChunkPtrs)
{
	for (size_t i = 0, count = nodeChunkPtrs.size(); i < count; ++i)
	{
		if (pNode >= nodeChunkPtrs[i] && (pNode < (nodeChunkPtrs[i] + OctreeUtils::eOctant_Count)))
		{
			return TNodeId((i* OctreeUtils::eOctant_Count) +(pNode - nodeChunkPtrs[i]));
		}
	}
	return s_invalidNodeId;
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeaves(TLeaves& outNodes, const T& primitive)
{
	if (m_root.IsLeaf())
	{
		CRY_ASSERT(!m_root.IsCollapsed());
		if (m_root.primitiveContainer.Intersects(m_root.domain, primitive, m_pUserData))
		{
			outNodes.push_back(SLeafInfo(&m_root, 0));
		}
	}
	else
	{
		FindIntersectingLeavesInChunk(outNodes, primitive, m_root.pChildren, 1);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeaves(TLeaves& outNodes1, TLeaves& outNodes2, TLeaves& outNodesBoth, const T& primitive1, const T& primitive2)
{
	if (m_root.IsLeaf())
	{
		CRY_ASSERT(!m_root.IsCollapsed());
		const bool intersects1 = m_root.primitiveContainer.Intersects(m_root.domain, primitive1, m_pUserData);
		const bool intersects2 = m_root.primitiveContainer.Intersects(m_root.domain, primitive2, m_pUserData);
		if (intersects1 || intersects2)
		{
			if (!intersects1)
			{
				outNodes2.emplace_back(&m_root, 0);
			}
			else if (!intersects2)
			{
				outNodes1.emplace_back(&m_root, 0);
			}
			else
			{
				outNodesBoth.emplace_back(&m_root, 0);
			}
		}
	}
	else
	{
		FindIntersectingLeavesInChunk(outNodes1, outNodes2, outNodesBoth, primitive1, primitive2, m_root.pChildren, 1);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeaves(TConstLeaves& outNodes, const T& primitive) const
{
	if (m_root.IsLeaf())
	{
		CRY_ASSERT(!m_root.IsCollapsed());
		if (m_root.primitiveContainer.Intersects(m_root.domain, primitive, m_pUserData))
		{
			outNodes.emplace_back(&m_root, 0);
		}
	}
	else
	{
		FindIntersectingLeavesInChunk(outNodes, primitive, m_root.pChildren, 1);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::PushInPlace(const T& primitive, bool bAllowSplitting)
{
	TLeaves leaves;
	leaves.reserve(OctreeUtils::eOctant_Count);
	FindIntersectingLeaves(leaves, primitive);
	for (const SLeafInfo& leafInfo : leaves)
	{
		leafInfo.pNode->primitiveContainer.Push(primitive);
		if (bAllowSplitting)
		{
			EvaluateSplitting(leafInfo);
		}
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::Remove(const T& primitive)
{
	TLeaves leaves;
	leaves.reserve(OctreeUtils::eOctant_Count);
	FindIntersectingLeaves(leaves, primitive);
	for (typename TLeaves::iterator it = leaves.begin(), itEnd = leaves.end(); it != itEnd; ++it)
	{
		Node* pNode = it->pNode;
		pNode->primitiveContainer.Remove(primitive);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::Move(const T& primitiveFrom, const T& primitiveTo, bool bAllowSplitting)
{
	TLeaves leavesFrom;
	TLeaves leavesTo;
	TLeaves leavesReplace;
	leavesFrom.reserve(OctreeUtils::eOctant_Count);
	leavesTo.reserve(OctreeUtils::eOctant_Count);
	leavesReplace.reserve(OctreeUtils::eOctant_Count);
	FindIntersectingLeaves(leavesFrom, leavesTo, leavesReplace, primitiveFrom, primitiveTo);

	for (typename TLeaves::iterator it = leavesFrom.begin(), itEnd = leavesFrom.end(); it != itEnd; ++it)
	{
		Node* pNode = it->pNode;
		pNode->primitiveContainer.Remove(primitiveFrom);
	}
	for (typename TLeaves::iterator it = leavesTo.begin(), itEnd = leavesTo.end(); it != itEnd; ++it)
	{
		Node* pNode = it->pNode;
		pNode->primitiveContainer.Push(primitiveTo);
	}
	for (typename TLeaves::iterator it = leavesReplace.begin(), itEnd = leavesReplace.end(); it != itEnd; ++it)
	{
		Node* pNode = it->pNode;
		pNode->primitiveContainer.Replace(primitiveFrom, primitiveTo);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::DefragmentNodes()
{
	TChildrenChunksPtrs childrenChunkPtrs;
	childrenChunkPtrs.reserve(256);

	m_root.FlattenChildrenChunksInto(childrenChunkPtrs);

	if (!childrenChunkPtrs.empty())
	{
		const size_t chunkCount = childrenChunkPtrs.size();
		const size_t chunkStride = OctreeUtils::eOctant_Count;
		const size_t nodeCount = chunkCount * chunkStride;
		const size_t bufferSize = nodeCount * sizeof(Node);

		Node* const pNewBufferStart = reinterpret_cast<Node*>(malloc(bufferSize));
		Node* const pNewBufferEnd = pNewBufferStart + nodeCount;
		Node* pCurrentChunk = pNewBufferStart;
		for (Node* pChunk : childrenChunkPtrs)
		{
			for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
			{
				Node& oldNode = pChunk[i];
				Node& newNode = pCurrentChunk[i];
				Node* pChildChunk = oldNode.pChildren ? &pNewBufferStart[NodeToId(oldNode.pChildren, childrenChunkPtrs)] : nullptr;
				const bool bIsValidChunkPtr = (!pChildChunk) || ((pChildChunk > pNewBufferStart) && (pChildChunk < pNewBufferEnd));
				new(&newNode)Node(std::move(oldNode), pChildChunk);
			}
			pCurrentChunk += chunkStride;
		}

		DestroyAllChildrenNodes();
		m_pBufferStart = pNewBufferStart;
		m_pBufferEnd = pNewBufferEnd;
		m_root.pChildren = pNewBufferStart;
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::PushInitialPrimitive(const T& primitive)
{
	CRY_ASSERT_MESSAGE(m_root.IsLeaf(), "Octree has already been split into leaves. PushInitialPrimitive() shouldn't be called anymore on it");
	if (m_root.IsLeaf())
	{
		m_root.primitiveContainer.Push(primitive);
	}
	else
	{
		PushInPlace(primitive);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::ForceEvaluateSplitting()
{
	TLeaves leaves;
	FindIntersectingLeaves(leaves, OctreeUtils::SIntersectAll());
	ForceEvaluateSplitting(leaves);
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::ForceEvaluateSplitting(const TLeaves& leaves)
{
	for (const SLeafInfo& leafInfo : leaves)
	{
		EvaluateSplitting(leafInfo);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeavesInChunk(TLeaves& outNodes, const T& primitive, Node* pNodeChunk, size_t depth)
{
	// it's likely cache_friendlier to iterate over the chunks before going deeper in the hierarchy
	Node* pChildrenChunks[OctreeUtils::eOctant_Count];
	int chunkCount = 0;

	// Go over chunk
	for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
	{
		Node* pChildNode = &pNodeChunk[i];
		if ((!pChildNode->IsCollapsed()) && pChildNode->primitiveContainer.Intersects(pChildNode->domain, primitive, m_pUserData))
		{
			if (pChildNode->IsLeaf())
			{
				outNodes.push_back(SLeafInfo(pChildNode, depth));
			}
			else
			{
				// collect deeper chunk pointers
				pChildrenChunks[chunkCount++] = pChildNode->pChildren;
			}
		}
	}

	// go deeper
	size_t nextDepth = depth + 1;
	for (int i = 0; i < chunkCount; ++i)
	{
		FindIntersectingLeavesInChunk(outNodes, primitive, pChildrenChunks[i], nextDepth);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeavesInChunk(TLeaves& outNodes1, TLeaves& outNodes2, TLeaves& outNodesBoth, const T& primitive1, const T& primitive2, Node* pNodeChunk, size_t depth)
{
	// it's likely cache_friendlier to iterate over the chunks before going deeper in the hierarchy
	Node* pChildrenChunksBoth[OctreeUtils::eOctant_Count];
	Node* pChildrenChunks1[OctreeUtils::eOctant_Count];
	Node* pChildrenChunks2[OctreeUtils::eOctant_Count];
	int chunkCountBoth = 0;
	int chunkCount1 = 0;
	int chunkCount2 = 0;

	// Go over chunk
	for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
	{
		Node* pChildNode = &pNodeChunk[i];
		if (!pChildNode->IsCollapsed())
		{
			const bool bPrimitive1Intersects = pChildNode->primitiveContainer.Intersects(pChildNode->domain, primitive1, m_pUserData);
			const bool bPrimitive2Intersects = pChildNode->primitiveContainer.Intersects(pChildNode->domain, primitive2, m_pUserData);
			if (bPrimitive1Intersects || bPrimitive2Intersects)
			{
				if (pChildNode->IsLeaf())
				{
					SLeafInfo newLeaf(pChildNode, depth);
					if (!bPrimitive1Intersects)
					{
						outNodes2.push_back(newLeaf);
					}
					else if (!bPrimitive2Intersects)
					{
						outNodes1.push_back(newLeaf);
					}
					else
					{
						outNodesBoth.push_back(newLeaf);
					}
				}
				else
				{
					// collect deeper chunk pointers
					if (!bPrimitive1Intersects)
					{
						pChildrenChunks2[chunkCount2++] = pChildNode->pChildren;
					}
					else if (!bPrimitive2Intersects)
					{
						pChildrenChunks1[chunkCount1++] = pChildNode->pChildren;
					}
					else
					{
						pChildrenChunksBoth[chunkCountBoth++] = pChildNode->pChildren;
					}
				}
			}
		}
	}

	// go deeper
	const size_t nextDepth = depth + 1;
	for (int i = 0; i < chunkCountBoth; ++i)
	{
		FindIntersectingLeavesInChunk(outNodes1, outNodes2, outNodesBoth, primitive1, primitive2, pChildrenChunksBoth[i], nextDepth);
	}
	for (int i = 0; i < chunkCount1; ++i)
	{
		FindIntersectingLeavesInChunk(outNodes1, primitive1, pChildrenChunks1[i], nextDepth);
	}
	for (int i = 0; i < chunkCount2; ++i)
	{
		FindIntersectingLeavesInChunk(outNodes2, primitive2, pChildrenChunks2[i], nextDepth);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename T>
void COctree<TPrimitiveContainer, TUserData >::FindIntersectingLeavesInChunk(TConstLeaves& outNodes, const T& primitive, Node* pNodeChunk, size_t depth) const
{
	// it's likely cache_friendlier to iterate over the chunks before going deeper in the hierarchy
	Node* pChildrenChunks[OctreeUtils::eOctant_Count];
	int chunkCount = 0;

	// Go over chunk
	for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
	{
		Node* pChildNode = &pNodeChunk[i];
		if ((!pChildNode->IsCollapsed()) && pChildNode->primitiveContainer.Intersects(pChildNode->domain, primitive, m_pUserData))
		{
			if (pChildNode->IsLeaf())
			{
				outNodes.push_back(SConstLeafInfo(pChildNode, depth));
			}
			else
			{
				// collect deeper chunk pointers
				pChildrenChunks[chunkCount++] = pChildNode->pChildren;
			}
		}
	}

	// go deeper
	size_t nextDepth = depth + 1;
	for (int i = 0; i < chunkCount; ++i)
	{
		FindIntersectingLeavesInChunk(outNodes, primitive, pChildrenChunks[i], nextDepth);
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename TFunc>
void COctree<TPrimitiveContainer, TUserData >::ForEachNode_RootToLeaves(const TFunc& func, Node* pNode, size_t depth)
{
	func(pNode, depth);
	if (!pNode->IsLeaf())
	{
		size_t newDepth = depth + 1;
		for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
		{
			ForEachNode_RootToLeaves(func, &pNode->pChildren[i], newDepth);
		}
	}
}

template<typename TPrimitiveContainer, typename TUserData>
template<typename TFunc>
void COctree<TPrimitiveContainer, TUserData >::ForEachNode_LeavesToRoot(const TFunc& func, Node* pNode, size_t depth)
{
	if (!pNode->IsLeaf())
	{
		size_t newDepth = depth + 1;
		for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
		{
			ForEachNode_LeavesToRoot(func, &pNode->pChildren[i], newDepth);
		}
	}
	func(pNode, depth);
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::EvaluateSplitting(const SLeafInfo leaf)
{
	Vec3 splittingReference;
	if (leaf.pNode->ShouldSplit(splittingReference, leaf.depth, m_pUserData))
	{
		leaf.pNode->Split(splittingReference, m_pUserData);
		int childrenDepth = leaf.depth + 1;
		for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
		{
			const SLeafInfo childLeaf(&leaf.pNode->pChildren[i], childrenDepth);
			EvaluateSplitting(childLeaf);
		}
	}
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::DestroyAllChildrenNodes()
{
	if (m_pBufferStart)
	{
		ForEachNode_LeavesToRoot([this](Node* pNode, size_t depth){ pNode->BufferFree(this->m_pBufferStart, this->m_pBufferEnd); });
		for (Node* pIt = m_pBufferStart; pIt != m_pBufferEnd; ++pIt)
		{
			pIt->~Node();
		}
		free(m_pBufferStart);
		m_pBufferStart = nullptr;
		m_pBufferEnd = nullptr;
	}
	m_root.DestroyChildren();
}

template<typename TPrimitiveContainer, typename TUserData>
void COctree<TPrimitiveContainer, TUserData >::SplitDomains(AABB (&outDomains)[OctreeUtils::eOctant_Count], const AABB& inDomain, const Vec3& reference)
{
	// start with the full domain
	for (int i = 0; i < OctreeUtils::eOctant_Count; ++i)
	{
		outDomains[i] = inDomain;
	}

	const bool bXMinOk = inDomain.min.x <= reference.x;
	const bool bXMaxOk = inDomain.max.x >= reference.x;
	const bool bYMinOk = inDomain.min.y <= reference.y;
	const bool bYMaxOk = inDomain.max.y >= reference.y;
	const bool bZMinOk = inDomain.min.z <= reference.z;
	const bool bZMaxOk = inDomain.max.z >= reference.z;

	const bool bXIn = bXMinOk && bXMaxOk;
	const bool bYIn = bYMinOk && bYMaxOk;
	const bool bZIn = bZMinOk && bZMaxOk;

	// restrain it for each octant
	if (bXIn && bYIn && bZIn)
	{
		// no collapsing required
		outDomains[OctreeUtils::eOctant_XmYmZm].max = reference;

		outDomains[OctreeUtils::eOctant_XpYmZm].min.x = reference.x;
		outDomains[OctreeUtils::eOctant_XpYmZm].max.y = reference.y;
		outDomains[OctreeUtils::eOctant_XpYmZm].max.z = reference.z;

		outDomains[OctreeUtils::eOctant_XmYpZm].max.x = reference.x;
		outDomains[OctreeUtils::eOctant_XmYpZm].min.y = reference.y;
		outDomains[OctreeUtils::eOctant_XmYpZm].max.z = reference.z;

		outDomains[OctreeUtils::eOctant_XpYpZm].min.x = reference.x;
		outDomains[OctreeUtils::eOctant_XpYpZm].min.y = reference.y;
		outDomains[OctreeUtils::eOctant_XpYpZm].max.z = reference.z;

		outDomains[OctreeUtils::eOctant_XmYmZp].max.x = reference.x;
		outDomains[OctreeUtils::eOctant_XmYmZp].max.y = reference.y;
		outDomains[OctreeUtils::eOctant_XmYmZp].min.z = reference.z;

		outDomains[OctreeUtils::eOctant_XpYmZp].min.x = reference.x;
		outDomains[OctreeUtils::eOctant_XpYmZp].max.y = reference.y;
		outDomains[OctreeUtils::eOctant_XpYmZp].min.z = reference.z;

		outDomains[OctreeUtils::eOctant_XmYpZp].max.x = reference.x;
		outDomains[OctreeUtils::eOctant_XmYpZp].min.y = reference.y;
		outDomains[OctreeUtils::eOctant_XmYpZp].min.z = reference.z;

		outDomains[OctreeUtils::eOctant_XpYpZp].min = reference;
	}
	else
	{
		const AABB& collapsed = OctreeUtils::MakeCollapsedAABB();

		// properly collapse the octants that would be empty
		if (bXMinOk && bYMinOk && bZMinOk)
		{
			outDomains[OctreeUtils::eOctant_XmYmZm].max.x = bXMaxOk ? reference.x : inDomain.max.x;
			outDomains[OctreeUtils::eOctant_XmYmZm].max.y = bYMaxOk ? reference.y : inDomain.max.y;
			outDomains[OctreeUtils::eOctant_XmYmZm].max.z = bZMaxOk ? reference.z : inDomain.max.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XmYmZm] = collapsed;
		}

		if (bXMaxOk && bYMinOk && bZMinOk)
		{
			outDomains[OctreeUtils::eOctant_XpYmZm].min.x = bXMinOk ? reference.x : inDomain.min.x;
			outDomains[OctreeUtils::eOctant_XpYmZm].max.y = bYMaxOk ? reference.y : inDomain.max.y;
			outDomains[OctreeUtils::eOctant_XpYmZm].max.z = bZMaxOk ? reference.z : inDomain.max.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XpYmZm] = collapsed;
		}

		if (bXMinOk && bYMaxOk && bZMinOk)
		{
			outDomains[OctreeUtils::eOctant_XmYpZm].max.x = bXMaxOk ? reference.x : inDomain.max.x;
			outDomains[OctreeUtils::eOctant_XmYpZm].min.y = bYMinOk ? reference.y : inDomain.min.y;
			outDomains[OctreeUtils::eOctant_XmYpZm].max.z = bZMaxOk ? reference.z : inDomain.max.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XmYpZm] = collapsed;
		}

		if (bXMaxOk && bYMaxOk && bZMinOk)
		{
			outDomains[OctreeUtils::eOctant_XpYpZm].min.x = bXMinOk ? reference.x : inDomain.min.x;
			outDomains[OctreeUtils::eOctant_XpYpZm].min.y = bYMinOk ? reference.y : inDomain.min.y;
			outDomains[OctreeUtils::eOctant_XpYpZm].max.z = bZMaxOk ? reference.z : inDomain.max.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XpYpZm] = collapsed;
		}

		if (bXMinOk && bYMinOk && bZMaxOk)
		{
			outDomains[OctreeUtils::eOctant_XmYmZp].max.x = bXMaxOk ? reference.x : inDomain.max.x;
			outDomains[OctreeUtils::eOctant_XmYmZp].max.y = bYMaxOk ? reference.y : inDomain.max.y;
			outDomains[OctreeUtils::eOctant_XmYmZp].min.z = bZMinOk ? reference.z : inDomain.min.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XmYmZp] = collapsed;
		}

		if (bXMaxOk && bYMinOk && bZMaxOk)
		{
			outDomains[OctreeUtils::eOctant_XpYmZp].min.x = bXMinOk ? reference.x : inDomain.min.x;
			outDomains[OctreeUtils::eOctant_XpYmZp].max.y = bYMaxOk ? reference.y : inDomain.max.y;
			outDomains[OctreeUtils::eOctant_XpYmZp].min.z = bZMinOk ? reference.z : inDomain.min.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XpYmZp] = collapsed;
		}

		if (bXMinOk && bYMaxOk && bZMaxOk)
		{
			outDomains[OctreeUtils::eOctant_XmYpZp].max.x = bXMaxOk ? reference.x : inDomain.max.x;
			outDomains[OctreeUtils::eOctant_XmYpZp].min.y = bYMinOk ? reference.y : inDomain.min.y;
			outDomains[OctreeUtils::eOctant_XmYpZp].min.z = bZMinOk ? reference.z : inDomain.min.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XmYpZp] = collapsed;
		}

		if (bXMaxOk && bYMaxOk && bZMaxOk)
		{
			outDomains[OctreeUtils::eOctant_XpYpZp].min.x = bXMinOk ? reference.x : inDomain.min.x;
			outDomains[OctreeUtils::eOctant_XpYpZp].min.y = bYMinOk ? reference.y : inDomain.min.y;
			outDomains[OctreeUtils::eOctant_XpYpZp].min.z = bZMinOk ? reference.z : inDomain.min.z;
		}
		else
		{
			outDomains[OctreeUtils::eOctant_XpYpZp] = collapsed;
		}
	}
}
