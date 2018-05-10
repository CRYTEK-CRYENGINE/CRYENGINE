// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBase.hpp
//  Version:     v1.00
//  Created:     17/07/2014 by Valerio Guagliumi.
//  Description: Interface wrappers used for full DirectX emulation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXEmulation__
#define __DXEmulation__

#if DXGL_FULL_EMULATION

template<typename Interface> struct SingleInheritance
{
	typedef void                                                                                              TBase;
};
	#define SINGLE_INHERITANCE(_Derived, _Base) template<> struct SingleInheritance<_Derived> { typedef _Base TBase; };

template<typename Interface>
struct SingleInheritanceInterface
{
	template<typename Object>
	static bool Query(Object* pThis, REFIID riid, void** ppvObject)
	{
		if (SingleInterface<Interface>::template Query<Object>(pThis, riid, ppvObject))
			return true;
		return SingleInheritanceInterface<typename SingleInheritance<Interface>::TBase>::template Query<Object>(pThis, riid, ppvObject);
	}
};

template<>
struct SingleInheritanceInterface<void>
{
	template<typename Object>
	static bool Query(Object* pThis, REFIID riid, void** ppvObject)
	{
		*ppvObject = NULL;
		return false;
	}
};

SINGLE_INHERITANCE(IUnknown, void)
SINGLE_INHERITANCE(ID3D10Blob, IUnknown)
SINGLE_INHERITANCE(ID3D11DeviceChild, IUnknown)
SINGLE_INHERITANCE(ID3D11DepthStencilState, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11BlendState, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11RasterizerState, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11Resource, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11Buffer, ID3D11Resource)
SINGLE_INHERITANCE(ID3D11Texture1D, ID3D11Resource)
SINGLE_INHERITANCE(ID3D11Texture2D, ID3D11Resource)
SINGLE_INHERITANCE(ID3D11Texture3D, ID3D11Resource)
SINGLE_INHERITANCE(ID3D11View, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11ShaderResourceView, ID3D11View)
SINGLE_INHERITANCE(ID3D11RenderTargetView, ID3D11View)
SINGLE_INHERITANCE(ID3D11DepthStencilView, ID3D11View)
SINGLE_INHERITANCE(ID3D11UnorderedAccessView, ID3D11View)
SINGLE_INHERITANCE(ID3D11VertexShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11HullShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11DomainShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11GeometryShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11PixelShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11ComputeShader, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11InputLayout, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11SamplerState, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11Asynchronous, ID3D11DeviceChild)
SINGLE_INHERITANCE(ID3D11Query, ID3D11Asynchronous)
SINGLE_INHERITANCE(ID3D11ShaderReflectionType, void)
SINGLE_INHERITANCE(ID3D11ShaderReflectionVariable, void)
SINGLE_INHERITANCE(ID3D11ShaderReflectionConstantBuffer, void)
SINGLE_INHERITANCE(ID3D11ShaderReflection, IUnknown)
SINGLE_INHERITANCE(IDXGIObject, IUnknown)
SINGLE_INHERITANCE(IDXGIDeviceSubObject, IDXGIObject)
SINGLE_INHERITANCE(IDXGIOutput, IDXGIObject)
SINGLE_INHERITANCE(IDXGIAdapter, IDXGIObject)
SINGLE_INHERITANCE(IDXGIAdapter1, IDXGIAdapter)
SINGLE_INHERITANCE(IDXGIFactory, IDXGIObject)
SINGLE_INHERITANCE(IDXGIFactory1, IDXGIFactory)
SINGLE_INHERITANCE(IDXGIDevice, IDXGIObject)
SINGLE_INHERITANCE(IDXGISwapChain, IDXGIDeviceSubObject)
SINGLE_INHERITANCE(ID3D11SwitchToRef, IUnknown)
SINGLE_INHERITANCE(ID3D11Device, IUnknown)
SINGLE_INHERITANCE(ID3D11DeviceContext, ID3D11DeviceChild)

struct SAggregateNode
{
	SAggregateNode* m_pNext;

	SAggregateNode()
		: m_pNext(NULL)
	{
	}

	void Insert(SAggregateNode* pHead)
	{
		m_pNext = pHead->m_pNext;
		pHead->m_pNext = this;
	}

	virtual bool QueryInterfaceInternal(REFIID riid, void** ppvObject)
	{
		return false;
	};
};

	#define DXGL_WRAPPER_ROOT_NO_COM(_Interface)           \
	  typedef Impl TImpl;                                  \
	  TImpl* m_pImpl;                                      \
	  void InitializeWrapper(TImpl * pImpl)                \
	  {                                                    \
	    m_pImpl = pImpl;                                   \
	    pImpl->m_pVirtual ## _Interface ## Wrapper = this; \
	  }
	#define DXGL_WRAPPER_ROOT(_Interface)                  \
	  typedef Impl TImpl;                                  \
	  TImpl* m_pImpl;                                      \
	  void InitializeWrapper(TImpl * pImpl)                \
	  {                                                    \
	    m_pImpl = pImpl;                                   \
	    Insert(&pImpl->GetAggregateHead());                \
	    pImpl->m_pVirtual ## _Interface ## Wrapper = this; \
	  }
	#define DXGL_WRAPPER_DERIVED(_Interface, _Parent)                     \
	  void InitializeWrapper(typename _Parent<Impl, Base>::TImpl * pImpl) \
	  {                                                                   \
	    _Parent<Impl, Base>::InitializeWrapper(pImpl);                    \
	    pImpl->m_pVirtual ## _Interface ## Wrapper = this;                \
	  }

namespace NDXGLWrappers
{

template<typename Impl, typename Base>
struct SUnknown : Base, SAggregateNode
{
	DXGL_WRAPPER_ROOT(Unknown)

	SUnknown() : m_pImpl(NULL) {}
	virtual ~SUnknown(){}

	ULONG STDMETHODCALLTYPE   AddRef()  { return m_pImpl->AddRef(); }
	ULONG STDMETHODCALLTYPE   Release() { return m_pImpl->Release(); }

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInheritanceInterface<Base>::Query(this, riid, ppvObject))
			return S_OK;
		SAggregateNode* pNode(m_pImpl->GetAggregateHead().m_pNext);
		while (pNode != NULL)
		{
			if (pNode->QueryInterfaceInternal(riid, ppvObject))
				return S_OK;
			pNode = pNode->m_pNext;
		}
		return E_NOINTERFACE;
	}

	bool QueryInterfaceInternal(REFIID riid, void** ppvObject)
	{
		return SingleInheritanceInterface<Base>::Query(this, riid, ppvObject);
	}
};

template<typename Impl, typename Base>
struct SD3D10Blob : SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D10Blob, SUnknown)
	SD3D10Blob(){}
	LPVOID STDMETHODCALLTYPE GetBufferPointer() { return this->m_pImpl->GetBufferPointer(); }
	SIZE_T STDMETHODCALLTYPE GetBufferSize()    { return this->m_pImpl->GetBufferSize(); }
};

template<typename Impl, typename Base>
struct SD3D11DeviceChild : SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11DeviceChild, SUnknown)
	SD3D11DeviceChild(){}
	void STDMETHODCALLTYPE    GetDevice(ID3D11Device** ppDevice)                             { this->m_pImpl->GetDevice(ppDevice); }
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)     { return this->m_pImpl->GetPrivateData(guid, pDataSize, pData); }
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) { return this->m_pImpl->SetPrivateData(guid, DataSize, pData); }
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)   { return this->m_pImpl->SetPrivateDataInterface(guid, pData); }
};

template<typename Impl, typename Base>
struct SD3D11DepthStencilState : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11DepthStencilState, SD3D11DeviceChild)
	SD3D11DepthStencilState(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11BlendState : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11BlendState, SD3D11DeviceChild)
	SD3D11BlendState(){}
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11RasterizerState : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11RasterizerState, SD3D11DeviceChild)
	SD3D11RasterizerState(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11Resource : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Resource, SD3D11DeviceChild)
	SD3D11Resource(){}
	void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) { this->m_pImpl->GetType(pResourceDimension); }
	void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority)            { this->m_pImpl->SetEvictionPriority(EvictionPriority); }
	UINT STDMETHODCALLTYPE GetEvictionPriority()                                 { return this->m_pImpl->GetEvictionPriority(); }
};

template<typename Impl, typename Base>
struct SD3D11Buffer : SD3D11Resource<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Buffer, SD3D11Resource)
	SD3D11Buffer(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_BUFFER_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11Texture1D : SD3D11Resource<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Texture1D, SD3D11Resource)
	SD3D11Texture1D(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE1D_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11Texture2D : SD3D11Resource<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Texture2D, SD3D11Resource)
	SD3D11Texture2D(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11Texture3D : SD3D11Resource<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Texture3D, SD3D11Resource)
	SD3D11Texture3D(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE3D_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11View : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11View, SD3D11DeviceChild)
	SD3D11View(){}
	void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) { this->m_pImpl->GetResource(ppResource); }
};

template<typename Impl, typename Base>
struct SD3D11ShaderResourceView : SD3D11View<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11ShaderResourceView, SD3D11View)
	SD3D11ShaderResourceView(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11RenderTargetView : SD3D11View<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11RenderTargetView, SD3D11View)
	SD3D11RenderTargetView(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11DepthStencilView : SD3D11View<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11DepthStencilView, SD3D11View)
	SD3D11DepthStencilView(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11UnorderedAccessView : SD3D11View<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11UnorderedAccessView, SD3D11View)
	SD3D11UnorderedAccessView(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base> struct SD3D11VertexShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11VertexShader, SD3D11DeviceChild) SD3D11VertexShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11HullShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11HullShader, SD3D11DeviceChild) SD3D11HullShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11DomainShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11DomainShader, SD3D11DeviceChild) SD3D11DomainShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11GeometryShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11GeometryShader, SD3D11DeviceChild) SD3D11GeometryShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11PixelShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11PixelShader, SD3D11DeviceChild) SD3D11PixelShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11ComputeShader : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11ComputeShader, SD3D11DeviceChild) SD3D11ComputeShader()
	{
	}
};
template<typename Impl, typename Base> struct SD3D11InputLayout : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11InputLayout, SD3D11DeviceChild) SD3D11InputLayout()
	{
	}
};

template<typename Impl, typename Base>
struct SD3D11SamplerState : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11SamplerState, SD3D11DeviceChild)
	SD3D11SamplerState(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11Asynchronous : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Asynchronous, SD3D11DeviceChild)
	SD3D11Asynchronous(){}
	UINT STDMETHODCALLTYPE GetDataSize() { return this->m_pImpl->GetDataSize(); }
};

template<typename Impl, typename Base>
struct SD3D11Query : SD3D11Asynchronous<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Query, SD3D11Asynchronous)
	SD3D11Query(){}
	void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC* pDesc) { this->m_pImpl->GetDesc(pDesc); }
};

template<typename Impl, typename Base>
struct SD3D11ShaderReflectionType : Base
{
	DXGL_WRAPPER_ROOT_NO_COM(D3D11ShaderReflectionType)
	SD3D11ShaderReflectionType() : m_pImpl(NULL){}
	~SD3D11ShaderReflectionType(){}
	HRESULT STDMETHODCALLTYPE                     GetDesc(D3D11_SHADER_TYPE_DESC* pDesc)                 { return this->m_pImpl->GetDesc(pDesc); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByIndex(UINT Index)                       { return this->m_pImpl->GetMemberTypeByIndex(Index); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByName(LPCSTR Name)                       { return this->m_pImpl->GetMemberTypeByName(Name); }
	LPCSTR STDMETHODCALLTYPE                      GetMemberTypeName(UINT Index)                          { return this->m_pImpl->GetMemberTypeName(Index); }
	HRESULT STDMETHODCALLTYPE                     IsEqual(ID3D11ShaderReflectionType* pType)             { return this->m_pImpl->IsEqual(pType); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetSubType()                                           { return this->m_pImpl->GetSubType(); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetBaseClass()                                         { return this->m_pImpl->GetBaseClass(); }
	UINT STDMETHODCALLTYPE                        GetNumInterfaces()                                     { return this->m_pImpl->GetNumInterfaces(); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetInterfaceByIndex(UINT uIndex)                       { return this->m_pImpl->GetInterfaceByIndex(uIndex); }
	HRESULT STDMETHODCALLTYPE                     IsOfType(ID3D11ShaderReflectionType* pType)            { return this->m_pImpl->IsOfType(pType); }
	HRESULT STDMETHODCALLTYPE                     ImplementsInterface(ID3D11ShaderReflectionType* pBase) { return this->m_pImpl->ImplementsInterface(pBase); }
};

template<typename Impl, typename Base>
struct SD3D11ShaderReflectionVariable : Base
{
	DXGL_WRAPPER_ROOT_NO_COM(D3D11ShaderReflectionVariable)
	SD3D11ShaderReflectionVariable() : m_pImpl(NULL){}
	~SD3D11ShaderReflectionVariable(){}
	HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_VARIABLE_DESC* pDesc) { return this->m_pImpl->GetDesc(pDesc); }
	ID3D11ShaderReflectionType* STDMETHODCALLTYPE           GetType()                                  { return this->m_pImpl->GetType(); }
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetBuffer()                                { return this->m_pImpl->GetBuffer(); }
	UINT STDMETHODCALLTYPE                                  GetInterfaceSlot(UINT uArrayIndex)         { return this->m_pImpl->GetInterfaceSlot(uArrayIndex); }
};

template<typename Impl, typename Base>
struct SD3D11ShaderReflectionConstantBuffer : Base
{
	DXGL_WRAPPER_ROOT_NO_COM(D3D11ShaderReflectionConstantBuffer)
	SD3D11ShaderReflectionConstantBuffer() : m_pImpl(NULL){}
	~SD3D11ShaderReflectionConstantBuffer(){}
	HRESULT STDMETHODCALLTYPE                         GetDesc(D3D11_SHADER_BUFFER_DESC* pDesc) { return this->m_pImpl->GetDesc(pDesc); }
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByIndex(UINT Index)           { return this->m_pImpl->GetVariableByIndex(Index); }
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR Name)           { return this->m_pImpl->GetVariableByName(Name); }
};

template<typename Impl, typename Base>
struct SD3D11ShaderReflection : SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11ShaderReflection, SUnknown)
	SD3D11ShaderReflection(){}
	HRESULT STDMETHODCALLTYPE                               GetDesc(D3D11_SHADER_DESC* pDesc)                                                         { return this->m_pImpl->GetDesc(pDesc); }
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(UINT Index)                                                      { return this->m_pImpl->GetConstantBufferByIndex(Index); }
	ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(LPCSTR Name)                                                      { return this->m_pImpl->GetConstantBufferByName(Name); }
	HRESULT STDMETHODCALLTYPE                               GetResourceBindingDesc(UINT ResourceIndex, D3D11_SHADER_INPUT_BIND_DESC* pDesc)           { return this->m_pImpl->GetResourceBindingDesc(ResourceIndex, pDesc); }
	HRESULT STDMETHODCALLTYPE                               GetInputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc)         { return this->m_pImpl->GetInputParameterDesc(ParameterIndex, pDesc); }
	HRESULT STDMETHODCALLTYPE                               GetOutputParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc)        { return this->m_pImpl->GetOutputParameterDesc(ParameterIndex, pDesc); }
	HRESULT STDMETHODCALLTYPE                               GetPatchConstantParameterDesc(UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC* pDesc) { return this->m_pImpl->GetPatchConstantParameterDesc(ParameterIndex, pDesc); }
	ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE       GetVariableByName(LPCSTR Name)                                                            { return this->m_pImpl->GetVariableByName(Name); }
	HRESULT STDMETHODCALLTYPE                               GetResourceBindingDescByName(LPCSTR Name, D3D11_SHADER_INPUT_BIND_DESC* pDesc)            { return this->m_pImpl->GetResourceBindingDescByName(Name, pDesc); }
	UINT STDMETHODCALLTYPE                                  GetMovInstructionCount()                                                                  { return this->m_pImpl->GetMovInstructionCount(); }
	UINT STDMETHODCALLTYPE                                  GetMovcInstructionCount()                                                                 { return this->m_pImpl->GetMovcInstructionCount(); }
	UINT STDMETHODCALLTYPE                                  GetConversionInstructionCount()                                                           { return this->m_pImpl->GetConversionInstructionCount(); }
	UINT STDMETHODCALLTYPE                                  GetBitwiseInstructionCount()                                                              { return this->m_pImpl->GetBitwiseInstructionCount(); }
	D3D_PRIMITIVE STDMETHODCALLTYPE                         GetGSInputPrimitive()                                                                     { return this->m_pImpl->GetGSInputPrimitive(); }
	BOOL STDMETHODCALLTYPE                                  IsSampleFrequencyShader()                                                                 { return this->m_pImpl->IsSampleFrequencyShader(); }
	UINT STDMETHODCALLTYPE                                  GetNumInterfaceSlots()                                                                    { return this->m_pImpl->GetNumInterfaceSlots(); }
	HRESULT STDMETHODCALLTYPE                               GetMinFeatureLevel(D3D_FEATURE_LEVEL* pLevel)                                             { return this->m_pImpl->GetMinFeatureLevel(pLevel); }
};

template<typename Impl, typename Base>
struct SDXGIObject : SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIObject, SUnknown)
	SDXGIObject(){}
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)  { return this->m_pImpl->SetPrivateData(Name, DataSize, pData); }
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown) { return this->m_pImpl->SetPrivateDataInterface(Name, pUnknown); }
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)      { return this->m_pImpl->GetPrivateData(Name, pDataSize, pData); }
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent)                         { return this->m_pImpl->GetParent(riid, ppParent); }
};

template<typename Impl, typename Base>
struct SDXGIDeviceSubObject : SDXGIObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIDeviceSubObject, SDXGIObject)
	SDXGIDeviceSubObject(){}
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) { return this->m_pImpl->GetDevice(riid, ppDevice); }
};

template<typename Impl, typename Base>
struct SDXGIOutput : SDXGIObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIOutput, SDXGIObject)
	SDXGIOutput(){}
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* pDesc)                                                                                       { return this->m_pImpl->GetDesc(pDesc); }
	HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT* pNumModes, DXGI_MODE_DESC* pDesc)                         { return this->m_pImpl->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc); }
	HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch, DXGI_MODE_DESC* pClosestMatch, IUnknown* pConcernedDevice) { return this->m_pImpl->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice); }
	HRESULT STDMETHODCALLTYPE WaitForVBlank()                                                                                                        { return this->m_pImpl->WaitForVBlank(); }
	HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown* pDevice, BOOL Exclusive)                                                                       { return this->m_pImpl->TakeOwnership(pDevice, Exclusive); }
	void STDMETHODCALLTYPE    ReleaseOwnership()                                                                                                     { this->m_pImpl->ReleaseOwnership(); }
	HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps)                                               { return this->m_pImpl->GetGammaControlCapabilities(pGammaCaps); }
	HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL* pArray)                                                                      { return this->m_pImpl->SetGammaControl(pArray); }
	HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL* pArray)                                                                            { return this->m_pImpl->GetGammaControl(pArray); }
	HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface* pScanoutSurface)                                                                       { return this->m_pImpl->SetDisplaySurface(pScanoutSurface); }
	HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface* pDestination)                                                                      { return this->m_pImpl->GetDisplaySurfaceData(pDestination); }
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)                                                                      { return this->m_pImpl->GetFrameStatistics(pStats); }
};

template<typename Impl, typename Base>
struct SDXGIAdapter : SDXGIObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIAdapter, SDXGIObject)
	SDXGIAdapter(){}
	HRESULT STDMETHODCALLTYPE EnumOutputs(UINT Output, IDXGIOutput** ppOutput)                         { return this->m_pImpl->EnumOutputs(Output, ppOutput); }
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC* pDesc)                                        { return this->m_pImpl->GetDesc(pDesc); }
	HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER* pUMDVersion) { return this->m_pImpl->CheckInterfaceSupport(InterfaceName, pUMDVersion); }
};

template<typename Impl, typename Base>
struct SDXGIAdapter1 : SDXGIAdapter<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIAdapter1, SDXGIAdapter)
	SDXGIAdapter1(){}
	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* pDesc) { return this->m_pImpl->GetDesc1(pDesc); }
};

template<typename Impl, typename Base>
struct SDXGIFactory : SDXGIObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIFactory, SDXGIObject)
	SDXGIFactory(){}
	HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter** ppAdapter)                                          { return this->m_pImpl->EnumAdapters(Adapter, ppAdapter); }
	HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags)                                          { return this->m_pImpl->MakeWindowAssociation(WindowHandle, Flags); }
	HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* pWindowHandle)                                                     { return this->m_pImpl->GetWindowAssociation(pWindowHandle); }
	HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) { return this->m_pImpl->CreateSwapChain(pDevice, pDesc, ppSwapChain); }
	HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter** ppAdapter)                               { return this->m_pImpl->CreateSoftwareAdapter(Module, ppAdapter); }
};

template<typename Impl, typename Base>
struct SDXGIFactory1 : SDXGIFactory<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIFactory1, SDXGIFactory)
	SDXGIFactory1(){}
	HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT Adapter, IDXGIAdapter1** ppAdapter) { return this->m_pImpl->EnumAdapters1(Adapter, ppAdapter); }
	BOOL STDMETHODCALLTYPE    IsCurrent(void)                                        { return this->m_pImpl->IsCurrent(); }
};

template<typename Impl, typename Base>
struct SDXGIDevice : SDXGIObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGIDevice, SDXGIObject)
	SDXGIDevice(){}
	HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter** pAdapter)                                                                                                                      { return this->m_pImpl->GetAdapter(pAdapter); }
	HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC* pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE* pSharedResource, IDXGISurface** ppSurface) { return this->m_pImpl->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface); }
	HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown* const* ppResources, DXGI_RESIDENCY* pResidencyStatus, UINT NumResources)                                                { return this->m_pImpl->QueryResourceResidency(ppResources, pResidencyStatus, NumResources); }
	HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority)                                                                                                                       { return this->m_pImpl->SetGPUThreadPriority(Priority); }
	HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT* pPriority)                                                                                                                     { return this->m_pImpl->GetGPUThreadPriority(pPriority); }
};

template<typename Impl, typename Base>
struct SDXGISwapChain : SDXGIDeviceSubObject<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(DXGISwapChain, SDXGIDeviceSubObject)
	SDXGISwapChain(){}
	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags)                                                               { return this->m_pImpl->Present(SyncInterval, Flags); }
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface)                                                { return this->m_pImpl->GetBuffer(Buffer, riid, ppSurface); }
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget)                                            { return this->m_pImpl->SetFullscreenState(Fullscreen, pTarget); }
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget)                                        { return this->m_pImpl->GetFullscreenState(pFullscreen, ppTarget); }
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc)                                                                 { return this->m_pImpl->GetDesc(pDesc); }
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) { return this->m_pImpl->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags); }
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)                                             { return this->m_pImpl->ResizeTarget(pNewTargetParameters); }
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput)                                                          { return this->m_pImpl->GetContainingOutput(ppOutput); }
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)                                                    { return this->m_pImpl->GetFrameStatistics(pStats); }
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount)                                                         { return this->m_pImpl->GetLastPresentCount(pLastPresentCount); }
};

template<typename Impl, typename Base>
struct SD3D11SwitchToRef : public SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11SwitchToRef, SUnknown)
	SD3D11SwitchToRef(){}
	BOOL STDMETHODCALLTYPE SetUseRef(BOOL UseRef) { return this->m_pImpl->SetUseRef(UseRef); }
	BOOL STDMETHODCALLTYPE GetUseRef()            { return this->m_pImpl->GetUseRef(); }
};

template<typename Impl, typename Base>
struct SD3D11Device : public SUnknown<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11Device, SUnknown)
	HRESULT STDMETHODCALLTYPE           CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)                                                                                                                                                                                           { return this->m_pImpl->CreateBuffer(pDesc, pInitialData, ppBuffer); }
	HRESULT STDMETHODCALLTYPE           CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)                                                                                                                                                                               { return this->m_pImpl->CreateTexture1D(pDesc, pInitialData, ppTexture1D); }
	HRESULT STDMETHODCALLTYPE           CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)                                                                                                                                                                               { return this->m_pImpl->CreateTexture2D(pDesc, pInitialData, ppTexture2D); }
	HRESULT STDMETHODCALLTYPE           CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)                                                                                                                                                                               { return this->m_pImpl->CreateTexture3D(pDesc, pInitialData, ppTexture3D); }
	HRESULT STDMETHODCALLTYPE           CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)                                                                                                                                                                      { return this->m_pImpl->CreateShaderResourceView(pResource, pDesc, ppSRView); }
	HRESULT STDMETHODCALLTYPE           CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)                                                                                                                                                                   { return this->m_pImpl->CreateUnorderedAccessView(pResource, pDesc, ppUAView); }
	HRESULT STDMETHODCALLTYPE           CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)                                                                                                                                                                            { return this->m_pImpl->CreateRenderTargetView(pResource, pDesc, ppRTView); }
	HRESULT STDMETHODCALLTYPE           CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                                  { return this->m_pImpl->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView); }
	HRESULT STDMETHODCALLTYPE           CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)                                                                                                            { return this->m_pImpl->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout); }
	HRESULT STDMETHODCALLTYPE           CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)                                                                                                                                                              { return this->m_pImpl->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader); }
	HRESULT STDMETHODCALLTYPE           CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)                                                                                                                                                        { return this->m_pImpl->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader); }
	HRESULT STDMETHODCALLTYPE           CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader) { return this->m_pImpl->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader); }
	HRESULT STDMETHODCALLTYPE           CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)                                                                                                                                                                 { return this->m_pImpl->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader); }
	HRESULT STDMETHODCALLTYPE           CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)                                                                                                                                                                    { return this->m_pImpl->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader); }
	HRESULT STDMETHODCALLTYPE           CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)                                                                                                                                                              { return this->m_pImpl->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader); }
	HRESULT STDMETHODCALLTYPE           CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)                                                                                                                                                           { return this->m_pImpl->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader); }
	HRESULT STDMETHODCALLTYPE           CreateClassLinkage(ID3D11ClassLinkage** ppLinkage)                                                                                                                                                                                                                                                          { return this->m_pImpl->CreateClassLinkage(ppLinkage); }
	HRESULT STDMETHODCALLTYPE           CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)                                                                                                                                                                                                                  { return this->m_pImpl->CreateBlendState(pBlendStateDesc, ppBlendState); }
	HRESULT STDMETHODCALLTYPE           CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)                                                                                                                                                                                   { return this->m_pImpl->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState); }
	HRESULT STDMETHODCALLTYPE           CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                              { return this->m_pImpl->CreateRasterizerState(pRasterizerDesc, ppRasterizerState); }
	HRESULT STDMETHODCALLTYPE           CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)                                                                                                                                                                                                             { return this->m_pImpl->CreateSamplerState(pSamplerDesc, ppSamplerState); }
	HRESULT STDMETHODCALLTYPE           CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)                                                                                                                                                                                                                                      { return this->m_pImpl->CreateQuery(pQueryDesc, ppQuery); }
	HRESULT STDMETHODCALLTYPE           CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)                                                                                                                                                                                                                      { return this->m_pImpl->CreatePredicate(pPredicateDesc, ppPredicate); }
	HRESULT STDMETHODCALLTYPE           CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)                                                                                                                                                                                                                            { return this->m_pImpl->CreateCounter(pCounterDesc, ppCounter); }
	HRESULT STDMETHODCALLTYPE           CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)                                                                                                                                                                                                                           { return this->m_pImpl->CreateDeferredContext(ContextFlags, ppDeferredContext); }
	HRESULT STDMETHODCALLTYPE           OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource)                                                                                                                                                                                                                           { return this->m_pImpl->OpenSharedResource(hResource, ReturnedInterface, ppResource); }
	HRESULT STDMETHODCALLTYPE           CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport)                                                                                                                                                                                                                                                { return this->m_pImpl->CheckFormatSupport(Format, pFormatSupport); }
	HRESULT STDMETHODCALLTYPE           CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)                                                                                                                                                                                                                { return this->m_pImpl->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels); }
	void STDMETHODCALLTYPE              CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo)                                                                                                                                                                                                                                                          { this->m_pImpl->CheckCounterInfo(pCounterInfo); }
	HRESULT STDMETHODCALLTYPE           CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits, UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength)                                                                                          { return this->m_pImpl->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength); }
	HRESULT STDMETHODCALLTYPE           CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)                                                                                                                                                                                                          { return this->m_pImpl->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize); }
	HRESULT STDMETHODCALLTYPE           GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)                                                                                                                                                                                                                                                  { return this->m_pImpl->GetPrivateData(guid, pDataSize, pData); }
	HRESULT STDMETHODCALLTYPE           SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)                                                                                                                                                                                                                                              { return this->m_pImpl->SetPrivateData(guid, DataSize, pData); }
	HRESULT STDMETHODCALLTYPE           SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)                                                                                                                                                                                                                                                { return this->m_pImpl->SetPrivateDataInterface(guid, pData); }
	D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(void)                                                                                                                                                                                                                                                                                       { return this->m_pImpl->GetFeatureLevel(); }
	UINT STDMETHODCALLTYPE              GetCreationFlags(void)                                                                                                                                                                                                                                                                                      { return this->m_pImpl->GetCreationFlags(); }
	HRESULT STDMETHODCALLTYPE           GetDeviceRemovedReason(void)                                                                                                                                                                                                                                                                                { return this->m_pImpl->GetDeviceRemovedReason(); }
	void STDMETHODCALLTYPE              GetImmediateContext(ID3D11DeviceContext** ppImmediateContext)                                                                                                                                                                                                                                               { this->m_pImpl->GetImmediateContext(ppImmediateContext); }
	HRESULT STDMETHODCALLTYPE           SetExceptionMode(UINT RaiseFlags)                                                                                                                                                                                                                                                                           { return this->m_pImpl->SetExceptionMode(RaiseFlags); }
	UINT STDMETHODCALLTYPE              GetExceptionMode(void)                                                                                                                                                                                                                                                                                      { return this->m_pImpl->GetExceptionMode(); }
};

template<typename Impl, typename Base>
struct SD3D11DeviceContext : SD3D11DeviceChild<Impl, Base>
{
	DXGL_WRAPPER_DERIVED(D3D11DeviceContext, SD3D11DeviceChild)
	void STDMETHODCALLTYPE                      VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                               { this->m_pImpl->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->PSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             { this->m_pImpl->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)                                                                                                                                                                                                    { this->m_pImpl->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation); }
	void STDMETHODCALLTYPE                      Draw(UINT VertexCount, UINT StartVertexLocation)                                                                                                                                                                                                                                 { this->m_pImpl->Draw(VertexCount, StartVertexLocation); }
	HRESULT STDMETHODCALLTYPE                   Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)                                                                                                                                                    { return this->m_pImpl->Map(pResource, Subresource, MapType, MapFlags, pMappedResource); }
	void STDMETHODCALLTYPE                      Unmap(ID3D11Resource* pResource, UINT Subresource)                                                                                                                                                                                                                               { this->m_pImpl->Unmap(pResource, Subresource); }
	void STDMETHODCALLTYPE                      PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      IASetInputLayout(ID3D11InputLayout* pInputLayout)                                                                                                                                                                                                                                { this->m_pImpl->IASetInputLayout(pInputLayout); }
	void STDMETHODCALLTYPE                      IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)                                                                                                                                            { this->m_pImpl->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets); }
	void STDMETHODCALLTYPE                      IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)                                                                                                                                                                                                    { this->m_pImpl->IASetIndexBuffer(pIndexBuffer, Format, Offset); }
	void STDMETHODCALLTYPE                      DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)                                                                                                                                { this->m_pImpl->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation); }
	void STDMETHODCALLTYPE                      DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)                                                                                                                                                             { this->m_pImpl->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation); }
	void STDMETHODCALLTYPE                      GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 { this->m_pImpl->GSSetShader(pShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)                                                                                                                                                                                                                        { this->m_pImpl->IASetPrimitiveTopology(Topology); }
	void STDMETHODCALLTYPE                      VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->VSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      Begin(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                { this->m_pImpl->Begin(pAsync); }
	void STDMETHODCALLTYPE                      End(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                  { this->m_pImpl->End(pAsync); }
	HRESULT STDMETHODCALLTYPE                   GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)                                                                                                                                                                                               { return this->m_pImpl->GetData(pAsync, pData, DataSize, GetDataFlags); }
	void STDMETHODCALLTYPE                      SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue)                                                                                                                                                                                                                 { this->m_pImpl->SetPredication(pPredicate, PredicateValue); }
	void STDMETHODCALLTYPE                      GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->GSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)                                                                                                                                                 { this->m_pImpl->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView); }
	void STDMETHODCALLTYPE                      OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) { this->m_pImpl->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts); }
	void STDMETHODCALLTYPE                      OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)                                                                                                                                                                                      { this->m_pImpl->OMSetBlendState(pBlendState, BlendFactor, SampleMask); }
	void STDMETHODCALLTYPE                      OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)                                                                                                                                                                                             { this->m_pImpl->OMSetDepthStencilState(pDepthStencilState, StencilRef); }
	void STDMETHODCALLTYPE                      SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)                                                                                                                                                                                            { this->m_pImpl->SOSetTargets(NumBuffers, ppSOTargets, pOffsets); }
	void STDMETHODCALLTYPE                      DrawAuto(void)                                                                                                                                                                                                                                                                   { this->m_pImpl->DrawAuto(); }
	void STDMETHODCALLTYPE                      DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                        { this->m_pImpl->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs); }
	void STDMETHODCALLTYPE                      DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                               { this->m_pImpl->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs); }
	void STDMETHODCALLTYPE                      Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)                                                                                                                                                                                                 { this->m_pImpl->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ); }
	void STDMETHODCALLTYPE                      DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                                    { this->m_pImpl->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs); }
	void STDMETHODCALLTYPE                      RSSetState(ID3D11RasterizerState* pRasterizerState)                                                                                                                                                                                                                              { this->m_pImpl->RSSetState(pRasterizerState); }
	void STDMETHODCALLTYPE                      RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                              { this->m_pImpl->RSSetViewports(NumViewports, pViewports); }
	void STDMETHODCALLTYPE                      RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects)                                                                                                                                                                                                                       { this->m_pImpl->RSSetScissorRects(NumRects, pRects); }
	void STDMETHODCALLTYPE                      CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox)                                                                                           { this->m_pImpl->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox); }
	void STDMETHODCALLTYPE                      CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)                                                                                                                                                                                                         { this->m_pImpl->CopyResource(pDstResource, pSrcResource); }
	void STDMETHODCALLTYPE                      UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)                                                                                                                       { this->m_pImpl->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch); }
	void STDMETHODCALLTYPE                      CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)                                                                                                                                                                     { this->m_pImpl->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView); }
	void STDMETHODCALLTYPE                      ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])                                                                                                                                                                                       { this->m_pImpl->ClearRenderTargetView(pRenderTargetView, ColorRGBA); }
	void STDMETHODCALLTYPE                      ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])                                                                                                                                                                              { this->m_pImpl->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values); }
	void STDMETHODCALLTYPE                      ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])                                                                                                                                                                            { this->m_pImpl->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values); }
	void STDMETHODCALLTYPE                      ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)                                                                                                                                                                    { this->m_pImpl->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil); }
	void STDMETHODCALLTYPE                      GenerateMips(ID3D11ShaderResourceView* pShaderResourceView)                                                                                                                                                                                                                      { this->m_pImpl->GenerateMips(pShaderResourceView); }
	void STDMETHODCALLTYPE                      SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD)                                                                                                                                                                                                                       { this->m_pImpl->SetResourceMinLOD(pResource, MinLOD); }
	FLOAT STDMETHODCALLTYPE                     GetResourceMinLOD(ID3D11Resource* pResource)                                                                                                                                                                                                                                     { return this->m_pImpl->GetResourceMinLOD(pResource); }
	void STDMETHODCALLTYPE                      ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)                                                                                                                                     { this->m_pImpl->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format); }
	void STDMETHODCALLTYPE                      ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState)                                                                                                                                                                                                    { this->m_pImpl->ExecuteCommandList(pCommandList, RestoreContextState); }
	void STDMETHODCALLTYPE                      HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 { this->m_pImpl->HSSetShader(pHullShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->HSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             { this->m_pImpl->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->DSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      { this->m_pImpl->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)                                                                                                                                 { this->m_pImpl->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts); }
	void STDMETHODCALLTYPE                      CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                           { this->m_pImpl->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances); }
	void STDMETHODCALLTYPE                      CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           { this->m_pImpl->CSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    { this->m_pImpl->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                 { this->m_pImpl->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->PSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               { this->m_pImpl->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      IAGetInputLayout(ID3D11InputLayout** ppInputLayout)                                                                                                                                                                                                                              { this->m_pImpl->IAGetInputLayout(ppInputLayout); }
	void STDMETHODCALLTYPE                      IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)                                                                                                                                                              { this->m_pImpl->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets); }
	void STDMETHODCALLTYPE                      IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)                                                                                                                                                                                                 { this->m_pImpl->IAGetIndexBuffer(pIndexBuffer, Format, Offset); }
	void STDMETHODCALLTYPE                      GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                           { this->m_pImpl->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology)                                                                                                                                                                                                                      { this->m_pImpl->IAGetPrimitiveTopology(pTopology); }
	void STDMETHODCALLTYPE                      VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->VSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)                                                                                                                                                                                                             { this->m_pImpl->GetPredication(ppPredicate, pPredicateValue); }
	void STDMETHODCALLTYPE                      GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->GSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                     { this->m_pImpl->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView); }
	void STDMETHODCALLTYPE                      OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                          { this->m_pImpl->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews); }
	void STDMETHODCALLTYPE                      OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)                                                                                                                                                                                        { this->m_pImpl->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask); }
	void STDMETHODCALLTYPE                      OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)                                                                                                                                                                                         { this->m_pImpl->OMGetDepthStencilState(ppDepthStencilState, pStencilRef); }
	void STDMETHODCALLTYPE                      SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets)                                                                                                                                                                                                                        { this->m_pImpl->SOGetTargets(NumBuffers, ppSOTargets); }
	void STDMETHODCALLTYPE                      RSGetState(ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                                                            { this->m_pImpl->RSGetState(ppRasterizerState); }
	void STDMETHODCALLTYPE                      RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                                  { this->m_pImpl->RSGetViewports(pNumViewports, pViewports); }
	void STDMETHODCALLTYPE                      RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects)                                                                                                                                                                                                                           { this->m_pImpl->RSGetScissorRects(pNumRects, pRects); }
	void STDMETHODCALLTYPE                      HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                   { this->m_pImpl->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->HSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               { this->m_pImpl->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->DSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            { this->m_pImpl->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
	void STDMETHODCALLTYPE                      CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                                                                                                                                                      { this->m_pImpl->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews); }
	void STDMETHODCALLTYPE                      CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                             { this->m_pImpl->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances); }
	void STDMETHODCALLTYPE                      CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 { this->m_pImpl->CSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
	void STDMETHODCALLTYPE                      CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          { this->m_pImpl->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
	void STDMETHODCALLTYPE                      ClearState(void)                                                                                                                                                                                                                                                                 { this->m_pImpl->ClearState(); }
	void STDMETHODCALLTYPE                      Flush(void)                                                                                                                                                                                                                                                                      { this->m_pImpl->Flush(); }
	D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(void)                                                                                                                                                                                                                                                                    { return this->m_pImpl->GetType(); }
	UINT STDMETHODCALLTYPE                      GetContextFlags(void)                                                                                                                                                                                                                                                            { return this->m_pImpl->GetContextFlags(); }
	HRESULT STDMETHODCALLTYPE                   FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList)                                                                                                                                                                                           { return this->m_pImpl->FinishCommandList(RestoreDeferredContextState, ppCommandList); }
};

} //namespace NDXGLWrappers

	#undef DXGL_WRAPPER_ROOT_NO_COM
	#undef DXGL_WRAPPER_ROOT
	#undef DXGL_WRAPPER_DERIVED

	#define DXGL_IMPLEMENT_INTERFACE(_Class, _Interface)                                                  \
	  typedef NDXGLWrappers::S ## _Interface<_Class, I ## _Interface> T ## _Interface ## Wrapper;         \
	  T ## _Interface ## Wrapper m_k ## _Interface ## Wrapper;                                            \
	  I ## _Interface * m_pVirtual ## _Interface ## Wrapper;                                              \
	  static ILINE void ToInterface(I ## _Interface * *ppInterface, _Class * pObject)                     \
	  {                                                                                                   \
	    *ppInterface = (pObject == NULL ? NULL : pObject->m_pVirtual ## _Interface ## Wrapper);           \
	  }                                                                                                   \
	  static ILINE _Class* FromInterface(I ## _Interface * pInterface)                                    \
	  {                                                                                                   \
	    return pInterface == NULL ? NULL : static_cast<T ## _Interface ## Wrapper*>(pInterface)->m_pImpl; \
	  }
	#define DXGL_INITIALIZE_INTERFACE(_Interface) \
	  m_k ## _Interface ## Wrapper.InitializeWrapper(this);

#else

	#define DXGL_IMPLEMENT_INTERFACE(_Class, _Interface)                              \
	  static ILINE void ToInterface(I ## _Interface * *ppInterface, _Class * pObject) \
	  {                                                                               \
	    *ppInterface = pObject;                                                       \
	  }                                                                               \
	  static ILINE _Class* FromInterface(I ## _Interface * pInterface)                \
	  {                                                                               \
	    return static_cast<_Class*>(pInterface);                                      \
	  }
	#define DXGL_INITIALIZE_INTERFACE(_Interface)

#endif

#endif //__DXEmulation__
