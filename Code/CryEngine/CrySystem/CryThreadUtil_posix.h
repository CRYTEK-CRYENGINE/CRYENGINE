// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////
// NOTE: INTERNAL HEADER NOT FOR PUBLIC USE
// This header should only be include by SystemThreading.cpp only
// It provides an interface for PThread intrinsics
// It's only client should be CThreadManager which should manage all thread interaction
#if !defined(INCLUDED_FROM_SYSTEM_THREADING_CPP)
	#error "CRYTEK INTERNAL HEADER. ONLY INCLUDE FROM SYSTEMTHRADING.CPP."
#endif
//////////////////////////////////////////////////////////////////////////

#define DEFAULT_THREAD_STACK_SIZE_KB 0
#define CRY_PTHREAD_THREAD_NAME_MAX  16

//////////////////////////////////////////////////////////////////////////
// THREAD CREATION AND MANAGMENT
//////////////////////////////////////////////////////////////////////////
namespace CryThreadUtil
{
// Define type for platform specific thread handle
typedef pthread_t TThreadHandle;

struct SThreadCreationDesc
{
	// Define platform specific thread entry function functor type
	typedef void* (* EntryFunc)(void*);

	const char* szThreadName;
	EntryFunc   fpEntryFunc;
	void*       pArgList;
	uint32      nStackSizeInBytes;
};

//////////////////////////////////////////////////////////////////////////
TThreadHandle CryGetCurrentThreadHandle()
{
	return (TThreadHandle)pthread_self();
}

//////////////////////////////////////////////////////////////////////////
// Note: Handle must be closed lated via CryCloseThreadHandle()
TThreadHandle CryDuplicateThreadHandle(const TThreadHandle& hThreadHandle)
{
	// Do not do anything
	// If you add a new platform which duplicates handles make sure to mirror the change in CryCloseThreadHandle(..)
	return hThreadHandle;
}

//////////////////////////////////////////////////////////////////////////
void CryCloseThreadHandle(TThreadHandle& hThreadHandle)
{
	pthread_detach(hThreadHandle);
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetCurrentThreadId()
{
	return threadID(pthread_self());
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetThreadId(TThreadHandle hThreadHandle)
{
	return threadID(hThreadHandle);
}

//////////////////////////////////////////////////////////////////////////
// Note: On OSX the thread name can only be set by the thread itself.
void CrySetThreadName(TThreadHandle pThreadHandle, const char* sThreadName)
{
	char threadName[CRY_PTHREAD_THREAD_NAME_MAX];
	if (!cry_strcpy(threadName, sThreadName))
	{
		CryLog("<ThreadInfo> CrySetThreadName: input thread name '%s' truncated to '%s'", sThreadName, threadName);
	}
#if CRY_PLATFORM_APPLE
	// On OSX the thread name can only be set by the thread itself.
	assert(pthread_equal(pthread_self(), (pthread_t)pThreadHandle));

	if (pthread_setname_np(threadName) != 0)
#else
	if (pthread_setname_np(pThreadHandle, threadName) != 0)
#endif
	{
		switch (errno)
		{
		case ERANGE:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadName: Unable to rename thread \"%s\". Error Msg: \"Name to long. Exceeds %d bytes.\"", sThreadName, CRY_PTHREAD_THREAD_NAME_MAX);
			break;
		default:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadName: Unsupported error code: %i", errno);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadAffinityMask(TThreadHandle pThreadHandle, DWORD dwAffinityMask)
{
#if CRY_PLATFORM_ANDROID
	// Not supported on ANDROID
	// Alternative solution
	// Watch out that android will clear the mask after a core has been switched off hence loosing the affinity mask setting!
	// http://stackoverflow.com/questions/16319725/android-set-thread-affinity
#elif CRY_PLATFORM_APPLE
	#pragma message "Warning: <ThreadInfo> CrySetThreadAffinityMask not implemented for platform"
	// Implementation details can be found here
	// https://developer.apple.com/library/mac/releasenotes/Performance/RN-AffinityAPI/
#else
	cpu_set_t cpu_mask;
	CPU_ZERO(&cpu_mask);
	for (int cpu = 0; cpu < sizeof(cpu_mask) * 8; ++cpu)
	{
		if (dwAffinityMask & (1 << cpu))
		{
			CPU_SET(cpu, &cpu_mask);
		}
	}

	if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) != 0)
	{
		switch (errno)
		{
		case EFAULT:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadAffinityMask: Supplied memory address was invalid.");
			break;
		case EINVAL:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadAffinityMask: The affinity bit mask [%u] contains no processors that are currently physically on the system and permitted to the	process .", dwAffinityMask);
			break;
		case EPERM:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadAffinityMask: The calling process does not have appropriate privileges. Mask [%u].", dwAffinityMask);
			break;
		case ESRCH:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadAffinityMask: The process whose ID is pid could not be found.");
			break;
		default:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> CrySetThreadAffinityMask: Unsupported error code: %i", errno);
			break;
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadPriority(TThreadHandle pThreadHandle, DWORD dwPriority)
{
	int policy;
	struct sched_param param;

	pthread_getschedparam(pThreadHandle, &policy, &param);
	param.sched_priority = sched_get_priority_max(dwPriority);
	pthread_setschedparam(pThreadHandle, policy, &param);
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadPriorityBoost(TThreadHandle pThreadHandle, bool bEnabled)
{
	// Not supported
}

//////////////////////////////////////////////////////////////////////////
bool CryCreateThread(TThreadHandle* pThreadHandle, const SThreadCreationDesc& threadDesc)
{
	uint32 nStackSize = threadDesc.nStackSizeInBytes != 0 ? threadDesc.nStackSizeInBytes : DEFAULT_THREAD_STACK_SIZE_KB * 1024;

	assert(pThreadHandle != THREADID_NULL);
	pthread_attr_t threadAttr;
	sched_param schedParam;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&threadAttr, nStackSize);

	const int err = pthread_create(
	  pThreadHandle,
	  &threadAttr,
	  threadDesc.fpEntryFunc,
	  threadDesc.pArgList);

	// Handle error on thread creation
	switch (err)
	{
	case 0:
		// No error
		break;
	case EAGAIN:
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to create thread \"%s\". Error Msg: \"Insufficient resources to create another thread, or a system-imposed limit on the number of threads was encountered.\"", threadDesc.szThreadName);
		return false;
	case EINVAL:
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to create thread \"%s\". Error Msg: \"Invalid attribute setting for thread creation.\"", threadDesc.szThreadName);
		return false;
	case EPERM:
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to create thread \"%s\". Error Msg: \"No permission to set the scheduling policy and parameters specified in attribute setting\"", threadDesc.szThreadName);
		return false;
	default:
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to create thread \"%s\". Unknown error message. Error code %i", threadDesc.szThreadName, err);
		break;
	}

	// Print info to log
	CryComment("<ThreadInfo>: New thread \"%s\" | StackSize: %u(KB)", threadDesc.szThreadName, threadDesc.nStackSizeInBytes / 1024);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CryThreadExitCall()
{
	// Notes on: pthread_exit
	// A thread that was create with pthread_create implicitly calls pthread_exit when the thread returns from its start routine (the function that was first called after a thread was created).
	// pthread_exit(NULL);
}
}

//////////////////////////////////////////////////////////////////////////
// FLOATING POINT EXCEPTIONS
//////////////////////////////////////////////////////////////////////////
namespace CryThreadUtil
{
///////////////////////////////////////////////////////////////////////////
void EnableFloatExceptions(EFPE_Severity eFPESeverity)
{
	// TODO:
	// Not implemented
	// for potential implementation see http://linux.die.net/man/3/feenableexcept
}

//////////////////////////////////////////////////////////////////////////
void EnableFloatExceptions(threadID nThreadId, EFPE_Severity eFPESeverity)
{
	// TODO:
	// Not implemented
	// for potential implementation see http://linux.die.net/man/3/feenableexcept
}

//////////////////////////////////////////////////////////////////////////
uint GetFloatingPointExceptionMask()
{
	// Not implemented
	return ~0;
}

//////////////////////////////////////////////////////////////////////////
void SetFloatingPointExceptionMask(uint nMask)
{
	// Not implemented
}
}
