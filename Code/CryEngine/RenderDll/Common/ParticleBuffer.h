// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <array>
#include <CryRenderer/RenderElements/CREParticle.h>
#include "../XRenderD3D9/DeviceManager/DeviceObjects.h"
#include "../XRenderD3D9/DeviceManager/D3D11/DeviceSubmissionQueue_D3D11.h" // CSubmissionQueue_DX11

struct CDeviceInputStream;
class CConstantBuffer;
class CDeviceResourceSet;
typedef _smart_ptr<CConstantBuffer> CConstantBufferPtr;
typedef std::shared_ptr<CDeviceResourceSet> CDeviceResourceSetPtr;

class CParticleSubBuffer
{
public:
	CParticleSubBuffer();
	~CParticleSubBuffer();

	void    Create(uint elemsCount, uint stride, DXGI_FORMAT format, uint32 flags);
	void    Release();
	byte*   Lock();
	void    Unlock(uint32 size);

	const CGpuBuffer&         GetBuffer() const { return m_buffer; }
	const CDeviceInputStream* GetStream() const { return m_stream; }

private:
	stream_handle_t           m_handle;
	const CDeviceInputStream* m_stream;
	CGpuBuffer                m_buffer;
	byte*                     m_pLockedData;
	uint                      m_flags;
};

class CParticleBufferSet
{
private:
	typedef std::array<CParticleSubBuffer, CREParticle::numBuffers> TDynBuffer;
	typedef std::array<DeviceFenceHandle, CREParticle::numBuffers>  TDeviceFences;

	template <typename Element, DXGI_FORMAT format, uint32_t flags>
	struct SubBuffer
	{
		static constexpr size_t stride = sizeof(Element);

		TDynBuffer m_buffers;
		// During writing, we only expose the base Video Memory pointer
		// and the offsets to the next free memory in this buffer
		Element* m_pMemoryBase[CREParticle::numBuffers];
		uint32 m_offset[CREParticle::numBuffers];
		uint m_size[CREParticle::numBuffers];
		uint m_maxSize = 0;

		inline void Create(uint elemsCount, uint maxSize)
		{
			for (uint i = 0; i < CREParticle::numBuffers; ++i)
			{
				m_buffers[i].Release();
				if (elemsCount)
					m_buffers[i].Create(elemsCount, stride, format, flags);
				m_pMemoryBase[i] = nullptr;
				m_size[i] = elemsCount;
				m_offset[i] = 0;
			}
			m_maxSize = std::max(maxSize, elemsCount);
		}
		inline void Release()
		{
			for (uint i = 0; i < CREParticle::numBuffers; ++i)
			{
				m_buffers[i].Release();
				m_pMemoryBase[i] = nullptr;
				m_offset[i] = 0;
			}
		}

		inline void Lock(uint id, uint32 offset)
		{
			assert(m_size[id]);

			if (offset > m_size[id] && m_size[id] < m_maxSize)
			{
				// Growth strategy: Increase size by at least 50% until max allowed size.
				m_size[id] = std::min(m_maxSize, std::max(offset, m_size[id] * 3 / 2));
				m_buffers[id].Create(m_size[id], stride, format, flags);

				if (m_size[id] < offset)
					CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_WARNING, "Particle buffer overflow. Consider increasing r_ParticleMaxVerticePoolSize.");
			}
			m_pMemoryBase[id] = reinterpret_cast<Element*>(m_buffers[id].Lock());
			m_offset[id] = 0;
		}
		inline void Lock(uint id) { Lock(id, m_offset[id]); }

		inline void Unlock(uint id, uint32 offset)
		{
			const auto usedBytes = std::min<uint>(offset, m_size[id]) * stride;

			m_buffers[id].Unlock(usedBytes);
			m_pMemoryBase[id] = nullptr;
		}
		inline void Unlock(uint id) { Unlock(id, m_offset[id]); }
	};

private:
	void                      CreateSpriteBuffer(uint poolSize);

	// Atomically allocates a sequence of numElems (if possible) in sub-buffer id of the buffer
	template <typename Buffer>
	static std::pair<uint, uint> AllocBuffer(Buffer &buffer, uint id, uint numElems)
	{
		const auto start = CryInterlockedAdd((volatile LONG*)&buffer.m_offset[id], numElems) - numElems;
		const auto availableElements = buffer.m_size[id];
		const auto count = availableElements <= start ? 0 : std::min<uint>(numElems, availableElements - start);
		return std::make_pair(start, count);
	}

	template <typename Buffer>
	const CGpuBuffer& GetGpuBuffer(Buffer &buffer, int frameId) const
	{
		const uint bindId = frameId % CREParticle::numBuffers;
		CRY_ASSERT_MESSAGE(!buffer.m_pMemoryBase[bindId], "Cannot use buffer that wasn't unlocked.");
		return buffer.m_buffers[bindId].GetBuffer();
	}
	template <typename Buffer>
	const CDeviceInputStream* GetStreamBuffer(Buffer &buffer, int frameId) const
	{
		const uint bindId = frameId % CREParticle::numBuffers;
		CRY_ASSERT_MESSAGE(!buffer.m_pMemoryBase[bindId], "Cannot use buffer that wasn't unlocked.");
		return buffer.m_buffers[bindId].GetStream();
	}

public:
	struct SAlloc
	{
		byte* m_pBase;
		uint  m_firstElem;
		uint  m_numElemns;
	};

	struct SAllocStreams
	{
		Vec3*             m_pPositions;
		SParticleAxes*    m_pAxes;
		SParticleColorST* m_pColorSTs;
		uint              m_firstElem;
		uint              m_numElemns;
	};

public:
	CParticleBufferSet();
	~CParticleBufferSet();

	void Create(uint poolSize, uint maxPoolSize);
	void Release();
	uint GetMaxNumSprites() const { return m_maxSpriteCount; }

	void Lock(int frameId);
	void Unlock(int frameId);
	void SetFence(int frameId);
	void WaitForFence(int frameId);
	bool IsValid(int frameId) const;

	uint GetAllocId(int frameId) const;
	SAlloc AllocVertices(uint index, uint numElems);
	SAlloc AllocIndices(uint index, uint numElems);
	SAllocStreams AllocStreams(uint index, uint numElems);

	inline const CDeviceInputStream* GetSpriteIndexBuffer() const         { return m_spriteIndexBufferStream; }
	inline const CGpuBuffer&         GetPositionStream(int frameId) const { return GetGpuBuffer(m_subBufferPositions, frameId); }
	inline const CGpuBuffer&         GetAxesStream(int frameId) const     { return GetGpuBuffer(m_subBufferAxes, frameId); }
	inline const CGpuBuffer&         GetColorSTsStream(int frameId) const { return GetGpuBuffer(m_subBufferColors, frameId); }
	inline const CDeviceInputStream* GetVertexStream(int frameId) const   { return GetStreamBuffer(m_subBufferVertices, frameId); }
	inline const CDeviceInputStream* GetIndexStream(int frameId) const    { return GetStreamBuffer(m_subBufferIndices, frameId); }

private:
	SubBuffer<SVF_P3F_C4B_T4B_N3F2, DXGI_FORMAT_UNKNOWN,  CDeviceObjectFactory::BIND_VERTEX_BUFFER | CDeviceObjectFactory::USAGE_CPU_WRITE> m_subBufferVertices;
	SubBuffer<uint16,               DXGI_FORMAT_R16_UINT, CDeviceObjectFactory::BIND_INDEX_BUFFER  | CDeviceObjectFactory::USAGE_CPU_WRITE> m_subBufferIndices;
	SubBuffer<Vec3,                 DXGI_FORMAT_UNKNOWN,  CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED> m_subBufferPositions;
	SubBuffer<SParticleAxes,        DXGI_FORMAT_UNKNOWN,  CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED> m_subBufferAxes;
	SubBuffer<SParticleColorST,     DXGI_FORMAT_UNKNOWN,  CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED> m_subBufferColors;

	stream_handle_t                    m_spriteIndexBufferHandle = 0;
	const CDeviceInputStream*          m_spriteIndexBufferStream = nullptr;
	TDeviceFences                      m_fences;
	uint                               m_maxSpriteCount = 0;
	bool                               m_valid = false;
};
