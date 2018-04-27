// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNCCONTEXT_H__
#define __SYNCCONTEXT_H__

#pragma once

#include "INetContextListener.h"
#include <CryNetwork/ISerialize.h>

struct INetSender;

static const uint32 LINKEDELEM_TERMINATOR = 0x3fffffff; // top two bits are free to pack history counts

enum ESyncContextFlags
{
	eSCF_AssumeEnabled     = 0x0001,
	eSCF_EnsureEnabled     = 0x0002,
#if ENABLE_THIN_BINDS
	eSCF_UseBindAspectMask = 0x0004,
#endif // ENABLE_THIN_BINDS
};

enum ESpawnState
{
	eSS_Unspawned,
	eSS_Spawned,
	eSS_Enabled,
};

enum EHistory
{
	eH_Configuration = 0,
	eH_Auth,
	eH_Profile,
	eH_AspectData,
	eH_NUM_HISTORIES
};

enum EHistorySlots
{
	eHS_Configuration = 1,
	eHS_Auth          = 1,
	eHS_Profile       = NumAspects,
	eHS_AspectData    = 2 * NumAspects,
};

enum EHistoryStartSlot
{
	eHSS_Configuration = 0,
	eHSS_Auth          = eHSS_Configuration + eHS_Configuration,
	eHSS_Profile       = eHSS_Auth + eHS_Auth,
	eHSS_AspectData    = eHSS_Profile + eHS_Profile,
	eHSS_NUM_SLOTS     = eHSS_AspectData + eHS_AspectData,
};

enum EHistoryCount
{
	eHC_Zero,
	eHC_One,
	eHC_MoreThanOne,
	eHC_Invalid, // should never be seen outside of CRegularlySyncedItem - indicates recalculation is needed
};

struct SHistoryRootPointer
{
	SHistoryRootPointer() : index(LINKEDELEM_TERMINATOR), historyCount(eHC_Zero)
	{
		++g_objcnt.historyRoot;
	}
	SHistoryRootPointer(const SHistoryRootPointer& rhs)
	{
		index = rhs.index;
		historyCount = rhs.historyCount;
		++g_objcnt.historyRoot;
	}
	~SHistoryRootPointer()
	{
		--g_objcnt.historyRoot;
	}
	SHistoryRootPointer& operator=(const SHistoryRootPointer& rhs)
	{
		this->~SHistoryRootPointer();
		new(this)SHistoryRootPointer(rhs);
		return *this;
	}
	uint32 index        : 30;
	uint32 historyCount : 2;
};

struct SContextViewObject
{
	SContextViewObject()
	{
		salt = 0;
		Reset();
	}

	// handle to our most recent ordered no-attach rmi
	SSendableHandle orderedRMIHandle; //min
	// handle to our most recent update message (cleared on send)
	SSendableHandle msgHandle; //xtra (min?)
	SSendableHandle activeHandle;
#if ENABLE_URGENT_RMIS
	SSendableHandle bindHandle;
#endif // ENABLE_URGENT_RMIS
	EntityId        predictionHandle;
	ESpawnState     spawnState;
	uint32          locks; //min
	uint16          salt;
	uint8           authority : 1; //min

	void            Reset()
	{
		spawnState = eSS_Unspawned;
		authority = false;
		locks = 0;
		orderedRMIHandle = SSendableHandle();
		msgHandle = SSendableHandle();
		activeHandle = SSendableHandle();
#if ENABLE_URGENT_RMIS
		bindHandle = SSendableHandle();
#endif // ENABLE_URGENT_RMIS
		predictionHandle = 0;
#ifdef _DEBUG
		lockers.clear();
#endif
	}

#ifdef _DEBUG
	std::map<string, int> lockers;
#endif
};

struct SContextViewObjectEx
{
	SContextViewObjectEx()
	{
		for (int i = 0; i < eHSS_NUM_SLOTS; i++) // just to avoid an assert below...
			historyElems[i] = SHistoryRootPointer();
		Reset();
	}

	// handles to our hash messages
	SSendableHandle notifyPartialUpdateHandle[NumAspects];

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	CTimeValue          voiceTransmitTime; //xtra
	CTimeValue          voiceReceiptTime;  //xtra
#endif
	CTimeValue          partialUpdateReceived[NumAspects]; //xtra
	int                 partialUpdatesRemaining[NumAspects];

	SHistoryRootPointer historyElems[eHSS_NUM_SLOTS];

	NetworkAspectType   dirtyAspects;
	uint32              ackedAspectVersions[NumAspects];

	void                Reset()
	{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		voiceReceiptTime = 0.0f;
		voiceTransmitTime = 0.0f;
#endif
		dirtyAspects = 0;

		for (NetworkAspectID i = 0; i < NumAspects; i++)
		{
			partialUpdateReceived[i] = 0.0f;
			partialUpdatesRemaining[i] = 0;
			notifyPartialUpdateHandle[i] = SSendableHandle();
			ackedAspectVersions[i] = ~uint32(0);
		}
		for (int i = 0; i < eHSS_NUM_SLOTS; i++)
		{
			NET_ASSERT(historyElems[i].index == LINKEDELEM_TERMINATOR);
			historyElems[i] = SHistoryRootPointer();
		}
	}
};

struct SSyncContext
{
	SSyncContext()
	{
		basisSeq = 0xbadf00d;
		currentSeq = 0xdeadbeef;
		timeValue = 0xdeadbeef;
		index = 0xde;
		flags = 0;
		pView = 0;
		pViewObj = 0;
		pSentAlready = 0;
		pViewObjEx = 0;
	}
	uint32                basisSeq;
	uint32                currentSeq;
	uint32                timeValue;
	NetworkAspectID       index;
	SNetObjectID          objId;
	uint32                flags;
	CContextView*         pView;
	NetworkAspectType*    pSentAlready;
	SContextObjectRef     ctxObj;
	SContextViewObject*   pViewObj;
	SContextViewObjectEx* pViewObjEx;
	//const SContextObject * pCtxObj;
};

struct SSendContext : public SSyncContext
{
	SSendContext() : pSender(0) {}
	INetSender* pSender;
};

struct SReceiveContext : public SSyncContext
{
	SReceiveContext(TSerialize s) : ser(s) {}
	mutable TSerialize ser;
};

// this is data about an attachment
struct SAttachmentIndex
{
	SNetObjectID id;
	uint32       index;

	bool operator<(const SAttachmentIndex& rhs) const
	{
		return id < rhs.id || (id == rhs.id && index < rhs.index);
	}
};

#endif
