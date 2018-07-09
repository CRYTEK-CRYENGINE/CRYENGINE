// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SHADERLIST__
#define __SHADERLIST__

#include <map>
#include <set>
#include <vector>

#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "CrySimpleMutex.hpp"

class CShaderListFile
{
	//////////////////////////////////////////////////////////////////////////
	struct SMetaData
	{
		SMetaData():
		m_Version(0),
		m_Count(-1)
		{}
		int32_t m_Version;
		int32_t m_Count;
	};
	bool m_bModified;
	std::string m_listname;
	std::string m_filename;
	std::string m_filenametmp;
	typedef std::map<std::string,SMetaData> Entries;
	Entries m_entries;
	std::vector<std::string> m_newLines;

	CCrySimpleMutex m_Mutex;

	//do not copy -> not safe
	CShaderListFile(const CShaderListFile&){}
	CShaderListFile operator=(const CShaderListFile&){}
public:
  CShaderListFile(std::string ListName);

	bool Load( const char *filename );
	bool Save();
	bool Reload();
	bool IsModified() const { return m_bModified; }

	void InsertLine( const char *szLine );
	void MergeNewLinesAndSave();

private:
	void CreatePath(const std::string& rPath);
	void MergeNewLines();
	// Returns:
	//   true - line was instered, false otherwise
	bool InsertLineInternal( const char *szLine );

	// Returns
	//   true=syntax is ok, false=syntax is wrong
	static bool CheckSyntax( const char *szLine,SMetaData& rMD,const char **sOutStr=NULL);
};

typedef std::map<std::string,CShaderListFile*>	tdShaderLists;

class CShaderList
{
	CCrySimpleMutex					m_Mutex;
	CCrySimpleMutex					m_Mutex2;
	unsigned int            m_lastTime;

	tdShaderLists						m_ShaderLists;

	void										Save();
public:
	static CShaderList&			Instance();

	void										Add(const std::string& rShaderListName,const char* pLine);

													CShaderList();
	void										Tick();
};

#endif
