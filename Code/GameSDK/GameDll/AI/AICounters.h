// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Helper to calculate and centralize global game side values related to AI actors
*************************************************************************/

#ifndef __AICOUNTERS_H__
#define __AICOUNTERS_H__


//////////////////////////////////////////////////////////////////////////
class CAICounter_Alertness
{
	public:
		struct IListener
		{
			virtual	~IListener(){}
			virtual void AlertnessChanged( int alertnessGlobal ) = 0; // called when there is any change on alertness, not just global. However only global value is sent. 
																																// To obtain the value of other alertness, have to call one of the Get's() from inside the callback function
		}; 


	public:
		CAICounter_Alertness();

		void Update( float frameTime );
		void Reset( bool bUnload );
		void AddListener( IListener *pListener );
		void RemoveListener( IListener *pListener );
		void Serialize( TSerialize ser );

		int GetAlertnessGlobal();
		int GetAlertnessEnemies();
		int GetAlertnessFriends();
		int GetAlertnessFaction( uint32 factionID );

	private:
		void NotifyListeners();
		void InstantUpdateIfNeed();
		void UpdateCounters();


	private:

		typedef std::vector<IListener *> TListenersVector;
		TListenersVector m_listeners;
		float m_timeNextUpdate;
		int m_alertnessGlobal;
		int m_alertnessFriends;
		int m_alertnessEnemies;
		std::vector<int> m_alertnessFaction;
		std::vector<int> m_tempAlertnessFaction;  // just to not recreate it every time we check if anything changed
		bool m_bFactionVectorsAreValid;
		bool m_bNewListeners;
		bool m_bJustUpdated;
		static const float UPDATE_INTERVAL;
};

//////////////////////////////////////////////////////////////////////////
class CAICounters
{
	public:
		CAICounters() { Reset( false ); }
		void Update( float frameTime );
		void Serialize( TSerialize ser );
		void Reset( bool bUnload );
		CAICounter_Alertness& GetAlertnessCounter() { return m_alertnessCounter; }

	private:

		CAICounter_Alertness m_alertnessCounter;
};

#endif //__AICOUNTERS_H__
