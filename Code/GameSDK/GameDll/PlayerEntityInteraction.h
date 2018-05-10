// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYER_ENTITY_INTERACTION_H__
#define __PLAYER_ENTITY_INTERACTION_H__

#if _MSC_VER > 1000
# pragma once
#endif



class CPlayerEntityInteraction
{
	struct SLastInteraction
	{
		SLastInteraction()
		{
			Reset();
		}

		void Reset()
		{
			m_frameId = 0;
		}
		 
		bool CanInteractThisFrame( const int frameId ) const
		{
			return (m_frameId != frameId);
		}

		void Update( const int frameId )
		{
			m_frameId = frameId;
		}

	private:
		int      m_frameId;
	};

public:
	CPlayerEntityInteraction();

	void ItemPickUpMechanic(CPlayer* pPlayer, const ActionId& actionId, int activationMode);
	void UseEntityUnderPlayer(CPlayer* pPlayer);

	void JustInteracted( );

	void Update(CPlayer* pPlayer, float frameTime);

private:
	void ReleaseHeavyWeapon(CPlayer* pPlayer);

	SLastInteraction m_lastUsedEntity;

	float m_autoPickupDeactivatedTime;

	bool m_useHoldFiredAlready;
	bool m_usePressFiredForUse;
	bool m_usePressFiredForPickup;
	bool m_useButtonPressed;
};



#endif
