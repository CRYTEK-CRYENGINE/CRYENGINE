// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <md5/md5.h>

//! A class to incrementally build md5 hash of data coming from files or memory blocks.
class CDigest
{
public:
	CDigest();

	void UpdateFromData(uint8* data, const size_t dataSize);
	bool UpdateFromFile(const string& filePath);

	string Final();

	CDigest Clone() const
	{
		return CDigest(*this);
	}

private:
	static const int s_digestSize = 16;

	void Final(std::array<uint8, s_digestSize>& digest);

	MD5Context m_context;
};

