// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <intrin.h>

static bool IsAMD()
{
#if CRY_PLATFORM_WINDOWS
	int CPUInfo[4];
	char refID[] = "AuthenticAMD";
	__cpuid( CPUInfo, 0x00000000 );
	return ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2];
#else
	return false;
#endif
}


static bool IsIntel()
{
#if CRY_PLATFORM_WINDOWS
	int CPUInfo[4];
	char refID[] = "GenuineIntel";
	__cpuid( CPUInfo, 0x00000000 );
	return ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2];
#else
	return false;
#endif
}


static bool IsVistaOrAbove()
{
	typedef BOOL (WINAPI* FP_VerifyVersionInfo) (LPOSVERSIONINFOEX, DWORD, DWORDLONG);
	FP_VerifyVersionInfo pvvi((FP_VerifyVersionInfo) GetProcAddress(GetModuleHandle("kernel32"), "VerifyVersionInfoA"));

	if (pvvi)
	{
		typedef ULONGLONG (WINAPI* FP_VerSetConditionMask) (ULONGLONG, DWORD, BYTE);
		FP_VerSetConditionMask pvscm((FP_VerSetConditionMask) GetProcAddress(GetModuleHandle("kernel32"), "VerSetConditionMask"));
		assert(pvscm);

		OSVERSIONINFOEX osvi;
		memset(&osvi, 0, sizeof(osvi));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		osvi.dwMajorVersion = 6;
		osvi.dwMinorVersion = 0;
		osvi.wServicePackMajor = 0;
		osvi.wServicePackMinor = 0;

		ULONGLONG mask(0);
		mask = pvscm(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
		mask = pvscm(mask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

		if (pvvi(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, mask))
			return true;
	}

	return false;
}


// Preferred solution to determine the number of available CPU cores, works reliably only on WinVista 32/64 and above
// See http://msdn2.microsoft.com/en-us/library/ms686694.aspx for reasons
static void GetNumCPUCoresGlpi(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	typedef BOOL (WINAPI *FP_GetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
	FP_GetLogicalProcessorInformation pglpi((FP_GetLogicalProcessorInformation) GetProcAddress(GetModuleHandle("kernel32"), "GetLogicalProcessorInformation"));
	if (pglpi && IsVistaOrAbove())
	{
		unsigned long bufferSize(0);
		pglpi(0, &bufferSize);

		void* pBuffer(malloc(bufferSize));

		SYSTEM_LOGICAL_PROCESSOR_INFORMATION* pLogProcInfo((SYSTEM_LOGICAL_PROCESSOR_INFORMATION*) pBuffer);
		if (pLogProcInfo && pglpi(pLogProcInfo, &bufferSize))
		{
			DWORD_PTR processAffinity, systemAffinity;
			GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);

			unsigned long numEntries(bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
			for (unsigned long i(0); i < numEntries; ++i)
			{
				switch (pLogProcInfo[i].Relationship)
				{
				case RelationProcessorCore:
					{
						++totAvailToSystem;
						if (pLogProcInfo[i].ProcessorMask & processAffinity)
							++totAvailToProcess;
					}
					break;

				default:
					break;
				}
			}
		}

		free(pBuffer);
	}
}


class CApicExtractor
{
public:
	CApicExtractor(unsigned int logProcsPerPkg = 1, unsigned int coresPerPkg = 1)
	{
		SetPackageTopology(logProcsPerPkg, coresPerPkg );
	}

	unsigned char SmtId(unsigned char apicId) const
	{
		return apicId & m_smtIdMask.mask;
	}

	unsigned char CoreId(unsigned char apicId) const
	{
		return (apicId & m_coreIdMask.mask) >> m_smtIdMask.width;
	}

	unsigned char PackageId(unsigned char apicId) const
	{
		return (apicId & m_pkgIdMask.mask) >> (m_smtIdMask.width + m_coreIdMask.width);
	}

	unsigned char PackageCoreId(unsigned char apicId) const
	{
		return (apicId & (m_pkgIdMask.mask | m_coreIdMask.mask)) >> m_smtIdMask.width;
	}

	unsigned int GetLogProcsPerPkg() const
	{
		return m_logProcsPerPkg;
	}

	unsigned int GetCoresPerPkg() const
	{
		return m_coresPerPkg;
	}

	void SetPackageTopology(unsigned int logProcsPerPkg, unsigned int coresPerPkg)
	{
		m_logProcsPerPkg   = (unsigned char) logProcsPerPkg;
		m_coresPerPkg      = (unsigned char) coresPerPkg;

		m_smtIdMask.width   = GetMaskWidth(m_logProcsPerPkg / m_coresPerPkg);
		m_coreIdMask.width  = GetMaskWidth(m_coresPerPkg);
		m_pkgIdMask.width   = 8 - (m_smtIdMask.width + m_coreIdMask.width);

		m_pkgIdMask.mask    = (unsigned char) (0xFF << (m_smtIdMask.width + m_coreIdMask.width));
		m_coreIdMask.mask   = (unsigned char) ((0xFF << m_smtIdMask.width) ^ m_pkgIdMask.mask);
		m_smtIdMask.mask    = (unsigned char) ~(0xFF << m_smtIdMask.width);
	}

private:
	unsigned char GetMaskWidth(unsigned char maxIds) const
	{
		--maxIds;
		unsigned char msbIdx(8);
		unsigned char msbMask(0x80);
		while (msbMask && !(msbMask & maxIds))
		{
			--msbIdx;
			msbMask >>= 1;
		}
		return msbIdx;
	}

	struct IdMask
	{
		unsigned char width;
		unsigned char mask;
	};

	unsigned char m_logProcsPerPkg;
	unsigned char m_coresPerPkg;
	IdMask m_smtIdMask;
	IdMask m_coreIdMask;
	IdMask m_pkgIdMask;
};


// Fallback solution for WinXP 32/64
static void GetNumCPUCoresApic(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	unsigned int numLogicalPerPhysical(1);
	unsigned int numCoresPerPhysical(1);

	int CPUInfo[4];
	__cpuid(CPUInfo, 0x00000001);
	if ((CPUInfo[3] & 0x10000000) != 0) // Hyperthreading / Multicore bit set
	{
		numLogicalPerPhysical = (CPUInfo[1] & 0x00FF0000) >> 16;

		if (IsIntel())
		{
			__cpuid(CPUInfo, 0x00000000);
			if (CPUInfo[0] >= 0x00000004)
			{
				__cpuidex(CPUInfo, 4, 0);
				numCoresPerPhysical = ((CPUInfo[0] & 0xFC000000) >> 26) + 1;
			}
		}
		else if (IsAMD())
		{
			__cpuid(CPUInfo, 0x80000000);
			if (CPUInfo[0] >= 0x80000008)
			{
				__cpuid(CPUInfo, 0x80000008);
				if (CPUInfo[2] & 0x0000F000)
					numCoresPerPhysical = 1 << ((CPUInfo[2] & 0x0000F000) >> 12);
				else
					numCoresPerPhysical = (CPUInfo[2] & 0xFF) + 1;
			}
		}
	}

	HANDLE hCurProcess(GetCurrentProcess());
	HANDLE hCurThread(GetCurrentThread());

	const int c_maxLogicalProcessors(sizeof(DWORD_PTR) * 8);
	unsigned char apicIds[c_maxLogicalProcessors] = { 0 };
	unsigned char items(0);

	DWORD_PTR processAffinity, systemAffinity;
	GetProcessAffinityMask(hCurProcess, &processAffinity, &systemAffinity);

	if (systemAffinity == 1)
	{
		assert(numLogicalPerPhysical == 1);
		apicIds[items++] = 0;
	}
	else
	{
		if (processAffinity != systemAffinity)
			SetProcessAffinityMask(hCurProcess, systemAffinity);

		DWORD_PTR prevThreadAffinity(0);
		for (DWORD_PTR threadAffinity = 1; threadAffinity && threadAffinity <= systemAffinity; threadAffinity <<= 1)
		{
			if (systemAffinity & threadAffinity)
			{
				if (!prevThreadAffinity)
				{
					assert(!items);
					prevThreadAffinity = SetThreadAffinityMask(hCurThread, threadAffinity);
				}
				else
				{
					assert(items > 0);
					SetThreadAffinityMask(hCurThread, threadAffinity);
				}

				Sleep(0);

				int CPUInfo[4];
				__cpuid(CPUInfo, 0x00000001);
				apicIds[items++] = (unsigned char) ((CPUInfo[1] & 0xFF000000) >> 24);
			}
		}

		SetProcessAffinityMask(hCurProcess, processAffinity);
		SetThreadAffinityMask(hCurThread, prevThreadAffinity);
		Sleep(0);
	}

	CApicExtractor apicExtractor(numLogicalPerPhysical, numCoresPerPhysical);

	totAvailToSystem = 0;
	{
		unsigned char pkgCoreIds[c_maxLogicalProcessors] = { 0 };
		for (unsigned int i(0); i < items; ++i)
		{
			unsigned int j(0);
			for (; j < totAvailToSystem; ++j)
			{
				if (pkgCoreIds[j] == apicExtractor.PackageCoreId(apicIds[i]))
					break;
			}
			if (j == totAvailToSystem)
			{
				pkgCoreIds[j] = apicExtractor.PackageCoreId(apicIds[i]);
				++totAvailToSystem;
			}
		}
	}

	totAvailToProcess = 0;
	{
		unsigned char pkgCoreIds[c_maxLogicalProcessors] = { 0 };
		for (unsigned int i(0); i < items; ++i)
		{
			if (processAffinity & ((DWORD_PTR) 1 << i))
			{
				unsigned int j(0);
				for (; j < totAvailToProcess; ++j)
				{
					if (pkgCoreIds[j] == apicExtractor.PackageCoreId(apicIds[i]))
						break;
				}
				if (j == totAvailToProcess)
				{
					pkgCoreIds[j] = apicExtractor.PackageCoreId(apicIds[i]);
					++totAvailToProcess;
				}
			}
		}
	}
}


static void GetNumCPUCores(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
{
	totAvailToSystem = 0;
	totAvailToProcess = 0;

	GetNumCPUCoresGlpi(totAvailToSystem, totAvailToProcess);

	if (!totAvailToSystem)
	{
		GetNumCPUCoresApic(totAvailToSystem, totAvailToProcess);
	}
}
