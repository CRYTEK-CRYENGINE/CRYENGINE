// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CONTEXTESTABLISHER_H__
#define __CONTEXTESTABLISHER_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include "Config.h"
#include "Protocol/NetChannel.h"

class CContextEstablisher : public IContextEstablisher, public _reference_target_t
{
public:
	CContextEstablisher();
	~CContextEstablisher();

	void OnFailDisconnect(CNetChannel* pChannel) { m_disconnectOnFail = pChannel; }

	void AddTask(EContextViewState state, IContextEstablishTask* pTask);
	void Start();
	bool StepTo(SContextEstablishState& state);
	void Fail(EDisconnectionCause cause, const string& reason);
	bool IsDone() { return m_done || m_offset < 0 || m_offset == m_tasks.size(); }
	void Done();

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CContextEstablisher");

		pSizer->Add(*this);
		pSizer->AddContainer(m_tasks);
	}

#ifndef _RELEASE
	void OutputTiming(void);
#endif
#if ENABLE_DEBUG_KIT
	void DebugDraw();
#endif

	const string& GetFailureReason() { return m_failureReason; }

private:
	EContextEstablishTaskResult PerformSteps(int& offset, SContextEstablishState& state);

	struct STask
	{
		STask() : state(eCVS_Initial), pTask(0)
#ifndef _RELEASE
			, numRuns(0)
			, done(0.0f)
#endif
		{
		}
		STask(EContextViewState s, IContextEstablishTask* p) : state(s), pTask(p)
#ifndef _RELEASE
			, numRuns(0)
			, done(0.0f)
#endif
		{
		}

		EContextViewState      state;
		IContextEstablishTask* pTask;

#ifndef _RELEASE
		int        numRuns;
		CTimeValue done;
#endif

		bool operator<(const STask& rhs) const
		{
			return state < rhs.state;
		}
	};
	// -1 if not started, otherwise how far down are we
	int                     m_offset;
	std::vector<STask>      m_tasks;
	bool                    m_done;

	string                  m_failureReason;
	int                     m_debuggedFrame;

	_smart_ptr<CNetChannel> m_disconnectOnFail;

	CTimeValue              m_begin;
};

typedef _smart_ptr<CContextEstablisher> CContextEstablisherPtr;

#endif
