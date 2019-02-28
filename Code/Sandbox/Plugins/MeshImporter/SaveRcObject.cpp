// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SaveRcObject.h"
#include "CallRcTask.h"
#include "RcCaller.h"
#include "DialogCommon.h"
#include "ImporterUtil.h"

#include <FilePathUtil.h>
#include <ThreadingUtils.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTemporaryDir>

void LogPrintf(const char* szFormat, ...);

void CaptureRcObjectSaveState(
	const std::shared_ptr<QTemporaryDir>& pTempDir,
	const FbxMetaData::SMetaData& metaData,
	SRcObjectSaveState& outSaveState)
{	
	outSaveState.sourceFilePath = metaData.sourceFilename;
	outSaveState.ext = metaData.outputFileExt;
	outSaveState.pTempDir = pTempDir; // Keep this directory alive.
	outSaveState.b32BitVertexPositions = metaData.bVertexPositionFormatF32;

	// When saving an asset, the CGF references the source by filename only, since both the CGF and
	// the FBX file are supposed to be in the same directory.
	FbxMetaData::SMetaData metaDataCopy(metaData);
	metaDataCopy.sourceFilename = PathUtil::GetFile(metaDataCopy.sourceFilename);

	outSaveState.metaData = metaDataCopy.ToJson();
}

void SaveRcObjectAsync(const SRcObjectSaveState& saveState, const string& targetFilePath, const FinalizeSave& finalize)
{	
	const string filename = PathUtil::GetFileName(targetFilePath);
	auto pTempFile = WriteTemporaryFile(saveState.pTempDir->path(), saveState.metaData, QtUtil::ToQString(filename));
	if (!pTempFile)
	{
		finalize(false, targetFilePath);
		return;
	}

	ThreadingUtils::Async([finalize, targetFilePath, pTempFile = std::move(pTempFile), saveState]()
	{
		const string filePath = QtUtil::ToString(pTempFile->fileName());
		CRcCaller rcCaller;

		const string options = string().Format("%s /overwritesourcefile=\"%s\" %s",
			CRcCaller::OptionOverwriteExtension("fbx"),
			saveState.sourceFilePath.c_str(),
			CRcCaller::OptionVertexPositionFormat(saveState.b32BitVertexPositions));

		rcCaller.SetAdditionalOptions(options);
		const bool bSuccess = rcCaller.Call(filePath);
		if (!bSuccess)
		{
			LogPrintf("%s: Compiling file %s failed.\n", __FUNCTION__, filePath.c_str());
		}
		else
		{
			ThreadingUtils::PostOnMainThread([filePath, targetFilePath, saveState]()
			{
				const string cgfFile = PathUtil::ReplaceExtension(filePath, saveState.ext);
				MeshImporter::RenameAllowOverwrite(cgfFile, targetFilePath);
				MeshImporter::RenameAllowOverwrite(cgfFile + ".cryasset", targetFilePath + ".cryasset");
			});
		}
		if (finalize)
		{
			ThreadingUtils::PostOnMainThread(finalize, bSuccess, targetFilePath);
		}
	});
}

