// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12GraphicsCommandList : public ID3D12GraphicsCommandList
{
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;

public:
	int m_RefCount;
	std::array<ID3D12GraphicsCommandList*, numTargets> m_Targets;
	ID3D12GraphicsCommandList* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12GraphicsCommandList<numTargets> self;
public:
	BroadcastableD3D12GraphicsCommandList(
	  _In_ ID3D12Device* pDevice,
	  _In_ UINT nodeMask,
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  _In_ ID3D12CommandAllocator* pCommandAllocator_,
	  _In_opt_ ID3D12PipelineState* pInitialState,
	  REFIID riid) : m_RefCount(1)
	{
		BroadcastableD3D12CommandAllocator<numTargets>* pCommandAllocator = (BroadcastableD3D12CommandAllocator<numTargets>*)pCommandAllocator_;

		DX12_ASSERT(nodeMask != 0, "0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;
			if (UINT nM = (nodeMask & (1 << i)))
			{
#if DX12_LINKEDADAPTER_SIMULATION
				// Always create on the first GPU, if running simulation
				if (CRenderer::CV_r_StereoEnableMgpu < 0)
					nM = 1;
#endif

				HRESULT ret = pDevice->CreateCommandList(
				  nM, type, *(*pCommandAllocator)[i], pInitialState, riid, (void**)&m_Targets[i]);
				DX12_ASSERT(ret == S_OK, "Failed to create graphics command list!");
			}
		}
	}

#define Pick(i, func)                           \
  m_Targets[i]->func;

#define Broadcast(func)                         \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
      if (m_Targets[i])                         \
        m_Targets[i]->func;                     \
  }                                             \
  else                                          \
  {                                             \
    m_Targets[0]->func;                         \
    m_Targets[1]->func;                         \
  }

#define Verify(func, message)                   \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
      DX12_ASSERT(func, message);               \
  }                                             \
  else                                          \
  {                                             \
    { int i = 0; DX12_ASSERT(func, message); }  \
    { int i = 1; DX12_ASSERT(func, message); }  \
  }

#define Parallelize(func)                       \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
      if (m_Targets[i])                         \
        m_Targets[i]->func;                     \
  }                                             \
  else                                          \
  {                                             \
    { int i = 0; m_Targets[i]->func; }          \
    { int i = 1; m_Targets[i]->func; }          \
  }

#define ParallelizeMapped(map, func)            \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
      if (m_Targets[i])                         \
        m_Targets[map(i)]->func;                \
  }                                             \
  else                                          \
  {                                             \
    { int i = 0; m_Targets[map(i)]->func; }     \
    { int i = 1; m_Targets[map(i)]->func; }     \
  }

#define BroadcastWithFail(func)                 \
  HRESULT ret;                                  \
                                                \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
    {                                           \
      if (m_Targets[i])                         \
      {                                         \
        if ((ret = m_Targets[i]->func) != S_OK) \
          return ret;                           \
      }                                         \
    }                                           \
  }                                             \
  else                                          \
  {                                             \
    if ((ret = m_Targets[0]->func) != S_OK)     \
      return ret;                               \
    if ((ret = m_Targets[1]->func) != S_OK)     \
      return ret;                               \
  }                                             \
                                                \
  return S_OK;

	static HRESULT STDMETHODCALLTYPE CreateCommandList(
	  _In_ ID3D12Device* pDevice,
	  _In_ UINT nodeMask,
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  _In_ ID3D12CommandAllocator* pCommandAllocator,
	  _In_opt_ ID3D12PipelineState* pInitialState,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandList)
	{
		return (*ppCommandList = new self(pDevice, nodeMask, type, pCommandAllocator, pInitialState, riid)) ? S_OK : E_FAIL;
	}

	#pragma region /* IUnknown implementation */

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
	  /* [in] */ REFIID riid,
	  /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) final
	{
		abort();
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) final
	{
		Broadcast(AddRef());
		return CryInterlockedIncrement(&m_RefCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void) final
	{
		Broadcast(Release());
		ULONG count = CryInterlockedDecrement(&m_RefCount);
		if (!count) delete this; return count;
	}

	#pragma endregion

	#pragma region /* ID3D12Object implementation */

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID guid,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_opt_(*pDataSize)  void* pData) final
	{
		return Pick(0, GetPrivateData(guid, pDataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData) final
	{
		BroadcastWithFail(SetPrivateData(guid, DataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) final
	{
		BroadcastWithFail(SetPrivateDataInterface(guid, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetName(
	  _In_z_ LPCWSTR Name) final
	{
		BroadcastWithFail(SetName(Name));
	}

	#pragma endregion

	#pragma region /* ID3D12DeviceChild implementation */

	virtual HRESULT STDMETHODCALLTYPE GetDevice(
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvDevice) final
	{
		return Pick(0, GetDevice(riid, ppvDevice));
	}

	#pragma endregion

	#pragma region /* ID3D12CommandList implementation */

	virtual D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType(void) final
	{
		return Pick(0, GetType());
	}

	#pragma endregion

	#pragma region /* ID3D12GraphicsCommandList implementation */

	virtual HRESULT STDMETHODCALLTYPE Close(void) final
	{
		BroadcastWithFail(Close());
	}

	virtual HRESULT STDMETHODCALLTYPE Reset(
	  _In_ ID3D12CommandAllocator* pAllocator,
	  _In_opt_ ID3D12PipelineState* pInitialState) final
	{
		BroadcastableD3D12CommandAllocator<numTargets>* Allocator = (BroadcastableD3D12CommandAllocator<numTargets>*)pAllocator;

		ParallelizeWithFail(Reset(*(*Allocator)[i], pInitialState));
	}

	virtual void STDMETHODCALLTYPE ClearState(
	  _In_opt_ ID3D12PipelineState* pPipelineState) final
	{
		Broadcast(ClearState(pPipelineState));
	}

	virtual void STDMETHODCALLTYPE DrawInstanced(
	  _In_ UINT VertexCountPerInstance,
	  _In_ UINT InstanceCount,
	  _In_ UINT StartVertexLocation,
	  _In_ UINT StartInstanceLocation) final
	{
		Broadcast(DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation));
	}

	virtual void STDMETHODCALLTYPE DrawIndexedInstanced(
	  _In_ UINT IndexCountPerInstance,
	  _In_ UINT InstanceCount,
	  _In_ UINT StartIndexLocation,
	  _In_ INT BaseVertexLocation,
	  _In_ UINT StartInstanceLocation) final
	{
		Broadcast(DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation));
	}

	virtual void STDMETHODCALLTYPE Dispatch(
	  _In_ UINT ThreadGroupCountX,
	  _In_ UINT ThreadGroupCountY,
	  _In_ UINT ThreadGroupCountZ) final
	{
		Broadcast(Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));
	}

	virtual void STDMETHODCALLTYPE CopyBufferRegion(
	  _In_ ID3D12Resource* pDstBuffer,
	  UINT64 DstOffset,
	  _In_ ID3D12Resource* pSrcBuffer,
	  UINT64 SrcOffset,
	  UINT64 NumBytes) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableDstBuffer = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pDstBuffer);
		BroadcastableD3D12Resource<numTargets>* pBroadcastableSrcBuffer = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pSrcBuffer);

		// For each copy, select the command-list which is on the GPU that owns the source resource: "Push" is the only cross-GPU copy allowed
		// The current resource-allocation contract disallows GPU-resources to be shared, so the condition will never be true because of it
		if (false /* pBroadcastableSrcBuffer->IsSharedAndGPUHeap() */)
		{
			ParallelizeMapped(pBroadcastableSrcBuffer->GetSharedOrigin, CopyBufferRegion(*(*pBroadcastableDstBuffer)[i], DstOffset, *(*pBroadcastableSrcBuffer)[i], SrcOffset, NumBytes));
		}
		// Exception: Copies from the Upload/Readback heap are "Push"able, because it's a CPU-to-GPU copy instead of a GPU-to-GPU one
		else
		{
			Parallelize(CopyBufferRegion(*(*pBroadcastableDstBuffer)[i], DstOffset, *(*pBroadcastableSrcBuffer)[i], SrcOffset, NumBytes));
		}
	}

	virtual void STDMETHODCALLTYPE CopyTextureRegion(
	  _In_ const D3D12_TEXTURE_COPY_LOCATION* pDst,
	  UINT DstX,
	  UINT DstY,
	  UINT DstZ,
	  _In_ const D3D12_TEXTURE_COPY_LOCATION* pSrc,
	  _In_opt_ const D3D12_BOX* pSrcBox) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableDstResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pDst->pResource);
		BroadcastableD3D12Resource<numTargets>* pBroadcastableSrcResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pSrc->pResource);

		D3D12_TEXTURE_COPY_LOCATION Dst = *pDst;
		D3D12_TEXTURE_COPY_LOCATION Src = *pSrc;

		for (int i = 0; i < numTargets; ++i)
		{
			Dst.pResource = *(*pBroadcastableDstResource)[i];
			Src.pResource = *(*pBroadcastableSrcResource)[i];

			// For each copy, select the command-list which is on the GPU that owns the source resource: "Push" is the only cross-GPU copy allowed
			// The current resource-allocation contract disallows GPU-resources to be shared, so the condition will never be true because of it
			if (false /* pBroadcastableSrcBuffer->IsSharedAndGPUHeap(i) */)
			{
				Pick(pBroadcastableSrcResource->GetSharedOrigin(i), CopyTextureRegion(&Dst, DstX, DstY, DstZ, &Src, pSrcBox));
			}
			// Exception: Copies from the Upload/Readback heap are "Push"able, because it's a CPU-to-GPU copy instead of a GPU-to-GPU one
			else
			{
				Pick(i, CopyTextureRegion(&Dst, DstX, DstY, DstZ, &Src, pSrcBox));
			}
		}
	}

	virtual void STDMETHODCALLTYPE CopyResource(
	  _In_ ID3D12Resource* pDstResource,
	  _In_ ID3D12Resource* pSrcResource) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableDstResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pDstResource);
		BroadcastableD3D12Resource<numTargets>* pBroadcastableSrcResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pSrcResource);

		// For each copy, select the command-list which is on the GPU that owns the source resource: "Push" is the only cross-GPU copy allowed
		// The current resource-allocation contract disallows GPU-resources to be shared, so the condition will never be true because of it
		if (false /* pBroadcastableSrcBuffer->IsSharedAndGPUHeap() */)
		{
			ParallelizeMapped(pBroadcastableSrcResource->GetSharedOrigin, CopyResource(*(*pBroadcastableDstResource)[i], *(*pBroadcastableSrcResource)[i]));
		}
		// Exception: Copies from the Upload/Readback heap are "Push"able, because it's a CPU-to-GPU copy instead of a GPU-to-GPU one
		else
		{
			Parallelize(CopyResource(*(*pBroadcastableDstResource)[i], *(*pBroadcastableSrcResource)[i]));
		}
	}

	virtual void STDMETHODCALLTYPE CopyTiles(
	  _In_ ID3D12Resource* pTiledResource,
	  _In_ const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
	  _In_ const D3D12_TILE_REGION_SIZE* pTileRegionSize,
	  _In_ ID3D12Resource* pBuffer,
	  UINT64 BufferStartOffsetInBytes,
	  D3D12_TILE_COPY_FLAGS Flags) final
	{
		abort();
	}

	virtual void STDMETHODCALLTYPE ResolveSubresource(
	  _In_ ID3D12Resource* pDstResource,
	  _In_ UINT DstSubresource,
	  _In_ ID3D12Resource* pSrcResource,
	  _In_ UINT SrcSubresource,
	  _In_ DXGI_FORMAT Format) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableDstResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pDstResource);
		BroadcastableD3D12Resource<numTargets>* pBroadcastableSrcResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pSrcResource);

		Parallelize(ResolveSubresource(*(*pBroadcastableDstResource)[i], DstSubresource, *(*pBroadcastableSrcResource)[i], SrcSubresource, Format));
	}

	virtual void STDMETHODCALLTYPE IASetPrimitiveTopology(
	  _In_ D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) final
	{
		Broadcast(IASetPrimitiveTopology(PrimitiveTopology));
	}

	virtual void STDMETHODCALLTYPE RSSetViewports(
	  _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
	  _In_reads_(NumViewports)  const D3D12_VIEWPORT* pViewports) final
	{
		Broadcast(RSSetViewports(NumViewports, pViewports));
	}

	virtual void STDMETHODCALLTYPE RSSetScissorRects(
	  _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
	  _In_reads_(NumRects)  const D3D12_RECT* pRects) final
	{
		Broadcast(RSSetScissorRects(NumRects, pRects));
	}

	virtual void STDMETHODCALLTYPE OMSetBlendFactor(
	  _In_opt_ const FLOAT BlendFactor[4]) final
	{
		Broadcast(OMSetBlendFactor(BlendFactor));
	}

	virtual void STDMETHODCALLTYPE OMSetStencilRef(
	  _In_ UINT StencilRef) final
	{
		Broadcast(OMSetStencilRef(StencilRef));
	}

	virtual void STDMETHODCALLTYPE SetPipelineState(
	  _In_ ID3D12PipelineState* pPipelineState) final
	{
		Broadcast(SetPipelineState(pPipelineState));
	}

	virtual void STDMETHODCALLTYPE ResourceBarrier(
	  _In_ UINT NumBarriers,
	  _In_reads_(NumBarriers)  const D3D12_RESOURCE_BARRIER* pBarriers) final
	{
		D3D12_RESOURCE_BARRIER* Barriers = (D3D12_RESOURCE_BARRIER*)alloca(sizeof(D3D12_RESOURCE_BARRIER) * NumBarriers);
		UINT numBarriers = NumBarriers;

		// Even though shared resources can only be used for CopyResource/Copy...Region/UpdateSubresource
		// they can cycle through COPY_SOURCE/COPY_DEST for that effect, and need support for regular barriers

		for (int i = 0; i < numTargets; ++i)
		{
			numBarriers = 0;
			bool skip = false;
			for (int n = 0; n < NumBarriers; ++n)
			{
				Barriers[numBarriers] = pBarriers[n];
				switch (Barriers[numBarriers].Type)
				{
				case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
				{
					BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(Barriers[n].Transition.pResource);

					Barriers[numBarriers].Transition.pResource = (ID3D12Resource*)*(*pBroadcastableResource)[i];

					skip =
						pBroadcastableResource->IsShared(i);
				}
				break;
				case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
				{
					BroadcastableD3D12Resource<numTargets>* pBroadcastableResourceBefore = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(Barriers[n].Aliasing.pResourceBefore);
					BroadcastableD3D12Resource<numTargets>* pBroadcastableResourceAfter = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(Barriers[n].Aliasing.pResourceAfter);

					Barriers[numBarriers].Aliasing.pResourceBefore = (ID3D12Resource*)*(*pBroadcastableResourceBefore)[i];
					Barriers[numBarriers].Aliasing.pResourceAfter = (ID3D12Resource*)*(*pBroadcastableResourceAfter)[i];

					skip =
						pBroadcastableResourceBefore->IsShared(i) ||
						pBroadcastableResourceAfter->IsShared(i);
				}
				break;
				case D3D12_RESOURCE_BARRIER_TYPE_UAV:
				{
					BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(Barriers[n].UAV.pResource);

					Barriers[numBarriers].UAV.pResource = (ID3D12Resource*)*(*pBroadcastableResource)[i];

					skip =
						pBroadcastableResource->IsShared(i);
				}
				break;
				}

				// Don't issue transition barriers multiple times for the same resource
				if (!skip)
					numBarriers++;
			}

			Pick(i, ResourceBarrier(numBarriers, Barriers));
		}
	}

	virtual void STDMETHODCALLTYPE ExecuteBundle(
	  _In_ ID3D12GraphicsCommandList* pCommandList_) final
	{
		self* pCommandList = (self*)pCommandList_;

		Parallelize(ExecuteBundle((ID3D12GraphicsCommandList*)*(*pCommandList)[i]));
	}

	virtual void STDMETHODCALLTYPE SetDescriptorHeaps(
	  _In_ UINT NumDescriptorHeaps,
	  _In_reads_(NumDescriptorHeaps)  ID3D12DescriptorHeap* const* ppDescriptorHeaps) final
	{
		ID3D12DescriptorHeap** DescriptorHeaps = (ID3D12DescriptorHeap**)alloca(sizeof(ID3D12DescriptorHeap*) * NumDescriptorHeaps);

		for (int i = 0; i < numTargets; ++i)
		{
			for (int n = 0; n < NumDescriptorHeaps; ++n)
			{
				BroadcastableD3D12DescriptorHeap<numTargets>* pDescriptorHeaps = (BroadcastableD3D12DescriptorHeap<numTargets>*)ppDescriptorHeaps[n];

				DescriptorHeaps[n] = (ID3D12DescriptorHeap*)*(*pDescriptorHeaps)[i];
			}

			Pick(i, SetDescriptorHeaps(NumDescriptorHeaps, DescriptorHeaps));
		}
	}

	virtual void STDMETHODCALLTYPE SetComputeRootSignature(
	  _In_ ID3D12RootSignature* pRootSignature) final
	{
		Broadcast(SetComputeRootSignature(pRootSignature));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRootSignature(
	  _In_ ID3D12RootSignature* pRootSignature) final
	{
		Broadcast(SetGraphicsRootSignature(pRootSignature));
	}

	virtual void STDMETHODCALLTYPE SetComputeRootDescriptorTable(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) final
	{
		auto GPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetGPUDeltasFromHandle(UINT32(BaseDescriptor.ptr >> 32));
		Verify(((BaseDescriptor.ptr >> 32) == 0) || (GPUDeltaDescriptors[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = BaseDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			GPUDescriptor.ptr = BaseDescriptor.ptr + GPUDeltaDescriptors[i];

			Pick(i, SetComputeRootDescriptorTable(RootParameterIndex, GPUDescriptor));
		}
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) final
	{
		auto GPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetGPUDeltasFromHandle(UINT32(BaseDescriptor.ptr >> 32));
		Verify(((BaseDescriptor.ptr >> 32) == 0) || (GPUDeltaDescriptors[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = BaseDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			GPUDescriptor.ptr = BaseDescriptor.ptr + GPUDeltaDescriptors[i];

			Pick(i, SetGraphicsRootDescriptorTable(RootParameterIndex, GPUDescriptor));
		}
	}

	virtual void STDMETHODCALLTYPE SetComputeRoot32BitConstant(
	  _In_ UINT RootParameterIndex,
	  _In_ UINT SrcData,
	  _In_ UINT DestOffsetIn32BitValues) final
	{
		Broadcast(SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(
	  _In_ UINT RootParameterIndex,
	  _In_ UINT SrcData,
	  _In_ UINT DestOffsetIn32BitValues) final
	{
		Broadcast(SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues));
	}

	virtual void STDMETHODCALLTYPE SetComputeRoot32BitConstants(
	  _In_ UINT RootParameterIndex,
	  _In_ UINT Num32BitValuesToSet,
	  _In_reads_(Num32BitValuesToSet * sizeof(UINT))  const void* pSrcData,
	  _In_ UINT DestOffsetIn32BitValues) final
	{
		Broadcast(SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(
	  _In_ UINT RootParameterIndex,
	  _In_ UINT Num32BitValuesToSet,
	  _In_reads_(Num32BitValuesToSet * sizeof(UINT))  const void* pSrcData,
	  _In_ UINT DestOffsetIn32BitValues) final
	{
		Broadcast(SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues));
	}

	virtual void STDMETHODCALLTYPE SetComputeRootConstantBufferView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE SetComputeRootShaderResourceView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(
	  _In_ UINT RootParameterIndex,
	  _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) final
	{
		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(BufferLocation >> 32));
		Verify(((BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		Parallelize(SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation + DeltaLocations[i]));
	}

	virtual void STDMETHODCALLTYPE IASetIndexBuffer(
	  _In_opt_ const D3D12_INDEX_BUFFER_VIEW* pView) final
	{
		// optional
		if (!pView)
		{
			Broadcast(IASetIndexBuffer(nullptr));
			return;
		}

		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(pView->BufferLocation >> 32));
		Verify(((pView->BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_INDEX_BUFFER_VIEW View = *pView;
		for (int i = 0; i < numTargets; ++i)
		{
			View.BufferLocation = pView->BufferLocation + DeltaLocations[i];

			Pick(i, IASetIndexBuffer(&View));
		}
	}

	virtual void STDMETHODCALLTYPE IASetVertexBuffers(
	  _In_ UINT StartSlot,
	  _In_ UINT NumViews,
	  _In_reads_opt_(NumViews)  const D3D12_VERTEX_BUFFER_VIEW* pViews) final
	{
		// optional
		if (!pViews)
		{
			Broadcast(IASetVertexBuffers(StartSlot, NumViews, nullptr));
			return;
		}

		D3D12_VERTEX_BUFFER_VIEW* Views = (D3D12_VERTEX_BUFFER_VIEW*)alloca(sizeof(D3D12_VERTEX_BUFFER_VIEW) * NumViews);

		for (int i = 0; i < numTargets; ++i)
		{
			for (UINT n = 0; n < NumViews; ++n)
			{
				auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(pViews[n].BufferLocation >> 32));
				DX12_ASSERT(((pViews[n].BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

				Views[n] = pViews[n];
				Views[n].BufferLocation = pViews[n].BufferLocation + DeltaLocations[i];
			}

			Pick(i, IASetVertexBuffers(StartSlot, NumViews, Views));
		}
	}

	virtual void STDMETHODCALLTYPE SOSetTargets(
	  _In_ UINT StartSlot,
	  _In_ UINT NumViews,
	  _In_reads_opt_(NumViews)  const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) final
	{
		// optional
		if (!pViews)
		{
			Broadcast(SOSetTargets(StartSlot, NumViews, nullptr));
			return;
		}

		D3D12_STREAM_OUTPUT_BUFFER_VIEW* Views = (D3D12_STREAM_OUTPUT_BUFFER_VIEW*)alloca(sizeof(D3D12_STREAM_OUTPUT_BUFFER_VIEW) * NumViews);

		for (int i = 0; i < numTargets; ++i)
		{
			for (UINT n = 0; n < NumViews; ++n)
			{
				auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(pViews[n].BufferLocation >> 32));
				auto DeltaFilledSizeLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(pViews[n].BufferFilledSizeLocation >> 32));
				DX12_ASSERT(((pViews[n].BufferLocation >> 32) == 0) || (DeltaLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");
				DX12_ASSERT(((pViews[n].BufferFilledSizeLocation >> 32) == 0) || (DeltaFilledSizeLocations[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");

				Views[n] = pViews[n];
				Views[n].BufferLocation = pViews[n].BufferLocation + DeltaLocations[i];
				Views[n].BufferFilledSizeLocation = pViews[n].BufferFilledSizeLocation + DeltaFilledSizeLocations[i];
			}

			Pick(i, SOSetTargets(StartSlot, NumViews, Views));
		}
	}

	virtual void STDMETHODCALLTYPE OMSetRenderTargets(
	  _In_ UINT NumRenderTargetDescriptors,
	  _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
	  _In_ BOOL RTsSingleHandleToDescriptorRange,
	  _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) final
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* RenderTargetDescriptors = (D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * NumRenderTargetDescriptors);
		D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilDescriptor;

		// TODO: switch inner and outer loop to reduce GetDeltasFromHandle() calls
		for (int i = 0; i < numTargets; ++i)
		{
			// optional
			if (pRenderTargetDescriptors)
			{
				for (int n = 0; n < (RTsSingleHandleToDescriptorRange ? 1 : NumRenderTargetDescriptors); ++n)
				{
					auto RenderTargetDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(pRenderTargetDescriptors[n].ptr >> 32));
					DX12_ASSERT(((pRenderTargetDescriptors[n].ptr >> 32) == 0) || (RenderTargetDeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

					RenderTargetDescriptors[n] = pRenderTargetDescriptors[n];
					RenderTargetDescriptors[n].ptr = pRenderTargetDescriptors[n].ptr + RenderTargetDeltaDescriptors[i];
				}
			}

			// optional
			if (pDepthStencilDescriptor)
			{
				auto DepthStencilDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(pDepthStencilDescriptor->ptr >> 32));
				DX12_ASSERT(((pDepthStencilDescriptor->ptr >> 32) == 0) || (DepthStencilDeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

				DepthStencilDescriptor = pDepthStencilDescriptor[0];
				DepthStencilDescriptor.ptr = pDepthStencilDescriptor->ptr + DepthStencilDeltaDescriptors[i];
			}

			Pick(i, OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors ? RenderTargetDescriptors : nullptr, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor ? &DepthStencilDescriptor : nullptr));
		}
	}

	virtual void STDMETHODCALLTYPE ClearDepthStencilView(
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
	  _In_ D3D12_CLEAR_FLAGS ClearFlags,
	  _In_ FLOAT Depth,
	  _In_ UINT8 Stencil,
	  _In_ UINT NumRects,
	  _In_reads_(NumRects)  const D3D12_RECT* pRects) final
	{
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DepthStencilView.ptr >> 32));

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DepthStencilView;
		for (int i = 0; i < numTargets; ++i)
		{
			DX12_ASSERT(((DepthStencilView.ptr >> 32) == 0) || (DeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

			Descriptor.ptr = DepthStencilView.ptr + DeltaDescriptors[i];

			Pick(i, ClearDepthStencilView(Descriptor, ClearFlags, Depth, Stencil, NumRects, pRects));
		}
	}

	virtual void STDMETHODCALLTYPE ClearRenderTargetView(
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
	  _In_ const FLOAT ColorRGBA[4],
	  _In_ UINT NumRects,
	  _In_reads_(NumRects)  const D3D12_RECT* pRects) final
	{
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(RenderTargetView.ptr >> 32));
		Verify(((RenderTargetView.ptr >> 32) == 0) || (DeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = RenderTargetView;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = RenderTargetView.ptr + DeltaDescriptors[i];

			Pick(i, ClearRenderTargetView(Descriptor, ColorRGBA, NumRects, pRects));
		}
	}

	virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
	  _In_ D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
	  _In_ ID3D12Resource* pResource,
	  _In_ const UINT Values[4],
	  _In_ UINT NumRects,
	  _In_reads_(NumRects)  const D3D12_RECT* pRects) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		auto GPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetGPUDeltasFromHandle(UINT32(ViewGPUHandleInCurrentHeap.ptr >> 32));
		auto CPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(ViewCPUHandle.ptr >> 32));
		Verify(((ViewGPUHandleInCurrentHeap.ptr >> 32) == 0) || (GPUDeltaDescriptors[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");
		Verify(((ViewCPUHandle.ptr >> 32) == 0) || (CPUDeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = ViewGPUHandleInCurrentHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = ViewCPUHandle;
		for (int i = 0; i < numTargets; ++i)
		{
			GPUDescriptor.ptr = ViewGPUHandleInCurrentHeap.ptr + GPUDeltaDescriptors[i];
			CPUDescriptor.ptr = ViewCPUHandle.ptr + CPUDeltaDescriptors[i];

			Pick(i, ClearUnorderedAccessViewUint(GPUDescriptor, CPUDescriptor, *(*pBroadcastableResource)[i], Values, NumRects, pRects));
		}
	}

	virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
	  _In_ D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
	  _In_ ID3D12Resource* pResource,
	  _In_ const FLOAT Values[4],
	  _In_ UINT NumRects,
	  _In_reads_(NumRects)  const D3D12_RECT* pRects) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		auto GPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetGPUDeltasFromHandle(UINT32(ViewGPUHandleInCurrentHeap.ptr >> 32));
		auto CPUDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(ViewCPUHandle.ptr >> 32));
		Verify(((ViewGPUHandleInCurrentHeap.ptr >> 32) == 0) || (GPUDeltaDescriptors[i] != 0), "GPUAddressCorruption! It should not be possible to receive 0 deltas.");
		Verify(((ViewCPUHandle.ptr >> 32) == 0) || (CPUDeltaDescriptors[i] != 0), "CPUAddressCorruption! It should not be possible to receive 0 deltas.");

		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = ViewGPUHandleInCurrentHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = ViewCPUHandle;
		for (int i = 0; i < numTargets; ++i)
		{
			GPUDescriptor.ptr = ViewGPUHandleInCurrentHeap.ptr + GPUDeltaDescriptors[i];
			CPUDescriptor.ptr = ViewCPUHandle.ptr + CPUDeltaDescriptors[i];

			Pick(i, ClearUnorderedAccessViewFloat(GPUDescriptor, CPUDescriptor, *(*pBroadcastableResource)[i], Values, NumRects, pRects));
		}
	}

	virtual void STDMETHODCALLTYPE DiscardResource(
	  _In_ ID3D12Resource* pResource,
	  _In_opt_ const D3D12_DISCARD_REGION* pRegion) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);

		Parallelize(DiscardResource(*(*pBroadcastableResource)[i], pRegion));
	}

	virtual void STDMETHODCALLTYPE BeginQuery(
	  _In_ ID3D12QueryHeap* pQueryHeap_,
	  _In_ D3D12_QUERY_TYPE Type,
	  _In_ UINT Index) final
	{
		BroadcastableD3D12QueryHeap<numTargets>* pQueryHeap = (BroadcastableD3D12QueryHeap<numTargets>*)pQueryHeap_;

		Parallelize(BeginQuery((ID3D12QueryHeap*)*(*pQueryHeap)[i], Type, Index));
	}

	virtual void STDMETHODCALLTYPE EndQuery(
	  _In_ ID3D12QueryHeap* pQueryHeap_,
	  _In_ D3D12_QUERY_TYPE Type,
	  _In_ UINT Index) final
	{
		BroadcastableD3D12QueryHeap<numTargets>* pQueryHeap = (BroadcastableD3D12QueryHeap<numTargets>*)pQueryHeap_;

		Parallelize(EndQuery((ID3D12QueryHeap*)*(*pQueryHeap)[i], Type, Index));
	}

	virtual void STDMETHODCALLTYPE ResolveQueryData(
	  _In_ ID3D12QueryHeap* pQueryHeap_,
	  _In_ D3D12_QUERY_TYPE Type,
	  _In_ UINT StartIndex,
	  _In_ UINT NumQueries,
	  _In_ ID3D12Resource* pDestinationBuffer_,
	  _In_ UINT64 AlignedDestinationBufferOffset) final
	{
		BroadcastableD3D12QueryHeap<numTargets>* pQueryHeap = (BroadcastableD3D12QueryHeap<numTargets>*)pQueryHeap_;
		BroadcastableD3D12Resource<numTargets>* pDestinationBuffer = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pDestinationBuffer_);

		// TODO: IsShared() possible?

		Parallelize(ResolveQueryData((ID3D12QueryHeap*)*(*pQueryHeap)[i], Type, StartIndex, NumQueries, *(*pDestinationBuffer)[i], AlignedDestinationBufferOffset));
	}

	virtual void STDMETHODCALLTYPE SetPredication(
	  _In_opt_ ID3D12Resource* pBuffer,
	  _In_ UINT64 AlignedBufferOffset,
	  _In_ D3D12_PREDICATION_OP Operation) final
	{
		// optional
		if (!pBuffer)
		{
			Parallelize(SetPredication(nullptr, AlignedBufferOffset, Operation));
			return;
		}

		BroadcastableD3D12Resource<numTargets>* pBroadcastableBuffer = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pBuffer);

		// TODO: IsShared() possible?

		Parallelize(SetPredication(*(*pBroadcastableBuffer)[i], AlignedBufferOffset, Operation));
	}

	virtual void STDMETHODCALLTYPE SetMarker(
	  UINT Metadata,
	  _In_reads_bytes_opt_(Size)  const void* pData,
	  UINT Size) final
	{
		Broadcast(SetMarker(Metadata, pData, Size));
	}

	virtual void STDMETHODCALLTYPE BeginEvent(
	  UINT Metadata,
	  _In_reads_bytes_opt_(Size)  const void* pData,
	  UINT Size) final
	{
		Broadcast(BeginEvent(Metadata, pData, Size));
	}

	virtual void STDMETHODCALLTYPE EndEvent(void) final
	{
		Broadcast(EndEvent());
	}

	virtual void STDMETHODCALLTYPE ExecuteIndirect(
	  _In_ ID3D12CommandSignature* pCommandSignature,
	  _In_ UINT MaxCommandCount,
	  _In_ ID3D12Resource* pArgumentBuffer,
	  _In_ UINT64 ArgumentBufferOffset,
	  _In_opt_ ID3D12Resource* pCountBuffer,
	  _In_ UINT64 CountBufferOffset) final
	{
		abort();
	}

	#pragma endregion

	inline void CopyResourceOvercross(
		ID3D12Resource* pDstResource,
		ID3D12Resource* pSrcResource
	)
	{
		BroadcastableD3D12Resource<numTargets>* pDstResources = (BroadcastableD3D12Resource<numTargets>*)pDstResource;
		BroadcastableD3D12Resource<numTargets>* pSrcResources = (BroadcastableD3D12Resource<numTargets>*)pSrcResource;

		m_Targets[0]->CopyResource(*(*pDstResources)[1], *(*pSrcResources)[0]);
		m_Targets[1]->CopyResource(*(*pDstResources)[0], *(*pSrcResources)[1]);
	}

};
