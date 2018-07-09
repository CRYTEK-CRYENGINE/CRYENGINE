// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Checkpoint Save/Load system for Game03

-------------------------------------------------------------------------
History:
- 10:07:2008 : Created By Jan Müller
- 05:02:2009 : Renamed to CheckpointGame for game-specific usage

*************************************************************************/

#ifndef __CHECKPOINTGAME_H__
#define __CHECKPOINTGAME_H__

#include <ICheckPointSystem.h>

class CCheckpointGame : public ICheckpointGameHandler
{
private:
	CCheckpointGame();
	static CCheckpointGame m_Instance;

public:
	static CCheckpointGame* GetInstance() { return &m_Instance; }
	virtual ~CCheckpointGame();

	void Init();

	// Writing game-specific data
	virtual void OnWriteData(XmlNodeRef parentNode);

	// Reading game-specific data
	virtual void OnReadData(XmlNodeRef parentNode);

	// Engine reset control
	virtual void OnPreResetEngine();
	virtual void OnPostResetEngine();

	// Restart
	virtual void OnRestartGameplay();

protected:
	//player data
	void WritePlayerData(XmlNodeRef parentNode);
	void ReadPlayerData(XmlNodeRef data);

	//player inventory
	void WritePlayerInventory(XmlNodeRef parentNode);
	void ReadPlayerInventory(XmlNodeRef parentNode);

	//get data or log warning
	template<class T>
	bool GetAttrSave(XmlNodeRef source, const char *name, T &data);
};

template<class T>
ILINE bool CCheckpointGame::GetAttrSave(XmlNodeRef source, const char *name, T &data)
{
	CRY_ASSERT(source != NULL);
	CRY_ASSERT(name);
	bool found = source->getAttr(name, data);
	if(!found)
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed reading %s from checkpoint node %s.", name, source->getTag());
	return found;
}

#endif //__CHECKPOINTGAME_H__
