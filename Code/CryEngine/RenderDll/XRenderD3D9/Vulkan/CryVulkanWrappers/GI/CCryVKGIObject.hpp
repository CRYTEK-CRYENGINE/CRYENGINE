// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

//Adapted from DX12 implementation. CCRYVK* objects are refcounted and mimic DXGI/DX behavior

#include "Vulkan/API/VKBase.hpp"

//-- Macros
//---------------------------------------------------------------------------------------------------------------------
#define IMPLEMENT_INTERFACES(...)                                         \
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(                       \
    REFIID riid,                                                          \
    void** ppvObject)                                                     \
  {                                                                       \
    bool found = checkInterfaces<__VA_ARGS__>(this, riid, ppvObject); \
    return found ? S_OK : E_NOINTERFACE;                                  \
  }

//-- Helper functions
//---------------------------------------------------------------------------------------------------------------------
template<typename Interface>
bool checkInterfaces(void* instance, REFIID riid, void** ppvObject)
{
	if (riid == __uuidof(Interface))
	{
		if (ppvObject)
		{
			*reinterpret_cast<Interface**>(ppvObject) = static_cast<Interface*>(instance);
			static_cast<Interface*>(instance)->AddRef();
			return true;
		}
		else
		{
			return false;
		}
	}
	return false;
}
//---------------------------------------------------------------------------------------------------------------------
template<typename Interface1, typename Interface2, typename ... Interfaces>
bool checkInterfaces(void* instance, REFIID riid, void** ppvObject)
{
	if (checkInterfaces<Interface1>(instance, riid, ppvObject))
	{
		return true;
	}
	else
	{
		return checkInterfaces<Interface2, Interfaces ...>(instance, riid, ppvObject);

	}
}

//---------------------------------------------------------------------------------------------------------------------
template<typename T>
static T* PassAddRef(T* ptr)
{
	if (ptr)
	{
		ptr->AddRef();
	}

	return ptr;
}

//---------------------------------------------------------------------------------------------------------------------
template<typename T>
static T* PassAddRef(const _smart_ptr<T>& ptr)
{
	if (ptr)
	{
		ptr.get()->AddRef();
	}

	return ptr.get();
}

//-- CCryVK* Base types
//---------------------------------------------------------------------------------------------------------------------
#ifndef CRY_PLATFORM_WINDOWS
#include "DX12\Includes\Unknwn_empty.h"
#else
#include <Unknwnbase.h>
#endif
//---------------------------------------------------------------------------------------------------------------------
class CCryVKObject : public IUnknown
{
public:

	CCryVKObject()
		: m_RefCount(0)
	{

	}

	virtual ~CCryVKObject()
	{

	}

	#pragma region /* IUnknown implementation */

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return CryInterlockedIncrement(&m_RefCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		ULONG RefCount;
		if (!(RefCount = CryInterlockedDecrement(&m_RefCount)))
		{
			delete this;
			return 0;
		}

		return RefCount;
	}

	#pragma endregion

private:
	int m_RefCount;
};

//---------------------------------------------------------------------------------------------------------------------

class CCryVKGIObject : public CCryVKObject
{
public:
	CCryVKGIObject() { AddRef(); } // High level code assumes GI objects are create with refcount 1

	#pragma region /* IDXGIObject implementation */

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID Name,
	  UINT DataSize,
	  _In_reads_bytes_(DataSize) const void* pData)
	{
		return -1;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID Name,
	  _In_ const IUnknown* pUnknown)
	{
		return -1;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID Name,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_(*pDataSize) void* pData)
	{
		return -1;
	}

	virtual HRESULT STDMETHODCALLTYPE GetParent(
	  _In_ REFIID riid,
	  _Out_ void** ppParent)
	{
		return -1;
	}

	#pragma endregion
};
