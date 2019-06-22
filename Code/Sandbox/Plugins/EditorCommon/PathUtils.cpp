// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "PathUtils.h"

#include "QtUtil.h"
#include <CryString/CryPath.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <IEditor.h>
#include <QDirIterator>

namespace Private_PathUtils
{

//! SplitIndexedName removes the trailing integer of a string. The resulting string is called the 'stem'.
//! Leading zeros of a trailing integer are ignored and part of the stem.
//! Only decimal integers are removed.
//! A string contains a trailing integer if \p name != \ stem. If there is no trailing number, the value
//! of \p num is -1.
//!
//! This function is typically used to generate unique filenames, such as 'Untitled2'.
//!
//! Examples:
//! test => test, -1
//! test12 => test, 12
//! test0.12 => test0., 12
//! test0xF2 => test0xF, 2
//! test0099 => test00, 99
//! test-12 => test-, 12
//! 123test0 => 123test0, -1
static void SplitIndexedName(const string& name, string& stem, int& num)
{
	CRY_ASSERT(!name.empty());

	size_t N = name.size();
	size_t i = N;
	while (i > 0 && isdigit(name[i - 1]))
	{
		i--;
	}
	while (i < N && name[i] == '0')
	{
		i++;
	}
	num = i != N ? atoi(&name.c_str()[i]) : -1;
	stem = name.substr(0, i);
}

string MatchPathSegmentsToPathOnFileSystem(const string& path, const string& rootFolder, const std::vector<string>& segments)
{
	QDir dir = QDir(QtUtil::ToQString(rootFolder));
	string finalPath = rootFolder;
	for (int i = 0; i < segments.size(); ++i)
	{
		const auto entries = dir.entryList({ QtUtil::ToQString(segments[i]) }, QDir::Dirs | QDir::Files);
		if (entries.empty())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "path %s doesn't exist on the file system", path);
			return "";
		}
		finalPath = PathUtil::Make(finalPath, QtUtil::ToString(entries[0]));
		dir = QDir(QtUtil::ToQString(finalPath));
	}
	return finalPath;
}

} // namespace Private_PathUtils

namespace PathUtil
{

string GetAudioLocalizationFolder()
{
	string sLocalizationFolder = PathUtil::GetLocalizationFolder();

	if (!sLocalizationFolder.empty())
	{
		sLocalizationFolder += CRY_NATIVE_PATH_SEPSTR "dialog" CRY_NATIVE_PATH_SEPSTR;
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "The localization folder is not set! Please make sure it is by checking the setting of cvar \"sys_localization_folder\"!");
	}

	return sLocalizationFolder;
}

string GetUserSandboxFolder()
{
	CryPathString path;
	gEnv->pCryPak->AdjustFileName("%USER%/Sandbox", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
	
	const size_t len = path.length();
	if (len == 0 || (path[len - 1] != '\\' && path[len - 1] != '/'))
	{
		path += '\\';
	}
	
	if (strstr(path, ":\\") == nullptr)
	{
		char buffer[MAX_PATH];
		CryGetCurrentDirectory(sizeof(buffer), buffer);
		CryPathString fullPath(buffer);
		fullPath += '\\';
		fullPath += path;
		return fullPath.c_str();
	}
	else
	{
		return path.c_str();
	}
}

std::vector<string> SplitIntoSegments(const string& path)
{
	std::vector<string> splittedPath;
	int currentPos = 0;
	for (string token = path.Tokenize("/\\", currentPos); !token.empty(); token = path.Tokenize("/\\", currentPos))
		splittedPath.push_back(token);

	return splittedPath;
}

void SplitPath(const string& fullPathFilename, string& driveLetter, string& directory, string& filename, string& extension)
{
	string strDriveLetter;
	string strDirectory;
	string strFilename;
	string strExtension;

	const char* pchLastPosition = fullPathFilename.c_str();
	const char* pchAuxPosition = fullPathFilename.c_str();

	// Directory names or filenames containing ":" are invalid, so we can assume if there is a :
	// it will be the drive name.
	const char* pchCurrentPosition = strchr(pchLastPosition, ':');
	if (pchCurrentPosition == nullptr)
	{
		driveLetter = "";
	}
	else
	{
		strDriveLetter.assign(pchLastPosition, pchCurrentPosition + 1);
		pchLastPosition = pchCurrentPosition + 1;
	}

	pchCurrentPosition = strrchr(pchLastPosition, '\\');
	pchAuxPosition = strrchr(pchLastPosition, '/');
	if ((pchCurrentPosition == nullptr) && (pchAuxPosition == nullptr))
	{
		directory = "";
	}
	else
	{
		// Since NULL is < valid pointer, so this will work.
		if (pchAuxPosition > pchCurrentPosition)
		{
			pchCurrentPosition = pchAuxPosition;
		}
		strDirectory.assign(pchLastPosition, pchCurrentPosition + 1);
		pchLastPosition = pchCurrentPosition + 1;
	}

	pchCurrentPosition = strrchr(pchLastPosition, '.');
	if (pchCurrentPosition == nullptr)
	{
		extension = "";
		strFilename.assign(pchLastPosition);
	}
	else
	{
		strExtension.assign(pchCurrentPosition);
		strFilename.assign(pchLastPosition, pchCurrentPosition);
	}

	driveLetter = strDriveLetter;
	directory = strDirectory;
	filename = strFilename;
	extension = strExtension;
}

string GetGameProjectAssetsPath()
{
	return gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute();
}

string GetGameProjectAssetsRelativePath()
{
	return gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();
}

string GetEditorCachePath()
{
	return Make(GetProjectFolder(), GetEditorCacheRelativePath());
}

string GetEditorCacheRelativePath()
{
	const ICVar* pCacheFolderVar = gEnv->pConsole->GetCVar("sys_resource_cache_folder");
	return ToUnixPath(pCacheFolderVar->GetString());
}

string GetCurrentProjectDirectoryAbsolute()
{
	return gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute();
}

string GetCurrentPlatformFolder()
{
#ifdef CRY_PLATFORM_WINDOWS
	return "windows";
#elif CRY_PLATFORM_LINUX
	return "linux";
#elif CRY_PLATFORM_POSIX
	return "posix";
#else
	#error Unknown Platform
#endif // CRY_PLATFORM_WINDOWS
}

string ReplaceFilename(const string& filepath, const string& filename)
{
	string driveLetter;
	string directory;
	string originalFilename;
	string extension;

	SplitPath(filepath, driveLetter, directory, originalFilename, extension);

	return driveLetter + directory + filename + extension;
}

string GetDirectory(const string& filepath)
{
	string driveLetter;
	string directory;
	string originalFilename;
	string extension;

	SplitPath(filepath, driveLetter, directory, originalFilename, extension);

	return directory;
}

string GetUniqueName(const string& templateName, const std::vector<string>& otherNames)
{
	using namespace Private_PathUtils;

	struct EqualNoCase
	{
		explicit EqualNoCase(const string& str) : m_str(str) {}

		bool operator()(const string& other)
		{
			return !m_str.compareNoCase(other);
		}

		const string& m_str;
	};

	CRY_ASSERT(!templateName.empty());

	if (std::find_if(otherNames.begin(), otherNames.end(), EqualNoCase(templateName)) == otherNames.end())
	{
		return templateName;
	}

	string stem;
	int num;
	SplitIndexedName(templateName, stem, num);
	if (num == -1)
	{
		num = 1;
	}

	while (true)
	{
		// const string name = string().Format("%s%d", stem.c_str(), num);
		char name[1024];
		sprintf(name, "%s%d", stem.c_str(), num);
		if (std::find_if(otherNames.begin(), otherNames.end(), EqualNoCase(name)) == otherNames.end())
		{
			return name;
		}
		num++;
	}
	CRY_ASSERT(0 && "unreachable");
	return string();
}

string GetUniqueName(const string& fileName, const string& folderPath)
{
	std::vector<string> resultSet;
	const string absFolder = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), folderPath);
	const size_t absFolderLen = PathUtil::AddSlash(absFolder).size();
	const string mask = string().Format("*.%s", PathUtil::GetExt(fileName));
	QDirIterator iterator(QtUtil::ToQString(absFolder), QStringList() << mask.c_str(), QDir::Files);
	while (iterator.hasNext())
	{
		string filePath = QtUtil::ToString(iterator.next()).substr(absFolderLen);
		PathUtil::RemoveExtension(filePath);
		PathUtil::RemoveExtension(filePath);
		resultSet.push_back(filePath);
	}
	return GetUniqueName(RemoveExtension(fileName), resultSet);
}

QString ReplaceExtension(const QString& str, const char* ext)
{
	// TODO: This is the lazy solution.
	return QtUtil::ToQString(PathUtil::ReplaceExtension(QtUtil::ToString(str), ext));
}

inline string AbsoluteToRelativePath(const string& absolutePath, const char* dirPathRelativeTo)
{
	CryPathString path(absolutePath);
	path = ToUnixPath(path);
	char buf[MAX_PATH];

	CryPathString rootPathStr = ToUnixPath(CryPathString(dirPathRelativeTo));
	if (rootPathStr.empty())
	{
		return absolutePath;
	}

	if (rootPathStr[rootPathStr.length() - 1] != '/')
	{
		rootPathStr += '/';
	}

	PathUtil::SimplifyFilePath(path.c_str(), buf, sizeof(buf), ePathStyle_Posix);

	// SimplifyFilePath removes the trailing slash. Restore it, if this is the path to the folder.
	if (!path.empty() && path[path.length() - 1] == '/')
	{
		path = PathUtil::AddSlash(buf);
	}
	else
	{
		path = buf;
	}
	if (cry_strnicmp(path.c_str(), rootPathStr.c_str(), rootPathStr.length()) == 0) // path starts with rootPathStr
		return path.substr(rootPathStr.length());
	else
		return path;
}

inline string AbsolutePathToCryPakPath(const string& path)
{
	if (path.empty())
		return "";

	char rootpath[MAX_PATH];
	CryGetCurrentDirectory(sizeof(rootpath), rootpath);

	return AbsoluteToRelativePath(path, rootpath);
}

string AbsolutePathToGamePath(const string& path)
{
	if (path.empty())
		return "";

	return AbsoluteToRelativePath(path, GetGameProjectAssetsPath());
}

QString ToUnixPath(const QString& path)
{
	if (path.indexOf('\\') != -1)
	{
		auto unixPath = path;
		unixPath.replace('\\', '/');
		return unixPath;
	}
	return path;
}

EDITOR_COMMON_API string GetEditorFolderPath()
{
	string path;

	return path.Format("%s/Editor", GetEnginePath());
}

EDITOR_COMMON_API string GetEditorScriptsFolderPath()
{
	string path;

	return path.Format("%s/Scripts", GetEditorFolderPath());
}

string GamePathToCryPakPath(const string& path, bool bForWriting /*= false*/)
{
	CRY_ASSERT(gEnv && gEnv->pCryPak);

	CryPathString adjustedFile;
	gEnv->pCryPak->AdjustFileName(path.c_str(), adjustedFile, bForWriting ? ICryPak::FLAGS_FOR_WRITING : 0);

	return adjustedFile.c_str();
}

QString ToGamePath(const QString& path)
{
	QString fixedPath = path;
	fixedPath.replace('\\', '/');

	const QString rootPath = QtUtil::ToQString(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute()) + "/";
	if (fixedPath.startsWith(rootPath, Qt::CaseInsensitive))
	{
		fixedPath = fixedPath.right(fixedPath.length() - rootPath.length());
	}

	const QString gameDir = QtUtil::ToQString(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative()) + "/";
	if (fixedPath.startsWith(gameDir, Qt::CaseInsensitive))
	{
		fixedPath = fixedPath.right(fixedPath.length() - gameDir.length());
	}

	return fixedPath;
}

string ToGamePath(const string& path)
{
	return ToGamePath(path.c_str());
}

string ToGamePath(const char* path)
{
	string fixedPath = ToUnixPath(path);

	string gameDir = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();
	gameDir += '/';

	if (cry_strnicmp(fixedPath.c_str(), gameDir.c_str(), gameDir.length()) == 0) // path starts with rootPathStr
		return fixedPath.substr(gameDir.length());
	else
		return AbsolutePathToGamePath(fixedPath);
}

bool IsValidFileName(const QString& name)
{
	if (name.isEmpty())
		return false;

	const int len = name.length();

	if (name[0] == ' ')
		return false;

	for (size_t i = 0; i < len; ++i)
	{
		const QChar c = name.at(i);
		if (!c.isLetterOrNumber() && c != ' ' && c != '-' && c != '_')
			return false;
	}

	return true;
}

//! Adjust the path to follow CryPak casing rules.
template<typename TString>
typename std::enable_if<detail::IsValidStringType<TString>::value, TString>::type
inline /*TString*/ AdjustCasingImpl(const char* szPath)
{
	ICryPak* const pPak = GetIEditor()->GetSystem()->GetIPak();
	CryPathString adjustedPath;
	pPak->AdjustFileName(szPath, adjustedPath, ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_PATH_REAL);
	return PathUtil::ToUnixPath(TString(adjustedPath.c_str()));
}

string AdjustCasing(const string& path)
{
	return AdjustCasingImpl<string>(path.c_str());
}

CryPathString AdjustCasing(const char* szPath)
{
	return AdjustCasingImpl<CryPathString>(szPath);
}

CryPathString AdjustCasing(const CryPathString& path)
{
	return AdjustCasingImpl<CryPathString>(path.c_str());
}

string MatchGamePathToCaseOnFileSystem(const string& path)
{
	return PathUtil::ToGamePath(Private_PathUtils::MatchPathSegmentsToPathOnFileSystem(path,
		PathUtil::GetGameFolder(), PathUtil::SplitIntoSegments(path)));
}

string MatchAbsolutePathToCaseOnFileSystem(const string& path)
{
	std::vector<string> pathSegments = PathUtil::SplitIntoSegments(path);
	string rootFolder = pathSegments[0] + '/';
	pathSegments.erase(pathSegments.begin());
	return Private_PathUtils::MatchPathSegmentsToPathOnFileSystem(path, rootFolder, pathSegments);
}

}
