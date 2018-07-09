// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDeviceContext.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11DeviceContext
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLDEVICECONTEXT__
#define __CRYDXGLDEVICECONTEXT__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
class CContext;
}

class CCryDXGLBlendState;
class CCryDXGLBuffer;
class CCryDXGLDepthStencilState;
class CCryDXGLDepthStencilView;
class CCryDXGLInputLayout;
class CCryDXGLQuery;
class CCryDXGLRasterizerState;
class CCryDXGLRenderTargetView;
class CCryDXGLSamplerState;
class CCryDXGLShader;
class CCryDXGLShaderResourceView;
class CCryDXGLUnorderedAccessView;

class CCryDXGLDeviceContext
#if DXGL_VIRTUAL_DEVICE_AND_CONTEXT && !DXGL_FULL_EMULATION
	: public ID3D11DeviceContext
#else
	: public CCryDXGLDeviceChild
#endif
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLDeviceContext, D3D11DeviceContext)

	CCryDXGLDeviceContext();
	virtual ~CCryDXGLDeviceContext();

	bool                  Initialize(CCryDXGLDevice* pDevice);
	void                  Shutdown();

	NCryOpenGL::CContext* GetGLContext();

	// ID3D11DeviceContext implementation
	void STDMETHODCALLTYPE                      VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
	void STDMETHODCALLTYPE                      Draw(UINT VertexCount, UINT StartVertexLocation);
	HRESULT STDMETHODCALLTYPE                   Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	void STDMETHODCALLTYPE                      Unmap(ID3D11Resource* pResource, UINT Subresource);
	void STDMETHODCALLTYPE                      PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      IASetInputLayout(ID3D11InputLayout* pInputLayout);
	void STDMETHODCALLTYPE                      IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets);
	void STDMETHODCALLTYPE                      IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
	void STDMETHODCALLTYPE                      DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
	void STDMETHODCALLTYPE                      DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void STDMETHODCALLTYPE                      GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
	void STDMETHODCALLTYPE                      VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      Begin(ID3D11Asynchronous* pAsync);
	void STDMETHODCALLTYPE                      End(ID3D11Asynchronous* pAsync);
	HRESULT STDMETHODCALLTYPE                   GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags);
	void STDMETHODCALLTYPE                      SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue);
	void STDMETHODCALLTYPE                      GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView);
	void STDMETHODCALLTYPE                      OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	void STDMETHODCALLTYPE                      OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask);
	void STDMETHODCALLTYPE                      OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef);
	void STDMETHODCALLTYPE                      SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets);
	void STDMETHODCALLTYPE                      DrawAuto(void);
	void STDMETHODCALLTYPE                      DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);
	void STDMETHODCALLTYPE                      DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);
	void STDMETHODCALLTYPE                      Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	void STDMETHODCALLTYPE                      DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);
	void STDMETHODCALLTYPE                      RSSetState(ID3D11RasterizerState* pRasterizerState);
	void STDMETHODCALLTYPE                      RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports);
	void STDMETHODCALLTYPE                      RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects);
	void STDMETHODCALLTYPE                      CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox);
	void STDMETHODCALLTYPE                      CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource);
	void STDMETHODCALLTYPE                      UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
	void STDMETHODCALLTYPE                      CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView);
	void STDMETHODCALLTYPE                      ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]);
	void STDMETHODCALLTYPE                      ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4]);
	void STDMETHODCALLTYPE                      ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4]);
	void STDMETHODCALLTYPE                      ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);
	void STDMETHODCALLTYPE                      GenerateMips(ID3D11ShaderResourceView* pShaderResourceView);
	void STDMETHODCALLTYPE                      SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD);
	FLOAT STDMETHODCALLTYPE                     GetResourceMinLOD(ID3D11Resource* pResource);
	void STDMETHODCALLTYPE                      ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);
	void STDMETHODCALLTYPE                      ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState);
	void STDMETHODCALLTYPE                      HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void STDMETHODCALLTYPE                      CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	void STDMETHODCALLTYPE                      CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void STDMETHODCALLTYPE                      CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void STDMETHODCALLTYPE                      CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void STDMETHODCALLTYPE                      VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      IAGetInputLayout(ID3D11InputLayout** ppInputLayout);
	void STDMETHODCALLTYPE                      IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets);
	void STDMETHODCALLTYPE                      IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset);
	void STDMETHODCALLTYPE                      GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology);
	void STDMETHODCALLTYPE                      VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue);
	void STDMETHODCALLTYPE                      GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView);
	void STDMETHODCALLTYPE                      OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);
	void STDMETHODCALLTYPE                      OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask);
	void STDMETHODCALLTYPE                      OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef);
	void STDMETHODCALLTYPE                      SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets);
	void STDMETHODCALLTYPE                      RSGetState(ID3D11RasterizerState** ppRasterizerState);
	void STDMETHODCALLTYPE                      RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports);
	void STDMETHODCALLTYPE                      RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects);
	void STDMETHODCALLTYPE                      HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void STDMETHODCALLTYPE                      CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);
	void STDMETHODCALLTYPE                      CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void STDMETHODCALLTYPE                      CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void STDMETHODCALLTYPE                      CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void STDMETHODCALLTYPE                      ClearState(void);
	void STDMETHODCALLTYPE                      Flush(void);
	D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(void);
	UINT STDMETHODCALLTYPE                      GetContextFlags(void);
	HRESULT STDMETHODCALLTYPE                   FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList);

	// Partial ID3D11DeviceContext1 implementation
	void STDMETHODCALLTYPE CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void STDMETHODCALLTYPE CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void STDMETHODCALLTYPE PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void STDMETHODCALLTYPE VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void STDMETHODCALLTYPE GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void STDMETHODCALLTYPE HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void STDMETHODCALLTYPE DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);

protected:
	static _smart_ptr<CCryDXGLBlendState>        CreateDefaultBlendState(CCryDXGLDevice* pDevice);
	static _smart_ptr<CCryDXGLDepthStencilState> CreateDefaultDepthStencilState(CCryDXGLDevice* pDevice);
	static _smart_ptr<CCryDXGLRasterizerState>   CreateDefaultRasterizerState(CCryDXGLDevice* pDevice);
	static _smart_ptr<CCryDXGLSamplerState>      CreateDefaultSamplerState(CCryDXGLDevice* pDevice);

	struct SStage
	{
		_smart_ptr<CCryDXGLShader>              m_spShader;
		_smart_ptr<CCryDXGLSamplerState>        m_aspSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		_smart_ptr<CCryDXGLShaderResourceView>  m_aspShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		_smart_ptr<CCryDXGLUnorderedAccessView> m_aspUnorderedAccessViews[D3D11_1_UAV_SLOT_COUNT];
		_smart_ptr<CCryDXGLBuffer>              m_aspConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		uint32 m_auConstantBufferOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		uint32 m_auConstantBufferSizes[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	};

	void SetShaderResources(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void SetUnorderedAccessViews(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews);
	void SetShader(uint32 uStage, CCryDXGLShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void SetSamplers(uint32 uStage, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void SetConstantBuffers(uint32 uStage, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant = NULL, const UINT* pNumConstants = NULL);

	void GetShaderResources(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	void GetUnorderedAccesses(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11UnorderedAccessView** ppUnorderedAccessViews);
	void GetShader(uint32 uStage, CCryDXGLShader** pShader, ID3D11ClassInstance** ppClassInstances, UINT* NumClassInstances);
	void GetSamplers(uint32 uStage, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void GetConstantBuffers(uint32 uStage, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant = NULL, UINT* pNumConstants = NULL);
protected:
	NCryOpenGL::CContext*                 m_pContext;
	_smart_ptr<CCryDXGLBlendState>        m_spBlendState;
	_smart_ptr<CCryDXGLDepthStencilState> m_spDepthStencilState;
	_smart_ptr<CCryDXGLRasterizerState>   m_spRasterizerState;

	uint32                                m_uStencilRef;
	float m_auBlendFactor[4];
	uint32                                m_uSampleMask;

	std::vector<SStage>                   m_kStages;

	_smart_ptr<CCryDXGLBuffer>            m_aspVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint32                                m_auVertexBufferStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint32                                m_auVertexBufferOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	_smart_ptr<CCryDXGLInputLayout>       m_spInputLayout;
	_smart_ptr<CCryDXGLBuffer>            m_spIndexBuffer;
	DXGI_FORMAT                           m_eIndexBufferFormat;
	uint32                                m_uIndexBufferOffset;
	D3D11_PRIMITIVE_TOPOLOGY              m_ePrimitiveTopology;
	_smart_ptr<CCryDXGLRenderTargetView>  m_aspRenderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	_smart_ptr<CCryDXGLDepthStencilView>  m_spDepthStencilView;
	uint32                                m_uNumViewports;
	D3D11_VIEWPORT                        m_akViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	uint32                                m_uNumScissorRects;
	D3D11_RECT                            m_akScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	_smart_ptr<CCryDXGLQuery>             m_spPredicate;
	bool m_bPredicateValue;
	_smart_ptr<CCryDXGLBuffer>            m_aspStreamOutputBuffers[D3D11_SO_BUFFER_SLOT_COUNT];
	uint32                                m_auStreamOutputBufferOffsets[D3D11_SO_BUFFER_SLOT_COUNT];

	_smart_ptr<CCryDXGLBlendState>        m_spDefaultBlendState;
	_smart_ptr<CCryDXGLDepthStencilState> m_spDefaultDepthStencilState;
	_smart_ptr<CCryDXGLRasterizerState>   m_spDefaultRasterizerState;
	_smart_ptr<CCryDXGLSamplerState>      m_spDefaultSamplerState;
};

#endif //__CRYDXGLDEVICECONTEXT__
