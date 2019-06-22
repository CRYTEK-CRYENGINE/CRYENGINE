// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <CryCore/smartptr.h>

#include <CryExtension/CryGUID.h>
#include <CrySandbox/CrySignal.h>

#include <QIcon>

class CAssetType;
class CAssetEditor;
class CEditableAsset;
struct IEditableAsset;
struct INewAsset;

//! Values that can be passed to CAsset::signalChanged and CAssetManager::signalAssetChanged
enum AssetChangeFlags
{
	//!
	eAssetChangeFlags_ReadOnly		= 0x00000001,
	eAssetChangeFlags_Modified		= 0x00000002,
	eAssetChangeFlags_Open			= 0x00000004,
	eAssetChangeFlags_Files			= 0x00000008,

	eAssetChangeFlags_All			= 0xFFFFFFFF
};

// \sa CAsset::GetDependencies
struct SAssetDependencyInfo
{
	SAssetDependencyInfo() = default;
	SAssetDependencyInfo(const string& path, int32 usageCount) : path(path), usageCount(usageCount)
	{
	}

	string path;        //!< The path is relative to the assets root directory.
	int32  usageCount;  //!< The instance count for the path or 0 if such information is not available.
};

// IAssetEditingSession Provides methods to manage state of unsaved changes of asset on editing.
// Editing session is owned by asset.
// \sa CAsset::GetEditingSession
// \sa CAssetEditor::CreateEditingSession
struct EDITOR_COMMON_API IAssetEditingSession
{
	virtual ~IAssetEditingSession() {}

	// Returns name of the editor that created the session object.
	virtual const char* GetEditorName() const = 0;

	// The implementation should save asset data files and update the asset by the IEditableAsset interface.
	//! \sa CAssetEditor::OnSaveAsset
	virtual bool OnSaveAsset(IEditableAsset& asset) = 0;

	//! The implementation must unconditionally give up all asset changes, if any.
	virtual void DiscardChanges(IEditableAsset& asset) = 0;

	// The implementation should save asset data files and update the asset by the INewAsset interface.
	//! \sa INewAsset
	//! \sa CAssetType::OnCopy
	virtual bool OnCopyAsset(INewAsset& asset) = 0;
};

//! Assets are the "unit of work" of CRYENGINE Asset Sytem and represent one or several files on disk, as well as a *.cryasset metadata file to carry extra information relevant to this asset.
class EDITOR_COMMON_API CAsset : public _i_multithread_reference_target_t
{
	//TODO : remove this
	friend class CAssetManager;
	friend class CAssetModel;
	friend class CEditableAsset;
private:
	class CThumbnailFinalizer;
	class CThumbnailLoader;

public:
	CAsset(const char* type, const CryGUID& guid, const char* name);
	virtual ~CAsset();

	const CAssetType* GetType() const { return m_type; }
	//! Returns the name of the asset. The name is only guaranteed to be unique per asset type.
	const string& GetName() const { return m_name; }
	const CryGUID& GetGUID() const { return m_guid; }
	uint64 GetLastModifiedTime() const { return m_lastModifiedTime; }

	//! Return count of the asset data files.
	const size_t GetFilesCount() const { return m_files.size(); }

	//! Returns path to the i-st data file. The path is relative to the assets root directory.
	const string& GetFile(size_t i) const { return m_files[i]; }

	//! Returns paths to the data files. The paths are relative to the assets root directory.
	const std::vector<string>& GetFiles() const { return m_files; }

	//! Computes the folder of the asset. This relies on the assumption that all the files for this asset are in the same place. The path will have a trailing /
	const string& GetFolder() const;

	//! Returns true if this asset was imported from a source file
	bool HasSourceFile() const;

	//! Returns path relative to the assets root directory. Can be empty.
	//! The asset and the source file must be in the same folder. Therefore all the assets that use the same source file must be in one directory.
	const string& GetSourceFile() const { return m_sourceFile; }

	//! Returns paths to the work files (DCC files). The paths are relative to the assets root directory.
	const std::vector<string>& GetWorkFiles() const { return m_workFiles; }

	//! Opens the source file in a separate process
	void OpenSourceFile() const;

	//! Returns path to the .cryasset file relative to the assets root directory.
	const string& GetMetadataFile() const { return m_metadataFile; }

	const std::vector<std::pair<string, string>>& GetDetails() const { return m_details; }

	const string GetDetailValue(const string& detailName) const;

	//! Returns asset dependencies as a collection of SAssetDependencyInfo
	// \sa SAssetDependencyInfo
	const std::vector<SAssetDependencyInfo>& GetDependencies() const { return m_dependencies; }

	//! Tests if the specified asset is used by another asset.
	//! \param szAnotherAssetPath paths to the main data file of another asset. The path must be relative to the assets root directory, as it returns by CAsset::GetFile.
	//! \return Returns a pair of {isUsedBy, usageCount}.
	//! \sa CAsset::GetFile
	std::pair<bool, int> IsAssetUsedBy(const char* szAnotherAssetPath) const;

	//! Tests if the specified asset uses another asset.
	//! \param szAnotherAssetPath paths to the main data file of another asset. The path  must be relative to the assets root directory.
	//! \return Returns a pair of {true/false, usageCount}.
	//! \sa CAsset::GetFile
	std::pair<bool, int> DoesAssetUse(const char* szAnotherAssetPath) const;


	//Importing methods
	void Reimport();

	//Editing methods

	bool CanBeEdited() const;
	bool IsBeingEdited() const { return m_flags.open != 0; }
	bool IsModified() const { return m_flags.modified != 0; }
	void SetModified(bool bModified);

	//! Returns true if the editor can write to asset files.
	bool IsWritable(bool includeSourceFile = true) const;

	//! Asset that is immutable cannot be changed. Engine assets are example of immutable assets.  
	bool IsImmutable() const;

	//! Opens asset for editing. If \pEditor is nullptr, a new editor is created based on the asset type.
	//! If pEditor is specified, it will open the asset in that editor
	//! If the asset is already opened in an editor, that editor will be shown regardless of the parameter.
	//! \sa CAssetType::Edit
	void Edit(CAssetEditor* pEditor = nullptr);

	//! Returns the current editor that has this asset opened
	CAssetEditor* GetCurrentEditor() const { return m_pEditor; }

	//! returns IAssetEditingSession interface to the current editing state or nullptr if the asset is not editing or the active editor does not support sessions.
	//! \sa IsBeingEdited()
	IAssetEditingSession* GetEditingSession() const { return m_pEditingSession.get(); }

	//! Saves asset. 
	// \sa CAssetEditor::Save
	// \sa IAssetEditingSession::OnSaveAsset
	void Save();

	//! Discards all unsaved changes, if any, and reloads the asset.
	void Reload();

	//! Returns true if the thumbnail is loaded, GetThumbnail will return a valid thumbnail
	bool IsThumbnailLoaded() const;

	//! Returns the thumbnail for this asset. May be the asset icon if the thumbnail has not been loaded
	QIcon GetThumbnail() const;

	//! Returns the thumbnail path for this asset next to the metafile.
	string GetThumbnailPath() const;

	//! Synchronously generates the thumbnail next to the metadata file, 
	//! only necessary for internal usage in the AssetSystem
	void GenerateThumbnail() const;

	//! Invalidates thumbnail, to allow re-loading the thumbnail from disk.
	void InvalidateThumbnail();

	//! Confirms that this asset has valid, well formed metadata
	bool Validate() const;

	void SetThumbnail(const QIcon& thumbnail);

	//! Attach to this signal to get notified if the asset has any change. If you need to observe all assets, there is an equivalent signal on the Asset Manager.
	//! Note that emitting this signal is called through NotifyChanged(), which ensures all notifications are emitted properly
	//! \sa AssetChangeFlags
	CCrySignal<void(CAsset&, int /*changeFlags*/)> signalChanged;

	//! Signal sent before this asset is removed from asset manager
	CCrySignal<void(CAsset*)> signalBeforeRemoved;
	//! Signal sent after this asset is removed from asset manager
	CCrySignal<void(CAsset*)> signalAfterRemoved;

private:
	//Filter string is how we handle "smart searching" in the content browser. All data aggregated into this string is going to be searchable in the main search bar. 
	const QString& GetFilterString(bool forceCompute = false) const;

	bool WriteToFile();
	void SetName(const char* szName);
	void SetLastModifiedTime(uint64 t);
	void SetMetadataFile(const char* szFilepath);
	void SetSourceFile(const char* szFilepath);
	void AddFile(const string& filePath);
	void SetFiles(const std::vector<string>& filenames);
	void AddWorkFile(const string& filePath);
	void SetWorkFiles(const std::vector<string>& filenames);
	void SetDetail(const string& name, const string& value);
	void SetDependencies(const std::vector<SAssetDependencyInfo>& dependencies);

	//! Enables the asset to adjust its internal state once opened by an editor. Called by the AssetEditor through CEditableAsset, when it has opened this asset. 
	void OnOpenedInEditor(CAssetEditor* pEditor);

	void NotifyChanged(int changeFlags = eAssetChangeFlags_All);
	
private:
	CAsset(const CAsset&) = delete;
	CAsset& operator=(const CAsset&) = delete;

	//Serialized metadata
	string              m_name;
	const CAssetType*	m_type;
	const CryGUID       m_guid;
	uint64              m_lastModifiedTime = 0;
	std::vector<string> m_files;
	std::vector<string> m_workFiles;
	string				m_sourceFile;
	string				m_metadataFile; //!< Unix path.
	std::vector<std::pair<string, string>> m_details;
	std::vector<SAssetDependencyInfo> m_dependencies;
	mutable QIcon		m_thumbnail;
	
	//TODO: adding type specific metadata container

	//Flags
	union
	{
		uint8 bitField;
		struct
		{
			uint8 open : 1;
			uint8 modified : 1;
			uint8 immutable : 1;
		};
	} m_flags;

	//Editor-only metadata

	//Editor instance that is currently opening this asset
	CAssetEditor*			m_pEditor;

	std::unique_ptr<IAssetEditingSession> m_pEditingSession;

	//Optimizations: caching some data to avoid computing it too often
	mutable QString		m_filterString;
	mutable string		m_folder;
};

//Use this only if you need to access an asset in an asynchronous task. You will prevent crashes in case the asset gets deleted. 
//In any other case it's fairly safe to use regular pointer.
typedef _smart_ptr<CAsset> CAssetPtr;
