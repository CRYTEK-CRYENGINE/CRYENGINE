// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Implementation of the IGameplayRecorder interface.

   -------------------------------------------------------------------------
   History:
   - 22:1:2007   18:53 : Created by Marcio Martins

*************************************************************************/
#ifndef __GAMEPLAYRECORDER_H__
#define __GAMEPLAYRECORDER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryGame/IGameFramework.h>
#include "IGameplayRecorder.h"
#include "IMetadataRecorder.h"

#include "CompositeData.h"

enum EDataTags4CC
{
	eDT_frame     = 'fram',
	eDT_actor     = 'actr',
	eDT_entity    = 'entt',
	eDT_name      = 'name',
	eDT_type      = 'type',
	eDT_health    = 'hlth',
	eDT_armor     = 'armr',
	eDT_isgod     = 'igod',
	eDT_3rdperson = 'p3rd',
	eDT_item      = 'item',
	eDT_posi      = 'posi',
	eDT_weapon    = 'weap',
	eDT_ammocount = 'ammo',
	eDT_firemode  = 'fire',
	eDT_ammotype  = 'ammt',
	eDT_position  = 'pos0',
};

class CGameplayRecorder : public IGameplayRecorder
{
public:
	CGameplayRecorder(CCryAction* pGameFramework);
	virtual ~CGameplayRecorder();

	void Init();
	void Update(float frameTime);
	void Release();
	void GetMemoryStatistics(ICrySizer* s);

	// IGameplayRecorder
	void                               RegisterListener(IGameplayListener* pGameplayListener);
	void                               UnregisterListener(IGameplayListener* pGameplayListener);
	virtual IGameStateRecorder*        EnableGameStateRecorder(bool bEnable, IGameplayListener* pGameplayListener, bool bRecording);
	inline virtual IGameStateRecorder* GetIGameStateRecorder()   { return m_pGameStateRecorder; }

	virtual CTimeValue                 GetSampleInterval() const { return m_sampleinterval; };

	void                               Event(IEntity* pEntity, const GameplayEvent& event);

	void                               OnGameData(const IMetadata* pGameData);
	//~IGameplayRecorder
private:
	CCryAction*                     m_pGameFramework;

	std::vector<IGameplayListener*> m_listeners;
	CTimeValue                      m_lastdiscreet;
	CTimeValue                      m_sampleinterval;

	IGameStateRecorder*             m_pGameStateRecorder;

	IMetadataRecorderPtr            m_pMetadataRecorder;

	class CExampleMetadataListener : public IMetadataListener
	{
	public:
		CExampleMetadataListener()
		{
			REGISTER_COMMAND("gamedata_record", GameDataRecord, VF_NULL, "");
			REGISTER_COMMAND("gamedata_playback", GameDataPlayback, VF_NULL, "");
			REGISTER_COMMAND("gamedata_stop_record_or_playback", GameDataStopRecordOrPlayback, VF_NULL, "");
		}

		// IMetadataListner
		void OnData(const IMetadata* metadata);

		virtual ~CExampleMetadataListener(){}
		void RecordGameData();

	private:
		static inline void KeyToString(uint32 key, char* buffer, size_t bufferSize)
		{
			bool isAlnum = true;
			int gotNull = 0;
			for (int i = 0; i < 4; i++)
			{
				char c = (key >> (8 * i)) & 0xff;
				if (!c)
					gotNull++;
				else if (gotNull)
					isAlnum = false;
				else
					isAlnum &= (isprint(int(c)) != 0);
			}

			if (isAlnum)
			{
				for (int i = 0; i < 4 - gotNull; i++)
					buffer[3 - i - gotNull] = (key >> (8 * i)) & 0xff;
				buffer[4 - gotNull] = 0;
			}
			else
			{
				cry_sprintf(buffer, bufferSize, "0x%.8x", key);
			}
		}

		static inline string KeyToString(uint32 key)
		{
			char buffer[16];
			KeyToString(key, buffer, sizeof(buffer));
			return buffer;
		}

		static void DumpMetadata(const IMetadata* metadata, const size_t ntabs)
		{
			string tabs;
			for (size_t i = 0; i < ntabs; ++i)
				tabs += ' ';
			string tag = KeyToString(metadata->GetTag());
			string typ = KeyToString(metadata->GetValueType());
			CryLog("%stag=%s, type=%s", tabs.c_str(), tag.c_str(), typ.c_str());
			for (size_t i = 0; i < metadata->GetNumFields(); ++i)
				DumpMetadata(metadata->GetFieldByIndex(i), ntabs + 1);
		}

		static void GameDataRecord(IConsoleCmdArgs* args)
		{
			if (args->GetArgCount() != 2)
				return;

			CGameplayRecorder* pRecorder = static_cast<CGameplayRecorder*>(CCryAction::GetCryAction()->GetIGameplayRecorder());
			if (!pRecorder->m_pMetadataRecorder->InitSave(args->GetArg(1)))
				GameWarning("MetadataRecorder::InitSave failed");
		}

		static void GameDataPlayback(IConsoleCmdArgs* args)
		{
			if (args->GetArgCount() != 2)
				return;

			CGameplayRecorder* pRecorder = static_cast<CGameplayRecorder*>(CCryAction::GetCryAction()->GetIGameplayRecorder());
			if (!pRecorder->m_pMetadataRecorder->InitLoad(args->GetArg(1)))
				GameWarning("MetadataRecorder::InitLoad failed");

			while (pRecorder->m_pMetadataRecorder->Playback(&pRecorder->m_example));
		}

		static void GameDataStopRecordOrPlayback(IConsoleCmdArgs* args)
		{
			CGameplayRecorder* pRecorder = static_cast<CGameplayRecorder*>(CCryAction::GetCryAction()->GetIGameplayRecorder());
			pRecorder->m_pMetadataRecorder->Reset();
		}
	};

	CExampleMetadataListener m_example;
};

#endif //__GAMEPLAYRECORDER_H__
