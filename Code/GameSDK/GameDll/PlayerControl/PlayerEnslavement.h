// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Various support classes for having the player's animation enslaved to other entities.

#if !defined(PlayerEnslavement_h)
#define PlayerEnslavement_h

class IActionController;


// Button-mashing sequence whereby the player escapes from the tractor-beams
// that some of the bosses use to 'capture' the player.
class CPlayerEnslavementForButtonMashing
{
public:

	CPlayerEnslavementForButtonMashing();
	~CPlayerEnslavementForButtonMashing();

	// Animation synchronization:
	void PreLoadADB(const char* adbFileName);
	void EnslavePlayer(IActionController* pMasterActionController, const bool enslave);


private:

	void ResetFightProgress();


private:

	static const char s_slaveContextName[];


private:

	// The file name of the ADB that contains the animations.
	CryFixedStringT<128> m_ADBFileName;

	// True if the animation action controller of the player has been enslaved to that of the
	// master entity; otherwise false.
	bool m_enslaved;	
};


// A helper class that controls the progression of the button-mashing fight
// and sends parameter information into the animation system.
class CPlayerFightProgressionForButtonMashing
{
public:

	CPlayerFightProgressionForButtonMashing();

	// Animation progression:
	void ResetFightProgress();
	void SetFightProgress( const float targetProgress );
	void UpdateAnimationParams( const float frameTime, float* currentFightProgress, float* currentFightProgressInv );

	// Queries:
	float GetCurrentFightProgress() const { return m_fightProgress; }
	float GetCurrentFightProgressInv() const { return m_fightProgressInv; }


private:

	static const float s_FightProgressionChangeSpeed;


private:

	// The current fight progression in the range of [0.0f .. 1.0f].
	float m_fightProgress;

	// The inverse of 'm_fightProgress'.
	float m_fightProgressInv;

	// The target fight progression as was dictated by the button-mashing handling routines (in the range
	// of [0.0f .. 1.0f]. -1.0f if no target fight progress was reported yet.
	float m_targetProgress;
};


#endif // !defined(PlayerEnslavement_h)
