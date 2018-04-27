// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIMATION_DATABASE_MANAGER_H__
#define __ANIMATION_DATABASE_MANAGER_H__

#include "AnimationDatabase.h"
#include "ICryMannequinEditor.h"

#include <CryCore/Containers/CryListenerSet.h>
#include <CrySystem/File/IResourceManager.h>

class CAnimationDatabaseLibrary
{
public:
	typedef const CAnimationDatabase* THandle;

	CAnimationDatabaseLibrary();
	~CAnimationDatabaseLibrary();

	void Insert(uint32 nCrc, CAnimationDatabase* pDb);

public:
	THandle LoadResource(const char* name, uint32 nFlags);
	THandle TryOpenResource(const char* name, uint32 nFlags);
	void    CreateResource(THandle& hdlOut, const char* name, uint32 nFlags);
	void    PublishResource(THandle& hdlOut);

protected:
	typedef std::map<uint32, CAnimationDatabase*> TAnimDatabaseList;

protected:
	bool LoadDatabase(const XmlNodeRef& xmlNode, CAnimationDatabase& animDB, bool recursive);
	bool LoadDatabaseDefinitions(const XmlNodeRef& root, const char* filename, const CTagDefinition** ppFragDefs, const CTagDefinition** ppTagDefs);

	void LoadDatabaseData
	(
	  CAnimationDatabase& animDB,
	  const XmlNodeRef root,
	  const CTagDefinition& fragIDs,
	  const CTagDefinition& tagIDs,
	  const TagState& adbFilter,
	  bool recursive,
	  CAnimationDatabase::SSubADB* subAnimDB = NULL
	);

	void AnimEntryToXML(XmlNodeRef outNode, const SAnimationEntry& animEntry, const char* name) const;
	bool XMLToAnimEntry(SAnimationEntry& animEntry, XmlNodeRef root) const;

	bool XMLToFragment(CFragment& fragment, XmlNodeRef root, bool transition) const;
	void FragmentToXML(XmlNodeRef outNode, const CFragment* fragment, bool transition) const;

	void ProceduralToXml(XmlNodeRef pOutNode, const SProceduralEntry& proceduralEntry) const;
	bool XmlToProcedural(XmlNodeRef pInNode, SProceduralEntry& outProceduralEntry) const;

	void XMLToBlend(SAnimBlend& animBlend, XmlNodeRef xmlBlend) const;
	void BlendToXML(XmlNodeRef outNode, const SAnimBlend& animBlend, const char* name) const;

private:
	CAnimationDatabaseLibrary(const CAnimationDatabaseLibrary&);
	CAnimationDatabaseLibrary& operator=(const CAnimationDatabaseLibrary&);

protected:
	TAnimDatabaseList m_databases;

	CTagDefinition    m_animFlags;
	CTagDefinition    m_transitionFlags;
};

class CAnimationTagDefLibrary
{
public:
	typedef const CTagDefinition* THandle;

	CAnimationTagDefLibrary();
	~CAnimationTagDefLibrary();

	void Insert(uint32 nCrc, CTagDefinition* pDef);

public:
	THandle LoadResource(const char* name, uint32 nFlags);
	THandle TryOpenResource(const char* name, uint32 nFlags);
	void    CreateResource(THandle& hdlOut, const char* name, uint32 nFlags);
	void    PublishResource(THandle& hdlOut);

protected:
	typedef std::map<uint32, CTagDefinition*> TTagDefList;

private:
	CAnimationTagDefLibrary(const CAnimationTagDefLibrary&);
	CAnimationTagDefLibrary& operator=(const CAnimationTagDefLibrary&);

protected:
	TTagDefList m_tagDefs;
};

class CAnimationControllerDefLibrary
{
public:
	typedef const SControllerDef* THandle;

	CAnimationControllerDefLibrary();
	~CAnimationControllerDefLibrary();

	void Insert(uint32 nCrc32, SControllerDef* pDef);

public:
	THandle LoadResource(const char* name, uint32 nFlags);
	THandle TryOpenResource(const char* name, uint32 nFlags);
	void    CreateResource(THandle& hdlOut, const char* name, uint32 nFlags);
	void    PublishResource(THandle& hdlOut);

protected:
	typedef std::map<uint32, SControllerDef*> TControllerDefList;

protected:
	SControllerDef* LoadControllerDef(const XmlNodeRef& xmlNode, const char* context);

private:
	CAnimationControllerDefLibrary(const CAnimationControllerDefLibrary&);
	CAnimationControllerDefLibrary& operator=(const CAnimationControllerDefLibrary&);

protected:
	TControllerDefList m_controllerDefs;
	CTagDefinition     m_fragmentFlags;
};

class CAnimationDatabaseManager
	: public IAnimationDatabaseManager
	  , public IMannequinEditorManager
	  , protected CAnimationDatabaseLibrary
	  , protected CAnimationTagDefLibrary
	  , protected CAnimationControllerDefLibrary
{
public:
	CAnimationDatabaseManager();
	~CAnimationDatabaseManager();

	virtual int GetTotalDatabases() const
	{
		return m_databases.size();
	}
	virtual const IAnimationDatabase* GetDatabase(int idx) const
	{
		for (TAnimDatabaseList::const_iterator iter = m_databases.begin(); iter != m_databases.end(); ++iter)
		{
			if (idx-- == 0)
			{
				return iter->second;
			}
		}
		return NULL;
	}

	virtual const IAnimationDatabase* Load(const char* filename);
	virtual const SControllerDef*     LoadControllerDef(const char* filename);
	virtual const CTagDefinition*     LoadTagDefs(const char* filename, bool isTags);
	virtual void                      SaveAll(IMannequinWriter* pWriter) const;

	virtual const IAnimationDatabase* FindDatabase(const uint32 crcFilename) const; 

	virtual const SControllerDef*     FindControllerDef(const uint32 crcFilename) const;
	virtual const SControllerDef*     FindControllerDef(const char* filename) const;

	virtual const CTagDefinition*     FindTagDef(const uint32 crcFilename) const;
	virtual const CTagDefinition*     FindTagDef(const char* filename) const;

	virtual IAnimationDatabase*       Create(const char* filename, const char* defFilename);
	virtual CTagDefinition*           CreateTagDefinition(const char* filename);

	virtual void                      ReloadAll();
	virtual void                      UnloadAll();

	// IMannequinEditorManager
	void                    GetAffectedFragmentsString(const CTagDefinition* pQueryTagDef, TagID tagID, char* buffer, int bufferSize);
	void                    ApplyTagDefChanges(const CTagDefinition* pOriginal, CTagDefinition* pModified);
	void                    RenameTag(const CTagDefinition* pOriginal, int32 tagCRC, const char* newName);
	void                    RenameTagGroup(const CTagDefinition* pOriginal, int32 tagGroupCRC, const char* newName);
	void                    GetIncludedTagDefs(const CTagDefinition* pQueriedTagDef, DynArray<CTagDefinition*>& tagDefs) const;

	EModifyFragmentIdResult CreateFragmentID(const CTagDefinition& fragmentIds, const char* szFragmentIdName);
	EModifyFragmentIdResult RenameFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID, const char* szFragmentIdName);
	EModifyFragmentIdResult DeleteFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID);

	bool                    SetFragmentTagDef(const CTagDefinition& fragmentIds, FragmentID fragmentID, const CTagDefinition* pFragTagDefs);

	void                    SetFragmentDef(const SControllerDef& controllerDef, FragmentID fragmentID, const SFragmentDef& fragmentDef);

	void                    SaveDatabasesSnapshot(SSnapshotCollection& snapshotCollection) const;
	void                    LoadDatabasesSnapshot(const SSnapshotCollection& snapshotCollection);

	void                    GetLoadedTagDefs(DynArray<const CTagDefinition*>& tagDefs);
	void                    GetLoadedDatabases(DynArray<const IAnimationDatabase*>& animDatabases) const;
	void                    GetLoadedControllerDefs(DynArray<const SControllerDef*>& controllerDefs) const;

	void                    RevertDatabase(const char* szFilename);
	void                    RevertControllerDef(const char* szFilename);
	void                    RevertTagDef(const char* szFilename);

	bool                    DeleteFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, bool logWarning);
	uint32                  AddFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, const CFragment& fragment);
	void                    SetFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, const CFragment& fragment);

	bool                    IsFileUsedByControllerDef(const SControllerDef& controllerDef, const char* szFilename) const;

	void                    RegisterListener(IMannequinEditorListener* pListener);
	void                    UnregisterListener(IMannequinEditorListener* pListener);

	void                    AddSubADBFragmentFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, FragmentID fragmentID);
	void                    RemoveSubADBFragmentFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, FragmentID fragmentID);
	uint32                  GetSubADBFragmentFilterCount(const IAnimationDatabase* pDatabase, const char* szSubADBFilename) const;
	FragmentID              GetSubADBFragmentFilter(const IAnimationDatabase* pDatabase, const char* szSubADBFilename, uint32 index) const;

	void                    SetSubADBTagFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, TagState tagState);
	TagState                GetSubADBTagFilter(const IAnimationDatabase* pDatabase, const char* szSubADBFilename) const;

	void                    SetBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid, const SFragmentBlend& fragBlend);
	SFragmentBlendUid       AddBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlend& fragBlend);
	void                    DeleteBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid);
	void                    GetFragmentBlends(const IAnimationDatabase* pDatabase, SEditorFragmentBlendID::TEditorFragmentBlendIDArray& outBlendIDs) const;
	void                    GetFragmentBlendVariants(const IAnimationDatabase* pDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo, SEditorFragmentBlendVariant::TEditorFragmentBlendVariantArray& outVariants) const;
	void                    GetFragmentBlend(const IAnimationDatabase* pIDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlendUid& blendUid, SFragmentBlend& outFragmentBlend) const;
	// ~IMannequinEditorManager

private:
	CAnimationDatabaseManager(const CAnimationDatabaseManager&);
	CAnimationDatabaseManager& operator=(const CAnimationDatabaseManager&);

	//--- used for saving
	struct SSaveState
	{
		string m_sFileName;
		bool   m_bSavedByTags;  // Otherwise saved by ID... tags is higher priority;

		SSaveState() : m_sFileName(), m_bSavedByTags(false) {}
	};
	typedef std::vector<SSaveState> TSaveStateList;

	struct SFragmentSaveEntry
	{
		TSaveStateList vSaveStates;
	};
	typedef std::vector<SFragmentSaveEntry> TFragmentSaveList;

	struct SFragmentBlendSaveEntry
	{
		TSaveStateList vSaveStates;
	};
	typedef std::map<CAnimationDatabase::SFragmentBlendID, SFragmentBlendSaveEntry> TFragmentBlendSaveDatabase;

	void       Save(const IAnimationDatabase& iAnimDB, const char* databaseName) const;

	XmlNodeRef SaveDatabase
	(
	  const CAnimationDatabase& animDB,
	  const CAnimationDatabase::SSubADB* subAnimDB,
	  const TFragmentSaveList& vFragSaveList,
	  const TFragmentBlendSaveDatabase& mBlendSaveDatabase,
	  bool flatten = false
	) const;

	void                             ReloadDatabase(IAnimationDatabase* pDatabase);
	void                             RemoveDataFromParent(CAnimationDatabase* parentADB, const CAnimationDatabase::SSubADB* subADB);
	void                             ClearDatabase(CAnimationDatabase* pDatabase);
	void                             Compress(CAnimationDatabase& animDB);

	XmlNodeRef                       SaveControllerDef(const SControllerDef& controllerDef) const;
	void                             ReloadControllerDef(SControllerDef* pControllerDef);
	void                             ReloadTagDefinition(CTagDefinition* pTagDefinition);

	void                             FindRootSubADB(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* pSubADB, FragmentID fragID, TagState tagState, string& outRootADB) const;
	std::vector<CAnimationDatabase*> FindImpactedDatabases(const CAnimationDatabase* pWorkingDatabase, FragmentID fragId, TagState globalTags) const;
	bool                             HasAncestor(const CAnimationDatabase& database, const string& ancestorName) const;
	bool                             HasAncestor(const CAnimationDatabase::SSubADB& database, const string& ancestorName) const;

	void                             PrepareSave(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB, TFragmentSaveList& vFragSaveList, TFragmentBlendSaveDatabase& mBlendSaveDatabase) const;

	bool                             FragmentMatches(uint32 fragCRC, const TagState& tagState, const CAnimationDatabase::SSubADB& subAnimDB, bool& bTagMatchFound) const;
	bool                             TransitionMatches(uint32 fragCRCFrom, uint32 fragCRCTo, const TagState& tagStateFrom, const TagState& tagStateTo, const CTagDefinition& parentTagDefs, const CAnimationDatabase::SSubADB& subAnimDB, bool& bTagMatchFound) const;
	bool                             CanSaveFragmentID(FragmentID fragID, const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB) const;
	bool                             ShouldSaveFragment(FragmentID fragID, const TagState& tagState, const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB, bool& bTagMatchFound) const;
	bool                             ShouldSaveTransition
	(
	  FragmentID fragIDFrom,
	  FragmentID fragIDTo,
	  const TagState& tagStateFrom,
	  const TagState& tagStateTo,
	  const CAnimationDatabase& animDB,
	  const CAnimationDatabase::SSubADB* subAnimDB,
	  bool& bTagMatchFound
	) const;

	void RevertSubADB(const char* szFilename, CAnimationDatabase* animDB, const CAnimationDatabase::SSubADB& subADB);

	void SaveSubADB
	(
	  IMannequinWriter* pWriter,
	  const CAnimationDatabase& animationDatabase,
	  const CAnimationDatabase::SSubADB& subADB,
	  const TFragmentSaveList& vFragSaveList,
	  const TFragmentBlendSaveDatabase& mBlendSaveDatabase
	) const;

	void SaveSubADB(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB& subAnimDB, const TFragmentSaveList& vFragSaveList, const TFragmentBlendSaveDatabase& mBlendSaveDatabase) const;

	void SaveDatabase(IMannequinWriter* pWriter, const IAnimationDatabase* pAnimationDatabase) const;
	void SaveControllerDef(IMannequinWriter* pWriter, const SControllerDef* pControllerDef) const;
	void SaveTagDefinition(IMannequinWriter* pWriter, const CTagDefinition* pTagDefinition) const;

	void NotifyListenersTagDefinitionChanged(const CTagDefinition& tagDef);

	void ReconcileSubDatabases(const CAnimationDatabase* pSourceDatabase);
	void ReconcileSubDatabase(const CAnimationDatabase* pSourceDatabase, CAnimationDatabase* pTargetSubDatabase);

private:

	static CAnimationDatabaseManager* s_Instance;

	typedef CListenerSet<IMannequinEditorListener*> TEditorListenerSet;
	TEditorListenerSet m_editorListenerSet;
};

#endif //__ANIMATION_DATABASE_MANAGER_H__
