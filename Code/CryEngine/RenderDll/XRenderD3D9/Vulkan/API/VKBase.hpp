// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "xxhash.h"
#include <fasthash/fasthash.inl>
#include <concqueue/concqueue.hpp>

#ifndef _RELEASE // TODO: Make this #ifdef _DEBUG?

	#define VK_LOG(cond, ...) \
	  do { if (cond) CryLog("Vulkan Log: " __VA_ARGS__); } while (0)
	#define VK_ERROR(...) \
	  do { CryLog("Vulkan Error: " __VA_ARGS__); } while (0)
	#define VK_WARNING(...) \
	  do { CryLog("Vulkan Warning: " __VA_ARGS__); } while (0)
	#define VK_FUNC_LOG() \
	  do { if (VK_FUNCTION_LOGGING) CryLog("Vulkan function call: %s", __FUNC__); } while (0)

	#define VK_ASSERT(cond, ...) \
	  CRY_ASSERT_MESSAGE(cond, "VK_ASSERT " __VA_ARGS__)

	#define VK_NOT_IMPLEMENTED //VK_ASSERT(0, "Not implemented!");

#else

	#define VK_LOG(cond, ...)     do {} while (0)
	#define VK_ERROR(...)         do {} while (0)
	#define VK_WARNING(cond, ...) do {} while (0)
	#define VK_FUNC_LOG()         do {} while (0)

	#define VK_ASSERT(cond, ...)

	#define VK_NOT_IMPLEMENTED
#endif

// TODO: remove once legacy pipeline is gone
#define DX12_MAP_DISCARD_MARKER       BIT(3)
#define DX12_COPY_REVERTSTATE_MARKER  BIT(2)
#define DX12_COPY_PIXELSTATE_MARKER   BIT(3)
#define DX12_COPY_CONCURRENT_MARKER   BIT(4)
#define DX12_RESOURCE_FLAG_OVERLAP    BIT(1)

//Should the VK objects be reference counted? For now use raw types
#define VK_PTR(T) _smart_ptr<T>

#define VK_PRECISE_DEDUPLICATION
#define VK_OMITTABLE_COMMANDLISTS
#define VK_CONCURRENCY_ANALYZER false
#define VK_FENCE_ANALYZER       false
#define VK_BARRIER_ANALYZER     false
#define VK_FUNCTION_LOGGING     false

namespace NCryVulkan
{

// Forward declarations
class CCommandList;
class CCommandListPool;
class CDevice;

//platform dependent surface creation info
struct SSurfaceCreationInfo
{
#ifdef CRY_PLATFORM_WINDOWS
	HWND      windowHandle;
	HINSTANCE appHandle;
#elif USE_SDL2_VIDEO && CRY_PLATFORM_ANDROID
	SDL_Window* pWindow;
	ANativeWindow* pNativeWindow;
#else
	#error  "Not implemented!""
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef uint32_t THash;
template<size_t length>
ILINE THash ComputeSmallHash(const void* data, UINT seed = 666)
{
	return fasthash::fasthash32<length>(data, seed);
}

//---------------------------------------------------------------------------------------------------------------------

class CRefCounted : public IUnknown
{
public:
	CRefCounted() : m_refCount(1) {}
	CRefCounted(CRefCounted&&) noexcept { /* refcount not copied on purpose*/ }
	CRefCounted& operator=(CRefCounted&&) noexcept { /* refcount not copied on purpose*/ return *this; }
	virtual ~CRefCounted() {}

	virtual ULONG AddRef() final threadsafe
	{
		return (ULONG)CryInterlockedIncrement(&m_refCount);
	}

	virtual ULONG Release() final threadsafe
	{
		const int newRefCount = CryInterlockedDecrement(&m_refCount);
		if (newRefCount == 0)
		{
			Destroy();
		}
		return (ULONG)newRefCount;
	}

	virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) final
	{
		*ppvObject = nullptr;
		return E_FAIL;
	}

	bool IsDeletable() const { return m_refCount == 0; }
	bool IsUniquelyOwned() const { return m_refCount == 1; }

protected:
	virtual void Destroy()
	{
		delete this;
	}

private:
	volatile int m_refCount = 1;
};

class CDeviceObject : public CRefCounted
{
public:
	CDevice* GetDevice() const
	{
		return m_pDevice;
	}

	CDeviceObject(CDevice* device) : m_pDevice(device) {}
	CDeviceObject(CDeviceObject&& r) noexcept = default;
	CDeviceObject& operator=(CDeviceObject&& r) noexcept = default;
	virtual ~CDeviceObject() override {}

	HRESULT SetPrivateData(REFGUID, UINT, const void*); // Legacy, to be removed!

private:
	CDevice* m_pDevice;
};

// Light-weight wrapper for Vulkan handle types.
// - T must be a Vulkan handle type (ie, VkImage).
// - CAutoHandle is movable, non-copyable type -> can be stored as a member in aggregate types, and let the compiler generate move-ctors and assignment-operators.
// - CAutoHandle validates that you don't leak the object (you still must call Detach/Destroy before the object is destroyed).
// - No automatic destruction since we don't cache the VkDevice instance (it would double the size of the object).
template<typename T>
class CAutoHandle
{
public:
	typedef VKAPI_ATTR void (VKAPI_CALL * TDestroyFunction)(VkDevice, T, const VkAllocationCallbacks*);

	CAutoHandle(T value = VK_NULL_HANDLE) : value(value) {}
	CAutoHandle(const CAutoHandle&) = delete;
	CAutoHandle(CAutoHandle&& other) noexcept : value(other.value) { other.value = VK_NULL_HANDLE; }
	CAutoHandle& operator=(const CAutoHandle&) = delete;
	CAutoHandle& operator=(CAutoHandle&& other) noexcept
	{
		CheckEmpty();
		value = other.value;
		other.value = VK_NULL_HANDLE;
		return *this;
	}

	~CAutoHandle()
	{
		CheckEmpty();
	}

	void CheckEmpty()
	{
		// If this triggers, you failed to clean up the handle, and you probably leaked the contents.
		VK_ASSERT(value == VK_NULL_HANDLE && "Attempt to overwrite or destroy non-empty handle, contained object has been leaked!");
	}

	// Cast to handle-type allowed
	operator T() const { return value; }

	// vkCreateXXX() take T* as an output parameter, which we can emplace directly, and also check for overwrite.
	T* operator&() { CheckEmpty(); return &value; }

	// Returns the (potentially null) raw handle, afterwards this object is empty.
	T Detach()
	{
		T result = value;
		value = VK_NULL_HANDLE;
		return result;
	}

	// Syntactic sugar for "if (handle) { vkDestroyXXX(device, handle.Detach(), nullptr); }
	// We use the symmetry of Vulkan's vkDestroyXXX function-signatures to enforce type-safety (ie, you can only pass the right function)
	void Destroy(TDestroyFunction pDestroyFunction, VkDevice device)
	{
		if (value != VK_NULL_HANDLE)
		{
			pDestroyFunction(device, value, nullptr);
			value = VK_NULL_HANDLE;
		}
	}

private:
	T value;
};

VkFormat    ConvertFormat(DXGI_FORMAT format);
DXGI_FORMAT ConvertFormat(VkFormat format);

}

//#include "VKDevice.hpp"
