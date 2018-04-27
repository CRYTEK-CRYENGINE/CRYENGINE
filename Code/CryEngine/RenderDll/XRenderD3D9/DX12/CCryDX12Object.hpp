// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "API/DX12Base.hpp"

#define DX12_BASE_OBJECT(className, interfaceName)         \
	typedef className     Class;                           \
	typedef               DX12_PTR (Class) Ptr;            \
	typedef               DX12_PTR (const Class) ConstPtr; \
	typedef interfaceName Super;                           \
	typedef interfaceName Interface;

#define DX12_OBJECT(className, superName)              \
	typedef className Class;                           \
	typedef           DX12_PTR (Class) Ptr;            \
	typedef           DX12_PTR (const Class) ConstPtr; \
	typedef superName Super;

#include "CryDX12Guid.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CCryDX12Buffer;
class CCryDX12DepthStencilView;
class CCryDX12Device;
class CCryDX12DeviceContext;
class CCryDX12MemoryManager;
class CCryDX12Query;
class CCryDX12RenderTargetView;
class CCryDX12SamplerState;
class CCryDX12Shader;
class CCryDX12ShaderResourceView;
class CCryDX12SwapChain;
class CCryDX12Texture1D;
class CCryDX12Texture2D;
class CCryDX12Texture3D;
class CCryDX12UnorderedAccessView;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class CCryDX12Object : public T
{
public:
	DX12_BASE_OBJECT(CCryDX12Object, T);

	CCryDX12Object()
		: m_RefCount(0)
	{

	}

	virtual ~CCryDX12Object()
	{
		DX12_LOG(g_nPrintDX12, "CCryDX12 object destroyed: %p", this);
	}

	#pragma region /* IUnknown implementation */

	VIRTUALGFX ULONG STDMETHODCALLTYPE AddRef() FINALGFX
	{
		return CryInterlockedIncrement(&m_RefCount);
	}

	VIRTUALGFX ULONG STDMETHODCALLTYPE Release() FINALGFX
	{
		ULONG RefCount;
		if (!(RefCount = CryInterlockedDecrement(&m_RefCount)))
		{
			delete this;
			return 0;
		}

		return RefCount;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE QueryInterface(
	  REFIID riid,
	  void** ppvObject) FINALGFX
	{
		if (
		  (riid == __uuidof(T)) ||
		  (riid == __uuidof(ID3D11Device       ) && __uuidof(ID3D11Device1ToImplement       ) == __uuidof(T)) ||
		  (riid == __uuidof(ID3D11DeviceContext) && __uuidof(ID3D11DeviceContext1ToImplement) == __uuidof(T))
		)
		{
			if (ppvObject)
			{
				*reinterpret_cast<T**>(ppvObject) = static_cast<T*>(this);
				static_cast<T*>(this)->AddRef();
			}

			return S_OK;
		}

		return E_NOINTERFACE;
	}

	#pragma endregion

private:
	int m_RefCount;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class CCryDX12GIObject : public T
{
public:
	DX12_BASE_OBJECT(CCryDX12GIObject, T);

	CCryDX12GIObject()
		: m_RefCount(0)
	{

	}

	virtual ~CCryDX12GIObject()
	{

	}

	#pragma region /* IUnknown implementation */

	VIRTUALGFX ULONG STDMETHODCALLTYPE AddRef() FINALGFX
	{
		return CryInterlockedIncrement(&m_RefCount);
	}

	VIRTUALGFX ULONG STDMETHODCALLTYPE Release() FINALGFX
	{
		ULONG RefCount;
		if (!(RefCount = CryInterlockedDecrement(&m_RefCount)))
		{
			delete this;
			return 0;
		}

		return RefCount;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE QueryInterface(
	  REFIID riid,
	  void** ppvObject) FINALGFX
	{
		if (
		  (riid == __uuidof(T)) ||
#if !CRY_PLATFORM_DURANGO
		  (riid == __uuidof(IDXGIDevice   ) && __uuidof(IDXGIDevice3   ) == __uuidof(T)) ||
#endif
		  (riid == __uuidof(IDXGIFactory  ) && __uuidof(IDXGIFactory4  ) == __uuidof(T)) ||
		  (riid == __uuidof(IDXGIAdapter  ) && __uuidof(IDXGIAdapter3  ) == __uuidof(T)) ||
		  (riid == __uuidof(IDXGIOutput   ) && __uuidof(IDXGIOutput4   ) == __uuidof(T)) ||
		  (riid == __uuidof(IDXGISwapChain) && __uuidof(IDXGISwapChain3) == __uuidof(T))
		)
		{
			if (ppvObject)
			{
				*reinterpret_cast<T**>(ppvObject) = static_cast<T*>(this);
				static_cast<T*>(this)->AddRef();
			}

			return S_OK;
		}

		return E_NOINTERFACE;
	}

	#pragma endregion

	#pragma region /* IDXGIObject implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID Name,
	  UINT DataSize,
	  _In_reads_bytes_(DataSize) const void* pData) FINALGFX
	{
		return -1;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID Name,
	  _In_ const IUnknown* pUnknown) FINALGFX
	{
		return -1;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID Name,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_(*pDataSize) void* pData) FINALGFX
	{
		return -1;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetParent(
	  _In_ REFIID riid,
	  _Out_ void** ppParent) FINALGFX
	{
		return -1;
	}

	#pragma endregion

private:
	int m_RefCount;
};
