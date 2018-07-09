// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QString>
#include <QTemporaryDir>

#include <functional>

namespace FbxMetaData
{

struct SMetaData;

} // namespace FbMetaData

// Here, saving a RC object means creating a data file, such as CGF or SKIN, by running the RC on a
// (temporary) meta-data file.
// Saving a RC object is a two-phase process. Firstly, we capture all necessary state that is going
// to be passed to the RC. Secondly, this state is passed to an actual call of the RC, which will
// run asynchronously.
// The general idea is that all relevant state is copied before tasks are launched asynchronously,
// so that consistency is guaranteed.

typedef std::function<void(bool bSuccess, string filePath)> FinalizeSave;

struct SRcObjectSaveState
{
	string metaData;
	string sourceFilePath;
	string ext;
	std::shared_ptr<QTemporaryDir> pTempDir; // Keep this alive.
	bool b32BitVertexPositions;
};

//! Captures all state required to create an object.
//! \sa SaveRcObjectAsync
void CaptureRcObjectSaveState(
	const std::shared_ptr<QTemporaryDir>& pTempDir,
	const FbxMetaData::SMetaData& metaData,
	SRcObjectSaveState& outSaveState);

//! Saves a RC object. This means a data-file, such as CGF or SKIN, will be created by calling the RC.
//! \param saveState State of the object that will be used to create the object.
//! \param finalize Completion callback.
//! \sa GetRcObjectSaveState.
void SaveRcObjectAsync(const SRcObjectSaveState& saveState, const string& targetFilePath, const FinalizeSave& finalize = FinalizeSave());
