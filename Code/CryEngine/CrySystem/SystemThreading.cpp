// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include "ThreadConfigManager.h"
#include <CryThreading/IThreadManager.h>
#include <CryCore/CryCustomTypes.h>

#define INCLUDED_FROM_SYSTEM_THREADING_CPP
#if CRY_PLATFORM_WINAPI
	#include "CryThreadUtil_win32.h"
#elif CRY_PLATFORM_POSIX
	#if CRY_PLATFORM_ORBIS
		#include "CryThreadUtil_sce.h"
	#else
		#include "CryThreadUtil_posix.h"
	#endif
#else
	#error ("Undefined platform")
#endif
#undef INCLUDED_FROM_SYSTEM_THREADING_CPP

//////////////////////////////////////////////////////////////////////////
static void ApplyThreadConfig(CryThreadUtil::TThreadHandle pThreadHandle, const SThreadConfig& rThreadDesc)
{
	// Apply config
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName)
	{
		CryThreadUtil::CrySetThreadName(pThreadHandle, rThreadDesc.szThreadName);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
	{
		CryThreadUtil::CrySetThreadAffinityMask(pThreadHandle, rThreadDesc.affinityFlag);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
	{
		CryThreadUtil::CrySetThreadPriority(pThreadHandle, rThreadDesc.priority);
	}
	if (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost)
	{
		CryThreadUtil::CrySetThreadPriorityBoost(pThreadHandle, !rThreadDesc.bDisablePriorityBoost);
	}

	CryComment("<ThreadInfo> Configured thread \"%s\" %s | AffinityMask: %u %s | Priority: %i %s | PriorityBoost: %s %s",
	           rThreadDesc.szThreadName, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName) ? "" : "(ignored)",
	           rThreadDesc.affinityFlag, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) ? "" : "(ignored)",
	           rThreadDesc.priority, (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) ? "" : "(ignored)",
	           !rThreadDesc.bDisablePriorityBoost ? "enabled" : "disabled", (rThreadDesc.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost) ? "" : "(ignored)");
}

//////////////////////////////////////////////////////////////////////////
struct SThreadMetaData : public CMultiThreadRefCount
{
	SThreadMetaData()
		: m_pThreadTask(0)
		, m_pThreadMngr(nullptr)
		, m_threadHandle(0)
		, m_threadId(0)
		, m_threadName("Cry_UnnamedThread")
		, m_isRunning(false)
	{
	}

	IThread*                                m_pThreadTask; // Pointer to thread task to be executed
	CThreadManager*                         m_pThreadMngr; // Pointer to thread manager

	CryThreadUtil::TThreadHandle            m_threadHandle; // Thread handle
	threadID                                m_threadId;     // The active threadId, 0 = Invalid Id

	CryMutex                                m_threadExitMutex;     // Mutex used to safeguard thread exit condition signaling
	CryConditionVariable                    m_threadExitCondition; // Signaled when the thread is about to exit

	CryFixedStringT<THREAD_NAME_LENGTH_MAX> m_threadName; // Thread name
	volatile bool                           m_isRunning;  // Indicates the thread is not ready to exit yet
};

//////////////////////////////////////////////////////////////////////////
class CThreadManager : public IThreadManager
{
public:
	// <interfuscator:shuffle>
	virtual ~CThreadManager()
	{
	}

	virtual bool          SpawnThread(IThread* pThread, const char* sThreadName, ...) override;
	virtual bool          JoinThread(IThread* pThreadTask, EJoinMode eJoinMode) override;

	virtual bool          RegisterThirdPartyThread(void* pThreadHandle, const char* sThreadName, ...) override;
	virtual bool          UnRegisterThirdPartyThread(const char* sThreadName, ...) override;

	virtual const char*   GetThreadName(threadID nThreadId) override;
	virtual threadID      GetThreadId(const char* sThreadName, ...) override;

	virtual void          ForEachOtherThread(IThreadManager::ThreadModifFunction fpThreadModiFunction, void* pFuncData = 0) override;

	virtual void          EnableFloatExceptions(EFPE_Severity eFPESeverity, threadID nThreadId = 0) override;
	virtual void          EnableFloatExceptionsForEachOtherThread(EFPE_Severity eFPESeverity) override;

	virtual uint          GetFloatingPointExceptionMask() override;
	virtual void          SetFloatingPointExceptionMask(uint nMask) override;

	IThreadConfigManager* GetThreadConfigManager() override
	{
		return &m_threadConfigManager;
	}
	// </interfuscator:shuffle>
private:
#if CRY_PLATFORM_POSIX
	static void*              RunThread(void* thisPtr);
#elif CRY_PLATFORM_WINAPI
	static unsigned __stdcall RunThread(void* thisPtr);
#else
	#error "Unsupported platform"
#endif

private:
	bool     UnregisterThread(IThread* pThreadTask);

	bool     SpawnThreadImpl(IThread* pThread, const char* sThreadName);

	bool     RegisterThirdPartyThreadImpl(CryThreadUtil::TThreadHandle pThreadHandle, const char* sThreadName);
	bool     UnRegisterThirdPartyThreadImpl(const char* sThreadName);

	threadID GetThreadIdImpl(const char* sThreadName);

private:
	// Note: Guard SThreadMetaData with a _smart_ptr and lock to ensure that a thread waiting to be signaled by another still
	// has access to valid SThreadMetaData even though the other thread terminated and as a result unregistered itself from the CThreadManager.
	// An example would be the join method. Where one thread waits on a signal from an other thread to terminate and release its SThreadMetaData,
	// sharing the same SThreadMetaData condition variable.
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>                                                SpawnedThreadMap;
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>::iterator                                      SpawnedThreadMapIter;
	typedef std::map<IThread*, _smart_ptr<SThreadMetaData>>::const_iterator                                SpawnedThreadMapConstIter;
	typedef std::pair<IThread*, _smart_ptr<SThreadMetaData>>                                               ThreadMapPair;

	typedef std::map<CryFixedStringT<THREAD_NAME_LENGTH_MAX>, _smart_ptr<SThreadMetaData>>                 SpawnedThirdPartyThreadMap;
	typedef std::map<CryFixedStringT<THREAD_NAME_LENGTH_MAX>, _smart_ptr<SThreadMetaData>>::iterator       SpawnedThirdPartyThreadMapIter;
	typedef std::map<CryFixedStringT<THREAD_NAME_LENGTH_MAX>, _smart_ptr<SThreadMetaData>>::const_iterator SpawnedThirdPartyThreadMapConstIter;
	typedef std::pair<CryFixedStringT<THREAD_NAME_LENGTH_MAX>, _smart_ptr<SThreadMetaData>>                ThirdPartyThreadMapPair;

	CryCriticalSection         m_spawnedThreadsLock; // Use lock for the rare occasion a thread is created/destroyed
	SpawnedThreadMap           m_spawnedThreads;     // Holds information of all spawned threads (through this system)

	CryCriticalSection         m_spawnedThirdPartyThreadsLock; // Use lock for the rare occasion a thread is created/destroyed
	SpawnedThirdPartyThreadMap m_spawnedThirdPartyThread;      // Holds information of all registered 3rd party threads (through this system)

	CThreadConfigManager       m_threadConfigManager;
};

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_POSIX
void*              CThreadManager::RunThread(void* thisPtr)
#elif CRY_PLATFORM_WINAPI
unsigned __stdcall CThreadManager::RunThread(void* thisPtr)
#else
	#error "Unsupported platform"
#endif
{
	// Check that we are not spawning a thread before gEnv->pSystem has been set
	// Otherwise we cannot enable floating point exceptions
	if (!gEnv || !gEnv->pSystem)
	{
		CryFatalError("[Error]: CThreadManager::RunThread requires gEnv->pSystem to be initialized.");
	}

	CRY_PROFILE_MARKER("Thread_Run");

	IThreadConfigManager* pThreadConfigMngr = gEnv->pThreadManager->GetThreadConfigManager();

	SThreadMetaData* pThreadData = reinterpret_cast<SThreadMetaData*>(thisPtr);
	pThreadData->m_threadId = CryThreadUtil::CryGetCurrentThreadId();

	// Apply config
	const SThreadConfig* pThreadConfig = pThreadConfigMngr->GetThreadConfig(pThreadData->m_threadName.c_str());
	ApplyThreadConfig(pThreadData->m_threadHandle, *pThreadConfig);

	CRY_PROFILE_THREADNAME(pThreadData->m_threadName.c_str());

	// Config not found, append thread name with no config tag
	if (pThreadConfig == pThreadConfigMngr->GetDefaultThreadConfig())
	{
		CryFixedStringT<THREAD_NAME_LENGTH_MAX> tmpString(pThreadData->m_threadName);
		const char* cNoConfigAppendix = "(NoCfgFound)";
		int nNumCharsToReplace = strlen(cNoConfigAppendix);

		// Replace thread name ending
		if (pThreadData->m_threadName.size() > THREAD_NAME_LENGTH_MAX - nNumCharsToReplace)
		{
			tmpString.replace(THREAD_NAME_LENGTH_MAX - nNumCharsToReplace, nNumCharsToReplace, cNoConfigAppendix, nNumCharsToReplace);
		}
		else
		{
			tmpString.append(cNoConfigAppendix);
		}

		// Print to log
		if (pThreadConfigMngr->ConfigLoaded())
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> No Thread config found for thread %s using ... default config.", pThreadData->m_threadName.c_str());
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Thread config not loaded yet. Hence no thread config was found for thread %s ... using default config.", pThreadData->m_threadName.c_str());
		}

		// Rename Thread
		CryThreadUtil::CrySetThreadName(pThreadData->m_threadHandle, tmpString.c_str());
		CRY_PROFILE_THREADNAME(tmpString.c_str());
	}

	// Enable FPEs
	gEnv->pThreadManager->EnableFloatExceptions((EFPE_Severity)g_cvars.sys_float_exceptions);

	// Execute thread code
	pThreadData->m_pThreadTask->ThreadEntry();

	// Disable FPEs
	gEnv->pThreadManager->EnableFloatExceptions(eFPE_None);

	// Signal imminent thread end
	pThreadData->m_threadExitMutex.Lock();
	pThreadData->m_isRunning = false;
	pThreadData->m_threadExitCondition.Notify();
	pThreadData->m_threadExitMutex.Unlock();

	// Unregister thread
	// Note: Unregister after m_threadExitCondition.Notify() to ensure pThreadData is still valid
	pThreadData->m_pThreadMngr->UnregisterThread(pThreadData->m_pThreadTask);

	CRY_PROFILE_MARKER("Thread_Stop");
	CryThreadUtil::CryThreadExitCall();

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::JoinThread(IThread* pThreadTask, EJoinMode eJoinMode)
{
	// Get thread object
	_smart_ptr<SThreadMetaData> pThreadImpl = 0;
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
		if (res == m_spawnedThreads.end())
		{
			// Thread has already finished and unregistered itself.
			// As it is complete we cannot wait for it.
			// Hence return true.
			return true;
		}

		pThreadImpl = res->second; // Keep object alive
	}

	// On try join, exit if the thread is not in a state to exit
	if (eJoinMode == eJM_TryJoin && pThreadImpl->m_isRunning)
	{
		return false;
	}

	// Wait for completion of the target thread exit condition
	pThreadImpl->m_threadExitMutex.Lock();
	while (pThreadImpl->m_isRunning)
	{
		pThreadImpl->m_threadExitCondition.Wait(pThreadImpl->m_threadExitMutex);
	}
	pThreadImpl->m_threadExitMutex.Unlock();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnregisterThread(IThread* pThreadTask)
{
	AUTO_LOCK(m_spawnedThreadsLock);

	SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
	if (res == m_spawnedThreads.end())
	{
		// Duplicate thread deletion
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: UnregisterThread: Unable to unregister thread. Thread name could not be found. Double deletion? IThread pointer: %p", pThreadTask);
		return false;
	}

	m_spawnedThreads.erase(res);
	return true;
}

//////////////////////////////////////////////////////////////////////////
const char* CThreadManager::GetThreadName(threadID nThreadId)
{
	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId == nThreadId)
			{
				return iter->second->m_threadName.c_str();
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId == nThreadId)
			{
				return iter->second->m_threadName.c_str();
			}
		}
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::ForEachOtherThread(IThreadManager::ThreadModifFunction fpThreadModiFunction, void* pFuncData)
{
	threadID nCurThreadId = CryThreadUtil::CryGetCurrentThreadId();

	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId != nCurThreadId)
			{
				fpThreadModiFunction(iter->second->m_threadId, pFuncData);
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadId != nCurThreadId)
			{
				fpThreadModiFunction(iter->second->m_threadId, pFuncData);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::SpawnThread(IThread* pThreadTask, const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!cry_vsprintf(strThreadName, sThreadName, args))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Spawn thread
	bool ret = SpawnThreadImpl(pThreadTask, strThreadName);

	if (!ret)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: CSystem::SpawnThread error spawning thread: \"%s\"", strThreadName);
	}

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::SpawnThreadImpl(IThread* pThreadTask, const char* sThreadName)
{
	if (pThreadTask == NULL)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "<ThreadInfo>: SpawnThread '%s' ThreadTask is NULL : ignoring", sThreadName);
		return false;
	}

	// Init thread meta data
	SThreadMetaData* pThreadMetaData = new SThreadMetaData();
	pThreadMetaData->m_pThreadTask = pThreadTask;
	pThreadMetaData->m_pThreadMngr = this;
	pThreadMetaData->m_threadName = sThreadName;

	// Add thread to map
	{
		AUTO_LOCK(m_spawnedThreadsLock);
		SpawnedThreadMapIter res = m_spawnedThreads.find(pThreadTask);
		if (res != m_spawnedThreads.end())
		{
			// Thread with same name already spawned
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: SpawnThread: Thread \"%s\" already exists.", sThreadName);
			delete pThreadMetaData;
			return false;
		}

		// Insert thread data
		m_spawnedThreads.insert(ThreadMapPair(pThreadTask, pThreadMetaData));
	}

	// Load config if we can and if no config has been defined to be loaded
	const SThreadConfig* pThreadConfig = gEnv->pThreadManager->GetThreadConfigManager()->GetThreadConfig(sThreadName);

	// Create thread description
	CryThreadUtil::SThreadCreationDesc desc = { sThreadName, RunThread, pThreadMetaData, pThreadConfig->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize ? pThreadConfig->stackSizeBytes : 0 };

	// Spawn new thread
	pThreadMetaData->m_isRunning = CryThreadUtil::CryCreateThread(&(pThreadMetaData->m_threadHandle), desc);

	// Validate thread creation
	if (!pThreadMetaData->m_isRunning)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: SpawnThread: Could not spawn thread \"%s\".", sThreadName);

		// Remove thread from map (also releases SThreadMetaData _smart_ptr)
		m_spawnedThreads.erase(m_spawnedThreads.find(pThreadTask));
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::RegisterThirdPartyThread(void* pThreadHandle, const char* sThreadName, ...)
{
	if (!pThreadHandle)
	{
		pThreadHandle = reinterpret_cast<void*>(CryThreadUtil::CryGetCurrentThreadHandle());
	}

	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!cry_vsprintf(strThreadName, sThreadName, args))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Register 3rd party thread
	bool ret = RegisterThirdPartyThreadImpl(reinterpret_cast<CryThreadUtil::TThreadHandle>(pThreadHandle), strThreadName);

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::RegisterThirdPartyThreadImpl(CryThreadUtil::TThreadHandle threadHandle, const char* sThreadName)
{
	if (strcmp(sThreadName, "") == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: CThreadManager::RegisterThirdPartyThread error registering third party thread. No name provided.");
		return false;
	}
	// Init thread meta data
	SThreadMetaData* pThreadMetaData = new SThreadMetaData();
	pThreadMetaData->m_pThreadTask = 0;
	pThreadMetaData->m_pThreadMngr = this;
	pThreadMetaData->m_threadName = sThreadName;
	pThreadMetaData->m_threadHandle = CryThreadUtil::CryDuplicateThreadHandle(threadHandle); // Ensure that we are not storing a pseudo handle
	pThreadMetaData->m_threadId = CryThreadUtil::CryGetThreadId(pThreadMetaData->m_threadHandle);

	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		// Check for duplicate
		SpawnedThirdPartyThreadMapConstIter res = m_spawnedThirdPartyThread.find(sThreadName);
		if (res != m_spawnedThirdPartyThread.end())
		{
			CryFatalError("CThreadManager::RegisterThirdPartyThread - Unable to register thread \"%s\""
			              "because another third party thread with the same name \"%s\" has already been registered with ThreadHandle: %p",
			              sThreadName, res->second->m_threadName.c_str(), reinterpret_cast<void*>(threadHandle));

			delete pThreadMetaData;
			return false;
		}

		// Insert thread data
		m_spawnedThirdPartyThread.insert(ThirdPartyThreadMapPair(pThreadMetaData->m_threadName.c_str(), pThreadMetaData));
	}

	// Get thread config
	const SThreadConfig* pThreadConfig = gEnv->pThreadManager->GetThreadConfigManager()->GetThreadConfig(sThreadName);

	// Apply config (if not default config)
	if (strcmp(pThreadConfig->szThreadName, sThreadName) == 0)
	{
		ApplyThreadConfig(threadHandle, *pThreadConfig);
	}

	// Update FP exception mask for 3rd party thread
	if (pThreadMetaData->m_threadId)
	{
		CryThreadUtil::EnableFloatExceptions(pThreadMetaData->m_threadId, (EFPE_Severity)g_cvars.sys_float_exceptions);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnRegisterThirdPartyThread(const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!cry_vsprintf(strThreadName, sThreadName, args))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i.", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Unregister 3rd party thread
	bool ret = UnRegisterThirdPartyThreadImpl(strThreadName);

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadManager::UnRegisterThirdPartyThreadImpl(const char* sThreadName)
{
	AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

	SpawnedThirdPartyThreadMapIter res = m_spawnedThirdPartyThread.find(sThreadName);
	if (res == m_spawnedThirdPartyThread.end())
	{
		// Duplicate thread deletion
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: UnRegisterThirdPartyThread: Unable to unregister thread. Thread name \"%s\" could not be found. Double deletion?", sThreadName);
		return false;
	}

	// Close thread handle
	CryThreadUtil::CryCloseThreadHandle(res->second->m_threadHandle);

	// Delete reference from container
	m_spawnedThirdPartyThread.erase(res);
	return true;
}

//////////////////////////////////////////////////////////////////////////
threadID CThreadManager::GetThreadId(const char* sThreadName, ...)
{
	va_list args;
	va_start(args, sThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!cry_vsprintf(strThreadName, sThreadName, args))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i. ", sThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Get thread name
	threadID ret = GetThreadIdImpl(strThreadName);

	va_end(args);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
threadID CThreadManager::GetThreadIdImpl(const char* sThreadName)
{
	// Loop over internally spawned threads
	{
		AUTO_LOCK(m_spawnedThreadsLock);

		SpawnedThreadMapConstIter iter = m_spawnedThreads.begin();
		SpawnedThreadMapConstIter iterEnd = m_spawnedThreads.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadName.compare(sThreadName) == 0)
			{
				return iter->second->m_threadId;
			}
		}
	}

	// Loop over third party threads
	{
		AUTO_LOCK(m_spawnedThirdPartyThreadsLock);

		SpawnedThirdPartyThreadMapConstIter iter = m_spawnedThirdPartyThread.begin();
		SpawnedThirdPartyThreadMapConstIter iterEnd = m_spawnedThirdPartyThread.end();

		for (; iter != iterEnd; ++iter)
		{
			if (iter->second->m_threadName.compare(sThreadName) == 0)
			{
				return iter->second->m_threadId;
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
static void EnableFPExceptionsForThread(threadID nThreadId, void* pData)
{
	EFPE_Severity eFPESeverity = *(EFPE_Severity*)pData;
	CryThreadUtil::EnableFloatExceptions(nThreadId, eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::EnableFloatExceptions(EFPE_Severity eFPESeverity, threadID nThreadId /*=0*/)
{
	CryThreadUtil::EnableFloatExceptions(nThreadId, eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::EnableFloatExceptionsForEachOtherThread(EFPE_Severity eFPESeverity)
{
	ForEachOtherThread(EnableFPExceptionsForThread, &eFPESeverity);
}

//////////////////////////////////////////////////////////////////////////
uint CThreadManager::GetFloatingPointExceptionMask()
{
	return CryThreadUtil::GetFloatingPointExceptionMask();
}

//////////////////////////////////////////////////////////////////////////
void CThreadManager::SetFloatingPointExceptionMask(uint nMask)
{
	CryThreadUtil::SetFloatingPointExceptionMask(nMask);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitThreadSystem()
{
	m_pThreadManager = new CThreadManager();
	m_env.pThreadManager = m_pThreadManager;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ShutDownThreadSystem()
{
	SAFE_DELETE(m_pThreadManager);
}
