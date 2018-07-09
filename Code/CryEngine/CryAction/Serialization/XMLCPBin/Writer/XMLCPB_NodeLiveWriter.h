// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_NODELIVEWRITER_H
	#define XMLCPB_NODELIVEWRITER_H

	#include "XMLCPB_AttrWriter.h"
	#include "../XMLCPB_Common.h"
	#include "XMLCPB_BufferWriter.h"

namespace XMLCPB {

class CWriter;
class CNodeLiveWriter;

//////////////////////////////////////////////////////////////////////////
// from outside, CNodeLiveWriterRef is the class that is always used to access a node. CNodeLiveWriter is never used directly from outside.
// The main reason for this is because the live nodes are reusable: It could be possible to keep a "valid" pointer to a livenode even after the original node has been discarded and another one has taken its place.
// That situation is avoided with the use of this wrapper reference and the safecheckID. Every time a livenode is reused, its safecheckID changes. If the safecheckID of
//   the CNodeLiveWriterRef does not match the one in the LiveNode, then it can not be used.

// Note: this object is slightly different than the CNodeLiveReaderRef used in the reader. the reader one does not need the safecheckID because the life of a LiveNodeReader
//       is directly determined by how many references are active. When no references are active, the LiveNodeReader is discarded. In the case of the writer we can not do
//       that because livenodes can ( and have to ) be moved into the main data buffer only at the right time, independently of what is the external use of them.

class CNodeLiveWriterRef
{
public:

	CNodeLiveWriterRef(CWriter& Writer)
		: m_Writer(Writer)
		, m_nodeId(XMLCPB_INVALID_ID)
		, m_pNode_Debug(NULL)
		, m_safecheckID(XMLCPB_INVALID_SAFECHECK_ID)
	{}

	explicit CNodeLiveWriterRef(CWriter& Writer, NodeLiveID nodeId);

	CNodeLiveWriterRef(const CNodeLiveWriterRef& other) : m_Writer(other.m_Writer)
	{
		CopyFrom(other);
	}

	CNodeLiveWriterRef& operator=(const CNodeLiveWriterRef& other)
	{
		CopyFrom(other);
		return *this;
	}

	CNodeLiveWriter* operator->() const { return GetNode(); }

	bool             IsValid() const;

private:

	void             CopyFrom(const CNodeLiveWriterRef& other);
	CNodeLiveWriter* GetNode() const;

private:
	CWriter&               m_Writer;
	NodeLiveID             m_nodeId;
	uint32                 m_safecheckID;
	const CNodeLiveWriter* m_pNode_Debug;   // only to help debugging. Dont use it.
};

// Nodes in the writting process follow this state path:
// - created as "LiveNode".
// - add children, attributes
// - flaged as "done" -> cant modify or add anything to it. this can be explicitely called, or automatically when another livenode of the same parent is created.
// - compacted -> its data is converted into binary format, and stored into the main buffer. All node children of a parent are compacted at the same time (actually, for nodes with big amount of children, they are compacted in blocks, not all at once )
// - novalid -> right after being compacted, the object is flaged as "novalid" so it can be reused for the next "LiveNode".

class CNodeLiveWriter : public _reference_target_t
{
	friend class CWriter;
	friend class CNodeLiveWriterRef;

public:

	CNodeLiveWriter(CWriter& Writer, NodeLiveID ID, StringID IDTag, uint32 safecheckID);
	CNodeLiveWriter(const CNodeLiveWriter& other) : m_Writer(other.m_Writer) { *this = other; }

	CNodeLiveWriter& operator=(const CNodeLiveWriter& other)
	{
		if (this != &other)
		{
			m_ID = other.m_ID;
			m_IDParent = other.m_IDParent;
			m_IDTag = other.m_IDTag;
			m_children = other.m_children;
			m_done = other.m_done;
			m_valid = other.m_valid;
			m_childrenAreCompacted = other.m_childrenAreCompacted;
			m_totalAmountChildren = other.m_totalAmountChildren;
			m_safecheckID = other.m_safecheckID;
			m_attrs = other.m_attrs;
			m_globalIdLastChildInBlock = other.m_globalIdLastChildInBlock;
		}
		return *this;
	}

	CNodeLiveWriterRef AddChildNode(const char* pChildName);
	void               Done();
	template<class T>
	void               AddAttr(const char* pAttrName, const T& data);
	void               AddAttr(const char* pAttrName, const uint8* data, uint32 len, bool needInmediateCopy = false);
	const char*        GetTag() const;

	// TODO: they do linear strcmp searchs, check if they could be avoided or if they need optimization
	bool        HaveAttr(const char* pAttrName);
	const char* ReadAttrStr(const char* pAttrName);

private:

	void       ChildIsDone();
	bool       IsValid() const { return m_valid; }
	bool       IsDone() const  { return m_done; }
	void       SetParent(NodeLiveID ID);
	NodeLiveID GetID() const   { return m_ID; }
	void       Reuse(StringID IDTag, uint32 safecheckID);
	void       CompactPendingChildren();
	void       Compact();
	uint32     GetSafeCheckID() const { return m_safecheckID; }

	#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
	void CheckHardcodedLimits();
	void ShowExtendedErrorInfo() const;
	#endif

	CWriter&                         m_Writer;
	NodeLiveID                       m_ID; // is unique on all the live nodes, but already compacted nodes could have used the same one, because the live nodes are reused and their ID do not change
	NodeLiveID                       m_IDParent;
	StringID                         m_IDTag;
	std::vector<NodeLiveID>          m_children;
	bool                             m_done;                 // true when is closed: cant add more children, more attrs, or anything else. a "done" node is just waiting to be compacted
	bool                             m_valid;                // live nodes are reused. this is false when the node is invalid, which meants is ready to be reused.
	bool                             m_childrenAreCompacted; // true: all children are already in the main buffer, they dont exist anymore as livenodes.
	uint                             m_totalAmountChildren;
	uint32                           m_safecheckID; // to prevent wrong use.
	std::vector<XMLCPB::CAttrWriter> m_attrs;
	std::vector<NodeGlobalID>        m_globalIdLastChildInBlock; // every entry is the globalId of the LAST child on each block of children

	// children blocks:
	// the children in a block are compacted at the same time, so they are contiguous in memory.
	// it would be possible to have all the children compacted at the same time, without blocks, but the separation in blocks avoid the need for an excesive amount of active live nodes at once for the very few nodes that have a big amount of children.

	static const int INITIAL_SIZE_CHILDREN_VECTOR;
	static const int INITIAL_SIZE_ATTRS_VECTOR;

	#ifdef XMLCPB_COLLECT_STATS
	// statistics
	struct TStats
	{
		TStats()
		{
			Reset();
		}
		void Reset()
		{
			m_maxNumChildren = 0;
			m_maxNumAttrs = 0;
			m_totalNodesCreated = 0;
			m_totalAttrs = 0;
			m_totalChildren = 0;
			m_totalSizeNodeData = 0;
			m_totalSizeAttrData = 0;
		}

		uint32 m_maxNumChildren;
		uint32 m_maxNumAttrs;
		uint32 m_totalNodesCreated;
		uint32 m_totalAttrs;
		uint32 m_totalChildren;
		uint32 m_totalSizeNodeData;            // not including attrs
		uint32 m_totalSizeAttrData;
	};

	static TStats m_stats;
	#endif

};

//////////////////////////////////////////////////////////////////////////
template<class T>
inline void CNodeLiveWriter::AddAttr(const char* pAttrName, const T& data)
{
	assert(!m_done && m_valid);

	CAttrWriter attr(m_Writer);
	attr.Set(pAttrName, data);

	m_attrs.push_back(attr);
}

} //end namespace

#endif
