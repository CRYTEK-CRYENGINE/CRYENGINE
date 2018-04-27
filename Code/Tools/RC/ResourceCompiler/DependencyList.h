// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DependencyList_h__
#define __DependencyList_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
class CDependencyList
{
public:
	struct SFile
	{
		string inputFile;
		string outputFile;
	};

private:
	std::vector<SFile> m_files;
	bool m_bDuplicatesWereRemoved;

public:
	CDependencyList();

	CDependencyList(const CDependencyList& obj);

	static string NormalizeFilename(const char* filename);

	size_t GetCount() const
	{
		return m_files.size();
	}

	const SFile& GetElement(size_t index) const
	{
		return m_files[index];
	}

	void Add(const char* inputFilename, const char* outputFilename);

	void RemoveDuplicates();

	void RemoveInputFiles(const std::vector<string>& inputFilesToRemove);
	
	void GetOutputFiles(const string& inputFile, std::vector<string>& result);

	void Save(const char* filename) const;

	void SaveOutputOnly(const char* filename) const;

	void Load(const char *filename);
};

#endif //__DependencyList_h__
