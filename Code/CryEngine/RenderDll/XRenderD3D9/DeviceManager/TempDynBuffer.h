// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TEMPDYNBUFFER_H_
#define _TEMPDYNBUFFER_H_

#pragma once

#include "../Common/DevBuffer.h"

extern CD3D9Renderer gcpRendD3D;

namespace TempDynBuffer
{
enum State
{
	Default,
	Allocated,
	Locked,
	Filled
};

struct ValidatorNull
{
	void Check(State)         {}
	void Check2(State, State) {}
	void Set(State)           {}
};

struct ValidatorDbgBreak
{
	ValidatorDbgBreak() : m_state(Default) {}

	void Check(State expectedState)
	{
		if (m_state != expectedState)
			__debugbreak();
	}

	void Check2(State expectedState0, State expectedState1)
	{
		if (m_state != expectedState0 && m_state != expectedState1)
			__debugbreak();
	}

	void Set(State newState)
	{
		m_state = newState;
	}

	State m_state;
};

#if !defined(_RELEASE)
typedef ValidatorDbgBreak ValidatorDefault;
#else
typedef ValidatorNull     ValidatorDefault;
#endif

//////////////////////////////////////////////////////////////////////////
template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
class TempDynBufferBase
{
public:
	static const buffer_handle_t invalidHandle = ~0u;
	buffer_handle_t GetHandle() const { return m_handle; }

public:
	buffer_handle_t GetBufferHandle() const { return m_handle; };

protected:
	TempDynBufferBase() : m_handle(invalidHandle), m_numElements(0), m_DevBufMan(&gcpRendD3D->m_DevBufMan), m_validator() {}
	~TempDynBufferBase() { m_validator.Check(Default); }

	void Allocate(buffer_size_t numElements) { AllocateInternal(numElements, sizeof(T)); }
	bool IsAllocated() const          { return m_handle != invalidHandle; }
	void Release();

	void Update(const T* pData) { UpdateInternal(pData, sizeof(T)); }
	T*   Lock();
	void Unlock();

protected:
	void AllocateInternal(buffer_size_t numElements, buffer_size_t elementSize);
	void UpdateInternal(const void* pData, buffer_size_t elementSize);

protected:
	buffer_handle_t             m_handle;
	buffer_size_t               m_numElements;
	CGuardedDeviceBufferManager m_DevBufMan;
	Validator                   m_validator;
};

template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
void TempDynBufferBase<T, bindType, Validator >::AllocateInternal(buffer_size_t numElements, buffer_size_t elementSize)
{
	m_validator.Check(Default);
	BUFFER_USAGE usageType = gRenDev->m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled
	                         ? BU_WHEN_LOADINGTHREAD_ACTIVE // use a separate pool for everything in video rendering mode
	                         : BU_TRANSIENT_RT;             // default to transient_RT
	m_handle = m_DevBufMan.Create(bindType, usageType, numElements * elementSize);
	IF (m_handle != invalidHandle, 1)
	{
		m_numElements = numElements;
		m_validator.Set(Allocated);
	}
}

template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
void TempDynBufferBase<T, bindType, Validator >::Release()
{
	IF (m_handle != invalidHandle, 1)
	{
		m_validator.Check2(Allocated, Filled);
		m_DevBufMan.Destroy(m_handle);
		m_handle = invalidHandle;
		m_numElements = 0;
	}
	m_validator.Set(Default);
}

template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
void TempDynBufferBase<T, bindType, Validator >::UpdateInternal(const void* pData, buffer_size_t elementSize)
{
	m_validator.Check(Allocated);
	m_DevBufMan.UpdateBuffer(m_handle, pData, CGuardedDeviceBufferManager::AlignBufferSizeForStreaming(m_numElements * elementSize));
	m_validator.Set(Filled);
}

template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
T* TempDynBufferBase<T, bindType, Validator >::Lock()
{
	m_validator.Check(Allocated);
	T* p = (T*) m_DevBufMan.BeginWrite(m_handle);
	m_validator.Set(Locked);
	return p;
}

template<typename T, BUFFER_BIND_TYPE bindType, class Validator>
void TempDynBufferBase<T, bindType, Validator >::Unlock()
{
	m_validator.Check(Locked);
	m_DevBufMan.EndReadWrite(m_handle);
	m_validator.Set(Filled);
}

//////////////////////////////////////////////////////////////////////////
template<typename T, class Validator = ValidatorDefault>
class TempDynVBBase : public TempDynBufferBase<T, BBT_VERTEX_BUFFER, Validator>
{
public:
	typedef TempDynBufferBase<T, BBT_VERTEX_BUFFER, Validator> Base;

public:
	using Base::IsAllocated;
	using Base::Release;

	using Base::Update;
	using Base::Lock;
	using Base::Unlock;

protected:
	TempDynVBBase() {}
	~TempDynVBBase() {}

	void BindInternal(uint32 streamID, buffer_size_t stride);

protected:
	using Base::m_handle;
	using Base::m_validator;
};

template<typename T, class Validator>
void TempDynVBBase<T, Validator >::BindInternal(uint32 streamID, buffer_size_t stride)
{
	m_validator.Check(Filled);
	buffer_size_t bufferOffset = ~0;
	D3DVertexBuffer* pVB = this->m_DevBufMan.GetD3DVB(m_handle, &bufferOffset);
	gcpRendD3D->FX_SetVStream(streamID, pVB, bufferOffset, stride);
}

//////////////////////////////////////////////////////////////////////////
template<typename T, class Validator = ValidatorDefault>
class TempDynVB : public TempDynVBBase<T, Validator>
{
	friend class TempDynVBAny;

public:
	typedef TempDynVBBase<T, Validator> Base;

public:
	static void CreateFillAndBind(const T* pData, buffer_size_t numElements, uint32 streamID) { CreateFillAndBindInternal(pData, numElements, streamID, sizeof(T)); }

public:
	TempDynVB() {}

	using Base::Allocate;
	void Bind(uint32 streamID) { BindInternal(streamID, sizeof(T)); }

protected:
	static void CreateFillAndBindInternal(const void* pData, buffer_size_t numElements, uint32 streamID, buffer_size_t stride);

protected:
	using Base::BindInternal;

protected:
	TempDynVB(const TempDynVB&);
	TempDynVB& operator=(const TempDynVB&);
};

template<typename T, class Validator>
void TempDynVB<T, Validator >::CreateFillAndBindInternal(const void* pData, buffer_size_t numElements, uint32 streamID, buffer_size_t stride)
{
	TempDynVB<T, Validator> vb;
	vb.AllocateInternal(numElements, stride);
	vb.UpdateInternal(pData, stride);
	vb.BindInternal(streamID, stride);
	vb.Release();
}

//////////////////////////////////////////////////////////////////////////
template<class Validator = ValidatorDefault>
class TempDynInstVB : public TempDynVBBase<void, Validator>
{
public:
	typedef TempDynVBBase<void, Validator> Base;

public:
	TempDynInstVB() {}

	void Allocate(buffer_size_t numElements, buffer_size_t elementSize) { AllocateInternal(numElements, elementSize); }
	void Bind(uint32 streamID, buffer_size_t stride)                    { BindInternal(streamID, stride); }

protected:
	using Base::AllocateInternal;
	using Base::BindInternal;

protected:
	TempDynInstVB(const TempDynInstVB&);
	TempDynInstVB& operator=(const TempDynInstVB&);
};

//////////////////////////////////////////////////////////////////////////
class TempDynVBAny
{
public:
	static void CreateFillAndBind(const void* pData, buffer_size_t numElements, uint32 streamID, buffer_size_t stride) { TempDynVB<void>::CreateFillAndBindInternal(pData, numElements, streamID, stride); }

private:
	TempDynVBAny();
};

//////////////////////////////////////////////////////////////////////////
template<RenderIndexType idxType>
struct MapIndexType {};

template<>
struct MapIndexType<Index16>
{
	typedef uint16 Type;
	enum { Size = sizeof(Type) };
};

template<>
struct MapIndexType<Index32>
{
	typedef uint32 Type;
	enum { Size = sizeof(Type) };
};

template<RenderIndexType idxType, class Validator = ValidatorDefault>
class TempDynIB : public TempDynBufferBase<typename MapIndexType<idxType>::Type, BBT_INDEX_BUFFER, Validator>
{
public:
	typedef typename MapIndexType<idxType>::Type                          IndexDataType;
	typedef TempDynBufferBase<IndexDataType, BBT_INDEX_BUFFER, Validator> Base;

public:
	static void CreateFillAndBind(const IndexDataType* pData, buffer_size_t numElements);

public:
	TempDynIB() {}

	void Allocate(buffer_size_t numElements);
	using Base::IsAllocated;
	using Base::Release;

	using Base::Update;
	using Base::Lock;
	using Base::Unlock;

	void Bind();

protected:
	TempDynIB(const TempDynIB&);
	TempDynIB& operator=(const TempDynIB&);

protected:
	using Base::m_handle;
	using Base::m_validator;
};

template<RenderIndexType idxType, class Validator>
void TempDynIB<idxType, Validator >::Allocate(buffer_size_t numElements)
{
	Base::Allocate(numElements);
}

template<RenderIndexType idxType, class Validator>
void TempDynIB<idxType, Validator >::Bind()
{
	m_validator.Check(Filled);
	buffer_size_t bufferOffset = ~0;
	D3DIndexBuffer* pIB = this->m_DevBufMan.GetD3DIB(m_handle, &bufferOffset);
	gcpRendD3D->FX_SetIStream(pIB, bufferOffset, idxType);
}

template<RenderIndexType idxType, class Validator>
void TempDynIB<idxType, Validator >::CreateFillAndBind(const IndexDataType* pData, buffer_size_t numElements)
{
	TempDynIB<idxType, Validator> ib;
	ib.Allocate(numElements);
	ib.Update(pData);
	ib.Bind();
	ib.Release();
}

} // namespace TempDynBuffer

using TempDynBuffer::TempDynVB;
using TempDynBuffer::TempDynVBAny;
typedef TempDynBuffer::TempDynInstVB<>    TempDynInstVB;

typedef TempDynBuffer::TempDynIB<Index16> TempDynIB16;
typedef TempDynBuffer::TempDynIB<Index32> TempDynIB32;

#endif // _TEMPDYNBUFFER_H_
