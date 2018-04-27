// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QuerySimulator.h"

#include <QMessageBox>

// these 2 #includes are only required for <Viewport.h> below
#include <QCursor>
#include <QEvent>

#include <IViewportManager.h>
#include <Viewport.h>
#include <QtUtil.h>


CSimulatedRuntimeParam::CSimulatedRuntimeParam()
	: m_pItemFactory(nullptr)
	, m_pItem(nullptr)
{
}

CSimulatedRuntimeParam::CSimulatedRuntimeParam(const char* szName, UQS::Client::IItemFactory& itemFactory)
	: m_name(szName)
	, m_pItemFactory(&itemFactory)
{
	m_pItem = m_pItemFactory->CreateItems(1, UQS::Client::IItemFactory::EItemInitMode::UseUserProvidedFunction);
}

CSimulatedRuntimeParam::CSimulatedRuntimeParam(const CSimulatedRuntimeParam& rhs)
	: m_name(rhs.m_name)
	, m_pItemFactory(rhs.m_pItemFactory)
{
	m_pItem = m_pItemFactory->CloneItem(rhs.m_pItem);
}

CSimulatedRuntimeParam::CSimulatedRuntimeParam(CSimulatedRuntimeParam&& rhs)
	: m_name(std::move(rhs.m_name))
	, m_pItemFactory(rhs.m_pItemFactory)
	, m_pItem(rhs.m_pItem)
{
	rhs.m_pItemFactory = nullptr;
	rhs.m_pItem = nullptr;
}

CSimulatedRuntimeParam& CSimulatedRuntimeParam::operator=(const CSimulatedRuntimeParam& rhs)
{
	if (this != &rhs)
	{
		assert(m_pItemFactory);
		assert(rhs.m_pItemFactory);
		assert(rhs.m_pItem);

		m_name = rhs.m_name;
		m_pItemFactory->DestroyItems(m_pItem);
		m_pItemFactory = rhs.m_pItemFactory;
		m_pItem = m_pItemFactory->CloneItem(rhs.m_pItem);
	}
	return *this;
}

CSimulatedRuntimeParam& CSimulatedRuntimeParam::operator=(CSimulatedRuntimeParam&& rhs)
{
	if (this != &rhs)
	{
		m_name = std::move(rhs.m_name);

		if (m_pItem && m_pItemFactory)
		{
			m_pItemFactory->DestroyItems(m_pItem);
		}

		m_pItemFactory = rhs.m_pItemFactory;
		m_pItem = rhs.m_pItem;

		rhs.m_pItem = nullptr;
	}
	return *this;
}

CSimulatedRuntimeParam::~CSimulatedRuntimeParam()
{
	if (m_pItemFactory && m_pItem)
	{
		m_pItemFactory->DestroyItems(m_pItem);
	}
}

bool CSimulatedRuntimeParam::Serialize(Serialization::IArchive& archive)
{
	//assert(m_pItemFactory);

	// happens when yasli uses a temporary object during its serialization
	if (!m_pItemFactory)
	{
		return false;
	}

	if (!m_pItemFactory->CanBePersistantlySerialized())
	{
		static std::set<string> labels;

		string label;
		label.Format("!^%s", m_name.c_str());
		const string& cachedLabel = *labels.insert(label).first;

		string dummyValue = "[cannot be serialized]";
		archive(dummyValue, "dummy", cachedLabel);
		archive.warning(dummyValue, "Cannot be serialized");

		return true;
	}

	if (archive.isOutput())
	{
		return m_pItemFactory->TrySerializeItem(m_pItem, archive, "value", m_name.c_str());
	}
	else
	{
		assert(archive.isInput());
		return m_pItemFactory->TryDeserializeItem(m_pItem, archive, "value", m_name.c_str());
	}
}

/////////////////////////////////////////////////////////////////////

void CQuerySimulator::CUQSHistoryPostRenderer::OnPostRender() const
{
	if (m_bPeriodicRenderingEnabled)
	{
		if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
		{
			if (const CViewport* pGameViewport = GetIEditor()->GetViewportManager()->GetGameViewport())
			{
				const Matrix34& viewTM = pGameViewport->GetViewTM();
				Matrix33 orientation;
				viewTM.GetRotation33(orientation);
				UQS::Core::SDebugCameraView uqsCameraView;
				uqsCameraView.pos = viewTM.GetTranslation();
				uqsCameraView.dir = orientation * Vec3(0, 1, 0);
				pHub->GetQueryHistoryManager().UpdateDebugRendering3D(&uqsCameraView, UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks::CreateAllBitsSet());
			}
		}
	}
}

void CQuerySimulator::CUQSHistoryPostRenderer::SetEnablePeriodicRendering(bool bEnable)
{
	m_bPeriodicRenderingEnabled = bEnable;
}

CQuerySimulator::CQuerySimulator()
	: m_runState(ERunState::NotRunningAtAll)
	, m_singleStepState(ESingleStepState::NotSingleSteppingAtAll)
	, m_pHistoryPostRenderer(nullptr)
{
	CCentralEventManager::StartOrStopQuerySimulator.Connect(this, &CQuerySimulator::OnStartOrStopQuerySimulator);
	CCentralEventManager::SingleStepQuerySimulatorOnce.Connect(this, &CQuerySimulator::OnSingleStepQuerySimulatorOnce);

	m_queryHost.SetCallback(functor(*this, &CQuerySimulator::OnQueryHostFinished));
	m_queryHost.SetQuerierName("QuerySimulator");

	// instantiate the CUQSHistoryPostRenderer on the heap - it's IPostRenderer is refcounted and deletes itself when un-registering from the game's viewport
	m_pHistoryPostRenderer = new CUQSHistoryPostRenderer;
	GetIEditor()->GetViewportManager()->GetGameViewport()->AddPostRenderer(m_pHistoryPostRenderer);	// this increments its refcount
}

CQuerySimulator::~CQuerySimulator()
{
	// - remove the QueryHistoryPostRenderer
	// - notice: we will *not* leak memory if the viewport-manager and/or the game-viewport are already gone, since they would then have decremented its refcount and eventually delete'd the object already
	if (IViewportManager* pViewportManager = GetIEditor()->GetViewportManager())
	{
		if (CViewport* pGameViewport = pViewportManager->GetGameViewport())
		{
			pGameViewport->RemovePostRenderer(m_pHistoryPostRenderer);	// this decrements its refcount, eventually delete'ing it
		}
	}

	CCentralEventManager::StartOrStopQuerySimulator.DisconnectObject(this);
}

void CQuerySimulator::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		if (m_runState != ERunState::NotRunningAtAll)
		{
			if (m_singleStepState != ESingleStepState::WaitingForNextTrigger)
			{
				if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
				{
					pHub->Update();
				}

				if (m_singleStepState == ESingleStepState::UpdateUQSExactlyOnce)
				{
					m_singleStepState = ESingleStepState::WaitingForNextTrigger;
				}
			}
		}
		break;

	case eNotify_OnBeginGameMode:
	case eNotify_OnBeginSimulationMode:
	case eNotify_OnEndGameMode:
	case eNotify_OnEndSimulationMode:
		// always stop when transitioning in or out of some kind of game mode
		EnsureStopRunning();
		break;
	}
}

void CQuerySimulator::OnStartOrStopQuerySimulator(const CCentralEventManager::SStartOrStopQuerySimulatorEventArgs& args)
{
	if (m_runState == ERunState::NotRunningAtAll)
	{
		// prepare a new query and start it
		m_queryHost.SetQueryBlueprint(args.queryBlueprintName.c_str());
		m_queryHost.GetRuntimeParamsStorage().Clear();
		for (const CSimulatedRuntimeParam& param : args.simulatedParams)
		{
			m_queryHost.GetRuntimeParamsStorage().AddOrReplace(param.GetName(), param.GetItemFactory(), param.GetItem());
		}

		if (StartQuery())
		{
			m_pHistoryPostRenderer->SetEnablePeriodicRendering(true);
			m_runState = args.bRunInfinitely ? ERunState::RunInfinitely : ERunState::RunOnlyOnce;
			CCentralEventManager::QuerySimulatorRunningStateChanged(CCentralEventManager::SQuerySimulatorRunningStateChangedEventArgs(true));
		}
	}
	else
	{
		EnsureStopRunning();
	}
}

void CQuerySimulator::OnSingleStepQuerySimulatorOnce(const CCentralEventManager::SSingleStepQuerySimulatorOnceEventArgs& args)
{
	if (m_runState == ERunState::NotRunningAtAll)
	{
		// prepare a new query and start it
		m_queryHost.SetQueryBlueprint(args.queryBlueprintName.c_str());
		m_queryHost.GetRuntimeParamsStorage().Clear();
		for (const CSimulatedRuntimeParam& param : args.simulatedParams)
		{
			m_queryHost.GetRuntimeParamsStorage().AddOrReplace(param.GetName(), param.GetItemFactory(), param.GetItem());
		}

		if (StartQuery())
		{
			m_pHistoryPostRenderer->SetEnablePeriodicRendering(true);
			m_runState = args.bRunInfinitely ? ERunState::RunInfinitely : ERunState::RunOnlyOnce;
			CCentralEventManager::QuerySimulatorRunningStateChanged(CCentralEventManager::SQuerySimulatorRunningStateChangedEventArgs(true));
		}
	}

	if (m_runState != ERunState::NotRunningAtAll)
	{
		m_singleStepState = ESingleStepState::UpdateUQSExactlyOnce;
	}
}

bool CQuerySimulator::StartQuery()
{
	const UQS::Core::CQueryID queryID = m_queryHost.StartQuery();

	if (queryID.IsValid())
	{
		if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
		{
			pHub->GetQueryHistoryManager().MakeHistoricQueryCurrentForInWorldRendering(UQS::Core::IQueryHistoryManager::Live, queryID);
		}
	}

	return queryID.IsValid();
}

void CQuerySimulator::EnsureStopRunning()
{
	if (m_runState == ERunState::NotRunningAtAll)
		return;

	m_queryHost.CancelQuery();
	m_pHistoryPostRenderer->SetEnablePeriodicRendering(false);
	m_runState = ERunState::NotRunningAtAll;
	m_singleStepState = ESingleStepState::NotSingleSteppingAtAll;
	CCentralEventManager::QuerySimulatorRunningStateChanged(CCentralEventManager::SQuerySimulatorRunningStateChangedEventArgs(false));
}

void CQuerySimulator::OnQueryHostFinished(void* pUserData)
{
	bool bKeepOnRunning = false;

	if (m_queryHost.HasSucceeded())
	{
		if (m_runState == ERunState::RunInfinitely)
		{
			StartQuery();
			bKeepOnRunning = true;
		}
	}
	else
	{
		QMessageBox::warning(nullptr, QtUtil::ToQString("Query exception"), QtUtil::ToQString(m_queryHost.GetExceptionMessage()));
	}

	if (!bKeepOnRunning)
	{
		EnsureStopRunning();
	}
}
