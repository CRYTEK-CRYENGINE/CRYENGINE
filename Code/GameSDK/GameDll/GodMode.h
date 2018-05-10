// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GOD_MODE_H
#define GOD_MODE_H

class CActor;

enum EGodModeState
{
	eGMS_None,
	eGMS_GodMode,
	eGMS_TeamGodMode,
	eGMS_DemiGodMode,
	eGMS_NearDeathExperience,
	eGMS_LAST
};

class CGodMode
{
public:
	static CGodMode& GetInstance();

	void RegisterConsoleVars(IConsole* pConsole);
	void UnregisterConsoleVars(IConsole* pConsole) const;

	void MoveToNextState();
	bool RespawnIfDead(CActor* actor) const;
	bool RespawnPlayerIfDead() const;

	EGodModeState GetCurrentState() const;
	const char* GetCurrentStateString() const;

	void Update(float frameTime);
	void DemiGodDeath();

	bool IsGod() const;
	bool IsDemiGod() const;
	bool IsGodModeActive() const;

	void ClearCheckpointData();
	void SetNewCheckpoint(const Matrix34& rWorldMat);

private:
	CGodMode();
	CGodMode(const CGodMode&);
	CGodMode& operator=(const CGodMode&);

	static const float m_timeToWaitBeforeRespawn;
	static const char* m_godModeCVarName;
	static const char* m_demiGodRevivesAtCheckpointCVarName;

	float m_elapsedTime;
	int m_godMode;
	int m_demiGodRevivesAtCheckpoint;

	bool m_hasHitCheckpoint;
	bool m_respawningFromDemiGodDeath;
	Matrix34	m_lastCheckpointWorldTM;
};

#endif
