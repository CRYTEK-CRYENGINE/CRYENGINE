// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CPUDetect.cpp : CPU detection.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "System.h"
#include "AutoDetectSpec.h"

#if CRY_PLATFORM_WINDOWS
	#include <intrin.h>
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <sys/resource.h> // setrlimit, getrlimit
#endif

#if CRY_PLATFORM_APPLE
	#include <mach/mach.h>
	#include <mach/mach_init.h>     // mach_thread_self
	#include <mach/thread_policy.h> // Mac OS Thread affinity API
	#include <sys/sysctl.h>
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#ifndef __GNU_SOURCE
		#define __GNU_SOURCE
	#endif
	#include <pthread.h> //already includes sched.h
#endif

/* features */
#define FPU_FLAG    0x0001
#define SERIAL_FLAG 0x40000
#define MMX_FLAG    0x800000
#define ISSE_FLAG   0x2000000

#ifdef __GNUC__
	#define cpuid(op, eax, ebx, ecx, edx) __asm__("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (op) : "cc");
#endif

struct SAutoMaxPriority
{
	SAutoMaxPriority()
	{
		/* get a copy of the current thread and process priorities */
#if CRY_PLATFORM_WINDOWS
		priority_class = GetPriorityClass(GetCurrentProcess());
		thread_priority = GetThreadPriority(GetCurrentThread());
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		nice_priority = getpriority(PRIO_PROCESS, 0);
		success = nice_priority >= 0 &&
		          pthread_getschedparam(pthread_self(), &thread_policy, &thread_sched_param) == 0;
#endif

		/* make this thread the highest possible priority */
#if CRY_PLATFORM_WINDOWS
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		if (success)
		{
			setpriority(PRIO_PROCESS, 0, MAX_NICE_PRIORITY);

			sched_param new_sched_param = thread_sched_param;
			new_sched_param.sched_priority = sched_get_priority_max(thread_policy);
			pthread_setschedparam(pthread_self(), thread_policy, &new_sched_param);
		}
#endif
	}

	~SAutoMaxPriority()
	{
		/* restore the thread priority */
#if CRY_PLATFORM_WINDOWS
		SetPriorityClass(GetCurrentProcess(), priority_class);
		SetThreadPriority(GetCurrentThread(), thread_priority);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		if (success)
		{
			pthread_setschedparam(pthread_self(), thread_policy, &thread_sched_param);
			setpriority(PRIO_PROCESS, 0, nice_priority);
		}
#endif
	}

#if CRY_PLATFORM_WINDOWS
	uint32      priority_class;
	int         thread_priority;
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	rlimit      nice_limit;
	int         nice_priority;
	int         thread_policy;
	sched_param thread_sched_param;
	bool        success;
	enum { MAX_NICE_PRIORITY = 40};
#endif
};

bool HasCPUID()
{
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT)
	_asm
	{
		pushfd                // save EFLAGS
		pop eax               // store EFLAGS in EAX
		mov ebx, eax          // save in EBX for later testing
		xor eax, 00200000h    // toggle bit 21
		push eax              // push to stack
		popfd                 // save changed EAX to EFLAGS
		pushfd                // push EFLAGS to TOS
		pop eax               // store EFLAGS in EAX
		cmp eax, ebx          // see if bit 21 has changed
		mov eax, 0            // clear eax to return "false"
		jz QUIT               // if no change, no CPUID
		mov eax, 1            // set eax to return "true"
QUIT:
	}
#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	// we can safely assume CPUID exists on these 64-bit platforms
	return true;
#else
	return false;
#endif
}

#if CRY_PLATFORM_MAC || CRY_PLATFORM_LINUX || CRY_PLATFORM_ORBIS
static inline void __cpuid(int CPUInfo[4], int InfoType)
{
	asm volatile ("cpuid" : "=a" (*CPUInfo), "=b" (*(CPUInfo + 1)), "=c" (*(CPUInfo + 2)), "=d" (*(CPUInfo + 3)) : "a" (InfoType));
}
#endif // CRY_PLATFORM_MAC

bool IsAMD()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000000);

	char szCPU[13];
	memset(szCPU, 0, sizeof(szCPU));
	*(int*)&szCPU[0] = CPUInfo[1];
	*(int*)&szCPU[4] = CPUInfo[3];
	*(int*)&szCPU[8] = CPUInfo[2];

	return (strcmp(szCPU, "AuthenticAMD") == 0);
#else
	return false;
#endif
}

bool IsIntel()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000000);

	char szCPU[13];
	memset(szCPU, 0, sizeof(szCPU));
	*(int*)&szCPU[0] = CPUInfo[1];
	*(int*)&szCPU[4] = CPUInfo[3];
	*(int*)&szCPU[8] = CPUInfo[2];

	return (strcmp(szCPU, "GenuineIntel") == 0);
#else
	return false;
#endif
}

bool Has64bitExtension()
{
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT) || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x80000001);   // Argument "Processor Signature and AMD Features"
	if (CPUInfo[3] & 0x20000000)    // Bit 29 in edx is set if 64-bit address extension is supported
		return true;
	else
		return false;
#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_MAC
	return true;
#else
	return false;
#endif
}

bool HTSupported()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	if (CPUInfo[3] & 0x10000000)    // Bit 28 in edx is set if HT is supported
		return true;
	else
		return false;
#else
	return false;
#endif
}

/*
   //////////////////////////////////////////////////////////////////////////
   bool IsMultiCoreCPU()
   {
   #if CRY_PLATFORM_WINDOWS
   int CPUInfo[4];
   __cpuid( CPUInfo, 0x00000000 );
   if( CPUInfo[0] != 0)		// Bit 28 in edx is set if HT is supported
    return true;
   else
    return false;
   #else
   return false;
   #endif
   }

   //////////////////////////////////////////////////////////////////////////
   int GetCoresPerCPU()
   {
   #if CRY_PLATFORM_WINDOWS
   int CPUInfo[4];
   __cpuid( CPUInfo, 0x00000004 );
   return CPUInfo[0] + 1;
   #else
   return 1;
   #endif
   }
 */

uint8 LogicalProcPerPhysicalProc()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	// Bits 16-23 in ebx contain the number of logical processors per physical processor when execute cpuid with eax set to 1
	return (uint8) ((CPUInfo[1] & 0x00FF0000) >> 16);
#else
	return 1;
#endif
}

uint8 GetAPIC_ID()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	// Bits 24-31 in ebx contain the unique initial APIC ID for the processor this code is running on. Default value = 0xff if HT is not supported.
	return (uint8) ((CPUInfo[1] & 0xFF000000) >> 24);
#else
	return 0;
#endif
}

void GetCPUName(char* pName)
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	if (pName)
	{
		int CPUInfo[4];
		__cpuid(CPUInfo, 0x80000000);
		if (CPUInfo[0] >= 0x80000004)
		{
			__cpuid(CPUInfo, 0x80000002);
			((int*)pName)[0] = CPUInfo[0];
			((int*)pName)[1] = CPUInfo[1];
			((int*)pName)[2] = CPUInfo[2];
			((int*)pName)[3] = CPUInfo[3];

			__cpuid(CPUInfo, 0x80000003);
			((int*)pName)[4] = CPUInfo[0];
			((int*)pName)[5] = CPUInfo[1];
			((int*)pName)[6] = CPUInfo[2];
			((int*)pName)[7] = CPUInfo[3];

			__cpuid(CPUInfo, 0x80000004);
			((int*)pName)[8] = CPUInfo[0];
			((int*)pName)[9] = CPUInfo[1];
			((int*)pName)[10] = CPUInfo[2];
			((int*)pName)[11] = CPUInfo[3];
		}
		else
			pName[0] = '\0';
	}
#else
	if (pName)
		pName[0] = '\0';
#endif
}

bool HasFPUOnChip()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	// Bit 0 in edx indicates presents of on chip FPU
	return (CPUInfo[3] & 0x00000001) != 0;
#else
	return false;
#endif
}

void GetCPUSteppingModelFamily(int& stepping, int& model, int& family)
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	stepping = CPUInfo[0] & 0xF;      // Bit 0-3 in eax specifies stepping
	model = (CPUInfo[0] >> 4) & 0xF;  // Bit 4-7 in eax specifies model
	family = (CPUInfo[0] >> 8) & 0xF; // Bit 8-11 in eax specifies family
#else
	stepping = 0;
	model = 0;
	family = 0;
#endif
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
unsigned long GetCPUFeatureSet()
{
	unsigned long features = 0;

	int CPUInfo[4];

	__cpuid(CPUInfo, 0);
	const int nIds = CPUInfo[0];

	__cpuid(CPUInfo, 0x80000000);
	const uint nExIds = CPUInfo[0];

	if (nIds > 0)
	{
		__cpuid(CPUInfo, 0x00000001);
		if (CPUInfo[3] & (1 << 25))
			features |= CPUF_SSE;
		if (CPUInfo[3] & (1 << 26))
			features |= CPUF_SSE2;
		if (CPUInfo[2] & (1 << 0))
			features |= CPUF_SSE3;
		if (CPUInfo[2] & (1 << 20))
			features |= CPUF_SSE4;
		if (CPUInfo[2] & (1 << 28))
			features |= CPUF_AVX;
		if (CPUInfo[2] & (1 << 12))
			features |= CPUF_FMA;
		if (CPUInfo[2] & (1 << 29))
			features |= CPUF_FP16;
	}
	if (nIds >= 7)
	{
		__cpuid(CPUInfo, 0x00000007);
		if (CPUInfo[1] & (1 << 5))
			features |= CPUF_AVX2;
	}
	if (nExIds > 0x80000000)
	{
		__cpuid(CPUInfo, 0x80000001);
		if (CPUInfo[3] & (1 << 31))
			features |= CPUF_3DNOW;
	}

	return features;
}
#endif

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
static unsigned long __stdcall DetectProcessor(void* arg)
{
	const char hex_chars[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};
	unsigned long signature = 0;
	unsigned long cache_temp;
	unsigned long cache_eax = 0;
	unsigned long cache_ebx = 0;
	unsigned long cache_ecx = 0;
	unsigned long cache_edx = 0;
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_X86
	unsigned long features_ebx = 0;
	unsigned long features_ecx = 0;
	unsigned char cyrix_flag = 0;
	#endif
	unsigned long features_edx = 0;
	unsigned long serial_number[3];
	unsigned char cpu_type;
	unsigned char fpu_type;
	unsigned char CPUID_flag = 0;
	unsigned char celeron_flag = 0;
	unsigned char pentiumxeon_flag = 0;
	unsigned char amd3d_flag = 0;
	unsigned char name_flag = 0;
	char vendor[13];
	vendor[0] = '\0';
	char name[49];
	name[0] = '\0';
	char* serial;
	const char* cpu_string;
	const char* cpu_extra_string;
	const char* fpu_string;
	const char* vendor_string;
	SCpu* p = (SCpu*) arg;

	memset(p, 0, sizeof(*p));

	if (HasCPUID())
	{
		if (IsAMD() && Has64bitExtension())
		{
			p->meVendor = eCVendor_AMD;
			p->mFeatures |= GetCPUFeatureSet();
			p->mbSerialPresent = false;
			cry_strcpy(p->mSerialNumber, "");
			GetCPUSteppingModelFamily(p->mStepping, p->mModel, p->mFamily);
			cry_strcpy(p->mVendor, "AMD");
			GetCPUName(p->mCpuType);
			cry_strcpy(p->mFpuType, HasFPUOnChip() ? "On-Chip" : "Unknown");
			p->mbPhysical = true;

			return 1;
		}
		else if (IsIntel() && Has64bitExtension())
		{
			p->meVendor = eCVendor_Intel;
			p->mFeatures |= GetCPUFeatureSet();
			p->mbSerialPresent = false;
			cry_strcpy(p->mSerialNumber, "");
			GetCPUSteppingModelFamily(p->mStepping, p->mModel, p->mFamily);
			cry_strcpy(p->mVendor, "Intel");
			GetCPUName(p->mCpuType);
			cry_strcpy(p->mFpuType, HasFPUOnChip() ? "On-Chip" : "Unknown");

			p->mbPhysical = true;

			// Timur this check is somehow broken on Intel Core Duo
			/*
			   // only check on 32 bits
			   if (p->mStepping>4 && HTSupported())
			   p->mbPhysical =  (GetAPIC_ID() & LogicalProcPerPhysicalProc()-1)==0;
			 */

			//if (IsMultiCoreCPU()) // Force to true for multi-core machines.
			{
				//p->mbPhysical = 1;
			}
			return 1;
		}
	}

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_X86
	// *INDENT-OFF* - do not attempt to format inline assembly!
	unsigned char  v86_flag;
	unsigned short fp_status;
	__asm
	{
		/*
		 * 8086 processor check
		 * Bits 12-15 of the FLAGS register are always set on the
		 * 8086 processor.
		 */
		pushf                /* push original FLAGS */
		pop ax               /* get original FLAGS */
		mov cx, ax           /* save original FLAGS */
		and ax, 0fffh        /* clear bits 12-15 in FLAGS */
		push ax              /* save new FLAGS value on stack */
		popf                 /* replace current FLAGS value */
		pushf                /* get new FLAGS */
		pop ax               /* store new FLAGS in AX */
		and ax, 0f000h       /* if bits 12-15 are set, then */
		cmp ax, 0f000h       /* processor is an 8086/8088 */
		mov cpu_type, 0      /* turn on 8086/8088 flag */
		jne check_80286      /* go check for 80286 */
		push sp              /* double check with push sp */
		pop dx               /* if value pushed was different */
		cmp dx, sp           /* means it's really an 8086 */
		jne end_cpu_type     /* jump if processor is 8086/8088 */
		mov cpu_type, 010h   /* indicate unknown processor */
		jmp end_cpu_type

		/*
		 * 286 processor check
		 * Bits 12-15 of the FLAGS register are always clear on the
		 * Intel 286 processor in real-address mode.
		 */
check_80286:
		smsw ax              /* save machine status word */
		and ax, 1            /* isolate PE bit of MSW */
		mov v86_flag, al     /* save PE bit to indicate V86 */
		or cx, 0f000h        /* try to set bits 12-15 */
		push cx              /* save new FLAGS value on stack */
		popf                 /* replace current FLAGS value */
		pushf                /* get new FLAGS */
		pop ax               /* store new FLAGS in AX */
		and ax, 0f000h       /* if bits 12-15 are clear */
		mov cpu_type, 2      /* processor=80286, turn on 80286 flag */
		jz end_cpu_type      /* jump if processor is 80286 */

		/*
		 * 386 processor check
		 * The AC bit, bit #18, is a new bit introduced in the EFLAGS
		 * register on the 486 processor to generate alignment
		 * faults.
		 * This bit cannot be set on the 386 processor.
		 */
		mov cpu_type, 3      /* turn on 80386 processor flag */
		pushfd               /* push original EFLAGS */
		pop eax              /* get original EFLAGS */
		mov ebx, eax         /* save original EFLAGS */
		xor eax, 040000h     /* flip AC bit in EFLAGS */
		push eax             /* save new EFLAGS value on stack */
		popfd                /* replace current EFLAGS value */
		pushfd               /* get new EFLAGS */
		pop eax              /* store new EFLAGS in EAX */
		cmp eax, ebx         /* can't toggle AC bit, processor=80386 */
		jz end_cpu_type      /* jump if 80386 processor */
		push ebx
		popfd                /* restore AC bit in EFLAGS */

		/*
		 * Checking for ability to set/clear ID flag (Bit 21) in EFLAGS
		 * which indicates the presence of a processor with the CPUID
		 * instruction.
		 */
		mov cpu_type, 4      /* turn on 80486 processor flag */
		pushfd               /* save EFLAGS to stack */
		pop eax              /* store EFLAGS in EAX */
		mov ebx, eax         /* save in EBX for testing later */
		xor eax, 0200000h    /* flip bit 21 in EFLAGS */
		push eax             /* save new EFLAGS value on stack */
		popfd                /* replace current EFLAGS value */
		pushfd               /* get new EFLAGS */
		pop eax              /* store new EFLAGS in EAX */
		cmp eax, ebx         /* see if bit 21 has changed */
		jne yes_CPUID        /* CPUID present */
		/* Cyrix processor check */
		xor ax, ax           /* clear ax */
		sahf                 /* clear flags, bit 1 is always 1 in flags */
		mov ax, 5            /* move 5 into the dividend */
		mov bx, 2            /* move 2 into the divisor */
		div bl               /* do an operation that does not change flags */
		lahf                 /* get flags */
		cmp ah, 2            /* check for change in flags */
		jne not_cyrix        /* flags changed, not a Cyrix CPU */
		mov cyrix_flag, 1
		/* TODO: identify cyrix processor */
not_cyrix:
		jmp end_cpu_type
yes_CPUID:
		push ebx
		popfd
		mov CPUID_flag, 1    /* flag indicating use of CPUID instruction */

		/*
		 * Execute CPUID instruction to determine vendor, family,
		 * model, stepping and features.
		 */
		push ebx             /* save registers */
		push esi
		push edi
		xor eax, eax         /* set up for CPUID instruction */
		cpuid                /* get and save vendor ID */
		mov byte ptr[vendor], bl
		mov byte ptr[vendor + 1], bh
		ror ebx, 16
		mov byte ptr[vendor + 2], bl
		mov byte ptr[vendor + 3], bh
		mov byte ptr[vendor + 4], dl
		mov byte ptr[vendor + 5], dh
		ror edx, 16
		mov byte ptr[vendor + 6], dl
		mov byte ptr[vendor + 7], dh
		mov byte ptr[vendor + 8], cl
		mov byte ptr[vendor + 9], ch
		ror ecx, 16
		mov byte ptr[vendor + 10], cl
		mov byte ptr[vendor + 11], ch
		mov byte ptr[vendor + 12], 0
		cmp eax, 1           /* make sure 1 is valid input for CPUID */
		jl end_CPUID_type    /* if not, jump to end */
		mov eax, 1
		cpuid                /* get family/model/stepping/features */
		mov signature, eax
		mov features_ebx, ebx
		mov features_ecx, ecx
		mov features_edx, edx
		shr eax, 8           /* isolate family */
		and eax, 0fh
		mov cpu_type, al     /* set cpu_type with family */

		/*
		 * Execute CPUID instruction to determine the cache descriptor
		 * information.
		 */
		xor eax, eax         /* set up to check the EAX value */
		cpuid
		cmp ax, 2            /* are cache descriptors supported? */
		jl end_CPUID_type
		mov eax, 2           /* set up to read cache descriptor */
		cpuid
		cmp al, 1            /* is one iteration enough to obtain */
		jne end_CPUID_type   /* cache information? */

		/* this code supports one iteration only. */
		mov cache_eax, eax   /* store cache information */
		mov cache_ebx, ebx   /* NOTE: for future processors, CPUID */
		mov cache_ecx, ecx   /* instruction may need to be run more */
		mov cache_edx, edx   /* than once to get complete cache information */
end_CPUID_type:
		pop edi              /* restore registers */
		pop esi
		pop ebx

		/* check for 3DNow! */
		mov eax, 080000000h  /* query for extended functions */
		cpuid                /* get extended function limit */
		cmp eax, 080000001h  /* functions up to 80000001h must be present */
		jb no_extended       /* 80000001h is not available */
		mov eax, 080000001h  /* setup extended function 1 */
		cpuid                /* call the function */
		test edx, 080000000h /* test bit 31 */
		jz end_3dnow
		mov amd3d_flag, 1    /* 3DNow! supported */
end_3dnow:

		/* get CPU name */
		mov eax, 080000000h
		cpuid
		cmp eax, 080000004h
		jb end_name          /* functions up to 80000004h must be present */
		mov name_flag, 1
		mov eax, 080000002h
		cpuid
		mov dword ptr[name], eax
		mov dword ptr[name + 4], ebx
		mov dword ptr[name + 8], ecx
		mov dword ptr[name + 12], edx
		mov eax, 080000003h
		cpuid
		mov dword ptr[name + 16], eax
		mov dword ptr[name + 20], ebx
		mov dword ptr[name + 24], ecx
		mov dword ptr[name + 28], edx
		mov eax, 080000004h
		cpuid
		mov dword ptr[name + 32], eax
		mov dword ptr[name + 36], ebx
		mov dword ptr[name + 40], ecx
		mov dword ptr[name + 44], edx
end_name:

no_extended:

end_cpu_type:

		/* detect FPU */
		fninit                      /* reset FP status word */
		mov fp_status, 05a5ah       /* initialize temp word to non-zero */
		fnstsw fp_status            /* save FP status word */
		mov ax, fp_status           /* check FP status word */
		cmp al, 0                   /* was correct status written */
		mov fpu_type, 0             /* no FPU present */
		jne end_fpu_type
		/* check control word */
		fnstcw fp_status            /* save FP control word */
		mov ax, fp_status           /* check FP control word */
		and ax, 103fh               /* selected parts to examine */
		cmp ax, 3fh                 /* was control word correct */
		mov fpu_type, 0
		jne end_fpu_type            /* incorrect control word, no FPU */
		mov fpu_type, 1
		/* 80287/80387 check for the Intel386 processor */
		cmp cpu_type, 3
		jne end_fpu_type
		fld1                        /* must use default control from FNINIT */
		fldz                        /* form infinity */
		fdiv                        /* 8087/Intel287 NDP say +inf = -inf */
		fld st                      /* form negative infinity */
		fchs                        /* Intel387 NDP says +inf <> -inf */
		fcompp                      /* see if they are the same */
		fstsw fp_status             /* look at status from FCOMPP */
		mov ax, fp_status
		mov fpu_type, 2             /* store Intel287 NDP for FPU type */
		sahf                        /* see if infinities matched */
		jz end_fpu_type             /* jump if 8087 or Intel287 is present */
		mov fpu_type, 3             /* store Intel387 NDP for FPU type */
end_fpu_type:
	}
	// *INDENT-ON* - do not attempt to format inline assembly!
	#else // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_X86
	cpu_type = 0xF;
	fpu_type = 3;
	signature = 0;
	#endif // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_X86
	p->mFamily = cpu_type;
	p->mModel = (signature >> 4) & 0xf;
	p->mStepping = signature & 0xf;

	p->mFeatures = 0;

	p->mFeatures |= amd3d_flag ? CPUF_3DNOW : 0;
	p->mFeatures |= (features_edx & MMX_FLAG) ? CPUF_MMX : 0;
	p->mFeatures |= (features_edx & ISSE_FLAG) ? CPUF_SSE : 0;
	p->mbSerialPresent = ((features_edx & SERIAL_FLAG) != 0);

	if (features_edx & SERIAL_FLAG)
	{
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_X86
		/* read serial number */
		__asm
		{
			mov eax, 1
			cpuid
			mov serial_number[2], eax /* top 32 bits are the processor signature bits */
			mov eax, 3
			cpuid
			mov serial_number[1], edx
			mov serial_number[0], ecx
		}
	#else
		serial_number[0] = serial_number[1] = serial_number[2] = 0;
	#endif

		/* format number */
		serial = p->mSerialNumber;

		serial[0] = hex_chars[(serial_number[2] >> 28) & 0x0f];
		serial[1] = hex_chars[(serial_number[2] >> 24) & 0x0f];
		serial[2] = hex_chars[(serial_number[2] >> 20) & 0x0f];
		serial[3] = hex_chars[(serial_number[2] >> 16) & 0x0f];

		serial[4] = '-';

		serial[5] = hex_chars[(serial_number[2] >> 12) & 0x0f];
		serial[6] = hex_chars[(serial_number[2] >> 8) & 0x0f];
		serial[7] = hex_chars[(serial_number[2] >> 4) & 0x0f];
		serial[8] = hex_chars[(serial_number[2] >> 0) & 0x0f];

		serial[9] = '-';

		serial[10] = hex_chars[(serial_number[1] >> 28) & 0x0f];
		serial[11] = hex_chars[(serial_number[1] >> 24) & 0x0f];
		serial[12] = hex_chars[(serial_number[1] >> 20) & 0x0f];
		serial[13] = hex_chars[(serial_number[1] >> 16) & 0x0f];

		serial[14] = '-';

		serial[15] = hex_chars[(serial_number[1] >> 12) & 0x0f];
		serial[16] = hex_chars[(serial_number[1] >> 8) & 0x0f];
		serial[17] = hex_chars[(serial_number[1] >> 4) & 0x0f];
		serial[18] = hex_chars[(serial_number[1] >> 0) & 0x0f];

		serial[19] = '-';

		serial[20] = hex_chars[(serial_number[0] >> 28) & 0x0f];
		serial[21] = hex_chars[(serial_number[0] >> 24) & 0x0f];
		serial[22] = hex_chars[(serial_number[0] >> 20) & 0x0f];
		serial[23] = hex_chars[(serial_number[0] >> 16) & 0x0f];

		serial[24] = '-';

		serial[25] = hex_chars[(serial_number[0] >> 12) & 0x0f];
		serial[26] = hex_chars[(serial_number[0] >> 8) & 0x0f];
		serial[27] = hex_chars[(serial_number[0] >> 4) & 0x0f];
		serial[28] = hex_chars[(serial_number[0] >> 0) & 0x0f];

		serial[29] = 0;
	}

	vendor_string = "Unknown";
	cpu_string = "Unknown";
	cpu_extra_string = "";
	fpu_string = "Unknown";

	if (!CPUID_flag)
	{
		switch (cpu_type)
		{
		case 0:
			cpu_string = "8086";
			break;

		case 2:
			cpu_string = "80286";
			break;

		case 3:
			cpu_string = "80386";
			switch (fpu_type)
			{
			case 2:
				fpu_string = "80287";
				break;

			case 1:
				fpu_string = "80387";
				break;

			default:
				fpu_string = "None";
				break;
			}
			break;

		case 4:
			if (fpu_type)
			{
				cpu_string = "80486DX, 80486DX2 or 80487SX";
				fpu_string = "on-chip";
			}
			else
				cpu_string = "80486SX";
			break;
		}
	}
	else
	{
		/* using CPUID instruction */
		if (!name_flag)
		{
			if (!strcmp(vendor, "GenuineIntel"))
			{
				vendor_string = "Intel";
				switch (cpu_type)
				{
				case 4:
					switch (p->mModel)
					{
					case 0:
					case 1:
						cpu_string = "80486DX";
						break;

					case 2:
						cpu_string = "80486SX";
						break;

					case 3:
						cpu_string = "80486DX2";
						break;

					case 4:
						cpu_string = "80486SL";
						break;

					case 5:
						cpu_string = "80486SX2";
						break;

					case 7:
						cpu_string = "Write-Back Enhanced 80486DX2";
						break;

					case 8:
						cpu_string = "80486DX4";
						break;

					default:
						cpu_string = "80486";
					}
					break;

				case 5:
					switch (p->mModel)
					{
					default:
					case 1:
					case 2:
					case 3:
						cpu_string = "Pentium";
						break;

					case 4:
						cpu_string = "Pentium MMX";
						break;
					}
					break;

				case 6:
					switch (p->mModel)
					{
					case 1:
						cpu_string = "Pentium Pro";
						break;

					case 3:
						cpu_string = "Pentium II";
						break;

					case 5:
					case 7:
						{
							cache_temp = cache_eax & 0xFF000000;
							if (cache_temp == 0x40000000)
								celeron_flag = 1;
							if ((cache_temp >= 0x44000000) && (cache_temp <= 0x45000000))
								pentiumxeon_flag = 1;
							cache_temp = cache_eax & 0xFF0000;
							if (cache_temp == 0x400000)
								celeron_flag = 1;
							if ((cache_temp >= 0x440000) && (cache_temp <= 0x450000))
								pentiumxeon_flag = 1;
							cache_temp = cache_eax & 0xFF00;
							if (cache_temp == 0x4000)
								celeron_flag = 1;
							if ((cache_temp >= 0x4400) && (cache_temp <= 0x4500))
								pentiumxeon_flag = 1;
							cache_temp = cache_ebx & 0xFF000000;
							if (cache_temp == 0x40000000)
								celeron_flag = 1;
							if ((cache_temp >= 0x44000000) && (cache_temp <= 0x45000000))
								pentiumxeon_flag = 1;
							cache_temp = cache_ebx & 0xFF0000;
							if (cache_temp == 0x400000)
								celeron_flag = 1;
							if ((cache_temp >= 0x440000) && (cache_temp <= 0x450000))
								pentiumxeon_flag = 1;
							cache_temp = cache_ebx & 0xFF00;
							if (cache_temp == 0x4000)
								celeron_flag = 1;
							if ((cache_temp >= 0x4400) && (cache_temp <= 0x4500))
								pentiumxeon_flag = 1;
							cache_temp = cache_ebx & 0xFF;
							if (cache_temp == 0x40)
								celeron_flag = 1;
							if ((cache_temp >= 0x44) && (cache_temp <= 0x45))
								pentiumxeon_flag = 1;
							cache_temp = cache_ecx & 0xFF000000;
							if (cache_temp == 0x40000000)
								celeron_flag = 1;
							if ((cache_temp >= 0x44000000) && (cache_temp <= 0x45000000))
								pentiumxeon_flag = 1;
							cache_temp = cache_ecx & 0xFF0000;
							if (cache_temp == 0x400000)
								celeron_flag = 1;
							if ((cache_temp >= 0x440000) && (cache_temp <= 0x450000))
								pentiumxeon_flag = 1;
							cache_temp = cache_ecx & 0xFF00;
							if (cache_temp == 0x4000)
								celeron_flag = 1;
							if ((cache_temp >= 0x4400) && (cache_temp <= 0x4500))
								pentiumxeon_flag = 1;
							cache_temp = cache_ecx & 0xFF;
							if (cache_temp == 0x40)
								celeron_flag = 1;
							if ((cache_temp >= 0x44) && (cache_temp <= 0x45))
								pentiumxeon_flag = 1;
							cache_temp = cache_edx & 0xFF000000;
							if (cache_temp == 0x40000000)
								celeron_flag = 1;
							if ((cache_temp >= 0x44000000) && (cache_temp <= 0x45000000))
								pentiumxeon_flag = 1;
							cache_temp = cache_edx & 0xFF0000;
							if (cache_temp == 0x400000)
								celeron_flag = 1;
							if ((cache_temp >= 0x440000) && (cache_temp <= 0x450000))
								pentiumxeon_flag = 1;
							cache_temp = cache_edx & 0xFF00;
							if (cache_temp == 0x4000)
								celeron_flag = 1;
							if ((cache_temp >= 0x4400) && (cache_temp <= 0x4500))
								pentiumxeon_flag = 1;
							cache_temp = cache_edx & 0xFF;
							if (cache_temp == 0x40)
								celeron_flag = 1;
							if ((cache_temp >= 0x44) && (cache_temp <= 0x45))
								pentiumxeon_flag = 1;

							if (celeron_flag)
							{
								cpu_string = "Celeron";
							}
							else
							{
								if (pentiumxeon_flag)
								{
									if (p->mModel == 5)
									{
										cpu_string = "Pentium II Xeon";
									}
									else
									{
										cpu_string = "Pentium III Xeon";
									}
								}
								else
								{
									if (p->mModel == 5)
									{
										cpu_string = "Pentium II";
									}
									else
									{
										cpu_string = "Pentium III";
									}
								}
							}
						}
						break;

					case 6:
						cpu_string = "Celeron";
						break;

					case 8:
						cpu_string = "Pentium III";
						break;
					}
					break;

				}

				if (signature & 0x1000)
				{
					cpu_extra_string = " OverDrive";
				}
				else if (signature & 0x2000)
				{
					cpu_extra_string = " dual upgrade";
				}
			}
			else if (!strcmp(vendor, "CyrixInstead"))
			{
				vendor_string = "Cyrix";
				switch (p->mFamily)
				{
				case 4:
					switch (p->mModel)
					{
					case 4:
						cpu_string = "MediaGX";
						break;
					}
					break;

				case 5:
					switch (p->mModel)
					{
					case 2:
						cpu_string = "6x86";
						break;

					case 4:
						cpu_string = "GXm";
						break;
					}
					break;

				case 6:
					switch (p->mModel)
					{
					case 0:
						cpu_string = "6x86MX";
						break;
					}
					break;
				}
			}
			else if (!strcmp(vendor, "AuthenticAMD"))
			{
				cry_strcpy(p->mVendor, "AMD");
				switch (p->mFamily)
				{
				case 4:
					cpu_string = "Am486 or Am5x86";
					break;

				case 5:
					switch (p->mModel)
					{
					case 0:
					case 1:
					case 2:
					case 3:
						cpu_string = "K5";
						break;

					case 4:
					case 5:
					case 6:
					case 7:
						cpu_string = "K6";
						break;

					case 8:
						cpu_string = "K6-2";
						break;

					case 9:
						cpu_string = "K6-III";
						break;
					}
					break;

				case 6:
					cpu_string = "Athlon";
					break;
				}
			}
			else if (!strcmp(vendor, "CentaurHauls"))
			{
				vendor_string = "Centaur";
				switch (cpu_type)
				{
				case 5:
					switch (p->mModel)
					{
					case 4:
						cpu_string = "WinChip";
						break;

					case 8:
						cpu_string = "WinChip2";
						break;
					}
					break;
				}
			}
			else if (!strcmp(vendor, "UMC UMC UMC "))
			{
				vendor_string = "UMC";
			}
			else if (!strcmp(vendor, "NexGenDriven"))
			{
				vendor_string = "NexGen";
			}
		}
		else
		{
			vendor_string = vendor;
			cpu_string = name;
		}

		if (features_edx & FPU_FLAG)
		{
			fpu_string = "On-Chip";
		}
		else
		{
			fpu_string = "Unknown";
		}
	}

	stack_string sCpuType = stack_string(cpu_string) + cpu_extra_string;

	cry_strcpy(p->mCpuType, sCpuType.c_str());
	cry_strcpy(p->mFpuType, fpu_string);
	cry_strcpy(p->mVendor, vendor_string);

	if (!stricmp(vendor_string, "Intel"))
		p->meVendor = eCVendor_Intel;
	else if (!stricmp(vendor_string, "Cyrix"))
		p->meVendor = eCVendor_Cyrix;
	else if (!stricmp(vendor_string, "AMD"))
		p->meVendor = eCVendor_AMD;
	else if (!stricmp(vendor_string, "Centaur"))
		p->meVendor = eCVendor_Centaur;
	else if (!stricmp(vendor_string, "NexGen"))
		p->meVendor = eCVendor_NexGen;
	else if (!stricmp(vendor_string, "UMC"))
		p->meVendor = eCVendor_UMC;
	else
		p->meVendor = eCVendor_Unknown;

	if (strstr(cpu_string, "8086"))
		p->meModel = eCpu_8086;
	else if (strstr(cpu_string, "80286"))
		p->meModel = eCpu_80286;
	else if (strstr(cpu_string, "80386"))
		p->meModel = eCpu_80386;
	else if (strstr(cpu_string, "80486"))
		p->meModel = eCpu_80486;
	else if (!stricmp(cpu_string, "Pentium MMX") || !stricmp(cpu_string, "Pentium"))
		p->meModel = eCpu_Pentium;
	else if (!stricmp(cpu_string, "Pentium Pro"))
		p->meModel = eCpu_PentiumPro;
	else if (!stricmp(cpu_string, "Pentium II"))
		p->meModel = eCpu_Pentium2;
	else if (!stricmp(cpu_string, "Pentium III"))
		p->meModel = eCpu_Pentium3;
	else if (!stricmp(cpu_string, "Pentium 4"))
		p->meModel = eCpu_Pentium4;
	else if (!stricmp(cpu_string, "Celeron"))
		p->meModel = eCpu_Celeron;
	else if (!stricmp(cpu_string, "Pentium II Xeon"))
		p->meModel = eCpu_Pentium2Xeon;
	else if (!stricmp(cpu_string, "Pentium III Xeon"))
		p->meModel = eCpu_Pentium3Xeon;
	else if (!stricmp(cpu_string, "MediaGX"))
		p->meModel = eCpu_CyrixMediaGX;
	else if (!stricmp(cpu_string, "6x86"))
		p->meModel = eCpu_Cyrix6x86;
	else if (!stricmp(cpu_string, "GXm"))
		p->meModel = eCpu_CyrixGXm;
	else if (!stricmp(cpu_string, "6x86MX"))
		p->meModel = eCpu_Cyrix6x86MX;
	else if (!stricmp(cpu_string, "Am486 or Am5x86"))
		p->meModel = eCpu_Am5x86;
	else if (!stricmp(cpu_string, "K5"))
		p->meModel = eCpu_AmK5;
	else if (!stricmp(cpu_string, "K6"))
		p->meModel = eCpu_AmK6;
	else if (!stricmp(cpu_string, "K6-2"))
		p->meModel = eCpu_AmK6_2;
	else if (!stricmp(cpu_string, "K6-III"))
		p->meModel = eCpu_AmK6_3;
	else if (!stricmp(cpu_string, "Athlon"))
		p->meModel = eCpu_AmAthlon;
	else if (!stricmp(cpu_string, "Duron"))
		p->meModel = eCpu_AmDuron;
	else if (!stricmp(cpu_string, "WinChip"))
		p->meModel = eCpu_CenWinChip;
	else if (!stricmp(cpu_string, "WinChip2"))
		p->meModel = eCpu_CenWinChip2;
	else
		p->meModel = eCpu_Unknown;

	p->mbPhysical = true;
	if (!strcmp(vendor_string, "GenuineIntel") && p->mStepping > 4 && HTSupported())
		p->mbPhysical = (GetAPIC_ID() & LogicalProcPerPhysicalProc() - 1) == 0;

	return 1;
}
#endif // CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS

#if CRY_PLATFORM_MAC || CRY_PLATFORM_LINUX
static void* DetectProcessorThreadProc(void* pData)
{
	DetectProcessor(pData);
	return NULL;
}
#endif

/*
   #if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
   typedef BOOL (WINAPI *LPFN_GLPI)( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

   int GetPhysicalCpuCount()
   {
   BOOL done;
   BOOL rc;
   DWORD returnLength;
   DWORD procCoreCount;
   DWORD byteOffset;
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer;
   LPFN_GLPI Glpi;

   Glpi = (LPFN_GLPI) GetProcAddress( GetModuleHandle(TEXT("kernel32")),"GetLogicalProcessorInformation");
   if (NULL == Glpi)
   {
    return (1);
   }

   done = FALSE;
   buffer = NULL;
   returnLength = 0;

   while (!done)
   {
    rc = Glpi(buffer, &returnLength);

    if (FALSE == rc)
    {
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
      {
        if (buffer)
          free(buffer);

        buffer=(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

        if (NULL == buffer)
        {
          return (1);
        }
      }
      else
      {
        return (1);
      }
    }
    else done = TRUE;
   }

   procCoreCount = 0;
   byteOffset = 0;

   while (byteOffset < returnLength)
   {
    switch (buffer->Relationship)
    {
    case RelationProcessorCore:
      procCoreCount++;
      break;

    default:
      break;
    }
    byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    buffer++;
   }

   free (buffer);

   return procCoreCount;
   }
   #else // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
   int GetPhysicalCpuCount()
   {
   return 1;
   }
   #endif // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
 */

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
// collection of functions to read from /proc/cpuinfo

static bool proc_read_str(char* buffer, char* output, size_t output_length)
{
	if (!buffer || !output || output_length <= 0)
	{
		return false;
	}
	while (*buffer && *buffer != ':')
	{
		++buffer;
	}
	if (*buffer == ':')
	{
		buffer += 2;
		cry_strcpy(output, output_length, buffer);
		const int len = strlen(output);
		if (len > 0 && output[len - 1] == '\n')
		{
			output[len - 1] = '\0';
		}
		return true;
	}
	return false;
}

static bool proc_read_int(char* buffer, int& output)
{
	if (!buffer)
	{
		return false;
	}
	while (*buffer && *buffer != ':')
	{
		++buffer;
	}
	if (*buffer == ':')
	{
		buffer += 2;
		output = atoi(buffer);
		return true;
	}
	return false;
}
#endif

/* ------------------------------------------------------------------------------ */
void CCpuFeatures::Detect(void)
{
	m_NumSystemProcessors = 1;
	m_NumAvailProcessors = 0;

	//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
	CryLogAlways("");

	DWORD_PTR process_affinity_mask;
	uint32 thread_processor_mask = 1;
	process_affinity_mask = 1;

	/* get the system info to derive the number of processors within the system. */

	SYSTEM_INFO sys_info;
	DWORD_PTR system_affinity_mask;
	GetSystemInfo(&sys_info);
	m_NumLogicalProcessors = m_NumSystemProcessors = sys_info.dwNumberOfProcessors;
	m_NumAvailProcessors = 0;
	GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask, &system_affinity_mask);

	for (unsigned char c = 0; c < m_NumSystemProcessors; c++)
	{
		if (process_affinity_mask & ((DWORD_PTR)1 << c))
		{
			m_NumAvailProcessors++;
			SetProcessAffinityMask(GetCurrentProcess(), static_cast<DWORD_PTR>(1) << c);
			DetectProcessor(&m_Cpu[c]);
			m_Cpu[c].mAffinityMask = ((DWORD_PTR)1 << c);
		}
	}

	SetProcessAffinityMask(GetCurrentProcess(), process_affinity_mask);

	m_bOS_ISSE = false;
	m_bOS_ISSE_EXCEPTIONS = false;
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	/*    m_NumLogicalProcessors = m_NumSystemProcessors = sysconf(_SC_NPROCESSORS_CONF);
	    m_NumAvailProcessors = sysconf(_SC_NPROCESSORS_ONLN);

	   //since gdb does not like multiple thread with custom affinities, this only in release
	   #if defined(_RELEASE)
	   pthread_attr_t attr;
	   pthread_t threads[MAX_CPU];
	   cpu_set_t cpuset;
	   CPU_ZERO(&cpuset);

	   for( unsigned char c = 0; c < m_NumAvailProcessors; c++ )
	   {
	     pthread_attr_init(&attr);
	     CPU_ZERO(&cpuset);
	     CPU_SET(c+1,&cpuset);
	     pthread_attr_setaffinity_np(&attr,sizeof(cpuset),&cpuset);
	     pthread_create(&threads[c],&attr,&DetectProcessorThreadProc,(void *)&m_Cpu[c]);
	   }
	   for( unsigned char c = 0; c < m_NumSystemProcessors; c++ )
	   {
	     pthread_join(threads[c], NULL);
	   }
	   #endif
	 */

	// Retrieve information from /proc/cpuinfo
	FILE* cpu_info = fopen("/proc/cpuinfo", "r");
	if (!cpu_info)
	{
		m_NumLogicalProcessors = m_NumSystemProcessors = m_NumAvailProcessors = 1;
		CryLogAlways("Could not open /proc/cpuinfo, defaulting values to 1.");
	}
	else
	{
		int nCores = 0;
		int nCpu = -1;
		int index = 0;
		char buffer[512];
		while (!feof(cpu_info))
		{
			fgets(buffer, sizeof(buffer), cpu_info);

			if (buffer[0] == '\0' || buffer[0] == '\n') continue;

			if (strncmp("processor", buffer, (index = strlen("processor"))) == 0)
			{
				++nCpu;
			}
			else if (strncmp("vendor_id", buffer, (index = strlen("vendor_id"))) == 0)
			{
				proc_read_str(&buffer[index], m_Cpu[nCpu].mVendor, sizeof(m_Cpu[nCpu].mVendor));
			}
			else if (strncmp("model name", buffer, (index = strlen("model name"))) == 0)
			{
				proc_read_str(&buffer[index], m_Cpu[nCpu].mCpuType, sizeof(m_Cpu[nCpu].mCpuType));
			}
			else if (strncmp("cpu cores", buffer, (index = strlen("cpu cores"))) == 0 && nCores == 0)
			{
				proc_read_int(&buffer[index], nCores);
			}
			else if (strncmp("fpu", buffer, (index = strlen("fpu"))) == 0)
			{
				while (buffer[index] != ':' && index < 512)
				{
					++index;
				}
				if (buffer[index] == ':')
				{
					if (strncmp(&buffer[index + 2], "yes", 3) == 0)
					{
						cry_strcpy(m_Cpu[nCpu].mFpuType, "On-Chip");
					}
					else
					{
						cry_strcpy(m_Cpu[nCpu].mFpuType, "Unkown");
					}
				}
			}
			else if (strncmp("cpu family", buffer, (index = strlen("cpu family"))) == 0)
			{
				proc_read_int(&buffer[index], m_Cpu[nCpu].mFamily);
			}
			else if (strncmp("model", buffer, (index = strlen("model"))) == 0)
			{
				proc_read_int(&buffer[index], m_Cpu[nCpu].mModel);
			}
			else if (strncmp("stepping", buffer, (index = strlen("stepping"))) == 0)
			{
				proc_read_int(&buffer[index], m_Cpu[nCpu].mStepping);
			}
			else if (strncmp("flags", buffer, (index = strlen("flags"))) == 0)
			{
				if (strstr(buffer + index, "mmx"))
				{
					m_Cpu[nCpu].mFeatures |= CPUF_MMX;
				}

				if (strstr(buffer + index, "sse"))
				{
					m_Cpu[nCpu].mFeatures |= CPUF_SSE;
				}

				if (strstr(buffer + index, "sse2"))
				{
					m_Cpu[nCpu].mFeatures |= CPUF_SSE2;
				}
			}
		}
		m_NumLogicalProcessors = m_NumAvailProcessors = nCpu + 1;
		m_NumSystemProcessors = nCores;

		fclose(cpu_info);
	}

#elif CRY_PLATFORM_APPLE
	size_t len;
	uint ncpu;

	len = sizeof(ncpu);
	if (sysctlbyname("hw.physicalcpu_max", &ncpu, &len, NULL, 0) == 0)
	{
		m_NumSystemProcessors = ncpu;
	}
	else
	{
		CryLogAlways("Failed to detect the number of available processors, defaulting to 1");
		m_NumSystemProcessors = 1;
	}

	if (sysctlbyname("hw.logicalcpu_max", &ncpu, &len, NULL, 0) == 0)
	{
		m_NumAvailProcessors = m_NumLogicalProcessors = ncpu;
	}
	else
	{
		CryLogAlways("Failed to detect the number of available logical processors, defaulting to 1");
		m_NumAvailProcessors = m_NumLogicalProcessors = 1;
	}
	uint64_t cpu_freq;
	len = sizeof(cpu_freq);
	if (sysctlbyname("hw.cpufrequency_max", &cpu_freq, &len, NULL, 0) != 0)
	{
		CryLogAlways("Failed to detect cpu frequency , defaulting to 0");
		cpu_freq = 0;
	}
	// Mac OS does not support specifying the processor where a process/thread will execute
	// The only way to execute on all N processors is through Thread Affinity API to create N threads with N different affinity tags
	/*pthread_t akThreads[MAX_CPU];
	   for( unsigned char c = 0; c < m_NumSystemProcessors; c++ )
	   {
	   pthread_create_suspended_np(&akThreads[c], NULL, &DetectProcessorThreadProc, &m_Cpu[c]);

	   thread_affinity_policy_data_t kPolicy;
	   kPolicy.affinity_tag = c + 1;	// Mask has to be non-zero or it will use the default affinity group
	   thread_policy_set(
	            pthread_mach_thread_np(akThreads[c]),
	            THREAD_AFFINITY_POLICY,
	            (thread_policy_t) &kPolicy,
	            THREAD_AFFINITY_POLICY_COUNT);
	   thread_resume(pthread_mach_thread_np(akThreads[c]));
	   }

	   for( unsigned char c = 0; c < m_NumSystemProcessors; c++ )
	   {
	   pthread_join(akThreads[c], NULL);
	   }*/
	// On macs, the processors are always the same model, so we can easily
	// calculate once and apply the settings for all.
	SCpu cpuInfo;
	#if !CRY_PLATFORM_IOS
	DetectProcessor(&cpuInfo);
	#endif
	for (int c = 0; c < m_NumAvailProcessors; c++)
	{
		m_Cpu[c] = cpuInfo;
	}

#elif CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	m_NumLogicalProcessors = m_NumSystemProcessors = m_NumPhysicsProcessors = m_NumAvailProcessors = 6;

	SCpu cpuInfo;
	DetectProcessor(&cpuInfo);

	// All cores are the same on bulldozer
	for (unsigned char c = 0; c < m_NumSystemProcessors; ++c)
	{
		m_Cpu[c] = cpuInfo;
	}
#endif

#if CRY_PLATFORM_WINDOWS
	CryLogAlways("Total number of logical processors: %d", m_NumSystemProcessors);
	CryLogAlways("Number of available logical processors: %d", m_NumAvailProcessors);

	uint numSysCores(1), numProcessCores(1);
	Win32SysInspect::GetNumCPUCores(numSysCores, numProcessCores);
	m_NumSystemProcessors = numSysCores;
	m_NumAvailProcessors = numProcessCores;
	CryLogAlways("Total number of system cores: %d", m_NumSystemProcessors);
	CryLogAlways("Number of cores available to process: %d", m_NumAvailProcessors);

#else
	CryLogAlways("Number of system processors: %d", m_NumSystemProcessors);
	CryLogAlways("Number of available processors: %d", m_NumAvailProcessors);
#endif

	if (m_NumAvailProcessors > MAX_CPU)
		m_NumAvailProcessors = MAX_CPU;

	for (int i = 0; i < m_NumAvailProcessors; i++)
	{
		SCpu* p = &m_Cpu[i];
		if (i == 0)
			m_nFeatures = p->mFeatures;
		else
			m_nFeatures &= p->mFeatures;

		CryLogAlways(" ");
		CryLogAlways("Processor %d:", i);
		CryLogAlways("  CPU: %s %s", p->mVendor, p->mCpuType);
		CryLogAlways("  Family: %d, Model: %d, Stepping: %d, FPU: %s", p->mFamily, p->mModel, p->mStepping, p->mFpuType);
		string sFeatures;
		if (p->mFeatures & CPUF_FP16) sFeatures += "FP16, ";
		if (p->mFeatures & CPUF_MMX) sFeatures += "MMX, ";
		if (p->mFeatures & CPUF_3DNOW) sFeatures += "3DNow, ";
		if (p->mFeatures & CPUF_SSE) sFeatures += "SSE, ";
		if (p->mFeatures & CPUF_SSE2) sFeatures += "SSE2, ";
		if (p->mFeatures & CPUF_SSE3) sFeatures += "SSE3, ";
		if (p->mFeatures & CPUF_SSE4) sFeatures += "SSE4.2, ";
		if (p->mFeatures & CPUF_AVX) sFeatures += "AVX, ";
		if (p->mFeatures & CPUF_AVX2) sFeatures += "AVX2, ";
		if (p->mFeatures & CPUF_FMA) sFeatures += "FMA3, ";
		if (sFeatures.size())
			sFeatures.resize(sFeatures.size() - 2);
		CryLogAlways("  Features: %s", sFeatures.c_str());
	}

	CryLogAlways(" ");

	//m_NumPhysicsProcessors = m_NumSystemProcessors;
	for (int i = m_NumPhysicsProcessors = 0; i < m_NumAvailProcessors; i++)
		if (m_Cpu[i].mbPhysical)
			++m_NumPhysicsProcessors;
}
