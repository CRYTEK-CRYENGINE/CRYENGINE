// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

	explicit TIOStream(T* pStream)             : m_pStream(pStream) {}
	bool IsValid() const                        { return m_pStream != 0; }
	T    Load(TParticleId pId) const            { return m_pStream[pId]; }
	void Store(TParticleId pId, T value)        { m_pStream[pId] = value; }
	void Fill(SUpdateRange range, T value);

	T operator[](TParticleId pId) const         { return m_pStream[pId]; }
	T& operator[](TParticleId pId)              { return m_pStream[pId]; }

#ifdef CRY_PFX2_USE_SSE
	Tv   Load(TParticleGroupId pgId) const      { return *(const Tv*)(m_pStream + pgId); }
	void Store(TParticleGroupId pgId, Tv value) { *(Tv*)(m_pStream + pgId) = value; }

	Tv operator[](TParticleGroupId pgId) const  { return *(const Tv*)(m_pStream + pgId); }
	Tv& operator[](TParticleGroupId pgId)       { return *(Tv*)(m_pStream + pgId); }
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
	T SafeLoad(TParticleId pId) const { return m_pStream[pId & m_safeMask]; }
	T operator[](TParticleId pId) const { return SafeLoad(pId); }
#ifdef CRY_PFX2_USE_SSE
	Tv SafeLoad(TParticleGroupId pgId) const { return *(const Tv*)(m_pStream + (pgId & m_safeMask)); }
	Tv operator[](TParticleGroupId pgId) const { return SafeLoad(pgId); }
	Tv SafeLoad(TParticleIdv pIdv) const;
#endif

private:
	using TIOStream<T>::Store;
	using TIOStream<T>::m_pStream;

	Tv                  m_safeSink;
	uint32              m_safeMask;
};

// Legacy types
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

template<>
struct TIOStream<Vec3>
{
public:
	TIOStream(float* pX, float* pY, float* pZ);
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

template<>
struct TIStream<Vec3>: public TIOStream<Vec3>
{
public:
	TIStream(const float* pX, const float* pY, const float* pZ, Vec3 defaultVal = Vec3(ZERO));
	Vec3  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Vec3v SafeLoad(TParticleGroupId pgId) const;
	Vec3v SafeLoad(TParticleIdv pIdv) const;
#endif
private:
	using TIOStream<Vec3>::Store;

	const Vec3v  m_safeSink;
	const uint32 m_safeMask;
};

typedef TIStream<Vec3>  IVec3Stream;
typedef TIOStream<Vec3> IOVec3Stream;

template<>
struct TIOStream<Quat>
{
public:
	TIOStream(float* pX, float* pY, float* pZ, float* pW);
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

template<>
struct TIStream<Quat>: public TIOStream<Quat>
{
public:
	TIStream(const float* pX, const float* pY, const float* pZ, const float* pW, Quat defaultVal = Quat(IDENTITY));
	Quat  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Quatv SafeLoad(TParticleGroupId pgId) const;
	Quatv SafeLoad(TParticleIdv pIdv) const;
#endif
private:
	using TIOStream<Quat>::Store;

	const Quatv  m_safeSink;
	const uint32 m_safeMask;
};

typedef TIStream<Quat>  IQuatStream;
typedef TIOStream<Quat> IOQuatStream;


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
