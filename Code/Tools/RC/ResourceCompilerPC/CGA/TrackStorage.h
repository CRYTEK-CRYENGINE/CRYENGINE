// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// Crytek Engine Source File.
// Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
// File name: TrackStorage.h
// Version: v1.00
// Created: 22/8/2006 by Alexey Medvedev.
// Compilers: Visual Studio.NET 2005
// Description: Storage for database of tracks.
// -------------------------------------------------------------------------
// History:
//
////////////////////////////////////////////////////////////////////////////
#pragma once

#include "AnimationLoader.h"

struct DBStatistics
{
	DBStatistics() : m_iNumAnimations(0), m_iNumControllers(0), m_iNumKeyTimes(0), m_iNumPositions(0), m_iNumRotations(0), m_iSavedBytes(0) {}
	int m_iNumAnimations;

	int m_iNumControllers;

	int m_iNumKeyTimes;
	int m_iNumPositions;
	int m_iNumRotations;


	int m_iSavedBytes;

};

struct GlobalAnimationHeaderCAF;

class CTrackStorage
{
public:
	explicit CTrackStorage(bool bigEndianOutput);
	~CTrackStorage();

	bool GetAnimFileTime(const string&  name, DWORD * ft);

	void AddAnimation(const GlobalAnimationHeaderCAF& header);

	void Analyze(uint32& TrackShader, uint32& SizeDataShared, uint32& TotalTracks, uint32& TotalMemory, CSkeletonInfo * currentSkeleton);

	void Clear()
	{
		m_arrGlobalAnimationHeaderCAF.clear();
		m_arrAnimNames.clear();
		m_arrKeyTimes.clear();
		m_arrRotationTracks.clear();
		m_arrPositionTracks.clear();
		m_arrKeyTimesRemap.clear();
		m_arrKeyPosRemap.clear();
		m_arrKeyRotRemap.clear();
	}

	uint32  FindKeyTimesTrack(KeyTimesInformationPtr& pKeyTimes)
	{
		const uint32 numKeys = pKeyTimes->GetNumKeys();
		TVectorRemap::iterator it = m_arrKeyTimesRemap.lower_bound(numKeys); 
		TVectorRemap::iterator end = m_arrKeyTimesRemap.upper_bound(numKeys); 

		for (; it != end; ++it)
		{
			if (IsKeyTimesIdentical(pKeyTimes, m_arrKeyTimes[it->second]))
			{
				return it->second;
			}
		}

		return -1;
	}

	uint32  FindPositionTrack(PositionControllerPtr& pKeys)
	{
		const uint32 numKeys = pKeys->GetNumKeys();
		TVectorRemap::iterator it = m_arrKeyPosRemap.lower_bound(numKeys); 
		TVectorRemap::iterator end = m_arrKeyPosRemap.upper_bound(numKeys); 

		for (; it != end; ++it)
		{
			if (IsPositionIdentical(pKeys, m_arrPositionTracks[it->second]))
			{
				return it->second;
			}
		}

		return -1;
	}

	uint32  FindRotationTrack(RotationControllerPtr& pKeys)
	{
		const uint32 numKeys = pKeys->GetNumKeys();
		TVectorRemap::iterator it = m_arrKeyRotRemap.lower_bound(numKeys); 
		TVectorRemap::iterator end = m_arrKeyRotRemap.upper_bound(numKeys); 

		for (; it != end; ++it)
		{
			if (IsRotationIdentical(pKeys, m_arrRotationTracks[it->second]))
			{
				return it->second;
			}
		}

		return -1;
	}

	void SaveDataBase905(const char* name, bool bPrepareForInPlaceStream, int pointerSize);

	const DBStatistics& GetStatistics()
	{
		return m_Statistics;
	}

protected:
	bool IsRotationIdentical(const RotationControllerPtr& track1, const TrackRotationStoragePtr&  track2);
	bool IsPositionIdentical(const PositionControllerPtr& track1, const TrackPositionStoragePtr& track2);
	bool IsKeyTimesIdentical(const KeyTimesInformationPtr& track1, const KeyTimesInformationPtr& track2);
	// get number animation from m_arrAnimations. If anim doesnt exist - new num created;
	uint32 FindOrAddAnimationHeader(const GlobalAnimationHeaderCAF& header, const string& name);
	uint32 FindAnimation(const string& name);

	void AnalyzeKeyTimes();

	void CreateBitsetKeyTimes(int k);

public:

	std::vector<GlobalAnimationHeaderCAF> m_arrGlobalAnimationHeaderCAF;
	typedef std::vector<string> TNamesVector;
	TNamesVector m_arrAnimNames;
	DynArray<KeyTimesInformationPtr> m_arrKeyTimes;
	DynArray<TrackRotationStoragePtr> m_arrRotationTracks;
	DynArray<TrackPositionStoragePtr> m_arrPositionTracks;

	typedef std::multimap<int, int> TVectorRemap;

	TVectorRemap m_arrKeyTimesRemap;
	TVectorRemap m_arrKeyPosRemap;
	TVectorRemap m_arrKeyRotRemap;

	bool m_bBigEndianOutput;

	DBStatistics m_Statistics;
	ThreadUtils::CriticalSection m_lock;
};
