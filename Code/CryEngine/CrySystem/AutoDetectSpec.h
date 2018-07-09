// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AUTODETECTSPEC_
#define _AUTODETECTSPEC_

#pragma once

#if CRY_PLATFORM_WINDOWS

// exposed AutoDetectSpec() helper functions for reuse in CrySystem
namespace Win32SysInspect
{
enum DXFeatureLevel
{
	DXFL_Undefined,
	DXFL_9_1,
	DXFL_9_2,
	DXFL_9_3,
	DXFL_10_0,
	DXFL_10_1,
	DXFL_11_0,
	DXFL_11_1,
	DXFL_12_0,
	DXFL_12_1
};

static const char* GetFeatureLevelAsString(DXFeatureLevel featureLevel)
{
	switch (featureLevel)
	{
	default:
	case Win32SysInspect::DXFL_Undefined:
		return "unknown";
	case Win32SysInspect::DXFL_9_1:
		return "D3D 9_1 (SM 2.0)";
	case Win32SysInspect::DXFL_9_2:
		return "D3D 9_2 (SM 2.0)";
	case Win32SysInspect::DXFL_9_3:
		return "D3D 9_3 (SM 2.x)";
	case Win32SysInspect::DXFL_10_0:
		return "D3D 10_0 (SM 4.0)";
	case Win32SysInspect::DXFL_10_1:
		return "D3D 10_1 (SM 4.x)";
	case Win32SysInspect::DXFL_11_0:
		return "D3D 11_0 (SM 5.0)";
	case Win32SysInspect::DXFL_11_1:
		return "D3D 11_1 (SM 5.x)";
	case Win32SysInspect::DXFL_12_0:
		return "D3D 12_0 (SM 6.0)";
	case Win32SysInspect::DXFL_12_1:
		return "D3D 12_1 (SM 6.0)";
	}
}

void          GetNumCPUCores(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess);
bool          IsDX11Supported();
bool          IsDX12Supported();
bool          GetGPUInfo(char* pName, size_t bufferSize, unsigned int& vendorID, unsigned int& deviceID, unsigned int& totLocalVidMem, DXFeatureLevel& featureLevel);
int           GetGPURating(unsigned int vendorId, unsigned int deviceId);
void          GetOS(SPlatformInfo::EWinVersion& ver, bool& is64Bit, char* pName, size_t bufferSize);
bool          IsVistaKB940105Required();

inline size_t SafeMemoryThreshold(size_t memMB)
{
	return (memMB * 8) / 10;
}
}

#endif // #if CRY_PLATFORM_WINDOWS

#endif // #ifndef _AUTODETECTSPEC_
