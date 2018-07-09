// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "Digest.h"
#include <md5/md5.h>
#include <array>
#include <thread>
#include <condition_variable>

namespace Digest_Private
{
	template <typename T>
	class CBlockingCollection
	{
	public:
		typedef std::deque<T> UnderlyingContainer;

	public:
		void Push(const T& value)
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				assert(!m_bCompleted);
				m_container.push_front(value);
			}
			m_condition.notify_one();
		}

		void Complete()
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_bCompleted = true;
			}
			m_condition.notify_one();
		}

		bool Pop(T& value)
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_condition.wait(lock, [this]() { return !m_container.empty() || IsCompleted(); });
			if (IsCompleted())
			{
				return false;
			}
			value = std::move(m_container.back());
			m_container.pop_back();
			return true;
		}

	private:
		bool IsCompleted() const { return m_bCompleted && m_container.empty(); };

	private:
		std::mutex              m_mutex;
		std::condition_variable m_condition;
		UnderlyingContainer		m_container;
		bool					m_bCompleted = false;
	};

	typedef std::unique_ptr<FILE, int(*)(FILE*)> file_ptr;

	template<typename T, size_t bufferSizeElements, typename C>
	bool ReadFile(const char* szFilePath, C callback)
	{
		T buffer[bufferSizeElements];

		file_ptr file(fopen(szFilePath, "rb"), fclose);

		if (!file.get())
			return false;

		while (size_t elementsRead = fread(buffer, sizeof(T), bufferSizeElements, file.get()))
		{
			callback(buffer, elementsRead);
		};

		return !std::ferror(file.get());
	}

	template<typename T, size_t bufferSizeElements, typename C>
	bool ReadFileParallel(const char* szFilePath, C callback)
	{
		static const size_t buffersCount = 3;

		T buffer[buffersCount][bufferSizeElements];

		file_ptr file(fopen(szFilePath, "rb"), fclose);

		if (!file.get())
			return false;

		struct SData
		{
			uint8* p; 
			size_t size;
		};

		CBlockingCollection<SData> writeQueue;
		CBlockingCollection<SData> readQueue;

		for (size_t i = 0; i < buffersCount; ++i)
		{
			writeQueue.Push(SData{ buffer[i], 0 });
		}

		// Wait and process data in a dedicated consumer thread.
		std::thread consumer([&readQueue, &writeQueue, &callback]()
		{
			SData data;
			while (readQueue.Pop(data))
			{
				callback(data.p, data.size);
				writeQueue.Push(data);
			}
		});

		SData data;
		while (writeQueue.Pop(data))
		{
			data.size = fread(data.p, sizeof(T), bufferSizeElements, file.get());
			if (!data.size)
			{
				break;
			}
			readQueue.Push(data);
		};
		readQueue.Complete();
		consumer.join();

		return !std::ferror(file.get());
	}
}

CDigest::CDigest()
{
	MD5Init(&m_context);
}

void CDigest::UpdateFromData(uint8* data, const size_t dataSize)
{
	MD5Update(&m_context, data, dataSize);
}

bool CDigest::UpdateFromFile(const string& filePath)
{
	using namespace Digest_Private;

	auto updateDigest = [this](uint8* data, const size_t dataSize)
	{
		UpdateFromData(data, dataSize);
	};

	static const size_t bufferSize = 32768;

	return ReadFileParallel<uint8, bufferSize>(filePath.c_str(), updateDigest);
}

void CDigest::Final(std::array<uint8, s_digestSize>& digest)
{
	MD5Final(digest.data(), &m_context);
}

string CDigest::Final()
{
	std::array<uint8, s_digestSize> digest;
	Final(digest);

	string result;
	for (uint8 i : digest)
	{
		result += string().Format("%02x", i);
	}
	return result;
}

