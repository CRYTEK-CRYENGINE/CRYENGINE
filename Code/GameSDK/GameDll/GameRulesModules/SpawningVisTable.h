// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SPAWNINGVISTABLE_H__
#define __SPAWNINGVISTABLE_H__

#include <CryPhysics/RayCastQueue.h>

#if MAX_PLAYER_LIMIT <= 16
typedef uint16 TVisBitField;
#elif MAX_PLAYER_LIMIT <= 32
typedef uint32 TVisBitField;
#elif MAX_PLAYER_LIMIT <= 64
typedef uint64 TVisBitField;
#else 
	#error "Need to fix up the bitfield"
#endif

//A more compact, understandable and efficient version of the SvSpawningVisTable
//	A single deferred raycast per spawn
//	Designed to cope with a sub-set of players, not all of them by default
//	Designed to run on the client, not the server

class CSpawningVisTable
{
public:
	CSpawningVisTable();
	~CSpawningVisTable();

	void	Update(EntityId currentBestSpawnId);
	bool	IsSpawnLocationVisibleByTeam(EntityId location, int teamId) const;

	void	AddSpawnLocation(EntityId location, bool doVisibilityTest);
	void	RemoveSpawnLocation(EntityId location);

	void	PlayerJoined(EntityId playerId);
	void	PlayerLeft(EntityId playerId);
	void	OnSetTeam(EntityId playerId, int teamId);

	ILINE void	HostMigrationStopAddingPlayers()		{ m_bBlockPlayerAddition = true;	}
				void	HostMigrationResumeAddingPlayers();

	void	Initialise();
	void	RepopulatePlayerList();

	static const int kNumBits				= sizeof(TVisBitField) * 8;
	static const int kMaxNumPlayers = MAX_PLAYER_LIMIT;			//Could be running on dedi server

private:
	static const int kInitialNumSpawns = 100;

	enum ESpawnTestType {
		eSTT_LineTest,
		eSTT_AreaTest
	};

	struct SSpawnVisibilityInfo
	{
		SSpawnVisibilityInfo(EntityId spawnId) : visBits(0), lastFrameTested(-1),
			currentlyTestingVis(-1), rayId(0), entityId(spawnId), spawnTestType(eSTT_LineTest) {}

		void OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result );
		void CancelRaycastRequest();

		TVisBitField		visBits;
		int8						lastFrameTested;
		int8						currentlyTestingVis : 7;
		ESpawnTestType	spawnTestType : 1;
		QueuedRayID			rayId;
		EntityId				entityId;
	};

	typedef std::vector<SSpawnVisibilityInfo> TSpawnVisList;
	typedef std::vector<Vec3> TPosnList;
	typedef std::vector<int8> TSpawnTeamList;
	typedef std::map<EntityId, int> TSpawnIndexMap;
	typedef std::vector<int16> TSpawnIndexList;

	int16 m_nLastTestIndex;
	bool	m_bBlockPlayerAddition;

	TSpawnIndexMap	m_spawnIndexMap;

	TSpawnVisList		m_spawnVisData;
	TSpawnTeamList	m_spawnTeamList;
	TSpawnIndexList m_spawnVisTestList;
	TSpawnIndexList m_spawnDependentList;
	TSpawnIndexList	m_spawnParentList;

	TPosnList	m_spawnPosnData;
	TPosnList	m_playerPosnData;

	TSpawnIndexList m_updatedSpawns;

	typedef CryFixedArray<EntityId, kMaxNumPlayers> TPlayerList;
	TPlayerList			m_playerList;

	void	QueueNextLineTestForSpawn(SSpawnVisibilityInfo& rSpawnVisInfo, const Vec3& rSpawnPosn);
	void	PerformAreaTestForSpawn(SSpawnVisibilityInfo& rSpawnVisInfo);

	void	DoVisibilityTests(const int kMaxNumLineTests, const int kMaxNumAreaTests, const int nSkipSpawnIndex);
	void	UpdateDependents();

	void	OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result );

	ILINE bool ShouldLineTestSpawn(const SSpawnVisibilityInfo& rVisInfo) const
				{ return (rVisInfo.spawnTestType == eSTT_LineTest) && (rVisInfo.rayId == 0); }
};

#endif //__SPAWNINGVISTABLE_H__