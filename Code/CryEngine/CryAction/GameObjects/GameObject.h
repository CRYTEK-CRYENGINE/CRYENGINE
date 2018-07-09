// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IViewSystem.h>
#include <IActionMapManager.h>
#include <IGameObject.h>
#include <CryCore/BitFiddling.h>

class CGameObjectSystem;
class CGameObject;

struct SBasicSpawnParams : public ISerializable
{
	string  name;
	uint16  classId;
	Vec3    pos;
	Quat    rotation;
	Vec3    scale;
	bool    bClientActor;
	uint16  nChannelId;
	uint32  flags;
	CryGUID baseComponent;

	virtual void SerializeWith(TSerialize ser)
	{
		if (ser.GetSerializationTarget() == eST_Network)
		{
			ser.Value("name", name, 'sstr');
			ser.Value("classId", classId, 'clas');
			ser.Value("pos", pos, 'spos');
			bool bHasScale = false;
			Vec3 vScale(1.0f, 1.0f, 1.0f);

			if (ser.IsWriting())
			{
				bHasScale = (scale.x != 1.0f) || (scale.y != 1.0f) || (scale.z != 1.0f);
				vScale = scale;
			}

			//As this is used in an RMI, we can branch on bHasScale and save ourselves 96bits in the
			//	vast majority of cases, at the cost of a single bit.
			ser.Value("hasScale", bHasScale, 'bool');

			if (bHasScale)
			{
				//We can't just use a scalar here. Some (very few) objects have non-uniform scaling.
				ser.Value("scale", vScale, 'sscl');
			}

			scale = vScale;

			ser.Value("rotation", rotation, 'srot');
			ser.Value("bClientActor", bClientActor, 'bool');
			ser.Value("nChannelId", nChannelId, 'schl');
			ser.Value("flags", flags, 'ui32');
			ser.Value("guid_hi", baseComponent.hipart);
			ser.Value("guid_lo", baseComponent.lopart);
		}
		else
		{
			ser.Value("name", name);
			ser.Value("classId", classId);
			ser.Value("pos", pos);
			ser.Value("scale", scale);
			ser.Value("rotation", rotation);
			ser.Value("bClientActor", bClientActor);
			ser.Value("nChannelId", nChannelId);
			ser.Value("flags", flags, 'ui32');
		}
	}
};

#if 0
// deprecated and won't compile at all...

struct SDistanceChecker
{
	SDistanceChecker()
		: m_distanceChecker(0)
		, m_distanceCheckerTimer(0.0f)
	{

	}

	void           Init(CGameObjectSystem* pGameObjectSystem, EntityId receiverId);
	void           Reset();
	void           Update(CGameObject& owner, float frameTime);

	ILINE EntityId GetDistanceChecker() const { return m_distanceChecker; }

private:
	EntityId m_distanceChecker;
	float    m_distanceCheckerTimer;
};
#else
struct SDistanceChecker
{
	ILINE void     Init(CGameObjectSystem* pGameObjectSystem, EntityId receiverId) {};
	ILINE void     Reset()                                                         {};
	ILINE void     Update(CGameObject& owner, float frameTime)                     {};

	ILINE EntityId GetDistanceChecker() const                                      { return 0; }
};
#endif

struct IGOUpdateDbg;

class CGameObject : public IGameObject
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CGameObject, "GameObject", "ec4e2fdc-dcff-4ab3-a691-b9cc4ece5788"_cry_guid);

public:
	CGameObject();
	virtual ~CGameObject();

	static void  CreateCVars();

	void         OnInitEvent();
	virtual void Update(SEntityUpdateContext& ctx);

	// IEntityComponent
	virtual EEntityProxy           GetProxyType() const final { return ENTITY_PROXY_USER; };
	virtual void                   Initialize() final;
	virtual void                   OnShutDown() final;
	virtual void                   Release() final;
	virtual void                   ProcessEvent(const SEntityEvent& event) final;
	virtual uint64                 GetEventMask() const final;
	virtual ComponentEventPriority GetEventPriority() const override;

	virtual NetworkAspectType      GetNetSerializeAspectMask() const override;
	virtual bool                   NetSerializeEntity(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;

	// we have gained (or lost) control of this object
	virtual void SetAuthority(bool auth) override
	{
		m_pNetEntity->SetAuthority(auth);
	}
	virtual bool HasAuthority() const override
	{
		return m_pNetEntity->HasAuthority();
	}

	virtual void GameSerialize(TSerialize ser) final;
	virtual bool NeedGameSerialize() final;
	// ~IEntityComponent

	// IActionListener
	virtual void OnAction(const ActionId& actionId, int activationMode, float value) override;
	virtual void AfterAction() override;
	// ~IActionListener

	// IGameObject
	virtual bool BindToNetwork(EBindToNetworkMode) override;
	virtual bool                  BindToNetworkWithParent(EBindToNetworkMode mode, EntityId parentId) override;
	virtual void                  ChangedNetworkState(NetworkAspectType aspects) override { MarkAspectsDirty(aspects); };
	virtual void                  MarkAspectsDirty(NetworkAspectType aspects) override;
	virtual void                  EnableAspect(NetworkAspectType aspects, bool enable) override;
	virtual void                  EnableDelegatableAspect(NetworkAspectType aspects, bool enable) override;
	virtual IGameObjectExtension* QueryExtension(const char* extension) const override;
	virtual IGameObjectExtension* QueryExtension(IGameObjectSystem::ExtensionID id) const override;

	virtual bool                  SetExtensionParams(const char* extension, SmartScriptTable params) override;
	virtual bool                  GetExtensionParams(const char* extension, SmartScriptTable params) override;
	virtual IGameObjectExtension* ChangeExtension(const char* extension, EChangeExtension change) override;
	virtual void                  SendEvent(const SGameObjectEvent&) override;
	virtual void                  SetChannelId(uint16 id) override;
	virtual uint16                GetChannelId() const override;
	virtual bool                  CaptureView(IGameObjectView* pGOV) override;
	virtual void                  ReleaseView(IGameObjectView* pGOV) override;
	virtual bool                  CaptureActions(IActionListener* pAL) override;
	virtual void                  ReleaseActions(IActionListener* pAL) override;
	virtual bool                  CaptureProfileManager(IGameObjectProfileManager* pPH) override;
	virtual void                  ReleaseProfileManager(IGameObjectProfileManager* pPH) override;
	virtual bool                  HasProfileManager() override;
	virtual void                  ClearProfileManager() override;
	virtual void                  EnableUpdateSlot(IGameObjectExtension* pExtension, int slot) override;
	virtual void                  DisableUpdateSlot(IGameObjectExtension* pExtension, int slot) override;
	virtual uint8                 GetUpdateSlotEnables(IGameObjectExtension* pExtension, int slot) override;
	virtual void                  EnablePostUpdates(IGameObjectExtension* pExtension) override;
	virtual void                  DisablePostUpdates(IGameObjectExtension* pExtension) override;
	virtual void                  PostUpdate(float frameTime) override;
	virtual void                  FullSerialize(TSerialize ser) override;
	virtual void                  PostSerialize() override;
	virtual void                  SetUpdateSlotEnableCondition(IGameObjectExtension* pExtension, int slot, EUpdateEnableCondition condition) override;
	virtual bool                  IsProbablyVisible() override;
	virtual bool                  IsProbablyDistant() override;
	virtual bool                  SetAspectProfile(EEntityAspects aspect, uint8 profile, bool fromNetwork) override;
	virtual uint8                 GetAspectProfile(EEntityAspects aspect) override;
	virtual IWorldQuery*          GetWorldQuery() override;
	virtual IMovementController*  GetMovementController() override;
	virtual IGameObjectExtension* GetExtensionWithRMIBase(const void* pBase) override;
	virtual void                  AttachDistanceChecker() override;
	virtual void                  ForceUpdate(bool force) override;
	virtual void                  ForceUpdateExtension(IGameObjectExtension* pExt, int slot) override;
	virtual void                  Pulse(uint32 pulse) override;
	virtual void                  RegisterAsPredicted() override;
	virtual void                  RegisterAsValidated(IGameObject* pGO, int predictionHandle) override;
	virtual int                   GetPredictionHandle() override;
	virtual void                  RequestRemoteUpdate(NetworkAspectType aspectMask) override;
	virtual void                  RegisterExtForEvents(IGameObjectExtension* piExtention, const int* pEvents, const int numEvents) override;
	virtual void                  UnRegisterExtForEvents(IGameObjectExtension* piExtention, const int* pEvents, const int numEvents) override;

	virtual void                  EnablePhysicsEvent(bool enable, int event) override
	{
		if (enable)
			m_enabledPhysicsEvents = m_enabledPhysicsEvents | event;
		else
			m_enabledPhysicsEvents = m_enabledPhysicsEvents & (~event);
	}
	virtual bool WantsPhysicsEvent(int event) override { return (m_enabledPhysicsEvents & event) != 0; };
	virtual void SetNetworkParent(EntityId id) override;

	virtual bool IsJustExchanging() override { return m_justExchanging; };
	virtual bool SetAIActivation(EGameObjectAIActivationMode mode) override;
	virtual void SetAutoDisablePhysicsMode(EAutoDisablePhysicsMode mode) override;
	virtual void EnablePrePhysicsUpdate(EPrePhysicsUpdate updateRule) override;
	// needed for debug
	virtual bool ShouldUpdate() override;
	// ~IGameObject

	virtual void     UpdateView(SViewParams& viewParams);
	virtual void     PostUpdateView(SViewParams& viewParams);
	virtual void     HandleEvent(const SGameObjectEvent& evt);
	virtual bool     CanUpdateView() const { return m_pViewDelegate != NULL; }
	IGameObjectView* GetViewDelegate()     { return m_pViewDelegate; }

#if GAME_OBJECT_SUPPORTS_CUSTOM_USER_DATA
	virtual void* GetUserData() const override;
	virtual void  SetUserData(void* ptr) override;
#endif

	virtual bool IsAspectDelegatable(NetworkAspectType aspect) const override;

	//----------------------------------------------------------------------
	// Network related functions

	virtual void         InitClient(int channelId);
	virtual void         PostInitClient(int channelId);

	NetworkAspectType    GetEnabledAspects() const override;
	uint8                GetDefaultProfile(EEntityAspects aspect) override;

	// called from CGameObject::BoundObject -- we have become bound on a client
	void         BecomeBound() override;
	bool         IsBoundToNetwork() const override;

	void         FlushActivatableExtensions() { FlushExtensions(false); }

	void         GetMemoryUsage(ICrySizer* s) const override;

	virtual void DontSyncPhysics() override;

	void         AcquireMutex();
	void         ReleaseMutex();

	// INetEntity-specific, not needed for CGameObject. 
	virtual void      RmiRegister(const SRmiHandler& handler) override {};
	virtual SRmiIndex RmiByDecoder(SRmiHandler::DecoderF decoder, SRmiHandler **handler) override
	{
		return SRmiIndex(0);
	};
	virtual SRmiHandler::DecoderF RmiByIndex(const SRmiIndex idx) override
	{
		return nullptr;
	};

	virtual void OnNetworkedEntityTransformChanged(EntityTransformationFlagsMask transformReasons) override;

	virtual void OnComponentAddedDuringInitialization(IEntityComponent* pComponent) const override { return m_pNetEntity->OnComponentAddedDuringInitialization(pComponent); }
	virtual void OnEntityInitialized() override { m_pNetEntity->OnEntityInitialized(); }

private:
	INetEntity*      m_pNetEntity;

	IActionListener* m_pActionDelegate;

	IGameObjectView* m_pViewDelegate;
	IView*           m_pView;

#if GAME_OBJECT_SUPPORTS_CUSTOM_USER_DATA
	void* m_pUserData;
#endif

	// Need a mutex to defend shutdown against event handling.
	CryMutex m_mutex;

	template<class T> bool DoGetSetExtensionParams(const char* extension, SmartScriptTable params);

	// any extensions (extra GameObject functionality) goes here
	struct SExtension
	{
		SExtension()
			: pExtension()
			, id(0)
			, refCount(0)
			, activated(false)
			, sticky(false)
			, postUpdate(false)
			, flagUpdateWhenVisible(0)
			, flagUpdateWhenInRange(0)
			, flagUpdateCombineOr(0)
			, flagDisableWithAI(0)
			, flagNeverUpdate(0)
			, eventReg(0)
		{
			uint32 slotbit = 1;
			for (uint32 i = 0; i < MAX_UPDATE_SLOTS_PER_EXTENSION; ++i)
			{
				updateEnables[i] = forceEnables[i] = 0;
				flagDisableWithAI += slotbit;
				slotbit <<= 1;
			}
		}

		// extension by flag event registration
		uint64                         eventReg;
		IGameObjectExtension*          pExtension;
		IGameObjectSystem::ExtensionID id;
		// refCount is the number of AcquireExtensions pending ReleaseExtensions
		uint8                          refCount;
		uint8                          updateEnables[MAX_UPDATE_SLOTS_PER_EXTENSION];
		uint8                          forceEnables[MAX_UPDATE_SLOTS_PER_EXTENSION];
		// upper layers only get to activate/deactivate extensions
		uint8                          flagUpdateWhenVisible: MAX_UPDATE_SLOTS_PER_EXTENSION;
		uint8                          flagUpdateWhenInRange: MAX_UPDATE_SLOTS_PER_EXTENSION;
		uint8                          flagUpdateCombineOr: MAX_UPDATE_SLOTS_PER_EXTENSION;
		uint8                          flagDisableWithAI: MAX_UPDATE_SLOTS_PER_EXTENSION;
		uint8                          flagNeverUpdate: MAX_UPDATE_SLOTS_PER_EXTENSION;
		bool                           activated  : 1;
		bool                           sticky     : 1;
		bool                           postUpdate : 1;

		bool operator<(const SExtension& rhs) const
		{
			return id < rhs.id;
		}

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(pExtension);
		}
	};

	static const int  MAX_ADDING_EXTENSIONS = 8;
	static SExtension m_addingExtensions[MAX_ADDING_EXTENSIONS];
	static int        m_nAddingExtension;

	typedef std::vector<SExtension> TExtensions;
	TExtensions m_extensions;
	bool        m_inRange             : 1;
	bool        m_justExchanging      : 1;
	bool        m_bVisible            : 1;
	bool        m_bPrePhysicsEnabled  : 1;
	bool        m_bPhysicsDisabled    : 1;
	bool        m_bNeedsNetworkRebind : 1;
	bool        m_bOnInitEventCalled  : 1;
	bool        m_bShouldUpdate       : 1;
	enum EUpdateState
	{
		eUS_Visible_Close = 0,
		eUS_Visible_FarAway,
		eUS_NotVisible_Close,
		eUS_NotVisible_FarAway,
		eUS_CheckVisibility_Close,
		eUS_CheckVisibility_FarAway,
		eUS_COUNT_STATES,
		eUS_INVALID = eUS_COUNT_STATES
	};
	uint                  m_updateState: CompileTimeIntegerLog2_RoundUp<eUS_COUNT_STATES>::result;
	uint                  m_aiMode: CompileTimeIntegerLog2_RoundUp<eGOAIAM_COUNT_STATES>::result;
	uint                  m_physDisableMode: CompileTimeIntegerLog2_RoundUp<eADPM_COUNT_STATES>::result;

	IGameObjectExtension* m_pGameObjectExtensionCachedKey;
	SExtension*           m_pGameObjectExtensionCachedValue;
	void        ClearCache() { m_pGameObjectExtensionCachedKey = nullptr; m_pGameObjectExtensionCachedValue = nullptr; }
	SExtension* GetExtensionInfo(IGameObjectExtension* pExt)
	{
		CRY_ASSERT(pExt);
		if (m_pGameObjectExtensionCachedKey == pExt)
		{
			CRY_ASSERT(m_pGameObjectExtensionCachedValue->pExtension == pExt);
			return m_pGameObjectExtensionCachedValue;
		}
		for (TExtensions::iterator iter = m_extensions.begin(); iter != m_extensions.end(); ++iter)
		{
			if (iter->pExtension == pExt)
			{
				m_pGameObjectExtensionCachedKey = iter->pExtension;
				m_pGameObjectExtensionCachedValue = &*iter;
				return &*iter;
			}
		}
		return 0;
	}

	enum EUpdateStateEvent
	{
		eUSE_BecomeVisible = 0,
		eUSE_BecomeClose,
		eUSE_BecomeFarAway,
		eUSE_Timeout,
		eUSE_COUNT_EVENTS,
	};
	float             m_updateTimer;

	SDistanceChecker  m_distanceChecker;

	int               m_enabledPhysicsEvents;
	int               m_forceUpdate;
	int               m_predictionHandle;

	EPrePhysicsUpdate m_prePhysicsUpdateRule;

	void FlushExtensions(bool includeStickyBits);
	bool ShouldUpdateSlot(const SExtension* pExt, uint32 slot, uint32 slotbit, bool checkAIDisable);
	void EvaluateUpdateActivation();
	void DebugUpdateState();
	bool ShouldUpdateAI();
	void RemoveExtension(const TExtensions::iterator& iter);
	void UpdateStateEvent(EUpdateStateEvent evt);
	bool TestIsProbablyVisible(uint state);
	bool TestIsProbablyDistant(uint state);
	bool DoSetAspectProfile(EEntityAspects aspect, uint8 profile, bool fromNetwork);
	void SetActivation(bool activate);
	void SetPhysicsDisable(bool disablePhysics);
	void PostRemoteSpawn();

	static const float        UpdateTimeouts[eUS_COUNT_STATES];
	static const EUpdateState UpdateTransitions[eUS_COUNT_STATES][eUSE_COUNT_EVENTS];
	static const char*        UpdateNames[eUS_COUNT_STATES];
	static const char*        EventNames[eUSE_COUNT_EVENTS];

	static CGameObjectSystem* m_pGOS;
};
