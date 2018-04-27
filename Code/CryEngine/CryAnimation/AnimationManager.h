// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#ifndef RESOURCE_COMPILER
#include "GlobalAnimationHeaderLMG.h"
#include "GlobalAnimationHeaderCAF.h"
#include "GlobalAnimationHeaderAIM.h"
#include "LoaderDBA.h"

class CAnimationSet;
class CryCGALoader;
class CAnimationManager;
class CAnimationSet;
struct CInternalSkinningInfo;

struct AnimSearchHelper
{

	typedef std::vector<int>                                                     TIndexVector;
	typedef std::map<uint32 /*crc*/, TIndexVector* /*vector of indexes*/>        TFoldersVector;

	typedef std::vector<uint32>                                                  TSubFolderCrCVector;
	typedef std::map<uint32 /*crc*/, TSubFolderCrCVector* /*vector of indexes*/> TSubFoldersMap;

	TFoldersVector       m_AnimationsMap;
	TSubFoldersMap       m_SubFoldersMap;

	bool                 AddAnimation(uint32 crc, uint32 gahIndex);
	void                 AddAnimation(const string& path, uint32 gahIndex);

	TSubFolderCrCVector* GetSubFoldersVector(uint32 crc);

	TIndexVector*        GetAnimationsVector(uint32 crc);
	TIndexVector*        GetAnimationsVector(const string& path);
	void                 Clear();

	AnimSearchHelper() {}
	~AnimSearchHelper() { Clear(); };

private:
	// Ensure non copyable
	AnimSearchHelper(const AnimSearchHelper&);
	AnimSearchHelper& operator=(const AnimSearchHelper&);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// class CAnimationManager
// Responsible for creation of multiple animations, subsequently bind to the character bones
// There is one instance of this class per a game.
/////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimationManager : public IAnimEvents
{
public:

	virtual IAnimEventList*       GetAnimEventList(const char* animationFilePath);
	virtual const IAnimEventList* GetAnimEventList(const char* animationFilePath) const;

	virtual bool                  SaveAnimEventToXml(const CAnimEventData& dataIn, XmlNodeRef& dataOut);
	virtual bool                  LoadAnimEventFromXml(const XmlNodeRef& dataIn, CAnimEventData& dataOut);
	virtual void                  InitializeSegmentationDataFromAnimEvents(const char* animationFilePath);

	void                          InitialiseRunTimePools();
	void                          DestroyRunTimePools();

	/////////////////////////////////////////////////////////////////////
	// finds the animation-asset by path-name.
	// Returns -1 if no animation was found.
	// Returns the animation ID if it was found.
	/////////////////////////////////////////////////////////////////////

	//! Searches for a CAF animation asset by its name.
	//! \param szFilePath Name of the animation to search for.
	//! \return Global ID of the animation or -1 if no animation was found.
	int GetGlobalIDbyFilePath_CAF(const char* szFilePath) const;

	//! Searches for an AIM animation asset by its name.
	//! \param szFilePath Name of the animation to search for.
	//! \return Global ID of the animation or -1 if no animation was found.
	int GetGlobalIDbyFilePath_AIM(const char* szFilePath) const;

	//! Searches for a blend-space (locomotion group) asset by its name.
	//! \param szFilePath Name of the blend-space to search for.
	//! \return Global ID of the blend-space or -1 if no blend-space was found.
	int GetGlobalIDbyFilePath_LMG(const char* szFilePath) const;

	//! Searches for a blend-space (locomotion group) asset by a crc32 hash of its name.
	//! \param szFilePath Lowercase crc32 hash of the blend-space name to search for.
	//! \return Global ID of the blend-space or -1 if no blend-space was found.
	int GetGlobalIDbyFilePathCRC_LMG(uint32 crc32) const;

	// create header with the specified name. If the header already exists, it return the index
	int               CreateGAH_CAF(const string& strFileName);
	int               CreateGAH_AIM(const string& strFileName);
	int               CreateGAH_LMG(const string& strFileName);

	CGlobalHeaderDBA* FindGlobalHeaderByCRC_DBA(uint32 crc);

	// loads existing animation record, returns false on error
	bool LoadAnimationTCB(int nGlobalAnimId, DynArray<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, const IDefaultSkeleton* pIDefaultSkeleton);

	void UnloadAnimationCAF(GlobalAnimationHeaderCAF& rCAF);
	void UnloadAnimationAIM(int nGLobalAnimID);

	// returns the total number of animations hold in memory (for statistics)
	size_t GetGlobalAnimCount() { return m_arrGlobalCAF.size();  }

	// notifies the controller manager that this client doesn't use the given animation any more.
	// these calls must be balanced with AnimationAddRef() calls
	void AnimationReleaseCAF(GlobalAnimationHeaderCAF& rCAF);
	void AnimationReleaseAIM(GlobalAnimationHeaderAIM& rAIM);
	void AnimationReleaseLMG(GlobalAnimationHeaderLMG& rLMG);

	// puts the size of the whole subsystem into this sizer object, classified, according to the flags set in the sizer
	void GetMemoryUsage(class ICrySizer* pSizer) const;
	void             DebugAnimUsage(uint32 printtxt);

	size_t           GetSizeOfDBA();
	bool             CreateGlobalHeaderDBA(DynArray<string>& name);
	bool             DBA_PreLoad(const char* pFilePathDBA, bool highPriority);
	bool             DBA_LockStatus(const char* pFilePathDBA, uint32 status, bool highPriority);
	bool             DBA_Unload(const char* pFilePathDBA);
	bool             DBA_Unload_All();
	bool             Unload_All_Animation();

	EReloadCAFResult ReloadCAF(const char* szFilePathCAF);
	int              ReloadLMG(const char* szFilePathLMG);

	uint32           GetDBACRC32fromFilePath(const char* szFilePathCAF);
	bool             IsDatabaseInMemory(uint32 nDBACRC32);

	DynArray<CGlobalHeaderDBA>         m_arrGlobalHeaderDBA;
	CNameCRCMap                        m_AnimationMapCAF;
	DynArray<GlobalAnimationHeaderCAF> m_arrGlobalCAF;
	//	CNameCRCMap		m_AnimationMapAIM;
	DynArray<GlobalAnimationHeaderAIM> m_arrGlobalAIM;
	//	CNameCRCMap		m_AnimationMapLMG;
	DynArray<GlobalAnimationHeaderLMG> m_arrGlobalLMG;

#ifdef EDITOR_PCDEBUGCODE
	typedef std::map<string, DynArray<char>> CachedBSPACES;
	CachedBSPACES m_cachedBSPACES;
#endif

	AnimSearchHelper   m_animSearchHelper;
	SAnimMemoryTracker m_AnimMemoryTracker;

	bool               m_shuttingDown;

	CAnimationManager(const CAnimationManager&);
	void operator=(const CAnimationManager&);

	CAnimationManager() : m_shuttingDown(false)
	{
		//We reserve the place for future animations.
		//The best performance will be archived when this number is >= actual number of animations that can be used, and not much greater
		m_arrGlobalHeaderDBA.reserve(128);
		m_arrGlobalCAF.reserve(0);
		m_arrGlobalAIM.reserve(0);
		m_arrGlobalLMG.reserve(128);

#if BLENDSPACE_VISUALIZATION
		//the first LMG must be an Internal Type (it is a pre-initalized 1D blend-space). We use it just for debugging on PC
		int nGlobalAnimID = CreateGAH_LMG("InternalPara1D.LMG");
		m_arrGlobalLMG[nGlobalAnimID].CreateInternalType_Para1D();
#endif

		InitialiseRunTimePools();
	}

	~CAnimationManager()
	{
		m_shuttingDown = true;
		DestroyRunTimePools();
	}

private:
};
#endif