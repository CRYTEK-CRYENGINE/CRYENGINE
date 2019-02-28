// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DriverD3D.h"
#include "Common/TypedConstantBuffer.h"

// forward declarations
class CShader;

namespace gpu
{

struct CounterReadbackEmpty
{
	int Readback(CDeviceBuffer* pBuffer)
	{
		assert(0);
		return 0;
	}
};

struct CounterReadbackUsed
{
	CounterReadbackUsed();
	void Readback(CDeviceBuffer* pBuffer);
	int  Retrieve();

private:
	CGpuBuffer* m_countReadbackBuffer;
#ifdef DURANGO
	void*       m_basePtr;
#endif

#if CRY_RENDERER_VULKAN
	uint64      m_readbackFence;
#endif

	bool        m_readbackCalled;
};

struct DataReadbackEmpty
{
	DataReadbackEmpty(int size, int stride){};
	const void* Map()
	{
		assert(0);
		return NULL;
	};
	void Unmap() { assert(0); };
};

struct DataReadbackUsed
{
	DataReadbackUsed(uint32 size, uint32 stride);
	void Readback(CGpuBuffer* buf, uint32 readLength);
	const void* Map(uint32 readLength);
	void Unmap();

private:
	CGpuBuffer* m_readback;
#ifdef DURANGO
	void*       m_basePtr;
#endif

#if CRY_RENDERER_VULKAN
	uint64      m_readbackFence;
#endif

	uint32      m_stride;
	uint32      m_size;
	bool        m_readbackCalled;
};

struct HostDataEmpty
{
	void   Resize(size_t size) {};
	uint8* Get()               { return nullptr; }
};

struct HostDataUsed
{
	void   Resize(size_t size) { m_data.resize(size); }
	uint8* Get()               { return &m_data[0]; }
private:
	std::vector<uint8> m_data;
};

struct BufferFlagsReadWrite
{
	enum
	{
		flags = CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS
	};
	typedef CounterReadbackEmpty CounterReadback;
	typedef DataReadbackEmpty    DataReadback;
	typedef HostDataEmpty        HostData;
};

struct BufferFlagsReadWriteReadback
{
	enum
	{
		flags = CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS | CDeviceObjectFactory::USAGE_UAV_OVERLAP
	};
	typedef CounterReadbackEmpty CounterReadback;
	typedef DataReadbackUsed     DataReadback;
	typedef HostDataEmpty        HostData;
};

struct BufferFlagsReadWriteAppend
{
	enum
	{
		flags = CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS | CDeviceObjectFactory::USAGE_UAV_COUNTER
	};
	typedef CounterReadbackUsed CounterReadback;
	typedef DataReadbackEmpty   DataReadback;
	typedef HostDataEmpty       HostData;
};

struct BufferFlagsDynamic
{
	enum
	{
		flags = CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE
	};
	typedef CounterReadbackEmpty CounterReadback;
	typedef DataReadbackEmpty    DataReadback;
	typedef HostDataUsed         HostData;
};

template<typename BFlags> class CStridedResource
{
public:
	CStridedResource(int stride, int size)
		: m_size(size), m_stride(stride), m_dataReadback(size, stride)
	{
		m_buffer.Create(size, stride, DXGI_FORMAT_UNKNOWN, BFlags::flags, NULL);
	}
#if 0
	ID3D11UnorderedAccessView* GetUAV()    { return m_buffer.GetDeviceUAV(); };
	ID3D11ShaderResourceView*  GetSRV()    { return m_buffer.GetSRV(); };
	ID3D11Buffer*              GetBuffer() { return m_buffer.GetBuffer(); };
#endif

	void                       UpdateBufferContent(void* pData, size_t nSize)
	{
		m_buffer.UpdateBufferContent(pData, m_stride * nSize);
	};
	void        ReadbackCounter() { return m_counterReadback.Readback(m_buffer.GetDevBuffer()); };
	int         RetrieveCounter() { return m_counterReadback.Retrieve(); };
	void        Readback()        { return m_dataReadback.Readback(m_buffer.GetDevBuffer()); };
	const void* Map()             { return (const void*)m_dataReadback.Map(); };
	void        Unmap()           { return m_dataReadback.Unmap(); };

private:
	const int  m_size;
	const int  m_stride;
	CGpuBuffer m_buffer;
	// this is only an nonempty struct if it is used
	typedef typename BFlags::CounterReadback CounterReadback;
	CounterReadback m_counterReadback;
	typedef typename BFlags::DataReadback    DataReadback;
	DataReadback    m_dataReadback;
};

template<typename T> inline DXGI_FORMAT DXGI_FORMAT_DETECT() { return DXGI_FORMAT_UNKNOWN; }
template<> inline DXGI_FORMAT DXGI_FORMAT_DETECT<int>() { return DXGI_FORMAT_R32_SINT; }
template<> inline DXGI_FORMAT DXGI_FORMAT_DETECT<uint>() { return DXGI_FORMAT_R32_UINT; }
template<> inline DXGI_FORMAT DXGI_FORMAT_DETECT<float>() { return DXGI_FORMAT_R32_FLOAT; }

template<typename T> inline CDeviceObjectFactory::EResourceAllocationFlags USAGE_DETECT() { return CDeviceObjectFactory::USAGE_STRUCTURED; }
template<> inline CDeviceObjectFactory::EResourceAllocationFlags USAGE_DETECT<int>() { return CDeviceObjectFactory::EResourceAllocationFlags(0); }
template<> inline CDeviceObjectFactory::EResourceAllocationFlags USAGE_DETECT<uint>() { return CDeviceObjectFactory::EResourceAllocationFlags(0); }
template<> inline CDeviceObjectFactory::EResourceAllocationFlags USAGE_DETECT<float>() { return CDeviceObjectFactory::EResourceAllocationFlags(0); }

template<typename T, typename BFlags> class CTypedResource
{
public:
	CTypedResource(int size) : m_size(size), m_dataReadback(size, sizeof(T))
	{
		m_hostData.Resize(size * sizeof(T));
	}

	void CreateDeviceBuffer()
	{
		m_buffer.Create(m_size, sizeof(T), DXGI_FORMAT_DETECT<T>(), USAGE_DETECT<T>() | BFlags::flags, NULL);
	}
	void FreeDeviceBuffer()
	{
		m_buffer.Release();
	}
	CGpuBuffer& GetBuffer() { return m_buffer; };

	T&          operator[](size_t i)
	{
		return *reinterpret_cast<T*>(m_hostData.Get() + i * sizeof(T));
	}
	size_t GetSize() const { return m_size; }
	void   UploadHostData()
	{
		UpdateBufferContent(m_hostData.Get(), m_size);
	};
	void UpdateBufferContent(const void* pData, size_t nSize)
	{
		m_buffer.UpdateBufferContent(pData, sizeof(T) * nSize);
	};
	void UpdateBufferContentAligned(const void* pData, size_t nSize)
	{
		m_buffer.UpdateBufferContent(pData, Align(sizeof(T) * nSize, CRY_PLATFORM_ALIGNMENT));
	};
	void     ReadbackCounter() { return m_counterReadback.Readback(m_buffer.GetDevBuffer()); };
	int      RetrieveCounter() { return m_counterReadback.Retrieve(); };
	void     Readback(uint32 readLength) { return m_dataReadback.Readback(&m_buffer, readLength); };
	const T* Map(uint32 readLength) { return (const T*)m_dataReadback.Map(readLength); };
	void     Unmap() { return m_dataReadback.Unmap(); };
	bool     IsDeviceBufferAllocated() { return m_buffer.GetDevBuffer() != nullptr; }
private:
	const int  m_size;
	CGpuBuffer m_buffer;
	// this is only an nonempty struct if it is used
	typedef typename BFlags::CounterReadback CounterReadback;
	CounterReadback m_counterReadback;
	typedef typename BFlags::DataReadback    DataReadback;
	DataReadback    m_dataReadback;
	typedef typename BFlags::HostData        HostData;
	HostData        m_hostData;
};

template<typename T, typename BFlags> using CStructuredResource = CTypedResource<T, BFlags>;

template<typename T> class CTypedConstantBuffer : public ::CTypedConstantBuffer<T, 256>
{
	typedef typename ::CTypedConstantBuffer<T, 256> TBase;
public:
	bool IsDeviceBufferAllocated() { return TBase::m_constantBuffer != nullptr; }
	T&   operator=(const T& hostData)
	{
		return TBase::m_hostBuffer = hostData;
	}
	T&       GetHostData()       { return TBase::m_hostBuffer; }
	const T& GetHostData() const { return TBase::m_hostBuffer; }
};

inline int GetNumberOfBlocksForArbitaryNumberOfThreads(const int threads, const int blockSize)
{
	return threads / blockSize + (threads % blockSize != 0);
}

template<class T>
class CDoubleBuffered
{
public:
	CDoubleBuffered(int size) : m_size(size), m_current(0), m_isDoubleBuffered(false) {}
	void Initialize(bool isDoubleBuffered)
	{
		Reset();

		m_buffers[0] = std::unique_ptr<T>(new T(m_size));
		m_buffers[0]->CreateDeviceBuffer();

		if (isDoubleBuffered)
		{
			m_buffers[1] = std::unique_ptr<T>(new T(m_size));
			m_buffers[1]->CreateDeviceBuffer();
		}

		m_isDoubleBuffered = isDoubleBuffered;
	}
	void Reset()
	{
		m_current = 0;
		m_buffers[0].reset();
		m_buffers[1].reset();
	}
	T&       Get()                 { return *m_buffers[m_current]; }
	const T& Get() const           { return *m_buffers[m_current]; }
	T&       GetBackBuffer()       { return *m_buffers[!m_current]; }
	const T& GetBackBuffer() const { return *m_buffers[!m_current]; }
	void     Swap()
	{
		if (m_isDoubleBuffered)
			m_current = !m_current;
	}
	int GetCurrentBufferId() const { return m_current; }

private:
	std::unique_ptr<T> m_buffers[2];
	const int          m_size;
	int                m_current;
	bool               m_isDoubleBuffered;
};

}
