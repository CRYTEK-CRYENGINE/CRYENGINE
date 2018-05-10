// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEditor.h>
#include <IPostRenderer.h>
#include <CryUQS/Client/ClientIncludes.h>
#include "CentralEventManager.h"


class CSimulatedRuntimeParam
{
public:

	CSimulatedRuntimeParam();
	explicit CSimulatedRuntimeParam(const char* szName, UQS::Client::IItemFactory& itemFactory);
	CSimulatedRuntimeParam(const CSimulatedRuntimeParam& rhs);
	CSimulatedRuntimeParam(CSimulatedRuntimeParam&& rhs);
	CSimulatedRuntimeParam& operator=(const CSimulatedRuntimeParam& rhs);
	CSimulatedRuntimeParam& operator=(CSimulatedRuntimeParam&& rhs);
	~CSimulatedRuntimeParam();

	bool Serialize(Serialization::IArchive& archive);

	const char*                GetName() const { return m_name.c_str(); }
	UQS::Client::IItemFactory& GetItemFactory() const { assert(m_pItemFactory); return *m_pItemFactory; }
	void*                      GetItem() const { assert(m_pItem); return m_pItem; }

private:

	string                     m_name;
	UQS::Client::IItemFactory* m_pItemFactory;
	void*                      m_pItem;
};

/////////////////////////////////////////////////////////////////////

class CQuerySimulator : public IAutoEditorNotifyListener
{
private:

	class CUQSHistoryPostRenderer : public IPostRenderer
	{
	public:

		// IPostRenderer
		virtual void OnPostRender() const override final;
		// ~IPostRenderer

		void SetEnablePeriodicRendering(bool bEnable);

	private:

		bool m_bPeriodicRenderingEnabled = false;
	};

	enum class ERunState
	{
		NotRunningAtAll,
		RunOnlyOnce,
		RunInfinitely,
	};

	enum class ESingleStepState
	{
		NotSingleSteppingAtAll,
		WaitingForNextTrigger,
		UpdateUQSExactlyOnce,
	};

public:

	CQuerySimulator();
	~CQuerySimulator();

	// IAutoEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override final;
	// ~IAutoEditorNotifyListener

	void OnStartOrStopQuerySimulator(const CCentralEventManager::SStartOrStopQuerySimulatorEventArgs& args);
	void OnSingleStepQuerySimulatorOnce(const CCentralEventManager::SSingleStepQuerySimulatorOnceEventArgs& args);

private:

	bool StartQuery();
	void EnsureStopRunning();
	void OnQueryHostFinished(void* pUserData);

private:

	UQS::Client::CQueryHost m_queryHost;
	ERunState m_runState;
	ESingleStepState m_singleStepState;
	CUQSHistoryPostRenderer* m_pHistoryPostRenderer;
};
