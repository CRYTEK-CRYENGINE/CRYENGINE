// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek GmbH, 2010.
// -------------------------------------------------------------------------
//  File name:   ListFile.h
//  Version:     v2.00
//  Created:     02/02/2010 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ListFile_h__
#define __ListFile_h__
#pragma once

class CListFile
{
public:
	explicit CListFile(IResourceCompiler* pRC);

	bool Process(
		const string& listFile,
		const string& formatList,
		const string& wildcardList,
		const string& defaultFolder,
		std::vector< std::pair<string, string> >& outFiles);

private:
	void ParseListFileInZip(
		const string& zipFilename,
		const string& listFilename,
		const std::vector<string>& formats,
		const std::vector<string>& wildcards,
		const string& defaultFolder,
		std::vector< std::pair<string, string> >& outFiles);

	bool ProcessLine(
		const string& line,
		const std::vector<string>& formats,
		const std::vector<string>& wildcards,
		const string& defaultFolder,
		std::vector< std::pair<string, string> >& outFiles);

	bool ReadLines(
		const string& listFile,
		std::vector<string>& lines);

private:
	IResourceCompiler* m_pRC;
};

#endif //__ListFile_h__
