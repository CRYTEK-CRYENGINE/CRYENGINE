// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CParticleContainer

ILINE TParticleId CParticleContainer::GetRealId(TParticleId pId) const
{
	if (pId < m_lastId)
		return pId;
	const uint numSpawn = GetNumSpawnedParticles();
	const uint gapSize = m_firstSpawnId - m_lastId;
	const uint movingId = m_lastSpawnId - min(gapSize, numSpawn);
	const uint offset = movingId - m_lastId;
	if (pId >= movingId)
		return pId - offset;
	return pId;
}

template<typename T>
void CParticleContainer::FillData(TDataType<T> type, const T& data, SUpdateRange range)
{
	if (HasData(type))
	{
		for (uint e = 0; e < type.Dim; ++e)
		{
			auto stream = IOStream(type[e]);
			stream.Fill(range, TDimInfo<T>::Elem(data, e));
		}
	}
}

inline void CParticleContainer::CopyData(EParticleDataType dstType, EParticleDataType srcType, SUpdateRange range)
{
	CRY_PFX2_ASSERT(dstType.info().type == srcType.info().type);
	if (HasData(dstType) && HasData(srcType))
	{
		size_t stride = dstType.info().typeSize;
		size_t count = range.size();
		uint dim = dstType.info().dimension;
		for (uint i = 0; i < dim; ++i)
			memcpy(
			  (byte*)m_pData[dstType + i] + stride * range.m_begin,
			  (byte*)m_pData[srcType + i] + stride * range.m_begin,
			  stride * count);
	}
}

ILINE TIStream<Vec3> CParticleContainer::IStream(TDataType<Vec3> type, Vec3 defaultVal) const
{
	return TIStream<Vec3>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		defaultVal);
}

ILINE TIOStream<Vec3> CParticleContainer::IOStream(TDataType<Vec3> type)
{
	return TIOStream<Vec3>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]));
}

ILINE TIStream<Quat> CParticleContainer::IStream(TDataType<Quat> type, Quat defaultVal) const
{
	return TIStream<Quat>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		Data(type[3]),
		defaultVal);
}
ILINE TIOStream<Quat> CParticleContainer::IOStream(TDataType<Quat> type)
{
	return TIOStream<Quat>(
		Data(type[0]),
		Data(type[1]),
		Data(type[2]),
		Data(type[3]));
}

}
