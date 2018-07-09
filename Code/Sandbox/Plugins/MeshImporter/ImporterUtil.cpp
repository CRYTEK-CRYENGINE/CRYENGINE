// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImporterUtil.h"

// EditorCommon.
#include <Controls/QuestionDialog.h>
#include <FileDialogs/EngineFileDialog.h>
#include <FilePathUtil.h>
#include <QViewport.h>
#include <Notifications/NotificationCenter.h>
#include <ThreadingUtils.h>

// EditorQt.
#include <AssetSystem/Asset.h>
#include "Util/FileUtil.h"

#include <CrySystem/IProjectManager.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include <QDir>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTemporaryFile>

void LogPrintf(const char* szFormat, ...);

QString GetAbsoluteGameFolderPath()
{
	return QtUtil::ToQString(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute());
}

static QString GetTempAssetFolder()
{
	char path[ICryPak::g_nMaxPath] = {};
	static const QString folder = QtUtil::ToQString(gEnv->pCryPak->AdjustFileName("%USER%/MeshImporter", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH));
	QDir().mkpath(folder);
	return folder;
}

QString AppendPath(const QString& lhp, const QString& rhp)
{
	return QDir::cleanPath(lhp + QDir::separator() + rhp);
}

string MakeAlphaNum(const string& str)
{
	string res;
	for (int i = 0; i < str.length(); ++i)
	{
		res += isalnum(str[i]) ? str[i] : ' ';
	}

	res.Trim();

	return res;
}


std::unique_ptr<QTemporaryFile> WriteTemporaryFile(const string& dirPath, const string& content, const string& templateName)
{
	return WriteTemporaryFile(QtUtil::ToQString(dirPath), content, QtUtil::ToQString(templateName));
}

std::unique_ptr<QTemporaryFile> WriteTemporaryFile(const QString& dirPath, const string& content, QString templateName)
{
	if (templateName.isEmpty())
	{
		templateName = "tmp_meshImporter_XXXXXX";
	}

	const QString templatePath = AppendPath(dirPath, templateName);
	std::unique_ptr<QTemporaryFile> pTmpFile(new QTemporaryFile(templatePath));
	if (pTmpFile->open())
	{
		if (pTmpFile->write(content.begin(), content.size()) == content.size())
		{
			pTmpFile->close();
			return std::move(pTmpFile);
		}
	}
	pTmpFile->close();
	LogPrintf("%s: Writing meta data failed.\n", __FUNCTION__);
	return std::unique_ptr<QTemporaryFile>();
}

std::unique_ptr<QTemporaryDir> CreateTemporaryDirectory()
{
	const QString templatePath = AppendPath(GetTempAssetFolder(), "tmp_meshImporter_XXXXXX");
	std::unique_ptr<QTemporaryDir> pTmpDir(new QTemporaryDir(templatePath));
	if (!pTmpDir->isValid())
	{
		LogPrintf("%s: Cannot create temporary directory. Template path is %s\n", __FUNCTION__, templatePath.toLocal8Bit().constData());
	}
	return std::move(pTmpDir);
}

//! Returns file extension of asset meta data. Lower-case and without leading dot.
//! Example: cryasset
static const char* AssetMetaDataExt()
{
	return "cryasset";
}

bool IsAssetMetaDataFile(string filePath)
{
	static string ext = string().Format(".%s", AssetMetaDataExt());
	filePath.MakeLower();
	auto it0 = ext.rbegin();
	auto it1 = filePath.rbegin();
	for (; it0 != ext.rend(); ++it0, ++it1)
	{
		if (it1 == filePath.rend() || *it1 != *it0)
		{
			return false;
		}
	}
	return true;
}

string GetFileFromAsset(CAsset& asset, const std::vector<string>& exts)
{
	const char delim = '*';

	string flatExts;
	flatExts.reserve(5 * exts.size());
	for (auto& ext : exts)
	{
		flatExts += delim;
		flatExts += ext;
	}
	flatExts.MakeLower();

	string needle;
	for (size_t i = 0, N = asset.GetFilesCount(); i < N; ++i)
	{
		needle.Format("%c%s", delim, PathUtil::GetExt(asset.GetFile(i)));
		if (flatExts.find(needle) != string::npos)
		{
			return asset.GetFile(i);
		}
	}

	return string();
}

bool WriteToFile(const QString& filePath, const QString& content)
{
	QFile file(filePath);
	if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
	{
		return false;
	}
	QTextStream out(&file);
	out << content;
	file.close();
	return true;
}

bool FileExists(const string& path)
{
	FILE* const pFile = fopen(path.c_str(), "rb");
	if (pFile)
	{
		fclose(pFile);
		return true;
	}
	return false;
}


static bool IsInDirectory(const string& query, const string& dir)
{
	string iquery = query;
	string idir = dir;
	return !strncmp(PathUtil::ToUnixPath(iquery.MakeLower()), PathUtil::ToUnixPath(idir.MakeLower()), idir.size());
}

bool IsPathAbsolute(const string& path)
{
	return QFileInfo(QtUtil::ToQString(path)).isAbsolute();
}

bool IsAssetPath(const string& filePath)
{
	return !IsPathAbsolute(filePath) || IsInDirectory(filePath, PathUtil::GetGameProjectAssetsPath());
}

bool CopyNoOverwrite(const QString& from, const QString& to, bool bSilent)
{
	if (QFileInfo(to).exists())
	{
		if (CFileUtil::CompareFiles(QtUtil::ToString(to).c_str(), QtUtil::ToString(from).c_str()))
		{
			LogPrintf("File %s already exists and is identical to %s. Skip copying.",
			          QtUtil::ToString(to).c_str(),
			          QtUtil::ToString(from).c_str());
			return true;
		}
		else
		{
			if (!bSilent)
			{
				const char* what =
					"Cannot copy source file %1 to %2, because file already exists.\n"
					"Either choose another location, or create a CGF from %2 instead.";

				CQuestionDialog::SWarning(QObject::tr("Source file already exists"), QObject::tr(what).arg(from).arg(to));
			}
			return false;
		}
	}
	else
	{
		return QFile::copy(from, to);
	}
}

bool CopyNoOverwrite(const string& from, const string& to)
{
	return !CopyNoOverwrite(QtUtil::ToQString(from), QtUtil::ToQString(to), true);
}

static bool CopyAllowOverwriteInternal(const QString& from, const QString& to)
{
	if (QFile::rename(from, to))
	{
		return true;
	}
	else
	{
		// Try to overwrite existing file.
		return QFile::remove(to) && QFile::rename(from, to);
	}
}

bool CopyAllowOverwrite(const string& from, const string& to)
{
	if (!FileExists(from))
	{
		return false; // Source file does not exist and copy operation is considered to be unsuccessful.
	}

	if (CFileUtil::CompareFiles(to.c_str(), from.c_str()))
	{
		return true; // Files are identical and copy operation is considered to be successful.
	}

	return CopyAllowOverwriteInternal(QtUtil::ToQString(from), QtUtil::ToQString(to));
}

bool IsFileWritable(const string& path)
{
	QFile f(QtUtil::ToQString(path));
	return (f.permissions() & QFileDevice::WriteOwner) != 0;
}

bool CFileImporter::Import(const string& inputFilePath, const string& outputFilePath)
{
	m_error.clear();

	if (inputFilePath.empty())
	{
		m_error = "Input file path is empty";
		return false;
	}

	m_outputFilePath = PathUtil::ToGamePath(outputFilePath.c_str());
	string absOutputFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), m_outputFilePath);
	if (absOutputFilePath.empty())
	{
		m_error = "Output file path is empty";
		return false;
	}

	QDir().mkpath(QtUtil::ToQString(PathUtil::GetPathWithoutFilename(absOutputFilePath)));

	// Copy source to target location.
	if (!CopyNoOverwrite(QtUtil::ToQString(inputFilePath), QtUtil::ToQString(absOutputFilePath), true))
	{
		if (m_mayOverwrite && m_mayOverwrite(absOutputFilePath))
		{
			QFile::remove(QtUtil::ToQString(absOutputFilePath));
			return CopyNoOverwrite(QtUtil::ToQString(inputFilePath), QtUtil::ToQString(absOutputFilePath), true);
		}
		else
		{
			m_error.Format("File %s already exists and is different from %s",
				PathUtil::ToUnixPath(absOutputFilePath.c_str()),
				PathUtil::ToUnixPath(inputFilePath.c_str()));
			return false;
		}
	}
	return true;
}

bool CFileImporter::Import(const string& inputFilePath)
{
	if (IsAssetPath(inputFilePath))
	{
		// No need to copy.
		m_outputFilePath = PathUtil::ToGamePath(inputFilePath);
		return true;
	}
	else
	{
		return Import(inputFilePath, ShowDialog(inputFilePath));
	}
}

void CFileImporter::SetMayOverwriteFunc(const MayOverwriteFunc& mayOverwrite)
{
	m_mayOverwrite = mayOverwrite;
}

string CFileImporter::GetOutputFilePath() const
{
	return m_outputFilePath;
}

string CFileImporter::GetError() const
{
	return m_error;
}

string CFileImporter::ShowDialog(const string& inputFilename)
{
	CEngineFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Save file to directory...");

	const QString path = CEngineFileDialog::RunGameSelectDirectory(runParams, nullptr);
	return PathUtil::Make(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), QtUtil::ToString(path)), PathUtil::GetFile(inputFilename));
}

// ==================================================
// Rendering.
// ==================================================

void ShowLabelNextToCursor(const QViewport* vp, const string& label)
{
	const float xOffset = 10.0f;
	const float yOffset = 10.0f;

	QPoint mousePos = vp->mapFromGlobal(QCursor::pos());
	if (mousePos.x() >= 0 && mousePos.x() < vp->width() &&
		mousePos.y() >= 0 && mousePos.y() < vp->height())
	{
		IRenderer* const pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* const pAux = pRenderer->GetIRenderAuxGeom();
		pAux->Draw2dLabel(mousePos.x() + xOffset, mousePos.y() + yOffset, 1.5, ColorF(1, 1, 1), false,
			"%s", label.c_str());
	}
}

// ==================================================
// Qt <-> STL
// ==================================================

QStringList ToStringList(const std::vector<string>& strs)
{
	QStringList ret;
	std::transform(strs.begin(), strs.end(), std::back_inserter(ret), [](const string& str) { return QtUtil::ToQString(str); }); // Lambda for overload resolution.
	return ret;
}

std::future<std::pair<bool, string>> CopySourceFileToDirectoryAsync(const string& from, const string& dir, bool bShowNotification)
{
	const QString absOriginalFilePath = QtUtil::ToQString(from);
	const string absDir = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), dir);

	const QFileInfo origInfo(absOriginalFilePath);
	const QFileInfo dirInfo(QtUtil::ToQString(absDir));

	if (!origInfo.isFile())
	{
		std::promise<std::pair<bool, string>> promise;
		promise.set_value({ false, string().Format("Absolute path '%s' is not a file.", QtUtil::ToString(absOriginalFilePath).c_str()) });
		return promise.get_future();
	}
		
	if (!dirInfo.isDir())
	{
		std::promise<std::pair<bool, string>> promise;
		promise.set_value({ false, string().Format("Absolute path '%s' is not a directory.", absDir) });
		return promise.get_future();
	}

	const QString targetFilePath = QDir(dirInfo.absoluteFilePath()).absoluteFilePath(origInfo.fileName());

	auto copyFunction = [absOriginalFilePath, targetFilePath]()
	{
		CFileImporter fileImporter;
		if (!fileImporter.Import(QtUtil::ToString(absOriginalFilePath), QtUtil::ToString(targetFilePath)))
		{
			return std::make_pair(false, fileImporter.GetError());
		}
		else
		{
			return std::make_pair(true, string());
		}
	};

	return ThreadingUtils::Async([copyFunction, targetFilePath, bShowNotification]()
	{
		if (bShowNotification)
		{
			const QString message = QApplication::tr("File %1").arg(targetFilePath);
			CProgressNotification notif(QApplication::tr("Copying file"), message);
			const std::pair<bool, string> ret = copyFunction();
			notif.SetProgress(1.0);
			return ret;
		}
		else
		{
			return copyFunction();
		}
	});
}


