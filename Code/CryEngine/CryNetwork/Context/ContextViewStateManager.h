// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CONTEXTVIEWSTATEMANAGER_H__
#define __CONTEXTVIEWSTATEMANAGER_H__

#include "Config.h"

class CContextViewStateManager
{
public:
	virtual ~CContextViewStateManager(){}
	CContextViewStateManager();

	void LockStateChanges(const char* whom)
	{
#if !CRYNETWORK_RELEASEBUILD
		m_lockers[whom]++;
#endif
		m_locks++;
		NET_ASSERT(m_locks);
	}
	void UnlockStateChanges(const char* whom)
	{
#if !CRYNETWORK_RELEASEBUILD
		NET_ASSERT(m_lockers[whom]);
		m_lockers[whom]--;
#endif
		NET_ASSERT(m_locks);
		m_locks--;
		if (!m_locks && HasPendingStateChange(false))
			OnNeedToSendStateInformation(true);
	}

	bool IsInState(EContextViewState state) const
	{
		return
		  m_remoteState == state &&
		  m_remoteFinished == false &&
		  m_localState == state &&
		  (m_localStateState == eLSS_Acked || (m_localStateState == eLSS_FinishedSet && m_locks));
	}
	bool IsPastOrInState(EContextViewState state) const
	{
		if (m_remoteState < state)
			return false;
		if (m_localState == state)
			return m_localStateState >= eLSS_Acked;
		return m_localState > state;
	}
	bool IsBeforeState(EContextViewState state) const
	{
		return m_localState < state && m_remoteState < state;
	}

	// should be protected, but needed by some of the async callbacks in CNetContext
	void FinishLocalState();

	int  GetStateDebugCode()
	{
		int local = 1 + int(m_localState) - int(eCVS_Initial);
		int lss = 1 + int(m_localStateState) - int(eLSS_Set);
		int remote = 1 + int(m_remoteState) - int(eCVS_Initial);
		return 1000 * remote + 100 * local + 10 * CLAMP(m_locks, 0, 9) + lss;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CContextViewStateManager");

		pSizer->Add(*this);
	}

protected:
	void         SetLocalState(EContextViewState state);
	bool         ShouldSendLocalState(EContextViewState& state, bool ignoreLocks);
	void         SendLocalState();
	bool         AckLocalState(bool bAck);
	bool         ShouldSendFinishLocalState(bool ignoreLocks);
	void         SendFinishLocalState();
	void         AckFinishLocalState(bool bAck);

	bool         SetRemoteState(EContextViewState state);
	bool         FinishRemoteState();

	virtual bool EnterState(EContextViewState state) = 0;
	virtual void ExitState(EContextViewState state) = 0;
	virtual void OnNeedToSendStateInformation(bool urgent) = 0;
	virtual void OnViewStateDisconnect(const char* message) = 0;

	bool         HasPendingStateChange(bool ignoreLocks)
	{
		EContextViewState notUsed;
		return ShouldSendFinishLocalState(ignoreLocks) || ShouldSendLocalState(notUsed, ignoreLocks);
	}

	void DumpLockers(const char* name);

private:
	enum ELocalStateState
	{
		eLSS_Set,
		eLSS_Sent,
		eLSS_Acked,
		eLSS_FinishedSet,
		eLSS_FinishedSent,
		eLSS_FinishedAcked
	};

	EContextViewState m_localState;
	EContextViewState m_remoteState;
	bool              m_remoteFinished;
	ELocalStateState  m_localStateState;
	int               m_locks;

	bool MaybeEnterState();
	void MaybeExitState();

	void Verify(bool condition, const char* message);
	void VerifyStateTransition(EContextViewState from, EContextViewState to) { Verify(ValidateStateTransition(from, to), "Invalid state transition"); }
	bool ValidateStateTransition(EContextViewState from, EContextViewState to);
	void Dump(const char* msg);
#if !CRYNETWORK_RELEASEBUILD
	std::map<string, int> m_lockers;
#endif
};

class CChangeStateLock
{
public:
	CChangeStateLock() : m_pContext(0), m_name("BAD") {}
	CChangeStateLock(CContextViewStateManager* pContext, const char* name) : m_pContext(pContext), m_name(name)
	{
		m_pContext->LockStateChanges(m_name);
	}
	CChangeStateLock(const CChangeStateLock& lk) : m_pContext(lk.m_pContext), m_name(lk.m_name)
	{
		if (m_pContext)
			m_pContext->LockStateChanges(m_name);
	}
	~CChangeStateLock()
	{
		if (m_pContext)
			m_pContext->UnlockStateChanges(m_name);
	}
	void Swap(CChangeStateLock& lk)
	{
		std::swap(m_pContext, lk.m_pContext);
		std::swap(m_name, lk.m_name);
	}
	CChangeStateLock& operator=(CChangeStateLock lk)
	{
		Swap(lk);
		return *this;
	}
	bool IsLocking() const
	{
		return m_pContext != 0;
	}

private:
	CContextViewStateManager* m_pContext;
	string                    m_name;
};

#endif
