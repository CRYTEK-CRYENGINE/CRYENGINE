// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TextFileReader_h__
#define __TextFileReader_h__
#pragma once

struct IPakSystem;

// Summary: 
//	Utility class to read text files and split lines inplace. One allocation per one file.
class TextFileReader
{
public:
	bool Load(const char* filename, std::vector<char*>& lines);
	bool LoadFromPak(IPakSystem* system, const char* filename, std::vector<char*>& lines);

private:
	// fix line endings to zeros, so we can use strings from the same buffer
	void PrepareLines(std::vector<char*>& lines);

	std::vector<char> m_buffer;
};

#endif
