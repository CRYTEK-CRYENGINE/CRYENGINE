// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     24/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLECONTAINER_H
#define PARTICLECONTAINER_H

#pragma once

#include <CrySerialization/IArchive.h>
#include "ParticleCommon.h"
#include "ParticleDataTypes.h"
#include "ParticleMath.h"
#include "ParticleDataStreams.h"

namespace pfx2
{

class CParticleContainer;

typedef TIStream<UCol>         IColorStream;
typedef TIStream<uint32>       IUintStream;
typedef TIOStream<uint32>      IOUintStream;
typedef TIStream<TParticleId>  IPidStream;
typedef TIOStream<TParticleId> IOPidStream;

class CParticleContainer
{
public:
	struct SParticleRangeIterator;

	struct SSpawnEntry
	{
		uint32 m_count;
		uint32 m_parentId;
		float  m_ageBegin;
		float  m_ageIncrement;
		float  m_fractionBegin;
		float  m_fractionIncrement;
	};

public:
	CParticleContainer();
	CParticleContainer(const CParticleContainer& copy);
	~CParticleContainer();

	void                              Resize(size_t newSize);
	void                              ResetUsedData();
	void                              AddParticleData(EParticleDataType type);
	bool                              HasData(EParticleDataType type) const { return m_useData[type]; }
	void                              AddParticle();
	void                              AddRemoveParticles(TConstArray<SSpawnEntry> spawnEntries, TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds);
	void                              Trim();
	void                              Clear();

	template<typename T> T*           GetData(EParticleDataType type);
	template<typename T> const T*     GetData(EParticleDataType type) const;
	template<typename T> void         FillData(EParticleDataType type, const T& data, SUpdateRange range, bool allDims = true);
	void                              CopyData(EParticleDataType dstType, EParticleDataType srcType, SUpdateRange range);

	IFStream                          GetIFStream(EParticleDataType type, float defaultVal = 0.0f) const;
	IOFStream                         GetIOFStream(EParticleDataType type);
	IVec3Stream                       GetIVec3Stream(EParticleDataType type, Vec3 defaultVal = Vec3(ZERO)) const;
	IOVec3Stream                      GetIOVec3Stream(EParticleDataType type);
	IQuatStream                       GetIQuatStream(EParticleDataType type, Quat defaultVal = Quat(IDENTITY)) const;
	IOQuatStream                      GetIOQuatStream(EParticleDataType type);
	IColorStream                      GetIColorStream(EParticleDataType type, UCol defaultVal = UCol()) const;
	IOColorStream                     GetIOColorStream(EParticleDataType type);
	IUintStream                       GetIUintStream(EParticleDataType type, uint32 defaultVal = 0) const;
	IOUintStream                      GetIOUintStream(EParticleDataType type);
	IPidStream                        GetIPidStream(EParticleDataType type, TParticleId defaultVal = gInvalidId) const;
	IOPidStream                       GetIOPidStream(EParticleDataType type);
	template<typename T> TIStream<T>  GetTIStream(EParticleDataType type, const T& defaultVal = T()) const;
	template<typename T> TIOStream<T> GetTIOStream(EParticleDataType type);

	bool                              Empty() const                   { return GetNumParticles() == 0; }
	uint32                            GetNumParticles() const         { return m_lastId + GetNumSpawnedParticles(); }
	bool                              HasSpawnedParticles() const     { return GetNumSpawnedParticles() != 0; }
	TParticleId                       GetFirstSpawnParticleId() const { return TParticleId(m_firstSpawnId); }
	TParticleId                       GetLastParticleId() const       { return TParticleId(m_lastSpawnId); }
	size_t                            GetMaxParticles() const         { return m_maxParticles; }
	uint32                            GetNumSpawnedParticles() const  { return m_lastSpawnId - m_firstSpawnId; }
	void                              ResetSpawnedParticles();
	void                              RemoveNewBornFlags();
	TParticleId                       GetRealId(TParticleId pId) const;
	uint32                            GetNextSpawnId() const          { return m_nextSpawnId; }
	SUpdateRange                      GetFullRange() const            { return SUpdateRange(0, GetLastParticleId()); }
	SUpdateRange                      GetSpawnedRange() const         { return SUpdateRange(GetFirstSpawnParticleId(), GetLastParticleId()); }

private:
	void AddParticles(TConstArray<SSpawnEntry> spawnEntries);
	void RemoveParticles(TConstArray<TParticleId> toRemove);
	void MakeSwapIds(TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds);

	StaticEnumArray<void*, EParticleDataType> m_pData;
	StaticEnumArray<bool, EParticleDataType>  m_useData;
	uint32 m_nextSpawnId;
	uint32 m_maxParticles;

	uint32 m_lastId;
	uint32 m_firstSpawnId;
	uint32 m_lastSpawnId;
};

}

#include "ParticleUpdate.h"
#include "ParticleContainerImpl.h"

#endif // PARTICLECONTAINER_H
