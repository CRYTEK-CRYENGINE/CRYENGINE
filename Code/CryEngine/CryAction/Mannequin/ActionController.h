// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ACTIONCONTROLLER_H__
#define __ACTIONCONTROLLER_H__

#include "ICryMannequin.h"
struct SScopeContext;
class CActionScope;

#ifdef _RELEASE
	#define CRYMANNEQUIN_WARN_ABOUT_VALIDITY() 0
#else
	#define CRYMANNEQUIN_WARN_ABOUT_VALIDITY() 1
#endif

enum EFlushMethod
{
	FM_Normal,
	FM_NormalLeaveAnimations,
	FM_Failure,
};

ILINE uint64 GetLeastSignificantBit(ActionScopes scopeMask)
{
	const uint64 mask = (uint64)scopeMask;
	static const uint64 MultiplyDeBruijnBitPosition[64] = 
	{
		0,  1,  2, 53,  3,  7, 54, 27,
		4, 38, 41,  8, 34, 55, 48, 28,
		62,  5, 39, 46, 44, 42, 22,  9,
		24, 35, 59, 56, 49, 18, 29, 11,
		63, 52,  6, 26, 37, 40, 33, 47,
		61, 45, 43, 21, 23, 58, 17, 10,
		51, 25, 36, 32, 60, 20, 57, 16,
		50, 31, 19, 15, 30, 14, 13, 12,
	};
	return MultiplyDeBruijnBitPosition[((uint64)((mask & ~(mask-1)) * 0x022fdd63cc95386d)) >> 58];
}

class CActionController : public IActionController
{
public:
	typedef std::vector<CActionController*> TActionControllerList;

	CActionController(IEntity* pEntity, SAnimationContext& context);

	// -- IActionController implementation --------------------------------------

	~CActionController();

	virtual void   OnEvent(const SGameObjectEvent& event) override;
	virtual void   OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event) override;

	virtual void   Reset() override;
	virtual void   Flush() override;

	virtual uint32 GetTotalScopes() const override
	{
		return m_scopeCount;
	}
	virtual void SetScopeContext(uint32 scopeContextID, IEntity& entity, ICharacterInstance* pCharacter, const IAnimationDatabase* animDatabase) override;
	virtual void ClearScopeContext(uint32 scopeContextID, bool flushAnimations = true) override;

	virtual bool IsScopeActive(uint32 scopeID) const override
	{
		CRY_ASSERT_MESSAGE((scopeID < m_scopeCount), "Invalid scope id");

		return ((m_activeScopes & BIT64(scopeID)) != 0);
	}
	virtual ActionScopes GetActiveScopeMask() const override
	{
		return m_activeScopes;
	}

	virtual IEntity& GetEntity() const override
	{
		CRY_ASSERT(m_cachedEntity);
		return *m_cachedEntity;
	}

	virtual EntityId GetEntityId() const override
	{
		return m_entityId;
	}

	virtual IScope*       GetScope(uint32 scopeID) override;
	virtual const IScope* GetScope(uint32 scopeID) const override;

	virtual uint32        GetScopeID(const char* name) const override
	{
		int scopeID = m_context.controllerDef.m_scopeIDs.Find(name);

		if (scopeID >= 0)
		{
			return scopeID;
		}
		else
		{
			return SCOPE_ID_INVALID;
		}
	}

	virtual TagID GetGlobalTagID(uint32 crc) const override
	{
		return GetContext().state.GetDef().Find(crc);
	}

	virtual TagID GetFragTagID(FragmentID fragID, uint32 crc) const override
	{
		if (fragID != FRAGMENT_ID_INVALID)
		{
			return m_context.controllerDef.GetFragmentTagDef(fragID)->Find(crc);
		}
		return TAG_ID_INVALID;
	}

	virtual FragmentID GetFragID(uint32 crc) const override
	{
		return m_context.controllerDef.m_fragmentIDs.Find(crc);
	}

	virtual const CTagDefinition* GetTagDefinition(FragmentID fragID) const override
	{
		if (fragID != FRAGMENT_ID_INVALID)
		{
			return m_context.controllerDef.GetFragmentTagDef(fragID);
		}
		return NULL;
	}

	virtual void               Queue(IAction& action, float time = -1.0f) override;
	virtual void               Requeue(IAction& action) override;

	virtual void               Update(float timePassed) override;

	virtual SAnimationContext& GetContext() override
	{
		return m_context;
	}

	virtual const SAnimationContext& GetContext() const override
	{
		return m_context;
	}

	virtual void Pause() override;
	virtual void Resume(uint32 resumeFlags = IActionController::ERF_Default) override;

	virtual void SetFlag(EActionControllerFlags flag, bool enable) override
	{
		if (enable)
			m_flags |= flag;
		else
			m_flags &= ~flag;
	}

	bool GetFlag(EActionControllerFlags flag) const
	{
		return (m_flags & flag) != 0;
	}

	virtual void  SetTimeScale(float timeScale) override;
	virtual float GetTimeScale() const override { return m_timeScale; }

#ifndef _RELEASE

	bool DebugFragments(bool isRootScope)
	{
		return ((s_mnDebugFragments > 0) && GetFlag(AC_DebugDraw) && (isRootScope || (s_mnDebugFragments == 2)));
	}

	void GetStateString(string& state) const;

#endif   //_RELEASE

	// Only needed for animationgraph?
	virtual bool                      IsActionPending(uint32 userToken) const override;

	virtual bool                      CanInstall(const IAction& action, const ActionScopes& scopeMask, float timeStep, float& timeTillInstall) const override;

	virtual bool                      QueryDuration(IAction& action, float& fragmentDuration, float& transitionDuration) const override;

	virtual void                      SetSlaveController(IActionController& target, uint32 targetContext, bool enslave, const IAnimationDatabase* piOptionTargetDatabase) override;
	void                              FlushSlaveController(IActionController& target);

	virtual void                      RegisterListener(IMannequinListener* listener) override;
	virtual void                      UnregisterListener(IMannequinListener* listener) override;

	virtual IProceduralContext*       FindOrCreateProceduralContext(const CryClassID& contextId) override;
	virtual const IProceduralContext* FindProceduralContext(const CryClassID& contextId) const override;
	virtual IProceduralContext*       FindProceduralContext(const CryClassID& contextId) override;
	virtual IProceduralContext*       CreateProceduralContext(const CryClassID& contextId) override;

	virtual QuatT                     ExtractLocalAnimLocation(FragmentID fragID, TagState fragTags, uint32 scopeID, uint32 optionIdx) override;

	virtual void                      Release() override;

	virtual const SMannParameter*     GetParam(const char* paramName) const override
	{
		return GetParam(MannGenCRC(paramName));
	}
	virtual const SMannParameter* GetParam(uint32 paramNameCRC) const override
	{
		const SMannParameter* pRet = m_mannequinParams.FindParam(paramNameCRC);
		for (TOwningControllerList::const_iterator iter = m_owningControllers.begin(); (iter != m_owningControllers.end()) && (pRet == NULL); ++iter)
		{
			const CActionController* pActionControllerParent = *iter;
			pRet = pActionControllerParent->GetParam(paramNameCRC);
		}
		return pRet;
	}
	virtual bool RemoveParam(const char* paramName) override
	{
		return m_mannequinParams.RemoveParam(paramName);
	}
	virtual bool RemoveParam(uint32 paramNameCRC) override
	{
		return m_mannequinParams.RemoveParam(paramNameCRC);
	}
	virtual void SetParam(const char* paramName, const SMannParameter& param) override
	{
		m_mannequinParams.SetParam(param.crc, param.value);
	}
	virtual void SetParam(const SMannParameter& param) override
	{
		m_mannequinParams.SetParam(param.crc, param.value);
	}
	virtual void ResetParams() override
	{
		m_mannequinParams.Reset();
	}

	// -- ~IActionController implementation --------------------------------------

	ActionScopes FlushScopesByScopeContext(uint32 scopeContextID, EFlushMethod flushMethod = FM_Normal); // returns the ActionScopes using this scopeContext

	void         ReleaseScopes();
	void         ReleaseScopeContexts();

#ifdef _DEBUG
	void ValidateActions();
#endif //_DEBUG

	const char* GetSafeEntityName() const // NOTE: should be safe to build a filename out of!
	{
		if (m_entityId == 0)
		{
			return "no entity";
		}

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
		CRY_ASSERT(pEntity);
		return pEntity ? pEntity->GetName() : "invalid";
	}

	void                OnEntityReturnedToPool(EntityId entityId, IEntity* pEntity);

	void                StartAction(IAction& action);
	void                EndAction(IAction& action, EFlushMethod flushMethod = FM_Normal);

	void                RegisterOwner(CActionController& owner)
	{
		m_owningControllers.push_back(&owner);
	}

	void UnregisterOwner(CActionController& owner)
	{
		TOwningControllerList::iterator iter = std::find(m_owningControllers.begin(), m_owningControllers.end(), &owner);

		if (iter != m_owningControllers.end())
		{
			m_owningControllers.erase(iter);
		}
		else
		{
			CryFatalError("Releasing reference to parent that does not exist!");
		}
	}

#ifndef _RELEASE
	static void                         ChangeDebug(const char* entName);
	static void                         DumpSequence(const char* entName, float dumpTime);
#endif //!_RELEASE
	static const TActionControllerList& GetGlobalActionControllers() { return s_actionControllers; }
	static IActionController*           FindActionController(const IEntity& entity);
	static void                         OnShutdown();

private:

	static void RegisterCVars();
	static void UnregisterCVars();
	static void Register(CActionController* ac);
	static void Unregister(CActionController* ac);

#ifndef _RELEASE
	void ValidateScopeContext(uint32 scopeContextID) const;
	void ValidateScopeContexts() const;
	void DebugDraw() const;
	void DebugDrawLocation(const QuatT& location, ColorB colorPos, ColorB colorX, ColorB colorY, ColorB colorZ) const;
#endif //!_RELEASE

	void         FlushScope(uint32 scopeID, ActionScopes scopeFlag, EFlushMethod flushMethod = FM_Normal);
	void         FlushProceduralContexts();

	bool         ResolveActionInstallations(float timePassed);
	void         ResolveActionStates();
	bool         BlendOffActions(float timePassed);
	void         PruneQueue();
	ActionScopes EndActionsOnScope(ActionScopes scopeMask, IAction* pPendingAction, bool blendOut = false, EFlushMethod flushMethod = FM_Normal);
	bool         TryInstalling(IAction& action, float timePassed);
	bool         CanInstall(const IAction& action, TagID subContext, const ActionScopes& scopeMask, float timeStep, float& timeTillInstall) const;
	void         Install(IAction& action, float timePassed);
	void         PushOntoQueue(IAction& action);
	bool         IsDifferent(const FragmentID fragID, const TagState& fragmentTags, const ActionScopes& scopeMask) const override;
	void         RequestInstall(const IAction& action, const ActionScopes& scopeMask);

	ActionScopes QueryScopeMask(FragmentID fragID, const TagState& fragTags, const TagID tagContext) const;
	ActionScopes ExpandOverlappingScopes(ActionScopes scopeMask) const;
	void         InsertEndingAction(IAction& action);
	void         SynchTagsToSlave(SScopeContext& scopeContext, bool enable);

	EntityId            m_entityId;
	IEntity*            m_cachedEntity;
	SAnimationContext&  m_context;

	SScopeContext*      m_scopeContexts;

	const uint32        m_scopeCount;
	CActionScope* const m_scopeArray;
	ActionScopes        m_activeScopes;

	typedef std::vector<IActionPtr> TActionList;
	TActionList m_queuedActions;

	struct SProcContext
	{
		CryClassID contextId;
		std::shared_ptr<IProceduralContext> pContext;
	};
	std::vector<SProcContext> m_procContexts;

	uint32                    m_flags;
	float                     m_timeScale;

	ActionScopes              m_scopeFlushMask;

	typedef std::vector<CActionController*> TOwningControllerList;
	TOwningControllerList m_owningControllers;

	void Record(const SMannHistoryItem& item);
	void RecordTagState();

	void UpdateValidity();
	bool UpdateRootEntityValidity();                        // returns true when root entity is present and valid
	bool UpdateScopeContextValidity(uint32 scopeContextId); // returns true when scopecontext is valid
#ifndef _RELEASE
	void DumpHistory(const char* filename, float earliestTime = 0.0f) const;
	static const int TOTAL_HISTORY_SLOTS = 200;
	SMannHistoryItem m_history[TOTAL_HISTORY_SLOTS];
	uint32           m_historySlot;
#endif //!_RELEASE
	TagState         m_lastTagStateRecorded;

	typedef std::vector<IMannequinListener*> TListenerList;
	TListenerList                            m_listeners;

	TActionList                              m_endedActions;
	std::vector<std::pair<IActionPtr, bool>> m_triggeredActions;

	CMannequinParams                         m_mannequinParams;

	static uint32                            s_blendChannelCRCs[MANN_NUMBER_BLEND_CHANNELS];
	static TActionControllerList             s_actionControllers;
	static CActionController::TActionList    s_actionList;
	static CActionController::TActionList    s_tickedActions;

	static bool                              s_registeredCVars;
#ifndef _RELEASE
	static ICVar*                            s_cvarMnDebug;
	static CActionController*                s_debugAC;
	static int                               s_mnDebugFragments;
#endif //!_RELEASE
#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
	static int s_mnFatalErrorOnInvalidEntity;
	static int s_mnFatalErrorOnInvalidCharInst;
#endif // !CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
};

#endif //!__ACTIONCONTROLLER_H__
