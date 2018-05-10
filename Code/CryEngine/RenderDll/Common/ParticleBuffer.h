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
	uint    GetStride() const { return m_stride; }

	const CGpuBuffer&         GetBuffer() const { return m_buffer; }
	const CDeviceInputStream* GetStream() const { return m_stream; }

private:
	stream_handle_t           m_handle;
	const CDeviceInputStream* m_stream;
	CGpuBuffer                m_buffer;
	byte*                     m_pLockedData;
	uint                      m_elemCount;
	uint                      m_stride;
	uint                      m_flags;
};

class CParticleBufferSet
{
private:
	typedef std::array<CParticleSubBuffer, CREParticle::numBuffers> TDynBuffer;
	typedef std::array<DeviceFenceHandle, CREParticle::numBuffers>  TDeviceFences;

	struct SubBuffer
	{
		TDynBuffer m_buffers;
		// During writing, we only expose the base Video Memory pointer
		// and the offsets to the next free memory in this buffer
		byte*  m_pMemoryBase[CREParticle::numBuffers];
		uint32 m_offset[CREParticle::numBuffers];
		uint32 m_availableMemory;
	};

public:
	enum EBufferTypes
	{
		EBT_Vertices,
		EBT_Indices,
		EBT_PositionsSRV,
		EBT_AxesSRV,
		EBT_ColorSTsSRV,
		EBT_Total,
	};

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

	void Create(uint poolSize);
	void Release();
	uint GetMaxNumSprites() const { return m_maxSpriteCount; }

	void Lock();
	void Unlock();
	void SetFence();
	void WaitForFence();
	bool IsValid() const;

	uint GetAllocId() const;
	uint GetBindId() const;
	void Alloc(uint index, EBufferTypes type, uint numElems, SAlloc* pAllocOut);
	void Alloc(uint index, uint numElems, SAllocStreams* pAllocOut);

	const CDeviceInputStream* GetSpriteIndexBuffer() const { return m_spriteIndexBufferStream; }
	const CGpuBuffer&         GetPositionStream() const;
	const CGpuBuffer&         GetAxesStream() const;
	const CGpuBuffer&         GetColorSTsStream() const;
	const CDeviceInputStream* GetVertexStream() const;
	const CDeviceInputStream* GetIndexStream() const;

private:
	void                      CreateSubBuffer(EBufferTypes type, uint elemsCount, uint stride, DXGI_FORMAT format, uint32 flags);
	void                      CreateSpriteBuffer(uint poolSize);
	const CGpuBuffer&         GetGpuBuffer(uint index) const;
	const CDeviceInputStream* GetStreamBuffer(uint index) const;

private:
	std::array<SubBuffer, EBT_Total>   m_subBuffers;
	stream_handle_t                    m_spriteIndexBufferHandle = 0;
	const CDeviceInputStream*          m_spriteIndexBufferStream = nullptr;
	TDeviceFences                      m_fences;
	uint                               m_maxSpriteCount = 0;
	uint                               m_ids[RT_COMMAND_BUF_COUNT] = {};
	bool                               m_valid = false;
};
