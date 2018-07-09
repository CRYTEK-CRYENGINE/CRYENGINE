// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CGAMEMECHANISMBASE_H__
#define __CGAMEMECHANISMBASE_H__

#include "GameMechanismEvents.h"

#if defined(_RELEASE)
#define INCLUDE_NAME_IN_GAME_MECHANISMS 0 // Do not switch this on! [TF]
#else
#define INCLUDE_NAME_IN_GAME_MECHANISMS 1
#endif

#if INCLUDE_NAME_IN_GAME_MECHANISMS
// NB: Specifying the wrong class name in the REGISTER_GAME_MECHANISM macro will now fail to compile, so it should be
// impossible (or trickier at least) to accidentally register your game mechanism instance with the wrong name. [TF]
#define REGISTER_GAME_MECHANISM(classType) CGameMechanismBase((GetConstSelf() == (classType *) NULL) ? NULL : (# classType))
#else
#define REGISTER_GAME_MECHANISM(classType) CGameMechanismBase(NULL)
#endif

class CGameMechanismBase
{
	public:
	struct SLinkedListPointers
	{
		CGameMechanismBase * m_nextMechanism;
		CGameMechanismBase * m_prevMechanism;
	};

	CGameMechanismBase(const char * className);
	virtual ~CGameMechanismBase();
	virtual void Update(float dt) = 0;
	virtual void Inform(EGameMechanismEvent gmEvent, const SGameMechanismEventData * data) {}

	ILINE SLinkedListPointers * GetLinkedListPointers()
	{
		return & m_linkedListPointers;
	}

#if INCLUDE_NAME_IN_GAME_MECHANISMS
	ILINE const char * GetName()
	{
		return m_className;
	}
#else
	ILINE const char * GetName()
	{
		return "?";
	}
#endif

	protected:
	// Get a const 'this' pointer which we're allowed to use during subclass initialization without disabling warning C4355
	inline const CGameMechanismBase * GetConstSelf() const { return this; }

	private:
	SLinkedListPointers m_linkedListPointers;

#if INCLUDE_NAME_IN_GAME_MECHANISMS
	const char * m_className;
#endif
};

#endif //__CGAMEMECHANISMBASE_H__