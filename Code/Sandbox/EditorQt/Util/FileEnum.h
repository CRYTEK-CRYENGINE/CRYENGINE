// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
//////////////////////////////////////////////////////////////////////////
//  CryENGINE Source File
//  Copyright (C) 2000-2012, Crytek GmbH, All rights reserved
//////////////////////////////////////////////////////////////////////////

class CFileEnum
{
public:
	CFileEnum();
	virtual ~CFileEnum();
	bool        GetNextFile(struct __finddata64_t* pFile);
	bool        StartEnumeration(const char* szEnumPathAndPattern, __finddata64_t* pFile);
	bool        StartEnumeration(const char* szEnumPath, const char* szEnumPattern, __finddata64_t* pFile);
	static bool ScanDirectory(
	  const string& path,
	  const string& file,
	  std::vector<string>& files,
	  bool bRecursive = true,
	  bool bSkipPaks = false);

protected:
	intptr_t m_hEnumFile;
};

