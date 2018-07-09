// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "FileStorage.h"
#include "FileUtil.h"
#include "TextFileReader.h"
#include <CrySystem\File\CryFile.h>  // Includes CryPath.h in correct order. 

namespace
{
	static const char* const s_versionMarker = "2084676E-6E48-4967-9031-FA39B5449789";

	typedef std::unique_ptr<FILE, int(*)(FILE*)> file_ptr;

	static void WriteHeaderFile(const string& filename, const std::vector<string>& lines)
	{
		const string delimeter("\n\r");

		string header;
		for (const auto& line : lines)
		{
			header = !header.empty() ? header + delimeter + line : line;
		}

		// Add an version/eof marker to integrity check.
		header += delimeter + s_versionMarker;

		file_ptr file(fopen(filename.c_str(), "wb"), fclose);
		setbuf(file.get(), nullptr);
		fwrite(header.data(), sizeof(string::value_type), header.size(), file.get());
	}

	string EncodeFilename(const char* key, const char* file)
	{
		return string().Format("%s-%s", key, file);
	}
} // namespace


const void CFileStorage::CopyFiles(const string& key, const string& destinationFolder, std::vector<string>& copiedFiles) const
{
	copiedFiles.clear();

	const string headerPath = PathUtil::Make(m_path, key, "txt");

	std::vector<char*> lines;
	TextFileReader header;
	if (!header.Load(headerPath.c_str(), lines))
	{
		return;
	}

	if (lines.empty() || stricmp(lines.back(), s_versionMarker) != 0)
	{
		return;
	}

	const size_t expectedNumberOfFiles = lines.size() - 1;

	copiedFiles.reserve(expectedNumberOfFiles);
	for ( size_t i = 0; i < expectedNumberOfFiles; ++i)
	{
		const char* file = lines[i];
		const string encodedFilename = EncodeFilename(key, file);
		const string cachedFile = PathUtil::Make(m_path, encodedFilename);
		const string outputFile = PathUtil::Make(destinationFolder, file);

		if (!::CopyFile(cachedFile, outputFile, FALSE))
		{
			break;
		}

		copiedFiles.emplace_back(outputFile);
	}

	if (!copiedFiles.empty() && copiedFiles.size() != expectedNumberOfFiles)
	{
		for (const string& file : copiedFiles)
		{
			::DeleteFile(file);
		}
		copiedFiles.clear();
	}
}

void CFileStorage::SetFiles(const string& key, const std::vector<string>& files) const
{
	if (files.empty() || !FileUtil::EnsureDirectoryExists(m_path.c_str()))
	{
		return;
	}

	std::vector<string> cacheFiles;
	cacheFiles.reserve(files.size());

	for (const string& path : files)
	{
		const string file = PathUtil::GetFile(path);

		const string encodedFilename = EncodeFilename(key, file);
		if (!CopyFile(path, PathUtil::Make(m_path, encodedFilename), FALSE))
		{
			return;
		}
		cacheFiles.push_back(file);
	}

	const string headerPath = PathUtil::Make(m_path, key, "txt");
	WriteHeaderFile(headerPath, cacheFiles);
}


