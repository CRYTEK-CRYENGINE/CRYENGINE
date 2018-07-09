// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

GameLocalizationManager is designed to look after the loading of strings
and be the sole place in the game that loads and unloads localization tags
*************************************************************************/

#ifndef GAME_LOCALIZATION_MANAGER_H
#define GAME_LOCALIZATION_MANAGER_H

#pragma once

class CGameLocalizationManager
{
public:
	CGameLocalizationManager();
	virtual ~CGameLocalizationManager();

	void SetGameType();
	void SetCredits(bool enable);

#if !defined(_RELEASE)
	static void LocalizationDumpLoadedTags(IConsoleCmdArgs* pArgs);
#endif //#if !defined(_RELEASE)

protected:
	enum ELocalizationTag
	{
		eLT_Init,
		eLT_GameType,
		eLT_Credits,
		eLT_Num
	};

	void LoadLocalizationData();
	void LegacyLoadLocalizationData();

	void LoadTag(ELocalizationTag tag);
	void LoadTagInternal( ELocalizationTag tag , const char* pTag );
	void UnloadTag(ELocalizationTag tag);

	string m_loadedTag[eLT_Num];
};

#endif //#ifndef GAME_LOCALIZATION_MANAGER_H
