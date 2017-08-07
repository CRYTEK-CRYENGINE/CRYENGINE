// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleMath.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// TIStream, TIOStream

template<typename T>
struct TIOStream
{
public:
	typedef vector4_t<T> Tv;

	explicit TIOStream(T* pStream) : m_pStream(pStream) {}
	bool IsValid() const { return m_pStream != 0; }
	T    Load(TParticleId pId) const;
	void Store(TParticleId pId, T value);
	void Fill(SUpdateRange range, T value);
#ifdef CRY_PFX2_USE_SSE
	Tv   Load(TParticleGroupId pgId) const;
	void Store(TParticleGroupId pgId, Tv value);
#endif

protected:
	T* __restrict m_pStream;
};

template<typename T>
struct TIStream: public TIOStream<T>
{
public:
	typedef vector4_t<T> Tv;

	explicit TIStream(const T* pStream = nullptr, T defaultVal = T());
	T    SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Tv   SafeLoad(TParticleGroupId pId) const;
	Tv   SafeLoad(TParticleIdv pIdv) const;
#endif

private:
	using TIOStream<T>::Store;
	using TIOStream<T>::m_pStream;

	Tv                  m_safeSink;
	uint32              m_safeMask;
};

typedef TIStream<float>        IFStream;
typedef TIOStream<float>       IOFStream;
typedef TIStream<UCol>         IColorStream;
typedef TIOStream<UCol>        IOColorStream;
typedef TIStream<uint32>       IUintStream;
typedef TIOStream<uint32>      IOUintStream;
typedef TIStream<TParticleId>  IPidStream;
typedef TIOStream<TParticleId> IOPidStream;

template<typename T, typename ID>
ILINE T DeltaTime(T deltaTime, ID id, const IFStream& normAges, const IFStream& lifeTimes)
{
	const T normAge = normAges.Load(id);
	const T lifeTime = lifeTimes.Load(id);
	return DeltaTime(deltaTime, normAge, lifeTime);
}

//////////////////////////////////////////////////////////////////////////

struct IOVec3Stream
{
public:
	IOVec3Stream(float* pX, float* pY, float* pZ);
	Vec3  Load(TParticleId pId) const;
	void  Store(TParticleId pId, Vec3 value);
#ifdef CRY_PFX2_USE_SSE
	Vec3v Load(TParticleGroupId pgId) const;
	void  Store(TParticleGroupId pgId, const Vec3v& value);
#endif
protected:
	float* __restrict m_pXStream;
	float* __restrict m_pYStream;
	float* __restrict m_pZStream;
};

struct IVec3Stream : public IOVec3Stream
{
public:
	IVec3Stream(const float* pX, const float* pY, const float* pZ, Vec3 defaultVal = Vec3(ZERO));
	Vec3  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Vec3v SafeLoad(TParticleGroupId pgId) const;
	Vec3v SafeLoad(TParticleIdv pIdv) const;
#endif
private:
	using IOVec3Stream::Store;

	const Vec3v             m_safeSink;
	const uint32            m_safeMask;
};

struct IOQuatStream
{
public:
	IOQuatStream(float* pX, float* pY, float* pZ, float* pW);
	Quat  Load(TParticleId pId) const;
	void  Store(TParticleId pId, Quat value);
#ifdef CRY_PFX2_USE_SSE
	Quatv Load(TParticleGroupId pgId) const;
	void  Store(TParticleGroupId pgId, const Quatv& value);
#endif
protected:
	float* __restrict m_pXStream;
	float* __restrict m_pYStream;
	float* __restrict m_pZStream;
	float* __restrict m_pWStream;
};

struct IQuatStream : public IOQuatStream
{
public:
	IQuatStream(const float* pX, const float* pY, const float* pZ, const float* pW, Quat defaultVal = Quat(IDENTITY));
	Quat  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Quatv SafeLoad(TParticleGroupId pgId) const;
	Quatv SafeLoad(TParticleIdv pIdv) const;
#endif
private:
	using IOQuatStream::Store;

	const Quatv             m_safeSink;
	const uint32            m_safeMask;
};

//////////////////////////////////////////////////////////////////////////

struct STempVec3Stream
{
public:
	STempVec3Stream(TParticleHeap* pMemHeap, SUpdateRange range);
	void Clear(SUpdateRange context);
	IOVec3Stream GetIOVec3Stream() const { return m_stream; }
private:
	TFloatArray  m_xBuffer;
	TFloatArray  m_yBuffer;
	TFloatArray  m_zBuffer;
	IOVec3Stream m_stream;
};

}

#include "ParticleDataStreamsImpl.h"
