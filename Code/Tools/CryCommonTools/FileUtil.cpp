// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileUtil.h"
#include "PathHelpers.h"
#include "StringHelpers.h"

#include <io.h>
#include <CryString/CryPath.h>

//////////////////////////////////////////////////////////////////////////
// returns true if 'dir' is a subdirectory of 'baseDir' or same directory as 'baseDir' 
// note: returns false in case of wrong names passed
static bool IsSubdirOrSameDir( const char* dir, const char* baseDir )
{
	wstring wdir, wbasedir;
	Unicode::Convert(wdir, dir);
	Unicode::Convert(wbasedir, baseDir);

	wchar_t szFullPathDir[2*1024];
	if (!_wfullpath(szFullPathDir, wdir.c_str(), sizeof(szFullPathDir) / sizeof(wchar_t)))
	{
		return false;
	}

	wchar_t szFullPathBaseDir[2*1024];
	if (!_wfullpath(szFullPathBaseDir, wbasedir.c_str(), sizeof(szFullPathBaseDir) / sizeof(wchar_t)))
	{
		return false;
	}

	const wchar_t* p = szFullPathDir;
	const wchar_t* q = szFullPathBaseDir;
	for(;;++p, ++q)
	{
		if (towlower(*p) == towlower(*q))
		{
			if (*p == 0)
			{
				// dir is exactly same as baseDir
				return true;
			}
			continue;
		}

		if ((*p=='/' || *p=='\\') && (*q=='/' || *q=='\\'))
		{
			continue;
		}

		if (*p == 0)
		{
			// dir length is shorter than baseDir length. so it's not a subdir
			return false;			
		}

		if (*q == 0)
		{
			// baseDir is shorter than dir. so may be it's a subdir.
			const bool isSubdir = (*p=='/' || *p=='\\');
			return isSubdir;
		}

		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
void FileUtil::FindFiles(
	std::vector<string>& resultFiles,
	const int maxFileCount,
	const string& root,
	const string& path,
	bool recursive,
	const string& fileMask,
	const std::vector<string>& dirsToIgnore)
{
	_wfinddata64_t c_file;
	intptr_t hFile;

	string fullPath = PathUtil::Make(PathUtil::Make(root, path), fileMask);
	wstring wfullPath, wfileMask;
	Unicode::Convert(wfullPath, fullPath);
	Unicode::Convert(wfileMask, fileMask);

	if ((hFile = _wfindfirst64(wfullPath.c_str(), &c_file)) != -1L)
	{
		do
		{
			if ((!(c_file.attrib & _A_SUBDIR)) && StringHelpers::MatchesWildcardsIgnoreCase(c_file.name, wfileMask))
			{
				string fileName;
				Unicode::Convert(fileName, c_file.name);

				resultFiles.push_back(PathUtil::Make(path, fileName));
				if ((int)resultFiles.size() >= maxFileCount)
				{
					return;
				}
			}
		}
		while (_wfindnext64(hFile, &c_file) == 0);
		_findclose(hFile);
	}

	// Scan directories.
	if (recursive)
	{
		fullPath = PathUtil::Make(PathUtil::Make(root, path), string("*.*"));
		Unicode::Convert(wfullPath, fullPath);
		if ((hFile = _wfindfirst64(wfullPath.c_str(), &c_file)) != -1L)
		{
			do
			{
				if ((c_file.attrib & _A_SUBDIR) && !StringHelpers::Equals(c_file.name, L".") && !StringHelpers::Equals(c_file.name, L".."))
				{
					string fileName;
					Unicode::Convert(fileName, c_file.name);

					bool bSkip = false;
					for (size_t i = 0; i < dirsToIgnore.size(); ++i)
					{
						if (StringHelpers::MatchesWildcardsIgnoreCase(fileName, dirsToIgnore[i]))
						{
							bSkip = true;
							break;
						}
					}
					if (!bSkip)
					{
						FindFiles(resultFiles, maxFileCount, root, PathUtil::Make(path, fileName), recursive, fileMask, dirsToIgnore);
						if (resultFiles.size() >= maxFileCount)
						{
							return;
						}
					}
				}
			}
			while (_wfindnext64(hFile, &c_file) == 0);
			_findclose(hFile);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// the paths must have trailing slash
static bool ScanDirectoryRecursive( const string &root, const string &path, const string &file, std::vector<string> &files, bool recursive, const string &dirToIgnore )
{
	_wfinddata64_t c_file;
	intptr_t hFile;

	bool anyFound = false;

	string fullPath = PathUtil::Make(root, path);

	if (!dirToIgnore.empty())
	{
		if (IsSubdirOrSameDir(fullPath.c_str(), dirToIgnore.c_str()))
		{
			return anyFound;
		}
	}

	fullPath = PathUtil::Make(fullPath, file);

	wstring wfullpath, wfile;
	Unicode::Convert(wfullpath, fullPath);
	Unicode::Convert(wfile, file);

	if ( (hFile = _wfindfirst64(wfullpath.c_str(), &c_file )) != -1L )
	{
		// Find the rest of the files.
		do {
			if ((!(c_file.attrib & _A_SUBDIR)) && StringHelpers::MatchesWildcardsIgnoreCase(c_file.name, wfile))
			{
				anyFound = true;

				string fileName;
				Unicode::Convert(fileName, c_file.name);

				string foundFile = PathUtil::Make(path, fileName);

				files.push_back(foundFile);
			}
		}	while (_wfindnext64( hFile, &c_file ) == 0);
		_findclose( hFile );
	}

	if (recursive)
	{
		fullPath = PathUtil::Make(PathUtil::Make(root, path), string("*.*"));
		Unicode::Convert(wfullpath, fullPath);

		if( (hFile = _wfindfirst64(wfullpath.c_str(), &c_file )) != -1L )
		{
			// Find directories.
			do {
				if (c_file.attrib & _A_SUBDIR)
				{
					// If recursive.
					if (wcscmp(c_file.name, L".") && wcscmp(c_file.name, L".."))
					{
						string fileName;
						Unicode::Convert(fileName, c_file.name);

						if (ScanDirectoryRecursive( root, PathUtil::Make(path, fileName),file,files,recursive,dirToIgnore ))
							anyFound = true;
					}
				}
			}	while (_wfindnext64( hFile, &c_file ) == 0);
			_findclose( hFile );
		}
	}

	return anyFound;
}

//////////////////////////////////////////////////////////////////////////

bool FileUtil::ScanDirectory(const string& path, const string& file, std::vector<string>& files, bool recursive, const string& dirToIgnore)
{
	return ScanDirectoryRecursive(path, "", file, files, recursive, dirToIgnore);
}


bool FileUtil::EnsureDirectoryExists(const char* szPathIn)
{
	if (!szPathIn || !szPathIn[0])
	{
		return true;
	}

	if (DirectoryExists(szPathIn))
	{
		return true;
	}

	wstring widePath;
	Unicode::Convert(widePath, szPathIn);

	wchar_t* p = const_cast<wchar_t*>(widePath.data());

	// Skip '/' and '//' in the beginning
	while (*p == '/' || *p == '\\')
	{
		++p;
	}

	for (;;)
	{
		while (*p != '/' && *p != '\\' && *p)
		{
			++p;
		}
		const wchar_t saved = *p;
		*p = 0;
		_wmkdir(&widePath[0]);
		*p++ = saved;
		if (saved == 0)
		{
			break;
		}
	}

	return DirectoryExists(szPathIn);
}
