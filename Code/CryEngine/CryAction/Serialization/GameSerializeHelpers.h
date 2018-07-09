// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAME_SERIALIZE_HELPERS_H__
#define __GAME_SERIALIZE_HELPERS_H__

#include "CryActionCVars.h"

// Some Helpers
// Checkpoint for easy section timing
struct Checkpoint
{
	// if msg is given (const char ptr!), auto-message on d'tor
	Checkpoint(bool bOutput, const char* msg = 0) : m_bOutput(bOutput), m_szMsg(msg)
	{
		Start();
	}

	~Checkpoint()
	{
		if (m_szMsg)
		{
			Check(m_szMsg);
		}
	}
	void Start()
	{
		m_startTime = gEnv->pTimer->GetAsyncTime();
	}

	CTimeValue End()
	{
		return gEnv->pTimer->GetAsyncTime() - m_startTime;
	}
	CTimeValue Check(const char* msg)
	{
		CTimeValue elapsed = End();
		if (m_bOutput && msg && CCryActionCVars::Get().g_saveLoadExtendedLog != 0)
			CryLog("Checkpoint %12s : %.4f ms", msg, elapsed.GetMilliSeconds());
		Start();
		return elapsed;
	}
	CTimeValue  m_startTime;
	bool        m_bOutput;
	const char* m_szMsg;
};

// Safe pausing of the CTimer
// pause the timer
// if it had been already paused before it will not be unpaused
struct SPauseGameTimer
{
	SPauseGameTimer()
	{
		m_bGameTimerWasPaused = gEnv->pTimer->IsTimerPaused(ITimer::ETIMER_GAME);
		gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, true);
	}

	~SPauseGameTimer()
	{
		if (m_bGameTimerWasPaused == false)
			gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, false);
	}

	bool m_bGameTimerWasPaused;
};

struct SBasicEntityData
{
	enum ExtraFlags
	{
		FLAG_HIDDEN             = BIT(0),
		FLAG_INVISIBLE          = BIT(2),
		FLAG_IGNORE_POS_ROT_SCL = BIT(3),
		FLAG_PHYSICS_ENABLED    = BIT(4)
	};

	SBasicEntityData() :
		pEntity(NULL),
		pos(0.0f, 0.0f, 0.0f),
		rot(1.0f, 0.0f, 0.0f, 0.0f),
		scale(1.0f, 1.0f, 1.0f),
		flags(0),
		iPhysType(0),
		parentEntity(0),
		aiObjectId(0),
		isHidden(false),
		isInvisible(false),
		ignorePosRotScl(false),
		isPhysicsEnabled(false)
	{
	}

	void Serialize(TSerialize ser)
	{
		uint32 flags2 = 0;

		if (ser.IsWriting())
		{
			if (isHidden)          flags2 |= FLAG_HIDDEN;
			if (isInvisible)       flags2 |= FLAG_INVISIBLE;
			if (ignorePosRotScl)   flags2 |= FLAG_IGNORE_POS_ROT_SCL;
			if (isPhysicsEnabled)  flags2 |= FLAG_PHYSICS_ENABLED;

			flags2 |= (iPhysType & 0xF) << (32 - 8);
		}

		ser.Value("id", id);
		ser.Value("flags", flags);
		ser.Value("flags2", flags2);

		// Moved here from CEntity::Serialize.
		ser.Value("aiObjectID", aiObjectId);

		if (ser.IsReading())
		{
			isHidden = (flags2 & FLAG_HIDDEN) != 0;
			isInvisible = (flags2 & FLAG_INVISIBLE) != 0;
			ignorePosRotScl = (flags2 & FLAG_IGNORE_POS_ROT_SCL) != 0;
			isPhysicsEnabled = (flags2 & FLAG_PHYSICS_ENABLED) != 0;
			iPhysType = (flags2 >> (32 - 8)) & 0xF;
		}

		if (!ignorePosRotScl)
		{
			ser.Value("pos", pos);

			if (ser.IsReading() || !rot.IsIdentity())
				ser.Value("rot", rot);

			if (ser.IsReading() || !scale.IsEquivalent(Vec3(1.0f, 1.0f, 1.0f)))
				ser.Value("scl", scale);
		}

		if (ser.IsReading() && scale.IsZero())
		{
			scale = Vec3(1.0f, 1.0f, 1.0f);
		}

		// Don't need to serialize this on non-dynamic entities.
		if (!(flags & ENTITY_FLAG_UNREMOVABLE) || !CCryActionCVars::Get().g_saveLoadBasicEntityOptimization)
		{
			ser.Value("name", name);
			ser.Value("class", className);
			ser.Value("archetype", archetype);
		}

		ser.Value("parent", parentEntity);
	}

	inline bool operator==(const SBasicEntityData& otherData) const
	{
		return id == otherData.id;
	}

	inline bool operator!=(const SBasicEntityData& otherData) const
	{
		return !(*this == otherData);
	}

	inline bool operator<(const SBasicEntityData& otherData) const
	{
		return id < otherData.id;
	}

	inline bool operator>(const SBasicEntityData& otherData) const
	{
		return id > otherData.id;
	}

	IEntity*    pEntity;
	EntityId    id;
	string      name;
	string      className;
	string      archetype;
	Vec3        pos;
	Quat        rot;
	Vec3        scale;
	uint32      flags;
	EntityId    parentEntity;
	tAIObjectID aiObjectId;
	int         iPhysType;

	bool        isHidden         : 1;
	bool        isInvisible      : 1;
	bool        ignorePosRotScl  : 1;
	bool        isPhysicsEnabled : 1;
};

class CSaveGameHolder
{
public:
	CSaveGameHolder(ISaveGame* pSaveGame) : m_pSaveGame(pSaveGame) {}
	~CSaveGameHolder()
	{
		if (m_pSaveGame)
			m_pSaveGame->Complete(false);
	}

	static ISaveGame* ReturnNull()
	{
		return NULL;
	}

	bool PointerOk()
	{
		return m_pSaveGame != 0;
	}

	bool Complete()
	{
		if (!m_pSaveGame)
			return false;
		bool ok = m_pSaveGame->Complete(true);
		m_pSaveGame = 0;
		return ok;
	}

	ISaveGame* operator->() const
	{
		return m_pSaveGame;
	}

	ISaveGame* Get() const
	{
		return m_pSaveGame;
	}

private:
	ISaveGame* m_pSaveGame;
};

struct STempAutoResourcesLock
{
	STempAutoResourcesLock() : m_bIsLocked(false)
	{
		Lock();
	}

	~STempAutoResourcesLock()
	{
		Unlock();
	}

	void Lock()
	{
		if (!m_bIsLocked)
		{
			if (gEnv->p3DEngine)
				gEnv->p3DEngine->LockCGFResources();
			m_bIsLocked = true;
		}
	}

	void Unlock()
	{
		if (m_bIsLocked)
		{
			if (gEnv->p3DEngine)
				gEnv->p3DEngine->UnlockCGFResources();
			m_bIsLocked = false;
		}
	}

	bool m_bIsLocked;
};

//TODO: this class is nearly identical to CSaveGameHolder, but CSaveGameHolder has an extra Complete() method. Is it safe to merge these two?
class CLoadGameHolder
{
public:
	CLoadGameHolder(ILoadGame* pLoadGame) : m_pLoadGame(pLoadGame) {}
	~CLoadGameHolder()
	{
		if (m_pLoadGame)
			m_pLoadGame->Complete();
	}

	static ILoadGame* ReturnNull()
	{
		return NULL;
	}

	bool PointerOk()
	{
		return m_pLoadGame != 0;
	}

	ILoadGame* operator->() const
	{
		return m_pLoadGame;
	}

	ILoadGame* Get() const
	{
		return m_pLoadGame;
	}

private:
	ILoadGame* m_pLoadGame;
};

#endif //__GAME_SERIALIZE_HELPERS_H__
