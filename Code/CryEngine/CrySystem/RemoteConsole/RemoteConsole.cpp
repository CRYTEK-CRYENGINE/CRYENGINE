// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   RemoteConsole.cpp
//  Version:     v1.00
//  Created:     12/6/2012 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "RemoteConsole.h"

#ifdef USE_REMOTE_CONSOLE
#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <../CryAction/ILevelSystem.h>
#if 0                         // currently no stroboscope support
#include "Stroboscope/Stroboscope.h"
#include "ThreadInfo.h"
#endif

#define DEFAULT_PORT 4600
#define DEFAULT_BUFFER 4096
#define MAX_BIND_ATTEMPTS 8   // it will try to connect using ports from DEFAULT_PORT to DEFAULT_PORT + MAX_BIND_ATTEMPTS - 1
#define SERVER_THREAD_NAME "RemoteConsoleServer"
#define CLIENT_THREAD_NAME "RemoteConsoleClient"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
CRemoteConsole::CRemoteConsole()
	: m_listener(1)
	, m_running(false)
#ifdef USE_REMOTE_CONSOLE
	, m_pServer(new SRemoteServer())
	, m_pLogEnableRemoteConsole(NULL)
#endif
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
CRemoteConsole::~CRemoteConsole()
{
#ifdef USE_REMOTE_CONSOLE
	Stop();
	delete m_pServer;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::RegisterConsoleVariables()
{
#ifdef USE_REMOTE_CONSOLE
	m_pLogEnableRemoteConsole = REGISTER_INT("log_EnableRemoteConsole", 1, VF_DUMPTODISK, "enables/disables the remote console");
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::UnregisterConsoleVariables()
{
#ifdef USE_REMOTE_CONSOLE
	m_pLogEnableRemoteConsole = NULL;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::Start()
{
#ifdef USE_REMOTE_CONSOLE
	m_pServer->StartServer();
	m_running = true;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::Stop()
{
#ifdef USE_REMOTE_CONSOLE
	m_running = false;
	m_pServer->StopServer();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool CRemoteConsole::IsStarted() const
{
	return m_running;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::AddLogMessage(const char* log)
{
#ifdef USE_REMOTE_CONSOLE
	if (!IsStarted()) return;

	IRemoteEvent* pEvent = new SStringEvent<eCET_LogMessage>(log);
	m_pServer->AddEvent(pEvent);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::AddLogWarning(const char* log)
{
#ifdef USE_REMOTE_CONSOLE
	if (!IsStarted()) return;

	IRemoteEvent* pEvent = new SStringEvent<eCET_LogWarning>(log);
	m_pServer->AddEvent(pEvent);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::AddLogError(const char* log)
{
#ifdef USE_REMOTE_CONSOLE
	if (!IsStarted()) return;

	IRemoteEvent* pEvent = new SStringEvent<eCET_LogError>(log);
	m_pServer->AddEvent(pEvent);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::Update()
{
#ifdef USE_REMOTE_CONSOLE
	if (m_pLogEnableRemoteConsole)
	{
		if (m_pLogEnableRemoteConsole->GetIVal() != 0 && !IsStarted())
			Start();
		else if (m_pLogEnableRemoteConsole->GetIVal() == 0 && IsStarted())
			Stop();
	}

	TEventBuffer events;
	m_pServer->GetEvents(events);
	for (TEventBuffer::iterator it = events.begin(), end = events.end(); it != end; ++it)
	{
		IRemoteEvent* pEvent = *it;
		switch (pEvent->GetType())
		{
		case eCET_ConsoleCommand:
			for (TListener::Notifier notifier(m_listener); notifier.IsValid(); notifier.Next())
				notifier->OnConsoleCommand(((SStringEvent<eCET_ConsoleCommand>*)pEvent)->GetData());
			break;
		case eCET_GameplayEvent:
			for (TListener::Notifier notifier(m_listener); notifier.IsValid(); notifier.Next())
				notifier->OnGameplayCommand(((SStringEvent<eCET_GameplayEvent>*)pEvent)->GetData());
			break;
#if 0                // currently no stroboscope support
		case eCET_Strobo_GetThreads:
			SendThreadData();
			break;
		case eCET_Strobo_GetResult:
			SendStroboscopeResult();
			break;
		default:
			assert(false); // NOT SUPPORTED FOR THE SERVER!!!
#endif
		}
		delete *it;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::RegisterListener(IRemoteConsoleListener* pListener, const char* name)
{
#ifdef USE_REMOTE_CONSOLE
	m_listener.Add(pListener, name);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CRemoteConsole::UnregisterListener(IRemoteConsoleListener* pListener)
{
#ifdef USE_REMOTE_CONSOLE
	m_listener.Remove(pListener);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
#if 0 // currently no stroboscope support
void CRemoteConsole::SendThreadData()
{
#if defined(USE_REMOTE_CONSOLE) && defined(ENABLE_PROFILING_CODE)
	SThreadInfo::TThreadInfo info;
	SThreadInfo::GetCurrentThreads(info);
	for (SThreadInfo::TThreadInfo::iterator it = info.begin(), end = info.end(); it != end; ++it)
	{
		char tmp[256];
		cry_sprintf(tmp, "%u:%s", it->first, it->second.c_str());
		m_pServer->AddEvent(new SStringEvent<eCET_Strobo_ThreadAdd>(tmp));
	}
	m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_ThreadDone>());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////
#if defined(USE_REMOTE_CONSOLE) && defined(ENABLE_PROFILING_CODE)
template <EConsoleEventType T>
void SendStroboscopeEvent(SRemoteServer* pServer, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[2048];
	cry_vsprintf(tmp, format, args);
	va_end(args);
	pServer->AddEvent(new SStringEvent<T>(tmp));
}
#endif

void CRemoteConsole::SendStroboscopeResult()
{
#if defined(USE_REMOTE_CONSOLE) && defined(ENABLE_PROFILING_CODE)
	SStrobosopeResult res = CStroboscope::GetInst()->GetResult();
	if (res.Valid)
	{
		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_ResultStart>());

		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_StatStart>());
		SendStroboscopeEvent<eCET_Strobo_StatAdd>(m_pServer, "Filename: %s", res.File.c_str());
		SendStroboscopeEvent<eCET_Strobo_StatAdd>(m_pServer, "Duration: %.6f", res.Duration);
		SendStroboscopeEvent<eCET_Strobo_StatAdd>(m_pServer, "Date: %s", res.Date.c_str());
		SendStroboscopeEvent<eCET_Strobo_StatAdd>(m_pServer, "Samples: %i", res.Samples);

		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_ThreadInfoStart>());
		for (SStrobosopeResult::TThreadInfo::const_iterator it = res.ThreadInfo.begin(), end = res.ThreadInfo.end(); it != end; ++it)
			SendStroboscopeEvent<eCET_Strobo_ThreadInfoAdd>(m_pServer, "%u %.6f %i %s", it->Id, it->Counts, it->Samples, it->Name.c_str());

		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_SymStart>());
		for (SStrobosopeResult::TSymbolInfo::const_iterator it = res.SymbolInfo.begin(), end = res.SymbolInfo.end(); it != end; ++it)
			SendStroboscopeEvent<eCET_Strobo_SymAdd>(m_pServer, "sym%i \"%s\" \"%s\" \"%s\" %i 0x%016llX", it->first, it->second.Module.c_str(), it->second.Procname.c_str(), it->second.File.c_str(), it->second.Line, it->second.BaseAddr);

		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_CallstackStart>());
		char tmp[256];
		for (SStrobosopeResult::TCallstackInfo::const_iterator it = res.CallstackInfo.begin(), end = res.CallstackInfo.end(); it != end; ++it)
		{
			string out;
			cry_sprintf(tmp, "%u %i %.6f", it->ThreadId, it->FrameId, it->Spend);
			out = tmp;
			for (SStrobosopeResult::SCallstackInfo::TSymbols::const_iterator sit = it->Symbols.begin(), send = it->Symbols.end(); sit != send; ++sit)
			{
				cry_sprintf(tmp, " sym%i", *sit);
				out += tmp;
			}
			m_pServer->AddEvent(new SStringEvent<eCET_Strobo_CallstackAdd>(out.c_str()));
		}

		m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_FrameInfoStart>());
		SendStroboscopeEvent<eCET_Strobo_FrameInfoAdd>(m_pServer, "%i %i", res.StartFrame, res.EndFrame);
		for (SStrobosopeResult::TFrameTime::const_iterator it = res.FrameTime.begin(), end = res.FrameTime.end(); it != end; ++it)
			SendStroboscopeEvent<eCET_Strobo_FrameInfoAdd>(m_pServer, "%i %.6f", it->first, it->second);
		}
	m_pServer->AddEvent(new SNoDataEvent<eCET_Strobo_ResultDone>());
#endif
}
#endif //#if 0 // currently no stroboscope support

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_REMOTE_CONSOLE

void SRemoteServer::StartServer()
{
	StopServer();
	m_bAcceptClients = true;

	if (!gEnv->pThreadManager->SpawnThread(this, SERVER_THREAD_NAME))
	{
		CryFatalError("Error spawning \"%s\" thread.", SERVER_THREAD_NAME);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::StopServer()
{
	SignalStopWork();
	if (m_socket != CRY_INVALID_SOCKET && m_socket != CRY_SOCKET_ERROR)
	{
		CrySock::closesocket(m_socket);
	}
	m_socket = CRY_SOCKET_ERROR;
	m_lock.Lock();
	for (TClients::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
		it->pClient->StopClient();
	m_lock.Unlock();
	m_stopEvent.Wait();
	m_stopEvent.Set();
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::ClientDone(SRemoteClient* pClient)
{
	m_lock.Lock();
	for (TClients::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
	{
		if (it->pClient == pClient)
		{
			it->pClient->SignalStopWork();
			gEnv->pThreadManager->JoinThread(it->pClient, eJM_Join);
			delete it->pClient;
			delete it->pEvents;
			m_clients.erase(it);
			break;
		}
	}

	if (m_clients.empty())
		m_stopEvent.Set();
	m_lock.Unlock();
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::SignalStopWork()
{
	m_bAcceptClients = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::ThreadEntry()
{
	CRYSOCKET sClient;
	CRYSOCKLEN_T iAddrSize;
	CRYSOCKADDR_IN local, client;

#ifdef CRY_PLATFORM_WINAPI
	WSADATA wsd;
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		gEnv->pLog->LogError("[RemoteKeyboard] Failed to load Winsock!\n");
		return;
	}
#endif

	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (m_socket == CRY_SOCKET_ERROR)
	{
		CryLog("Remote console FAILED. socket() => CRY_SOCKET_ERROR");
		return;
	}

	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_family      = AF_INET;

	bool bindOk = false;
	for (uint32 i = 0; i < MAX_BIND_ATTEMPTS; ++i)
	{
		local.sin_port = htons(DEFAULT_PORT + i);
		const int result = CrySock::bind(m_socket, (CRYSOCKADDR*)&local, sizeof(local));
		if (CrySock::TranslateSocketError(result) == CrySock::eCSE_NO_ERROR)
		{
			bindOk = true;
			break;
		}
	}

	if (bindOk == false)
	{
		CryLog("Remote console FAILED. bind() => CRY_SOCKET_ERROR. Faild ports from %d to %d", DEFAULT_PORT, DEFAULT_PORT + MAX_BIND_ATTEMPTS - 1);
		return;
	}

	CrySock::listen(m_socket, 8);

	CRYSOCKADDR_IN saIn;
	CRYSOCKLEN_T slen = sizeof(saIn);
	if (CrySock::getsockname(m_socket, (CRYSOCKADDR*)&saIn, &slen) == 0)
	{
		CryLog("Remote console listening on: %d\n", ntohs(saIn.sin_port));
	}
	else
	{
		CryLog("Remote console FAILED to listen on: %d\n", ntohs(saIn.sin_port));
	}

	while (m_bAcceptClients)
	{
		iAddrSize = sizeof(client);
		sClient   = CrySock::accept(m_socket, (CRYSOCKADDR*)&client, &iAddrSize);
		if (!m_bAcceptClients || sClient == CRY_INVALID_SOCKET)
		{
			break;
		}

		m_lock.Lock();
		m_stopEvent.Reset();
		SRemoteClient* pClient = new SRemoteClient(this);
		m_clients.push_back(SRemoteClientInfo(pClient));
		pClient->StartClient(sClient);
		m_lock.Unlock();
	}
	if (m_socket != CRY_INVALID_SOCKET && m_socket != CRY_SOCKET_ERROR)
	{
		CrySock::closesocket(m_socket);
	}
	//WSACleanup();
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::AddEvent(IRemoteEvent* pEvent)
{
	m_lock.Lock();
	for (TClients::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
	{
		it->pEvents->push_back(pEvent->Clone());
	}
	m_lock.Unlock();
	delete pEvent;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteServer::GetEvents(TEventBuffer& buffer)
{
	m_lock.Lock();
	buffer = m_eventBuffer;
	m_eventBuffer.clear();
	m_lock.Unlock();
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool SRemoteServer::WriteBuffer(SRemoteClient* pClient, char* buffer, int& size)
{
	m_lock.Lock();
	IRemoteEvent* pEvent = NULL;
	for (TClients::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
	{
		if (it->pClient == pClient)
		{
			TEventBuffer* pEvents = it->pEvents;
			if (!pEvents->empty())
			{
				pEvent = pEvents->front();
				pEvents->pop_front();
			}
			break;
		}
	}
	m_lock.Unlock();
	const bool res = pEvent != NULL;
	if (pEvent)
	{
		SRemoteEventFactory::GetInst()->WriteToBuffer(pEvent, buffer, size);
		delete pEvent;
	}
	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool SRemoteServer::ReadBuffer(const char* buffer, int data)
{
	IRemoteEvent* pEvent = SRemoteEventFactory::GetInst()->CreateEventFromBuffer(buffer, data);
	const bool res       = pEvent != NULL;
	if (pEvent)
	{
		if (pEvent->GetType() != eCET_Noop)
		{
			m_lock.Lock();
			m_eventBuffer.push_back(pEvent);
			m_lock.Unlock();
		}
		else
		{
			delete pEvent;
		}
	}

	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteClient::StartClient(CRYSOCKET socket)
{
	m_socket = socket;
	if (!gEnv->pThreadManager->SpawnThread(this, CLIENT_THREAD_NAME))
	{
		CryFatalError("Error spawning \"%s\" thread.", CLIENT_THREAD_NAME);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteClient::StopClient()
{
	SignalStopWork();
	CrySock::closesocket(m_socket);
	m_socket = CRY_SOCKET_ERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteClient::SignalStopWork()
{
	m_pServer->ClientDone(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteClient::ThreadEntry()
{
	char szBuff[DEFAULT_BUFFER];
	int  size;
	SNoDataEvent<eCET_Req> reqEvt;

	std::vector<string> autoCompleteList;
	FillAutoCompleteList(autoCompleteList);

	bool ok = true;
	bool autoCompleteDoneSent = false;
	while (ok)
	{
		// read data
		SRemoteEventFactory::GetInst()->WriteToBuffer(&reqEvt, szBuff, size);
		ok &= SendPackage(szBuff, size);
		ok &= RecvPackage(szBuff, size);
		ok &= m_pServer->ReadBuffer(szBuff, size);

		for (int i = 0; i < 20 && !autoCompleteList.empty(); ++i)
		{
			SStringEvent<eCET_AutoCompleteList> autoCompleteListEvt(autoCompleteList.back().c_str());
			SRemoteEventFactory::GetInst()->WriteToBuffer(&autoCompleteListEvt, szBuff, size);
			ok &= SendPackage(szBuff, size);
			ok &= RecvPackage(szBuff, size);
			ok &= m_pServer->ReadBuffer(szBuff, size);
			autoCompleteList.pop_back();
		}
		if (autoCompleteList.empty() && !autoCompleteDoneSent)
		{
			SNoDataEvent<eCET_AutoCompleteListDone> autoCompleteDone;
			SRemoteEventFactory::GetInst()->WriteToBuffer(&autoCompleteDone, szBuff, size);

			ok &= SendPackage(szBuff, size);
			ok &= RecvPackage(szBuff, size);
			ok &= m_pServer->ReadBuffer(szBuff, size);
			autoCompleteDoneSent = true;
		}

		// send data
		while (ok && m_pServer->WriteBuffer(this, szBuff, size))
		{
			ok &= SendPackage(szBuff, size);
			ok &= RecvPackage(szBuff, size);
			IRemoteEvent* pEvt = SRemoteEventFactory::GetInst()->CreateEventFromBuffer(szBuff, size);
			ok &= pEvt && pEvt->GetType() == eCET_Noop;
			delete pEvt;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool SRemoteClient::RecvPackage(char* buffer, int& size)
{
	size = 0;
	int ret, idx = 0;
	do
	{
		ret = CrySock::recv(m_socket, buffer + idx, DEFAULT_BUFFER - idx, 0);
		if (ret <= 0 || ret == CRY_SOCKET_ERROR)
			return false;
		idx += ret;
	}
	while (buffer[idx - 1] != '\0');
	size = idx;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool SRemoteClient::SendPackage(const char* buffer, int size)
{
	int ret, idx = 0;
	int left = size + 1;
	assert(buffer[size] == '\0');
	while (left > 0)
	{
		ret = CrySock::send(m_socket, &buffer[idx], left, 0);
		if (ret <= 0 || ret == CRY_SOCKET_ERROR)
			return false;
		left -= ret;
		idx  += ret;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteClient::FillAutoCompleteList(std::vector<string>& list)
{
	std::vector<const char*> cmds;
	size_t count = gEnv->pConsole->GetSortedVars(NULL, 0);
	cmds.resize(count);
	count = gEnv->pConsole->GetSortedVars(&cmds[0], count);
	for (size_t i = 0; i < count; ++i)
	{
		list.push_back(cmds[i]);
	}
	for (int i = 0, end = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetLevelCount(); i < end; ++i)
	{
		ILevelInfo* pLevel    = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetLevelInfo(i);
		string item           = "map ";
		const char* levelName = pLevel->GetName();
		int start             = 0;
		for (int k = 0, kend = strlen(levelName); k < kend; ++k)
		{
			if ((levelName[k] == '\\' || levelName[k] == '/') && k + 1 < kend)
				start = k + 1;
		}
		item += levelName + start;
		list.push_back(item);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Event factory ///////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
#define REGISTER_EVENT_NODATA(evt) RegisterEvent(new SNoDataEvent<evt>());
#define REGISTER_EVENT_STRING(evt) RegisterEvent(new SStringEvent<evt>(""));

/////////////////////////////////////////////////////////////////////////////////////////////
SRemoteEventFactory::SRemoteEventFactory()
{
	REGISTER_EVENT_NODATA(eCET_Noop);
	REGISTER_EVENT_NODATA(eCET_Req);
	REGISTER_EVENT_STRING(eCET_LogMessage);
	REGISTER_EVENT_STRING(eCET_LogWarning);
	REGISTER_EVENT_STRING(eCET_LogError);
	REGISTER_EVENT_STRING(eCET_ConsoleCommand);
	REGISTER_EVENT_STRING(eCET_AutoCompleteList);
	REGISTER_EVENT_NODATA(eCET_AutoCompleteListDone);

	REGISTER_EVENT_NODATA(eCET_Strobo_GetThreads);
	REGISTER_EVENT_STRING(eCET_Strobo_ThreadAdd);
	REGISTER_EVENT_NODATA(eCET_Strobo_ThreadDone);
	REGISTER_EVENT_NODATA(eCET_Strobo_GetResult);
	REGISTER_EVENT_NODATA(eCET_Strobo_ResultStart);
	REGISTER_EVENT_NODATA(eCET_Strobo_ResultDone);

	REGISTER_EVENT_NODATA(eCET_Strobo_StatStart);
	REGISTER_EVENT_STRING(eCET_Strobo_StatAdd);
	REGISTER_EVENT_NODATA(eCET_Strobo_ThreadInfoStart);
	REGISTER_EVENT_STRING(eCET_Strobo_ThreadInfoAdd);
	REGISTER_EVENT_NODATA(eCET_Strobo_SymStart);
	REGISTER_EVENT_STRING(eCET_Strobo_SymAdd);
	REGISTER_EVENT_NODATA(eCET_Strobo_CallstackStart);
	REGISTER_EVENT_STRING(eCET_Strobo_CallstackAdd);

	REGISTER_EVENT_STRING(eCET_GameplayEvent);

	REGISTER_EVENT_NODATA(eCET_Strobo_FrameInfoStart);
	REGISTER_EVENT_STRING(eCET_Strobo_FrameInfoAdd);
}

/////////////////////////////////////////////////////////////////////////////////////////////
SRemoteEventFactory::~SRemoteEventFactory()
{
	for (TPrototypes::iterator it = m_prototypes.begin(), end = m_prototypes.end(); it != end; ++it)
	{
		delete it->second;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
IRemoteEvent* SRemoteEventFactory::CreateEventFromBuffer(const char* buffer, int size)
{
	if (size > 1 && buffer[size - 1] == '\0')
	{
		EConsoleEventType type         = EConsoleEventType(buffer[0] - '0');
		TPrototypes::const_iterator it = m_prototypes.find(type);
		if (it != m_prototypes.end())
		{
			IRemoteEvent* pEvent = it->second->CreateFromBuffer(buffer + 1, size - 1);
			return pEvent;
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteEventFactory::WriteToBuffer(IRemoteEvent* pEvent, char* buffer, int& size)
{
	assert(m_prototypes.find(pEvent->GetType()) != m_prototypes.end());
	buffer[0] = '0' + (char)pEvent->GetType();
	pEvent->WriteToBuffer(buffer + 1, size, DEFAULT_BUFFER - 2);
	buffer[++size] = '\0';
}

/////////////////////////////////////////////////////////////////////////////////////////////
void SRemoteEventFactory::RegisterEvent(IRemoteEvent* pEvent)
{
	assert(m_prototypes.find(pEvent->GetType()) == m_prototypes.end());
	m_prototypes[pEvent->GetType()] = pEvent;
}
/////////////////////////////////////////////////////////////////////////////////////////////

#endif //#ifdef USE_REMOTE_CONSOLE
