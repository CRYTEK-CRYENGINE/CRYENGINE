// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

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
	CParticleContainer();
	CParticleContainer(const CParticleContainer& copy);
	~CParticleContainer();

	void                              Resize(uint32 newSize);
	void                              ResetUsedData();
	void                              AddParticleData(EParticleDataType type);
	bool                              HasData(EParticleDataType type) const { return m_useData[type]; }
	void                              AddParticle();
	void                              Trim();
	void                              Clear();

	template<typename T> void         FillData(TDataType<T> type, const T& data, SUpdateRange range);
	void                              CopyData(EParticleDataType dstType, EParticleDataType srcType, SUpdateRange range);

	template<typename T> TIStream<T>  IStream(TDataType<T> type, const T& defaultVal = T()) const                           { return TIStream<T>(Data(type), defaultVal);	}
	template<typename T> TIOStream<T> IOStream(TDataType<T> type) 	                                                        { return TIOStream<T>(Data(type));	}

	TIStream<Vec3>                    IStream(TDataType<Vec3> type, Vec3 defaultVal = Vec3(0)) const;
	TIOStream<Vec3>                   IOStream(TDataType<Vec3> type);
	TIStream<Quat>                    IStream(TDataType<Quat> type, Quat defaultVal = Quat(IDENTITY)) const;
	TIOStream<Quat>                   IOStream(TDataType<Quat> type);

	// Legacy compatibility
	IFStream                          GetIFStream(TDataType<float> type, float defaultVal = 0.0f) const                      { return IStream(type, defaultVal); }
	IOFStream                         GetIOFStream(TDataType<float> type)                                                    { return IOStream(type); }
	IVec3Stream                       GetIVec3Stream(TDataType<Vec3> type, Vec3 defaultVal = Vec3(ZERO)) const               { return IStream(type, defaultVal); }
	IOVec3Stream                      GetIOVec3Stream(TDataType<Vec3> type)                                                  { return IOStream(type); }
	IQuatStream                       GetIQuatStream(TDataType<Quat> type, Quat defaultVal = Quat(IDENTITY)) const           { return IStream(type, defaultVal); }
	IOQuatStream                      GetIOQuatStream(TDataType<Quat> type)                                                  { return IOStream(type); }
	IColorStream                      GetIColorStream(TDataType<UCol> type, UCol defaultVal = UCol()) const                  { return IStream(type, defaultVal); }
	IOColorStream                     GetIOColorStream(TDataType<UCol> type)                                                 { return IOStream(type); }
	IPidStream                        GetIPidStream(TDataType<TParticleId> type, TParticleId defaultVal = gInvalidId) const  { return IStream(type, defaultVal); }
	IOPidStream                       GetIOPidStream(TDataType<TParticleId> type)                                            { return IOStream(type); }

	uint32                            GetNumParticles() const         { CRY_PFX2_ASSERT(!HasNewBorns()); return m_lastId; }
	uint32                            GetRealNumParticles() const     { return m_lastId + HasNewBorns() * GetNumSpawnedParticles(); }
	uint32                            GetMaxParticles() const         { return m_maxParticles; }
	uint32                            GetNumSpawnedParticles() const  { return m_lastSpawnId - m_firstSpawnId; }
	bool                              HasNewBorns() const             { return m_lastSpawnId > m_lastId; }
	bool                              IsNewBorn(TParticleId id) const { return id >= m_firstSpawnId && id < m_lastSpawnId; }
	TParticleId                       GetRealId(TParticleId pId) const;
	SUpdateRange                      GetFullRange() const            { CRY_PFX2_ASSERT(!HasNewBorns()); return SUpdateRange(0, m_lastId); }
	SUpdateRange                      GetSpawnedRange() const         { return SUpdateRange(m_firstSpawnId, m_lastSpawnId); }
	SUpdateRange                      GetNonSpawnedRange() const      { CRY_PFX2_ASSERT(!HasNewBorns()); return SUpdateRange(0, m_firstSpawnId); }

	void                              AddParticles(TConstArray<SSpawnEntry> spawnEntries);
	void                              ResetSpawnedParticles();
	void                              RemoveParticles(TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds);

private:
	template<typename T> T*           Data(TDataType<T> type)                { return reinterpret_cast<T*>(m_pData[type]); }
	template<typename T> const T*     Data(TDataType<T> type) const          { return reinterpret_cast<const T*>(m_pData[type]); }

	void                              MakeSwapIds(TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds);
	
	StaticEnumArray<void*, EParticleDataType> m_pData;
	StaticEnumArray<bool, EParticleDataType>  m_useData;
	TParticleId m_nextSpawnId;
	TParticleId m_maxParticles;

	TParticleId m_lastId;
	TParticleId m_firstSpawnId;
	TParticleId m_lastSpawnId;
};

}

#include "ParticleContainerImpl.h"

