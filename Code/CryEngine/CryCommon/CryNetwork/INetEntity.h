// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/BaseTypes.h>
#include <CryNetwork/SerializeFwd.h>

#include <CryEntitySystem/IEntityBasicTypes.h>
#include <CrySchematyc/Utils/EnumFlags.h>

struct IEntityComponent;

struct ISerializableInfo : public CMultiThreadRefCount, public ISerializable {};
typedef _smart_ptr<ISerializableInfo> ISerializableInfoPtr;

#define UNSAFE_NUM_ASPECTS 32                 // 8,16 or 32
#define NUM_ASPECTS        (UNSAFE_NUM_ASPECTS)

#define _CRYNETWORK_CONCAT(x, y) x ## y
#define CRYNETWORK_CONCAT(x, y)  _CRYNETWORK_CONCAT(x, y)
#define ASPECT_TYPE CRYNETWORK_CONCAT(uint, UNSAFE_NUM_ASPECTS)

#if NUM_ASPECTS > 32
	#error Aspects > 32 Not supported at this time
#endif

typedef ASPECT_TYPE NetworkAspectType;
typedef uint8       NetworkAspectID;

#define NET_ASPECT_ALL (NetworkAspectType(0xFFFFFFFF))

enum EEntityAspects : uint32
{
	eEA_All               = NET_ASPECT_ALL,
	// 0x01u                       // aspect 0
	eEA_Script            = 0x02u, // aspect 1
	// 0x04u                       // aspect 2
	eEA_Physics           = 0x08u, // aspect 3
	eEA_GameClientStatic  = 0x10u, // aspect 4
	eEA_GameServerStatic  = 0x20u, // aspect 5
	eEA_GameClientDynamic = 0x40u, // aspect 6
	eEA_GameServerDynamic = 0x80u, // aspect 7
#if NUM_ASPECTS > 8
	eEA_GameClientA       = 0x0100u, // aspect 8
	eEA_GameServerA       = 0x0200u, // aspect 9
	eEA_GameClientB       = 0x0400u, // aspect 10
	eEA_GameServerB       = 0x0800u, // aspect 11
	eEA_GameClientC       = 0x1000u, // aspect 12
	eEA_GameServerC       = 0x2000u, // aspect 13
	eEA_GameClientD       = 0x4000u, // aspect 14
	eEA_GameClientE       = 0x8000u, // aspect 15
#endif
#if NUM_ASPECTS > 16
	eEA_GameClientF = 0x00010000u,       // aspect 16
	eEA_GameClientG = 0x00020000u,       // aspect 17
	eEA_GameClientH = 0x00040000u,       // aspect 18
	eEA_GameClientI = 0x00080000u,       // aspect 19
	eEA_GameClientJ = 0x00100000u,       // aspect 20
	eEA_GameServerD = 0x00200000u,       // aspect 21
	eEA_GameClientK = 0x00400000u,       // aspect 22
	eEA_GameClientL = 0x00800000u,       // aspect 23
	eEA_GameClientM = 0x01000000u,       // aspect 24
	eEA_GameClientN = 0x02000000u,       // aspect 25
	eEA_GameClientO = 0x04000000u,       // aspect 26
	eEA_GameClientP = 0x08000000u,       // aspect 27
	eEA_GameServerE = 0x10000000u,       // aspect 28
	eEA_Aspect29    = 0x20000000u,       // aspect 29
	eEA_Aspect30    = 0x40000000u,       // aspect 30
	eEA_Aspect31    = 0x80000000u,       // aspect 31
#endif
};

enum EBindToNetworkMode
{
	eBTNM_Normal,
	eBTNM_Force,
	eBTNM_NowInitialized
};

//! Implement this to subscribe for aspect profile change events.
struct IGameObjectProfileManager
{
	virtual ~IGameObjectProfileManager() {}

	//! Override to receive profile change events.
	virtual bool  SetAspectProfile(EEntityAspects aspect, uint8 profile) = 0;
	//! Override to change the default (spawned) profile for an aspect.
	virtual uint8 GetDefaultProfile(EEntityAspects aspect) = 0;
};

// RMI-related stuff
enum ERMInvocation
{
	eRMI_ToClientChannel = 0x01,
	eRMI_ToOwnClient     = 0x02,
	eRMI_ToOtherClients  = 0x04,
	eRMI_ToAllClients    = 0x08,

	eRMI_ToServer        = 0x100,

	eRMI_NoLocalCalls    = 0x10000,
	eRMI_NoRemoteCalls   = 0x20000,

	eRMI_ToRemoteClients = eRMI_NoLocalCalls | eRMI_ToAllClients
};

//! This enum describes different reliability mechanisms for packet delivery.
//! Do not change ordering here without updating ServerContextView, ClientContextView.
//! \note This is pretty much deprecated; new code should use the message queue facilities
//! to order messages, and be declared either ReliableUnordered or UnreliableUnordered.
enum ENetReliabilityType
{
	eNRT_ReliableOrdered,
	eNRT_ReliableUnordered,
	eNRT_UnreliableOrdered,
	eNRT_UnreliableUnordered,

	// Must be last.
	eNRT_NumReliabilityTypes
};

//! Implementation of CContextView relies on the first two values being as they are.
enum ERMIAttachmentType
{
	eRAT_PreAttach  = 0,
	eRAT_PostAttach = 1,
	eRAT_NoAttach,

	//! Urgent RMIs will be sent at the next sync with game.
	//! \note Use with caution as this will increase bandwidth.
	eRAT_Urgent,

	//! If an RMI doesn't care about the object update, then use independent RMIs as they can be sent in the urgent RMI flush.
	eRAT_Independent,

	// Must be last.
	eRAT_NumAttachmentTypes
};

struct INetAtSyncItem;
struct INetChannel;

//! Each entity has a network proxy implementing this interface. Use it
//! to bind the entity to network, to fine-tune synced aspects or to subscribe
//! for aspect profile or authority change events.
//! \see CNetEntity
struct INetEntity
{
	virtual ~INetEntity() {};

	//! Every entity must be bound to network to be synchronized.
	virtual bool BindToNetwork(EBindToNetworkMode mode = eBTNM_Normal) = 0;
	//! Bind this entity to the network system, with a dependency on its parent
	virtual bool BindToNetworkWithParent(EBindToNetworkMode mode, EntityId parentId) = 0;

	//! Use this to mark the aspect dirty and trigger its synchronization.
	virtual void MarkAspectsDirty(NetworkAspectType aspects) = 0;

	//! Enables/disables network aspects.
	/** Can be called anytime after binding. */
	virtual void              EnableAspect(NetworkAspectType aspects, bool enable) = 0;
	virtual NetworkAspectType GetEnabledAspects() const = 0;

	//! Can be used to disable delegation of a certain aspect, i.e. physics.
	/** This should be called before BindToNetwork(). By default, all delegatable aspects are enabled.*/
	virtual void EnableDelegatableAspect(NetworkAspectType aspects, bool enable) = 0;
	virtual bool IsAspectDelegatable(NetworkAspectType aspect) const = 0;

	//! Retrieves the default profile of an aspect.
	virtual uint8 GetDefaultProfile(EEntityAspects aspect) = 0;
	//! Changes the current profile of an aspect.
	/** Game code should ensure that things work out correctly. */
	/** Example: Change physicalization. */
	virtual bool   SetAspectProfile(EEntityAspects aspect, uint8 profile, bool fromNetwork = false) = 0;
	virtual uint8  GetAspectProfile(EEntityAspects aspect) = 0;

	virtual bool   CaptureProfileManager(IGameObjectProfileManager* pPM) = 0;
	virtual void   ReleaseProfileManager(IGameObjectProfileManager* pPM) = 0;
	virtual bool   HasProfileManager() = 0;
	virtual void   ClearProfileManager() = 0;

	virtual void   SetChannelId(uint16 id) = 0;
	virtual uint16 GetChannelId() const = 0;
	virtual void   BecomeBound() = 0;
	virtual bool   IsBoundToNetwork() const = 0;

	//! A one off call to never enable the physics aspect, this needs to be done *before* the BindToNetwork
	virtual void DontSyncPhysics() = 0;

	//! Internal use. \see CNetContext::DelegateAuthority() to delegate the authority.
	virtual void SetAuthority(bool auth) = 0;
	virtual bool HasAuthority() const = 0;
	virtual void SetNetworkParent(EntityId id) = 0;

	//! Internal use. \see IEntityComponent::NetSerialize() to implement network serialization.
	virtual bool NetSerializeEntity(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) = 0;

	//! Sent by the entity when its transformation changes
	virtual void OnNetworkedEntityTransformChanged(EntityTransformationFlagsMask transformReasons) = 0;

	virtual void OnComponentAddedDuringInitialization(IEntityComponent* pComponent) const = 0;
	virtual void OnEntityInitialized() = 0;

	//--------------------------------------------------------------------------

	//! Entity-local unique index of the RMI that is sent over the network.
	struct SRmiIndex
	{
		explicit SRmiIndex(size_t idx) : value(static_cast<decltype(value)>(idx)) {};
		uint8 value;
	};

	//! This structure describes a single RMI handler. Each entity contains a vector
	//! of those, and the index of a particular handler is sent over the network.
	struct SRmiHandler
	{
		typedef INetAtSyncItem* (* DecoderF)(TSerialize*, EntityId, INetChannel*);
		DecoderF            decoder;
		ENetReliabilityType reliability;
		ERMIAttachmentType  attach_type;
	};

	//! Internal use.
	virtual void                  RmiRegister(const SRmiHandler& handler) = 0;
	//! Internal use.
	virtual SRmiIndex             RmiByDecoder(SRmiHandler::DecoderF decoder, SRmiHandler** handler) = 0;
	//! Internal use.
	virtual SRmiHandler::DecoderF RmiByIndex(const SRmiIndex idx) = 0;
};
