// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:SmartGlassManager.h
//
//	History:
//	-Jan 16,2013:Originally Created by Steve Barnett
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(SUPPORT_SMARTGLASS)

	#include "SmartGlassManager.h"
	#include "../Common/SmartGlassContext.h"

using Windows::Foundation::TypedEventHandler;

// WinRT wrapper class required to receive callbacks from SmartGlass
ref class CSmartGlassManagerListenerWrapper sealed
{
public:
	void Shutdown()
	{
		m_mgr = NULL;
	}

	void OnDeviceAdded(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device)
	{
		if (m_mgr)
		{
			m_mgr->OnDeviceAdded(sender, device);
		}
	}

	void OnDeviceRemoved(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device)
	{
		if (m_mgr)
		{
			m_mgr->OnDeviceRemoved(sender, device);
		}
	}

	void OnQosChanged(SmartGlassDevice^ device, QosMetricsChangedEventArgs^ args)
	{
		if (m_mgr)
		{
			m_mgr->OnQosChanged(device, args);
		}
	}

	// Cannot have a standard type in the interface of a WinRT type, so constructor must be private and CSmartGlassManager declared as a friend
	friend class CSmartGlassManager;

private:
	CSmartGlassManagerListenerWrapper(class CSmartGlassManager* mgr)
		: m_mgr(mgr)
	{
	}

	CSmartGlassManager* m_mgr;
};

int CSmartGlassManager::s_nextContextId = 0;

CSmartGlassManager::CSmartGlassManager()
	: m_pWatcher(nullptr)
	, m_pListenerWrapper(nullptr)
	, m_pfnInputListenerFactory(NULL)
{
	m_pWatcher = ref new SmartGlassDeviceWatcher();
	m_pListenerWrapper = ref new CSmartGlassManagerListenerWrapper(this);

	m_deviceAddedCookie = m_pWatcher->DeviceAdded += ref new TypedEventHandler<SmartGlassDeviceWatcher^, SmartGlassDevice^>(m_pListenerWrapper, &CSmartGlassManagerListenerWrapper::OnDeviceAdded);
	m_deviceRemovedCookie = m_pWatcher->DeviceRemoved += ref new TypedEventHandler<SmartGlassDeviceWatcher^, SmartGlassDevice^>(m_pListenerWrapper, &CSmartGlassManagerListenerWrapper::OnDeviceRemoved);
}

CSmartGlassManager::~CSmartGlassManager()
{
	m_pListenerWrapper->Shutdown();
	m_pListenerWrapper = nullptr;

	m_pWatcher->DeviceAdded -= m_deviceAddedCookie;
	m_pWatcher->DeviceRemoved -= m_deviceRemovedCookie;
	m_pWatcher = nullptr;

	Lock();
	SmartGlassPlayerVec::iterator end = m_playerInfo.end();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != end; ++it)
	{
		SAFE_RELEASE(it->ctx);
	}
	m_playerInfo.clear();
	Unlock();
}

void CSmartGlassManager::Update()
{
	Lock();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != m_playerInfo.end(); )
	{
		if (it->ctx->IsDisconnected())
		{
			SAFE_RELEASE(it->ctx);
			it = m_playerInfo.erase(it);
		}
		else
		{
			if (it->listenerAllocationRequired && m_pfnInputListenerFactory)
			{
				it->ctx->SetInputListener(m_pfnInputListenerFactory(it->userId, it->ctx->GetWidth(), it->ctx->GetHeight()));
				it->listenerAllocationRequired = false;
			}
			gEnv->pRenderer->UpdateSmartGlassDevice(it->ctx);
			it->ctx->SendInputEvents();
			++it;
		}
	}
	Unlock();
}

void CSmartGlassManager::SetFlashPlayer(string userId, struct IFlashPlayer* pFlashPlayer)
{
	Lock();
	SmartGlassPlayerVec::iterator end = m_playerInfo.end();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != end; ++it)
	{
		if (userId == it->userId)
		{
			it->ctx->SetFlashPlayer(pFlashPlayer);
		}
	}
	Unlock();
}

void CSmartGlassManager::SetInputListenerFactory(CSmartGlassManager::SmartGlassInputListenerFactory* pfnFactory)
{
	Lock();
	// Store factory for use with new contexts
	m_pfnInputListenerFactory = pfnFactory;
	// Update existing contexts
	SmartGlassPlayerVec::iterator end = m_playerInfo.end();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != end; ++it)
	{
		if (pfnFactory)
		{
			it->ctx->SetInputListener(m_pfnInputListenerFactory(it->userId, it->ctx->GetWidth(), it->ctx->GetHeight()));
		}
		else
		{
			it->ctx->SetInputListener(NULL);
		}
	}
	Unlock();
}

void CSmartGlassManager::OnDeviceAdded(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device)
{
	UNREFERENCED_PARAMETER(sender);

	Windows::Foundation::EventRegistrationToken qosChangedCookie = device->QosMetricsChanged += ref new TypedEventHandler<SmartGlassDevice^, QosMetricsChangedEventArgs^>(m_pListenerWrapper, &CSmartGlassManagerListenerWrapper::OnQosChanged);

	Lock();
	if (m_playerInfo.size() < MAX_PLAYERS)
	{
		char buffer[65];
		cry_sprintf(buffer, "<unknown user %d>", s_nextContextId++);
		string userId(buffer);
		if (device->User != nullptr)
		{
			const wchar_t* pwszUserId = device->User->LiveIdentity->Data();
			size_t length = wcslen(pwszUserId);
			char* pszUserId = new char[length + 1];
			char defChar = '?';
			WideCharToMultiByte(CP_ACP, 0, pwszUserId, -1, pszUserId, length, &defChar, NULL);
			pszUserId[length] = '\0';
			userId = pszUserId;
			delete[] pszUserId;
			CryLog("<SmartGlass> Companion %d connected as user '%s'.", m_playerInfo.size(), userId.c_str());
		}
		else
		{
			CryLog("<SmartGlass> Companion %d connected without a user (generated id %s).", m_playerInfo.size(), userId.c_str());
		}

		ISmartGlassContext* ctx = new CSmartGlassContext();
		CSmartGlassPlayerData data;
		ctx->AddRef();
		data.deviceId = device->DirectSurface->Id;
		data.userId = userId;
		data.ctx = ctx;
		data.listenerAllocationRequired = true;
		data.qosChangedCookie = qosChangedCookie;
		ctx->SetDevice(device);
		gEnv->pRenderer->InitializeSmartGlassContext(ctx);
		m_playerInfo.push_back(data);
	}
	else
	{
		CryLog("<SmartGlass> Too many players!");
	}
	Unlock();
}

void CSmartGlassManager::OnDeviceRemoved(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device)
{
	UNREFERENCED_PARAMETER(sender);

	Lock();
	SmartGlassDirectSurface^ pDirectSurface = device->DirectSurface;
	SmartGlassPlayerVec::iterator end = m_playerInfo.end();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != end; ++it)
	{
		if (it->deviceId == pDirectSurface->Id)
		{
			CryLog("<SmartGlass> Companion %d disconnected.", m_playerInfo.size());
			it->ctx->MarkDisconnected();
			device->QosMetricsChanged -= it->qosChangedCookie;
			break;
		}
	}
	Unlock();
}

void CSmartGlassManager::OnQosChanged(SmartGlassDevice^ device, QosMetricsChangedEventArgs^ args)
{
	UNREFERENCED_PARAMETER(device);

	Lock();
	SmartGlassDirectSurface^ pDirectSurface = device->DirectSurface;
	SmartGlassPlayerVec::iterator end = m_playerInfo.end();
	for (SmartGlassPlayerVec::iterator it = m_playerInfo.begin(); it != end; ++it)
	{
		if (it->deviceId == pDirectSurface->Id)
		{
			// FIXME args->PoorNetwork not in March XDK
			//CryLog("<SmartGlass> Companion QoS changed %s.", args->PoorNetwork ? "(poor network conditions)" : "");
		}
	}
	Unlock();
}

#endif
