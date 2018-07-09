// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////
// NOTE: INTERNAL HEADER NOT FOR PUBLIC USE
// This header should only be include by SystemThreading.cpp only
// It provides an interface for WinApi intrinsics
// It's only client should be CThreadManager which should manage all thread interaction
#if !defined(INCLUDED_FROM_SYSTEM_THREADING_CPP)
	#error "CRYTEK INTERNAL HEADER. ONLY INCLUDE FROM SYSTEMTHRADING.CPP."
#endif
//////////////////////////////////////////////////////////////////////////

#define DEFAULT_THREAD_STACK_SIZE_KB 0

// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
static string GetLastErrorAsString()
{
	// Get the error message, if any.
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0)
		return "";

#if CRY_PLATFORM_DURANGO
	// FormatMessageA is not available on Durango
	string errMsg;
	errMsg.Format("Error code: %u (Note: Durango does not support error code resolving.)", errorMessageID);
	return errMsg;
#else
	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), messageBuffer, 0, NULL);

	string message(messageBuffer, size);

	// Free the buffer.
	LocalFree(messageBuffer);

	return message;
#endif
}

//////////////////////////////////////////////////////////////////////////
// THREAD CREATION AND MANAGMENT
//////////////////////////////////////////////////////////////////////////
namespace CryThreadUtil
{
// Define type for platform specific thread handle
typedef THREAD_HANDLE TThreadHandle;

struct SThreadCreationDesc
{
	// Define platform specific thread entry function functor type
	typedef unsigned int (_stdcall * EntryFunc)(void*);

	const char* szThreadName;
	EntryFunc   fpEntryFunc;
	void*       pArgList;
	uint32      nStackSizeInBytes;
};

//////////////////////////////////////////////////////////////////////////
TThreadHandle CryGetCurrentThreadHandle()
{
	return GetCurrentThread();   // most likely returns pseudo handle (0xfffffffe)
}

//////////////////////////////////////////////////////////////////////////
// Note: Handle must be closed lated via CryCloseThreadHandle()
TThreadHandle CryDuplicateThreadHandle(const TThreadHandle& hThreadHandle)
{
	// NOTES:
	// GetCurrentThread() may return a psydo handle to the current thread
	// to avoid going into the slower kernel mode.
	// Hence the handle is useless when being used from an other thread.
	//	- GetCurrentThread()  -> 0xfffffffe
	//	- GetCurrentProcess() -> 0xffffffff

	HANDLE hRealHandle = 0;
	DuplicateHandle(GetCurrentProcess(),    // Source Process Handle.
	                hThreadHandle,          // Source Handle to dup.
	                GetCurrentProcess(),    // Target Process Handle.
	                &hRealHandle,           // Target Handle pointer.
	                0,                      // Options flag.
	                TRUE,                   // Inheritable flag
	                DUPLICATE_SAME_ACCESS); // Options

	return (TThreadHandle)hRealHandle;
}

//////////////////////////////////////////////////////////////////////////
void CryCloseThreadHandle(TThreadHandle& hThreadHandle)
{
	if (hThreadHandle)
	{
		CloseHandle(hThreadHandle);
	}
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetCurrentThreadId()
{
	return GetCurrentThreadId();
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetThreadId(TThreadHandle hThreadHandle)
{
	return GetThreadId(hThreadHandle);
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadName(TThreadHandle pThreadHandle, const char* sThreadName)
{
	const DWORD MS_VC_EXCEPTION = 0x406D1388;

	struct SThreadNameDesc
	{
		DWORD  dwType;      // Must be 0x1000.
		LPCSTR szName;      // Pointer to name (in user addr space).
		DWORD  dwThreadID;  // Thread ID (-1=caller thread).
		DWORD  dwFlags;     // Reserved for future use, must be zero.
	};

	SThreadNameDesc info;
	info.dwType = 0x1000;
	info.szName = sThreadName;
	info.dwThreadID = GetThreadId(pThreadHandle);
	info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable : 6312 6322)
	// warning C6312: Possible infinite loop: use of the constant EXCEPTION_CONTINUE_EXECUTION in the exception-filter expression of a try-except
	// warning C6322: empty _except block
	__try
	{
		// Raise exception to set thread name for attached debugger
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR*)&info);
	}
	__except (GetExceptionCode() == MS_VC_EXCEPTION ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER)
	{
	}
#pragma warning(pop)
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadAffinityMask(TThreadHandle pThreadHandle, DWORD dwAffinityMask)
{
	SetThreadAffinityMask(pThreadHandle, dwAffinityMask);
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadPriority(TThreadHandle pThreadHandle, DWORD dwPriority)
{
	if (!SetThreadPriority(pThreadHandle, dwPriority))
	{
		string errMsg = GetLastErrorAsString();
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to set thread priority. System Error Msg: \"%s\"", errMsg.c_str());
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySetThreadPriorityBoost(TThreadHandle pThreadHandle, bool bEnabled)
{
	SetThreadPriorityBoost(pThreadHandle, !bEnabled);
}

//////////////////////////////////////////////////////////////////////////
bool CryCreateThread(TThreadHandle* pThreadHandle, const SThreadCreationDesc& threadDesc)
{
	const uint32 nStackSize = threadDesc.nStackSizeInBytes != 0 ? threadDesc.nStackSizeInBytes : DEFAULT_THREAD_STACK_SIZE_KB * 1024;

	// Create thread
	unsigned int threadId = 0;
#if CRY_PLATFORM_DURANGO
	*pThreadHandle = (void*)CreateThread(NULL, nStackSize, (LPTHREAD_START_ROUTINE)threadDesc.fpEntryFunc, threadDesc.pArgList, CREATE_SUSPENDED, (LPDWORD)&threadId);
#else
	*pThreadHandle = (void*)_beginthreadex(NULL, nStackSize, threadDesc.fpEntryFunc, threadDesc.pArgList, CREATE_SUSPENDED, &threadId);
#endif

	if (!(*pThreadHandle))
	{
		string errMsg = GetLastErrorAsString();
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo> Unable to create thread \"%s\". System Error Msg: \"%s\"", threadDesc.szThreadName, errMsg.c_str());
		return false;
	}

	// Start thread
	ResumeThread(*pThreadHandle);

	// Print info to log
	CryComment("<ThreadInfo>: New thread \"%s\" | StackSize: %u(KB)", threadDesc.szThreadName, threadDesc.nStackSizeInBytes / 1024);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CryThreadExitCall()
{
	// Note on: ExitThread() (from MSDN)
	// ExitThread is the preferred method of exiting a thread in C code.
	// However, in C++ code, the thread is exited before any destructor can be called or any other automatic cleanup can be performed.
	// Therefore, in C++ code, you should return from your thread function.
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
	// Optimization
	// Enable DAZ/FZ
	// Denormals Are Zeros
	// Flush-to-Zero
	_controlfp(_DN_FLUSH, _MCW_DN);
	_mm_setcsr(_mm_getcsr() | _MM_FLUSH_ZERO_ON);

#ifndef _RELEASE
	if (eFPESeverity == eFPE_None)
	{
		// mask all floating exceptions off.
		_controlfp(_MCW_EM, _MCW_EM);
		_mm_setcsr(_mm_getcsr() | _MM_MASK_MASK);
	}
	else
	{
		// Clear pending exceptions
		_fpreset();

		if (eFPESeverity == eFPE_Basic)
		{
			// Enable:
			// - _EM_ZERODIVIDE
			// - _EM_INVALID
			//
			// Disable:
			// - _EM_DENORMAL
			// - _EM_OVERFLOW
			// - _EM_UNDERFLOW
			// - _EM_INEXACT

			_controlfp(_EM_INEXACT | _EM_DENORMAL | _EM_UNDERFLOW | _EM_OVERFLOW, _MCW_EM);
			_mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | (_MM_MASK_DENORM | _MM_MASK_INEXACT | _MM_MASK_UNDERFLOW | _MM_MASK_OVERFLOW));

			//_mm_setcsr(_mm_getcsr() & ~0x280);
		}

		if (eFPESeverity == eFPE_All)
		{
			// Enable:
			// - _EM_ZERODIVIDE
			// - _EM_INVALID
			// - _EM_UNDERFLOW
			// - _EM_OVERFLOW
			//
			// Disable:
			// - _EM_INEXACT
			// - _EM_DENORMAL

			_controlfp(_EM_INEXACT | _EM_DENORMAL, _MCW_EM);
			_mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | (_MM_MASK_INEXACT | _MM_MASK_DENORM));
		}
	}
#endif // _RELEASE
}

//////////////////////////////////////////////////////////////////////////
void EnableFloatExceptions(threadID nThreadId, EFPE_Severity eFPESeverity)
{
	if (eFPESeverity >= eFPE_LastEntry)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Floating Point Exception (FPE) severity is out of range. (%i)", eFPESeverity);
	}

	// Check if the thread ID matches the current thread
	if (nThreadId == 0 || nThreadId == CryGetCurrentThreadId())
	{
		EnableFloatExceptions(eFPESeverity);
		return;
	}

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, true, nThreadId);

	if (hThread == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Unable to open thread. %p", hThread);
		return;
	}

	SuspendThread(hThread);

	CONTEXT ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.ContextFlags = CONTEXT_ALL;
	if (GetThreadContext(hThread, &ctx) == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Unable to get thread context");
		ResumeThread(hThread);
		CloseHandle(hThread);
		return;
	}

#if CRY_PLATFORM_64BIT
	//////////////////////////////////////////////////////////////////////////
	// Note:
	// DO NOT USE ctx.FltSave.MxCsr ... SetThreadContext() will copy the value of ctx.MxCsr into it
	//////////////////////////////////////////////////////////////////////////
	DWORD& floatMxCsr = ctx.MxCsr;                    // Hold FPE Mask and Status for MMX (SSE) floating point registers
	WORD& floatControlWord = ctx.FltSave.ControlWord; // Hold FPE Mask for floating point registers
	WORD& floatStatuslWord = ctx.FltSave.StatusWord;  // Holds FPE Status for floating point registers
#else
	DWORD& floatMxCsr = *(DWORD*)(&ctx.ExtendedRegisters[24]); // Hold FPE Mask and Status for MMX (SSE) floating point registers
	DWORD& floatControlWord = ctx.FloatSave.ControlWord;       // Hold FPE Mask for floating point registers
	DWORD& floatStatuslWord = ctx.FloatSave.StatusWord;        // Holds FPE Status for floating point registers
#endif

	// Flush-To-Zero Mode
	// Two conditions must be met for FTZ processing to occur:
	// - The FTZ bit (bit 15) in the MXCSR register must be masked (value = 1).
	// - The underflow exception (bit 11) needs to be masked (value = 1).

	// Set flush mode to zero mode
	floatControlWord = (floatControlWord & ~_MCW_DN) | _DN_FLUSH;
	floatMxCsr = (floatMxCsr & ~_MM_FLUSH_ZERO_MASK) | (_MM_FLUSH_ZERO_ON);

#ifndef _RELEASE

	// Reset FPE bits
	floatControlWord = floatControlWord | _MCW_EM;
	floatMxCsr = floatMxCsr | _MM_MASK_MASK;

	// Clear pending exceptions
	floatStatuslWord = floatStatuslWord & ~(_SW_INEXACT | _SW_UNDERFLOW | _SW_OVERFLOW | _SW_ZERODIVIDE | _SW_INVALID | _SW_DENORMAL);
	floatMxCsr = floatMxCsr & ~(_MM_EXCEPT_INEXACT | _MM_EXCEPT_UNDERFLOW | _MM_EXCEPT_OVERFLOW | _MM_EXCEPT_DIV_ZERO | _MM_EXCEPT_INVALID | _MM_EXCEPT_DENORM);

	if (eFPESeverity == eFPE_Basic)
	{
		// Enable:
		// - _EM_ZERODIVIDE
		// - _EM_INVALID
		//
		// Disable:
		// - _EM_DENORMAL
		// - _EM_OVERFLOW
		// - _EM_UNDERFLOW
		// - _EM_INEXACT

		floatControlWord = (floatControlWord & ~_MCW_EM) | (_EM_DENORMAL | _EM_INEXACT | EM_UNDERFLOW | _EM_OVERFLOW);
		floatMxCsr = (floatMxCsr & ~_MM_MASK_MASK) | (_MM_MASK_DENORM | _MM_MASK_INEXACT | _MM_MASK_UNDERFLOW | _MM_MASK_OVERFLOW);
	}

	if (eFPESeverity == eFPE_All)
	{
		// Enable:
		// - _EM_ZERODIVIDE
		// - _EM_INVALID
		// - _EM_UNDERFLOW
		// - _EM_OVERFLOW
		//
		// Disable:
		// - _EM_INEXACT
		// - _EM_DENORMAL

		floatControlWord = (floatControlWord & ~_MCW_EM) | (_EM_INEXACT | _EM_DENORMAL);
		floatMxCsr = (floatMxCsr & ~_MM_MASK_MASK) | (_MM_MASK_INEXACT | _MM_MASK_DENORM);
	}
#endif

	ctx.ContextFlags = CONTEXT_ALL;
	if (SetThreadContext(hThread, &ctx) == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Error setting ThreadContext for ThreadID: %u", nThreadId);
		ResumeThread(hThread);
		CloseHandle(hThread);
		return;
	}

	ResumeThread(hThread);
	CloseHandle(hThread);
}

//////////////////////////////////////////////////////////////////////////
uint GetFloatingPointExceptionMask()
{
	uint nMask = 0;
	_clearfp();
	_controlfp_s(&nMask, 0, 0);
	return nMask;
}

//////////////////////////////////////////////////////////////////////////
void SetFloatingPointExceptionMask(uint nMask)
{
	uint temp = 0;
	_clearfp();
#if CRY_PLATFORM_32BIT
	const unsigned int kAllowedBits = _MCW_DN | _MCW_EM | _MCW_RC | _MCW_IC | _MCW_PC;
#else
	const unsigned int kAllowedBits = _MCW_DN | _MCW_EM | _MCW_RC;
#endif
	_controlfp_s(&temp, nMask, kAllowedBits);
}
}
