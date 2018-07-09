// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CPUDetect.cpp : CPU detection.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#ifndef __CPUDETECT_H__
#define __CPUDETECT_H__

//-------------------------------------------------------
/// Cpu class
//-------------------------------------------------------
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#define MAX_CPU 64
#else
	#define MAX_CPU 32
#endif

/// Type of Cpu Vendor.
enum ECpuVendor
{
	eCVendor_Unknown,
	eCVendor_Intel,
	eCVendor_Cyrix,
	eCVendor_AMD,
	eCVendor_Centaur,
	eCVendor_NexGen,
	eCVendor_UMC,
	eCVendor_M68K
};

/// Type of Cpu Model.
enum ECpuModel
{
	eCpu_Unknown,

	eCpu_8086,
	eCpu_80286,
	eCpu_80386,
	eCpu_80486,
	eCpu_Pentium,
	eCpu_PentiumPro,
	eCpu_Pentium2,
	eCpu_Pentium3,
	eCpu_Pentium4,
	eCpu_Pentium2Xeon,
	eCpu_Pentium3Xeon,
	eCpu_Celeron,
	eCpu_CeleronA,

	eCpu_Am5x86,
	eCpu_AmK5,
	eCpu_AmK6,
	eCpu_AmK6_2,
	eCpu_AmK6_3,
	eCpu_AmK6_3D,
	eCpu_AmAthlon,
	eCpu_AmDuron,

	eCpu_CyrixMediaGX,
	eCpu_Cyrix6x86,
	eCpu_CyrixGXm,
	eCpu_Cyrix6x86MX,

	eCpu_CenWinChip,
	eCpu_CenWinChip2,
};

struct SCpu
{
	ECpuVendor    meVendor;
	ECpuModel     meModel;
	uint32        mFeatures;
	bool          mbSerialPresent;
	char          mSerialNumber[30];
	int           mFamily;
	int           mModel;
	int           mStepping;
	char          mVendor[64];
	char          mCpuType[64];
	char          mFpuType[64];
	bool          mbPhysical; // false for hyperthreaded
	DWORD_PTR     mAffinityMask;

	// constructor
	SCpu()
		: meVendor(eCVendor_Unknown), meModel(eCpu_Unknown), mFeatures(0),
		mbSerialPresent(false), mFamily(0), mModel(0), mStepping(0),
		mbPhysical(true), mAffinityMask(0)
	{
		memset(mSerialNumber, 0, sizeof(mSerialNumber));
		memset(mVendor, 0, sizeof(mVendor));
		memset(mCpuType, 0, sizeof(mCpuType));
		memset(mFpuType, 0, sizeof(mFpuType));
	}
};

class CCpuFeatures
{
private:
	uint  m_NumLogicalProcessors;
	uint  m_NumSystemProcessors;
	uint  m_NumAvailProcessors;
	uint  m_NumPhysicsProcessors;
	uint32 m_nFeatures;
	bool m_bOS_ISSE;
	bool m_bOS_ISSE_EXCEPTIONS;
public:

	SCpu m_Cpu[MAX_CPU];

public:
	CCpuFeatures()
	{
		m_NumLogicalProcessors = 0;
		m_NumSystemProcessors = 0;
		m_NumAvailProcessors = 0;
		m_NumPhysicsProcessors = 0;
		m_nFeatures = 0;
		m_bOS_ISSE = 0;
		m_bOS_ISSE_EXCEPTIONS = 0;
		ZeroMemory(m_Cpu, sizeof(m_Cpu));
	}

	void      Detect();

	uint      GetLogicalCPUCount() const          { return m_NumLogicalProcessors; }
	uint      GetPhysCPUCount() const             { return m_NumPhysicsProcessors; }
	uint      GetCPUCount() const                 { return m_NumAvailProcessors; }
	uint32    GetFeatures() const                 { return m_nFeatures; }
	DWORD_PTR GetCPUAffinityMask(uint iCPU) const { assert(iCPU < MAX_CPU); return iCPU < GetCPUCount() ? m_Cpu[iCPU].mAffinityMask : 0; }
	DWORD_PTR GetPhysCPUAffinityMask(uint iCPU) const
	{
		if (iCPU > GetPhysCPUCount())
			return 0;
		int i;
		for (i = 0; (int)iCPU >= 0; i++)
			if (m_Cpu[i].mbPhysical)
				--iCPU;
		PREFAST_ASSUME(i > 0 && i < MAX_CPU);
		return m_Cpu[i - 1].mAffinityMask;
	}
};

#endif // #ifndef __CPUDETECT_H__
