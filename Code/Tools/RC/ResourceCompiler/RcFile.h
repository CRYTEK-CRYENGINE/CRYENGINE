// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __rcfile_h__
#define __rcfile_h__
#pragma once

class RcFile
{
public:
	string m_sourceLeftPath;
	string m_sourceInnerPathAndName;
	string m_targetLeftPath;
	string m_sourceDigest;

public:
	RcFile()
	{
	}

	RcFile(const string& sourceLeftPath, const string& sourceInnerPathAndName, const string& targetLeftPath)
		: m_sourceLeftPath(sourceLeftPath)
		, m_sourceInnerPathAndName(sourceInnerPathAndName)
		, m_targetLeftPath(targetLeftPath.empty() ? sourceLeftPath : targetLeftPath)
	{
	}
};

#endif // __rcfile_h__
