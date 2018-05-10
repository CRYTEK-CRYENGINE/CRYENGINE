// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompletionPortFileReader.h"
#include "FileReader.h"
#include <condition_variable>
//#include <thread>

namespace CompletionPortFileReader_Private
{

class CUniqueHandle
{
public:
	CUniqueHandle(HANDLE value) : m_handle(value) {}
	~CUniqueHandle() { ::CloseHandle(m_handle); }
	inline operator HANDLE() const
	{
		assert(m_handle != INVALID_HANDLE_VALUE);
		return m_handle;
	}

	CUniqueHandle(CUniqueHandle&& v)
	{
		m_handle = v.m_handle;
		v.m_handle = INVALID_HANDLE_VALUE;
	}
	const CUniqueHandle& operator=(CUniqueHandle&&) = delete;

	//Non-copyable
	CUniqueHandle(const CUniqueHandle&) = delete;
	const CUniqueHandle& operator=(const CUniqueHandle&) = delete;
private:
	HANDLE m_handle;
};

class CResourceTracker
{
public:
	CResourceTracker(size_t maxOpenFilesCount, size_t maxMemoryUsageBytes)
		: m_maxOpenFilesCount(maxOpenFilesCount)
		, m_maxMemoryUsageBytes(maxMemoryUsageBytes)
		, m_openFilesCount(0)
		, m_memoryUsageBytes(0)
	{
	}

public:

	// Blocks the current thread until gets a free file slot.
	void Wait(size_t requestedSizeInBytes)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condition.wait(lock, [this, requestedSizeInBytes]()
		{
			// We allow to allocate more memory than the maximum if it is the only active request at the moment.
			// Thus, we serve such requests sequentially, one after the other.
			return !m_openFilesCount || (IsFileSlotAvailable() && IsMemoryAvailable(requestedSizeInBytes));
		});
		m_memoryUsageBytes += requestedSizeInBytes;
		++m_openFilesCount;
	}

	// Signal a free file slot/memoty is available.
	void Signal(size_t freedSizeInBytes)
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			CRY_ASSERT(m_memoryUsageBytes >= freedSizeInBytes);
			CRY_ASSERT(m_openFilesCount);

			m_memoryUsageBytes -= freedSizeInBytes;
			--m_openFilesCount;
		}
		m_condition.notify_one();
	}

	virtual void WaitForIdle()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condition.wait(lock, [this]()
		{
			// Both are == 0 or both are != 0.
			CRY_ASSERT((!m_openFilesCount) == (!m_memoryUsageBytes));

			return m_openFilesCount == 0;
		});
	}

private:
	bool IsFileSlotAvailable()                          { return m_openFilesCount < m_maxOpenFilesCount; }
	bool IsMemoryAvailable(size_t requestedSizeInBytes) { return (m_memoryUsageBytes + requestedSizeInBytes) <= m_maxMemoryUsageBytes; }

private:
	std::mutex              m_mutex;
	std::condition_variable m_condition;
	const size_t            m_maxOpenFilesCount;
	const size_t            m_maxMemoryUsageBytes;
	size_t                  m_openFilesCount;
	size_t                  m_memoryUsageBytes;
};

class CCompletionPortFileReader : public AssetLoader::IFileReader
{
public:
	struct SCompletionData : OVERLAPPED
	{
		SCompletionData(CUniqueHandle&& file, size_t size, CompletionHandler completionHandler, void* pUserData)
			: file(std::move(file))
			, buffer(size)
			, completionHandler(completionHandler)
			, pUserData(pUserData)
		{
			assert(size);
			assert(completionHandler);

			Internal = 0;
			InternalHigh = 0;
			Offset = 0;
			OffsetHigh = 0;
			hEvent = NULL;
		}

		CUniqueHandle     file;
		std::vector<char> buffer;
		CompletionHandler completionHandler;
		void* pUserData;
	};

public:
	CCompletionPortFileReader(size_t maxOpenFilesCount, size_t maxMemoryUsageBytes)
		: m_completionPort(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
		, m_resourceTracker(maxOpenFilesCount, maxMemoryUsageBytes)
	{
		if (m_completionPort == NULL)
		{
			return;
		}
		uint concurrentThreadsSupported = std::thread::hardware_concurrency();
		concurrentThreadsSupported = std::max(concurrentThreadsSupported, 2U);

		m_threads.reserve(concurrentThreadsSupported);
		for (size_t i = 0; i < concurrentThreadsSupported; ++i)
		{
			m_threads.emplace_back(::CreateThread(nullptr, 0, CompletionThreadProc, this, CREATE_SUSPENDED, nullptr));
		}

		if (std::any_of(m_threads.begin(), m_threads.end(), [](const CUniqueHandle& t) { return t == nullptr; }))
		{
			m_threads.clear();
		}

		for (const CUniqueHandle& t : m_threads)
		{
			::ResumeThread(t);
		}
	}

	~CCompletionPortFileReader()
	{
		WaitForCompletion();

		// Signal the Completion threads to exit.
		for (const CUniqueHandle& t : m_threads)
		{
			::PostQueuedCompletionStatus(m_completionPort, 0, 0, nullptr);
		}

		for (const CUniqueHandle& t : m_threads)
		{
			::WaitForSingleObject(t, INFINITE);
		}
	}

	bool ReadFileAsync(const char* szFilePath, const CompletionHandler& completionHandler, void* pUserData) override
	{
		if (m_threads.empty())
		{
			return false;
		}

		CUniqueHandle file(::CreateFile(szFilePath, FILE_GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		LARGE_INTEGER fileSize = { 0 };
		if (!::GetFileSizeEx(file, &fileSize) || !fileSize.QuadPart)
		{
			return false;
		}

		// TODO : Performance optimization: pre-allocate a circular array of SCompletionData [maxOpenFilesCount].
		m_resourceTracker.Wait(fileSize.QuadPart);

		std::unique_ptr<SCompletionData> pData(new SCompletionData(std::move(file), fileSize.QuadPart, completionHandler, pUserData));

		const ULONG_PTR completionKey = 1;
		HANDLE port = ::CreateIoCompletionPort(pData->file, m_completionPort, completionKey, 0);
		if ((port != m_completionPort) || !::ReadFile(pData->file, pData->buffer.data(), pData->buffer.size(), nullptr, pData.get()) && GetLastError() != ERROR_IO_PENDING)
		{
			m_resourceTracker.Signal(fileSize.QuadPart);
			return false;
		}

		// The completion routine will deallocate the memory used by the overlapped structure.
		return pData.release() != nullptr;
	}

	virtual void WaitForCompletion() override
	{
		m_resourceTracker.WaitForIdle();
	}

private:
	static DWORD WINAPI CompletionThreadProc(LPVOID lpParameter)
	{
		return static_cast<CCompletionPortFileReader*>(lpParameter)->Completion();
	}

	DWORD WINAPI Completion()
	{
		DWORD numberOfBytes;
		ULONG_PTR completionKey;
		OVERLAPPED* pOverlapped = nullptr;

		while (true)
		{
			// Upon failure(the return value is FALSE) pOverlapped parameter can still have a valid value.
			const bool bOk = ::GetQueuedCompletionStatus(m_completionPort, &numberOfBytes, &completionKey, &pOverlapped, INFINITE);

			if (bOk && !pOverlapped)
			{
				break;
			}

			const size_t bufferSize = ((SCompletionData*)pOverlapped)->buffer.size();

			// The system does not use the OVERLAPPED structure after the completion routine is called,
			// deallocate the memory used by the overlapped structure.
			{
				std::unique_ptr<SCompletionData> pData((SCompletionData*)pOverlapped);

				if (bOk && (pData->buffer.size() == numberOfBytes))
				{
					pData->completionHandler(pData->buffer.data(), pData->buffer.size(), pData->pUserData);
				}
			}
			m_resourceTracker.Signal(bufferSize);
		}
		return 0;
	}

private:
	CUniqueHandle              m_completionPort;
	std::vector<CUniqueHandle> m_threads;
	CResourceTracker           m_resourceTracker;
};

}

namespace AssetLoader
{

std::unique_ptr<IFileReader> CreateCompletionPortFileReader()
{
	using namespace CompletionPortFileReader_Private;

	// TODO: console variable?
	const size_t maxOpenFilesCount = 16;
	const size_t maxMemoryUsageBytes = 1 << 30; // 1GB.
	return std::unique_ptr<IFileReader>(new CCompletionPortFileReader(maxOpenFilesCount, maxMemoryUsageBytes));
}

}

