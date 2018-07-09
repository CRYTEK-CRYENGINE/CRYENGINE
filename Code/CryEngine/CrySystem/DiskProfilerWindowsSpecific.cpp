// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "DiskProfilerWindowsSpecific.h"

#if CRY_PLATFORM_WINDOWS

#include <CryString/CryString.h>

CDiskProfilerWindowsSpecific::CDiskProfilerWindowsSpecific() : m_log(gEnv->pSystem)
{
	string path = PathUtil::GetEnginePath();
	path = R"(\\.\)" + path.substr(0, path.find_first_of('\\')); // Format to '\\.\C:'
	m_engineDriveHandle = CreateFile(path.c_str(),
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (m_engineDriveHandle == INVALID_HANDLE_VALUE)
	{
		m_engineDriveHandle = nullptr;
	}
	else
	{
		DWORD numBytesReturned = 0;
		LPOVERLAPPED overlapped = nullptr;

		STORAGE_PROPERTY_QUERY query;
		query.PropertyId = StorageDeviceProperty;
		query.QueryType = PropertyStandardQuery;

		STORAGE_DESCRIPTOR_HEADER header;
		BOOL bResult = DeviceIoControl(m_engineDriveHandle,
			IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query),
			&header, sizeof(header),
			&numBytesReturned,
			overlapped);

		if (bResult)
		{
			DWORD size = header.Size;
			char* buffer = new char[size];
			bResult = DeviceIoControl(m_engineDriveHandle,
				IOCTL_STORAGE_QUERY_PROPERTY,
				&query, sizeof(query),
				buffer, size,
				&numBytesReturned,
				overlapped);

			if (bResult)
			{
				STORAGE_DEVICE_DESCRIPTOR* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
				const char* driveInfo = buffer + sizeof(*desc);
				const char* vendorID = driveInfo + desc->VendorIdOffset;
				m_engineDriveName = vendorID;
				CryLogAlways("Engine installed on drive: %s", vendorID);
			}
			delete[] buffer;
		}
	}
}

CDiskProfilerWindowsSpecific::~CDiskProfilerWindowsSpecific()
{
	if (m_engineDriveHandle)
	{
		CloseHandle(m_engineDriveHandle);
	}
}

void CDiskProfilerWindowsSpecific::Update(float timeNow, bool writeToLog)
{
	if (m_lastUpdateTime + 1.0f < timeNow || m_lastUpdateTime == 0.0f)
	{
		DWORD numBytesReturned = 0;
		LPOVERLAPPED overlapped = nullptr;
		BOOL bResult = DeviceIoControl(m_engineDriveHandle,
			IOCTL_DISK_PERFORMANCE,
			NULL, 0,
			&m_currDiskPerf, sizeof(m_currDiskPerf),
			&numBytesReturned,
			overlapped);

		if (m_hasLastPerf)
		{
			const uint64 numBytesRead = m_currDiskPerf.BytesRead.QuadPart - m_lastDiskPerf.BytesRead.QuadPart;
			const uint64 readTime = m_currDiskPerf.ReadTime.QuadPart - m_lastDiskPerf.ReadTime.QuadPart;
			const double readTime100ns = readTime ? (10'000'000.0 / (double)readTime) : 0.0;
			const double bytesPerSec = numBytesRead * readTime100ns;
			m_mbRead = numBytesRead / 1024.0 / 1024.0;
			m_readTimeSec = (m_currDiskPerf.ReadTime.QuadPart - m_lastDiskPerf.ReadTime.QuadPart) / 10'000'000.0;
			m_mbPerSec = numBytesRead ? bytesPerSec / 1024.0 / 1024.0 : 0.0;
		}
		m_lastDiskPerf = m_currDiskPerf;
		m_hasLastPerf = true;
		m_lastUpdateTime = timeNow;

		if (writeToLog)
		{
			m_log.Log("Elapsed time: %f, Total disk usage MB/s: %.2lf, read: %.2lf MB, read time: %.3lf sec", timeNow, m_mbPerSec, m_mbRead, m_readTimeSec);
		}
	}
}

void CDiskProfilerWindowsSpecific::DisableDiskProfiling()
{
	DWORD numBytesReturned = 0;
	LPOVERLAPPED overlapped = nullptr;
	BOOL bResult = DeviceIoControl(m_engineDriveHandle,
		IOCTL_DISK_PERFORMANCE_OFF,
		NULL, 0,
		NULL, 0,
		&numBytesReturned,
		overlapped);
	m_hasLastPerf = false;
}

#endif