// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Note: temporary solution, this should be removed as soon as the device
// layer for Durango is available
#if BUFFER_USE_STAGED_UPDATES == 0
namespace detail
{
	template<> inline void safe_release<ID3D11Buffer>(ID3D11Buffer*& ptr)
	{
		if (ptr)
		{
			void* buffer_ptr = CDeviceObjectFactory::GetBackingStorage(ptr);
			if (ptr->Release() == 0ul)
			{
				CDeviceObjectFactory::FreeBackingStorage(buffer_ptr);
			}
			ptr = NULL;
		}
	}
}
#endif

#if DURANGO_ENABLE_ASYNC_DIPS
	#include "../../DriverD3D.h"
	#include "DeviceWrapper_D3D11.h" // CCryDeviceContextWrapper
#endif

namespace
{

#if DURANGO_ENABLE_ASYNC_DIPS
struct SCommandBuffer
{
	enum { COMMAND_BUFFER_SIZE = 8 << 20 };

	CRY_ALIGN(64) char m_buffer[COMMAND_BUFFER_SIZE];
	uint32 m_current_offset;

	SCommandBuffer()
	{
		memset(this, 0, sizeof(*this));
	}

	char* Allocate(uint32 size)
	{
		if (m_current_offset + size >= COMMAND_BUFFER_SIZE)
		{
			__debugbreak();
		}
		char* result = &m_buffer[m_current_offset];
		m_current_offset += size;
		return result;
	}
};
static SCommandBuffer s_CommandBuffer;

enum JobType
{
	JT_BIND_CB = 0,
	JT_BIND_CB_OFFSET,
	JT_BIND_SAMPLER,
	JT_BIND_SRV,
	JT_BIND_UAV,
	JT_BIND_VB,
	JT_BIND_IB,
	JT_BIND_TOPOLOGY,
	JT_BIND_VTX_DECL,
	JT_BIND_SHADER,
	JT_SET_RASTERSTATE,
	JT_SET_DEPTHSTATE,
	JT_SET_BLENDSTATE,
	JT_FLUSH_GPU_CACHE,
	JT_FLUSH_GPU_CACHE_RANGE,
	JT_DRAW,
	JT_DRAW_INSTANCED,
	JT_DRAW_INDEXED,
	JT_DRAW_INDEXED_INSTANCED,
};

struct SJob
{
	int32 type;

	SJob(int type)
		: type(type)
	{}

	template<typename Type>
	void Process(char*& iter)
	{
		Type* job = static_cast<Type*>(this);
		iter += job->Process();
	}
};

struct SRasterState : public SJob
{
	ID3D11RasterizerState* pRasterizerState;

	SRasterState()
		: SJob(JT_SET_RASTERSTATE)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().RSSetState(pRasterizerState);
		return sizeof(*this);
	}
};

struct SDepthState : public SJob
{
	ID3D11DepthStencilState* dss;
	uint32                   stencilref;

	SDepthState()
		: SJob(JT_SET_DEPTHSTATE)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().OMSetDepthStencilState(dss, stencilref);
		return sizeof(*this);
	}
};

struct SBlendState : public SJob
{
	ID3D11BlendState* pBlendState;
	float             BlendFactor[4];
	uint32            SampleMask;

	SBlendState()
		: SJob(JT_SET_BLENDSTATE)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().OMSetBlendState(pBlendState, BlendFactor, SampleMask);
		return sizeof(*this);
	}
};

struct SBindShader : public SJob
{
	ID3D11Resource* Shader;
	int32           stage;

	SBindShader()
		: SJob(JT_BIND_SHADER)
	{}

	size_t Process()
	{
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().VSSetShader((D3DVertexShader*)Shader, NULL, 0);
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().PSSetShader((D3DPixelShader*)Shader, NULL, 0);
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().HSSetShader((ID3D11HullShader*)Shader, NULL, 0);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().GSSetShader((ID3D11GeometryShader*)Shader, NULL, 0);
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().DSSetShader((ID3D11DomainShader*)Shader, NULL, 0);
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetShader((ID3D11ComputeShader*)Shader, NULL, 0);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		return sizeof(*this);
	}
};

struct SBindCB : public SJob
{
	D3DBuffer** Buffers;
	int32       stage;
	uint32      base;
	uint32      count;

	SBindCB()
		: SJob(JT_BIND_CB)
	{}

	size_t Process()
	{
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().VSSetConstantBuffers(base, count, Buffers);
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().PSSetConstantBuffers(base, count, Buffers);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().GSSetConstantBuffers(base, count, Buffers);
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().DSSetConstantBuffers(base, count, Buffers);
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().HSSetConstantBuffers(base, count, Buffers);
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetConstantBuffers(base, count, Buffers);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
		return sizeof(*this) + sizeof(Buffers[0]) * count;
	}
};

struct SBindOffsetCB : public SJob
{
	D3DBuffer** Buffers;
	uint32*     Offsets;
	uint32*     Counts;
	int32       stage;
	uint32      base;
	uint32      count;

	SBindOffsetCB()
		: SJob(JT_BIND_CB_OFFSET)
	{}

	size_t Process()
	{
	#if (CRY_RENDERER_DIRECT3D >= 110)
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().VSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().PSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().GSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().DSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().HSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetConstantBuffers1(base, count, Buffers, Offsets, Counts);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
	#else
		CryFatalError("D3D11.1 API functions are not supported on this platform");
	#endif
		return sizeof(*this) + sizeof(Buffers[0]) * count + sizeof(Offsets[0]) * count + sizeof(Counts[0]) * count;
	}
};

struct SBindSampler : public SJob
{
	D3DSamplerState** Samplers;
	int32             stage;
	uint32            base;
	uint32            count;

	SBindSampler()
		: SJob(JT_BIND_SAMPLER)
	{}

	size_t Process()
	{
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().VSSetSamplers(base, count, Samplers);
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().PSSetSamplers(base, count, Samplers);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().GSSetSamplers(base, count, Samplers);
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().DSSetSamplers(base, count, Samplers);
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().HSSetSamplers(base, count, Samplers);
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetSamplers(base, count, Samplers);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
		return sizeof(*this) + sizeof(Samplers[0]) * count;
	}
};

struct SBindSRV : public SJob
{
	D3DShaderResource** SRVs;
	int32               stage;
	uint32              base;
	uint32              count;

	SBindSRV()
		: SJob(JT_BIND_SRV)
	{}

	size_t Process()
	{
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().VSSetShaderResources(base, count, SRVs);
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().PSSetShaderResources(base, count, SRVs);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().GSSetShaderResources(base, count, SRVs);
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().DSSetShaderResources(base, count, SRVs);
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().HSSetShaderResources(base, count, SRVs);
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetShaderResources(base, count, SRVs);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
		return sizeof(*this) + sizeof(SRVs[0]) * count;
	}
};

struct SBindUAV : public SJob
{
	D3DUAV** UAVs;
	uint32*  counts;
	int32    stage;
	uint32   base;
	uint32   count;

	SBindUAV()
		: SJob(JT_BIND_UAV)
	{}

	size_t Process()
	{
		switch (stage)
		{
		case CSubmissionQueue_DX11::TYPE_VS:
			assert(0 && "NOT IMPLEMENTED ON D3D11.0");
			break;
		case CSubmissionQueue_DX11::TYPE_PS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, NULL, NULL, base, count, UAVs, counts);
			break;
		case CSubmissionQueue_DX11::TYPE_GS:
			assert(0 && "NOT IMPLEMENTED ON D3D11.0");
			break;
		case CSubmissionQueue_DX11::TYPE_DS:
			assert(0 && "NOT IMPLEMENTED ON D3D11.0");
			break;
		case CSubmissionQueue_DX11::TYPE_HS:
			assert(0 && "NOT IMPLEMENTED ON D3D11.0");
			break;
		case CSubmissionQueue_DX11::TYPE_CS:
			gcpRendD3D->GetDeviceContext_Unsynchronized().CSSetUnorderedAccessViews(base, count, UAVs, counts);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
		return sizeof(*this) + sizeof(UAVs[0]) * count + sizeof(counts[0]) * count;
	}
};

struct SBindVB : public SJob
{
	D3DBuffer** Buffers;
	uint32*     Strides;
	uint32*     Offsets;
	uint32      base;
	uint32      count;

	SBindVB()
		: SJob(JT_BIND_VB)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().IASetVertexBuffers(base, count, Buffers, Strides, Offsets);
		return sizeof(*this) + sizeof(Buffers[0]) * count + sizeof(Strides[0]) * count + sizeof(Offsets[0]) * count;
	}
};

struct SBindIB : public SJob
{
	D3DBuffer* Buffer;
	int32      format;
	uint32     offset;

	SBindIB()
		: SJob(JT_BIND_IB)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().IASetIndexBuffer(Buffer, (DXGI_FORMAT)format, offset);
		return sizeof(*this);
	}
};

struct SBindTopology : public SJob
{
	int32 topology;

	SBindTopology()
		: SJob(JT_BIND_TOPOLOGY)
	{}

	NO_INLINE size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)topology);
		return sizeof(*this);
	}
};

struct SBindVtxDecl : public SJob
{
	D3DVertexDeclaration* Decl;

	SBindVtxDecl()
		: SJob(JT_BIND_VTX_DECL)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().IASetInputLayout(Decl);
		return sizeof(*this);
	}
};

struct SFlushGPUCache : public SJob
{
	D3DBuffer* Buffer;

	SFlushGPUCache()
		: SJob(JT_FLUSH_GPU_CACHE)
	{}

	size_t Process()
	{
	#if CRY_PLATFORM_DURANGO
		gcpRendD3D->GetPerformanceDeviceContext_Unsynchronized().FlushGpuCaches(Buffer);
	#endif
		return sizeof(*this);
	}
};

struct SFlushGPUCacheRange : public SJob
{
	UINT   Flags;
	void*  pBaseAddress;
	SIZE_T SizeInBytes;

	SFlushGPUCacheRange()
		: SJob(JT_FLUSH_GPU_CACHE_RANGE)
	{}

	size_t Process()
	{
	#if CRY_PLATFORM_DURANGO
		gcpRendD3D->GetPerformanceDeviceContext_Unsynchronized().FlushGpuCacheRange(
		  Flags, pBaseAddress, SizeInBytes);
	#endif
		return sizeof(*this);
	}
};

struct SDraw : public SJob
{
	uint32 nVerticesCount;
	uint32 nStartVertex;

	SDraw()
		: SJob(JT_DRAW)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().Draw(nVerticesCount, nStartVertex);
		return sizeof(*this);
	}
};

struct SDrawInstanced : public SJob
{
	uint32 nInstanceVerts;
	uint32 nInstanceCount;
	uint32 nStartVertex;
	uint32 nStartInstance;

	SDrawInstanced()
		: SJob(JT_DRAW_INSTANCED)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().DrawInstanced(nInstanceVerts, nInstanceCount, nStartVertex, nStartInstance);
		return sizeof(*this);
	}
};

struct SDrawIndexed : public SJob
{
	uint32 NumIndices;
	uint32 nIndexOffset;
	uint32 nVertexOffset;

	SDrawIndexed()
		: SJob(JT_DRAW_INDEXED)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().DrawIndexed(NumIndices, nIndexOffset, nVertexOffset);
		return sizeof(*this);
	}
};

struct SDrawIndexedInstanced : public SJob
{
	uint32 numIndices;
	uint32 nInsts;
	uint32 startIndex;
	uint32 v0;
	uint32 v1;

	SDrawIndexedInstanced()
		: SJob(JT_DRAW_INDEXED_INSTANCED)
	{}

	size_t Process()
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().DrawIndexedInstanced(numIndices, nInsts, startIndex, v0, v1);
		return sizeof(*this);
	}
};

static void ExecuteAsyncDIPJob(char* iter, char* end, volatile int* finished)
{
	while (iter < end)
	{
		SJob* job = reinterpret_cast<SJob*>(iter);
		switch (job->type)
		{
		case JT_BIND_CB:
			job->Process<SBindCB>(iter);
			break;
		case JT_BIND_CB_OFFSET:
			job->Process<SBindOffsetCB>(iter);
			break;
		case JT_BIND_SAMPLER:
			job->Process<SBindSampler>(iter);
			break;
		case JT_BIND_SRV:
			job->Process<SBindSRV>(iter);
			break;
		case JT_BIND_UAV:
			job->Process<SBindUAV>(iter);
			break;
		case JT_BIND_VB:
			job->Process<SBindVB>(iter);
			break;
		case JT_BIND_IB:
			job->Process<SBindIB>(iter);
			break;
		case JT_BIND_TOPOLOGY:
			job->Process<SBindTopology>(iter);
			break;
		case JT_BIND_VTX_DECL:
			job->Process<SBindVtxDecl>(iter);
			break;
		case JT_BIND_SHADER:
			job->Process<SBindShader>(iter);
			break;
		case JT_SET_RASTERSTATE:
			job->Process<SRasterState>(iter);
			break;
		case JT_SET_DEPTHSTATE:
			job->Process<SDepthState>(iter);
			break;
		case JT_SET_BLENDSTATE:
			job->Process<SBlendState>(iter);
			break;
		case JT_FLUSH_GPU_CACHE:
			job->Process<SFlushGPUCache>(iter);
			break;
		case JT_FLUSH_GPU_CACHE_RANGE:
			job->Process<SFlushGPUCacheRange>(iter);
			break;
		case JT_DRAW:
			job->Process<SDraw>(iter);
			break;
		case JT_DRAW_INSTANCED:
			job->Process<SDrawInstanced>(iter);
			break;
		case JT_DRAW_INDEXED:
			job->Process<SDrawIndexed>(iter);
			break;
		case JT_DRAW_INDEXED_INSTANCED:
			job->Process<SDrawIndexedInstanced>(iter);
			break;
		default:
			CRY_ASSERT(0);
			break;
		}
		;
	}
	CryInterlockedDecrement(finished);
}
#endif
}

#if DURANGO_ENABLE_ASYNC_DIPS
	#if !CRY_PLATFORM_DURANGO
		DECLARE_JOB("AsyncDIP", TExecuteAsyncDIP, ExecuteAsyncDIPJob);
		typedef PROD_CONS_QUEUE_TYPE (TExecuteAsyncDIP, 4096) TAsyncDIPQueue;
		typedef TExecuteAsyncDIP::packet TExecuteAsyncDIPPacket;
		static TAsyncDIPQueue* s_dip_queue = NULL;
	#endif
#endif

#if DURANGO_ENABLE_ASYNC_DIPS
inline void CSubmissionQueue_DX11::BindConstantBuffersAsync(SHADER_TYPE type)
{
	using namespace DevManUtil;
	while (m_CB[type].dirty)
	{
		const unsigned lsb = m_CB[type].dirty & ~(m_CB[type].dirty - 1);
		const unsigned lbit = bsr(lsb);
		const unsigned hbit = bsf(~(m_CB[type].dirty) & ~(lsb - 1));
		const unsigned base = lbit;
		const unsigned count = hbit - lbit;

		SBindCB* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindCB)))SBindCB;
		pJob->stage = type;
		pJob->base = base;
		pJob->count = count;
		pJob->Buffers = (D3DBuffer**)s_CommandBuffer.Allocate(sizeof(m_CB[type].buffers[0]) * count);
		for (size_t i = 0; i < count; ++i)
		{
			pJob->Buffers[i] = m_CB[type].buffers[base + i];
		}

		const signed mask = iszero(32 - (signed)hbit) - 1;
		m_CB[type].dirty &= (~((1 << hbit) - (1 << lbit)) & mask);
	}
}

inline void CSubmissionQueue_DX11::BindOffsetConstantBuffersAsync(SHADER_TYPE type)
{
	using namespace DevManUtil;

	while (m_CB[type].dirty1)
	{
		const unsigned lsb = m_CB[type].dirty1 & ~(m_CB[type].dirty1 - 1);
		const unsigned lbit = bsr(lsb);
		const unsigned hbit = bsf(~(m_CB[type].dirty1) & ~(lsb - 1));
		const unsigned base = lbit;
		const unsigned count = hbit - lbit;

		SBindOffsetCB* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindOffsetCB)))SBindOffsetCB;
		pJob->stage = type;
		pJob->base = base;
		pJob->count = count;
		pJob->Buffers = (D3DBuffer**)s_CommandBuffer.Allocate(sizeof(m_CB[type].buffers1[0]) * count);
		pJob->Offsets = (uint32*)s_CommandBuffer.Allocate(sizeof(m_CB[type].offsets[0]) * count);
		pJob->Counts = (uint32*)s_CommandBuffer.Allocate(sizeof(m_CB[type].sizes[0]) * count);
		for (size_t i = 0; i < count; ++i)
		{
			pJob->Buffers[i] = m_CB[type].buffers1[base + i];
			pJob->Offsets[i] = m_CB[type].offsets[base + i];
			pJob->Counts[i] = m_CB[type].sizes[base + i];
		}

		const signed mask = iszero(32 - (signed)hbit) - 1;
		m_CB[type].dirty1 &= (~((1 << hbit) - (1 << lbit)) & mask);
	}
}

inline void CSubmissionQueue_DX11::BindSamplersAsync(SHADER_TYPE type)
{
	using namespace DevManUtil;
	while (m_Samplers[type].dirty)
	{
		const unsigned lsb = m_Samplers[type].dirty & ~(m_Samplers[type].dirty - 1);
		const unsigned lbit = bsr(lsb);
		const unsigned hbit = bsf(~(m_Samplers[type].dirty) & ~(lsb - 1));
		const unsigned base = lbit;
		const unsigned count = hbit - lbit;

		SBindSampler* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindSampler)))SBindSampler;
		pJob->stage = type;
		pJob->base = base;
		pJob->count = count;
		pJob->Samplers = (D3DSamplerState**)s_CommandBuffer.Allocate(sizeof(m_Samplers[type].samplers[0]) * count);
		for (size_t i = 0; i < count; ++i)
		{
			pJob->Samplers[i] = m_Samplers[type].samplers[base + i];
		}

		const signed mask = iszero(32 - (signed)hbit) - 1;
		m_Samplers[type].dirty &= (~((1 << hbit) - (1 << lbit)) & mask);
	}
}

inline void CSubmissionQueue_DX11::BindSRVsAsync(SHADER_TYPE type)
{
	using namespace DevManUtil;
	for (size_t j = 0; j < MAX_SRV_DIRTY; ++j)
	{
		while (m_SRV[type].dirty[j])
		{
			const unsigned lsb = m_SRV[type].dirty[j] & ~(m_SRV[type].dirty[j] - 1);
			const unsigned lbit = bsr(lsb);
			const unsigned hbit = bsf(~(m_SRV[type].dirty[j]) & ~(lsb - 1));
			const unsigned base = j * 32 + lbit;
			const unsigned count = hbit - lbit;

			SBindSRV* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindSRV)))SBindSRV;
			pJob->stage = type;
			pJob->base = base;
			pJob->count = count;
			pJob->SRVs = (D3DShaderResource**)s_CommandBuffer.Allocate(sizeof(m_SRV[type].views[0]) * count);
			for (size_t i = 0; i < count; ++i)
			{
				pJob->SRVs[i] = m_SRV[type].views[base + i];
			}

			const signed mask = iszero(32 - (signed)hbit) - 1;
			m_SRV[type].dirty[j] &= (~((1 << hbit) - (1 << lbit)) & mask);
		}
	}
}

inline void CSubmissionQueue_DX11::BindUAVsAsync(SHADER_TYPE type)
{
	using namespace DevManUtil;
	for (size_t j = 0; j < MAX_UAV_DIRTY; ++j)
	{
		while (m_UAV[type].dirty[j])
		{
			const unsigned lsb = m_UAV[type].dirty[j] & ~(m_UAV[type].dirty[j] - 1);
			const unsigned lbit = bsr(lsb);
			const unsigned hbit = bsf(~(m_UAV[type].dirty[j]) & ~(lsb - 1));
			const unsigned base = j * 32 + lbit;
			const unsigned count = hbit - lbit;

			SBindUAV* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindUAV)))SBindUAV;
			pJob->stage = type;
			pJob->base = base;
			pJob->count = count;
			pJob->UAVs = (D3DUAV**)s_CommandBuffer.Allocate(sizeof(m_UAV[type].views[0]) * count);
			pJob->counts = (uint32*)s_CommandBuffer.Allocate(sizeof(m_UAV[type].counts[0]) * count);
			for (size_t i = 0; i < count; ++i)
			{
				pJob->UAVs[i] = m_UAV[type].views[base + i];
				pJob->counts[i] = m_UAV[type].counts[base + i];
			}

			const signed mask = iszero(32 - (signed)hbit) - 1;
			m_UAV[type].dirty[j] &= (~((1 << hbit) - (1 << lbit)) & mask);
		}
	}
}

inline void CSubmissionQueue_DX11::BindShaderAsync(SHADER_TYPE type)
{
	if (m_Shaders[type].dirty)
	{
		SBindShader* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindShader)))SBindShader;
		pJob->stage = type;
		pJob->Shader = m_Shaders[type].shader;
		m_Shaders[type].dirty = false;
	}
}

inline void CSubmissionQueue_DX11::BindStateAsync()
{
	if (m_RasterState.dirty)
	{
		SRasterState* pJob = new(s_CommandBuffer.Allocate(sizeof(SRasterState)))SRasterState;
		pJob->pRasterizerState = m_RasterState.pRasterizerState;
		m_RasterState.dirty = false;
	}
	if (m_BlendState.dirty)
	{
		SBlendState* pJob = new(s_CommandBuffer.Allocate(sizeof(SBlendState)))SBlendState;
		pJob->pBlendState = m_BlendState.pBlendState;
		for (size_t i = 0; i < 4; ++i)
		{
			pJob->BlendFactor[i] = m_BlendState.BlendFactor[i];
		}
		pJob->SampleMask = m_BlendState.SampleMask;
		m_BlendState.dirty = false;
	}
	if (m_DepthStencilState.dirty)
	{
		SDepthState* pJob = new(s_CommandBuffer.Allocate(sizeof(SDepthState)))SDepthState;
		pJob->dss = m_DepthStencilState.dss;
		pJob->stencilref = m_DepthStencilState.stencilref;
		m_DepthStencilState.dirty = false;
	}
}

inline void CSubmissionQueue_DX11::BindIAAsync()
{
	using namespace DevManUtil;
	while (m_VBs.dirty)
	{
		const unsigned lsb = m_VBs.dirty & ~(m_VBs.dirty - 1);
		const unsigned lbit = bsr(lsb);
		const unsigned hbit = bsf(~(m_VBs.dirty) & ~(lsb - 1));
		const unsigned base = lbit;
		const unsigned count = hbit - lbit;

		SBindVB* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindVB)))SBindVB;
		pJob->base = base;
		pJob->count = count;
		pJob->Buffers = (D3DBuffer**)s_CommandBuffer.Allocate(sizeof(m_VBs.buffers[0]) * count);
		pJob->Strides = (uint32*)s_CommandBuffer.Allocate(sizeof(m_VBs.strides[0]) * count);
		pJob->Offsets = (uint32*)s_CommandBuffer.Allocate(sizeof(m_VBs.offsets[0]) * count);
		for (size_t i = 0; i < count; ++i)
		{
			pJob->Buffers[i] = m_VBs.buffers[base + i];
			pJob->Strides[i] = m_VBs.strides[base + i];
			pJob->Offsets[i] = m_VBs.offsets[base + i];
		}

		const signed mask = iszero(32 - (signed)hbit) - 1;
		m_VBs.dirty &= (~((1 << hbit) - (1 << lbit)) & mask);
	}
	if (m_IB.dirty)
	{
		SBindIB* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindIB)))SBindIB;
		pJob->Buffer = m_IB.buffer;
		pJob->format = m_IB.format;
		pJob->offset = m_IB.offset;
		m_IB.dirty = 0;
	}
	if (m_VertexDecl.dirty)
	{
		SBindVtxDecl* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindVtxDecl)))SBindVtxDecl;
		pJob->Decl = m_VertexDecl.decl;
		m_VertexDecl.dirty = false;
	}
	if (m_Topology.dirty)
	{
		SBindTopology* pJob = new(s_CommandBuffer.Allocate(sizeof(SBindTopology)))SBindTopology;
		pJob->topology = m_Topology.topology;
		m_Topology.dirty = false;
	}
}

inline void CSubmissionQueue_DX11::CommitDeviceStatesAsync()
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	DETAILED_PROFILE_MARKER("CommitDeviceStatesAsyncAsync");

	#if defined(BUFFER_ENABLE_DIRECT_ACCESS) && (CRY_RENDERER_DIRECT3D == 110) && !CRY_RENDERER_GNM
	{
		DETAILED_PROFILE_MARKER("InvalidateBuffers");
		const uint32 threadid = gRenDev->m_RP.m_nProcessThreadID;
		CDeviceObjectFactory::BufferInvalidationsT& invalidated = GetDeviceObjectFactory().m_buffer_invalidations[threadid];
		for (size_t i = 0, end = invalidated.size(); i < end; ++i)
		{
			SFlushGPUCacheRange* pJob = new(s_CommandBuffer.Allocate(sizeof(SFlushGPUCacheRange)))SFlushGPUCacheRange;
			pJob->Flags = 0;
			pJob->pBaseAddress = (void*)((uintptr_t)invalidated[i].base_ptr + invalidated[i].offset);
			pJob->SizeInBytes = invalidated[i].size;

		}
		invalidated.clear();
	}
	#endif

	{
		DETAILED_PROFILE_MARKER("BindDeviceResourcesAsync");
		BindIAAsync();
		BindStateAsync();
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindShaderAsync((SHADER_TYPE)i);
		}
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindConstantBuffersAsync((SHADER_TYPE)i);
		}
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindOffsetConstantBuffersAsync((SHADER_TYPE)i);
		}
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindSRVsAsync((SHADER_TYPE)i);
		}
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindUAVsAsync((SHADER_TYPE)i);
		}
		for (signed i = 0; i < MAX_TYPE; ++i)
		{
			BindSamplersAsync((SHADER_TYPE)i);
		}
	}
}

inline void CSubmissionQueue_DX11::SubmitAsyncDipJob(char* start, char* end, volatile int* nSyncState)
{
	// prevent overflow in the queue
	uint32 nFreeQueueSlots = std::abs((int)(*(volatile uint32*)(&m_nPull) - m_nPush));
	if (nFreeQueueSlots * 4 > nQueueSize * 3)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "AsyncDip Queue overflow detected; Please increase the queue size to prevent performance issues");
		while (nFreeQueueSlots * 4 > nQueueSize * 3)
		{
			CrySleep(0);
			nFreeQueueSlots = std::abs((int)(*(volatile uint32*)(&m_nPull) - m_nPush));
		}
	}

	// write job params to queue and flush the data
	uint32 nPushSlot = m_nPush & (nQueueSize - 1);
	m_arrParams[nPushSlot].start = start;
	m_arrParams[nPushSlot].end = end;
	m_arrParams[nPushSlot].nSyncState = nSyncState;
	MemoryBarrier();
	m_nPush += 1;
	MemoryBarrier();

	// signal job to start
	UAsyncDipState nCurrentState;
	UAsyncDipState nNewState;

	do
	{
		nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
		nNewState = nCurrentState;
		nNewState.nNumJobs += 1;

		if (nCurrentState.nWorker_Idle == 0)
		{
			break;    // all workers are busy
		}

		// check if a worker is currently working on the queue
		if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
		{
			return;    // added job to currently active worker
		}

	}
	while (true);

	// reaching this line means that all workers were busy
	//assert(CurrentState.nNumJobs == 0);
	ExecuteAsyncDIPJob(m_arrParams[m_nPull % nQueueSize].start, m_arrParams[m_nPull % nQueueSize].end, m_arrParams[m_nPull % nQueueSize].nSyncState);
	m_nPull += 1;
}

inline void CSubmissionQueue_DX11::ExecuteAsyncDIP()
{
	while (true)
	{
		// if we have a job (due the job counter), we can saftly assume that the
		// push index was incremented by the job submitting code
		assert((*(volatile uint32*)(&m_nPush) != m_nPull));

		// Execute next Job
		uint32 nPullSlot = m_nPull & (nQueueSize - 1);
		ExecuteAsyncDIPJob(m_arrParams[nPullSlot].start, m_arrParams[nPullSlot].end, m_arrParams[nPullSlot].nSyncState);
		m_nPull += 1;

		// Decrement JobCounter
		UAsyncDipState nCurrentState;
		UAsyncDipState nNewState;
		do
		{
			nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
			nNewState = nCurrentState;

			assert(nNewState.nNumJobs > 0);
			nNewState.nNumJobs -= 1;

			if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
			{
				break;
			}

		}
		while (true);

		// stop processing if we have run out of jobs
		if (nNewState.nNumJobs == 0)
		{
			break;
		}
	}
}
#endif   // DURANGO_ENABLE_ASYNC_DIPS == 1
