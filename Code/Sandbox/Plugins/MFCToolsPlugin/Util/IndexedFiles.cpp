// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IndexedFiles.h"

volatile int CIndexedFiles::s_bIndexingDone;
CIndexedFiles* CIndexedFiles::s_pIndexedFiles = NULL;

void CIndexedFiles::Initialize(const string& path, CFileUtil::ScanDirectoryUpdateCallBack updateCB)
{
	m_files.clear();
	m_pathToIndex.clear();
	m_tags.clear();
	m_rootPath = path;

	bool anyFiles = CFileUtil::ScanDirectory(path, "*.*", m_files, true, true, updateCB);

	if (anyFiles == false)
	{
		m_files.clear();
		return;
	}

	if (updateCB)
		updateCB("Parsing & tagging...");

	for (int i = 0; i < m_files.size(); ++i)
		m_pathToIndex[m_files[i].filename.GetString()] = i;

	PrepareTagTable();
}

void CIndexedFiles::AddFile(const CFileUtil::FileDesc& path)
{
	assert(m_pathToIndex.find(path.filename.GetString()) == m_pathToIndex.end());
	m_files.push_back(path);
	m_pathToIndex[path.filename.GetString()] = m_files.size() - 1;
	std::vector<string> tags;
	GetTags(tags, path.filename.GetString());
	for (int k = 0; k < tags.size(); ++k)
	{
		m_tags[tags[k]].insert(m_files.size() - 1);
	}
}

void CIndexedFiles::RemoveFile(const string& path)
{
	if (m_pathToIndex.find(path) == m_pathToIndex.end())
		return;
	std::map<string, int>::iterator itr = m_pathToIndex.find(path);
	int index = itr->second;
	m_pathToIndex.erase(itr);
	m_files.erase(m_files.begin() + index);
	std::vector<string> tags;
	GetTags(tags, path);
	for (int k = 0; k < tags.size(); ++k)
	{
		m_tags[tags[k]].erase(index);
	}
}

void CIndexedFiles::Refresh(const string& path, bool recursive)
{
	CFileUtil::FileArray files;
	bool anyFiles = CFileUtil::ScanDirectory(m_rootPath, PathUtil::Make(path, "*.*").c_str(), files, recursive, recursive ? true : false);

	if (anyFiles == false)
		return;

	for (int i = 0; i < files.size(); ++i)
	{
		if (m_pathToIndex.find(files[i].filename.GetString()) == m_pathToIndex.end())
			AddFile(files[i]);
	}
}

void CIndexedFiles::GetFilesWithTags(CFileUtil::FileArray& files, const std::vector<string>& tags) const
{
	files.clear();
	if (tags.empty())
		return;
	int_set candidates;
	TagTable::const_iterator i;
	// Gets candidate files from the first tag.
	for (i = m_tags.begin(); i != m_tags.end(); ++i)
	{
		if (strncmp(i->first, tags[0], strlen(tags[0])) == 0)
			candidates.insert(i->second.begin(), i->second.end());
	}
	// Reduces the candidates further using additional tags, if any.
	for (int k = 1; k < tags.size(); ++k)
	{
		// Gathers the filter set.
		int_set filter;
		for (i = m_tags.begin(); i != m_tags.end(); ++i)
		{
			if (strncmp(i->first, tags[k], strlen(tags[k])) == 0)
				filter.insert(i->second.begin(), i->second.end());
		}

		// Filters the candidates using it.
		for (int_set::iterator m = candidates.begin(); m != candidates.end(); )
		{
			if (filter.find(*m) == filter.end())
			{
				int_set::iterator target = m;
				++m;
				candidates.erase(target);
			}
			else
				++m;
		}
	}
	// Outputs the result.
	files.reserve(candidates.size());
	for (int_set::const_iterator m = candidates.begin(); m != candidates.end(); ++m)
		files.push_back(m_files[*m]);
}

void CIndexedFiles::GetTags(std::vector<string>& tags, const string& path) const
{
	tags.clear();
	string tag;
	int curPos = 0;
	tag = path.Tokenize("/\\.", curPos);
	while (tag != "")
	{
		tags.push_back(tag.MakeLower());
		tag = path.Tokenize("/\\.", curPos);
	}
}

void CIndexedFiles::GetTagsOfPrefix(std::vector<string>& tags, const string& prefix) const
{
	tags.clear();
	TagTable::const_iterator i;
	for (i = m_tags.begin(); i != m_tags.end(); ++i)
	{
		if (strncmp(i->first, prefix, strlen(prefix)) == 0)
			tags.push_back(i->first);
	}
}

void CIndexedFiles::PrepareTagTable()
{
	std::vector<string> tags;
	for (int i = 0; i < m_files.size(); ++i)
	{
		GetTags(tags, m_files[i].filename.GetString());
		for (int k = 0; k < tags.size(); ++k)
		{
			m_tags[tags[k]].insert(i);
		}
	}
}

