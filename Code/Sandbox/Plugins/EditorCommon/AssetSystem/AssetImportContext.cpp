// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetImportContext.h"
#include "Loader/AssetLoaderHelpers.h"
#include "FilePathUtil.h"

#include "Controls/QuestionDialog.h"
#include "ThreadingUtils.h"
#include "QtUtil.h"

#include <CryString/CryPath.h>

namespace Private_AssetImportContext
{

bool ConfirmWrite(const string& filePath, bool& all)
{
	CQuestionDialog qd;
	const QString what = QObject::tr("Do you want to overwrite file %1?").arg(QtUtil::ToQString(filePath));
	const auto buttons =
		QDialogButtonBox::StandardButton::Yes |
		QDialogButtonBox::StandardButton::YesToAll |
		QDialogButtonBox::StandardButton::No |
		QDialogButtonBox::StandardButton::NoToAll;
	qd.SetupQuestion(QObject::tr("Overwrite file?"), what, buttons);
	auto r = qd.Execute();
	switch (r)
	{
	case QDialogButtonBox::YesToAll:
		all = true;
		return true;
	case QDialogButtonBox::NoToAll:
		all = true;
		return false;
	case QDialogButtonBox::Yes:
		all = false;
		return true;
	case QDialogButtonBox::No:
	default:
		all = false;
		return false;
	};
}

} // namespace Private_AssetImportContext

CAssetImportContext::~CAssetImportContext()
{
}

void CAssetImportContext::OverrideAssetName(const string& filename)
{
	m_assetName = !PathUtil::GetExt(filename) ? filename : PathUtil::GetFileName(filename);
}

string CAssetImportContext::GetAssetName() const
{
	return !m_assetName.empty() ? m_assetName : PathUtil::GetFileName(m_inputFilePath);
}

string CAssetImportContext::GetOutputFilePath(const char* szExt) const
{
	return PathUtil::Make(m_outputDirectoryPath, GetAssetName(), szExt);
}

string CAssetImportContext::GetOutputSourceFilePath() const
{
	return PathUtil::Make(m_outputDirectoryPath, PathUtil::GetFile(m_inputFilePath));
}

const std::vector<CAsset*>& CAssetImportContext::GetImportedAssets() const
{
	return m_importedAssets;
}

void CAssetImportContext::AddImportedAsset(CAsset* pAsset)
{
	m_importedAssets.push_back(pAsset);
}

void CAssetImportContext::ClearImportedAssets()
{
	m_importedAssets.clear();
}

bool CAssetImportContext::HasSharedData(const string& key) const
{
	return !GetSharedData(key).isNull();
}

bool CAssetImportContext::HasSharedPointer(const string& key) const
{
	return GetSharedPointer(key) != nullptr;
}

void CAssetImportContext::SetSharedData(const string& key, const QVariant& value)
{
	m_sharedData[key] = value;
}

void CAssetImportContext::SetSharedPointer(const string& key, const std::shared_ptr<void>& value)
{
	m_sharedPointers[key] = value;
}

QVariant CAssetImportContext::GetSharedData(const string& key) const
{
	auto it = m_sharedData.find(key);
	return it != m_sharedData.end() ? it->second : QVariant();
}

void* CAssetImportContext::GetSharedPointer(const string& key) const
{
	auto it = m_sharedPointers.find(key);
	return it != m_sharedPointers.end() ? it->second.get() : nullptr;
}

void CAssetImportContext::ClearSharedDataAndPointers()
{
	m_sharedData.clear();
	m_sharedPointers.clear();
}

CAsset* CAssetImportContext::LoadAsset(const string& filePath)
{
	return AssetLoader::CAssetFactory::LoadAssetFromXmlFile(filePath);
}

CEditableAsset CAssetImportContext::CreateEditableAsset(CAsset& asset)
{
	return CEditableAsset(asset);
}

bool CAssetImportContext::CanWrite(const string& filePath)
{
	const bool bFileExists = PathUtil::FileExists(filePath);
	bool bWrite = false;
	if (m_existingFileOperation == EExistingFileOperation::Overwrite || !bFileExists)
	{
		bWrite = true;
	}
	else if (m_existingFileOperation == EExistingFileOperation::Skip)
	{
		bWrite = false;
	}
	else
	{
		bool bAll = false;
		bWrite = ThreadingUtils::PostOnMainThread(Private_AssetImportContext::ConfirmWrite, PathUtil::ToUnixPath(filePath), std::ref(bAll)).get();
		if (bAll)
		{
			m_existingFileOperation = bWrite ? EExistingFileOperation::Overwrite : EExistingFileOperation::Skip;
		}
	}

	if (bFileExists)
	{
		gEnv->pLog->Log("%s: %s file '%s'.\n", __FUNCTION__, (bWrite ? "Overwriting" : "Skipping"), filePath.c_str());
	}

	return bWrite;
}
