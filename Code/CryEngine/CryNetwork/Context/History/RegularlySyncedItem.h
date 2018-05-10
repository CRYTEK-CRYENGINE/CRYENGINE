// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __REGULARLYSYNCEDITEM_H__
#define __REGULARLYSYNCEDITEM_H__

#include "Config.h"
#include "../SyncContext.h"

class CRegularlySyncedItem;

class CRegularlySyncedItems_StaticDefs
{
public:
	struct SElem
	{
		SElem()
		{
			++g_objcnt.rselem;
		}
		~SElem()
		{
			--g_objcnt.rselem;
		}
		SElem(const SElem& rhs) : seq(rhs.seq), data(rhs.data)
		{
			++g_objcnt.rselem;
		}
		SElem& operator=(const SElem& rhs)
		{
			seq = rhs.seq;
			data = rhs.data;
			return *this;
		}
		uint32 seq;
		uint32 data;
	};

protected:
	struct SLinkedElem : public SElem
	{
#if DETAIL_REGULARLY_SYNCED_ITEM_DEBUG
		SLinkedElem() : nextSetter(-1) {}
		int    nextSetter;
#endif
		uint32 next  : 31;
		uint32 valid : 1;
	};
};

class CRegularlySyncedItems : public CRegularlySyncedItems_StaticDefs
{
	friend class CRegularlySyncedItem;

public:
	CRegularlySyncedItems(uint32 idBase, uint32 idMul) : m_idBase(idBase), m_idMul(idMul), m_curSeq(0), m_resizes(10) {}
	virtual ~CRegularlySyncedItems() { NET_ASSERT(m_elems.empty()); }

	void                 GetMemoryStatistics(ICrySizer* pSizer);
	CRegularlySyncedItem GetItem(uint32 id, SHistoryRootPointer* historyElems);

	void                 Reset();

	ILINE uint32         GetResizeCount() const { return m_resizes; }

private:
	virtual void Destroy(uint32 data) {}

	void         GotCurSeq(uint32 curSeq)
	{
		if (curSeq > m_curSeq)
			m_curSeq = curSeq;
	}

	uint32                   m_idMul;
	uint32                   m_idBase;
	uint32                   m_curSeq;

	std::vector<SLinkedElem> m_elems;
	std::vector<uint32>      m_freeElems;
	uint32                   m_resizes;
	void Verify();
};

class CRegularlySyncedItem : public CRegularlySyncedItems_StaticDefs
{
	friend class CRegularlySyncedItems;

public:
	CRegularlySyncedItem() : m_pParent(nullptr), m_pRoot(nullptr), m_id(0) {}

	typedef CRegularlySyncedItems::SElem SElem;

	void          AddElem(uint32 seq, uint32 data);
	// WARNING: returned pointer is only valid until AddElem is called on *ANY* other item
	SElem*        FindPrevElem(uint32 basisSeq, uint32 curSeq) const;
	void          RemoveOlderThan(uint32 seq);
	void          Flush();
	void          AckUpdate(uint32 seq, bool ack);

	void          DumpStuff(const char* txt, SNetObjectID id, uint32 basis, uint32 cur);

	ILINE uint32  GetResizeCount() const { return m_pParent->GetResizeCount(); }

	EHistoryCount GetHistoryCount() const;

	struct ValueIter : public CRegularlySyncedItems_StaticDefs
	{
	public:
		friend class CRegularlySyncedItem;

		ValueIter() : m_cur(LINKEDELEM_TERMINATOR), m_pElems(0) {}

		SElem* Current();
		void   Next();

	private:
		ValueIter(uint32 idx, SLinkedElem* pElems) : m_cur(idx), m_pElems(pElems) {}
		uint32       m_cur;
		SLinkedElem* m_pElems;
	};
	struct ConstValueIter : public CRegularlySyncedItems_StaticDefs
	{
	public:
		friend class CRegularlySyncedItem;

		ConstValueIter() : m_cur(LINKEDELEM_TERMINATOR), m_pElems(0) {}

		const SElem* Current();
		void         Next();

	private:
		ConstValueIter(uint32 idx, SLinkedElem* pElems) : m_cur(idx), m_pElems(pElems) {}
		uint32             m_cur;
		const SLinkedElem* m_pElems;
	};

	ValueIter      GetIter();
	ConstValueIter GetConstIter() const;

private:
	void          RemoveFrom(uint32 start, uint32 assertIfSeqGTE = ~0);
	void          CompleteAck(uint32& prevnextidx, bool ack, uint32 current);
	EHistoryCount GetHistoryCount_Slow() const;
	void          CheckValid()
	{
		//NET_ASSERT(m_pRoot->historyCount == eHC_Invalid || m_pRoot->historyCount == GetHistoryCount_Slow());
		m_pParent->Verify();
	}

	typedef CRegularlySyncedItems::SLinkedElem SLinkedElem;
	ILINE CRegularlySyncedItem(CRegularlySyncedItems* pParent, SHistoryRootPointer* pRoot, uint32 id) : m_pParent(pParent), m_pRoot(pRoot), m_id(id)
	{
	}

	CRegularlySyncedItems* m_pParent;
	SHistoryRootPointer*   m_pRoot;
	uint32                 m_id;
};

ILINE CRegularlySyncedItem CRegularlySyncedItems::GetItem(uint32 id, SHistoryRootPointer* historyElems)
{
	return CRegularlySyncedItem(this, historyElems + m_idBase + m_idMul * id, id);
}

#endif
