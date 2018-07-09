// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <future>
#include <memory>

class CAsset;
class QViewport;

namespace FbxMetaData
{
struct SMetaData;
} // namespace FbxMetaData

class QTemporaryDir;
class QTemporaryFile;

// ==================================================
// Path utilities.
// ==================================================

bool IsPathAbsolute(const string& path);

//! Returns true if \p filePath is contained in the asset directory.
bool IsAssetPath(const string& filePath);

QString AppendPath(const QString& lhp, const QString& rhp);

QString GetAbsoluteGameFolderPath();

// Replaces characters that are neither alphabetical nor digits.
// Use this function to produce filenames from strings that might contain special characters.
string MakeAlphaNum(const string& str);

// ==================================================
// File utilities.
// ==================================================

bool WriteToFile(const QString& filePath, const QString& content);
bool FileExists(const string& path);

bool CopyNoOverwrite(const string& from, const string& to);

//! CopyAllowOverwrite copies a file from \p from (source) to \p to (target) and allows overwriting of the target.
//! No copy is performed if the source file and target file are identical (this is still considered a successful copy).
//! \param from Absolute path of source file.
//! \param to Absolute path of target file.
//! \return True if copy operation succeeded; False, otherwise.
//!   The copy operation fails (returns false), if...
//!     - ... the source does not exist.
//!     - ... the directory containing the target does not exist.
bool CopyAllowOverwrite(const string& from, const string& to);

bool IsFileWritable(const string& path);

// ==================================================
// Temporary directories and files utilities.
// ==================================================

std::unique_ptr<QTemporaryFile> WriteTemporaryFile(const string& dirPath, const string& content, const string& templateName);
std::unique_ptr<QTemporaryFile> WriteTemporaryFile(const QString& dirPath, const string& content, QString templateName);

std::unique_ptr<QTemporaryDir> CreateTemporaryDirectory();

// ==================================================
// Assets.
// ==================================================

bool IsAssetMetaDataFile(string filePath);

//! Returns any file of asset that matches one of the file extensions in \p exts.
string GetFileFromAsset(CAsset& asset, const std::vector<string>& exts);

// ==================================================
// File importing.
// ==================================================

//! Copies a file to the asset folder.
class CFileImporter
{
public:
	typedef std::function<bool(const string&)> MayOverwriteFunc;

public:
	bool Import(const string& inputFilePath, const string& outputFilePath);
	bool Import(const string& inputFilePath);

	void SetMayOverwriteFunc(const MayOverwriteFunc& mayOverwrite);

	string GetOutputFilePath() const; //!< Returns path that is relative to asset directory.

	string GetError() const;

private:
	static string ShowDialog(const string& inputFilename);

private:
	MayOverwriteFunc m_mayOverwrite;
	string m_outputFilePath;
	string m_error;
};

//! Copies source file to directory asynchronously. Skips copy if an identical file is already
//! present in the target directory.
//! This method is typically used by implementations of SaveAs.
//! \param from Unix path, absolute.
//! \param dir Unix path, relative to asset directory. Does not contain trailing slash.
//! \return Pair (true, string()) on success; (false, error message) on failure.
//! \sa CBaseDialog::SaveAs CFileImporter
std::future<std::pair<bool, string>> CopySourceFileToDirectoryAsync(const string& from, const string& dir, bool bShowNotification = true);

// ==================================================
// Rendering.
// ==================================================

void ShowLabelNextToCursor(const QViewport* pViewport, const string& label);

const float kSelectionCooldownPerSec = 1.1f;

// ==================================================
// Qt <-> STL
// ==================================================

QStringList ToStringList(const std::vector<string>& strs);

