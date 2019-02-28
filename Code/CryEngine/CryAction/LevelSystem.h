// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LEVELSYSTEM_H__
#define __LEVELSYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "ILevelSystem.h"

#define LEVEL_ROTATION_DEBUG 0

#if LEVEL_ROTATION_DEBUG
	#define Log_LevelRotation(...) CryLog("CLevelRotation" __VA_ARGS__)
#else
	#define Log_LevelRotation(...)
#endif

class CLevelInfo :
	public ILevelInfo
{
	friend class CLevelSystem;
public:
	CLevelInfo() : m_heightmapSize(0), m_bMetaDataRead(false), m_isModLevel(false), m_scanTag(ILevelSystem::TAG_UNKNOWN), m_levelTag(ILevelSystem::TAG_UNKNOWN)
	{
		SwapEndian(m_scanTag, eBigEndian);
		SwapEndian(m_levelTag, eBigEndian);
	};
	virtual ~CLevelInfo() {};

	// ILevelInfo
	virtual const char*                      GetName() const override                 { return m_levelName.c_str(); };
	virtual const bool                       IsOfType(const char* sType) const override;
	virtual const char*                      GetPath() const override                 { return m_levelPath.c_str(); };
	virtual const char*                      GetPaks() const override                 { return m_levelPaks.c_str(); };
	virtual bool                             GetIsModLevel() const override           { return m_isModLevel; }
	virtual const uint32                     GetScanTag() const override              { return m_scanTag; }
	virtual const uint32                     GetLevelTag() const override             { return m_levelTag; }
	virtual const char*                      GetDisplayName() const override;
	virtual const char*                      GetPreviewImagePath() const override     { return m_previewImagePath.c_str(); }
	virtual const char*                      GetBackgroundImagePath() const override  { return m_backgroundImagePath.c_str(); }
	virtual const char*                      GetMinimapImagePath() const override     { return m_minimapImagePath.c_str(); }
	virtual int                              GetHeightmapSize() const override        { return m_heightmapSize; };
	virtual const bool                       MetadataLoaded() const override          { return m_bMetaDataRead; }

	virtual int                              GetGameTypeCount() const override        { return m_gameTypes.size(); };
	virtual const ILevelInfo::TGameTypeInfo* GetGameType(int gameType) const override { return &m_gameTypes[gameType]; };
	virtual bool                             SupportsGameType(const char* gameTypeName) const override;
	virtual const ILevelInfo::TGameTypeInfo* GetDefaultGameType() const override;
	virtual bool                             HasGameRules() const override            { return !m_gamerules.empty(); }

	virtual const ILevelInfo::SMinimapInfo&  GetMinimapInfo() const override          { return m_minimapInfo; }

	virtual const char*                      GetDefaultGameRules() const override     { return m_gamerules.empty() ? nullptr : m_gamerules[0].c_str(); }
	virtual size_t                           GetGameRulesCount() const override       { return m_gamerules.size(); }
	virtual size_t                           GetGameRules(const char** pszGameRules, size_t numGameRules) const override;
	virtual bool                             GetAttribute(const char* name, TFlowInputData& val) const override;
	// ~ILevelInfo

	void GetMemoryUsage(ICrySizer*) const;

private:
	void ReadMetaData();
	bool ReadInfo();

	bool OpenLevelPak();
	void CloseLevelPak();

	string                                 m_levelName;
	string                                 m_levelPath;
	string                                 m_levelPaks;
	string                                 m_levelDisplayName;
	string                                 m_previewImagePath;
	string                                 m_backgroundImagePath;
	string                                 m_minimapImagePath;

	string                                 m_levelPakFullPath;
	string                                 m_levelMMPakFullPath;
	string                                 m_levelSvoPakFullPath;

	std::vector<string>                    m_gamerules;
	int                                    m_heightmapSize;
	uint32                                 m_scanTag;
	uint32                                 m_levelTag;
	bool                                   m_bMetaDataRead;
	std::vector<ILevelInfo::TGameTypeInfo> m_gameTypes;
	bool                                   m_isModLevel;
	SMinimapInfo                           m_minimapInfo;
	typedef std::map<string, TFlowInputData, stl::less_stricmp<string>> TAttributeList;
	TAttributeList                         m_levelAttributes;

	DynArray<string>                       m_levelTypeList;
};

class CLevelRotation : public ILevelRotation
{
public:
	CLevelRotation();
	virtual ~CLevelRotation();

	typedef struct SLevelRotationEntry
	{
		string              levelName;
		std::vector<string> gameRulesNames;
		std::vector<int>    gameRulesShuffle;
		int                 currentModeIndex;

		const char* GameModeName() const; //will return next game mode/rules according to shuffle
	} SLevelRotationEntry;

	typedef std::vector<SLevelRotationEntry> TLevelRotationVector;

	// ILevelRotation
	virtual bool Load(ILevelRotationFile* file);
	virtual bool LoadFromXmlRootNode(const XmlNodeRef rootNode, const char* altRootTag);

	virtual void Reset();
	virtual int  AddLevel(const char* level);
	virtual void AddGameMode(int level, const char* gameMode);

	virtual int  AddLevel(const char* level, const char* gameMode);

	//call to set the playlist ready for a new session
	virtual void                Initialise(int nSeed);

	virtual bool                First();
	virtual bool                Advance();

	virtual bool                AdvanceAndLoopIfNeeded();

	virtual const char*         GetNextLevel() const;
	virtual const char*         GetNextGameRules() const;

	virtual const char*         GetLevel(uint32 idx, bool accessShuffled = true) const;
	virtual int                 GetNGameRulesForEntry(uint32 idx, bool accessShuffled = true) const;
	virtual const char*         GetGameRules(uint32 idx, uint32 iMode, bool accessShuffled = true) const;

	virtual const char*         GetNextGameRulesForEntry(int idx) const; //always matches shuffling

	virtual const int           NumAdvancesTaken() const;
	virtual void                ResetAdvancement();

	virtual int                 GetLength() const;
	virtual int                 GetTotalGameModeEntries() const;
	virtual int                 GetNext() const;

	virtual void                ChangeLevel(IConsoleCmdArgs* pArgs = NULL);

	virtual bool                NextPairMatch() const;

	virtual TRandomisationFlags GetRandomisationFlags() const { return m_randFlags; }
	virtual void                SetRandomisationFlags(TRandomisationFlags flags);

	virtual bool                IsRandom() const;
	//~ILevelRotation

	ILINE void                       SetExtendedInfoId(const ILevelRotation::TExtInfoId id) { m_extInfoId = id; }
	ILINE ILevelRotation::TExtInfoId GetExtendedInfoId()                                    { return m_extInfoId; }

protected:

	void      ModeShuffle();
	void      ShallowShuffle();
	void      AddGameMode(SLevelRotationEntry& level, const char* gameMode);
	void      AddLevelFromNode(const char* levelName, XmlNodeRef& levelNode);

	ILINE int GetRealRotationIndex(uint idx, bool accessShuffled) const
	{
		int realId = idx;
		if (idx < m_rotation.size())
		{
			if (accessShuffled && (m_randFlags & ePRF_Shuffle))
			{
				realId = m_shuffle[idx];
			}
		}
		else
		{
			realId = -1;
		}

		Log_LevelRotation(" GetRealRotationIndex passed %d returning %d", idx, realId);
		return realId;
	}

	TLevelRotationVector       m_rotation;
	TRandomisationFlags        m_randFlags;
	int                        m_next;
	std::vector<uint>          m_shuffle;
	int                        m_advancesTaken;

	ILevelRotation::TExtInfoId m_extInfoId;   //if this ID is set, we disable implicit shuffling
	bool                       m_hasGameModesDecks;
};

class CLevelSystem :
	public ILevelSystem,
	public ISystem::ILoadingProgressListener
{
public:
	CLevelSystem(ISystem* pSystem);
	virtual ~CLevelSystem();

	void Release() { delete this; };

	// ILevelSystem
	virtual DynArray<string>* GetLevelTypeList();
	virtual void              Rescan(const char* levelsFolder, const uint32 tag);
	virtual void              LoadRotation();
	virtual int               GetLevelCount();
	virtual ILevelInfo*       GetLevelInfo(int level);
	virtual ILevelInfo*       GetLevelInfo(const char* levelName);

	virtual void              AddListener(ILevelSystemListener* pListener);
	virtual void              RemoveListener(ILevelSystemListener* pListener);

	virtual ILevelInfo*       GetCurrentLevel() const { return m_pCurrentLevelInfo; }
	virtual ILevelInfo*       LoadLevel(const char* levelName);
	virtual void              UnLoadLevel();
	virtual ILevelInfo*       SetEditorLoadedLevel(const char* levelName, bool bReadLevelInfoMetaData = false);
	virtual void              PrepareNextLevel(const char* levelName);
	virtual float             GetLastLevelLoadTime() { return m_fLastLevelLoadTime; };
	virtual bool              IsLevelLoaded()        { return m_bLevelLoaded; }

	virtual ILevelRotation*   GetLevelRotation()     { return &m_levelRotation; };

	virtual ILevelRotation*   FindLevelRotationForExtInfoId(const ILevelRotation::TExtInfoId findId);

	virtual bool              AddExtendedLevelRotationFromXmlRootNode(const XmlNodeRef rootNode, const char* altRootTag, const ILevelRotation::TExtInfoId extInfoId);
	virtual void              ClearExtendedLevelRotations();
	virtual ILevelRotation*   CreateNewRotation(const ILevelRotation::TExtInfoId id);
	// ~ILevelSystem

	// ILoadingProgessListener
	virtual void OnLoadingProgress(int steps);
	// ~ILoadingProgessListener

	void PrecacheLevelRenderData();
	void GetMemoryUsage(ICrySizer* s) const;

	void SaveOpenedFilesList();

private:

	// ILevelSystemListener events notification
	void OnLevelNotFound(const char* levelName);
	void OnLoadingStart(ILevelInfo* pLevel);
	void OnLoadingLevelEntitiesStart(ILevelInfo* pLevelInfo);
	void OnLoadingComplete(ILevelInfo* pLevel);
	void OnLoadingError(ILevelInfo* pLevel, const char* error);
	void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount);
	void OnUnloadComplete(ILevelInfo* pLevel);

	void    ScanFolder(const string& rootFolder, bool modFolder, const uint32 tag);
	void    LogLoadingTime();
	bool    LoadLevelInfo(CLevelInfo& levelInfo);

	// internal get functions for the level infos ... they preserve the type and don't
	// directly cast to the interface
	CLevelInfo* GetLevelInfoInternal(int level);
	CLevelInfo* GetLevelInfoInternal(const char* levelName);
	CLevelInfo* GetLevelInfoByPathInternal(const char* szLevelPath);

	typedef std::vector<CLevelRotation> TExtendedLevelRotations;

	ISystem*                           m_pSystem;
	std::vector<CLevelInfo>            m_levelInfos;
	string                             m_levelsFolder;
	ILevelInfo*                        m_pCurrentLevelInfo;
	ILevelInfo*                        m_pLoadingLevelInfo;

	TExtendedLevelRotations            m_extLevelRotations;
	CLevelRotation                     m_levelRotation;

	string                             m_lastLevelName;
	float                              m_fLastLevelLoadTime;
	float                              m_fFilteredProgress;
	float                              m_fLastTime;

	bool                               m_bLevelLoaded;
	bool                               m_bRecordingFileOpens;

	int                                m_nLoadedLevelsCount;

	CTimeValue                         m_levelLoadStartTime;

	static int                         s_loadCount;

	std::vector<ILevelSystemListener*> m_listeners;

	DynArray<string>                   m_levelTypeList;
};

#endif //__LEVELSYSTEM_H__
