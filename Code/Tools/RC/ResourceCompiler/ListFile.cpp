// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek GmbH, 2010.
// -------------------------------------------------------------------------
//  File name:   ListFile.cpp
//  Version:     v2.00
//  Created:     02/02/2010 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ListFile.h"
#include "StringHelpers.h"
#include "TempFilePakExtraction.h"
#include "PathHelpers.h"
#include "IPakSystem.h"
#include "IResCompiler.h"
#include "IRCLog.h"

//////////////////////////////////////////////////////////////////////////
CListFile::CListFile(IResourceCompiler* pRC)
	: m_pRC(pRC)
{
}

//////////////////////////////////////////////////////////////////////////
bool CListFile::Process(
	const string& listFile,
	const string& formatList,
	const string& wildcardList,
	const string& defaultFolder,
	std::vector< std::pair<string, string> >& outFiles)
{
	std::vector<string> wildcards;
	StringHelpers::Split(wildcardList, ";", false, wildcards);
	for (size_t i = 0; i < wildcards.size(); ++i)
	{
		wildcards[i].replace('/', '\\');
	}

	std::vector<string> formats;
	StringHelpers::Split(formatList, ";", false, formats);
	for (size_t i = 0; i < formats.size(); ++i)
	{
		formats[i].replace('/', '\\');
	}
	if (formats.empty())
	{
		formats.push_back("{0}");
	}

	if (!listFile.empty() && listFile[0] == '@')
	{
		int splitter = listFile.find_first_of("|;,");
		if (splitter < 0)
			return false;

		string zipFilename = listFile.substr(1, splitter - 1);
		string listFilename = listFile.substr(splitter + 1);
		zipFilename.Trim();
		listFilename.Trim();

		ParseListFileInZip(zipFilename, listFilename, formats, wildcards, defaultFolder, outFiles);
		return true;
	}

	// Parse List File.
	std::vector<string> lines;
	if (!ReadLines(listFile,lines))
	{
		return false;
	}

	for (size_t i = 0; i < lines.size(); ++i)
	{
		string line = lines[i];

		// Line can either contain filename, folder & filename
		// or a zip file + list file (ex: @Levels\AlienVessel\Level.pak|resourcelist.txt)
		if (line[0] == '@')
		{
			// the line starts with '@' character, this means a zip file
			
			line = line.substr(1); // erase @ character

			const size_t splitter = line.find_first_of("|;,");
			if (splitter == line.npos)
			{
				continue;
			}

			string zipFilename = line.substr(0, splitter);
			string listFilename = line.substr(splitter + 1);
			zipFilename.Trim();
			listFilename.Trim();

			ParseListFileInZip(zipFilename, listFilename, formats, wildcards, defaultFolder, outFiles);
		}
		else
		{
			if (!ProcessLine(line, formats, wildcards, defaultFolder, outFiles))
			{
				return false;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CListFile::ParseListFileInZip(
	const string& zipFilename,
	const string& listFilename,
	const std::vector<string>& formats,
	const std::vector<string>& wildcards,
	const string& defaultFolder,
	std::vector< std::pair<string, string> >& outFiles)
{
	// Open zip file
	IPakSystem* const pPakSystem = m_pRC->GetPakSystem();
	const char* const pTempPath = m_pRC->GetTmpPath();

	const string sFileInPak = string("@") + zipFilename + "|" + listFilename;
	TempFilePakExtraction fileProxy(sFileInPak.c_str(), pTempPath, pPakSystem);

	// Parse List File.
	std::vector<string> lines;
	if (!ReadLines(fileProxy.GetTempName(), lines))
	{
		RCLogWarning("List file %s not found in zip file %s", listFilename.c_str(), zipFilename.c_str());
		return;
	}

	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (!ProcessLine(lines[i], formats, wildcards, defaultFolder, outFiles))
		{
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CListFile::ProcessLine(
	const string& line,
	const std::vector<string>& formats,
	const std::vector<string>& wildcards,
	const string& defaultFolder,
	std::vector< std::pair<string, string> >& outFiles)
{
	string folderName;
	string fileName;

	{
		// Line can either contain filename (in quotes or without them) or folder & filename (both in quotes).

		const char* p0 = strchr(line.c_str(), '\"');
		const char* p1 = (p0 ? strchr(p0 + 1, '\"') : 0);
		const char* p2 = (p1 ? strchr(p1 + 1, '\"') : 0);
		const char* p3 = (p2 ? strchr(p2 + 1, '\"') : 0);
		if (p0 == 0)
		{
			// single filename without quotes
			folderName = defaultFolder;
			fileName = line;
		}
		else if (p1 && (p2 == 0))
		{
			// single filename in quotes
			folderName = defaultFolder;
			fileName = string(p0 + 1, p1);
		}
		else if (p3)
		{
			// folder & filename in quotes
			folderName = string(p0 + 1, p1);
			fileName = string(p2 + 1, p3);
		}
		else
		{
			RCLogError("Bad syntax of a row in list file");
			return false;
		}
	}

	if (fileName.empty())
	{
		RCLogError("Filename is empty in a row of list file");
		return false;
	}

	fileName.replace('/', '\\');
	folderName.replace('/', '\\');

	std::vector<string> tokens;

	{
		bool bMatchFound = false;

		for (size_t i = 0; i < wildcards.size(); ++i)
		{
			if (StringHelpers::MatchesWildcardsIgnoreCase(fileName, wildcards[i]))
			{
				if (!StringHelpers::MatchesWildcardsIgnoreCaseExt(fileName, wildcards[i], tokens))
				{
					RCLogError("Unexpected failure in %s", __FUNCTION__);
					return false;
				}
				bMatchFound = true;
				break;
			}
		}

		if (!bMatchFound)
		{
			return true;
		}
	}

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		string str = formats[formatIndex];

		size_t scanFromPos = 0;
		for (;;)
		{
			const size_t startPos = str.find('{', scanFromPos);
			if (startPos == str.npos)
			{
				break;
			}

			const size_t endPos = str.find('}', startPos + 1);
			if (endPos == str.npos)
			{
				break;
			}

			const string indexStr = str.substr(startPos + 1, endPos - startPos - 1);

			bool bBadSyntax = indexStr.empty();
			for (size_t i = 0; i < indexStr.length(); ++i)
			{
				if (!isdigit(indexStr[i]))
				{
					 bBadSyntax = true;
				}
			}
			if (bBadSyntax)
			{
				RCLogError("Syntax error in element {%s} in input string %s", indexStr.c_str(), formats[formatIndex].c_str());
				return false;
			}

			const int index = atoi(indexStr.c_str());
			if ((index < 0) || (index > tokens.size()))
			{
				RCLogError("Bad index specified in {%s} in input string %s", indexStr.c_str(), formats[formatIndex].c_str());
				return false;
			}

			const string& replaceWith = (index == 0) ? fileName : tokens[index - 1];
			str = str.replace(startPos, endPos - startPos + 1, replaceWith);
			scanFromPos = startPos + replaceWith.size();
		}

		outFiles.push_back(std::pair<string, string>(folderName, str));
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CListFile::ReadLines(
	const string& listFile,
	std::vector<string>& lines)
{
	FILE* const f = fopen(listFile, "rt");
	if (!f)
	{
		return false;
	}

	char line[2048];
	while (fgets(line, sizeof(line), f) != NULL)
	{
		if (line[0])
		{
			string strLine = line;
			strLine.Trim();
			if (!strLine.empty())
			{
				lines.push_back(strLine);
			}
		}
	}

	fclose(f);

	return true;
}
