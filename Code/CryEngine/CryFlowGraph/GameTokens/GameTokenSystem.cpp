// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CGameTokenSystem.cpp
//  Version:     v1.00
//  Created:     20/10/2005 by Craig,Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "GameTokenSystem.h"
#include "GameToken.h"
#include "ScriptBind_GameToken.h"
#include <CryRenderer/IRenderer.h>

#define SCRIPT_GAMETOKEN_ALWAYS_CREATE // GameToken.SetValue from Script always creates the token
// #undef SCRIPT_GAMETOKEN_ALWAYS_CREATE     // GameToken.SetValue from Script warns if Token not found

#define DEBUG_GAME_TOKENS
#undef DEBUG_GAME_TOKENS

class IGameTokenIterator : public IGameTokenIt
{
protected:
	IGameTokenIterator(CGameTokenSystem* pGTS)
		: m_nRefCount(0)
		, m_pGTS(pGTS)
	{
		CRY_ASSERT(pGTS);
	}
	virtual ~IGameTokenIterator() {};

private:
	virtual void AddRef()  override { m_nRefCount++; }
	virtual void Release() override { --m_nRefCount; if (m_nRefCount <= 0) { delete this; } }

private:
	int m_nRefCount;

protected:
	CGameTokenSystem* m_pGTS;
};


class CGameTokenIterator : public IGameTokenIterator
{
public:
	CGameTokenIterator(CGameTokenSystem* pGTS) : IGameTokenIterator(pGTS)
	{
		MoveFirst();
	}

	virtual ~CGameTokenIterator() {};

	bool IsEnd()
	{
		if (!m_pGTS || !m_pGTS->m_pGameTokensMap)
			return true;

		return (m_it == m_pGTS->m_pGameTokensMap->end());
	}

	IGameToken* This()
	{
		if (IsEnd())
			return nullptr;
		else
			return m_it->second;
	}

	IGameToken* Next()
	{
		if (IsEnd())
			return nullptr;
		else
		{
			IGameToken* pCurrent = m_it->second;
			std::advance(m_it, 1);
			return pCurrent;
		}
	}

	void MoveFirst()
	{
		if (m_pGTS && m_pGTS->m_pGameTokensMap)
			m_it = m_pGTS->m_pGameTokensMap->begin();
	};

protected: // ---------------------------------------------------
	CGameTokenSystem::GameTokensMap::iterator m_it;
};

#ifdef _GAMETOKENSDEBUGINFO
int CGameTokenSystem::m_CVarShowDebugInfo = 0;
int CGameTokenSystem::m_CVarPosX = 0;
int CGameTokenSystem::m_CVarPosY = 0;
int CGameTokenSystem::m_CVarNumHistoricLines = DBG_HISTORYSIZE;
ICVar* CGameTokenSystem::m_pCVarFilter = nullptr;
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef _GAMETOKENSDEBUGINFO
namespace
{
void AddGameTokenToDebugList(IConsoleCmdArgs* pArgs)
{
	IGameTokenSystem* pGameTokenSystem = gEnv->pGameFramework->GetIGameTokenSystem();
	if (pGameTokenSystem && pArgs->GetArgCount() > 1)
		pGameTokenSystem->AddTokenToDebugList(pArgs->GetArg(1));
}

void RemoveGameTokenFromDebugList(IConsoleCmdArgs* pArgs)
{
	IGameTokenSystem* pGameTokenSystem = gEnv->pGameFramework->GetIGameTokenSystem();
	if (pGameTokenSystem && pArgs->GetArgCount() > 1)
		pGameTokenSystem->RemoveTokenFromDebugList(pArgs->GetArg(1));
}
}
#endif

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem::CGameTokenSystem()
{
	CGameToken::g_pGameTokenSystem = this;
	m_pScriptBind = new CScriptBind_GameToken(this);
	m_pGameTokensMap = new GameTokensMap();
	m_bGoingIntoGame = false;

#ifdef _GAMETOKENSDEBUGINFO
	m_debugHistory.resize(DBG_HISTORYSIZE);
	ClearDebugHistory();
	REGISTER_CVAR2("gt_show", &CGameTokenSystem::m_CVarShowDebugInfo, 0, 0, "Show Game Tokens debug info. 1=screen and log, 2=screen only, 3=log only");
	REGISTER_CVAR2("gt_showPosX", &CGameTokenSystem::m_CVarPosX, 0, 0, "Defines the starting column in screen for game tokens debug info");
	REGISTER_CVAR2("gt_showPosY", &CGameTokenSystem::m_CVarPosY, 0, 0, "Defines the starting line in screen for game tokens debug info");
	REGISTER_CVAR2("gt_showLines", &CGameTokenSystem::m_CVarNumHistoricLines, DBG_HISTORYSIZE, 0, "How many lines is used by the historic list");
	m_pCVarFilter = REGISTER_STRING("gt_showFilter", nullptr, VF_NULL, "In the historic list only shows game tokens that include the filter string");
	REGISTER_COMMAND("gt_AddToDebugList", AddGameTokenToDebugList, VF_CHEAT, "Adds a game token by name to the list of game tokens to be shown on screen");
	REGISTER_COMMAND("gt_RemoveFromDebugList", RemoveGameTokenFromDebugList, VF_CHEAT, "Removes a game token by name from the list of game tokens to be shown on screen");
#endif
}

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem::~CGameTokenSystem()
{
	if (m_pScriptBind)
		delete m_pScriptBind;

	IConsole* pConsole = gEnv->pConsole;
	pConsole->UnregisterVariable("gt_show", true);
	pConsole->UnregisterVariable("gt_showPosX", true);
	pConsole->UnregisterVariable("gt_showPosY", true);
	pConsole->UnregisterVariable("gt_showLines", true);
	pConsole->UnregisterVariable("gt_showFilter", true);
}

//////////////////////////////////////////////////////////////////////////
IGameToken* CGameTokenSystem::SetOrCreateToken(const char* sTokenName, const TFlowInputData& defaultValue)
{
	CRY_ASSERT(sTokenName);
	if (*sTokenName == 0) // empty string
	{
		GameWarning(_HELP("Creating game token with empty name"));
		return nullptr;
	}

#ifdef DEBUG_GAME_TOKENS
	GameTokensMap::iterator iter(m_pGameTokensMap->begin());
	CryLogAlways("GT 0x%p: DEBUG looking for token '%s'", this, sTokenName);
	while (iter != m_pGameTokensMap->end())
	{
		CryLogAlways("GT 0x%p: Token key='%s' name='%s' Val='%s'", this, (*iter).first, (*iter).second->GetName(), (*iter).second->GetValueAsString());
		++iter;
	}
#endif

	CGameToken* pToken = stl::find_in_map(*m_pGameTokensMap, sTokenName, nullptr);
	if (pToken)
	{
		pToken->m_value.SetUnlocked();
		pToken->SetValue(defaultValue);
		pToken->m_value.SetLocked();
		return pToken;
	}

	pToken = new CGameToken;
	pToken->m_name = sTokenName;
	pToken->m_value = defaultValue;
	pToken->m_value.SetLocked();
	(*m_pGameTokensMap)[pToken->m_name.c_str()] = pToken;

	return pToken;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DeleteToken(IGameToken* pToken)
{
	CRY_ASSERT(pToken);
	GameTokensMap::iterator it = m_pGameTokensMap->find(((CGameToken*)pToken)->m_name.c_str());
	if (it != m_pGameTokensMap->end())
	{
#ifdef DEBUG_GAME_TOKENS
		CryLogAlways("GameTokenSystemNew::DeleteToken: About to delete Token 0x%p '%s' val=%s", pToken, pToken->GetName(), pToken->GetValueAsString());
#endif

		m_pGameTokensMap->erase(it);
		delete (CGameToken*)pToken;
	}
}

//////////////////////////////////////////////////////////////////////////
IGameToken* CGameTokenSystem::FindToken(const char* sTokenName)
{
	if (m_pGameTokensMap)
		return stl::find_in_map(*m_pGameTokensMap, sTokenName, nullptr);
	else
		return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RenameToken(IGameToken* pToken, const char* sNewName)
{
	CRY_ASSERT(pToken);
	CGameToken* pCToken = (CGameToken*)pToken;
	GameTokensMap::iterator it = m_pGameTokensMap->find(pCToken->m_name.c_str());
	if (it != m_pGameTokensMap->end())
		m_pGameTokensMap->erase(it);
#ifdef DEBUG_GAME_TOKENS
	CryLogAlways("GameTokenSystemNew::RenameToken: 0x%p '%s' -> '%s'", pCToken, pCToken->m_name, sNewName);
#endif
	pCToken->m_name = sNewName;
	(*m_pGameTokensMap)[pCToken->m_name.c_str()] = pCToken;
}

IGameTokenIt* CGameTokenSystem::GetGameTokenIterator()
{
	return new CGameTokenIterator(this);
}

//////////////////////////////////////////////////////////////////////////
CGameToken* CGameTokenSystem::GetToken(const char* sTokenName)
{
	// Load library if not found.
	CGameToken* pToken = stl::find_in_map(*m_pGameTokensMap, sTokenName, nullptr);
	if (!pToken)
	{
		//@TODO: Load game token lib here.
	}
	return pToken;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RegisterListener(const char* sGameToken, IGameTokenEventListener* pListener, bool bForceCreate, bool bImmediateCallback)
{
	CGameToken* pToken = GetToken(sGameToken);

	if (!pToken && bForceCreate)
	{
		pToken = new CGameToken;
		pToken->m_name = sGameToken;
		pToken->m_value = TFlowInputData(0.0f);
		(*m_pGameTokensMap)[pToken->m_name.c_str()] = pToken;
	}

	if (pToken)
	{
		pToken->AddListener(pListener);

		if (bImmediateCallback)
		{
			pListener->OnGameTokenEvent(EGAMETOKEN_EVENT_CHANGE, pToken);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::UnregisterListener(const char* sGameToken, IGameTokenEventListener* pListener)
{
	CGameToken* pToken = GetToken(sGameToken);
	if (pToken)
	{
		pToken->RemoveListener(pListener);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RegisterListener(IGameTokenEventListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::UnregisterListener(IGameTokenEventListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Notify(EGameTokenEvent event, CGameToken* pToken)
{
	if (gEnv->IsEditing() || m_bGoingIntoGame)
		return;

	for (Listeners::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnGameTokenEvent(event, pToken);
	}
	if (pToken)
		pToken->Notify(event);

#ifdef _GAMETOKENSDEBUGINFO
	if (CGameTokenSystem::m_CVarShowDebugInfo != 0 && pToken)
	{
		bool filterPass = true;
		if (m_pCVarFilter && m_pCVarFilter->GetString() != nullptr && m_pCVarFilter->GetString()[0] != 0)
		{
			stack_string temp1 = m_pCVarFilter->GetString();
			temp1.MakeUpper();
			stack_string temp2 = pToken->GetName();
			temp2.MakeUpper();
			filterPass = strstr(temp2.c_str(), temp1.c_str()) != nullptr;
		}

		if (filterPass)
		{
			int numHistoryLines = GetHistoryBufferSize();
			if (numHistoryLines != m_oldNumHistoryLines)
				ClearDebugHistory();

			if (numHistoryLines > 0)
			{
				if (m_historyEnd >= numHistoryLines)
					m_historyEnd = 0;
				else
				{
					m_historyEnd = (m_historyEnd + 1) % numHistoryLines;
					if (m_historyEnd == m_historyStart)
						m_historyStart = (m_historyStart + 1) % numHistoryLines;
				}

				SDebugHistoryEntry& entry = m_debugHistory[m_historyEnd];
				entry.tokenName = pToken->GetName();
				entry.value = GetTokenDebugString(pToken);
				entry.timeChanged = pToken->GetLastChangeTime();
			}
			if (CGameTokenSystem::m_CVarShowDebugInfo != 2)
				CryLog("---GAMETOKEN CHANGE.  '%s' = '%s'", pToken->GetName(), pToken->GetValueAsString());
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::TriggerTokensAsChanged()
{
	for (GameTokensMap::const_iterator it = m_pGameTokensMap->begin(), itEnd = m_pGameTokensMap->end(); it != itEnd; ++it)
	{
		it->second->TriggerAsChanged(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Serialize(TSerialize ser)
{
	// when writing game tokens we store the per-level tokens (Level.xxx) in a group
	// with the real level name

	TFlowInputData data;
	std::vector<CGameToken*> levelTokens;
	static const char* levelPrefix = "Level.";
	static const size_t prefixLength = 6;
	assert(strlen(levelPrefix) == prefixLength);
	const bool bSaving = ser.IsWriting();

	if (bSaving)
	{
		// Saving.
		if (m_levelToLevelSave)
		{
			IXmlStringData* xmlData = m_levelToLevelSave->getXMLData(5000);
			string strXMLData(xmlData->GetString());
			ser.Value("LTLGameTokensData", strXMLData);
			xmlData->Release();
		}

		int nTokens = 0;
		ser.BeginGroup("GlobalTokens");
		if (!m_pGameTokensMap->empty())
		{
			GameTokensMap::iterator iter = m_pGameTokensMap->begin();
			GameTokensMap::iterator iterEnd = m_pGameTokensMap->end();
			while (iter != iterEnd)
			{
				CGameToken* pToken = iter->second;

				if (0 == strncmp(levelPrefix, pToken->m_name.c_str(), prefixLength))
				{
					levelTokens.push_back(pToken);
				}
				else
				{
					nTokens++;
					ser.BeginGroup("Token");
					ser.Value("name", pToken->m_name);
					ser.Value("flags", pToken->m_nFlags);
					pToken->m_value.Serialize(ser);
					ser.EndGroup();
				}
				++iter;
			}
		}
		ser.Value("count", nTokens);
		ser.EndGroup();

		nTokens = (int)levelTokens.size();
		ser.BeginGroup("LevelTokens");
		ser.Value("count", nTokens);
		if (!levelTokens.empty())
		{
			std::vector<CGameToken*>::iterator iter = levelTokens.begin();
			std::vector<CGameToken*>::iterator iterEnd = levelTokens.end();
			while (iter != iterEnd)
			{
				CGameToken* pToken = *iter;
				{
					ser.BeginGroup("Token");
					ser.Value("name", pToken->m_name);
					ser.Value("flags", pToken->m_nFlags);
					pToken->m_value.Serialize(ser);
					ser.EndGroup();
				}
				++iter;
			}
		}
		ser.EndGroup();
	}
	else
	{
#ifdef _GAMETOKENSDEBUGINFO
		ClearDebugHistory();
#endif

		// Loading.

		{
			string strXMLData;
			ser.Value("LTLGameTokensData", strXMLData);
			m_levelToLevelSave = gEnv->pSystem->LoadXmlFromBuffer(strXMLData.c_str(), strXMLData.length());
		}

		for (int pass = 0; pass < 2; pass++)
		{
			if (pass == 0)
				ser.BeginGroup("GlobalTokens");
			else
				ser.BeginGroup("LevelTokens");

			string tokenName;
			int nTokens = 0;
			ser.Value("count", nTokens);
			uint32 flags = 0;
			for (int i = 0; i < nTokens; i++)
			{
				ser.BeginGroup("Token");
				ser.Value("name", tokenName);
				ser.Value("flags", flags);
				data.Serialize(ser);
				CGameToken* pToken = (CGameToken*)FindToken(tokenName);
				if (pToken)
				{
					pToken->m_value = data;
					pToken->m_nFlags = flags;
				}
				else
				{
					// Create token.
					pToken = (CGameToken*)SetOrCreateToken(tokenName, data);
					pToken->m_nFlags = flags;
				}
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::LoadLibs(const char* sFileSpec)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());
	m_bGoingIntoGame = true;

	ICryPak* pPak = gEnv->pCryPak;
	_finddata_t fd;
	string dir = PathUtil::GetPathWithoutFilename(sFileSpec);
	intptr_t handle = pPak->FindFirst(sFileSpec, &fd);
	if (handle != -1)
	{
		int res = 0;
		do
		{
			_InternalLoadLibrary(PathUtil::Make(dir, fd.name), "GameTokensLibrary");
			res = pPak->FindNext(handle, &fd);
		}
		while (res >= 0);
		pPak->FindClose(handle);
	}

	TriggerTokensAsChanged();

	m_bGoingIntoGame = false;
}

#if 0 // temp leave in - support for level game tokens should be added back
bool CGameTokenSystem::LoadLevelLibrary(const char* sFileSpec, bool bEraseLevelTokens)
{
	if (bEraseLevelTokens)
		RemoveLevelTokens();

	return _InternalLoadLibrary(sFileSpec, "GameTokensLevelLibrary");
}
#endif

void CGameTokenSystem::RemoveLibrary(const char* sPrefix)
{
	if (m_pGameTokensMap)
	{
		string prefixDot(sPrefix);
		prefixDot += ".";

		GameTokensMap::iterator iter = m_pGameTokensMap->begin();
		while (iter != m_pGameTokensMap->end())
		{
			bool isLibToken(iter->second->m_name.find(prefixDot) == 0);
			GameTokensMap::iterator next = iter;
			++next;
			if (isLibToken)
			{
				CGameToken* pToDelete = (*iter).second;
				m_pGameTokensMap->erase(iter);
				SAFE_DELETE(pToDelete);
			}
			iter = next;
		}
		stl::find_and_erase(m_libraries, sPrefix);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Unload()
{
	if (m_pGameTokensMap)
	{
		GameTokensMap::iterator iter = m_pGameTokensMap->begin();
		while (iter != m_pGameTokensMap->end())
		{
			SAFE_DELETE((*iter).second);
			++iter;
		}
	}
	if (!gEnv->IsEditor())
	{
		SAFE_DELETE(m_pGameTokensMap);
	}
	stl::free_container(m_listeners);
	stl::free_container(m_libraries);
#ifdef _GAMETOKENSDEBUGINFO
	ClearDebugHistory();
#endif
}
//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Reset()
{
	if (m_pGameTokensMap)
	{
		Unload();
	}

	if (!m_pGameTokensMap)
	{
		m_pGameTokensMap = new GameTokensMap();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenSystem::_InternalLoadLibrary(const char* filename, const char* tag)
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename);
	if (!root)
	{
		GameWarning(_HELP("Unable to load game token database: %s"), filename);
		return false;
	}

	if (0 != strcmp(tag, root->getTag()))
	{
		GameWarning(_HELP("Not a game tokens library : %s"), filename);
		return false;
	}

	// GameTokens are (currently) not saved with their full path
	// we expand it here to LibName.TokenName
	string libName;
	{
		const char* sLibName = root->getAttr("Name");
		if (sLibName == 0)
		{
			GameWarning("GameTokensLibrary::LoadLibrary: Unable to find LibName in file '%s'", filename);
			return false;
		}
		libName = sLibName;
	}

	// we dont skip already loaded libraries anymore. We need to reload them to be sure that all necessary gametokens are present even if some level has not up-to-date libraries.
	if (!stl::find(m_libraries, libName)) //return true;
		m_libraries.push_back(libName);

	libName += ".";

	int numChildren = root->getChildCount();
	for (int i = 0; i < numChildren; i++)
	{
		XmlNodeRef node = root->getChild(i);

		const char* sName = node->getAttr("Name");
		const char* sType = node->getAttr("Type");
		const char* sValue = node->getAttr("Value");
		const char* sLocalOnly = node->getAttr("LocalOnly");
		int localOnly = atoi(sLocalOnly);

		EFlowDataTypes tokenType = FlowNameToType(sType);
		if (tokenType == eFDT_Any || tokenType == eFDT_Void)
		{
			GameWarning("While loading %s: Unsupported type '%s' for Game Token '%s' (%s:%d)",
				libName.c_str(), sType, sName, node->getTag(), node->getLine());
			continue;
		}

		TFlowInputData initialValue = TFlowInputData::CreateDefaultInitializedForTag((int)tokenType, true);
		initialValue.SetValueWithConversion(string(sValue));

		string fullName(libName);
		fullName += sName;

		IGameToken* pToken = stl::find_in_map(*m_pGameTokensMap, fullName, nullptr);
		if (!pToken)
		{
			pToken = SetOrCreateToken(fullName, initialValue);
			if (pToken && localOnly)
				pToken->SetFlags(pToken->GetFlags() | EGAME_TOKEN_LOCALONLY);
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::SerializeSaveLevelToLevel(const char** ppGameTokensList, uint32 numTokensToSave)
{
	m_levelToLevelSave = gEnv->pSystem->CreateXmlNode("GameTokensLevelToLevel");

	IXmlSerializer* pSerializer = gEnv->pSystem->GetXmlUtils()->CreateXmlSerializer();
	ISerialize* pSer = pSerializer->GetWriter(m_levelToLevelSave);
	TSerialize ser = TSerialize(pSer);

	uint32 numTokensSaved = 0;
	for (uint32 i = 0; i < numTokensToSave; ++i)
	{
		const char* pName = ppGameTokensList[i];
		CGameToken* pToken = GetToken(pName);
		if (pToken)
		{
			numTokensSaved++;
			ser.BeginGroup("Token");
			ser.Value("name", pToken->m_name);
			pToken->m_value.Serialize(ser);
			ser.EndGroup();
		}
		else
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "GameTokenSystem. GameToken %s was not found when trying to serialize (save) level to level", pName ? pName : "<NULL>");
	}
	ser.Value("numTokens", numTokensSaved);

	pSerializer->Release();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::SerializeReadLevelToLevel()
{
	if (!m_levelToLevelSave)
		return;

	IXmlSerializer* pSerializer = gEnv->pSystem->GetXmlUtils()->CreateXmlSerializer();
	ISerialize* pSer = pSerializer->GetReader(m_levelToLevelSave);
	TSerialize ser = TSerialize(pSer);

	uint32 numTokens = 0;
	ser.Value("numTokens", numTokens);
	for (uint32 i = 0; i < numTokens; i++)
	{
		ser.BeginGroup("Token");
		string tokenName;
		ser.Value("name", tokenName);
		CGameToken* pToken = GetToken(tokenName.c_str());
		if (pToken)
			pToken->m_value.Serialize(ser);
		else
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "GameTokenSystem. GameToken '%s' was not found when trying to serialize (read) level to level", tokenName.c_str());
		ser.EndGroup();
	}

	pSerializer->Release();
	m_levelToLevelSave = nullptr; // this frees it
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DumpAllTokens()
{
	GameTokensMap::iterator iter(m_pGameTokensMap->begin());
	int i = 0;
	while (iter != m_pGameTokensMap->end())
	{
		CryLogAlways("#%04d [%s]=[%s]", i, (*iter).second->GetName(), (*iter).second->GetValueAsString());
		++iter;
		++i;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DebugDraw()
{
#ifdef _GAMETOKENSDEBUGINFO

	static bool drawnLastFrame = false;
	float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	if (CGameTokenSystem::m_CVarShowDebugInfo == 0 || CGameTokenSystem::m_CVarShowDebugInfo == 3)
	{
		drawnLastFrame = false;
		return;
	}

	if (!drawnLastFrame)
		ClearDebugHistory();

	drawnLastFrame = true;

	if (gEnv->pConsole->IsOpened())
		return;

	IRenderer* pRenderer = gEnv->pRenderer;
	if (!pRenderer)
		return;

	int linesDrawn = 0;

	{
		TDebugListMap::iterator iter = m_debugList.begin();
		int i = 0;
		while (iter != m_debugList.end())
		{
			CGameToken* pToken = GetToken(iter->c_str());

			DrawToken(iter->c_str(), GetTokenDebugString(pToken), pToken ? pToken->GetLastChangeTime() : CTimeValue(), i);

			++iter;
			++i;
		}
		linesDrawn = i;
	}

	{
		int numHistoryLines = GetHistoryBufferSize();
		if (numHistoryLines > 0)
		{
			string buf;
			IRenderAuxText::Draw2dLabel(20.0f + m_CVarPosX, 20.0f + m_CVarPosY + 12.0f * (float)linesDrawn, 1.2f, color, false, "---------------HISTORY----------------");
			linesDrawn++;
			for (int i = 0; i < numHistoryLines; i++)
			{
				uint index = (m_historyStart + i) % numHistoryLines;
				SDebugHistoryEntry& entry = m_debugHistory[index];
				DrawToken(entry.tokenName, entry.value, entry.timeChanged, linesDrawn + i);
			}
		}
	}

#endif
}

#ifdef _GAMETOKENSDEBUGINFO

//////////////////////////////////////////////////////////////////////////
const char* CGameTokenSystem::GetTokenDebugString(CGameToken* pToken)
{
	if (!pToken)
		return "<TOKEN NOT FOUND>";

	return pToken->GetValueAsString();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::ClearDebugHistory()
{
	m_historyStart = 0;
	m_historyEnd = GetHistoryBufferSize();
	m_oldNumHistoryLines = GetHistoryBufferSize();
	for (int i = 0; i < DBG_HISTORYSIZE; i++)
	{
		m_debugHistory[i].tokenName.clear();
		m_debugHistory[i].value.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DrawToken(const char* pTokenName, const char* pTokenValue, const CTimeValue& timeChanged, int line)
{
	IRenderer* pRenderer = gEnv->pRenderer;
	float notChanged[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float changed[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	float color[] = { 0.f, 0.f, 0.f, 0.f };

	string buf;
	buf.Format("%s = %s", pTokenName, pTokenValue);
	float dt = (gEnv->pTimer->GetFrameStartTime() - timeChanged).GetSeconds();

	const float COLOR_TIME_LAPSE = 10;  // seconds
	{
		float timeNorm = min(1.f, dt / COLOR_TIME_LAPSE);
		for (int i = 0; i < 4; i++)
			color[i] = min(1.f, (timeNorm * notChanged[i] + (1.f - timeNorm) * changed[i]));
	}

	IRenderAuxText::Draw2dLabel(20.0f + m_CVarPosX, 20.0f + m_CVarPosY + 12.0f * (float)line, 1.2f, color, false, "%s", buf.c_str());
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::AddTokenToDebugList(const char* pToken)
{
	string tokenName(pToken);
	m_debugList.insert(tokenName);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RemoveTokenFromDebugList(const char* pToken)
{
	string tokenName(pToken);
	TDebugListMap::iterator iter = m_debugList.find(tokenName);
	if (iter != m_debugList.end())
		m_debugList.erase(iter);
}
#endif

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::GetMemoryStatistics(ICrySizer* s)
{
	SIZER_SUBCOMPONENT_NAME(s, "GameTokenSystem");
	s->Add(*this);
	s->AddObject(m_listeners);
	if (m_pGameTokensMap)
	{
		s->AddHashMap(*m_pGameTokensMap);

		for (GameTokensMap::iterator iter = m_pGameTokensMap->begin(); iter != m_pGameTokensMap->end(); ++iter)
		{
			iter->second->GetMemoryStatistics(s);
		}
	}
}
