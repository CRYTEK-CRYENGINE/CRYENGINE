// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugOutput.h"

void CDebugOutput::Put(uint8 val)
{
	CryAutoLock<CDebugOutput> lk(*this);
	if (m_buffers.empty() || !m_buffers.back().ready)
		m_buffers.push_back(SBuf());
	NET_ASSERT(m_buffers.back().ready);
	SBuf& buf = m_buffers.back();
	buf.data[buf.sz++] = val;
	if (buf.sz == DATA_SIZE)
		buf.ready = false;
}

void CDebugOutput::Run(CRYSOCKET sock)
{
	{
		CryAutoLock<CDebugOutput> lk(*this);
		m_buffers.resize(0);
		Write('i');
		Write(m_sessionID.GetHumanReadable());
	}

	while (true)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		timeval timeout;
		timeout.tv_sec = 30;
		timeout.tv_usec = 0;
		// not portable to *nix
		int n = select(CRYSOCKET(0), NULL, &fds, NULL, &timeout);

		switch (n)
		{
		case 0:
		case CRY_SOCKET_ERROR:
			return;
		}

		bool sleep = false;
		{
			CryAutoLock<CDebugOutput> lk(*this);
			if (m_buffers.empty() || m_buffers.front().ready)
				sleep = true;
			else
			{
				SBuf& buf = m_buffers.front();
				int w = CrySock::send(sock, buf.data + buf.pos, buf.sz - buf.pos, 0);
				CrySock::eCrySockError sockErr = CrySock::TranslateSocketError(w);
				if (sockErr != CrySock::eCSE_NO_ERROR)
				{
					if (sockErr == CrySock::eCSE_EWOULDBLOCK)
						sleep = true;
					else
						return;
				}
				else
				{
					buf.pos += w;
					if (buf.pos == buf.sz)
						m_buffers.pop_front();
				}
			}
		}
		if (sleep)
		{
			CrySleep(1000);
		}
	}
}
