// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INETCONTEXTLISTENER_H__
#define __INETCONTEXTLISTENER_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include <CryCore/BitFiddling.h>
#include "MementoMemoryManager.h"
#include "Compression/CompressionManager.h"
#include "Utils.h"

class CSimpleOutputStream;
class CVoicePacket;
class CNetContextState;

static const uint8 NoProfile = 255;

enum ENetObjectEvent
{
	eNOE_InvalidEvent       = 0,
	eNOE_BindObject         = 0x00000001,
	eNOE_UnbindObject       = 0x00000002,
	eNOE_Reset              = 0x00000004,
	eNOE_ObjectAspectChange = 0x00000008,
	eNOE_UnboundObject      = 0x00000010,
	eNOE_ChangeContext      = 0x00000020,
	eNOE_EstablishedContext = 0x00000040,
	eNOE_ReconfiguredObject = 0x00000080,
	eNOE_BindAspects        = 0x00000100,
	eNOE_UnbindAspects      = 0x00000200,
	eNOE_ContextDestroyed   = 0x00000400,
	eNOE_SetAuthority       = 0x00000800,
	eNOE_UpdateScheduler    = 0x00001000,
	eNOE_RemoveStaticEntity = 0x00002000,
	eNOE_InGame             = 0x00004000,
	eNOE_PredictSpawn       = 0x00008000,
	eNOE_SendVoicePackets   = 0x00010000,
	eNOE_RemoveRMIListener  = 0x00020000,
	eNOE_SetAspectProfile   = 0x00040000,
	eNOE_SyncWithGame_Start = 0x00080000,
	eNOE_SyncWithGame_End   = 0x00100000,
	eNOE_GotBreakage        = 0x00200000,
	eNOE_ValidatePrediction = 0x00400000,
	eNOE_PartialUpdate      = 0x00800000,

	eNOE_DebugEvent         = 0x80000000
};

enum ENetObjectDebugEvent
{
};

enum ENetChannelEvent
{
	eNCE_InvalidEvent     = 0,
	eNCE_ChannelDestroyed = 0x00000001,
	eNCE_RevokedAuthority = 0x00000002,
	eNCE_SetAuthority     = 0x00000004,
	eNCE_UpdatedObject    = 0x00000008,
	eNCE_BoundObject      = 0x00000010,
	eNCE_UnboundObject    = 0x00000020,
	eNCE_SentUpdate       = 0x00000040,
	eNCE_AckedUpdate      = 0x00000080,
	eNCE_NackedUpdate     = 0x00000100,
	eNCE_InGame           = 0x00000200,
	eNCE_DeclareWitness   = 0x00000400,
	eNCE_SendRMI          = 0x00000800,
	eNCE_AckedRMI         = 0x00001000,
	eNCE_NackedRMI        = 0x00002000,
	eNCE_ScheduleRMI      = 0x00004000,
};

// these two functions are implemented in NetContext.cpp
string GetEventName(ENetObjectEvent);
string GetEventName(ENetChannelEvent);

struct SNetObjectAspectChange
{
	ILINE SNetObjectAspectChange() : aspectsChanged(0), forceChange(0) {}
	ILINE SNetObjectAspectChange(NetworkAspectType aspectsChanged, NetworkAspectType forceChange)
	{
		this->aspectsChanged = aspectsChanged;
		this->forceChange = forceChange;
	}
	ILINE SNetObjectAspectChange& operator|=(const SNetObjectAspectChange& a)
	{
		aspectsChanged |= a.aspectsChanged;
		forceChange |= a.forceChange;
		return *this;
	}
	NetworkAspectType aspectsChanged;
	NetworkAspectType forceChange;
};

struct SNetIntBreakDescription
{
	const SNetMessageDef*    pMessage;
	IBreakDescriptionInfoPtr pMessagePayload;
	ENetBreakDescFlags       flags;
	DynArray<EntityId>       spawnedObjects;
	EntityId                 breakageEntity;
};
struct SNetClientBreakDescription : public DynArray<SNetObjectID>, public CMultiThreadRefCount
{
	SNetClientBreakDescription()
	{
		++g_objcnt.clientBreakDescription;
	}
	~SNetClientBreakDescription()
	{
		--g_objcnt.clientBreakDescription;
	}
};
typedef _smart_ptr<SNetClientBreakDescription> SNetClientBreakDescriptionPtr;

struct SNetObjectEvent
{
	SNetObjectEvent() :
		event(eNOE_InvalidEvent),
		aspects(0)
	{
	}

	ENetObjectEvent event;
	SNetObjectID    id;
	union
	{
		struct
		{
			union
			{
				NetworkAspectType aspects;
				NetworkAspectID   aspectID;
			};
			uint8 profile; // only for eNOE_SetAspectProfile
		};
		// eNOE_SetAuthority
		bool authority;
		// used for eNOE_ObjectAspectChange
		const std::pair<SNetObjectID, SNetObjectAspectChange>* pChanges;
		// used for eNOE_SendVoicePackets
		struct
		{
			bool                Route;
			const CVoicePacket* pVoicePackets;
			size_t              nVoicePackets;
		};
		// eNOE_RemoveStaticEntity, eNOE_UnbindObject
		EntityId                       userID;
		// eNOE_RemoveRMIListener
		IRMIListener*                  pRMIListener;
		ENetObjectDebugEvent           dbgEvent;
		// eNOE_GotBreakage
		const SNetIntBreakDescription* pBreakage;
		// eNOE_PredictSpawn, eNOE_ValidatePrediction
		struct
		{
			EntityId predictedEntity;
			EntityId predictedReference;
		};
		// eNOE_ChangeContext
		CNetContextState* pNewState;
	};
};

struct SNetChannelEvent
{
	SNetChannelEvent() :
		event(eNCE_InvalidEvent),
		aspects(0),
		seq(~unsigned(0)),
		pRMIBody(0)
	{
	}

	ENetChannelEvent   event;
	NetworkAspectType  aspects;
	SNetObjectID       id;
	unsigned           seq;
	IRMIMessageBodyPtr pRMIBody;
};

struct SObjectMemUsage
{
	SObjectMemUsage() : required(0), used(0), instances(0) {}
	size_t required;
	size_t used;
	size_t instances;

	SObjectMemUsage& operator+=(const SObjectMemUsage& rhs)
	{
		required += rhs.required;
		used += rhs.used;
		instances += rhs.instances;
		return *this;
	}

	friend SObjectMemUsage operator+(const SObjectMemUsage& a, const SObjectMemUsage& b)
	{
		SObjectMemUsage temp = a;
		temp += b;
		return temp;
	}
};

struct INetContextListener : public CMultiThreadRefCount
{
	inline virtual ~INetContextListener() {}
	virtual bool            IsDead() = 0;
	virtual void            OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent) = 0;
	virtual void            OnChannelEvent(CNetContextState* pState, INetChannel* pFrom, SNetChannelEvent* pEvent) = 0;
	virtual string          GetName() = 0;
	virtual SObjectMemUsage GetObjectMemUsage(SNetObjectID id) { return SObjectMemUsage(); }
	virtual void            PerformRegularCleanup() = 0;
	virtual INetChannel*    GetAssociatedChannel()             { return NULL; }
	virtual void            Die() = 0;
};
typedef _smart_ptr<INetContextListener> INetContextListenerPtr;

struct IRMILoggerValue
{
	virtual ~IRMILoggerValue(){}
	virtual void WriteStream(CSimpleOutputStream&, const char*) = 0;
};

struct IRMILogger
{
	virtual ~IRMILogger(){}
	virtual void BeginFunction(const char* name) = 0;
	virtual void OnProperty(const char* name, IRMILoggerValue* pSer) = 0;
	virtual void EndFunction() = 0;

	virtual void LogCppRMI(EntityId id, IRMICppLogger* pLogger) = 0;
};

class CContextView;
typedef _smart_ptr<CContextView> CContextViewPtr;

static const int NumAspects = NUM_ASPECTS;

enum EContextObjectLock
{
	eCOL_Reference = 0,
	eCOL_GameDataSync,
	//	eCOL_GameAspectSync,
	eCOL_DisableAspect0,
	eCOL_DisableAspect1,
	eCOL_DisableAspect2,
	eCOL_DisableAspect3,
	eCOL_DisableAspect4,
	eCOL_DisableAspect5,
	eCOL_DisableAspect6,
	eCOL_DisableAspect7,
#if NUM_ASPECTS > 8
	eCOL_DisableAspect8,
	eCOL_DisableAspect9,
	eCOL_DisableAspect10,
	eCOL_DisableAspect11,
	eCOL_DisableAspect12,
	eCOL_DisableAspect13,
	eCOL_DisableAspect14,
	eCOL_DisableAspect15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
	eCOL_DisableAspect16,
	eCOL_DisableAspect17,
	eCOL_DisableAspect18,
	eCOL_DisableAspect19,
	eCOL_DisableAspect20,
	eCOL_DisableAspect21,
	eCOL_DisableAspect22,
	eCOL_DisableAspect23,
	eCOL_DisableAspect24,
	eCOL_DisableAspect25,
	eCOL_DisableAspect26,
	eCOL_DisableAspect27,
	eCOL_DisableAspect28,
	eCOL_DisableAspect29,
	eCOL_DisableAspect30,
	eCOL_DisableAspect31,
#endif//NUM_ASPECTS > 16
	eCOL_NUM_LOCKS
};

enum ESpawnType
{
	eST_Normal = 0,
	eST_Static,
	eST_Collected,
	eST_NUM_SPAWN_TYPES
};

struct SContextObject
{
	SContextObject()
		: bAllocated(false)
		, bOwned(false)
		, bControlled(false)
		, pController(nullptr)
		, spawnType(eST_Normal)
		, salt(1)
		, userID(0)
		, refUserID(0)
	{

		for (size_t i = 0; i < eCOL_NUM_LOCKS; ++i)
		{
			vLocks[i] = eCOL_Reference;

		}
		for (size_t i = 0; i < NumAspects; ++i)
		{
			vAspectProfiles[i] = 255; // TODO: find out correct default value
			vAspectDefaultProfiles[i] = 255;
		}
	}
	bool                  bAllocated  : 1; //min
	// is this context the owner of the object (can set authority)
	bool                  bOwned      : 1;                                                        //min?
	bool                  bControlled : 1;                                                        //min?
	uint                  spawnType: CompileTimeIntegerLog2_RoundUp<eST_NUM_SPAWN_TYPES>::result; //min
	// all of the different kinds of global locks an object can have
	uint16                vLocks[eCOL_NUM_LOCKS]; //min
#if CHECKING_BIND_REFCOUNTS
	std::map<string, int> debug_whosLocking;
#endif
	// our parent object
	SNetObjectID           parent; //min?
	// the user id for this object
	EntityId               userID; //min?
	// the original user id for this object (it's never cleared to zero)
	EntityId               refUserID; //min?
	// currently controlling channel (or NULL)
	INetContextListenerPtr pController; //min
	// which profile is each aspect in
	uint8                  vAspectProfiles[NumAspects]; //xtra
	// what is the default profile for each aspect
	uint8                  vAspectDefaultProfiles[NumAspects]; //xtra
	uint16                 salt;

	const char* GetName() const;

	const char* GetClassName() const;

	const char* GetLockString() const
	{
		static char s[eCOL_NUM_LOCKS + 1];
		for (int i = 0; i < eCOL_NUM_LOCKS; i++)
		{
			if (vLocks[i] < 10)
				s[i] = '0' + vLocks[i];
			else
				s[i] = '#';
		}
		s[eCOL_NUM_LOCKS] = 0;
		return s;
	}
};

struct SContextObjectEx
{
	SContextObjectEx()
	{
#if FULL_ON_SCHEDULING
		hasPosition = false;
		hasDirection = false;
		hasFov = false;
		hasDrawDistance = false;

		position.zero();
		direction.zero();
		fov = 0.0f;
		drawDistance = 0.0f;
#endif

		nAspectsEnabled = 0;
		delegatableMask = NET_ASPECT_ALL;
#if ENABLE_THIN_BINDS
		bindAspectMask = 0;
#endif // ENABLE_THIN_BINDS

		for (size_t i = 0; i < NumAspects; ++i)
		{
			for (size_t j = 0; j < MaxProfilesPerAspect; ++j)
				vAspectProfileChunks[i][j] = 0;
			vAspectData[i] = CMementoMemoryManager::InvalidHdl;
			vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
		}
	}

#if FULL_ON_SCHEDULING
	bool hasPosition     : 1; //xtra
	bool hasDirection    : 1; //xtra
	bool hasFov          : 1; //xtra
	bool hasDrawDistance : 1; //xtra
#endif

	// which aspects are enabled on this object?
	NetworkAspectType nAspectsEnabled; //min
	NetworkAspectType delegatableMask;

#if ENABLE_THIN_BINDS
	NetworkAspectType bindAspectMask;
#endif // ENABLE_THIN_BINDS

	// mapping of profile --> chunk for this object
	ChunkID vAspectProfileChunks[NumAspects][MaxProfilesPerAspect]; //xtra

	// scheduling profiles
	uint32                 scheduler_normal; //xtra
	uint32                 scheduler_owned;  //xtra
	CPriorityPulseStatePtr pPulseState;      //xtra

#if FULL_ON_SCHEDULING
	// optional - hasPosition
	Vec3  position; //xtra
	// optional - hasDirection
	Vec3  direction; //xtra
	// optional - hasFov
	float fov; //xtra
	// optional - hasDrawDistance
	float drawDistance; //xtra
#endif

	// current state of each aspect
	TMemHdl vAspectData[NumAspects]; //xtra
	TMemHdl vRemoteAspectData[NumAspects];
	uint32  vAspectDataVersion[NumAspects]; //xtra
};

struct SContextObjectRef
{
	SContextObjectRef() : main(NULL), xtra(NULL) {}
	SContextObjectRef(const SContextObject* m, const SContextObjectEx* x) : main(m), xtra(x) {}
	const SContextObject*   main;
	const SContextObjectEx* xtra;
};

inline ChunkID GetChunkIDFromObject(const SContextObjectRef& ref, NetworkAspectID aspectIdx)
{
	if (ref.main->vAspectProfiles[aspectIdx] >= MaxProfilesPerAspect)
		return ref.xtra->vAspectProfileChunks[aspectIdx][ref.main->vAspectDefaultProfiles[aspectIdx]];
	return ref.xtra->vAspectProfileChunks[aspectIdx][ref.main->vAspectProfiles[aspectIdx]];
}

#endif
