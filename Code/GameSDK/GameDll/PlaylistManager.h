// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Manager for playlists

-------------------------------------------------------------------------
History:
- 06:03:2010 : Created by Tom Houghton

*************************************************************************/

#ifndef ___PLAYLISTMANAGER_H___
#define ___PLAYLISTMANAGER_H___

#if _MSC_VER > 1000
# pragma once
#endif

#include "ILevelSystem.h"

//------------------------------------------------------------------------
#define GAME_VARIANT_NAME_MAX_LENGTH	32
#define USE_DEDICATED_LEVELROTATION 1
#define PLAYLIST_MANAGER_CUSTOM_NAME "Custom"

//------------------------------------------------------------------------
struct SSupportedVariantInfo
{
	SSupportedVariantInfo(const char *pName) : m_name(pName), m_id(0) {}
	SSupportedVariantInfo() : m_name("Unknown"), m_id(0) {}
	SSupportedVariantInfo(const char *pName, int id) : m_name(pName), m_id(id) {}

	typedef CryFixedStringT<GAME_VARIANT_NAME_MAX_LENGTH> TFixedString;

	TFixedString	m_name;
	int						m_id;
};

//------------------------------------------------------------------------
struct SPlaylistRotationExtension
{
	typedef std::vector<SSupportedVariantInfo> TVariantsVec;
	typedef CryFixedStringT<32>  TLocaliNameStr;
	typedef CryFixedStringT<64>  TLocaliDescStr;

	string  uniqueName;
	TLocaliNameStr localName;			// if @ string key for localized name, or just already localized name
	TLocaliDescStr localDescription;			// if @ string key for localized description, or just already localized description
	CryFixedStringT<64> imagePath;
	TVariantsVec m_supportedVariants;
	int		m_maxPlayers;
	bool  m_enabled;
	bool  m_requiresUnlocking;
	bool  m_hidden;
	bool  m_resolvedVariants;
	bool  m_allowDefaultVariant;
	bool	m_newC3Playlist;

	void Reset()
	{
		uniqueName.resize(0);
		localName.clear();
		localDescription.clear();
		m_supportedVariants.clear();
		m_maxPlayers = MAX_PLAYER_LIMIT;
		m_enabled = false;
		m_requiresUnlocking = false;
		m_hidden = false;
		m_resolvedVariants = false;
		m_allowDefaultVariant = true;
		m_newC3Playlist = false;
	}

	// Suppress passedByValue for smart pointers like XmlNodeRef
	// cppcheck-suppress passedByValue
	bool LoadFromXmlNode(const XmlNodeRef xmlNode);
};

//------------------------------------------------------------------------
struct SPlaylist
{
	void Reset()
	{
		rotExtInfo.Reset();
		id = 0;
		bSynchedFromServer = false;
#if USE_DEDICATED_LEVELROTATION
		m_bIsCustomPlaylist = false;
#endif
	}
	ILINE const char* GetName() const { return rotExtInfo.localName.c_str(); }
	ILINE const char* GetDescription() const { return rotExtInfo.localDescription.c_str(); }
	ILINE const char* GetUniqueName() const { return rotExtInfo.uniqueName.c_str(); }
	ILINE bool  IsEnabled() const { return rotExtInfo.m_enabled; }
	ILINE bool  IsHidden() const { return rotExtInfo.m_hidden; }
	ILINE void  SetEnabled(bool e) { rotExtInfo.m_enabled = e; }
	ILINE bool	RequiresUnlocking() const { return rotExtInfo.m_requiresUnlocking; }
	ILINE bool  IsSelectable() const { return (!bSynchedFromServer); }
	ILINE const SPlaylistRotationExtension::TVariantsVec &GetSupportedVariants() const
	{
		CRY_ASSERT(rotExtInfo.m_resolvedVariants);
		return rotExtInfo.m_supportedVariants;
	}
	void ResolveVariants();

	SPlaylistRotationExtension  rotExtInfo;
	ILevelRotation::TExtInfoId  id;
	bool												bSynchedFromServer;
#if USE_DEDICATED_LEVELROTATION
	bool												m_bIsCustomPlaylist;
#endif
};

//------------------------------------------------------------------------
struct SGameVariant
{
	SGameVariant() : m_id(0), m_restrictRank(0), m_requireRank(0), m_allowInCustomGames(false), m_enabled(false), m_requiresUnlock(false), m_supportsAllModes(true), m_allowSquads(true), m_allowClans(true) {}

	typedef CryFixedStringT<8> TFixedString8;
	typedef CryFixedStringT<GAME_VARIANT_NAME_MAX_LENGTH> TFixedString;
	typedef CryFixedStringT<64> TLongFixedString;

	typedef CryFixedArray<TLongFixedString, 32> TLongStringArray;
	typedef CryFixedArray<int, 16> TIntArray;

	TFixedString			m_name;
	// For m_local* string params : If the first character is @ it'll be localized in flash, otherwise it should have been localized already.
	TFixedString			m_localName;
	TLongFixedString		m_localDescription;
	TLongFixedString		m_localDescriptionUpper;
	TFixedString8				m_suffix;

	TLongStringArray	m_options;
	TIntArray					m_supportedModes;			// Only populated if m_supportsAllModes is not set, 
																					// contains values from EGameMode enum
	int								m_id;
	int								m_restrictRank;
	int								m_requireRank;
	bool							m_allowInCustomGames;
	bool							m_enabled;
	bool							m_requiresUnlock;
	bool							m_supportsAllModes;
	bool							m_allowSquads;
	bool							m_allowClans;
};

//------------------------------------------------------------------------
struct SGameModeOption
{
public:
	typedef CryFixedStringT<32> TFixedString;

	SGameModeOption() : m_pCVar(NULL), m_netMultiplyer(1.f), m_profileMultiplyer(1.f), m_fDefault(0.f), m_iDefault(0), m_gameModeSpecific(false), m_floatPrecision(0) {}

	void SetInt(ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, int iDefault);
	void SetFloat(ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer, float fDefault, int floatPrecision);
	void SetString(ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific);
	void CopyToCVar(CProfileOptions *pProfileOptions, bool useDefault, const char *pGameModeName);
	void CopyToProfile(CProfileOptions *pProfileOptions, const char *pGameModeName);
	bool IsGameModeSpecific() { return m_gameModeSpecific; }

private:
	void SetCommon(ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer);

public:
	TFixedString m_profileOption;
	TFixedString m_strValue;
	ICVar *m_pCVar;
	float m_netMultiplyer;			// Value to multiply the cvar value to send across the network.
	float m_profileMultiplyer;	// Value to multiply the cvar value to get the profile one.
	float m_fDefault;
	int m_iDefault;
	int m_floatPrecision;
	bool m_gameModeSpecific;
};

//------------------------------------------------------------------------
struct SConfigVar
{
	ICVar *m_pCVar;
	bool m_bNetSynched;
};

//------------------------------------------------------------------------
struct SOptionRestriction
{
	enum EOperationType
	{
		eOT_Equal,
		eOT_NotEqual,
		eOT_LessThan,
		eOT_GreaterThan,
	};

	struct SOperand
	{
		struct SValue
		{
			CryFixedStringT<16> m_string;
			float m_float;
			int m_int;
		};

		ICVar *m_pVar;
		SValue m_comparisionValue;
		SValue m_fallbackValue;
		EOperationType m_type;
	};

	SOperand m_operand1;
	SOperand m_operand2;
};

//------------------------------------------------------------------------
class CPlaylistManager :	public IConsoleVarSink
{
private:
	typedef std::vector<SPlaylist>					TPlaylists;
	typedef std::vector<SGameModeOption>		TOptionsVec;
	typedef std::vector<SConfigVar>					TConfigVarsVec;
	typedef std::vector<SOptionRestriction> TOptionRestrictionsVec;

public:
	typedef std::vector<SGameVariant>	TVariantsVec;
	typedef std::vector<SGameVariant*>	TVariantsPtrVec;

	// IConsoleVarSink
	virtual bool OnBeforeVarChange( ICVar *pVar,const char *sNewValue ) { return true; }
	virtual void OnAfterVarChange( ICVar *pVar );
	virtual void OnVarUnregister(ICVar* pVar) {}
	// ~IConsoleVarSink

	CPlaylistManager();
	~CPlaylistManager();

	void  Init(const char* pPath);

	void  AddPlaylistsFromPath(const char* pPath);
	void  AddPlaylistFromXmlNode(XmlNodeRef xmlNode);

	void  AddVariantsFromPath(const char* pPath);

	ILevelRotation*  GetLevelRotation();

	bool  DisablePlaylist(const char* uniqueName);

	ILINE bool  HavePlaylistSet() const { return (m_currentPlaylistId != 0); } 

	bool ChoosePlaylist(const int chooseIdx);
	bool ChoosePlaylistById(const ILevelRotation::TExtInfoId playlistId);
	bool ChooseVariant(const int selectIdx);

	bool HaveUnlockedPlaylist(const SPlaylist *pPlaylist, CryFixedStringT<128>* pUnlockString=NULL);
	bool HaveUnlockedPlaylistVariant(const SPlaylist *pPlaylist, const int variantId, CryFixedStringT<128>* pUnlockString=NULL);

	const char *GetActiveVariant();
	const int GetActiveVariantIndex() const;
	ILINE bool IsUsingCustomVariant() { return GetActiveVariantIndex() == m_customVariantIndex; }
	const char *GetVariantName(const int index);
	int GetVariantIndex(const char *pName);
	const int GetNumVariants() const;
	const int GetNumPlaylists() const;

	const SPlaylist* GetPlaylist(const int index);
	const SPlaylist* GetCurrentPlaylist();
	const SPlaylist* GetPlaylistByUniqueName(const char *pInUniqueName)			{ return FindPlaylistByUniqueName(pInUniqueName); }
	const char*			 GetPlaylistNameById(ILevelRotation::TExtInfoId findId)			{ return FindPlaylistById(findId)->GetName(); }
	int              GetPlaylistIndex( ILevelRotation::TExtInfoId findId ) const;

	void ClearCurrentPlaylist() { SetCurrentPlaylist(0); }

	ILevelRotation::TExtInfoId CreateCustomPlaylist(const char *pName);

	void SetModeOptions();

	const TVariantsVec &GetVariants() const { return m_variants; }
	void GetVariantsForGameMode(const char *pGameMode, TVariantsPtrVec &result);

	ILINE int GetDefaultVariant() const { return m_defaultVariantIndex; }
	ILINE int GetCustomVariant() const { return m_customVariantIndex; }
	ILINE const SGameVariant *GetVariant(int index) const
	{
		if ((index >= 0) && (index < (int)m_variants.size()))
		{
			return &m_variants[index];
		}
		else
		{
			return NULL;
		}
	}

	void GetTotalPlaylistCounts(uint32 *outUnlocked, uint32 *outTotal);

	void SetGameModeOption(const char *pOption, const char *pValue);
	void GetGameModeOption(const char *pOption, CryFixedStringT<32> &result);
	uint32 GetGameModeOptionCount()
	{
		return (uint32)m_options.size();
	}
	void GetGameModeProfileOptionName(const uint32 index, CryFixedStringT<32> &result);

	uint16 PackCustomVariantOption(uint32 index);
	int UnpackCustomVariantOption(uint16 value, uint32 index, int* pIntValue, float* pFloatValue);
	int UnpackCustomVariantOptionProfileValues(uint16 value, uint32 index, int* pIntValue, float* pFloatValue, int* pFloatPrecision);
	void ReadDetailedServerInfo(uint16 *pOptions, uint32 numOptions);

	void WriteSetCustomVariantOptions(CCryLobbyPacket* pPacket, CPlaylistManager::TOptionsVec pOptions, uint32 numOptions);
	void WriteSetVariantPacket(CCryLobbyPacket *pPacket);

	void ReadSetCustomVariantOptions(CCryLobbyPacket* pPacket, CPlaylistManager::TOptionsVec pOptions, uint32 numOptions);
	void ReadSetVariantPacket(CCryLobbyPacket *pPacket);

	bool AdvanceRotationUntil(const int newNextIdx);

	void OnPromoteToServer();
	
	template<class T> static void GetLocalisationSpecificAttr(const XmlNodeRef& rNode, const char* pAttrKey, const char* pLang, T* pOutVal);

#if USE_DEDICATED_LEVELROTATION
	bool IsUsingCustomRotation();
#endif

	void IgnoreCVarChanges(bool enabled) { m_bIsSettingOptions = enabled; }

private:
	void  ClearAllVariantCVars();
	void  SaveCurrentSettingsToProfile();

	void  SetCurrentPlaylist(const ILevelRotation::TExtInfoId id);
	void  SetActiveVariant(int variantIndex);

	ILevelRotation*  GetRotationForCurrentPlaylist();

	SPlaylist*  FindPlaylistById(ILevelRotation::TExtInfoId findId);
	SPlaylist*  FindPlaylistByUniqueName(const char* uniqueName);

	void	LoadVariantsFile(const char *pPath);
	void  AddVariantFromXmlNode(XmlNodeRef xmlNode);
	bool  DisableVariant(const char *pName);
	SGameVariant *FindVariantByName(const char *pName);

	void AddGameModeOptionInt(const char *pCVarName, const char *pProfileOption, bool gameModeSpecific, int iDefault);
	void AddGameModeOptionFloat(const char *pCVarName, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer, float fDefault, int floatPrecision);
	void AddGameModeOptionString(const char *pCVarName, const char *pProfileOption, bool gameModeSpecific);
	SGameModeOption *GetGameModeOptionStruct(const char *pOptionName);

	void AddConfigVar(const char *pCVarName, bool bNetSynched);

	ILevelRotation::TExtInfoId GetPlaylistId(const char *pUniqueName) const;

	void  Deinit();

#if USE_DEDICATED_LEVELROTATION
	void LoadLevelRotation();
	void ReadServerInfo(XmlNodeRef xmlNode, int &outVariantIndex, int &outMaxPlayers);
	void SetOptionsFromXml(XmlNodeRef xmlNode, SGameVariant *pVariant);
#endif

#if !defined(_RELEASE)
	void DbgPlaylistsList();
	void DbgPlaylistsChoose(const int chooseIdx);
	void DbgPlaylistsUnchoose();
	void DbgPlaylistsShowVariants();
	void DbgPlaylistsSelectVariant(const int selectIdx);

	static void CmdPlaylistsList(IConsoleCmdArgs* pCmdArgs);
	static void CmdPlaylistsChoose(IConsoleCmdArgs* pCmdArgs);
	static void CmdPlaylistsUnchoose(IConsoleCmdArgs* pCmdArgs);
	static void CmdPlaylistsShowVariants(IConsoleCmdArgs* pCmdArgs);
	static void CmdPlaylistsSelectVariant(IConsoleCmdArgs* pCmdArgs);
#endif

	static void CmdStartPlaylist(IConsoleCmdArgs* pCmdArgs);
	static bool DoStartPlaylistCommand(const char *pPlaylistArg);

private:
	int GetSynchedVarsSize();
	void WriteSynchedVars(CCryLobbyPacket* pPacket);
	void ReadSynchedVars(CCryLobbyPacket* pPacket);

	void LoadOptionRestrictions();
	bool LoadOperand(XmlNodeRef operandXml, SOptionRestriction::SOperand &outResult);
	void CheckRestrictions(ICVar *pChangedVar);
	void ApplyRestriction(SOptionRestriction::SOperand &operand1, SOptionRestriction::SOperand &operand2);
	bool CheckOperation(SOptionRestriction::SOperand &operand, bool bEnsureFalse);

	static void OnCustomOptionCVarChanged(ICVar *pCVar);

	TPlaylists  m_playlists;
	TVariantsVec m_variants;
	TOptionsVec m_options;
	TConfigVarsVec m_configVars;
	TOptionRestrictionsVec m_optionRestrictions;

	ILevelRotation::TExtInfoId  m_currentPlaylistId;
	int m_activeVariantIndex;
	int m_defaultVariantIndex;
	int m_customVariantIndex;

	bool m_inited;
	bool m_bIsSettingOptions;
};


#endif  // ___PLAYLISTMANAGER_H___
