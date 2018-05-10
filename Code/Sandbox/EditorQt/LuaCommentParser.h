// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LUACOMMENTPARSER_H__
#define __LUACOMMENTPARSER_H__

struct LuaTable
{
	LuaTable(int open, int close, string name)
	{
		m_Name = name;
		m_OpenBracePos = open;
		m_CloseBracePos = close;
	}

	void AddChild(LuaTable* table)
	{
		m_ChildTables.push_back(table);
	}

	LuaTable* FindChildByName(const char* name)
	{
		for (int i = 0; i < m_ChildTables.size(); i++)
		{
			if (m_ChildTables[i]->m_Name == name)
			{
				return m_ChildTables[i];
			}
		}
		return NULL;
	}

	~LuaTable()
	{
		for (int i = 0; i < m_ChildTables.size(); i++)
		{
			delete m_ChildTables[i];
		}
		m_ChildTables.resize(0);
	}

	string                 m_Name;
	int                    m_OpenBracePos;
	int                    m_CloseBracePos;
	std::vector<LuaTable*> m_ChildTables;
};

class LuaCommentParser
{
public:
	bool                     OpenScriptFile(const char* path);
	bool                     ParseComment(const char* tablePath, const char* varName, float* minVal, float* maxVal, float* stepVal, string* desc);
	static LuaCommentParser* GetInstance();
	~LuaCommentParser(void);
	void                     CloseScriptFile();
protected:
	int                      FindTables(LuaTable* parentTable = NULL, int offset = 0);
	string                   FindTableNameForBracket(int bracketPos);
	bool                     IsAlphaNumericalChar(char c);
	int                      GetVarInTable(const char* tablePath, const char* varName);
	int                      FindStringInFileSkippingComments(string searchString, int offset = 0);
	int                      FindWordInFileSkippingComments(string searchString, int offset = 0);
protected:
	FILE* m_pFile;
	char* m_AllText;
	char* m_MovingText;
private:
	LuaCommentParser();
	LuaTable* m_RootTable;
};

#endif __LUACOMMENTPARSER_H__

