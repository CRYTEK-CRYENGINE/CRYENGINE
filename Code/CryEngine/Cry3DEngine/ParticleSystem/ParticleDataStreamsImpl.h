// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifdef CRY_PFX2_USE_SSE
template<> ILINE pfx2::UColv convert<pfx2::UColv>(UCol v) { return _mm_set1_epi32(v.dcolor); }
#endif

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// TIOStream

template<typename T>
ILINE T TIOStream<T>::Load(TParticleId pId) const
{
	return m_pStream[pId];
}

template<typename T>
ILINE void TIOStream<T>::Store(TParticleId pId, T value)
{
	m_pStream[pId] = value;
}

template<typename T>
ILINE void TIOStream<T>::Fill(SUpdateRange range, T value)
{
	static const uint VStride = sizeof(Tv) / sizeof(T);
	using UpdateRangeV = TIndexRange<TParticleId, VStride>;
	UpdateRangeV rangev(range);
	Tv valuev = convert<Tv>(value);
	for (TParticleId idx = rangev.m_begin; idx < rangev.m_end; idx += VStride)
		*(Tv*)(m_pStream + idx) = valuev;
}

template<typename T>
ILINE TIStream<T>::TIStream(const T* pStream, T defaultVal)
	: TIOStream<T>(pStream ? const_cast<T*>(pStream) : reinterpret_cast<T*>(&m_safeSink))
	, m_safeSink(convert<Tv>(defaultVal))
	, m_safeMask(pStream ? ~0 : 0)
{
}

template<typename T>
ILINE T TIStream<T>::SafeLoad(TParticleId pId) const
{
	return m_pStream[pId & m_safeMask];
}

//////////////////////////////////////////////////////////////////////////
// IOVec3Stream

ILINE IOVec3Stream::IOVec3Stream(float* pX, float* pY, float* pZ)
	: m_pXStream(pX)
	, m_pYStream(pY)
	, m_pZStream(pZ)
{
}

ILINE Vec3 IOVec3Stream::Load(TParticleId pId) const
{
	return Vec3(m_pXStream[pId], m_pYStream[pId], m_pZStream[pId]);
}

ILINE void IOVec3Stream::Store(TParticleId pId, Vec3 value)
{
	CRY_PFX2_DEBUG_ONLY_ASSERT(IsValid(value));
	m_pXStream[pId] = value.x;
	m_pYStream[pId] = value.y;
	m_pZStream[pId] = value.z;
}

ILINE IVec3Stream::IVec3Stream(const float* pX, const float* pY, const float* pZ, Vec3 defaultVal)
	: IOVec3Stream((float*)pX, (float*)pY, (float*)pZ)
	, m_safeSink(ToFloatv(defaultVal.x), ToFloatv(defaultVal.y), ToFloatv(defaultVal.z))
	, m_safeMask((pX && pY && pZ) ? ~0 : 0)
{
	if (!m_safeMask)
	{
		m_pXStream = (float*)&m_safeSink.x;
		m_pYStream = (float*)&m_safeSink.y;
		m_pZStream = (float*)&m_safeSink.z;
	}
}

ILINE Vec3 IVec3Stream::SafeLoad(TParticleId pId) const
{
	return Vec3(
		m_pXStream[pId & m_safeMask],
		m_pYStream[pId & m_safeMask],
		m_pZStream[pId & m_safeMask]);
}

//////////////////////////////////////////////////////////////////////////
// IOQuatStream

ILINE IOQuatStream::IOQuatStream(float* pX, float* pY, float* pZ, float* pW)
	: m_pXStream(pX)
	, m_pYStream(pY)
	, m_pZStream(pZ)
	, m_pWStream(pW)
{
}

ILINE Quat IOQuatStream::Load(TParticleId pId) const
{
	return Quat(
		m_pWStream[pId],
		m_pXStream[pId],
		m_pYStream[pId],
		m_pZStream[pId]);
}

ILINE void IOQuatStream::Store(TParticleId pId, Quat value)
{
	m_pXStream[pId] = value.v.x;
	m_pYStream[pId] = value.v.y;
	m_pZStream[pId] = value.v.z;
	m_pWStream[pId] = value.w;
}


ILINE IQuatStream::IQuatStream(const float* pX, const float* pY, const float* pZ, const float* pW, Quat defaultVal)
	: IOQuatStream((float*)pX, (float*)pY, (float*)pZ, (float*)pW)
	, m_safeSink(ToFloatv(defaultVal.w), ToFloatv(defaultVal.v.x), ToFloatv(defaultVal.v.y), ToFloatv(defaultVal.v.z))
	, m_safeMask((pX && pY && pZ && pW) ? ~0 : 0)
{
	if (!m_safeMask)
	{
		m_pXStream = (float*)&m_safeSink.v.x;
		m_pYStream = (float*)&m_safeSink.v.y;
		m_pZStream = (float*)&m_safeSink.v.z;
		m_pWStream = (float*)&m_safeSink.w;
	}

}

ILINE Quat IQuatStream::SafeLoad(TParticleId pId) const
{
	return Quat(
		m_pWStream[pId & m_safeMask],
		m_pXStream[pId & m_safeMask],
		m_pYStream[pId & m_safeMask],
		m_pZStream[pId & m_safeMask]);
}

//////////////////////////////////////////////////////////////////////////

ILINE STempVec3Stream::STempVec3Stream(TParticleHeap* pMemHeap, SUpdateRange range)
	: m_xBuffer(*pMemHeap, range.size())
	, m_yBuffer(*pMemHeap, range.size())
	, m_zBuffer(*pMemHeap, range.size())
	, m_stream(m_xBuffer.data() - *range.begin(), m_yBuffer.data() - *range.begin(), m_zBuffer.data() - *range.begin())
{
}

ILINE void STempVec3Stream::Clear(SUpdateRange range)
{
	const uint numBytes = range.size() * sizeof(float);
	memset(m_xBuffer.data(), 0, numBytes);
	memset(m_yBuffer.data(), 0, numBytes);
	memset(m_zBuffer.data(), 0, numBytes);
}

}

#ifdef CRY_PFX2_USE_SSE
	#include "ParticleDataStreamsImplSSE.h"
#endif
