// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifdef CRY_PFX2_USE_SSE
template<> ILINE pfx2::UColv convert<pfx2::UColv>(UCol v) { return _mm_set1_epi32(v.dcolor); }
#endif

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// TIStream

template<typename T, typename Tv>
ILINE TIStream<T, Tv>::TIStream(const T* pStream, T defaultVal)
	: m_safeSink(convert<Tv>(defaultVal))
	, m_pStream(pStream ? pStream : reinterpret_cast<T*>(&m_safeSink))
	, m_safeMask(pStream ? ~0 : 0)
{
}

template<typename T, typename Tv>
ILINE T TIStream<T, Tv >::Load(TParticleId pId) const
{
	return *(m_pStream + pId);
}

template<typename T, typename Tv>
ILINE T TIStream<T, Tv >::SafeLoad(TParticleId pId) const
{
	return *(m_pStream + (pId & m_safeMask));
}

template<typename T, typename Tv>
ILINE void TIOStream<T, Tv >::Store(TParticleId pId, T value)
{
	*(const_cast<T*>(m_pStream) + pId) = value;
}

//////////////////////////////////////////////////////////////////////////

ILINE IVec3Stream::IVec3Stream(const float* pX, const float* pY, const float* pZ, Vec3 defaultVal)
	: m_safeSink(ToFloatv(defaultVal.x), ToFloatv(defaultVal.y), ToFloatv(defaultVal.z))
	, m_pXStream((pX && pY && pZ) ? pX : (float*)&m_safeSink.x)
	, m_pYStream((pX && pY && pZ) ? pY : (float*)&m_safeSink.y)
	, m_pZStream((pX && pY && pZ) ? pZ : (float*)&m_safeSink.z)
	, m_safeMask((pX && pY && pZ) ? ~0 : 0)
{
}

ILINE Vec3 IVec3Stream::Load(TParticleId pId) const
{
	return Vec3(*(m_pXStream + pId), *(m_pYStream + pId), *(m_pZStream + pId));
}

ILINE Vec3 IVec3Stream::SafeLoad(TParticleId pId) const
{
	return Vec3(
		*(m_pXStream + (pId & m_safeMask)),
		*(m_pYStream + (pId & m_safeMask)),
		*(m_pZStream + (pId & m_safeMask)));
}

ILINE void IOVec3Stream::Store(TParticleId pId, Vec3 value)
{
	*(const_cast<float*>(m_pXStream) + pId) = value.x;
	*(const_cast<float*>(m_pYStream) + pId) = value.y;
	*(const_cast<float*>(m_pZStream) + pId) = value.z;
}

ILINE IQuatStream::IQuatStream(const float* pX, const float* pY, const float* pZ, const float* pW, Quat defaultVal)
	: m_safeSink(ToFloatv(defaultVal.w), ToFloatv(defaultVal.v.x), ToFloatv(defaultVal.v.y), ToFloatv(defaultVal.v.z))
	, m_pXStream((pX && pY && pZ && pW) ? pX : (float*)&m_safeSink.v.x)
	, m_pYStream((pX && pY && pZ && pW) ? pY : (float*)&m_safeSink.v.y)
	, m_pZStream((pX && pY && pZ && pW) ? pZ : (float*)&m_safeSink.v.z)
	, m_pWStream((pX && pY && pZ && pW) ? pW : (float*)&m_safeSink.w)
	, m_safeMask((pX && pY && pZ && pW) ? ~0 : 0)
{
}

ILINE Quat IQuatStream::Load(TParticleId pId) const
{
	return Quat(
		*(m_pWStream + pId),
		*(m_pXStream + pId),
		*(m_pYStream + pId),
		*(m_pZStream + pId));
}

ILINE Quat IQuatStream::SafeLoad(TParticleId pId) const
{
	return Quat(
		*(m_pWStream + (pId & m_safeMask)),
		*(m_pXStream + (pId & m_safeMask)),
		*(m_pYStream + (pId & m_safeMask)),
		*(m_pZStream + (pId & m_safeMask)));
}

ILINE void IOQuatStream::Store(TParticleId pId, Quat value)
{
	*(const_cast<float*>(m_pWStream) + pId) = value.w;
	*(const_cast<float*>(m_pXStream) + pId) = value.v.x;
	*(const_cast<float*>(m_pYStream) + pId) = value.v.y;
	*(const_cast<float*>(m_pZStream) + pId) = value.v.z;
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
