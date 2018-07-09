// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "EditableAsset.h"

#include <CryString/CryString.h>
#include <CryCore/Containers/VectorMap.h>

#include <QVariant>

class CAssetType;

//! Encapsulates state of a single import request (i.e. a single file).
//! This structure is passed to an asset type when importing.
class EDITOR_COMMON_API CAssetImportContext
{
private:
	enum class EExistingFileOperation
	{
		NotSet,
		Overwrite,
		Skip
	};

public:
	CAssetImportContext()
		: m_inputFilePath()
		, m_outputDirectoryPath()
		, m_bImportAll(false)
	{
	}

	CAssetImportContext(const CAssetImportContext&) = delete;
	CAssetImportContext& operator=(const CAssetImportContext&) = delete;

	~CAssetImportContext();

	string GetInputFilePath() const
	{
		return m_inputFilePath;
	}

	void SetInputFilePath(const string& path)
	{
		m_inputFilePath = path;
	}

	//! Returns output directory path relative to asset directory.
	string GetOutputDirectoryPath() const
	{
		return m_outputDirectoryPath;
	}

	//! Sets output directory path. The pash should be relative to the assets root directory.
	void SetOutputDirectoryPath(const string& path)
	{
		m_outputDirectoryPath = path;
	}

	//! Replaces the default asset file name with the specified one. 
	//! The default asset name is the name of the source file. \sa CAssetImportContext::SetInputFilePath  
	//! \param filename The asset basename without path and extension.
	void OverrideAssetName(const string& filename);

	// Returns output asset name without path and extension.
	string GetAssetName() const;

	//! Returns path to the asset file in the output directory. The path is relative to asset root directory.
	//! \param szExt The asset file extension.
	string GetOutputFilePath(const char* szExt) const;

	//! Returns path to the asset source file in the output directory. The path is relative to asset root directory.
	string GetOutputSourceFilePath() const;

	void SetImportAll(bool bImportAll)
	{
		m_bImportAll = bImportAll;
	}

	bool IsImportAll() const
	{
		return m_bImportAll;
	}

	void SetOverwriteAll(bool bOverwriteAll)
	{
		m_existingFileOperation = EExistingFileOperation::Overwrite;
	}

	bool IsOverwriteAll() const
	{
		return m_existingFileOperation == EExistingFileOperation::Overwrite;
	}

	void SetAssetTypes(const std::vector<CAssetType*>& assetTypes)
	{
		m_assetTypes = assetTypes;
	}

	const std::vector<CAssetType*> GetAssetTypes() const
	{
		return m_assetTypes;
	}

	const std::vector<CAsset*>& GetImportedAssets() const;

	void AddImportedAsset(CAsset* pAsset);
	void ClearImportedAssets();

	// Shared data stored in the context is available to all importers.
	// There are two types of data: variants and pointers.
	// Prefer variants over pointers.
	// Pointers can be used for types that cannot be converted to QVariant.
	// The life-time of the pointee is managed by the context.
	// By using shared_ptr, we make sure that the correct deletion function is called.

	bool HasSharedData(const string& key) const; 
	bool HasSharedPointer(const string& key) const;
	void SetSharedData(const string& key, const QVariant& value);
	void SetSharedPointer(const string& key, const std::shared_ptr<void>& value);
	QVariant GetSharedData(const string& key) const;
	void* GetSharedPointer(const string& key) const;
	void ClearSharedDataAndPointers();

	CAsset* LoadAsset(const string& filePath);

	CEditableAsset CreateEditableAsset(CAsset& asset);

	//! CanWrite returns whether a file path may be written (or overwritten) for importing.
	bool CanWrite(const string& filePath);

private:
	std::vector<CAssetType*> m_assetTypes;
	std::vector<CAsset*> m_importedAssets;

	VectorMap<string, QVariant> m_sharedData;
	VectorMap<string, std::shared_ptr<void>> m_sharedPointers;

	string m_inputFilePath;
	string m_outputDirectoryPath;
	string m_assetName;
	EExistingFileOperation m_existingFileOperation = EExistingFileOperation::NotSet;
	bool m_bImportAll;
};

