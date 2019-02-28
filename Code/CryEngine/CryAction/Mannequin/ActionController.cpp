// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ActionController.h"
#include "ActionScope.h"

#include "AnimationDatabase.h"

#include <CryExtension/CryCreateClassInstance.h>

#include "MannequinUtils.h"
#include "MannequinDebug.h"

const uint32 MAX_ALLOWED_QUEUE_SIZE = 10;

CActionController::TActionControllerList CActionController::s_actionControllers;
CActionController::TActionList CActionController::s_actionList;
CActionController::TActionList CActionController::s_tickedActions;

bool CActionController::s_registeredCVars = false;
uint32 CActionController::s_blendChannelCRCs[MANN_NUMBER_BLEND_CHANNELS];

#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
int CActionController::s_mnFatalErrorOnInvalidCharInst = 0;
int CActionController::s_mnFatalErrorOnInvalidEntity = 0;
#endif

#ifndef _RELEASE

CActionController* CActionController::s_debugAC = NULL;
ICVar* CActionController::s_cvarMnDebug = NULL;
int CActionController::s_mnDebugFragments = 0;

void ChangeDebug(ICVar* pICVar)
{
	if (!pICVar)
		return;

	const char* pVal = pICVar->GetString();
	CActionController::ChangeDebug(pVal);
}

void DumpSequence(IConsoleCmdArgs* pArgs)
{
	const int argCount = pArgs->GetArgCount();
	if (argCount >= 2)
	{
		const char* pVal = pArgs->GetArg(1);
		if (pVal)
		{
			float dumpTime = (argCount >= 3) ? (float)atof(pArgs->GetArg(2)) : -1.0f;

			CActionController::DumpSequence(pVal, dumpTime);
		}
	}
}

struct SACEntityNameAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{
		return CActionController::GetGlobalActionControllers().size();
	};

	virtual const char* GetValue(int nIndex) const
	{
		return CActionController::GetGlobalActionControllers()[nIndex]->GetSafeEntityName();
	};
};

static SACEntityNameAutoComplete s_ACEntityNameAutoComplete;

#endif //!_RELEASE

void CActionController::RegisterCVars()
{
	if (!s_registeredCVars)
	{
		IConsole* pConsole = gEnv->pConsole;
		assert(pConsole);
		if (!pConsole)
			return;

#ifndef _RELEASE
		s_cvarMnDebug = REGISTER_STRING_CB("mn_debug", "", VF_CHEAT, "Entity to display debug information for action controller for", ::ChangeDebug);
		REGISTER_COMMAND("mn_dump", ::DumpSequence, VF_CHEAT, "Dumps specified entity's CryMannequin history in the path specified by mn_sequence_path");

		if (gEnv->pConsole != NULL)
		{
			gEnv->pConsole->RegisterAutoComplete("mn_debug", &s_ACEntityNameAutoComplete);
			gEnv->pConsole->RegisterAutoComplete("mn_dump", &s_ACEntityNameAutoComplete);
		}
		REGISTER_CVAR3("mn_debugfragments", s_mnDebugFragments, 0, VF_CHEAT, "Log out fragment queues");
#endif //!_RELEASE

#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
		REGISTER_CVAR3("mn_fatalerroroninvalidentity", s_mnFatalErrorOnInvalidEntity, 1, VF_CHEAT, "Throw a fatal error when an invalid entity is detected");
		REGISTER_CVAR3("mn_fatalerroroninvalidcharinst", s_mnFatalErrorOnInvalidCharInst, 1, VF_CHEAT, "Throw a fatal error when an invalid character instance is detected");
#endif //!CRYMANNEQUIN_WARN_ABOUT_VALIDITY()

		char channelName[10];
		cry_strcpy(channelName, "channel0");
		for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
		{
			channelName[7] = '0' + i;
			s_blendChannelCRCs[i] = MannGenCRC(channelName);
		}

		s_registeredCVars = true;
	}
}

void CActionController::UnregisterCVars()
{
	if (s_registeredCVars)
	{
		IConsole* pConsole = gEnv->pConsole;
		if (pConsole)
		{
#ifndef _RELEASE
			pConsole->UnRegisterAutoComplete("mn_debug");
			pConsole->UnRegisterAutoComplete("mn_dump");
			pConsole->UnregisterVariable("mn_debug");
			pConsole->RemoveCommand("mn_dump");
			pConsole->UnregisterVariable("mn_debugfragments");
#endif //!_RELEASE

#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
			pConsole->UnregisterVariable("mn_fatalerroroninvalidentity");
			pConsole->UnregisterVariable("mn_fatalerroroninvalidcharinst");
#endif // !CRYMANNEQUIN_WARN_ABOUT_VALIDITY()

			s_registeredCVars = false;
		}
	}
}

void CActionController::Register(CActionController* ac)
{
	RegisterCVars();

	s_actionControllers.push_back(ac);

#ifndef _RELEASE
	::ChangeDebug(s_cvarMnDebug);
#endif //_RELEASE
}

void CActionController::Unregister(CActionController* ac)
{
	TActionControllerList::iterator iter = std::find(s_actionControllers.begin(), s_actionControllers.end(), ac);
	if (iter != s_actionControllers.end())
	{
		s_actionControllers.erase(iter);

		if (s_actionControllers.empty())
		{
			stl::free_container(s_actionControllers);
			stl::free_container(s_actionList);
			stl::free_container(s_tickedActions);
			UnregisterCVars();
		}
	}

#ifndef _RELEASE
	if (ac == s_debugAC)
	{
		s_debugAC = NULL;
	}
#endif //_RELEASE
}

void CActionController::OnShutdown()
{
	const uint32 numControllers = s_actionControllers.size();

	if (numControllers > 0)
	{
		for (TActionControllerList::iterator iter = s_actionControllers.begin(); iter != s_actionControllers.end(); ++iter)
		{
			CActionController* pAC = *iter;
			CryLogAlways("ActionController not released - Owner Controller Def: %s", pAC->GetContext().controllerDef.m_filename.c_str());
		}
		CryFatalError("ActionControllers (%u) not released at shutdown", numControllers);
	}
}

IActionController* CActionController::FindActionController(const IEntity& entity)
{
	const EntityId id = entity.GetId();
	for (TActionControllerList::iterator iter = s_actionControllers.begin(); iter != s_actionControllers.end(); ++iter)
	{
		CActionController* pAC = *iter;
		if (pAC->GetEntityId() == id)
			return pAC;
	}
	return NULL;
}

#ifndef _RELEASE
void CActionController::ChangeDebug(const char* entName)
{
	CActionController* debugAC = NULL;
	if (entName && entName[0])
	{
		for (TActionControllerList::iterator iter = s_actionControllers.begin(); iter != s_actionControllers.end(); ++iter)
		{
			CActionController* ac = *iter;
			if (stricmp(ac->GetSafeEntityName(), entName) == 0)
			{
				debugAC = ac;
				break;
			}
		}
	}

	if (debugAC != s_debugAC)
	{
		if (s_debugAC)
		{
			s_debugAC->SetFlag(AC_DebugDraw, false);
		}
		if (debugAC)
		{
			debugAC->SetFlag(AC_DebugDraw, true);
		}
		s_debugAC = debugAC;
	}
}

void BuildFilename(stack_string& filename, const char* entityName)
{
	char dateTime[128];
	time_t ltime;
	time(&ltime);
	tm* pTm = localtime(&ltime);
	strftime(dateTime, 128, "_%d_%b_%Y_%H_%M_%S.xml", pTm);

	ICVar* pSequencePathCVar = gEnv->pConsole->GetCVar("mn_sequence_path");
	if (pSequencePathCVar)
	{
		filename.append(pSequencePathCVar->GetString());
	}

	filename.append(entityName);
	filename.append(dateTime);
}

void CActionController::DumpSequence(const char* entName, float dumpTime)
{
	CActionController* debugAC = NULL;
	for (TActionControllerList::iterator iter = s_actionControllers.begin(); iter != s_actionControllers.end(); ++iter)
	{
		CActionController* ac = *iter;
		if (stricmp(ac->GetSafeEntityName(), entName) == 0)
		{
			debugAC = ac;
			break;
		}
	}

	if (!debugAC)
	{
		debugAC = s_debugAC;
	}

	if (debugAC)
	{
		stack_string filename;
		BuildFilename(filename, debugAC->GetSafeEntityName());
		debugAC->DumpHistory(filename.c_str(), dumpTime);
	}
}
#endif //!_RELEASE

CActionController::CActionController(IEntity* pEntity, SAnimationContext& context)
	: m_entityId(pEntity ? pEntity->GetId() : 0)
	, m_cachedEntity(pEntity)
	, m_context(context)
	, m_scopeCount(context.controllerDef.m_scopeIDs.GetNum())
	, m_scopeArray(static_cast<CActionScope*>(CryModuleMalloc(sizeof(CActionScope) * m_scopeCount)))
	, m_activeScopes(ACTION_SCOPES_NONE)
	, m_flags(0)
	, m_timeScale(1.f)
	, m_scopeFlushMask(ACTION_SCOPES_NONE)
#ifndef _RELEASE
	, m_historySlot(0)
#endif //_RELEASE
	, m_lastTagStateRecorded(TAG_STATE_EMPTY)
{
	const uint32 numScopeContexts = context.controllerDef.m_scopeContexts.GetNum();
	m_scopeContexts = new SScopeContext[numScopeContexts];
	for (uint32 i = 0; i < numScopeContexts; i++)
	{
		m_scopeContexts[i].Reset(i);
	}

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		const SScopeDef& scopeDef = context.controllerDef.m_scopeDef[i];
		const SScopeContextDef& scopeContextDef = context.controllerDef.m_scopeContextDef[scopeDef.context];
		const TagState additionalTags = context.controllerDef.m_tags.GetUnion(scopeDef.additionalTags, scopeContextDef.additionalTags);
		new(m_scopeArray + i)CActionScope(context.controllerDef.m_scopeIDs.GetTagName(i), i, *this, context, m_scopeContexts[scopeDef.context], scopeDef.layer, scopeDef.numLayers, additionalTags);
	}

	Register(this);
}

CActionController::~CActionController()
{
	Unregister(this);

	if (!m_owningControllers.empty())
	{
		const uint32 numOwningControllers = m_owningControllers.size();
		for (uint32 i = 0; i < numOwningControllers; i++)
		{
			m_owningControllers[i]->FlushSlaveController(*this);
		}
		m_owningControllers.clear();
	}

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CActionScope& scope = m_scopeArray[i];
		if (scope.m_pAction && (&scope.m_pAction->GetRootScope() == &scope) && scope.m_pAction->IsStarted())
		{
			scope.m_pAction->Exit();
		}
	}

	ReleaseScopes();
	ReleaseScopeContexts();

	uint32 numProcContexts = m_procContexts.size();
	for (uint32 i = 0; i < numProcContexts; i++)
	{
		m_procContexts[i].pContext.reset();
	}
}

void CActionController::Release()
{
	delete this;
}

bool CActionController::CanInstall(const IAction& action, TagID subContext, const ActionScopes& scopeMask, float timeStep, float& timeTillInstall) const
{
	timeTillInstall = 0.0f;

	//--- Ensure we test against all effected scopes
	ActionScopes expandedScopeMask = ExpandOverlappingScopes(scopeMask);
	expandedScopeMask &= m_activeScopes;

	//--- Calc frag tag state
	SFragTagState fragTagState(m_context.state.GetMask(), action.GetFragTagState());
	if (subContext != TAG_ID_INVALID)
	{
		const SSubContext subContextDef = m_context.controllerDef.m_subContext[subContext];
		fragTagState.globalTags = m_context.controllerDef.m_tags.GetUnion(fragTagState.globalTags, subContextDef.additionalTags);
	}

	ActionScopes scopeFlag = 1;
	for (uint32 i=0; i<m_scopeCount; i++, scopeFlag <<= 1)
	{
		if (scopeFlag & expandedScopeMask)
		{
			const CActionScope& scope = m_scopeArray[i];
			{
				SScopeContext& scopeContext = m_scopeContexts[scope.GetContextID()];
				CActionController* pSlaveAC = scopeContext.pEnslavedController;

				if (!pSlaveAC)
				{
					float timeRemaining;
					SAnimBlend animBlend;
					EPriorityComparison priorityComp = Higher;
					const bool isRequeue = (&action == scope.m_pAction);
					const IActionPtr pCompareAction = scope.m_pAction ? scope.m_pAction : scope.m_pExitingAction;
					if (pCompareAction)
					{
						if (isRequeue && (action.m_rootScope != &scope))
						{
							//--- If this is a requeued action, only use the primary scope to time transitions
							priorityComp = Higher;
						}
						else if (pCompareAction->m_rootScope != &scope)
						{
							//--- Only test the action on its root scope, as that is timing the action
							continue;
						}
						else
						{
							priorityComp = action.DoComparePriority(*pCompareAction);
						}

						expandedScopeMask &= ~pCompareAction->GetInstalledScopeMask();
					}

					if (!scope.CanInstall(priorityComp, action.GetFragmentID(), fragTagState, isRequeue, timeRemaining))
					{
						return false;
					}

					timeTillInstall = max(timeTillInstall, timeRemaining);
				}
				else
				{
					//--- Ensure that we time our transitions based on the target too
					const uint32 fragCRC = m_context.controllerDef.m_fragmentIDs.GetTagCRC(action.GetFragmentID());
					const FragmentID tgtFragID = pSlaveAC->GetContext().controllerDef.m_fragmentIDs.Find(fragCRC);
					SFragTagState childFragTagState;
					childFragTagState.globalTags = pSlaveAC->GetContext().state.GetMask();
					childFragTagState.fragmentTags = action.GetFragTagState();
					const ActionScopes childScopeMask = pSlaveAC->GetContext().controllerDef.GetScopeMask(tgtFragID, childFragTagState);
					float tgtTimeTillInstall;
					if (!pSlaveAC->CanInstall(action, TAG_ID_INVALID, childScopeMask, timeStep, tgtTimeTillInstall))
					{
						return false;
					}

					timeTillInstall = max(timeTillInstall, tgtTimeTillInstall);
				}
			}
		}
	}

	return (timeStep >= timeTillInstall);
}

bool CActionController::CanInstall(const IAction& action, const ActionScopes& scopeMask, float timeStep, float& timeTillInstall) const
{
	return CanInstall(action, action.GetSubContext(), scopeMask, timeStep, timeTillInstall);
}

bool CActionController::IsDifferent(const FragmentID fragID, const TagState& fragmentTags, const ActionScopes& scopeMask) const
{
	const uint32 numScopes = GetTotalScopes();
	const ActionScopes mask = GetActiveScopeMask() & scopeMask;

	uint32 installedContexts = 0;
	for (uint32 i = 0; i < numScopes; i++)
	{
		if (BIT64(i) & mask)
		{
			const CActionScope& scope = m_scopeArray[i];
			if (scope.NeedsInstall(installedContexts))
			{
				installedContexts |= scope.GetContextMask();
				if (scope.IsDifferent(fragID, fragmentTags))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void CActionController::RequestInstall(const IAction& action, const ActionScopes& scopeMask)
{
	//--- Ensure we test against all effected scopes
	ActionScopes expandedScopeMask = ExpandOverlappingScopes(scopeMask);
	expandedScopeMask &= m_activeScopes;

	uint32 contextIDs = 0;
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		if (BIT64(i) & expandedScopeMask)
		{
			CActionScope& scope = m_scopeArray[i];
			if (scope.NeedsInstall(contextIDs))
			{
				contextIDs |= scope.GetContextMask();
				IAction* const pScopeAction = scope.m_pAction.get();
				if (pScopeAction && (pScopeAction->m_rootScope == &scope))
				{
					EPriorityComparison priorityComp = action.DoComparePriority(*pScopeAction);
					pScopeAction->OnRequestBlendOut(priorityComp);
				}
			}
		}
	}
}

void CActionController::InsertEndingAction(IAction& action)
{
	stl::push_back_unique(m_endedActions, IActionPtr(&action));
}

void CActionController::Install(IAction& action, float timeRemaining)
{
	const FragmentID fragmentID = action.GetFragmentID();
	const bool isReinstall = (action.GetStatus() == IAction::Installed);
	const TagID subContext = action.GetSubContext();
	uint32 optionIdx = action.GetOptionIdx();

	if (optionIdx == OPTION_IDX_RANDOM)
	{
		optionIdx = m_context.randGenerator.GenerateUint32();
	}

	action.BeginInstalling();
	if (!isReinstall)
	{
		action.Install();
	}

	//--- Setup scope mask
	ActionScopes scopeMask = action.GetForcedScopeMask() | QueryScopeMask(action.GetFragmentID(), action.GetFragTagState(), action.GetSubContext());
	ActionScopes filteredScope = scopeMask & m_activeScopes;
	SFragTagState tagState = SFragTagState(m_context.state.GetMask(), action.GetFragTagState());

	ActionScopes overlappedScopes = ExpandOverlappingScopes(filteredScope);
	m_scopeFlushMask |= overlappedScopes;

	RecordTagState();
	Record(SMannHistoryItem(action.GetForcedScopeMask(), fragmentID, tagState.fragmentTags, optionIdx, true));

	if (subContext != TAG_ID_INVALID)
	{
		const SSubContext subContextDef = m_context.controllerDef.m_subContext[subContext];
		tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(tagState.globalTags, m_context.subStates[subContext].GetMask());
		tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(tagState.globalTags, subContextDef.additionalTags);
	}

	action.m_installedScopeMask = filteredScope;
	m_scopeFlushMask &= ~filteredScope;

	//--- Now install action into scopes & animations on all other contexts
	ActionScopes rootScope = SCOPE_ID_INVALID;
	uint32 installedContexts = 0;
	bool isOneShot = (fragmentID != FRAGMENT_ID_INVALID);
	ActionScopes scopeFlag = 1;
	for (uint32 i=0; i<m_scopeCount; i++, scopeFlag <<= 1)
	{
		CActionScope& scope = m_scopeArray[i];
		IActionPtr pExitingAction = scope.m_pAction;
		bool exitCurrentAction = false;

		if (scopeFlag & filteredScope)
		{
			bool higherPriority = true;
			if (pExitingAction)
			{
				higherPriority = (action.DoComparePriority(*pExitingAction) == Higher);
			}

			scope.Install(action); // do this before QueueFragment so QueueFragment can call OnAnimInstalled on the action

			//--- Flush any previously existing exiting action, as this has already been overridden
			if (scope.m_pExitingAction)
			{
				InsertEndingAction(*scope.m_pExitingAction);
				scope.m_pExitingAction->m_flags &= ~IAction::TransitioningOut;
				scope.m_pExitingAction = NULL;
			}

			SScopeContext& scopeContext = m_scopeContexts[scope.GetContextID()];
			CActionController* pSlaveAC = scopeContext.pEnslavedController;

			const bool rootContextInstallation = scope.NeedsInstall(installedContexts);
			const bool waitingOnRootScope = (rootScope == SCOPE_ID_INVALID);

			if (pSlaveAC != NULL)
			{
				//--- Ensure that our slave tags are up to date
				SynchTagsToSlave(scopeContext, true);

				uint32 fragCRC = m_context.controllerDef.m_fragmentIDs.GetTagCRC(fragmentID);
				FragmentID slaveFragID = pSlaveAC->GetContext().controllerDef.m_fragmentIDs.Find(fragCRC);

				if (slaveFragID != FRAGMENT_ID_INVALID)
				{
					IActionPtr pDummyAction = action.CreateSlaveAction(slaveFragID, tagState.fragmentTags, pSlaveAC->GetContext());
					pDummyAction->m_mannequinParams = action.m_mannequinParams;
					pDummyAction->Initialise(pSlaveAC->m_context);
					pDummyAction->m_optionIdx = optionIdx;
					pSlaveAC->Install(*pDummyAction, timeRemaining);

					if (pDummyAction->GetInstalledScopeMask() != ACTION_SCOPES_NONE)
					{
						pSlaveAC->m_triggeredActions.push_back(std::make_pair(pDummyAction, false));
						action.m_slaveActions.push_back(pDummyAction);

						pSlaveAC->ResolveActionStates();

						//--- Copy the timing settings back from the target
						CActionScope* pTargetScope = (CActionScope*)&pDummyAction->GetRootScope();
						scope.ClearSequencers();
						scope.m_lastFragmentID = fragmentID;
						scope.m_fragmentDuration = pTargetScope->m_fragmentDuration;
						scope.m_transitionDuration = pTargetScope->m_transitionDuration;
						scope.m_transitionOutroDuration = pTargetScope->m_transitionOutroDuration;
						scope.m_fragmentTime = pTargetScope->m_fragmentTime;
						scope.m_sequenceFlags = pTargetScope->m_sequenceFlags;
						scope.QueueAnimFromSequence(0, 0, false);
					}
					else
					{
						pDummyAction.reset();
					}
				}
			}
			else if (!isReinstall || scope.m_isOneShot || scope.IsDifferent(action.m_fragmentID, action.m_fragTags, action.m_subContext))
			{
				if (scope.QueueFragment(fragmentID, tagState, optionIdx, timeRemaining, action.GetUserToken(), waitingOnRootScope, higherPriority, rootContextInstallation))
				{
					if (scope.HasOutroTransition() && pExitingAction)
					{
						pExitingAction->TransitionOutStarted();
					}
				}
			}

			if (waitingOnRootScope && scope.HasFragment())
			{
				float rootStartTime = scope.GetFragmentStartTime();
				timeRemaining = rootStartTime;
				rootScope = i;

				isOneShot = scope.m_isOneShot;
			}

			installedContexts |= scope.GetContextMask();

			exitCurrentAction = (pExitingAction && (pExitingAction->m_rootScope == &scope));
		}
		else
		{
			exitCurrentAction = (pExitingAction && ((scopeFlag & overlappedScopes) != 0));
		}

		if (exitCurrentAction && (pExitingAction != &action))
		{
			InsertEndingAction(*pExitingAction);
			pExitingAction->m_eStatus = IAction::Exiting;
		}
	}

	if (isOneShot)
	{
		action.m_flags |= IAction::FragmentIsOneShot;
	}
	else
	{
		action.m_flags &= ~IAction::FragmentIsOneShot;
	}

	if (rootScope == SCOPE_ID_INVALID)
	{
		rootScope = GetLeastSignificantBit(filteredScope);
	}
	action.m_rootScope = &m_scopeArray[rootScope];
}

bool CActionController::TryInstalling(IAction& action, float timePassed)
{
	const FragmentID fragmentID = action.GetFragmentID();

	const TagID subContext = action.GetSubContext();
	ActionScopes scopeMask = action.GetForcedScopeMask() | QueryScopeMask(fragmentID, action.GetFragTagState(), subContext);
	ActionScopes filteredScope = scopeMask & m_activeScopes;

	//--- Request Install
	RequestInstall(action, filteredScope);

	//--- Can I install now?
	float timeRemaining;
	if (CanInstall(action, filteredScope, timePassed, timeRemaining))
	{
		Install(action, timeRemaining);

		return true;
	}

	return false;
}

bool CActionController::QueryDuration(IAction& action, float& fragmentDuration, float& transitionDuration) const
{
	const TagID subContext = action.GetSubContext();
	ActionScopes scopeMask = action.GetForcedScopeMask() | QueryScopeMask(action.GetFragmentID(), action.GetFragTagState(), subContext);
	scopeMask = scopeMask & m_activeScopes;
	uint32 optionIdx = action.GetOptionIdx();
	if (optionIdx == OPTION_IDX_RANDOM)
	{
		optionIdx = m_context.randGenerator.GenerateUint32();
		action.SetOptionIdx(optionIdx);
	}

	SFragTagState tagState = SFragTagState(m_context.state.GetMask(), action.GetFragTagState());
	if (subContext != TAG_ID_INVALID)
	{
		const SSubContext subContextDef = m_context.controllerDef.m_subContext[subContext];
		tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(tagState.globalTags, m_context.subStates[subContext].GetMask());
		tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(tagState.globalTags, subContextDef.additionalTags);
	}
	ActionScopes scopeFlag = 1;
	for (uint32 i=0; i<m_scopeCount; i++, scopeFlag <<= 1)
	{
		if (scopeFlag & scopeMask)
		{
			const CActionScope& scope = m_scopeArray[i];

			bool higherPriority = true;
			if (scope.m_pAction)
			{
				higherPriority = (action.DoComparePriority(*scope.m_pAction) == Higher);
			}

			SBlendQuery query;
			scope.FillBlendQuery(query, action.GetFragmentID(), tagState, higherPriority, NULL);

			const SScopeContext& scopeContext = m_scopeContexts[scope.GetContextID()];
			SFragmentData fragData;
			IAnimationSet* pAnimSet = scopeContext.pCharInst ? scopeContext.pCharInst->GetIAnimationSet() : NULL;
			// Normally, a scope context must have an animation database associated in order to be active, but enslavement may break this, so we check just in case
			bool hasFound = scopeContext.pDatabase && (scopeContext.pDatabase->Query(fragData, query, optionIdx, pAnimSet) & eSF_Fragment) != 0;

			if (hasFound)
			{
				fragmentDuration = transitionDuration = 0.0f;
				for (uint32 p = 0; p < SFragmentData::PART_TOTAL; p++)
				{
					if (fragData.transitionType[p] == eCT_Normal)
					{
						fragmentDuration += fragData.duration[p];
					}
					else
					{
						transitionDuration += fragData.duration[p];
					}
				}

				return true;
			}
		}
	}

	return false;
}

void CActionController::ReleaseScopes()
{
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CActionScope& scope = m_scopeArray[i];
		if (scope.m_pAction && (&scope.m_pAction->GetRootScope() == &scope) && scope.m_pAction->IsStarted())
		{
			scope.m_pAction->Exit();
		}

		if (scope.m_pExitingAction && (&scope.m_pExitingAction->GetRootScope() == &scope) && scope.m_pExitingAction->IsStarted())
		{
			scope.m_pExitingAction->Exit();
		}
	}

	for (uint32 i = 0; i < m_scopeCount; ++i)
	{
		CActionScope& scope = m_scopeArray[i];
		scope.Flush(FM_Normal);
	}

	for (uint32 i = 0; i < m_scopeCount; ++i)
	{
		CActionScope& scope = m_scopeArray[i];

		IAction* const pScopeAction = scope.m_pAction.get();
		if (pScopeAction && pScopeAction->m_rootScope == &scope)
		{
			pScopeAction->m_eStatus = IAction::None;
			pScopeAction->m_flags &= ~IAction::Requeued;

			pScopeAction->m_rootScope = NULL;
		}
		scope.m_pAction = NULL;

		IAction* const pExitAction = scope.m_pExitingAction.get();
		if (pExitAction && pExitAction->m_rootScope == &scope)
		{
			pExitAction->m_eStatus = IAction::None;
			pExitAction->m_flags &= ~IAction::Requeued;

			pExitAction->m_rootScope = NULL;
		}
		scope.m_pExitingAction = NULL;
	}

	for (uint32 i = 0; i < m_queuedActions.size(); ++i)
	{
		m_queuedActions[i]->m_eStatus = IAction::None;
	}

	for (uint32 i = 0; i < m_scopeCount; ++i)
	{
		m_scopeArray[i].~CActionScope();
	}
	CryModuleFree(m_scopeArray);

	m_queuedActions.clear();
	m_activeScopes = 0;
}

void CActionController::ReleaseScopeContexts()
{
	//--- Remove ourselves from any enslaved characters
	const uint32 numContexts = m_context.controllerDef.m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numContexts; i++)
	{
		SScopeContext& scopeContext = m_scopeContexts[i];
		if (scopeContext.pEnslavedController)
		{
			SynchTagsToSlave(scopeContext, false);
			scopeContext.pEnslavedController->UnregisterOwner(*this);
		}
	}
	delete[] m_scopeContexts;
	m_scopeContexts = NULL;
}

void CActionController::FlushScope(uint32 scopeID, ActionScopes scopeFlag, EFlushMethod flushMethod)
{
	CActionScope& scope = m_scopeArray[scopeID];

	if (scope.m_pAction && (&scope.m_pAction->GetRootScope() == &scope))
	{
		EndActionsOnScope(scopeFlag, NULL, true, flushMethod);
	}
	scope.m_pAction = NULL;

	scope.Flush(flushMethod);
}

void CActionController::SetScopeContext(uint32 scopeContextID, IEntity& entity, ICharacterInstance* pCharacter, const IAnimationDatabase* animDatabase)
{
	if (scopeContextID >= (uint32)m_context.controllerDef.m_scopeContexts.GetNum())
	{
		CryFatalError("[SetScopeContext] Invalid scope context id %u used for %s", scopeContextID, m_cachedEntity->GetName());
	}

	SScopeContext& scopeContext = m_scopeContexts[scopeContextID];

	if ((pCharacter != scopeContext.pCharInst) || (animDatabase != scopeContext.pDatabase))
	{
		if (scopeContext.pEnslavedController)
		{
			SynchTagsToSlave(scopeContext, false);
			scopeContext.pEnslavedController->UnregisterOwner(*this);
		}

		if (scopeContext.pCharInst)
		{
			scopeContext.pCharInst->SetPlaybackScale(1.f);
		}

		// CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_INFO, "***SetScopeContext %2d on entity %s (id=%d; %p) to entity=%s (id=%8d; %p), char=%x", scopeContextID, GetSafeEntityName(), m_entityId, m_cachedEntity, entity.GetName(), entity.GetId(), &entity, pCharacter);

		// Flush all scopes for the specified scopeContext
		ActionScopes scopesUsingContext = FlushScopesByScopeContext(scopeContextID);

		// Fill context data
		scopeContext.pCharInst = pCharacter;
		scopeContext.pDatabase = (CAnimationDatabase*)animDatabase;
		scopeContext.entityId = entity.GetId();
		scopeContext.pCachedEntity = &entity;
		scopeContext.pEnslavedController = NULL;

		if (pCharacter)
		{
			pCharacter->SetPlaybackScale(GetTimeScale());
		}

		// If the new context is valid, activate its scopes
		bool scopeContextIsValid = UpdateScopeContextValidity(scopeContextID);
		CRY_ASSERT(scopeContextIsValid);
		if (scopeContextIsValid && animDatabase)
			m_activeScopes |= scopesUsingContext;
		else
			m_activeScopes &= ~scopesUsingContext;
	}
}

void CActionController::ClearScopeContext(uint32 scopeContextID, bool flushAnimations)
{
	if (scopeContextID >= (uint32)m_context.controllerDef.m_scopeContexts.GetNum())
	{
		CryFatalError("[ClearScopeContext] Invalid scope context id %u used for %s", scopeContextID, m_cachedEntity->GetName());
	}

	//-- Flush all scopes that use this scopecontext
	ActionScopes scopesUsingContext = FlushScopesByScopeContext(scopeContextID, flushAnimations ? FM_Normal : FM_NormalLeaveAnimations);

	SScopeContext& scopeContext = m_scopeContexts[scopeContextID];

	if (scopeContext.pEnslavedController)
	{
		SynchTagsToSlave(scopeContext, false);
		scopeContext.pEnslavedController->UnregisterOwner(*this);
	}

	if (scopeContext.pCharInst)
	{
		scopeContext.pCharInst->SetPlaybackScale(1.f);
	}

	//--- Clear context data
	scopeContext.pCharInst.reset();
	scopeContext.pDatabase = NULL;
	scopeContext.entityId = 0;
	scopeContext.pCachedEntity = NULL;
	scopeContext.pEnslavedController = NULL;

	//--- Disable scopes that use this context
	m_activeScopes &= ~scopesUsingContext;
}

IScope* CActionController::GetScope(uint32 scopeID)
{
	CRY_ASSERT_MESSAGE((scopeID < m_scopeCount), "Invalid scope id");

	if (scopeID < m_scopeCount)
	{
		return &m_scopeArray[scopeID];
	}

	return NULL;
}

const IScope* CActionController::GetScope(uint32 scopeID) const
{
	CRY_ASSERT_MESSAGE((scopeID < m_scopeCount), "Invalid scope id");

	if (scopeID < m_scopeCount)
	{
		return &m_scopeArray[scopeID];
	}

	return NULL;
}

#ifdef _DEBUG
void CActionController::ValidateActions()
{
	for (uint32 i = 0, scopeFlag = 1; i < m_scopeCount; i++, scopeFlag <<= 1)
	{
		CActionScope& scope = m_scopeArray[i];

		if (scope.m_pAction)
		{
			CRY_ASSERT(scope.m_pAction->m_installedScopeMask & scopeFlag);
		}
	}
}
#endif //_DEBUG

void CActionController::ResolveActionStates()
{
	m_scopeFlushMask &= m_activeScopes;
	for (uint32 i = 0; i < m_scopeCount; ++i)
	{
		const auto scopeFlag = ActionScopes(1) << i;
		if (scopeFlag & m_scopeFlushMask)
		{
			CActionScope& scope = m_scopeArray[i];
			scope.BlendOutFragments();
			scope.m_pAction.reset();
		}
	}
	m_scopeFlushMask = 0;

	// Release dangling references.
	for (uint32 i = 0; i < m_scopeCount; ++i)
	{
		const auto scopeFlag = ActionScopes(1) << i;
		CActionScope& scope = m_scopeArray[i];
		if (scope.m_pAction && !(scope.m_pAction->m_installedScopeMask & scopeFlag))
		{
			scope.m_pAction.reset();
		}
	}

	//--- Now delete dead actions
	for (uint32 i = 0; i < m_endedActions.size(); i++)
	{
		IAction* action = m_endedActions[i];

		if (action->IsTransitioningOut())
		{
			CActionScope* pActionScope = (CActionScope*)&action->GetRootScope();
			pActionScope->m_pExitingAction = action;
		}
		else
		{
			EndAction(*action);
		}
	}
	m_endedActions.clear();

#ifdef _DEBUG
	ValidateActions();
#endif //_DEBUG

	//--- Now install actions
	for (uint32 i = 0; i < m_triggeredActions.size(); i++)
	{
		IAction* action = m_triggeredActions[i].first.get();
		const bool isReinstall = m_triggeredActions[i].second;

		for (TActionList::iterator iter = m_queuedActions.begin(); iter != m_queuedActions.end(); ++iter)
		{
			if (*iter == action)
			{
				m_queuedActions.erase(iter);
				break;
			}
		}

		if (action->GetStatus() == IAction::Installed)
		{
			CActionScope& rootScope = (CActionScope&) action->GetRootScope();
			if (isReinstall)
			{
				//--- Reinstallation? -> remove queue's additional reference and wipe flag
				action->m_flags &= ~(IAction::Requeued);
			}
			else if (!rootScope.HasOutroTransition())
			{
				action->Enter();
			}
		}

		action->EndInstalling();
	}
	m_triggeredActions.clear();
}

bool CActionController::ResolveActionInstallations(float timePassed)
{
	bool changed = false;

	ActionScopes scopeMaskAcc = 0;

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		IAction* pScopeAction = m_scopeArray[i].GetPlayingAction();
		if (pScopeAction && (pScopeAction->m_rootScope == &m_scopeArray[i]))
		{
			pScopeAction->OnResolveActionInstallations();
		}
	}

	const uint32 numActions = m_queuedActions.size();

	s_actionList = m_queuedActions;
	for (uint32 i = 0; i < numActions; i++)
	{
		IAction& action = *s_actionList[i];

		ActionScopes scopeMask = action.GetForcedScopeMask() | QueryScopeMask(action.GetFragmentID(), action.GetFragTagState(), action.GetSubContext());
		ActionScopes scopeMaskFiltered = scopeMask & m_activeScopes;
		action.m_rootScope = scopeMaskFiltered ? &m_scopeArray[GetLeastSignificantBit(scopeMaskFiltered)] : nullptr;
		const bool isRequeue = ((action.m_flags & IAction::Requeued) != 0);

		if (!isRequeue && !stl::find(s_tickedActions, &action) && action.m_rootScope)
		{
			action.UpdatePending(timePassed);
			s_tickedActions.push_back(&action);
		}

		IAction::EStatus status = action.GetStatus();

		if ((status == IAction::Finished) || (action.m_flags & IAction::Stopping) || ((status != IAction::Installed) && isRequeue))
		{
			//--- Remove from queue & clear requeued flag
			for (TActionList::iterator iter = m_queuedActions.begin(); iter != m_queuedActions.end(); ++iter)
			{
				if (*iter == &action)
				{
					m_queuedActions.erase(iter);
					break;
				}
			}
			action.m_flags &= ~IAction::Requeued;
		}
		else
		{
			if (scopeMaskFiltered)
			{
				const bool canTrigger = (scopeMaskAcc & scopeMaskFiltered) == 0;
				const bool isReinstall = (action.GetStatus() == IAction::Installed);

				scopeMaskAcc |= scopeMaskFiltered;

				if (canTrigger && TryInstalling(action, timePassed))
				{
					m_triggeredActions.push_back(std::make_pair(&action, isReinstall));
					changed = true;
				}
			}
		}
	}
	s_actionList.clear();

	ResolveActionStates();

	if (BlendOffActions(timePassed))
	{
		changed = true;
	}

	return changed;
}

bool CActionController::BlendOffActions(float timePassed)
{
	bool hasEndedAction = false;

	//--- Check for any already finished actions and flush them here
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CActionScope& rootScope = m_scopeArray[i];
		IActionPtr pExitingAction = rootScope.m_pAction;

		if (pExitingAction
		    && (&pExitingAction->GetRootScope() == &rootScope)
		    && (rootScope.GetFragmentTime() > 0.0f)
		    && ((pExitingAction->m_flags & IAction::TransitionPending) == 0)) // No blend off if there is a fragment we are waiting to blend to already
		{
			float timeLeft = 0.0f;
			EPriorityComparison priority = (pExitingAction->GetStatus() == IAction::Finished) ? Higher : Lower;
			if (rootScope.CanInstall(priority, FRAGMENT_ID_INVALID, SFragTagState(), false, timeLeft) && (timePassed >= timeLeft))
			{
				const ActionScopes installedScopes = pExitingAction->GetInstalledScopeMask() & m_activeScopes;
				ActionScopes scopeFlag = 1;
				for (uint32 s=0; s<m_scopeCount; s++, scopeFlag<<=1)
				{
					if ((scopeFlag & installedScopes) != 0)
					{
						CActionScope& scope = m_scopeArray[s];
						scope.QueueFragment(FRAGMENT_ID_INVALID, SFragTagState(m_context.state.GetMask()), OPTION_IDX_RANDOM, 0.0f, 0, priority == Higher);
						scope.m_pAction = NULL;
					}
				}

				if (rootScope.HasOutroTransition())
				{
					rootScope.m_pExitingAction = pExitingAction;
					pExitingAction->TransitionOutStarted();
				}
				else
				{
					EndAction(*pExitingAction);
				}

				hasEndedAction = true;
			}
		}
	}

#ifdef _DEBUG
	ValidateActions();
#endif //_DEBUG

	return hasEndedAction;
}

void CActionController::PruneQueue()
{
	int numTooManyActions = m_queuedActions.size() - MAX_ALLOWED_QUEUE_SIZE;

	if (numTooManyActions > 0)
	{
		// print the remaining action queue before shrinking it
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "--- Mannequin Error, attempting to queue too many actions on entity '%s' (queue size = %" PRISIZE_T ", but only up to %u allowed) ---", GetSafeEntityName(), m_queuedActions.size(), MAX_ALLOWED_QUEUE_SIZE);
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "    %" PRISIZE_T " actions in queue:", m_queuedActions.size());
		for (size_t k = 0; k < m_queuedActions.size(); k++)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "    #%" PRISIZE_T ": %s", k, m_queuedActions[k]->GetName());
		}

		do
		{
			bool removedAction = false;
			// Remove from the back of the list
			for (int i = (int)m_queuedActions.size() - 1; i >= 0; --i)
			{
				IActionPtr pOtherAction = m_queuedActions[i];
				const bool isInterruptable = ((pOtherAction->GetFlags() & IAction::Interruptable) != 0);
				if (!isInterruptable)
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Ditching other action '%s' (%p) (at queue index %d)", pOtherAction->GetName(), pOtherAction.get(), i);
					m_queuedActions.erase(m_queuedActions.begin() + i);
					pOtherAction->Fail(AF_QueueFull);
					numTooManyActions = m_queuedActions.size() - MAX_ALLOWED_QUEUE_SIZE;  // need to re-compute the overflow as IAction::Fail() might have just queued new actions
					removedAction = true;
					break;
				}
			}
			// Can't remove anything - so just bail out
			if (removedAction == false)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Can't remove anything");
				break;
			}
		}
		while (numTooManyActions > 0);
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "---------------------");
	}
}

void CActionController::Update(float timePassed)
{
	if ((m_flags & AC_PausedUpdate) == 0)
	{
		timePassed *= GetTimeScale();

		SetFlag(AC_IsInUpdate, true);
		UpdateValidity();

		RecordTagState();

		for (uint32 i = 0; i < m_scopeCount; i++)
		{
			CActionScope& scope = m_scopeArray[i];
			IActionPtr pScopeAction = scope.GetPlayingAction();
			if (pScopeAction && (pScopeAction->m_rootScope == &scope))
			{
				//--- Insert user data into the parameter system
				ICharacterInstance* pInst = scope.GetCharInst();
				if (pInst)
				{
					for (uint32 ch = 0; ch < MANN_NUMBER_BLEND_CHANNELS; ch++)
					{
						pScopeAction->SetParam(s_blendChannelCRCs[ch], pInst->GetISkeletonAnim()->GetUserData(ch));
					}
				}

				const float scaledDeltaTime = timePassed * pScopeAction->GetSpeedBias();
				pScopeAction->Update(scaledDeltaTime);

				//--- Reset the pending transition flag
				//--- If the transition still exists it'll be reset in the blend code
				pScopeAction->m_flags &= ~IAction::TransitionPending;
			}
		}

		for (uint32 i = 0; i < m_scopeCount; i++)
		{
			CActionScope& scope = m_scopeArray[i];
			IActionPtr pScopeAction = scope.GetPlayingAction();
			if (pScopeAction && (pScopeAction->m_rootScope == &scope))
			{
				// Detect early blend-out
				if (scope.m_isOneShot)
				{
					const float scaledDeltaTime = timePassed * pScopeAction->GetSpeedBias();
					const float timeToCompletion = scope.CalculateFragmentTimeRemaining() - scope.m_blendOutDuration;
					const bool reachedCompletionPoint = timeToCompletion >= 0.0f && timeToCompletion < scaledDeltaTime;
					if (reachedCompletionPoint)
					{
						pScopeAction->OnActionFinished();
					}
				}
			}
		}

		CRY_ASSERT(s_tickedActions.empty());
		uint32 numIts;
		const uint32 MAX_ITERATIONS = 5;
		for (numIts = 0; numIts < MAX_ITERATIONS; numIts++)
		{
			if (!ResolveActionInstallations(timePassed))
				break;
		}
		s_tickedActions.clear();

		if (numIts == MAX_ITERATIONS)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Error, cannot resolve action changes in %u iterations for '%s'", MAX_ITERATIONS, GetSafeEntityName());
		}
		else if (numIts > 0)
		{
			//--- Something has changed, update state
			mannequin::debug::WebDebugState(*this);
		}

		PruneQueue();

		//--- Update scope sequencers
		for (uint32 i = 0; i < m_scopeCount; i++)
		{
			if (BIT64(i) & m_activeScopes)
			{
				CActionScope& scope = m_scopeArray[i];
				scope.Update(timePassed);
			}
		}

		uint32 numProcContexts = m_procContexts.size();
		for (uint32 i = 0; i < numProcContexts; i++)
		{
			m_procContexts[i].pContext->Update(timePassed);
		}
		SetFlag(AC_IsInUpdate, false);
	}

#ifndef _RELEASE
	if (m_flags & AC_DumpState)
	{
		stack_string filename;
		BuildFilename(filename, GetSafeEntityName());
		DumpHistory(filename.c_str(), -1.0f);

		m_flags &= ~AC_DumpState;
	}

	if (m_flags & AC_DebugDraw)
	{
		DebugDraw();
	}
#endif //!_RELEASE
}

void CActionController::SetSlaveController(IActionController& target, uint32 targetContext, bool enslave, const IAnimationDatabase* piOptionTargetDatabase)
{
	const bool doFullEnslavement = !piOptionTargetDatabase;

	if (targetContext >= m_context.controllerDef.m_scopeContexts.GetNum())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "SetSlaveController: invalid scope context index!");
		return;
	}

	UpdateValidity();

	CActionController& targetController = static_cast<CActionController&>(target);
	SScopeContext& scopeContext = m_scopeContexts[targetContext];

	if (enslave)
	{
		targetController.UpdateValidity();
		CRY_ASSERT(targetController.GetEntityId());

		if (scopeContext.pEnslavedController)
		{
			SynchTagsToSlave(scopeContext, false);
			scopeContext.pEnslavedController->UnregisterOwner(*this);
			scopeContext.pEnslavedController = NULL;
		}

		if (targetController.GetEntityId())
		{
			IEntity& targetEnt = targetController.GetEntity();

			if (doFullEnslavement)
			{
				SetScopeContext(targetContext, targetEnt, NULL, NULL);

				targetController.RegisterOwner(*this);

				//--- Enable all associated scopes
				ActionScopes scopeFlag = 1;
				for (size_t i = 0; i < m_scopeCount; i++, scopeFlag <<= 1)
				{
					const CActionScope& scope = m_scopeArray[i];
					if (scope.GetContextID() == targetContext)
					{
						m_activeScopes |= scopeFlag;
					}
				}

				scopeContext.pEnslavedController = &targetController;

				const SControllerDef& controllerDef = GetContext().controllerDef;
				const CTagDefinition& tagDef = controllerDef.m_tags;
				const CTagDefinition& tagDefSlave = targetController.GetContext().controllerDef.m_tags;
				const SScopeContextDef& scopeContextDef = controllerDef.m_scopeContextDef[targetContext];
				scopeContext.sharedTags = tagDef.GetIntersection(tagDef.GetSharedTags(tagDefSlave), scopeContextDef.sharedTags);
				scopeContext.sharedTags = tagDef.GetUnion(scopeContext.sharedTags, scopeContextDef.additionalTags);
				scopeContext.setTags = TAG_STATE_EMPTY;

				SynchTagsToSlave(scopeContext, true);
			}
			else
			{
				//--- Hookup all the scopes
				const ActionScopes targetScopeMask = targetController.GetActiveScopeMask();
				const ActionScopes rootScope = GetLeastSignificantBit(targetScopeMask);
				CActionScope& targetScope = targetController.m_scopeArray[rootScope];
				CRY_ASSERT(piOptionTargetDatabase);
				SetScopeContext(targetContext, targetScope.GetEntity(), targetScope.GetCharInst(), piOptionTargetDatabase);

				const ActionScopes clearMask = targetController.m_activeScopes & ~BIT64(rootScope);
				ActionScopes scopeMask = 1;
				for (uint32 i=0; i<targetController.m_scopeCount; i++, scopeMask <<= 1)
				{
					if (scopeMask & clearMask)
					{
						CActionScope& scope = targetController.m_scopeArray[i];
						scope.BlendOutFragments();
						scope.UpdateSequencers(0.0f);
					}
				}
				targetController.EndActionsOnScope(targetScopeMask, NULL);
			}
		}
	}
	else
	{
		ClearScopeContext(targetContext);
	}

	if (!doFullEnslavement)
	{
		targetController.SetFlag(AC_PausedUpdate, enslave);
	}
}

void CActionController::FlushSlaveController(IActionController& target)
{
	const uint32 numScopeContexts = m_context.controllerDef.m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numScopeContexts; i++)
	{
		SScopeContext& scopeContext = m_scopeContexts[i];
		if (scopeContext.pEnslavedController == &target)
		{
			scopeContext.pCharInst.reset();
			scopeContext.pDatabase = NULL;
			scopeContext.entityId = 0;
			scopeContext.pCachedEntity = NULL;
			scopeContext.pEnslavedController = NULL;

			//--- Disable scopes that use this context
			ActionScopes scopeFlag = 1;
			for (size_t s = 0; s < m_scopeCount; s++, scopeFlag <<= 1)
			{
				CActionScope& scope = m_scopeArray[s];
				if (scope.GetContextID() == i)
				{
					m_activeScopes &= ~scopeFlag;
					if (scope.m_pAction)
					{
						scope.m_pAction->m_installedScopeMask &= ~scopeFlag;
						scope.m_pAction.reset();
					}
				}
			}
		}
	}
}

void CActionController::SynchTagsToSlave(SScopeContext& scopeContext, bool enable)
{
	CActionController* const pTargetController = scopeContext.pEnslavedController;

	if (pTargetController)
	{
		const CTagDefinition& tagDefSlave = pTargetController->GetContext().controllerDef.m_tags;
		CTagState& targetTagState = pTargetController->GetContext().state;

		//--- Clear previous setting
		for (uint32 i = 0; i < tagDefSlave.GetNum(); i++)
		{
			if (tagDefSlave.IsSet(scopeContext.setTags, i))
			{
				targetTagState.Set(i, false);
			}
		}

		scopeContext.setTags = TAG_STATE_EMPTY;

		if (enable)
		{
			const CTagDefinition& tagDef = GetContext().controllerDef.m_tags;
			TagState sourceTagState = GetContext().state.GetMask();
			sourceTagState = tagDef.GetUnion(sourceTagState, GetContext().controllerDef.m_scopeContextDef[scopeContext.id].additionalTags);

			for (uint32 i = 0; i < tagDef.GetNum(); i++)
			{
				TagID groupID = tagDef.GetGroupID(i);
				const bool isKnown = ((groupID != TAG_ID_INVALID) && tagDef.IsGroupSet(scopeContext.sharedTags, groupID))
				                     || tagDef.IsSet(scopeContext.sharedTags, i);
				if (isKnown && (tagDef.IsSet(sourceTagState, i)))
				{
					int tagCRC = tagDef.GetTagCRC(i);
					TagID tagID = tagDefSlave.Find(tagCRC);
					targetTagState.Set(tagID, true);
					tagDefSlave.Set(scopeContext.setTags, tagID, true);
				}
			}
		}
	}
}

void CActionController::Reset()
{
	Flush();

	const uint32 numScopeContexts = (uint32)m_context.controllerDef.m_scopeContexts.GetNum();
	for (uint32 itContext = 0; itContext < numScopeContexts; ++itContext)
	{
		ClearScopeContext(itContext, false);
	}
}

void CActionController::Flush()
{
	if (m_flags & AC_IsInUpdate)
	{
		CryFatalError("Flushing the action controller in the middle of an update, this is bad!");
	}
	UpdateValidity();
	ActionScopes scopeFlag = 1;
	for (uint32 i=0; i<m_scopeCount; i++, scopeFlag <<= 1)
	{
		CActionScope& scope = m_scopeArray[i];
		FlushScope(i, scopeFlag);
	}

	const uint32 numQueuedActions = m_queuedActions.size();
	for (uint32 i = 0; i < numQueuedActions; i++)
	{
		IActionPtr pAction = m_queuedActions[i];
		CRY_ASSERT(pAction != NULL);

		if (pAction)
		{
			pAction->m_eStatus = IAction::None;
		}
	}
	m_queuedActions.clear();

	FlushProceduralContexts();
}

void CActionController::FlushProceduralContexts()
{
	uint32 numProcContexts = m_procContexts.size();
	for (uint32 i = 0; i < numProcContexts; i++)
	{
		m_procContexts[i].pContext.reset();
	}
	m_procContexts.clear();
}

void CActionController::RegisterListener(IMannequinListener* listener)
{
	m_listeners.push_back(listener);
}

void CActionController::UnregisterListener(IMannequinListener* listener)
{
	const uint32 numListeners = m_listeners.size();
	for (uint32 i = 0; i < numListeners; i++)
	{
		if (m_listeners[i] == listener)
		{
			m_listeners.erase(m_listeners.begin() + i);
			return;
		}
	}

	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Attempting to remove an unknown listener!");
}

void CActionController::EndAction(IAction& action, EFlushMethod flushMethod)
{
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CRY_ASSERT(m_scopeArray[i].m_pAction != &action);
	}

	switch (flushMethod)
	{
	case FM_Normal:
	case FM_NormalLeaveAnimations:
		if (action.Interrupt())
		{
			action.Initialise(m_context);

			if (action.m_flags & IAction::Requeued)
			{
				action.m_flags &= ~IAction::Requeued;
			}
			else
			{
				PushOntoQueue(action);
			}
		}
		else
		{
			if (action.m_flags & IAction::Requeued)
			{
				for (TActionList::iterator iter = m_queuedActions.begin(); iter != m_queuedActions.end(); ++iter)
				{
					if (*iter == &action)
					{
						m_queuedActions.erase(iter);
						break;
					}
				}
			}
		}
		break;
	case FM_Failure:
		action.Fail(AF_InvalidContext);
		break;
	default:
		CRY_ASSERT(false);
	}
}

void CActionController::StartAction(IAction& action)
{
	action.Enter();
}

ActionScopes CActionController::ExpandOverlappingScopes(ActionScopes scopeMask) const
{
	ActionScopes expandedScopeMask = scopeMask;
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		const CActionScope& scope = m_scopeArray[i];
		const IAction* const pScopeAction = scope.m_pAction.get();
		if (pScopeAction && (pScopeAction->m_eStatus != IAction::Exiting) && ((pScopeAction->GetInstalledScopeMask() & scopeMask) != 0))
		{
			expandedScopeMask |= pScopeAction->GetInstalledScopeMask();
		}
	}

	return expandedScopeMask;
}

ActionScopes CActionController::EndActionsOnScope(ActionScopes scopeMask, IAction* pPendingAction, bool blendOut, EFlushMethod flushMethod)
{
	TActionList deleteActionList;
	//--- Expand the scope mask to all overlapped scopes
	ActionScopes expandedScopeMask = ExpandOverlappingScopes(scopeMask);

	//--- Clean up scopes
	ActionScopes scopeFlag = 1;
	for (uint32 i=0; i<m_scopeCount; i++, scopeFlag<<=1)
	{
		CActionScope& scope = m_scopeArray[i];
		if (scope.m_pAction && (scope.m_pAction->GetInstalledScopeMask() & expandedScopeMask))
		{
			if ((scope.m_pAction->m_rootScope == &scope) && (pPendingAction != scope.m_pAction))
			{
				deleteActionList.push_back(scope.m_pAction);
			}

			scope.m_pAction = NULL;

			if (blendOut && (scope.m_scopeContext.pDatabase != NULL))
			{
				scope.BlendOutFragments();
			}
		}
	}

	//--- Clean up actions
	for (uint32 i = 0; i < deleteActionList.size(); i++)
	{
		EndAction(*deleteActionList[i]);
	}

	return expandedScopeMask;
}

void CActionController::PushOntoQueue(IAction& action)
{
	const int priority = action.GetPriority();
	const bool requeued = ((action.GetFlags() & IAction::Requeued) != 0);

	for (TActionList::iterator iter = m_queuedActions.begin(); iter != m_queuedActions.end(); ++iter)
	{
		const EPriorityComparison comparison = action.DoComparePriority(**iter);

		bool insertHere = false;
		if (comparison == Higher)
		{
			insertHere = true;
		}
		else if (comparison == Equal)
		{
			const bool otherRequeued = (((*iter)->GetFlags() & IAction::Requeued) != 0);

			if (requeued && !otherRequeued)
			{
				insertHere = true;
			}
		}

		if (insertHere)
		{
			m_queuedActions.insert(iter, &action);
			return;
		}
	}
	m_queuedActions.push_back(&action);
}

ActionScopes CActionController::QueryScopeMask(FragmentID fragID, const TagState& fragTags, const TagID subContext) const
{
	ActionScopes scopeMask = 0;

	SFragTagState fragTagState(m_context.state.GetMask(), fragTags);

	if (fragID != FRAGMENT_ID_INVALID)
	{
		scopeMask = m_context.controllerDef.GetScopeMask(fragID, fragTagState);
	}

	if (subContext != TAG_ID_INVALID)
	{
		scopeMask |= m_context.controllerDef.m_subContext[subContext].scopeMask;
	}

	return scopeMask;
}

void CActionController::Queue(IAction& action, float time)
{
	action.m_queueTime = time;
	action.Initialise(m_context);

	PushOntoQueue(action);
}

void CActionController::Requeue(IAction& action)
{
	CRY_ASSERT(action.GetStatus() == IAction::Installed);

	PushOntoQueue(action);
}

void CActionController::OnEvent(const SGameObjectEvent& event)
{
	UpdateValidity();

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CActionScope& scope = m_scopeArray[i];
		IAction* const pAction = scope.m_pAction.get();
		if (pAction && (&pAction->GetRootScope() == &scope))
		{
			pAction->OnEvent(event);
		}
	}
}

void CActionController::OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event)
{
	UpdateValidity();

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CActionScope& scope = m_scopeArray[i];
		IAction* const pAction = scope.m_pAction.get();
		if (pAction && (&pAction->GetRootScope() == &scope) && (pAction->GetStatus() == IAction::Installed))
		{
			pAction->OnAnimationEvent(pCharacter, event);
		}
	}
}

void CActionController::Record(const SMannHistoryItem& item)
{
	const uint32 numListeners = m_listeners.size();
#ifndef _RELEASE
	const float curTime = gEnv->pTimer->GetCurrTime();

	m_history[m_historySlot] = item;
	m_history[m_historySlot].time = curTime;
	m_historySlot = (m_historySlot + 1) % TOTAL_HISTORY_SLOTS;
#endif //_RELEASE

	for (uint32 i = 0; i < numListeners; i++)
	{
		m_listeners[i]->OnEvent(item, *this);
	}
}

void CActionController::RecordTagState()
{
	const TagState newTagState = m_context.state.GetMask();
	if (m_lastTagStateRecorded != newTagState)
	{
		SMannHistoryItem tagItem(newTagState);
		Record(tagItem);
		m_lastTagStateRecorded = newTagState;
	}
}

#ifndef _RELEASE

void CActionController::DumpHistory(const char* filename, float timeDelta) const
{
	XmlNodeRef root = GetISystem()->CreateXmlNode("History");

	float endTime = gEnv->pTimer->GetCurrTime();
	float startTime = (timeDelta > 0.0f) ? endTime - timeDelta : 0.0f;
	root->setAttr("StartTime", startTime);
	root->setAttr("EndTime", endTime);

	for (uint32 i = 0; i < TOTAL_HISTORY_SLOTS; i++)
	{
		uint32 slotID = (i + m_historySlot + 1) % TOTAL_HISTORY_SLOTS;
		const SMannHistoryItem& item = m_history[slotID];

		if (item.time >= startTime)
		{
			//--- Save slot out
			XmlNodeRef event = GetISystem()->CreateXmlNode("Item");
			event->setAttr("Time", item.time);
			switch (item.type)
			{
			case SMannHistoryItem::Fragment:
				{
					if (item.fragment != FRAGMENT_ID_INVALID)
					{
						event->setAttr("FragmentID", m_context.controllerDef.m_fragmentIDs.GetTagName(item.fragment));
						const CTagDefinition* tagDef = m_context.controllerDef.GetFragmentTagDef(item.fragment);
						if ((item.tagState != TAG_STATE_EMPTY) && tagDef)
						{
							CryStackStringT<char, 512> sTagState;
							tagDef->FlagsToTagList(item.tagState, sTagState);
							event->setAttr("TagState", sTagState.c_str());
						}
					}
					CryStackStringT<char, 512> sScopeMask;
					m_context.controllerDef.m_scopeIDs.IntegerFlagsToTagList(item.scopeMask, sScopeMask);
					event->setAttr("ScopeMask", sScopeMask.c_str());
					event->setAttr("OptionIdx", item.optionIdx);
					event->setAttr("Trump", item.trumpsPrevious);
					break;
				}
			case SMannHistoryItem::Tag:
				{
					CryStackStringT<char, 512> sTagState;
					m_context.controllerDef.m_tags.FlagsToTagList(item.tagState, sTagState);
					event->setAttr("TagState", sTagState.c_str());
					break;
				}
			case SMannHistoryItem::None:
				continue;
				break;
			}

			root->addChild(event);
		}
	}

	const bool success = root->saveToFile(filename);
	if (!success)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Error saving CryMannequin history to '%s'", filename);
	}
}

void CActionController::DebugDrawLocation(const QuatT& location, ColorB colorPos, ColorB colorX, ColorB colorY, ColorB colorZ) const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const float thickness = 7.0f;
	const Vec3 pushUp(0.0f, 0.03f, 0.0f);

	pAuxGeom->DrawLine(location.t + pushUp, colorX, location.t + pushUp + location.q.GetColumn0(), colorX, thickness);
	pAuxGeom->DrawLine(location.t + pushUp, colorY, location.t + pushUp + location.q.GetColumn1(), colorY, thickness);
	pAuxGeom->DrawLine(location.t + pushUp, colorZ, location.t + pushUp + location.q.GetColumn2(), colorZ, thickness);

	const float radius = 0.06f;
	pAuxGeom->DrawSphere(location.t + pushUp, radius, colorPos);
}

float g_mannequinYPosEnd = 0;

void CActionController::DebugDraw() const
{
	g_mannequinYPosEnd = 0;

	static float XPOS_SCOPELIST = 50.0f;
	static float XPOS_SCOPEACTIONLIST = 250.0f;
	static float XPOS_QUEUE = 900.0f;
	static float XINC_PER_LETTER = 6.0f;
	static float YPOS = 10.0f;
	static float XPOS_SEQUENCE = XPOS_SCOPELIST + 8.0f;
	static float YINC = 12.0f;
	static float YINC_SEQUENCE = 12.0f;
	static float YINC_SCOPES = 4.0f;
	static float FONT_SIZE = 1.3f;
	static float FONT_SIZE_ANIMLIST = 1.2f;
	static float FONT_COLOUR[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float FONT_COLOUR_INACTIVE[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	static float FONT_COLOUR_ANIM_PENDING[4] = { 1.0f, 1.0f, 0.5f, 1.0f };
	static float FONT_COLOUR_ANIM_INSTALLED[4] = { 0.5f, 1.0f, 0.5f, 1.0f };
	static float FONT_COLOUR_PROC_PENDING[4] = { 0.5f, 1.0f, 1.0f, 1.0f };
	static float FONT_COLOUR_PROC_INSTALLED[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
	static float FONT_COLOUR_ACTION_QUEUE[4] = { 0.5f, 1.0f, 0.5f, 1.0f };

	float ypos = YPOS;

	const SControllerDef& controllerDef = m_context.controllerDef;
	if (m_flags & AC_PausedUpdate)
	{
		IRenderAuxText::Draw2dLabel(XPOS_SCOPELIST, ypos, FONT_SIZE, FONT_COLOUR, false, "[PAUSED ACTION CONTROLLER]");
		ypos += YINC;
	}
	CryStackStringT<char, 1024> sTagState;
	m_context.controllerDef.m_tags.FlagsToTagList(m_context.state.GetMask(), sTagState);
	IRenderAuxText::Draw2dLabel(XPOS_SCOPELIST, ypos, FONT_SIZE, FONT_COLOUR, false, "TagState: %s", sTagState.c_str());
	ypos += YINC;

	for (uint32 i = 0; i < m_context.subStates.size(); i++)
	{
		CryStackStringT<char, 1024> sSubState;
		m_context.controllerDef.m_tags.FlagsToTagList(m_context.subStates[i].GetMask(), sSubState);
		IRenderAuxText::Draw2dLabel(XPOS_SCOPELIST, ypos, FONT_SIZE, FONT_COLOUR, false, "%s: %s", controllerDef.m_subContextIDs.GetTagName(i), sSubState.c_str());
		ypos += YINC;
	}
	ypos += YINC;

	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		CryStackStringT<char, 1024> sMessage;
		const CActionScope& scope = m_scopeArray[i];

		FragmentID fragID = scope.GetLastFragmentID();
		const char* fragName = (fragID != FRAGMENT_ID_INVALID) ? controllerDef.m_fragmentIDs.GetTagName(fragID) : "NoFragment";
		if (scope.HasFragment())
		{
			controllerDef.m_tags.FlagsToTagList(scope.m_lastFragSelection.tagState.globalTags, sMessage);

			const CTagDefinition* pFragTags = controllerDef.GetFragmentTagDef(fragID);

			if (pFragTags && (scope.m_lastFragSelection.tagState.fragmentTags != TAG_STATE_EMPTY))
			{
				CryStackStringT<char, 1024> sFragmentTags;
				pFragTags->FlagsToTagList(scope.m_lastFragSelection.tagState.fragmentTags, sFragmentTags);
				if (!sFragmentTags.empty())
				{
					CryStackStringT<char, 1024> sLastOptionIdx;
					sLastOptionIdx.Format("%u", scope.GetLastOptionIdx());

					if (scope.m_lastFragSelection.tagState.globalTags != TAG_STATE_EMPTY)
						sMessage += "+";
					sMessage += "[" + sFragmentTags + "] option " + sLastOptionIdx;
				}
			}
		}
		else
		{
			sMessage = "No Match: ";

			if (scope.m_pAction)
			{
				// scope tags (aka additional tags)
				CryStackStringT<char, 1024> sAdditionalTags;
				controllerDef.m_tags.FlagsToTagList(scope.m_additionalTags, sAdditionalTags);
				sMessage += sAdditionalTags;

				// scope context tags (aka additional tags)
				CryStackStringT<char, 1024> sScopeContext;
				controllerDef.m_tags.FlagsToTagList(controllerDef.m_scopeContextDef[scope.m_scopeContext.id].additionalTags, sScopeContext);
				sMessage += sScopeContext;

				// frag tags (aka fragment-specific tags)
				FragmentID fragIDNotMatched = scope.m_pAction->GetFragmentID();
				if (fragIDNotMatched != FRAGMENT_ID_INVALID)
				{
					const CTagDefinition* pFragTags = controllerDef.GetFragmentTagDef(fragIDNotMatched);

					if (pFragTags && scope.m_pAction->GetFragTagState() != TAG_STATE_EMPTY)
					{
						CryStackStringT<char, 1024> sFragmentTags;
						pFragTags->FlagsToTagList(scope.m_pAction->GetFragTagState(), sFragmentTags);
						if (!sFragmentTags.empty())
						{
							CryStackStringT<char, 1024> sLastOptionIdx;
							sLastOptionIdx.Format("%u", scope.GetLastOptionIdx());
							if (scope.m_additionalTags != TAG_STATE_EMPTY)
								sMessage += "+";
							sMessage += "[" + sFragmentTags + "] option " + sLastOptionIdx;
						}
					}
				}
			}
			else
			{
				sMessage = "No Action";
			}
		}

		float *colour = (m_activeScopes & BIT64(1<<i)) ? FONT_COLOUR : FONT_COLOUR_INACTIVE;
		IRenderAuxText::Draw2dLabel(XPOS_SCOPELIST, ypos, FONT_SIZE, colour, false, "%s:", scope.m_name.c_str());

		CryStackStringT<char, 1024> sExitingAction;
		if (scope.m_pExitingAction)
			sExitingAction = scope.m_pExitingAction->GetName();

		sExitingAction += scope.m_pAction ? scope.m_pAction->GetName() : " --- ";
		if (scope.m_pAction)
			IRenderAuxText::Draw2dLabel(XPOS_SCOPEACTIONLIST, ypos, FONT_SIZE, colour, false, "%s \t%s(%s)\tP: %d %d TR: %f", sExitingAction.c_str(), fragName, sMessage.c_str(), scope.m_pAction ? scope.m_pAction->GetPriority() : 0, scope.m_pAction ? scope.m_pAction->m_refCount : 0, scope.CalculateFragmentTimeRemaining());
		else
			IRenderAuxText::Draw2dLabel(XPOS_SCOPEACTIONLIST, ypos, FONT_SIZE, colour, false, "%s", sExitingAction.c_str());
		ypos += YINC;

		if (scope.m_scopeContext.pCharInst)
		{
			IAnimationSet* animSet = scope.m_scopeContext.pCharInst->GetIAnimationSet();
			for (uint32 l = 0; l < scope.m_numLayers; l++)
			{
				const CActionScope::SSequencer& sequencer = scope.m_layerSequencers[l];

				if (sequencer.sequence.size() > 0)
				{
					float xpos = XPOS_SEQUENCE;
					for (uint32 k = 0; k < sequencer.sequence.size(); k++)
					{
						const char* animName = sequencer.sequence[k].animation.animRef.c_str();
						int letterCount = animName ? strlen(animName) : 0;
						float* colourA = FONT_COLOUR;
						if (k == sequencer.pos)
						{
							colourA = FONT_COLOUR_ANIM_PENDING;
						}
						else if (k == sequencer.pos - 1)
						{
							colourA = FONT_COLOUR_ANIM_INSTALLED;
						}
						IRenderAuxText::Draw2dLabel(xpos, ypos, FONT_SIZE_ANIMLIST, colourA, false, "%s", animName);
						xpos += XPOS_SCOPELIST + (letterCount * XINC_PER_LETTER);
					}
					ypos += YINC_SEQUENCE;
				}
			}
		}

		const uint32 numProcLayers = scope.m_procSequencers.size();
		for (uint32 l = 0; l < numProcLayers; l++)
		{
			const CActionScope::SProcSequencer& procSeq = scope.m_procSequencers[l];

			if (procSeq.sequence.size() > 0)
			{
				float xpos = XPOS_SEQUENCE;
				for (uint32 k = 0; k < procSeq.sequence.size(); k++)
				{
					const SProceduralEntry& procEntry = procSeq.sequence[k];

					const bool isNone = procEntry.IsNoneType();
					if (!isNone)
					{
						const char* typeName = mannequin::FindProcClipTypeName(procEntry.typeNameHash);
						int letterCount = typeName ? strlen(typeName) : 0;
						float* colourA = FONT_COLOUR;
						if (k == procSeq.pos)
						{
							colourA = FONT_COLOUR_PROC_PENDING;
						}
						else if (k == procSeq.pos - 1)
						{
							colourA = FONT_COLOUR_PROC_INSTALLED;
						}

						CryStackStringT<char, 1024> sTypename(typeName);
						IProceduralParams::StringWrapper infoString;
						if (procEntry.pProceduralParams)
						{
							procEntry.pProceduralParams->GetExtraDebugInfo(infoString);
						}
						if (!infoString.IsEmpty())
							sTypename += "(" + CryStackStringT<char, 1024>(infoString.c_str()) + ")";

						IRenderAuxText::Draw2dLabel(xpos, ypos, FONT_SIZE_ANIMLIST, colourA, false, "%s", sTypename.c_str());
						xpos += XPOS_SCOPELIST + (letterCount * XINC_PER_LETTER);
					}
				}
				ypos += YINC_SEQUENCE;
			}
		}

		ypos += YINC_SCOPES;

		if (scope.m_pAction)
		{
			QuatT targetPos(IDENTITY);

			const bool success = scope.m_pAction->GetParam("TargetPos", targetPos);
			if (success)
			{
				DebugDrawLocation(
				  targetPos,
				  RGBA8(0x80, 0x80, 0x80, 0xff),
				  RGBA8(0xb0, 0x80, 0x80, 0xff),
				  RGBA8(0x80, 0xb0, 0x80, 0xff),
				  RGBA8(0x80, 0x80, 0xb0, 0xff));
			}
		}
	}

	g_mannequinYPosEnd = ypos;

	ypos = YPOS;
	IRenderAuxText::Draw2dLabel(XPOS_QUEUE, ypos, FONT_SIZE, FONT_COLOUR_ACTION_QUEUE, false, "Pending Action Queue");
	ypos += YINC * 2.0f;
	for (uint32 i = 0; i < m_queuedActions.size(); i++)
	{
		CryStackStringT<char, 1024> sScopes;
		const IAction& action = *m_queuedActions[i];
		bool isPending = action.GetStatus() == IAction::Pending;

		bool first = true;
		for (uint32 k = 0; k < m_scopeCount; k++)
		{
			if (BIT64(k) & action.GetForcedScopeMask())
			{
				if (first)
				{
					sScopes = m_scopeArray[k].m_name;
				}
				else
				{
					sScopes += "|" + m_scopeArray[k].m_name;
				}

				first = false;
			}
		}
		FragmentID fragID = action.GetFragmentID();
		const char* fragName = (fragID != FRAGMENT_ID_INVALID) ? m_context.controllerDef.m_fragmentIDs.GetTagName(fragID) : "NoFragment";
		IRenderAuxText::Draw2dLabel(XPOS_QUEUE, ypos, FONT_SIZE, isPending ? FONT_COLOUR_ACTION_QUEUE : FONT_COLOUR, false, "%s: %s P: %d - %s", action.GetName(), fragName, action.GetPriority(), sScopes.c_str());

		ypos += YINC;
	}

	{
		QuatT targetPos(IDENTITY);

		const bool success = m_mannequinParams.GetParam("TargetPos", targetPos);
		if (success)
		{
			DebugDrawLocation(
			  targetPos,
			  RGBA8(0xa0, 0xa0, 0xa0, 0xff),
			  RGBA8(0xc0, 0xa0, 0xa0, 0xff),
			  RGBA8(0xa0, 0xc0, 0xa0, 0xff),
			  RGBA8(0xa0, 0xa0, 0xc0, 0xff));
		}
	}

	if (m_cachedEntity && gEnv->pRenderer)
	{
		AABB bbox;
		m_cachedEntity->GetWorldBounds(bbox);
		const Vec3 entityWorldPos = m_cachedEntity->GetWorldPos();
		const Vec3 debugPos = Vec3(entityWorldPos.x, entityWorldPos.y, bbox.max.z);
		const float radius = 0.1f;
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		pAuxGeom->DrawSphere(debugPos, radius, RGBA8(0xff, 0x00, 0x00, 0xff));
	}
}

void CActionController::GetStateString(string& state) const
{

	state += "[ ";
	for (uint32 i = 0; i < m_scopeCount; i++)
	{
		state += "{ ";

		const CActionScope& scope = m_scopeArray[i];
		state += "\"scope_id\" : \"";
		state += m_context.controllerDef.m_scopeIDs.GetTagName(i);
		state += "\", ";

		state += "\"action\" : \"";
		state += scope.GetAction() ? scope.GetAction()->GetName() : "None";
		state += "\", ";

		state += "\"fragment\" : \"";
		state += (scope.GetLastFragmentID() != FRAGMENT_ID_INVALID) ? m_context.controllerDef.m_fragmentIDs.GetTagName(scope.GetLastFragmentID()) : "None";
		state += "\"";

		state += "} ";

		if (i < (m_scopeCount - 1))
		{
			state += ", ";
		}
	}

	state += "] ";

}

#endif //!_RELEASE

IProceduralContext* CActionController::FindOrCreateProceduralContext(const CryClassID& contextId)
{
	IProceduralContext* procContext = FindProceduralContext(contextId);
	if (procContext)
		return procContext;

	return CreateProceduralContext(contextId);
}

IProceduralContext* CActionController::CreateProceduralContext(const CryClassID& contextId)
{
	const bool hasValidRootEntity = UpdateRootEntityValidity();
	if (!hasValidRootEntity)
		return NULL;

	SProcContext newProcContext;
	newProcContext.contextId = contextId;
	CryCreateClassInstance<IProceduralContext>(contextId, newProcContext.pContext);
	if (newProcContext.pContext)
	{
		m_procContexts.push_back(newProcContext);

		newProcContext.pContext->Initialise(*m_cachedEntity, *this);
	}
	
	return newProcContext.pContext.get();
}

IProceduralContext* CActionController::FindProceduralContext(const CryClassID& contextId)
{
	for (auto& proceduralContext : m_procContexts)
	{
		if (proceduralContext.contextId.hipart == contextId.hipart
			&& proceduralContext.contextId.lopart == contextId.lopart)
		{
			return proceduralContext.pContext.get();
		}
	}
	return nullptr;
}

const IProceduralContext* CActionController::FindProceduralContext(const CryClassID& contextId) const
{
	for(auto& proceduralContext : m_procContexts)
	{
		if (proceduralContext.contextId.hipart == contextId.hipart
			&& proceduralContext.contextId.lopart == contextId.lopart)
		{
			return proceduralContext.pContext.get();
		}
	}
	return nullptr;
}

bool CActionController::IsActionPending(uint32 userToken) const
{
	for (uint32 i = 0; i < m_queuedActions.size(); i++)
	{
		const IAction& action = *m_queuedActions[i];
		if ((action.GetStatus() == IAction::Pending) && (userToken == action.GetUserToken()))
			return true;
	}
	return false;
}

void CActionController::Pause()
{
	if (!GetFlag(AC_PausedUpdate))
	{
		SetFlag(AC_PausedUpdate, true);
		for (size_t i = 0; i < m_scopeCount; ++i)
		{
			m_scopeArray[i].Pause();
		}
	}
}

void CActionController::Resume(uint32 resumeFlags)
{
	if (GetFlag(AC_PausedUpdate))
	{
		SetFlag(AC_PausedUpdate, false);
		const bool restartAnimations = ((resumeFlags & ERF_RestartAnimations) != 0);
		if (restartAnimations)
		{
			for (size_t i = 0; i < m_scopeCount; ++i)
			{
				CActionScope& scope = m_scopeArray[i];
				const float blendTime = -1;
				scope.Resume(blendTime, resumeFlags);
			}
		}
	}
}

ActionScopes CActionController::FlushScopesByScopeContext(uint32 scopeContextID, EFlushMethod flushMethod)
{
	ActionScopes scopesUsingContext = 0;
	ActionScopes scopeFlag = 1;
	for (size_t i = 0; i < m_scopeCount; i++, scopeFlag <<= 1)
	{
		CActionScope& scope = m_scopeArray[i];
		if (scope.GetContextID() == scopeContextID)
		{
			FlushScope(i, scopeFlag, flushMethod);

			scopesUsingContext |= scopeFlag;
		}
	}

	return scopesUsingContext;
}

void CActionController::UpdateValidity()
{
	// Validate root entity
	const bool rootEntityIsValid = UpdateRootEntityValidity();

	// Validate all scopecontexts (and keep track of which ones are invalid)
	const uint32 numScopeContexts = m_context.controllerDef.m_scopeContexts.GetNum();
	uint32 scopeContextsToFlush = 0;
	for (uint32 scopeContextID = 0; scopeContextID < numScopeContexts; scopeContextID++)
	{
		const bool scopeContextIsValid = UpdateScopeContextValidity(scopeContextID);
		if (!rootEntityIsValid || !scopeContextIsValid)
		{
			scopeContextsToFlush |= BIT64(scopeContextID);
		}
	}

	// Flush the invalid scopecontexts
	if (scopeContextsToFlush)
	{
		for (uint32 scopeContextID = 0; scopeContextID < numScopeContexts; scopeContextID++)
		{
			if (scopeContextsToFlush & BIT64(scopeContextID))
			{
				ActionScopes flushedScopes = FlushScopesByScopeContext(scopeContextID, FM_Failure);
				m_activeScopes &= ~flushedScopes;
			}
		}
	}
}

bool CActionController::UpdateScopeContextValidity(uint32 scopeContextID)
{
	SScopeContext& scopeContext = m_scopeContexts[scopeContextID];

	const bool hasInvalidEntity = scopeContext.HasInvalidEntity();
	if (hasInvalidEntity)
	{
#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
		IEntity* expectedEntity = gEnv->pEntitySystem->GetEntity(scopeContext.entityId);
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[CActionController::UpdateScopeContextValidity] Dangling Entity %p (expected %p for id=%u) in context '%s'", (void*)scopeContext.pCachedEntity, (void*)expectedEntity, scopeContext.entityId, m_context.controllerDef.m_scopeContexts.GetTagName(scopeContextID));
		CRY_ASSERT_MESSAGE(0,"[CActionController::UpdateScopeContextValidity] Dangling Entity");
#endif // !CRYMANNEQUIN_WARN_ABOUT_VALIDITY()

		scopeContext.pCharInst = NULL;
		scopeContext.entityId = 0;
		scopeContext.pCachedEntity = NULL;
	}

	const bool hasInvalidCharInst = scopeContext.HasInvalidCharInst();
	if (hasInvalidCharInst)
	{
#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
		const char* entityName = (scopeContext.pCachedEntity ? scopeContext.pCachedEntity->GetName() : "<NULL>");
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[CActionController::UpdateScopeContextValidity] Dangling Char Inst in entity '%s' (id=%d) in context '%s'", entityName, scopeContext.entityId, m_context.controllerDef.m_scopeContexts.GetTagName(scopeContextID));
		CRY_ASSERT_MESSAGE(0,"[CActionController::UpdateScopeContextValidity] Dangling Char Inst in entity ");
#endif // !CRYMANNEQUIN_WARN_ABOUT_VALIDITY()

		scopeContext.pCharInst = NULL;
	}

	return !(hasInvalidEntity || hasInvalidCharInst);
}

bool CActionController::UpdateRootEntityValidity()
{
	const IEntity* expectedEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
	const bool hasValidRootEntity = (expectedEntity == m_cachedEntity);
	if (!hasValidRootEntity)
	{
#if CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[CActionController::UpdateRootEntityValidity] Dangling Entity %p (expected %p for id=%u) in actioncontroller for '%s'", (void*)m_cachedEntity, (void*)expectedEntity, m_entityId, m_context.controllerDef.m_filename.c_str());
		CRY_ASSERT_MESSAGE(0,"[CActionController::UpdateRootEntityValidity] Dangling Entity");
#endif // !CRYMANNEQUIN_WARN_ABOUT_VALIDITY()
		m_entityId = 0;
		m_cachedEntity = NULL;
	}

	return hasValidRootEntity && (m_cachedEntity || m_entityId == 0);
}

QuatT CActionController::ExtractLocalAnimLocation(FragmentID fragID, TagState fragTags, uint32 scopeID, uint32 optionIdx)
{
	QuatT targetLocation(IDENTITY);

	if (IScope* pScope = GetScope(scopeID))
	{
		IAction* pCurrentAction = pScope->GetAction();
		ICharacterInstance* pCharInst = pScope->GetCharInst();

		const IAnimationDatabase& animDatabase = pScope->GetDatabase();
		const IAnimationSet* pAnimationSet = pCharInst ? pCharInst->GetIAnimationSet() : NULL;

		if (pAnimationSet)
		{
			SFragmentData fragData;
			SBlendQuery blendQuery;

			blendQuery.fragmentFrom = pCurrentAction ? pCurrentAction->GetFragmentID() : FRAGMENT_ID_INVALID;
			blendQuery.fragmentTo = fragID;

			blendQuery.tagStateFrom.globalTags = blendQuery.tagStateTo.globalTags = GetContext().state.GetMask();
			blendQuery.tagStateFrom.fragmentTags = pCurrentAction ? pCurrentAction->GetFragTagState() : TAG_STATE_EMPTY;
			blendQuery.tagStateTo.fragmentTags = fragTags;

			blendQuery.flags = SBlendQuery::fromInstalled | SBlendQuery::toInstalled;

			animDatabase.Query(fragData, blendQuery, optionIdx, pAnimationSet);

			if (!fragData.animLayers.empty() && !fragData.animLayers[0].empty())
			{
				int animID = pAnimationSet->GetAnimIDByCRC(fragData.animLayers[0][0].animation.animRef.crc);
				if (animID >= 0)
				{
					pAnimationSet->GetAnimationDCCWorldSpaceLocation(animID, targetLocation);
				}
			}
		}
	}

	return targetLocation;
}

void CActionController::SetTimeScale(float timeScale)
{
	const uint32 numContexts = m_context.controllerDef.m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numContexts; ++i)
	{
		const SScopeContext& scopeContext = m_scopeContexts[i];
		if (scopeContext.pCharInst)
		{
			scopeContext.pCharInst->SetPlaybackScale(timeScale);
		}
	}
	m_timeScale = timeScale;
}
