// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_ID3D11DeviceContext.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains a definition of the ID3D11DeviceContext interface
//               matching the one in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_ID3D11DeviceContext_h__
#define __DXGL_ID3D11DeviceContext_h__

#if !DXGL_FULL_EMULATION
	#include "../Interfaces/CCryDXGLDeviceChild.hpp"
#endif //DXGL_FULL_EMULATION

struct ID3DDevice;

struct ID3D11DeviceContext
#if DXGL_FULL_EMULATION
	: ID3D11DeviceChild
#else
	: CCryDXGLDeviceChild
#endif
{
	// ID3D11DeviceContext interface (adapted from DXSDK/include/D3D11.h)
	virtual void STDMETHODCALLTYPE                      VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) = 0;
	virtual void STDMETHODCALLTYPE                      Draw(UINT VertexCount, UINT StartVertexLocation) = 0;
	virtual HRESULT STDMETHODCALLTYPE                   Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) = 0;
	virtual void STDMETHODCALLTYPE                      Unmap(ID3D11Resource* pResource, UINT Subresource) = 0;
	virtual void STDMETHODCALLTYPE                      PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      IASetInputLayout(ID3D11InputLayout* pInputLayout) = 0;
	virtual void STDMETHODCALLTYPE                      IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets) = 0;
	virtual void STDMETHODCALLTYPE                      IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset) = 0;
	virtual void STDMETHODCALLTYPE                      DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) = 0;
	virtual void STDMETHODCALLTYPE                      DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) = 0;
	virtual void STDMETHODCALLTYPE                      GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) = 0;
	virtual void STDMETHODCALLTYPE                      VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      Begin(ID3D11Asynchronous* pAsync) = 0;
	virtual void STDMETHODCALLTYPE                      End(ID3D11Asynchronous* pAsync) = 0;
	virtual HRESULT STDMETHODCALLTYPE                   GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags) = 0;
	virtual void STDMETHODCALLTYPE                      SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue) = 0;
	virtual void STDMETHODCALLTYPE                      GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView) = 0;
	virtual void STDMETHODCALLTYPE                      OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) = 0;
	virtual void STDMETHODCALLTYPE                      OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) = 0;
	virtual void STDMETHODCALLTYPE                      OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef) = 0;
	virtual void STDMETHODCALLTYPE                      SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets) = 0;
	virtual void STDMETHODCALLTYPE                      DrawAuto(void) = 0;
	virtual void STDMETHODCALLTYPE                      DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs) = 0;
	virtual void STDMETHODCALLTYPE                      DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs) = 0;
	virtual void STDMETHODCALLTYPE                      Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) = 0;
	virtual void STDMETHODCALLTYPE                      DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs) = 0;
	virtual void STDMETHODCALLTYPE                      RSSetState(ID3D11RasterizerState* pRasterizerState) = 0;
	virtual void STDMETHODCALLTYPE                      RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports) = 0;
	virtual void STDMETHODCALLTYPE                      RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects) = 0;
	virtual void STDMETHODCALLTYPE                      CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox) = 0;
	virtual void STDMETHODCALLTYPE                      CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource) = 0;
	virtual void STDMETHODCALLTYPE                      UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) = 0;
	virtual void STDMETHODCALLTYPE                      CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView) = 0;
	virtual void STDMETHODCALLTYPE                      ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]) = 0;
	virtual void STDMETHODCALLTYPE                      ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4]) = 0;
	virtual void STDMETHODCALLTYPE                      ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4]) = 0;
	virtual void STDMETHODCALLTYPE                      ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) = 0;
	virtual void STDMETHODCALLTYPE                      GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) = 0;
	virtual void STDMETHODCALLTYPE                      SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD) = 0;
	virtual FLOAT STDMETHODCALLTYPE                     GetResourceMinLOD(ID3D11Resource* pResource) = 0;
	virtual void STDMETHODCALLTYPE                      ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) = 0;
	virtual void STDMETHODCALLTYPE                      ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState) = 0;
	virtual void STDMETHODCALLTYPE                      HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) = 0;
	virtual void STDMETHODCALLTYPE                      CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      IAGetInputLayout(ID3D11InputLayout** ppInputLayout) = 0;
	virtual void STDMETHODCALLTYPE                      IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets) = 0;
	virtual void STDMETHODCALLTYPE                      IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset) = 0;
	virtual void STDMETHODCALLTYPE                      GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) = 0;
	virtual void STDMETHODCALLTYPE                      VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue) = 0;
	virtual void STDMETHODCALLTYPE                      GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView) = 0;
	virtual void STDMETHODCALLTYPE                      OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews) = 0;
	virtual void STDMETHODCALLTYPE                      OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask) = 0;
	virtual void STDMETHODCALLTYPE                      OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef) = 0;
	virtual void STDMETHODCALLTYPE                      SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets) = 0;
	virtual void STDMETHODCALLTYPE                      RSGetState(ID3D11RasterizerState** ppRasterizerState) = 0;
	virtual void STDMETHODCALLTYPE                      RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports) = 0;
	virtual void STDMETHODCALLTYPE                      RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects) = 0;
	virtual void STDMETHODCALLTYPE                      HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) = 0;
	virtual void STDMETHODCALLTYPE                      CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews) = 0;
	virtual void STDMETHODCALLTYPE                      CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) = 0;
	virtual void STDMETHODCALLTYPE                      CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) = 0;
	virtual void STDMETHODCALLTYPE                      CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) = 0;
	virtual void STDMETHODCALLTYPE                      ClearState(void) = 0;
	virtual void STDMETHODCALLTYPE                      Flush(void) = 0;
	virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(void) = 0;
	virtual UINT STDMETHODCALLTYPE                      GetContextFlags(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE                   FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList) = 0;
};

#endif // __DXGL_ID3D11DeviceContext_h__
