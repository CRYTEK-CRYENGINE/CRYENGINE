// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/Platform/CryWindows.h>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/// \brief encapsulates a windows handle and closes it automatically
class CUniqueHandle
{
public:
	inline CUniqueHandle()
		: m_handle(INVALID_HANDLE_VALUE)
	{}

	inline CUniqueHandle(HANDLE handle)
		: m_handle(handle)
	{}
	inline CUniqueHandle& operator=(HANDLE handle)
	{
		Close();
		m_handle = handle;
		return *this;
	}

	inline CUniqueHandle(const CUniqueHandle&)
		: m_handle(INVALID_HANDLE_VALUE)
	{
		// Copy is NOOP
	}
	inline CUniqueHandle& operator=(const CUniqueHandle&)
	{
		// Copy is NOOP
		return *this;
	}

	inline CUniqueHandle(CUniqueHandle&& handle)
		: m_handle(handle.Move())
	{}
	inline CUniqueHandle& operator=(CUniqueHandle&& handle)
	{
		Close();
		m_handle = handle.Move();
		return *this;
	}

	inline ~CUniqueHandle()
	{
		Close();
	}

	inline bool IsValid() const
	{
		return m_handle != INVALID_HANDLE_VALUE;
	}

	inline HANDLE GetHandle() const
	{
		return m_handle;
	}

	inline operator HANDLE() const
	{
		return m_handle;
	}

	inline operator HANDLE&()
	{
		return m_handle;
	}

	inline HANDLE Move()
	{
		auto handle = m_handle;
		m_handle = INVALID_HANDLE_VALUE;
		return handle;
	}

private:
	inline void Close()
	{
		if (IsValid())
		{
			CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
	}

private:
	HANDLE m_handle;
};

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

