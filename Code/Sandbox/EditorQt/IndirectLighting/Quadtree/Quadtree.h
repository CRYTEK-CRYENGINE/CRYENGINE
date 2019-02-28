// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   quadtree definition
 */
#ifndef QUADTREE_H
#define QUADTREE_H

#pragma once

#include "QuadtreeHelper.h"
#include <CryCore/Platform/platform.h>
#include <CryMemory/CrySizer.h>

//!< namespace for all quadtree related types
namespace NQT
{
//!< default template arg declaration
template<class TLeafContent, uint32 TMaxCellElems = 1, class TPosType = float, class TIndexType = uint16, bool TUseRadius = false>
class CQuadTree;

//!< quad tree template class
//!<		template args:
//!<			TLeafContent: content storage type to organize in a quadtree, must implement < and == (a pointer or index is therefore fine)
//!<										also needs to be a unique identifier
//!<			TMaxCellElems: max elements per leaf cell
//!<			TPosType: position type of leaves (separate 2D vec for that)
//!<			TIndexType: type of indices within this tree, should be the less sized type it can get (preferably uint16)
//!<			TUseRadius: if true, then each leaf has a radius and can be present in multiple leaves
//!<				tree is not optimized for that in terms of memory, leaves are stored multiple times
//!<		quadtree has a center to place it relatively
//!<		each node stores just one index, if first bit is set to indicate if the index is a leaf
//!<		each node stores only first index to most left child since they are all arranged in consecutive order
//!<		quadtree is subdivided if in one cell are more than TMaxCellElems elems
//!<		each leaf node stores index to next leaf in the same cell to save memory in the nodes
//!<		index 0 is the index of the first node, so 0 also indicates that a node has neither leaves nor any more children nodes
//!<		position of each node quad center is computed on the fly
//!<		the indices to the children are denoted as follow:
//!<				0 -> quadrant with subtracted x and y (SS)
//!<				1 -> quadrant with added x and subtracted y (AS)
//!<				2 -> quadrant with subtracted x and added y (SA)
//!<				3 -> quadrant with added x and y (AA)
//!<		deleted elements are collected in a separate set and removed from any indexing, tree gets cleared via OptimizeTree
//!<		when doing any content retrieval don't forget that this is only valid as long as the leaves array does not change
//!<				which it does when inserting leaves or optimizing tree (if something has been deleted beforehand)
//!<		bear in mind that tree is prepared for oriented positions, but retrieval functions are not addressing it and it is not tested neither
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
class CQuadTree
{
public:
	template<class TLC, uint32 TMCE, class TPT, class TIT, bool TUR>
	friend class CQuadTree;

	typedef SVec2<TPosType>                             TVec2; //!< 2D vector according to pos type
	typedef SVec2<typename SCenterType<TPosType>::Type> TVec2F;
	typedef std::pair<TLeafContent*, TVec2>             TContentRetrievalType; //!<type each content is retrieved as

	//!< function pointer taking a TLeafContent and buffer as input writing to the buffer
	typedef void (*         SerializeContentFunction)(const TLeafContent& crContent, uint8* pBuffer);
	//!< function pointer returning size of each serialized content element
	typedef const size_t (* SerializeContentSizeFunction)();
	//!< function pointer taking a buffer as input to restore the content from a buffer, returns true if successful
	typedef const bool (*   DeSerializeContentFunction)(TLeafContent& rContent, const uint8* cpBuffer);

	enum EConsistencyRes
	{
		CR_CONSISTENT = 0,
		CR_FIXED_INCONSISTENCY,
		CR_INCONSISTENT
	};

private:
	typedef uint16                                               TRadiusType; //!< typedef for 2 byte radius type
	//!< typedef to vector type for center to use float always but in the case of position double's
	typedef SVec2<double>                                        TVec2D;//double type needed for precision in pos determinations
	typedef SChildIndices<TUseRadius, TMaxCellElems, TIndexType> TChildIndices;   //!< child indices type according to template specification

	//!< leaf type
	template<class TLeafContentQuad, class TIndexTypeQuad>
	struct SQuadLeaf
	{
	public:
		//!< index used for indicating that a leaf has more leaves in the same child cell, highest bit
		enum {HAS_MORE_LEAVES_INDEX = (2 << (sizeof(TIndexType) * 8 - 2))};
		enum {INVALID_LEAF_INDEX = HAS_MORE_LEAVES_INDEX};

		SQuadLeaf();                                                                                                                                                      //!< default constructor necessary for being used in vectors
		SQuadLeaf(const TLeafContentQuad& crCont, const TVec2& crPos, const TRadiusType);                                                                                 //!< constructor taking pos and contents
		const TIndexTypeQuad                        GetNextCellLeaf() const;                                                                                              //!< retrieves next leaf within this cell
		static const size_t                         GetSerializationSize(const size_t cContentSerializationSize);                                                         //!< retrieves size for serialization
		void                                        Serialize(SerializeContentFunction pSerializeFnct, const size_t cContentSerializationSize, uint8*& pStreamPos) const; //!< serializes the leaf and moves the current stream pointer
		void                                        SetNextCellLeaf(const TIndexTypeQuad cNextIndex, const bool cHasNextLeaf = true);
		const TLeafContentQuad&                     GetContent() const;                         //!< retrieves reference to content
		const TLeafContentQuad*                     GetContentPointer() const;                  //!< retrieves address to content
		void                                        SetContent(const TLeafContentQuad& crCont); //!< sets content
		const TVec2&                                GetPos() const;//!< retrieves the pos
		void                                        SetPos(const TVec2& crPos);//!< sets the pos
		SQuadLeaf<TLeafContentQuad, TIndexTypeQuad> Clone() const;  //!< clones this leaf

	protected:                         //we need this to be private since it is using a flag together with an index, direct access will result in wrong results
		TLeafContentQuad  cont;          //!< leaf content
		TVec2             pos;           //!< 2D position
		TIndexTypeQuad    nextLeafIndex; //!< index to next leaf in the same child node(to save memory in the nodes itself), same bit used for indication
		//!< align to a 4 byte boundary (to allow content type to operate faster)
		uint8             pad[sizeof(uint32) - ((sizeof(TVec2) + sizeof(TLeafContentQuad) + sizeof(TIndexTypeQuad)) % sizeof(uint32))];

		const TRadiusType GetRadius() const;    //!< retrieves radius, needed for common leaf usage

		template<class TLeafContentF, uint32 TMaxCellElemsF, class TPosTypeF, class TIndexTypeF, bool TUseRadiusF>
		friend class CQuadTree;
	};

	//!< leaf type with radius
	template<class TLeafContentQuad, class TIndexTypeQuad>
	struct SQuadLeafRadius : public SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>
	{
	public:
		SQuadLeafRadius();                                                                                                                                                      //!< default constructor necessary for being used in vectors
		SQuadLeafRadius(const TLeafContentQuad& crCont, const TVec2& crPos, const TRadiusType cRadius = 0);                                                                     //!< constructor taking radius, pos and contents
		static const size_t                               GetSerializationSize(const size_t cContentSerializationSize);                                                         //!< retrieves size for serialization
		void                                              Serialize(SerializeContentFunction pSerializeFnct, const size_t cContentSerializationSize, uint8*& pStreamPos) const; //!< serializes the leaf and moves the current stream pointer
		SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad> Clone() const;                                                                                                        //!< clones this leaf

	private:
		TRadiusType radius;   //!< leaf radius

		const TRadiusType GetRadius() const;    //!< retrieves radius, needed for common leaf usage

		template<class TLeafContentF, uint32 TMaxCellElemsF, class TPosTypeF, class TIndexTypeF, bool TUseRadiusF>
		friend class CQuadTree;
	};

	//!< selector template type to select leaf type according to presence of radius or not
	template<class TLeafContentQuad, class TIndexTypeQuad, bool TSelector>
	struct SLeafSelector;

	template<class TLeafContentQuad, class TIndexTypeQuad>
	struct SLeafSelector<TLeafContentQuad, TIndexTypeQuad, true>
	{
		typedef SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad> LeafType;
	};

	template<class TLeafContentQuad, class TIndexTypeQuad>
	struct SLeafSelector<TLeafContentQuad, TIndexTypeQuad, false>
	{
		typedef SQuadLeaf<TLeafContentQuad, TIndexTypeQuad> LeafType;
	};

	typedef typename SLeafSelector<TLeafContent, TIndexType, TUseRadius>::LeafType TLeaf;
	typedef std::vector<TLeaf>                                                     TLeafVect;

	//!< helper type for sorting
	struct SQuadLeafSort
	{
		TLeaf* pLeaf;     //!< leaf pointer
		float  distSq;    //!< distance to it

		SQuadLeafSort(const TLeaf*& crpLeaf, const float cDistSq);      //!< constructor for both elems
		const bool operator<(const SQuadLeafSort& crLeafSort)  const;   //!< sort operator this is all made for
	};

	//!< another helper type for sorting
	struct SContentSort
	{
		typedef TContentRetrievalType TRetrievalType;

		TRetrievalType contRetrieval;     //!< content retrieval
		float          distSq;            //!< distance to it

		SContentSort(TLeafContent* cpCont, const TVec2& crPos, const float cDistSq); //constructor for both elems
		const bool operator<(const SContentSort& crLeafSort) const;                  //!< sort operator this is all made for
	};

	typedef CSearchContainer<SContentSort, TIndexType> TSearchContainer;    //!< container type for gathering all search results

	//!< node type
	struct SQuadNode
	{
		union
		{
			TIndexType childNodeIndex;    //!< index to child 0 (index > 0, 0 is root node)
			TIndexType leafIndex;         //!< index to first leaf, 0 is possible, never set IS_LEAF_INDEX without corresponding leaf index
		};

		//!< index used as value for indicating that a certain node is no leaf node (first bit of TIndexType), highest bit
		enum {IS_LEAF_INDEX = (2 << (sizeof(TIndexType) * 8 - 2))};

		SQuadNode()                                     //!< constructor to initialize node and leaf index to 0
			: childNodeIndex(0){}
		const bool HasLeafNodes() const;                //!< returns true if it has leaf nodes, true otherwise
		const bool HasChildNodes() const                //!< returns true if it has child nodes, false otherwise
		{
			return ((leafIndex & ~IS_LEAF_INDEX) != 0 && (leafIndex & IS_LEAF_INDEX) == 0);  //return true if index is not 0 and leaf bit is not set
		}
		void             SetIsLeaf(const bool cIsLeaf); //!< sets/removes the leaf bit
		const TIndexType GetLeafIndex() const           //!< retrieves leaf index, takes care of IS_LEAF_INDEX
		{
			assert(HasLeafNodes());
			return (leafIndex & ~IS_LEAF_INDEX);
		}
		void SetLeafIndex(const TIndexType cIndex);     //!< sets leaf index, takes care of IS_LEAF_INDEX
		void SwapEndianess();
	};

	typedef std::vector<SQuadNode> TNodeVec;

	//!< contains the full data for each node (center pos, side length, node index)
	//!< retrieved during node traversal
	struct SQuadNodeData
	{
		TIndexType nodeIndex;     //!< node index
		TVec2F     centerPos;     //!< center position of node, float to keep the frac data(otherwise it would alias and result in f.i.: 5->3->2 whereas it should be: 5->3->1
		float      sideLength;    //!< side length of node

		SQuadNodeData();    //!< default constructor
		//!< constructor with all elements initializing
		SQuadNodeData(const TIndexType cNodeIndex, const TVec2F& crCenterPos, const float cSideLength);
	};

	//!< header for serialization
	//!< contains quadtree data plus number of nodes and leaves
	struct SHeader
	{
		uint32     headerSize;          //!< size of header in bytes
		uint32     fullStreamSize;      //!< size of entire stream including header in bytes
		S2DMatrix  orientMatrix;        //!< old orientation matrix, not used anymore
		float      minSideLength;       //!< minimum distance of two cell centers in the lowest subdivision level
		TIndexType leafCount;           //!< leaf count
		TIndexType uniqueLeaveCount;    //!< unique leaves count
		TIndexType nodeCount;           //!< node count
		TVec2F     centerPos;           //!< center pos
		TPosType   sideLength;          //!< initial side length
		uint16     maxSubDivLevels;     //!< max subdivision levels

		SHeader();              //!< constructor sets header size
		void SwapEndianess();   //!< hand coded swap endian function since automatic one does not work
	};

	//!< return value for errors when a node index was expected, exception handling not enabled for quadtree
	static const TIndexType INVALID_CHILD_NODE = 0;

public:
#if defined(_DEBUG)
	//!< debug statistics
	struct SClosestContentStat
	{
		uint32 leafArrayAccesses;   //!< number of m_Leaves accesses
		uint32 nodeArrayAccesses;   //!< number of m_Nodes accesses

		SClosestContentStat()     //!< default constructor clearing all to 0
			: leafArrayAccesses(0), nodeArrayAccesses(0){}
		void Reset()                //!< resets all to 0
		{
			leafArrayAccesses = nodeArrayAccesses = 0;
		}
	};
#endif

	//!< data retrieved for interpolation retrieval
	struct SInterpolRetrievalType
	{
		const TLeafContent* cpCont;   //!< leaf content	pointer
		TVec2F              pos;      //!< pos of leaf
		float               distSq;   //!< square distance to a certain pos
	};

	static const uint32 scInitalSliceGranularity = 16;          //!< initial slice granularity (4 per quadrant)
	//!< typedef for scInitalSliceGranularity elements (circle with scInitalSliceGranularity equal slices)
	typedef SInterpolRetrievalType TInitialInterpolRetrievalType[scInitalSliceGranularity];

	//!< data needed for interpolation retrieval plus quadrant index
	struct SInterpolRetrievalTypeHelper : public SInterpolRetrievalType
	{
		uint8 sliceIndex;       //!< slice it belongs to
		uint8 nextSliceIndex;   //!< closest next slice index it belongs to
	};

	//!< hold all data required for interpolation retrieval for one single neighbor node
	struct SInterpolNodeData
	{
		SInterpolRetrievalTypeHelper data[TMaxCellElems];
		uint8                        count;
	};

	//***************************** public functions ********************************************************************

	//!< constructor taking center position and initial subdivision levels and max number of these
	CQuadTree
	(
	  const TPosType cSideExtension,
	  const TVec2& crCenterPosition = TVec2(0, 0),
	  const uint16 cMaxSubDivLevels = 9,
	  const uint32 cReservedNodeMem = 0,
	  const uint32 cReservedLeafMem = 0
	);
	//!< default constructor if to be initialized later
	CQuadTree(const uint32 cReservedNodeMem = 0, const uint32 cReservedLeafMem = 0);
	//!< destructor
	~CQuadTree();
	//!< initializes quadtree
	void Init
	(
	  const TPosType cSideExtension,
	  const uint16 cMaxSubDivLevels = 7,
	  const TVec2& crCenterPosition = TVec2(0, 0)
	);
	//!< resets tree
	void             Reset();
	//!< returns max number of leaves
	const TIndexType GetLeafCapacity() const;
	//!< optimizes memory consumption, should be only called when no more elems are to be inserted
	//!< reorganizes tree if any node has been removed
	void             OptimizeTree();
	//!< returns memory consumption in bytes
	const size_t     ConsumedMemory() const;
	//!< returns the current number of stored leaves, only unique ones are count (leaves stored multiple times in case of TUseRadius)
	const TIndexType LeafCount() const;
	//!< returns current number of nodes
	const TIndexType NodeCount() const;
	//!< converts tree into a quadtree with different index and/or TMaxCellElems type
	template<uint32 TNewMaxCellElems, class TNewIndexType>
	CQuadTree(const CQuadTree<TLeafContent, TNewMaxCellElems, TPosType, TNewIndexType, TUseRadius>& crConvertFrom);
	//!< retrieves the contents from all leaves
	void GetAllContents(std::vector<std::pair<TVec2, const TLeafContent*>>& rContents) const;
	//!< retrieves the content of a leaf if one exists at the exact position crPos, sets pointer to rpCont, set to NULL if no leaf was at this pos
	void GetLeafByPos(const TLeafContent*& rpCont, const TVec2& crPos) const;
	//!< retrieves the closest content to crPos with its position
	//!< rContents is sorted from closest element to most far away element
	//!< constraint can be a node count or radius: use float or TIndexType
	//!< if cSort is false, the results are not sorted, in case of radius search, this is very fast
	//!<		because the elems are stored directly into the result vector rather than stored temporarily
	//!< calls GetClosestContentsTemplatized to avoid any duplication of code writing, resolves with SClosestContentsChecker
	template<class TNodeConstraint>
	void GetClosestContents
	(
	  const TVec2& crPos,
	  const TNodeConstraint cNodeConstraint,
	  std::vector<TContentRetrievalType>& rContents,
	  const bool cSort = true
	) const;
	//!< retrieves all leaves intersected by ray from outside the quadtree, only valid if radius is supported, sorts by distance
	void GetRayIntersectionContentsOutside
	(
	  std::vector<const TLeafContent*>& rContents,
	  const TVec2F& crPos,
	  const TVec2F& crDir,
	  const float cRayLength,
	  const uint32 cMaxCount
	) const;
	//!< retrieves all leaves intersected by ray from inside the quadtree, only valid if radius is supported, sorts by distance
	void GetRayIntersectionContents
	(
	  std::vector<const TLeafContent*>& rContents,
	  const TVec2F& crPos,
	  const TVec2F& crDir,
	  const float cRayLength,
	  const uint32 cMaxCount
	) const;
	//!< retrieves the 4 leaves for interpolation with its content and pos
	//!< it can be cached and therefore only needs to be refetched if necessary (due to pos change, quadtree should not change however)
	//!< returns true if all 4 quadrants have been filled
	//!< we need a special boolean template here since we need the full speed
	const TIndexType GetInterpolationNodes(TInitialInterpolRetrievalType pLeaves, const TVec2F& crPos, const float cMaxDistSq) const;
	//!< inserts a new leaf, subdivides if necessary, returns insertion result
	//!< if radius is used, leaf is inserted multiple times, success is returned when it has been inserted at least once
	const EInsertionResult InsertLeaf(const TVec2& crPos, const TLeafContent& crContent, const float cRadius = 0.f, const bool cVerify = true);
	//!< removes a leaf, content and crPos are tested for equality
	const bool             RemoveLeaf(const TLeafContent& crCont, const TVec2& crPos);
	//!< serializes the tree, calls optimize and then serializes all nodes,
	//!< function pointer used for serialization of contents to be able to restore it properly
	//!< stores the size for serialized elements to be able to restore it accordingly
	void SerializeTree(std::vector<uint8>& rStream, SerializeContentFunction pSerializeFnct, SerializeContentSizeFunction pSizeFnct, const bool cOptimizeBefore = true);
	//!< constructs a tree from a serialized stream
	//!< function pointer used to turn the serialized TLeafContent back into a TLeafContent
	//!< returns true if construction was successful
	//!< cStreamSizeToDecode tells the stream size to construct quadtree from, must be at least as large as the quadtree
	//!< newStreamPos is set to the new position after the quadtree if returned successfully
	const bool ConstructTreeFromStream(const uint8 * cpStream, const uint32 cStreamSizeToDecode, DeSerializeContentFunction, SerializeContentSizeFunction, uint8 * &newStreamPos);
	//!< returns max number of subdivision levels
	const uint16 GetMaxSubdivLevels() const;
	//!< draws the quadtree with different colour for each subdivision level, starts with red colour and ends up with green for the lowest levels
	//!< stores in current directory, 32 bit and alpha channel is used for leaf count in this cell
	void                      Draw(const char* cpFileName = "QuadTree_Debug.tga");
#if defined(_DEBUG)
	const SClosestContentStat GetDebugStats() const   //!< retrieves debug stats from last operation
	{
		assert(m_IsInitialized == true);
		return m_DebugStats;
	}
#endif
	//!< memory info for cry sizer
	void GetMemoryUsage(ICrySizer* pSizer);

	//!< runs a consistency check on nodes and leaves (array size bound checks)
	const EConsistencyRes IsConsistent();

	//*********************************************************************************************************************

private:
	TLeafVect               m_Leaves;            //!< vector of leaves
	TNodeVec                m_Nodes;             //!< vector of nodes
	TVec2F                  m_Center;            //!< 2D center position
	TPosType                m_SideLength;        //!< extension of one quad side
	std::set<TIndexType>    m_DeletedLeaves;     //!< set of indices to currently leaves marked as deleted
	uint16                  m_MaxSubDivLevels;   //!< max number of subdivision levels, level 0 is just the root node, set in constructor
	float                   m_MinSideLength;     //!< minimum distance of two cell centers in the lowest subdivision level
	TIndexType              m_UniqueLeaveCount;  //!< counts number of actual stored leaves which are unique (stored multiple times in case of radius)
	float                   m_MaxRadius;         //!< max radius the compression scheme can cope with
	float                   m_CompressionFactor; //!< factor to get from the TRadiusType to float

	static const uint32     scLookupCount = 4 * scInitalSliceGranularity;
	std::pair<uint8, uint8> m_SliceLookupTable[scLookupCount + 1][2 /*sign of dirx*/];//+1 for safety and inaccuracy(no need to check then)

#if defined(_DEBUG)
	mutable SClosestContentStat m_DebugStats;    //!< debug statistics to keep track of the number of access operations for last op
	bool                        m_IsInitialized; //!< keeps track if is was initialized
#endif

	//!< content check to only enable it for certain implemented types: float(radius), TIndexType(max node count)
	template<typename T, typename U>
	struct SClosestContentsChecker
	{
		typedef T Type;
	};

	template<typename U>
	struct SClosestContentsChecker<float, U>
	{
		typedef float Type;
	};

	template<typename U>
	struct SClosestContentsChecker<double, U>
	{
		typedef float Type;
	};

	//also enable for uint32 be used as index type since constants have this default type
	template<typename U>
	struct SClosestContentsChecker<int32, U>
	{
		typedef TIndexType Type;
	};

	typedef std::pair<SQuadNodeData, TEChildIndex>        TTravRecType;       //!< typedef to type of traversal record
	typedef CTravVec<TTravRecType>                        TTravRecVecType;    //!< typedef to type of traversal record vector
	typedef CTravVec<SQuadNodeData>                       TNeighborCellsType; //!< typedef to type of neighbor cell traversal
	typedef std::vector<std::pair<SQuadNodeData, uint16>> TNodeDataVec;       //!< typedef for node data retrieval with radius

	//!< subdivides current node one level, retrieves index to first child(SS)
	const TIndexType Subdivide(const TVec2F& crCenterPos, SQuadNode& rQuadNodeToSubdiv);
	//!< subdivides current node cLevels levels, returns true if successful
	const bool       Subdivide(const float cQuadSideLength, const TVec2F& crCenterPos, SQuadNode& rQuadNodeToSubdiv, const uint32 cLevels);
	void             InsertRootNode(); //!< inserts first node into an empty quadtree
	//!< retrieves the leaf index into m_Leaves from content and pos equality test, also stores the rParentNodeIndex, helper function
	const TIndexType GetLeafArrayIndex
	(
	  const TLeafContent& crCont,
	  const TVec2& crPos,
	  TIndexType& rParentNodeIndex,
	  TTravRecVecType& rTraversedNodes
	) const;

	//!< walks recursively through the tree and retrieves the most subdivided cell (all data) node the pos is in, helper function
	//!< cStartNodeIndex is the node to check the cell index for, use 0 to start at root
	//!< rTraversedNodes is push_back'd with all nodes traversed through starting with cStartNodeIndex and
	//!< stopping at the parent of rNodeData.nodeIndex (so excluding the last node), TEChildIndex tells through which child the traversal went
	//!< also sets current subdivision depth into rSubdivisonLevel, root node equals 0
	void RetrieveDeepestNode
	(
	  const TVec2F& crPos,
	  const float cSideLength,
	  const TVec2F& crCenter,
	  SQuadNodeData& rNodeData,
	  const TIndexType cStartNodeIndex,
	  TTravRecVecType& rTraversedNodes,
	  uint16& rSubdivisonLevel
	) const;
	//!< returns number of leaves in that cell, 0 if it is not a leaf node, rLastCellLeaf is set to the last leaf in that cell if count > 0
	const TIndexType GetCellLeafCount(const SQuadNode& crNode, std::vector<TIndexType>& rCellLeaves) const;
	//!< retrieves the cNodeCount closest contents with its position to crPos for one particular leaf
	void             GetClosestLeafContents
	(
	  const TLeaf& crLeaf,
	  const TVec2& crPos,
	  const float cSideLength,
	  const TVec2F& crCenterPos,
	  TSearchContainer& rSortedContents
	) const;
	//!< helper functions for inserting new leaf contents according to their distance
	void AddLeafContent(const SContentSort& crContToAdd, TSearchContainer& rSortedContents) const;
	//!< helper function for traversing from a node through all nodes	touching all cells within a certain min dist and adding closest leaf contents
	//!< crStartNodeRec takes care where to start to traverse by not touching the child again which was traversed already
	void GetClosestNodeContents
	(
	  const TTravRecType& crStartNodeRec,
	  const TVec2& crPos,
	  TSearchContainer& rSortedContents
	) const;
	//!< helper function for traversing from a node through all nodes	touching all cells within a certain min dist and adding closest leaf contents
	//!< called from function above but going into leaves as well as touching all cells since no traversal was made there already
	void GetClosestContentsDistRecurse
	(
	  const TIndexType cNodeIndex,
	  const TVec2F& crCenterPos,
	  const float cSideLength,
	  const TVec2& crPos,
	  TSearchContainer& rSortedContents
	) const;
	//!< verifies whether a certain position can be inserted or not (to avoid deep subdivision or even invalid operations)
	//!< returns true if pos is within limits, false if it cannot get inserted
	const EInsertionResult VerifyLeafPos
	(
	  const std::vector<TIndexType>& crCellLeaves,
	  const TVec2& crPos,
	  const SQuadNodeData& crNodeData,
	  const uint16 cCurrentSubdivLevel
	) const;
	//!< returns next grid point at lowest subdiv level, positions are relative and 0,0 is the upper left corner
	template<class TCompType>
	const SVec2<TCompType> ComputeNextGridPoint00(const SVec2<TCompType>& crPos) const;
	//!< sets the max number of subdivision levels
	void                   SetMaxSubdivLevels(const uint16 cLevels);
	//!< return max number of elems, index type determines also the max number of nodes, if TMaxCellElems==1 -> nodes count is restricting
	const TIndexType       MaxElems() const;
	//!< retrieves the leaf by index
	void                   RetrieveLeafByIndex(const TIndexType cLeafIndex, TLeaf& rLeaf) const;
	//!< retrieves the closest content to crPos with its position
	//!< rContents is sorted from closest element to most far away element
	//!< constraint can be a node count or radius: use float or TIndexType
	void GetClosestContentsContainer
	(
	  const TVec2& crPos,
	  TSearchContainer& rSortedContents
	) const;
	//following functionality is only needed when radius is present
	//!< queries whether a certain pos in within a cell or not
	const bool IsPosInCell(const TVec2& crPos, const float cSideLength, const TVec2F& crCenter) const;
	//!< queries whether a certain pos with a radius intersects a cell or not
	const bool IsPosInCell(const TVec2& crPos, const float cSideLength, const TVec2F& crCenter, const float cRadius) const;
	//!< recursively gathers all leaves a ray intersects along its path
	//!< returns true if rCurCount >= cMaxCount (break condition for recursion)
	const bool GatherAllIntersectingNodes
	(
	  std::vector<const TLeafContent*>& rContents,
	  const SQuadNodeData& crNodeData,
	  const TVec2F& crRayPos,
	  const TVec2F& crRayDir,
	  const float cRayLength,
	  const uint32 cMaxCount,
	  uint32& rCurCount
	) const;
	//!< recursively gathers all nodes a pos with a radius share
	void GatherAllRadiusSharingNodes
	(
	  TNodeDataVec& rNodeVec,
	  const SQuadNodeData& crNodeData,
	  const TVec2& crPos,
	  const float cRadius,
	  const uint16 cSubDivLevel = 0
	) const;
	//!< helper function for InsertLeafWithSubdivision, recurses for one node and inserts all leaf really going there
	//!< returns insertion success if leaf has been inserted
	const EPosInsertionResult InsertLeafWithSubdivisionRecurse
	(
	  const TVec2& crPos,              //new leaf pos
	  const SQuadNodeData& crNodedata, //current node properties
	  const std::vector<TIndexType>& crCellLeaves,
	  uint16 subDivLevel,
	  const TIndexType cNewLeafIndex,
	  const float cRadius,
	  TChildIndices aChildIndices[TMaxCellElems + 1]
	);
	//!< helper function for InsertLeaf, determines how the leaves distribute in current node
	//!< sets it for each cell to true if all leaves go to this cell (which makes it necessary to subdivide further)
	void GatherChildInfo
	(
	  const TVec2& crPos,
	  const TVec2F& crNodeCenter,
	  TChildIndices aChildIndices[TMaxCellElems + 1],
	  const std::vector<TIndexType>& crCellLeaves,
	  const float cRadius,
	  const TVec2F cChildCenters[4],
	  const float cChildNodeSideLength,
	  SArraySelector<TUseRadius, bool>& rSubdivisionInfo,
	  TEChildIndexExt& rPosChildIndex   //index of cell where new node goes
	);
	//!< helper function for InsertLeaf, propagates the leaves into the recently subdivided 4 cells, called when no radius is used and
	//!< not all leaves go into the same cell
	void PropagateLeavesToNewNode
	(
	  TChildIndices aChildIndices[TMaxCellElems + 1],
	  const TIndexType cFirstChildIndex,
	  const std::vector<TIndexType>& crCellLeaves,
	  const TIndexType cNewLeafIndex,
	  const SArraySelector<TUseRadius, bool>& crNodeInsertionMask
	);
	//!< helper function for InsertLeaf, fallback for insertion of leaves where max subdivision level is reached but also TMaxCellElems
	//!< returns true if new leaf has been inserted
	const EPosInsertionResult PropagateLeavesToNewNodeFallback
	(
	  TChildIndices aChildIndices[TMaxCellElems + 1],
	  const TVec2& crPos,
	  const SQuadNodeData& crNodedata,
	  const std::vector<TIndexType>& crCellLeaves,
	  const TIndexType cNewLeafIndex
	);
	//!< initializes and resets static insertion vector to avoid too many reallocations
	static void       ResetChildIndicesVec(TChildIndices aChildIndices[TMaxCellElems + 1]);
	//!< access functions for leaves and nodes (to not repeat assertion all over the place)
	const SQuadNode&  GetNode(const TIndexType cIndex) const;
	SQuadNode&        GetNode(const TIndexType cIndex);
	const TLeaf&      GetLeaf(const TIndexType cIndex) const;
	TLeaf&            GetLeaf(const TIndexType cIndex);
	//!< removes a leaf from a node, unlinks all
	void              RemoveLeafFromNode(SQuadNode& rNode, const TIndexType cLeafIndex);
	const TIndexType  RemoveLeafFromNode(SQuadNode& rParentNode, const TLeafContent& crCont, const TVec2& crPos); //returns index of removed leaf
	//!< radius functions to be able to use just 2 bytes for it
	const float       GetRadiusFromLeaf(const TLeaf& crLeaf) const;//!< retrieves the radius for a certain leaf
	const float       GetRadiusFromLeaf(const TIndexType cLeafIndex) const;//!< retrieves the radius for a certain leaf
	const float       GetRadius(const TRadiusType cComprRadius) const;//!< retrieves the radius
	const TRadiusType CompressRadius(const float cRadius) const;  //!< retrieves the compressed radius
	void              InitializeRadiusCompression();//!< initializes the radius compression
	//!< controls leaf insertion
	const bool        VerifyInsertionLeafState(TIndexType& rLeafIndex, TChildIndices& rChildIndex);
	//!< propagates cell leaves of gathered neighbour cells to the per quadrant closest leaf
	void              PropagateNeighbourLeaves
	(
	  const TNeighborCellsType& crCells,
	  SInterpolRetrievalType* pLeaves,
	  const TVec2F& crPos,
	  const float cMaxDistSq
	) const;
	//!< fill node data for interpolation
	void FillInterpolationNodeData(const SQuadNode& crNode, const TVec2F& crPos, SInterpolNodeData& rNodeData, const float cMaxDistSq) const;
	//!< helper function for FillInterpolationNodeData to start recursion from current
	void GetRadiusNodeCellsFromTraversal
	(
	  TNeighborCellsType& rCells,
	  const TTravRecType& crStartNodeRec,
	  const TVec2F& crPos,
	  const float cRadiusSq
	) const;
	//!< helper function for GetRadiusNodeCellsFromTraversal to recurse from current node down the hierarchy finding all nodes intersecting a certain radius
	//!< if a cell has leaves and intersects the circle, the node is added to  rCells
	void GetRadiusNodeCellsRecurse
	(
	  TNeighborCellsType& rCells,
	  const SQuadNode& crNode,
	  const TVec2F& crCenterPos,
	  const float cSideLength,
	  const TVec2F& crPos,
	  const float cRadiusSq
	) const;
	//!< makes sure that there exist no node without any leaves or children
	void        EnsureTreeIntegrity(const TTravRecVecType crTraversedNodes);
	//!< returns the slice index for a given center pos and leaf pos (4 quadrant and each halved as well)
	const uint8 GetSliceIndex(const float cDirX, const float cDirY, const float cDistSq, uint8& rNextIndex) const;
	//!< adds all leaves from a leaf node if intersecting with the ray
	//!< returns true if enough leaves have been added (cCurCount >= cMaxCount)
	const bool AddIntersectingLeaves
	(
	  std::vector<const TLeafContent*>& rContents,
	  const SQuadNode& crNode,
	  const TVec2F& crPos,
	  const TVec2F& crDir,
	  const float cRayLength,
	  const uint32 cMaxCount,
	  uint32& rCurCount
	) const;
	//!< recursively gathers all leaves a ray intersects starting at parent node crStartNodeRec(without previously tested child node)
	void GetNodeRayIntersectionContents
	(
	  const TTravRecType& crStartNodeRec,
	  std::vector<const TLeafContent*>& rContents,
	  const TVec2F& crPos,
	  const TVec2F& crDir,
	  const float cRayLength,
	  const uint32 cMaxCount,
	  uint32& rCurCount
	) const;
	//!< filters the best and important nodes for interpolation from crInitialNodes into rFinalNodes
	const TIndexType OptimizeInterpolationNodes
	(
	  TInitialInterpolRetrievalType pFinalNodes,
	  const TInitialInterpolRetrievalType cpInitialNodes,
	  const float cMaxDistSq
	) const;

	//!< create lookup table on startup
	void BuildSliceLookupTable();

	//functions from former orientation policy
	template<class TCompType, class TCenterCompType>
	const TEChildIndex GetQuadrantIndex(const SVec2<TCenterCompType>& crCenter, const SVec2<TCompType>& crPos) const;
	template<class TCompType>
	void               GetSubdivisionCenters(const float cSideLength, const SVec2<TCompType>& crCenter, SVec2<TCompType> childCenters[4]) const;
	template<class TCompType, class TCenterCompType>
	const float        GetMinDistToQuad(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCompType>& crPos) const;
	template<class TCompType, class TCenterCompType>
	const float        GetMinDistToQuadSq(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCompType>& crPos) const;
	template<class TCenterCompType>
	const float        GetMinDistToQuadSq(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCenterCompType>& crPos) const;
};
};//NQT

#include "QuadtreeUtilities.inl"
#include "Quadtree.inl"
#include "QuadtreeNodeLeaf.inl"
#endif//QUADTREE_H

