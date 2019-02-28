// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IGAMEOBJECT_H__
#define __IGAMEOBJECT_H__

#pragma once
#define GAME_OBJECT_SUPPORTS_CUSTOM_USER_DATA 1

#include <CryEntitySystem/IEntityComponent.h>

#include <CryNetwork/INetEntity.h>
#include <CryNetwork/SerializeFwd.h>
#include "IActionMapManager.h"
#include <CryMemory/PoolAllocator.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryNetwork/Rmi.h>
#include "IGameObjectSystem.h"

inline void GameWarning(const char*, ...) PRINTF_PARAMS(1, 2);

struct IGameObjectExtension;
struct IGameObjectView;
struct IActionListener;
struct IMovementController;
struct IGameObjectProfileManager;
struct IWorldQuery;

enum EEntityPhysicsEvents
{
	eEPE_OnCollisionLogged        = 1 << 0,   // Logged events on lower byte.
	eEPE_OnPostStepLogged         = 1 << 1,
	eEPE_OnStateChangeLogged      = 1 << 2,
	eEPE_OnCreateEntityPartLogged = 1 << 3,
	eEPE_OnUpdateMeshLogged       = 1 << 4,
	eEPE_AllLogged                = eEPE_OnCollisionLogged | eEPE_OnPostStepLogged |
	                                eEPE_OnStateChangeLogged | eEPE_OnCreateEntityPartLogged |
	                                eEPE_OnUpdateMeshLogged,

	eEPE_OnCollisionImmediate        = 1 << 8, // Immediate events on higher byte.
	eEPE_OnPostStepImmediate         = 1 << 9,
	eEPE_OnStateChangeImmediate      = 1 << 10,
	eEPE_OnCreateEntityPartImmediate = 1 << 11,
	eEPE_OnUpdateMeshImmediate       = 1 << 12,
	eEPE_AllImmediate                = eEPE_OnCollisionImmediate | eEPE_OnPostStepImmediate |
	                                   eEPE_OnStateChangeImmediate | eEPE_OnCreateEntityPartImmediate |
	                                   eEPE_OnUpdateMeshImmediate,
};

static const int MAX_UPDATE_SLOTS_PER_EXTENSION = 5;

enum EUpdateEnableCondition
{
	eUEC_Never,
	eUEC_Always,
	eUEC_Visible,
	eUEC_InRange,
	eUEC_VisibleAndInRange,
	eUEC_VisibleOrInRange,
	eUEC_VisibleOrInRangeIgnoreAI,
	eUEC_VisibleIgnoreAI,
	eUEC_WithoutAI,
};

enum EPrePhysicsUpdate
{
	ePPU_Never,
	ePPU_Always,
	ePPU_WhenAIActivated
};

enum EGameObjectAIActivationMode
{
	eGOAIAM_Never,
	eGOAIAM_Always,
	eGOAIAM_VisibleOrInRange,
	// Must be last.
	eGOAIAM_COUNT_STATES,
};

enum EAutoDisablePhysicsMode
{
	eADPM_Never,
	eADPM_WhenAIDeactivated,
	eADPM_WhenInvisibleAndFarAway,
	// Must be last.
	eADPM_COUNT_STATES,
};

struct SGameObjectExtensionRMI
{
	void GetMemoryUsage(ICrySizer* pSizer) const {}
	typedef INetAtSyncItem* (* DecoderFunction)(TSerialize, EntityId*, INetChannel*);

	DecoderFunction       decoder;
	const char*           description;
	const void*           pBase;
	const SNetMessageDef* pMsgDef;
	ERMIAttachmentType    attach;
	bool                  isServerCall;
	bool                  lowDelay;
	ENetReliabilityType   reliability;
};

// Summary
//   Interface used to interact with a game object
// See Also
//   IGameObjectExtension
struct IGameObject : public IEntityComponent, public IActionListener, public INetEntity
{
protected:
	template<class T>
	class CRMIBodyImpl : public IRMIMessageBody
	{
	public:
		void SerializeWith(TSerialize ser)
		{
			m_params.SerializeWith(ser);
		}

		size_t GetSize()
		{
			return sizeof(*this);
		}

#if ENABLE_RMI_BENCHMARK
		virtual const SRMIBenchmarkParams* GetRMIBenchmarkParams()
		{
			return NetGetRMIBenchmarkParams<T>(m_params);
		}
#endif

		static CRMIBodyImpl* Create(const SGameObjectExtensionRMI* method, EntityId id, const T& params, IRMIListener* pListener, int userId, EntityId dependentId)
		{
			return new(GetRMIAllocator<sizeof(CRMIBodyImpl)>().Allocate())CRMIBodyImpl(method, id, params, pListener, userId, dependentId);
		}

		void DeleteThis()
		{
			this->~CRMIBodyImpl();
			GetRMIAllocator<sizeof(CRMIBodyImpl)>().Deallocate(this);
		}

	private:
		T m_params;

		CRMIBodyImpl(const SGameObjectExtensionRMI* method, EntityId id, const T& params, IRMIListener* pListener, int userId, EntityId dependentId) :
			IRMIMessageBody(method->reliability, method->attach, id, method->pMsgDef, pListener, userId, dependentId),
			m_params(params)
		{
		}
	};

public:
	// query extension. returns 0 if extension is not there.
	virtual IGameObjectExtension* QueryExtension(const char* extension) const = 0;
	virtual IGameObjectExtension* QueryExtension(IGameObjectSystem::ExtensionID id) const = 0;

	// set extension parameters
	virtual bool                  SetExtensionParams(const char* extension, SmartScriptTable params) = 0;
	// get extension parameters
	virtual bool                  GetExtensionParams(const char* extension, SmartScriptTable params) = 0;
	// send a game object event
	virtual void                  SendEvent(const SGameObjectEvent&) = 0;
	// force the object to update even if extensions' slots are "sleeping"...
	virtual void                  ForceUpdate(bool force) = 0;
	virtual void                  ForceUpdateExtension(IGameObjectExtension* pGOE, int slot) = 0;
	// serialize some aspects of the game object
	virtual void                  FullSerialize(TSerialize ser) = 0;
	// in case things have to be set after serialization
	virtual void                  PostSerialize() = 0;
	// is the game object probably visible?
	virtual bool                  IsProbablyVisible() = 0;
	virtual bool                  IsProbablyDistant() = 0;
	virtual IGameObjectExtension* GetExtensionWithRMIBase(const void* pBase) = 0;
	virtual void                  EnablePrePhysicsUpdate(EPrePhysicsUpdate updateRule) = 0;
	virtual void                  Pulse(uint32 pulse) = 0;
	virtual void                  RegisterAsPredicted() = 0;
	virtual void                  RegisterAsValidated(IGameObject* pGO, int predictionHandle) = 0;
	virtual int                   GetPredictionHandle() = 0;

	virtual void                  RegisterExtForEvents(IGameObjectExtension* piExtention, const int* pEvents, const int numEvents) = 0;
	virtual void                  UnRegisterExtForEvents(IGameObjectExtension* piExtention, const int* pEvents, const int numEvents) = 0;

	virtual void                  ChangedNetworkState(NetworkAspectType aspects) = 0;

	// enable/disable sending physics events to this game object
	virtual void EnablePhysicsEvent(bool enable, int events) = 0;
	virtual bool WantsPhysicsEvent(int events) = 0;
	virtual void AttachDistanceChecker() = 0;

	// enable/disable AI activation flag
	virtual bool SetAIActivation(EGameObjectAIActivationMode mode) = 0;
	// enable/disable auto-disabling of physics
	virtual void SetAutoDisablePhysicsMode(EAutoDisablePhysicsMode mode) = 0;
	// for debugging updates
	virtual bool ShouldUpdate() = 0;

	// register a partial update in the netcode without actually serializing - useful only for working around other bugs
	virtual void RequestRemoteUpdate(NetworkAspectType aspectMask) = 0;

	// WARNING: there *MUST* be at least one frame between spawning ent and using this function to send an RMI if
	// that RMI is _FAST, otherwise the dependent entity is ignored
	template<class MI, class T>
	void InvokeRMIWithDependentObject(const MI method, const T& params, unsigned where, EntityId ent, int channel = -1)
	{
		InvokeRMI_Primitive(method, params, where, 0, 0, channel, ent);
	}

	template<class MI, class T>
	void InvokeRMI(const MI method, const T& params, unsigned where, int channel = -1)
	{
		InvokeRMI_Primitive(method, params, where, 0, 0, channel, 0);
	}

	template<class MI, class T>
	void InvokeRMI_Primitive(const MI method, const T& params, unsigned where, IRMIListener* pListener, int userId, int channel, EntityId dependentId)
	{
		method.Verify(params);
		gEnv->pGameFramework->DoInvokeRMI(CRMIBodyImpl<T>::Create(
				method.pMethodInfo, GetEntityId(), params, pListener, userId, dependentId),
			where, channel, true);
	}

	// turn an extension on
	ILINE bool                  ActivateExtension(const char* extension)   { return ChangeExtension(extension, eCE_Activate) != 0; }
	// turn an extension off
	ILINE void                  DeactivateExtension(const char* extension) { ChangeExtension(extension, eCE_Deactivate); }
	// forcefully get a pointer to an extension (may instantiate if needed)
	ILINE IGameObjectExtension* AcquireExtension(const char* extension)    { return ChangeExtension(extension, eCE_Acquire); }
	// release a previously acquired extension
	ILINE void                  ReleaseExtension(const char* extension)    { ChangeExtension(extension, eCE_Release); }

	ILINE EntityId GetEntityId() const
	{
		return m_entityId;
	}

	// for extensions to register for special things
	virtual bool                       CaptureView(IGameObjectView* pGOV) = 0;
	virtual void                       ReleaseView(IGameObjectView* pGOV) = 0;
	virtual bool                       CaptureActions(IActionListener* pAL) = 0;
	virtual void                       ReleaseActions(IActionListener* pAL) = 0;
	virtual void                       EnableUpdateSlot(IGameObjectExtension* pExtension, int slot) = 0;
	virtual void                       DisableUpdateSlot(IGameObjectExtension* pExtension, int slot) = 0;
	virtual uint8                      GetUpdateSlotEnables(IGameObjectExtension* pExtension, int slot) = 0;
	virtual void                       EnablePostUpdates(IGameObjectExtension* pExtension) = 0;
	virtual void                       DisablePostUpdates(IGameObjectExtension* pExtension) = 0;
	virtual void                       SetUpdateSlotEnableCondition(IGameObjectExtension* pExtension, int slot, EUpdateEnableCondition condition) = 0;
	virtual void                       PostUpdate(float frameTime) = 0;
	virtual IWorldQuery*               GetWorldQuery() = 0;

	virtual bool                       IsJustExchanging() = 0;

	ILINE void                         SetMovementController(IMovementController* pMC) { m_pMovementController = pMC; }
	virtual ILINE IMovementController* GetMovementController()                         { return m_pMovementController; }

	virtual void                       GetMemoryUsage(ICrySizer* pSizer) const         {};

#if GAME_OBJECT_SUPPORTS_CUSTOM_USER_DATA
	virtual void* GetUserData() const = 0;
	virtual void  SetUserData(void* ptr) = 0;
#endif

protected:
	enum EChangeExtension
	{
		eCE_Activate,
		eCE_Deactivate,
		eCE_Acquire,
		eCE_Release
	};

	IGameObject() : m_entityId(0), m_pMovementController(0) {}
	EntityId             m_entityId;
	IMovementController* m_pMovementController;

private:
	// change extension activation/reference somehow
	virtual IGameObjectExtension* ChangeExtension(const char* extension, EChangeExtension change) = 0;
};

struct IRMIAtSyncItem : public INetAtSyncItem, public IRMICppLogger {};

template<class T, class Obj>
class CRMIAtSyncItem : public IRMIAtSyncItem
{
public:
	typedef bool (Obj::* CallbackFunc)(const T&, INetChannel*);

	// INetAtSyncItem
	// INetAtSyncItem

	static ILINE CRMIAtSyncItem* Create(const T& params, EntityId id, const SGameObjectExtensionRMI* pRMI, CallbackFunc callback, INetChannel* pChannel)
	{
		return new(GetRMIAllocator<sizeof(CRMIAtSyncItem)>().Allocate())CRMIAtSyncItem(params, id, pRMI, callback, pChannel);
	}

	bool Sync()
	{
		bool ok = false;
		bool foundObject = false;
		char msg[256];
		msg[0] = 0;

		if (IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject(m_id))
		{
			INDENT_LOG_DURING_SCOPE(true, "During game object sync: %s %s", pGameObject->GetEntity()->GetEntityTextDescription().c_str(), m_pRMI->pMsgDef->description);

			if (Obj* pGameObjectExtension = (Obj*)pGameObject->GetExtensionWithRMIBase(m_pRMI->pBase))
			{
				ok = (pGameObjectExtension->*m_callback)(m_params, m_pChannel);
				foundObject = true;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR,
					"Game object extension with base %.8x for entity %s for RMI %s not found",
					(uint32)(TRUNCATE_PTR)m_pRMI->pBase, pGameObject->GetEntity()->GetName(),
					m_pRMI->pMsgDef->description);
			}
		}
		else
		{
			cry_sprintf(msg, "Entity %u for RMI %s not found", m_id, m_pRMI->pMsgDef->description);
		}

		if (!ok)
		{
			CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, 
				"Error handling RMI %s", m_pRMI->pMsgDef->description);

			if (!foundObject && !gEnv->bServer && !m_pChannel->IsInTransition())
			{
				CRY_ASSERT(msg[0]);
				m_pChannel->Disconnect(eDC_ContextCorruption, msg);
			}
			else
			{
				ok = true;
				// fake for singleplayer/multiplayer server
				// singleplayer - 'impossible' to get right during quick-load
				// multiplayer server - object can be deleted while the message is in flight
			}
		}

		if (!foundObject)
			return true; // for editor
		else
			return ok;
	}

	bool SyncWithError(EDisconnectionCause& disconnectCause, string& disconnectMessage)
	{
		return Sync();
	}

	void DeleteThis()
	{
		this->~CRMIAtSyncItem();
		GetRMIAllocator<sizeof(CRMIAtSyncItem)>().Deallocate(this);
	}
	// ~INetAtSyncItem

	// IRMICppLogger
	virtual const char* GetName()
	{
		return m_pRMI->description;
	}
	virtual void SerializeParams(TSerialize ser)
	{
		m_params.SerializeWith(ser);
	}
	// ~IRMICppLogger

private:
	CRMIAtSyncItem(const T& params, EntityId id, const SGameObjectExtensionRMI* pRMI, CallbackFunc callback, INetChannel* pChannel) : m_params(params), m_id(id), m_pRMI(pRMI), m_callback(callback), m_pChannel(pChannel) {}

	T                              m_params;
	EntityId                       m_id;
	const SGameObjectExtensionRMI* m_pRMI;
	CallbackFunc                   m_callback;
	INetChannel*                   m_pChannel;
};

struct IGameObject;
struct SViewParams;

template<class T_Derived, class T_Parent, size_t MAX_STATIC_MESSAGES = 64>
class CGameObjectExtensionHelper : public T_Parent
{
public:
	static void GetGameObjectExtensionRMIData(void** ppRMI, size_t* nCount)
	{
		*ppRMI = ms_statics.m_vMessages;
		*nCount = ms_statics.m_nMessages;
	}

	const void* GetRMIBase() const
	{
		return ms_statics.m_vMessages;
	}

	static void SetExtensionId(IGameObjectSystem::ExtensionID id) { ms_statics.m_extensionId = id; }

protected:

	static T_Derived* QueryExtension(EntityId id)
	{
		IGameObject* pGO = gEnv->pGameFramework->GetGameObject(id);
		if (pGO)
		{
			return static_cast<T_Derived*>(pGO->QueryExtension(T_Derived::ms_statics.m_extensionId));
		}

		return NULL;
	}

	static void ActivateOutputPort(EntityId id, int port, const TFlowInputData& data)
	{
		SEntityEvent evnt;
		evnt.event = ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT;
		evnt.nParam[0] = port;
		evnt.nParam[1] = (INT_PTR)&data;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity)
			pEntity->SendEvent(evnt);
	}

	static const SGameObjectExtensionRMI* Helper_AddMessage(SGameObjectExtensionRMI::DecoderFunction decoder, const char* description, ERMIAttachmentType attach, bool isServerCall, ENetReliabilityType reliability, bool lowDelay)
	{
		if (ms_statics.m_nMessages >= MAX_STATIC_MESSAGES)
		{
			// Assert or CryFatalError here uses gEnv, which is not yet initialized.
			__debugbreak();
			((void (*)())NULL)();
			return NULL;
		}
		SGameObjectExtensionRMI& rmi = ms_statics.m_vMessages[ms_statics.m_nMessages++];
		rmi.decoder = decoder;
		rmi.description = description;
		rmi.attach = attach;
		rmi.isServerCall = isServerCall;
		rmi.pBase = ms_statics.m_vMessages;
		rmi.reliability = reliability;
		rmi.pMsgDef = 0;
		rmi.lowDelay = lowDelay;
		return &rmi;
	}

private:
	struct Statics
	{
		size_t                         m_nMessages;
		SGameObjectExtensionRMI        m_vMessages[MAX_STATIC_MESSAGES];
		IGameObjectSystem::ExtensionID m_extensionId;
	};

	static Statics ms_statics;
};

#define DECLARE_RMI(name, params, reliability, attachment, isServer, lowDelay)                              \
  public:                                                                                                   \
    struct MethodInfo_ ## name                                                                              \
    {                                                                                                       \
      MethodInfo_ ## name(const SGameObjectExtensionRMI * pMethodInfo) { this->pMethodInfo = pMethodInfo; } \
      const SGameObjectExtensionRMI* pMethodInfo;                                                           \
      ILINE void Verify(const params &p) const                                                              \
      {                                                                                                     \
      }                                                                                                     \
    };                                                                                                      \
  private:                                                                                                  \
    static INetAtSyncItem* Decode_ ## name(TSerialize, EntityId*, INetChannel*);                            \
    bool Handle_ ## name(const params &, INetChannel*);                                                     \
    static const ERMIAttachmentType Attach_ ## name = attachment;                                           \
    static const bool ServerCall_ ## name = isServer;                                                       \
    static const ENetReliabilityType Reliability_ ## name = reliability;                                    \
    static const bool LowDelay_ ## name = lowDelay;                                                         \
    typedef params Params_ ## name;                                                                         \
    static MethodInfo_ ## name m_info ## name;                                                              \
  public:                                                                                                   \
    static const MethodInfo_ ## name& name() { return m_info ## name; }

#define DECLARE_INTERFACE_RMI(name, params, reliability, attachment, isServer, lowDelay)                    \
  protected:                                                                                                \
    static const ERMIAttachmentType Attach_ ## name = attachment;                                           \
    static const bool ServerCall_ ## name = isServer;                                                       \
    static const ENetReliabilityType Reliability_ ## name = reliability;                                    \
    static const bool LowDelay_ ## name = lowDelay;                                                         \
    typedef params Params_ ## name;                                                                         \
  public:                                                                                                   \
    struct MethodInfo_ ## name                                                                              \
    {                                                                                                       \
      MethodInfo_ ## name(const SGameObjectExtensionRMI * pMethodInfo) { this->pMethodInfo = pMethodInfo; } \
      const SGameObjectExtensionRMI* pMethodInfo;                                                           \
      ILINE void Verify(const params &p) const                                                              \
      {                                                                                                     \
      }                                                                                                     \
    };                                                                                                      \
    virtual const MethodInfo_ ## name& name() = 0

#define DECLARE_IMPLEMENTATION_RMI(name)                                  \
  private:                                                                \
    INetAtSyncItem * Decode_ ## name(TSerialize, EntityId, INetChannel*); \
    static bool Handle_ ## name(const Params_ ## name &, INetChannel*);   \
    static MethodInfo_ ## name m_info ## name;                            \
  public:                                                                 \
    const MethodInfo_ ## name& name() { return m_info ## name; }

#define IMPLEMENT_RMI(cls, name)                                                                                                                                                                                            \
  cls::MethodInfo_ ## name cls::m_info ## name = cls::Helper_AddMessage(&cls::Decode_ ## name, "RMI:" # cls ":" # name, cls::Attach_ ## name, cls::ServerCall_ ## name, cls::Reliability_ ## name, cls::LowDelay_ ## name); \
  INetAtSyncItem* cls::Decode_ ## name(TSerialize ser, EntityId * pID, INetChannel * pChannel)                                                                                                                              \
  {                                                                                                                                                                                                                         \
    CRY_ASSERT(pID);                                                                                                                                                                                                        \
    Params_ ## name params;                                                                                                                                                                                                 \
    params.SerializeWith(ser);                                                                                                                                                                                              \
    NetLogRMIReceived(params, pChannel);                                                                                                                                                                                    \
    return CRMIAtSyncItem<Params_ ## name, cls>::Create(params, *pID, m_info ## name.pMethodInfo, &cls::Handle_ ## name, pChannel);                                                                                         \
  }                                                                                                                                                                                                                         \
  ILINE bool cls::Handle_ ## name(const Params_ ## name & params, INetChannel * pNetChannel)

#define IMPLEMENT_INTERFACE_RMI(cls, name)                                                                                                                                                                                  \
  cls::MethodInfo_ ## name cls::m_info ## name = cls::Helper_AddMessage(&cls::Decode_ ## name, "RMI:" # cls ":" # name, cls::Attach_ ## name, cls::ServerCall_ ## name, cls::Reliability_ ## name, cls::LowDelay_ ## name); \
  INetAtSyncItem* cls::Decode_ ## name(TSerialize ser, INetChannel * pChannel)                                                                                                                                              \
  {                                                                                                                                                                                                                         \
    Params_ ## name params;                                                                                                                                                                                                 \
    params.SerializeWith(ser);                                                                                                                                                                                              \
    return CRMIAtSyncItem<Params_ ## name, cls>::Create(params, id, m_info ## name.pMethodInfo, &cls::Handle_ ## name, pChannel);                                                                                           \
  }                                                                                                                                                                                                                         \
  ILINE bool cls::Handle_ ## name(const Params_ ## name & params, INetChannel * pNetChannel)

/*
 * _FAST versions may send the RMI without waiting for the frame to end; be sure that consistency with the entity is not important!
 */

//
// PreAttach/PostAttach RMI's cannot have their reliability specified (see CGameObjectDispatch::RegisterInterface() for details)
#define DECLARE_SERVER_RMI_PREATTACH(name, params)             DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PreAttach, true, false)
#define DECLARE_CLIENT_RMI_PREATTACH(name, params)             DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PreAttach, false, false)
#define DECLARE_SERVER_RMI_POSTATTACH(name, params)            DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PostAttach, true, false)
#define DECLARE_CLIENT_RMI_POSTATTACH(name, params)            DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PostAttach, false, false)
#define DECLARE_SERVER_RMI_NOATTACH(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_NoAttach, true, false)
#define DECLARE_CLIENT_RMI_NOATTACH(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_NoAttach, false, false)

// PreAttach/PostAttach RMI's cannot have their reliability specified (see CGameObjectDispatch::RegisterInterface() for details)
#define DECLARE_SERVER_RMI_PREATTACH_FAST(name, params)             DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PreAttach, true, true)
#define DECLARE_CLIENT_RMI_PREATTACH_FAST(name, params)             DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PreAttach, false, true)
#define DECLARE_SERVER_RMI_POSTATTACH_FAST(name, params)            DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PostAttach, true, true)
#define DECLARE_CLIENT_RMI_POSTATTACH_FAST(name, params)            DECLARE_RMI(name, params, eNRT_UnreliableOrdered, eRAT_PostAttach, false, true)
#define DECLARE_SERVER_RMI_NOATTACH_FAST(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_NoAttach, true, true)
#define DECLARE_CLIENT_RMI_NOATTACH_FAST(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_NoAttach, false, true)

#if ENABLE_URGENT_RMIS
	#define DECLARE_SERVER_RMI_URGENT(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_Urgent, true, false)
	#define DECLARE_CLIENT_RMI_URGENT(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_Urgent, false, false)
#else
	#define DECLARE_SERVER_RMI_URGENT(name, params, reliability) DECLARE_SERVER_RMI_NOATTACH(name, params, reliability)
	#define DECLARE_CLIENT_RMI_URGENT(name, params, reliability) DECLARE_CLIENT_RMI_NOATTACH(name, params, reliability)
#endif // ENABLE_URGENT_RMIS

#if ENABLE_INDEPENDENT_RMIS
	#define DECLARE_SERVER_RMI_INDEPENDENT(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_Independent, true, false)
	#define DECLARE_CLIENT_RMI_INDEPENDENT(name, params, reliability) DECLARE_RMI(name, params, reliability, eRAT_Independent, false, false)
#else
	#define DECLARE_SERVER_RMI_INDEPENDENT(name, params, reliability) DECLARE_SERVER_RMI_NOATTACH(name, params, reliability)
	#define DECLARE_CLIENT_RMI_INDEPENDENT(name, params, reliability) DECLARE_CLIENT_RMI_NOATTACH(name, params, reliability)
#endif // ENABLE_INDEPENDENT_RMIS

/*
   // Todo:
   //		Temporary, until a good solution for sending noattach fast messages can be found
   #define DECLARE_SERVER_RMI_NOATTACH_FAST(a,b,c) DECLARE_SERVER_RMI_NOATTACH(a,b,c)
   #define DECLARE_CLIENT_RMI_NOATTACH_FAST(a,b,c) DECLARE_CLIENT_RMI_NOATTACH(a,b,c)
 */

//
#define DECLARE_INTERFACE_SERVER_RMI_PREATTACH(name, params, reliability)       DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PreAttach, true, false)
#define DECLARE_INTERFACE_CLIENT_RMI_PREATTACH(name, params, reliability)       DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PreAttach, false, false)
#define DECLARE_INTERFACE_SERVER_RMI_POSTATTACH(name, params, reliability)      DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PostAttach, true, false)
#define DECLARE_INTERFACE_CLIENT_RMI_POSTATTACH(name, params, reliability)      DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PostAttach, false, false)

#define DECLARE_INTERFACE_SERVER_RMI_PREATTACH_FAST(name, params, reliability)  DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PreAttach, true, true)
#define DECLARE_INTERFACE_CLIENT_RMI_PREATTACH_FAST(name, params, reliability)  DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PreAttach, false, true)
#define DECLARE_INTERFACE_SERVER_RMI_POSTATTACH_FAST(name, params, reliability) DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PostAttach, true, true)
#define DECLARE_INTERFACE_CLIENT_RMI_POSTATTACH_FAST(name, params, reliability) DECLARE_INTERFACE_RMI(name, params, reliability, eRAT_PostAttach, false, true)

template<class T, class U, size_t N>
typename CGameObjectExtensionHelper<T, U, N>::Statics CGameObjectExtensionHelper<T, U, N>::ms_statics;

struct IGameObjectView
{
	virtual ~IGameObjectView(){}
	virtual void UpdateView(SViewParams& params) = 0;
	virtual void PostUpdateView(SViewParams& params) = 0;
};

// Declare game object as deprecated except for the legacy modules that expose functionality for it
#if !defined(eCryModule) || (eCryModule != eCryM_GameFramework && eCryModule != eCryM_LegacyGameDLL \
	&& eCryModule != eCryM_FlowGraph && eCryModule != eCryM_Editor && eCryModule != eCryM_Movie     \
	&& eCryModule != eCryM_Legacy && eCryModule != eCryM_Schematyc2)

	#define CRY_DEPRECATED_GAMEOBJECT CRY_DEPRECATED("(v5.4) IGameObjectExtension has been replaced with IEntityComponent, and will be removed in a future update.")

#else
	#define CRY_DEPRECATED_GAMEOBJECT
#endif

// Summary
//   Interface used to implement legacy game object extensions, superseded by IEntityComponent
struct IGameObjectExtension : public IEntityComponent
{
	CRY_DEPRECATED_GAMEOBJECT IGameObjectExtension() : m_pGameObject(0) {}

	// IEntityComponent
	//! GetEventMask implementation is now required for the legacy game object extensions, as a valid value is now needed for performance reasons.
	//! Return the mask (e.g. ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) of all events you expect to process in ProcessEvent.
	virtual uint64 GetEventMask() const override = 0;
	virtual void Initialize() override {};
	virtual void NetReplicateSerialize(TSerialize ser) override
	{
		if (ser.IsReading())
		{
			SerializeSpawnInfo(ser);
		}
		else
		{
			if (ISerializableInfoPtr pSpawnInfo = GetSpawnInfo())
			{
				pSpawnInfo->SerializeWith(ser);
			}
		}
	}
	// ~IEntityComponent

	// Summary
	//   Initialize the extension
	// Parameters
	//   pGameObject - a pointer to the game object which will use the extension
	// Remarks
	//   IMPORTANT: It's very important that the implementation of this function
	//   call the protected function SetGameObject() during the execution of the
	//   Init() function. Unexpected results would happen otherwise.
	virtual bool Init(IGameObject* pGameObject) = 0;

	// Summary
	//   Post-initialize the extension
	// Description
	//   During the post-initialization, the extension is now contained in the
	//   game object
	// Parameters
	//   pGameObject - a pointer to the game object which owns the extension
	virtual void PostInit(IGameObject* pGameObject) = 0;

	// Summary
	//   Initialize the extension (client only)
	// Description
	//   This initialization function should be use to initialize resource only
	//   used in the client
	// Parameters
	//   channelId - id of the server channel of the client to receive the
	//               initialization
	virtual void InitClient(int channelId) = 0;

	// Summary
	//   Post-initialize the extension (client only)
	// Description
	//   This initialization function should be use to initialize resource only
	//   used in the client. During the post-initialization, the extension is now
	//   contained in the game object
	// Parameters
	//   channelId - id of the server channel of the client to receive the
	//               initialization
	virtual void PostInitClient(int channelId) = 0;

	// Summary
	//   Reload the extension
	// Description
	//   Called when owning entity is reloaded
	// Parameters
	//   pGameObject - a pointer to the game object which owns the extension
	// Returns
	//   TRUE if the extension should be kept, FALSE if it should be removed
	// Remarks
	//   IMPORTANT: It's very important that the implementation of this function
	//   call the protected function ResetGameObject() during the execution of the
	//   ReloadExtension() function. Unexpected results would happen otherwise.
	virtual bool ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) = 0;

	// Summary
	//   Post-reload the extension
	// Description
	//   Called when owning entity is reloaded and all its extensions have either
	//   either been reloaded or destroyed
	// Parameters
	//   pGameObject - a pointer to the game object which owns the extension
	virtual void PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) = 0;

	// Summary
	//   Performs the serialization the extension
	// Parameters
	//   ser - object used to serialize values
	//   aspect - serialization aspect, used for network serialization
	//   profile - which profile to serialize; 255 == don't care
	//   flags - physics flags to be used for serialization
	// See Also
	//   ISerialize
	virtual void FullSerialize(TSerialize ser) = 0;
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) override = 0;

	// Summary
	//   Return the aspects NetSerialize serializes.
	//   Overriding this to return only the aspects used will speed up the net bind of the object.
	virtual NetworkAspectType GetNetSerializeAspects() { return eEA_All; }

	// Summary
	//   Performs post serialization fixes
	virtual void PostSerialize() = 0;

	// Summary
	//   Performs the serialization of special spawn information
	// Parameters
	//   ser - object used to serialize values
	// See Also
	//   Serialize, ISerialize
	virtual void                 SerializeSpawnInfo(TSerialize ser) = 0;

	virtual ISerializableInfoPtr GetSpawnInfo() = 0;

	// Summary
	//   Performs frame dependent extension updates
	// Parameters
	//   ctx - Update context
	//   updateSlot - updateSlot
	// See Also
	//   PostUpdate, SEntityUpdateContext, IGameObject::EnableUpdateSlot
	virtual void Update(SEntityUpdateContext& ctx, int updateSlot) = 0;

	// Summary
	//   Processes game specific events
	// Parameters
	//   event - game event
	// See Also
	//   SGameObjectEvent
	virtual void HandleEvent(const SGameObjectEvent& event) = 0;

	virtual void GameSerialize(TSerialize ser ) override { FullSerialize(ser); };                         //!< From IEntityComponent

	virtual void SetChannelId(uint16 id) = 0;

	// Summary
	//   Retrieves the RMI Base pointer
	// Description
	//   Internal function used for RMI. It's usually implemented by
	//   CGameObjectExtensionHelper provides a way of checking who should
	//   receive some RMI call.
	virtual const void* GetRMIBase() const = 0;

	// Summary
	//   Performs an additional update
	// Parameters
	//   frameTime - time elapsed since the last frame update
	// See Also
	//   Update, IGameObject::EnablePostUpdates, IGameObject::DisablePostUpdates
	virtual void PostUpdate(float frameTime) = 0;

	// Summary
	virtual void PostRemoteSpawn() = 0;

	// Summary
	//   Retrieves the pointer to the game object
	// Returns
	//   A pointer to the game object which hold this extension
	ILINE IGameObject* GetGameObject() const { return m_pGameObject; }

protected:
	void SetGameObject(IGameObject* pGameObject)
	{
		m_pGameObject = pGameObject;
	}

	// Kept around for legacy reasons
	void ResetGameObject() {}

private:
	IGameObject* m_pGameObject;
};

#define CHANGED_NETWORK_STATE(object, aspects)     do { /* IEntity * pEntity = object->GetGameObject()->GetEntity(); CryLogAlways("%s changed aspect %x (%s %d)", pEntity ? pEntity->GetName() : "NULL", aspects, __FILE__, __LINE__); */ object->GetGameObject()->ChangedNetworkState(aspects); } while (0)
#define CHANGED_NETWORK_STATE_GO(object, aspects)  do { /* IEntity * pEntity = object->GetEntity(); CryLogAlways("%s changed aspect %x (%s %d)", pEntity ? pEntity->GetName() : "NULL", aspects, __FILE__, __LINE__); */ object->ChangedNetworkState(aspects); } while (0)
#define CHANGED_NETWORK_STATE_REF(object, aspects) do { /* IEntity * pEntity = object.GetGameObject()->GetEntity(); CryLogAlways("%s changed aspect %x (%s %d)", pEntity ? pEntity->GetName() : "NULL", aspects, __FILE__, __LINE__); */ object.GetGameObject()->ChangedNetworkState(aspects); } while (0)

#endif
