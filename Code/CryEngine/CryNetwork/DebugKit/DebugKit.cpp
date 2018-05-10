// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugKit.h"
#include "Network.h"

#if ENABLE_DEBUG_KIT

#include <CryRenderer/IRenderAuxGeom.h>

CryCriticalSection CDebugKit::m_lockThis;
CDebugKit* volatile CDebugKit::m_pThis = 0;

CDebugKit::CDebugKit() : CEncoding(GetDebugOutput())
{
	m_state = eS_Starting;
	m_running = false;
}

CDebugKit::~CDebugKit()
{
}

void CDebugKit::Update()
{
	float per = (1 + sinf(g_time.GetSeconds() * 2 * gf_PI / 10)) / 2;
	float alpha = expf(-15 * (per - 0.3f) * (per - 0.3f));
	float clr[] = { 0.8f, 0.8f, 0.8f, alpha };

	gEnv->pAuxGeomRenderer->Draw2dLabel(10, 10, 1, clr, false, "NET DEBUG KIT ENABLED **BUILD FOR CRYTEK INTERNAL USE ONLY**");

	const char* state = "<invalid state>";
	switch (m_state)
	{
	case eS_Starting:
		state = "Starting...";
		break;
	case eS_Searching:
		state = "Searching for debug server";
		break;
	case eS_Complete:
		state = "Completed (shutdown)";
		break;
	case eS_Running:
		state = "Communicating with debug server";
		break;
	}

	gEnv->pAuxGeomRenderer->Draw2dLabel(10, 20, 1, clr, false, "State: %s", state);
}

void CDebugKit::ThreadEntry()
{
	m_running = true;

	while (m_running && m_state != eS_Complete)
	{
		switch (m_state)
		{
		case eS_Starting:
			CrySleep(500);
			m_state = eS_Searching;
			break;
		case eS_Searching:
			if (!InitServer())
				m_state = eS_Starting;
			else
				m_state = eS_Running;
			break;
		case eS_Running:
			RunServer();
			m_state = eS_Starting;
			break;
		}
	}

	m_state = eS_Complete;
}

void CDebugKit::SignalStopWork()
{
	m_running = false;
}

bool CDebugKit::InitServer()
{
	CRYSOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == CRY_INVALID_SOCKET)
		return false;

	bool ok = false;

	BOOL opt = 1;
	CrySock::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));

	char buf = 's';
	CRYSOCKADDR_IN addr;
	memset(&addr, 0, sizeof(addr));
	CRYSOCKLEN_T addrlen = sizeof(addr);
	CRYSOCKET svrSock = CRY_INVALID_SOCKET;
	int n = 0;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8000);
	*(uint32*)& addr.sin_addr = 0xffffffffu;
	if (1 != sendto(sock, &buf, 1, 0, (CRYSOCKADDR*)&addr, sizeof(addr)))
		goto done;

	static const int BUFFERSIZE = 1024;
	char buffer[BUFFERSIZE];

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	// not portable to *nix
	n = select(CRYSOCKET(0), &fds, NULL, NULL, &timeout);
	switch (n)
	{
	case 0:
	case CRY_SOCKET_ERROR:
		goto done;
	}
	int r = CrySock::recvfrom(sock, buffer, BUFFERSIZE, 0, (CRYSOCKADDR*)&addr, &addrlen);
	if (addrlen != sizeof(addr))
		goto done;
	if (r % 4 || !r)
		goto done;

	uint32* pSvcs = (uint32*)buffer;

	{
		CryAutoCriticalSection lk(m_lockThis);

		m_servicesReq.resize(0);
		m_servicesReq.insert(m_servicesReq.end(), pSvcs, pSvcs + r / 4);
		m_server = addr;

		if (m_servicesReq[0] != 'ok')
			goto done;

		std::sort(m_servicesReq.begin(), m_servicesReq.end());

		CEncoding::SetEnabled(std::binary_search(m_servicesReq.begin(), m_servicesReq.end(), 'strm'));
	#if STATS_COLLECTOR_DEBUG_KIT
		STATS.SetDebugKit(std::binary_search(m_servicesReq.begin(), m_servicesReq.end(), 'stat'));
	#endif
	}

	ok = true;

done:
	CrySock::closesocket(sock);
	return ok;
}

void CDebugKit::AddDataEnt(const Vec3& data)
{
	m_curDataEnt.data = data;
	if (m_curDataEnt.obj)
	{
		CryAutoCriticalSection lk(m_lockThis);
		if (std::binary_search(m_servicesReq.begin(), m_servicesReq.end(), m_curDataEnt.key))
		{
			Write('d');
			Write(m_curDataEnt.obj);
			Write(m_curDataEnt.value);
			Write(m_curDataEnt.key);
			Write(m_curDataEnt.data.x);
			Write(m_curDataEnt.data.y);
			Write(m_curDataEnt.data.z);
			Write((uint32)0xdeadbeaf);
		}
	}
}

void CDebugKit::LogSnapping(const Vec3& witnessPos, const Vec3& witnessDir, const Vec3& entityPos0, const Vec3& entityPos1, const string& entityCls)
{
	CryAutoCriticalSection lk(m_lockThis);
	if (std::binary_search(m_servicesReq.begin(), m_servicesReq.end(), 'snap'))
	{
		Write('p');
		Write(witnessPos);
		Write(witnessDir);
		Write(entityPos0);
		Write(entityPos1);
		Write(entityCls);
	}
}

void CDebugKit::SCBegin(uint32 tag)
{
	CryAutoCriticalSection lk(m_lockThis);
	Write('s');
	Write(tag);
}

void CDebugKit::SCPut(const char* str)
{
	CryAutoCriticalSection lk(m_lockThis);
	Write(string(str));
}

void CDebugKit::SCPut(float f)
{
	CryAutoCriticalSection lk(m_lockThis);
	Write(f);
}

void CDebugKit::SetSessionID(const CSessionID& session)
{
	CEncoding::SetSessionID(session);
	CDebugOutput::m_sessionID = session;
}

void CDebugKit::RunServer()
{
	m_server.sin_port = htons(8001);

	CRYSOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == CRY_INVALID_SOCKET)
		return;
	if (0 == CrySock::connect(sock, (CRYSOCKADDR*)&m_server, sizeof(m_server)))
	{
	#if CRY_PLATFORM_ORBIS
		int nTrue = 1;
		if (CrySock::setsockopt(sock, SOL_SOCKET, SO_NBIO, &nTrue, sizeof(nTrue)) == 0)
	#elif CRY_PLATFORM_WINDOWS
		u_long nTrue = 1;
		if (ioctlsocket(sock, FIONBIO, &nTrue) != SOCKET_ERROR)
	#else
		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) != -1)
	#endif
		{
			CDebugOutput::Run(sock);
			CrySock::shutdown(sock, SD_BOTH);
			CrySleep(1500);
		}
	}
	CrySock::closesocket(sock);
}

#endif
