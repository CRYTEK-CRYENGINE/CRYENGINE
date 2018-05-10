// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameTokenSystem.h
//  Version:     v1.00
//  Created:     20/10/2005 by Craig,Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _GameTokenSystem_h_
#define _GameTokenSystem_h_
#pragma once

#include <CryGame/IGameTokens.h>
#include <CryCore/StlUtils.h>

class CGameToken;

//////////////////////////////////////////////////////////////////////////
// Game Token Manager implementation class.
//////////////////////////////////////////////////////////////////////////
class CGameTokenSystem : public IGameTokenSystem
{
	friend class CGameTokenIterator;
public:
	CGameTokenSystem();
	~CGameTokenSystem();

	//////////////////////////////////////////////////////////////////////////
	// IGameTokenSystem
	//////////////////////////////////////////////////////////////////////////
	virtual IGameToken*   SetOrCreateToken(const char* sTokenName, const TFlowInputData& defaultValue) override;
	virtual void          DeleteToken(IGameToken* pToken) override;
	virtual IGameToken*   FindToken(const char* sTokenName) override;
	virtual void          RenameToken(IGameToken* pToken, const char* sNewName) override;

	virtual IGameTokenIt* GetGameTokenIterator() override;

	virtual void          RegisterListener(const char* sGameToken, IGameTokenEventListener* pListener, bool bForceCreate, bool bImmediateCallback) override;
	virtual void          UnregisterListener(const char* sGameToken, IGameTokenEventListener* pListener) override;

	virtual void          RegisterListener(IGameTokenEventListener* pListener) override;
	virtual void          UnregisterListener(IGameTokenEventListener* pListener) override;

	virtual void          TriggerTokensAsChanged() override;

	virtual void          Reset() override;
	virtual void          Unload() override;
	virtual void          Serialize(TSerialize ser) override;

	virtual void          DebugDraw() override;

	virtual void          LoadLibs(const char* sFileSpec) override;
	virtual void          RemoveLibrary(const char* sPrefix) override;

	virtual void          SerializeSaveLevelToLevel(const char** ppGameTokensList, uint32 numTokensToSave) override;
	virtual void          SerializeReadLevelToLevel() override;

	virtual void          GetMemoryStatistics(ICrySizer* s) override;

	virtual void          SetGoingIntoGame(bool bGoingIntoGame) override { m_bGoingIntoGame = bGoingIntoGame; }
	//////////////////////////////////////////////////////////////////////////

	CGameToken* GetToken(const char* sTokenName);
	void        Notify(EGameTokenEvent event, CGameToken* pToken);
	void        DumpAllTokens();

private:
	bool _InternalLoadLibrary(const char* filename, const char* tag);

	// Use Hash map for speed.
	typedef std::unordered_map<const char*, CGameToken*, stl::hash_stricmp<const char*>, stl::hash_stricmp<const char*>> GameTokensMap;

	typedef std::vector<IGameTokenEventListener*>                                                                        Listeners;
	Listeners m_listeners;

	// Loaded libraries.
	std::vector<string>          m_libraries;

	GameTokensMap*               m_pGameTokensMap; // A pointer so it can be fully unloaded on level unload
	class CScriptBind_GameToken* m_pScriptBind;
	XmlNodeRef                   m_levelToLevelSave;
	bool                         m_bGoingIntoGame;

#ifdef _GAMETOKENSDEBUGINFO
	typedef std::set<string> TDebugListMap;
	TDebugListMap m_debugList;
	struct SDebugHistoryEntry
	{
		string     tokenName;
		string     value;
		CTimeValue timeChanged;
	};

	enum { DBG_HISTORYSIZE = 25 };
	uint32                          m_historyStart; // start of the circular list in m_debugHistory.
	uint32                          m_historyEnd;
	uint32                          m_oldNumHistoryLines; // just to control dynamic changes without using a callback
	std::vector<SDebugHistoryEntry> m_debugHistory;       // works like a circular list
	static int                      m_CVarShowDebugInfo;
	static int                      m_CVarPosX;
	static int                      m_CVarPosY;
	static int                      m_CVarNumHistoricLines;
	static ICVar*                   m_pCVarFilter;

	void        DrawToken(const char* pTokenName, const char* pTokenValue, const CTimeValue& timeChanged, int line);
	void        ClearDebugHistory();
	int         GetHistoryBufferSize() { return min(int(DBG_HISTORYSIZE), m_CVarNumHistoricLines); }
	const char* GetTokenDebugString(CGameToken* pToken);

public:
	virtual void AddTokenToDebugList(const char* pToken) override;
	virtual void RemoveTokenFromDebugList(const char* pToken) override;

#endif

};

#endif // _GameTokenSystem_h_
