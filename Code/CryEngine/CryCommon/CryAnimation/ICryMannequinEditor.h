// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __I_CRY_MANNEQUIN_EDITOR_H__
#define __I_CRY_MANNEQUIN_EDITOR_H__

#include "ICryMannequin.h"

enum EFileEntryType { eFET_Database, eFET_ControllerDef, eFET_TagDef };

struct IMannequinWriter
{
	virtual ~IMannequinWriter() {}
	virtual void SaveFile(const char* szFilename, XmlNodeRef xmlNode, EFileEntryType fileEntryType) = 0;
	virtual void WriteModifiedFiles() = 0;
};

struct IMannequinEditorListener
{
	virtual ~IMannequinEditorListener() {}
	virtual void OnMannequinTagDefInvalidated(const CTagDefinition& tagDefinition) = 0;
};

struct SAnimDBSnapshot
{
	IAnimationDatabase* pDatabase;
	XmlNodeRef          xmlData;
};

struct SAnimControllerDefSnapshot
{
	SControllerDef* pControllerDef;
	XmlNodeRef      xmlData;
};

typedef DynArray<SAnimDBSnapshot>            TAnimDBSnapshotCollection;
typedef DynArray<SAnimControllerDefSnapshot> TAnimControllerDefSnapshotCollection;

struct SSnapshotCollection
{
	TAnimDBSnapshotCollection            m_databases;
	TAnimControllerDefSnapshotCollection m_controllerDefs;
};

enum EModifyFragmentIdResult
{
	eMFIR_Success,
	eMFIR_DuplicateName,
	eMFIR_InvalidNameIdentifier,
	eMFIR_UnknownInputTagDefinition,
	eMFIR_InvalidFragmentId,
};

struct SEditorFragmentBlendID
{
	FragmentID fragFrom;
	FragmentID fragTo;
	typedef DynArray<SEditorFragmentBlendID> TEditorFragmentBlendIDArray;
};

struct SEditorFragmentBlendVariant
{
	SFragTagState tagsFrom;
	SFragTagState tagsTo;
	typedef DynArray<SEditorFragmentBlendVariant> TEditorFragmentBlendVariantArray;
};

struct IMannequinEditorManager
{
	virtual ~IMannequinEditorManager() {}

	virtual EModifyFragmentIdResult CreateFragmentID(const CTagDefinition& fragmentIds, const char* szFragmentIdName) = 0;
	virtual EModifyFragmentIdResult RenameFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID, const char* szFragmentIdName) = 0;
	virtual EModifyFragmentIdResult DeleteFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID) = 0;

	virtual bool                    SetFragmentTagDef(const CTagDefinition& fragmentIds, FragmentID fragmentID, const CTagDefinition* pFragTagDefs) = 0;
	virtual void                    SetFragmentDef(const SControllerDef& controllerDef, FragmentID fragmentID, const SFragmentDef& fragmentDef) = 0;

	virtual void                    SaveDatabasesSnapshot(SSnapshotCollection& snapshotCollection) const = 0;
	virtual void                    LoadDatabasesSnapshot(const SSnapshotCollection& snapshotCollection) = 0;

	virtual void                    GetLoadedTagDefs(DynArray<const CTagDefinition*>& tagDefs) = 0;
	virtual void                    GetLoadedDatabases(DynArray<const IAnimationDatabase*>& animDatabases) const = 0;
	virtual void                    GetLoadedControllerDefs(DynArray<const SControllerDef*>& controllerDefs) const = 0;

	virtual void                    SaveAll(IMannequinWriter* pWriter) const = 0;
	virtual void                    SaveControllerDef(IMannequinWriter* pWriter, const SControllerDef* controllerDef) const = 0;
	virtual void                    SaveDatabase(IMannequinWriter* pWriter, const IAnimationDatabase* database) const = 0;
	virtual void                    SaveTagDefinition(IMannequinWriter* pWriter, const CTagDefinition* tagDef) const = 0;
	virtual void                    RevertDatabase(const char* szFilename) = 0;
	virtual void                    RevertControllerDef(const char* szFilename) = 0;
	virtual void                    RevertTagDef(const char* szFilename) = 0;

	virtual bool                    DeleteFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, bool logWarning = true) = 0;
	virtual uint32                  AddFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, const CFragment& fragment) = 0;
	virtual void                    SetFragmentEntry(IAnimationDatabase* pDatabase, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, const CFragment& fragment) = 0;

	virtual bool                    IsFileUsedByControllerDef(const SControllerDef& controllerDef, const char* szFilename) const = 0;

	virtual void                    GetAffectedFragmentsString(const CTagDefinition* pQueryTagDef, TagID tagID, char* buffer, int bufferSize) = 0;
	virtual void                    ApplyTagDefChanges(const CTagDefinition* pOriginal, CTagDefinition* pModified) = 0;
	virtual void                    RenameTag(const CTagDefinition* pOriginal, int32 tagCRC, const char* newName) = 0;
	virtual void                    RenameTagGroup(const CTagDefinition* pOriginal, int32 tagGroupCRC, const char* newName) = 0;

	// Returns all tagdefinitions that this tagdefinition includes (recursively).
	// Notes:
	// - Does not include the pQueriedTagDef
	// - Only includes tagdefinitions that are currently loaded
	// - Clears tagDefs list when pQueriedTagDef is nullptr
	virtual void              GetIncludedTagDefs(const CTagDefinition* pQueriedTagDef, DynArray<CTagDefinition*>& tagDefs) const = 0;

	virtual void              RegisterListener(IMannequinEditorListener* pListener) = 0;
	virtual void              UnregisterListener(IMannequinEditorListener* pListener) = 0;

	virtual void              AddSubADBFragmentFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, FragmentID fragmentID) = 0;
	virtual void              RemoveSubADBFragmentFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, FragmentID fragmentID) = 0;
	virtual uint32            GetSubADBFragmentFilterCount(const IAnimationDatabase* pDatabase, const char* szSubADBFilename) const = 0;
	virtual FragmentID        GetSubADBFragmentFilter(const IAnimationDatabase* pDatabase, const char* szSubADBFilename, uint32 index) const = 0;

	virtual void              SetSubADBTagFilter(IAnimationDatabase* pDatabase, const char* szSubADBFilename, TagState tagState) = 0;
	virtual TagState          GetSubADBTagFilter(const IAnimationDatabase* pDatabase, const char* szSubADBFilename) const = 0;

	virtual void              SetBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid, const SFragmentBlend& fragBlend) = 0;
	virtual SFragmentBlendUid AddBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlend& fragBlend) = 0;
	virtual void              DeleteBlend(IAnimationDatabase* pDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid) = 0;
	virtual void              GetFragmentBlends(const IAnimationDatabase* pDatabase, SEditorFragmentBlendID::TEditorFragmentBlendIDArray& outBlendIDs) const = 0;
	virtual void              GetFragmentBlendVariants(const IAnimationDatabase* pDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo, SEditorFragmentBlendVariant::TEditorFragmentBlendVariantArray& outVariants) const = 0;
	virtual void              GetFragmentBlend(const IAnimationDatabase* pIDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlendUid& blendUid, SFragmentBlend& outFragmentBlend) const = 0;
};

struct IMannequinGameListener
{
	virtual ~IMannequinGameListener(){}

	virtual void OnSpawnParticleEmitter(struct IParticleEmitter* pEffect, IActionController& actionController) = 0;
};

#endif
