// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Project/ProjectDefines.h>
#include <CrySystem/ICmdLine.h>

#if defined(ENABLE_STATS_AGENT)

	#include <cstdlib>
	#include <cstring>
	#include <atomic>

	#include "StatsAgent.h"
	#include "StatsAgentPipe.h"
	#include <CryThreading/MultiThread_Containers.h>

	#include <CryThreading/IThreadManager.h>

class CStatsAgentImpl
{
	/* 10mins - assuming 60fps which the game can run at when it is in the precache phase */
	static const int DELAY_MESSAGES_TIMEOUT = 60 * 60 * 10;
	static const int DELAY_MESSAGES_EXIT_DELAY = 10;

	class CThread : public ::IThread
	{
	public:
		CThread(class CStatsAgentImpl& statsAgentImpl)
			: m_StatsAgentImpl(statsAgentImpl)
		{
			if (!gEnv->pThreadManager->SpawnThread(this, "StatsAgent"))
			{
				CryFatalError((string().Format("Error spawning \"%s\" thread.", "StatsAgent").c_str()));
			}
		}

		virtual ~CThread()
		{
			SignalStopWork();
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}

		// Signals the thread that it should not accept anymore work and exit
		void SignalStopWork()
		{
			m_bQuit = true;
		}

	protected:
		// Start accepting work on thread
		virtual void ThreadEntry() override;

	private:
		class CStatsAgentImpl& m_StatsAgentImpl;
		std::atomic<bool>      m_bQuit = false;
	};

	class CSystemEventListener : public ::ISystemEventListener
	{
	public:
		CSystemEventListener()
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CStatsAgent");
		}

		virtual ~CSystemEventListener()
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	protected:
		virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
		{
			switch (event)
			{
			case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
			case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
			case ESYSTEM_EVENT_LEVEL_LOAD_START:
			case ESYSTEM_EVENT_LEVEL_UNLOAD:
				{
					CryLogAlways("CStatsAgent: starting delay of messages");
					CStatsAgent::SetDelayMessages(true);
					break;
				}
			case ESYSTEM_EVENT_LEVEL_LOAD_END:
			case ESYSTEM_EVENT_LEVEL_PRECACHE_START:
				{
					CryLogAlways("CStatsAgent: starting long timeout of messages");
					CStatsAgent::SetDelayTimeout(DELAY_MESSAGES_TIMEOUT);
					break;
				}
			case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
			case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
				{
					CryLogAlways("CStatsAgent: stopping delay of messages");
					CStatsAgent::SetDelayMessages(false);
					break;
				}
			}
		}
	};

	CryMT::vector<CryStringLocal> m_MessageQueue;
	std::unique_ptr<CThread>      m_pStatsThread;
	bool                          m_bDelayMessages = false;
	int                           m_nDelayMessagesCounter = -1;
	CSystemEventListener          m_listener;

	/* Helper functions */

	//Whether string starts with certain prefix literal.
	template<int Length>
	static bool StrStartsWith(const CryStringLocal& str, const char(&szSubstr)[Length])
	{
		static_assert(Length >= 1, "invalid string literal");     //at least '\0'
		if (Length - 1 > str.GetLength())
			return false;
		return strncmp(str.c_str(), szSubstr, Length - 1) == 0;
	}

	//Returns remaining 0-terminated substring if matching prefix, or nullptr otherwise.
	template<int Length>
	static const char* TryParse(const CryStringLocal& str, const char(&szSubstr)[Length])
	{
		if (StrStartsWith(str, szSubstr))
		{
			return str.c_str() + Length - 1;
		}
		return nullptr;
	}

public:
	CStatsAgentImpl(const char* szPipeName)
	{
		if (CStatsAgentPipe::OpenPipe(szPipeName))
		{
			// Kick off a thread to listen on the pipe
			m_pStatsThread = stl::make_unique<CThread>(*this);
		}
	}

	~CStatsAgentImpl()
	{
		CStatsAgentPipe::ClosePipe();
	}

	void SetDelayTimeout(const int timeout)
	{
		m_nDelayMessagesCounter = timeout;
	}

	void SetDelayMessages(bool bEnable)
	{
		if (bEnable)
		{
			m_bDelayMessages = true;
			m_nDelayMessagesCounter = -1;
		}
		else if (m_bDelayMessages)
		{
			SetDelayTimeout(DELAY_MESSAGES_EXIT_DELAY);
		}
	}

	void Update()
	{
		if (CStatsAgentPipe::IsPipeOpen())
		{
			if (m_bDelayMessages)
			{
				auto numMessages = m_MessageQueue.size();
				CryLogAlways("CStatsAgent: Delaying processing of %d messages delayCounter:%d", numMessages, m_nDelayMessagesCounter);
				for (auto i = 0; i < numMessages; i++)
				{
					CryLogAlways("CStatsAgent: Message[%d] '%s'", i, m_MessageQueue[i].c_str());
				}
				if (m_nDelayMessagesCounter >= 0)
				{
					--m_nDelayMessagesCounter;
					if (m_nDelayMessagesCounter <= 0)
					{
						m_nDelayMessagesCounter = 0;
						m_bDelayMessages = false;
					}
				}
			}
			CryStringLocal message;
			std::vector<CryStringLocal> unprocessedMessages;
			bool bSaveMessages = false;
			while (m_MessageQueue.try_pop_front(message))
			{
				// Can't process the message - have to save it and put it back after the queue has been drained
				if (bSaveMessages)
				{
					unprocessedMessages.push_back(message);
					continue;
				}
				// Just return the dump filename
				if (StrStartsWith(message, "getdumpfilename"))
				{
					CryReplayInfo info {};
					CryGetIMemReplay()->GetInfo(info);
					const char* pFilename = info.filename;
					if (!pFilename) { pFilename = "<unknown>"; }

					CStatsAgentPipe::Send(pFilename, "dumpfile", message.c_str());
				}
				// Get the value of a CVAR
				else if (const char* pCVARName = TryParse(message, "getcvarvalue "))
				{
					if (ICVar* pValue = gEnv->pConsole->GetCVar(pCVARName))
					{
						CStatsAgentPipe::Send(pValue->GetString(), "cvarvalue", message.c_str());
					}
					else
					{
						CryFixedStringT<64> response = "Unknown CVAR '";
						response += pCVARName;
						response += "'";
						CStatsAgentPipe::Send(response.c_str(), "error", message.c_str());
					}
				}
				// Create a new CVAR
				else if (const char* pCVARName = TryParse(message, "createcvar "))
				{
					REGISTER_STRING(pCVARName, "", 0, "Amble CVAR");

					CStatsAgentPipe::Send("finished", nullptr, message.c_str());
				}
				// Execute the command
				else if (const char* pCommand = TryParse(message, "exec "))
				{
					if (!m_bDelayMessages)
					{
						// Execute the rest of the string
						if (gEnv && gEnv->pConsole)
						{
							gEnv->pConsole->ExecuteString(pCommand);

							CStatsAgentPipe::Send("finished", nullptr, message.c_str());
						}
					}
					else
					{
						// Stop processing the message queue - store the queue and restore it
						bSaveMessages = true;
						unprocessedMessages.push_back(message);
					}
				}
				// Unknown command
				else
				{
					CryFixedStringT<64> response = "Unknown command '";
					response += message.c_str();
					response += "'";
					CStatsAgentPipe::Send(response.c_str(), "error", message.c_str());
				}
			}
			m_MessageQueue.append(unprocessedMessages.begin(), unprocessedMessages.end());
		}
	}

	void ThreadReceiveMessage()
	{
		if (CStatsAgentPipe::IsPipeOpen())
		{
			const char* szMessage = CStatsAgentPipe::Receive();
			if (szMessage != nullptr && strlen(szMessage) != 0)
			{
				m_MessageQueue.push_back(szMessage);
			}
		}
	}
};

static CStatsAgentImpl* g_pStatsAgentImpl = nullptr;

void CStatsAgentImpl::CThread::ThreadEntry()
{
	while (!m_bQuit)
	{
		m_StatsAgentImpl.ThreadReceiveMessage();
		CrySleep(1000);
	}
}

void CStatsAgent::CreatePipe(const ICmdLineArg* pPipeName)
{
	CRY_ASSERT(pPipeName != nullptr);
	CRY_ASSERT(g_pStatsAgentImpl == nullptr);

	CryLogAlways("CStatsAgent::CreatePipe: pPipeName = %s", pPipeName->GetValue());
	string sPipeName = pPipeName->GetValue();
	sPipeName.Trim();//Trimming is essential as command line arg value may contain spaces at the end
	g_pStatsAgentImpl = new CStatsAgentImpl(sPipeName.c_str());
}

void CStatsAgent::ClosePipe()
{
	if (g_pStatsAgentImpl != nullptr)
	{
		CryLogAlways("CStatsAgent: ClosePipe");
		delete g_pStatsAgentImpl;
		g_pStatsAgentImpl = nullptr;
	}
}

void CStatsAgent::Update()
{
	if (g_pStatsAgentImpl != nullptr)
		g_pStatsAgentImpl->Update();
}

void CStatsAgent::SetDelayTimeout(const int timeout)
{
	CryLogAlways("CStatsAgent: SetDelayTimeout %d", timeout);
	if (g_pStatsAgentImpl != nullptr)
		g_pStatsAgentImpl->SetDelayTimeout(timeout);
}

void CStatsAgent::SetDelayMessages(bool enable)
{
	CryLogAlways("CStatsAgent: SetDelayMessages %d", enable);
	if (g_pStatsAgentImpl != nullptr)
		g_pStatsAgentImpl->SetDelayMessages(enable);
}

#endif  // defined(ENABLE_STATS_AGENT)
