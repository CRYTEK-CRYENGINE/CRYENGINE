// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Checkpoint Save/Load system for Game04

   -------------------------------------------------------------------------
   History:
   - 10:07:2008 : Created By Jan Müller
   - 05:02:2009 : Refactored and moved By Kevin Kirst

*************************************************************************/

#ifndef __CHECKPOINT_SYSTEM_H__
#define __CHECKPOINT_SYSTEM_H__

#include "ICheckPointSystem.h"

class CCheckpointSystem : public ICheckpointSystem
{
public:
	virtual ~CCheckpointSystem();

	//get save/load system
	static CCheckpointSystem* GetInstance();

	//retrieve metaData for a checkpoint file
	bool GetMetaData(const char* fileName, SCheckpointData& metaData, bool bRepairId = true);

	//load last (during this session) saved checkpoint
	virtual bool LoadLastCheckpoint();

	//load a game checkpoint from the specified filename
	virtual bool LoadGame(const char* fileName);

	//save a game checkpoint with the specified filename
	virtual bool SaveGame(EntityId checkpointId, const char* fileName);

	//set game handler
	virtual void SetGameHandler(ICheckpointGameHandler* pHandler);

	//clear game handler
	virtual void ClearGameHandler();

	//add checkpoint system listener
	virtual void RegisterListener(ICheckpointListener* pListener);

	//remove checkpoint system listener
	virtual void RemoveListener(ICheckpointListener* pListener);

	//return a list of savegame names for the current level
	void GetCurrentLevelCheckpoints(std::vector<string>& saveNames);

	//save/load worldMatrix for an Entity
	virtual void SerializeWorldTM(IEntity* pEntity, XmlNodeRef data, bool writing);

	// use this to save XMLNodes during OnSave
	bool SaveXMLNode(XmlNodeRef node, const char* identifier);

	// use this to load XMLNodes during OnLoad
	XmlNodeRef LoadXMLNode(const char* identifier);

	// save a single, external entity position/orientation/status during "OnSave"
	// the entity is loaded automatically with the checkpoint
	virtual bool SaveExternalEntity(EntityId id);

private:
	//this is a singleton
	CCheckpointSystem();

	//SAVING *********************************

	//actor data : mainly flags
	void WriteActorData(XmlNodeRef parentNode);

	//vehicle data
	void WriteVehicleData(XmlNodeRef parentNode);

	//checkpoint data
	void WriteMetaData(EntityId checkpointId, XmlNodeRef parentNode, SCheckpointData& outMetaData);

	//save the game token system
	void WriteGameTokens(XmlNodeRef parentNode);

	//write xml node to file
	void WriteXML(XmlNodeRef data, const char* fileName);

	//LOADING ********************************

	//delete all dynamic entities
	void DeleteDynamicEntities();

	//trigger flowgraph node etc.
	void OnCheckpointLoaded(SCheckpointData metaData);

	//loads the map a checkpoint was saved in, if it's not matching the current level
	bool LoadCheckpointMap(const SCheckpointData& metaData, CryFixedStringT<32>& curLevelName);

	//loads entities which were saved by external calls (flowgraph)
	void LoadExternalEntities(XmlNodeRef parentNode);

	//read checkpoint from XML
	XmlNodeRef ReadXML(const char* fileName);

	//load the game token system
	void ReadGameTokens(XmlNodeRef parentNode);

	//resetting dynamic parts of the engine
	void ResetEngine();

	//respawn AI's and read actor flags
	void RespawnAI(XmlNodeRef data);

	//reads meta data
	bool ReadMetaData(XmlNodeRef parentNode, SCheckpointData& metaData, bool bRepairId = true);

	//finish up loading and start game
	void RestartGameplay();

	//SHARED DATA ***************************

	//scan the savegame directory and return savegame names in a vector
	static void ParseSavedFiles(std::vector<string>& saveNames, const char* pLevelName = NULL);

	//test/fix the entity id of a saved entity by checking for it's name
	//returns true if the id is fixed, false if it's (still) broken
	bool RepairEntityId(EntityId& id, const char* pEntityName);

	//this removes sub-directories from a level name
	void RepairLevelName(CryFixedStringT<32>& levelName);

	//set or fix filename extension
	static void SetFilenameExtension(FixedCheckpointString& fileName);

	//sends onSave/onLoad to all listeners
	static void UpdateListener(SCheckpointData data, bool wasSaving);

	//VARIABLES ***************************

	//save last checkpoint for "quickload"
	static FixedCheckpointString g_lastSavedCheckpoint;

	//list of CheckpointSystem listeners
	static std::list<ICheckpointListener*> g_vCheckpointSystemListeners;

	//game handler
	ICheckpointGameHandler* m_pGameHandler;
};

#endif //__CHECKPOINT_SYSTEM_H__
