// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define DEVICE_MANAGER_USE_TYPE_DELEGATES 1

#if CRY_PLATFORM_ORBIS && !CRY_RENDERER_GNM
	#define DEVICE_MANAGER_IMMEDIATE_STATE_WRITE 1
#else
	#define DEVICE_MANAGER_IMMEDIATE_STATE_WRITE 0
#endif

class CDeviceTexture;
class CDeviceVertexBuffer;
class CDeviceIndexBuffer;
class CCryDeviceContextWrapper;
class CConstantBuffer;

//===============================================================================================================

class CSubmissionQueue_DX11
{
	friend class CCryDX12DeviceContext;

public:
	enum SHADER_TYPE
	{
		TYPE_VS = 0,
		TYPE_PS,
		TYPE_GS,
		TYPE_DS,
		TYPE_HS,
		TYPE_CS,
		MAX_TYPE,
	};

	static_assert(
		int(eHWSC_Vertex  ) == int(TYPE_VS) &&
		int(eHWSC_Pixel   ) == int(TYPE_PS) &&
		int(eHWSC_Geometry) == int(TYPE_GS) &&
		int(eHWSC_Domain  ) == int(TYPE_DS) &&
		int(eHWSC_Hull    ) == int(TYPE_HS) &&
		int(eHWSC_Compute ) == int(TYPE_CS),
		"SHADER_TYPE enumeration should match EHWShaderClass for performance reasons");

	enum
	{
		MAX_BOUND_CB       = 16,
		MAX_BOUND_VBS      = 16,
		MAX_BOUND_SRVS     = 128,
		MAX_BOUND_UAVS     = 64,
		MAX_BOUND_SAMPLERS = 16,
		MAX_SRV_DIRTY      = MAX_BOUND_SRVS / 32,
		MAX_UAV_DIRTY      = MAX_BOUND_UAVS / 32,
		SRV_DIRTY_SHIFT    = 5,
		SRV_DIRTY_MASK     = 31,
		UAV_DIRTY_SHIFT    = 5,
		UAV_DIRTY_MASK     = 31
	};

private:
	friend class CD3DRenderer;

private:

#if !DEVICE_MANAGER_IMMEDIATE_STATE_WRITE
	// Constant Buffers
	struct
	{
		D3DBuffer* buffers[MAX_BOUND_CB];
		D3DBuffer* buffers1[MAX_BOUND_CB];
		uint32     offsets[MAX_BOUND_CB];
		uint32     sizes[MAX_BOUND_CB];
		uint32     dirty;
		uint32     dirty1;
	} m_CB[MAX_TYPE];

	// Shader Resource Views
	struct
	{
		D3DShaderResource* views[MAX_BOUND_SRVS];
		uint32             dirty[MAX_SRV_DIRTY];
	} m_SRV[MAX_TYPE];

	// Unordered Access Views
	struct
	{
		D3DUAV* views[MAX_BOUND_UAVS];
		uint32  counts[MAX_BOUND_UAVS];
		uint32  dirty[MAX_UAV_DIRTY];
	} m_UAV[MAX_TYPE];

	// SamplerStates
	struct
	{
		D3DSamplerState* samplers[MAX_BOUND_SAMPLERS];
		uint32           dirty;
	} m_Samplers[MAX_TYPE];

	// VertexBuffers
	struct
	{
		D3DBuffer* buffers[MAX_BOUND_VBS];
		uint32     offsets[MAX_BOUND_VBS];
		uint32     strides[MAX_BOUND_VBS];
		uint32     dirty;
	} m_VBs;

	// IndexBuffer
	struct
	{
		D3DBuffer* buffer;
		uint32     offset;
		int        format;
		uint32     dirty;
	} m_IB;

	struct
	{
		D3DVertexDeclaration* decl;
		bool                  dirty;
	} m_VertexDecl;

	struct
	{
		D3D11_PRIMITIVE_TOPOLOGY topology;
		bool                     dirty;
	} m_Topology;

	struct
	{
		ID3D11DepthStencilState* dss;
		uint32                   stencilref;
		bool                     dirty;
	} m_DepthStencilState;

	struct
	{
		ID3D11BlendState* pBlendState;
		float             BlendFactor[4];
		uint32            SampleMask;
		bool              dirty;
	} m_BlendState;

	struct
	{
		ID3D11RasterizerState* pRasterizerState;
		bool                   dirty;
	} m_RasterState;

	struct
	{
		ID3D11Resource* shader;
		bool            dirty;
	} m_Shaders[MAX_TYPE];
#endif

	uint32 m_numInvalidDrawcalls;

#if DURANGO_ENABLE_ASYNC_DIPS
	// counter to cap the number of device calls which are executed as a AsyncDIP Job
	uint32 m_nAsyncDipJobCounter;

	void BindConstantBuffersAsync(SHADER_TYPE type);
	void BindOffsetConstantBuffersAsync(SHADER_TYPE type);
	void BindSRVsAsync(SHADER_TYPE type);
	void BindUAVsAsync(SHADER_TYPE type);
	void BindSamplersAsync(SHADER_TYPE type);
	void BindIAAsync();
	void BindShaderAsync(SHADER_TYPE type);
	void BindStateAsync();
	void CommitDeviceStatesAsync();
#endif

	void BindConstantBuffers(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindOffsetConstantBuffers(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindSRVs(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindUAVs(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindSamplers(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindShader(SHADER_TYPE type, CCryDeviceContextWrapper& rDeviceContext);
	void BindState(CCryDeviceContextWrapper& rDeviceContext);
	void BindIA(CCryDeviceContextWrapper& rDeviceContext);

	bool ValidateDrawcall();

public:
	CSubmissionQueue_DX11();
	~CSubmissionQueue_DX11();

	void           Init();
	void           ClearState();
	void           RT_Tick();

	inline void    BindConstantBuffer(SHADER_TYPE type, const CConstantBuffer* Buffer, uint32 slot);
	inline void    BindConstantBuffer(SHADER_TYPE type, const CConstantBuffer* Buffer, uint32 slot, uint32 offset, uint32 size);
	inline void    BindCBs(SHADER_TYPE type, const CConstantBuffer* const* Buffer, uint32 start_slot, uint32 count);
	inline void    BindConstantBuffer(SHADER_TYPE type, D3DBuffer* Buffer, uint32 slot);
	inline void    BindConstantBuffer(SHADER_TYPE type, D3DBuffer* Buffer, uint32 slot, uint32 offset, uint32 size);
	inline void    BindCBs(SHADER_TYPE type, D3DBuffer* const* Buffer, uint32 start_slot, uint32 count);
	inline void    BindSRV(SHADER_TYPE type, D3DShaderResource* SRV, uint32 slot);
	inline void    BindSRV(SHADER_TYPE type, D3DShaderResource* const* SRV, uint32 start_slot, uint32 count);
	inline void    BindUAV(SHADER_TYPE type, D3DUAV* UAV, uint32 counts, uint32 slot);
	inline void    BindUAV(SHADER_TYPE type, D3DUAV* const* UAV, const uint32* counts, uint32 start_slot, uint32 count);
	inline void    BindSampler(SHADER_TYPE type, D3DSamplerState* Sampler, uint32 slot);
	inline void    BindSampler(SHADER_TYPE type, D3DSamplerState* const* Samplers, uint32 start_slot, uint32 count);
	inline void    BindVB(D3DBuffer* VB, uint32 slot, uint32 offset, uint32 stride);
	inline void    BindVB(uint32 start, uint32 count, D3DBuffer* const* Buffers, const uint32* offset, const uint32* stride);
	inline void    BindIB(D3DBuffer* Buffer, uint32 offset, DXGI_FORMAT format);
	inline void    BindVtxDecl(D3DVertexDeclaration* decl);
	inline void    BindTopology(D3D11_PRIMITIVE_TOPOLOGY top);
	inline void    BindShader(SHADER_TYPE type, ID3D11Resource* shader);
	inline void    SetDepthStencilState(ID3D11DepthStencilState* dss, uint32 stencilref);
	inline void    SetBlendState(ID3D11BlendState* pBlendState, float* BlendFactor, uint32 SampleMask);
	inline void    SetRasterState(ID3D11RasterizerState* pRasterizerState);

	void           CommitDeviceStates();
	void           Draw(uint32 nVerticesCount, uint32 nStartVertex);
	void           DrawInstanced(uint32 nInstanceVerts, uint32 nInsts, uint32 nStartVertex, uint32 nStartInstance);
	void           DrawIndexed(uint32, uint32, uint32);
	void           DrawIndexedInstanced(uint32 numIndices, uint32 nInsts, uint32 startIndex, uint32 v0, uint32 v1);
	void           DrawIndexedInstancedIndirect(ID3D11Buffer*, uint32);
	void           Dispatch(uint32, uint32, uint32);
	void           DispatchIndirect(ID3D11Buffer*, uint32);

	uint32 GetNumInvalidDrawcalls()
	{
		return m_numInvalidDrawcalls;
	}

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
private:
	#if DURANGO_ENABLE_ASYNC_DIPS
	// fields for the AsyncDIP Job queue
	enum { nQueueSize = 2048 }; // size of the queue to store the job parameters

	// submit new AsyncDIP packages to the queue
	void SubmitAsyncDipJob(char* start, char* end, volatile int* nSyncState);

	// struct to define the job parameters
	struct SParam
	{
		char*         start;
		char*         end;
		volatile int* nSyncState;
	};

	CRY_ALIGN(64) uint32 m_nPush;
	CRY_ALIGN(64) uint32 m_nPull;
	CRY_ALIGN(64) SParam m_arrParams[nQueueSize];
public:

	// execute async dip jobs (should be called by the job system during idle time)
	void ExecuteAsyncDIP();
	#endif // DURANGO_ENABLE_ASYNC_DIPS
#endif   // CRY_PLATFORM_DURANGO
};

#include "DeviceSubmissionQueue_D3D11.inl"

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
	#include "DeviceSubmissionQueue_D3D11_Durango.inl"
#endif
