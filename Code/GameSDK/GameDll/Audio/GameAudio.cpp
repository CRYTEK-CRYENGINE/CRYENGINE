// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameAudio.h"
#include "GameAudioUtils.h"
#include "Game.h"
#include "GameRules.h"
#include "Scriptbind_GameAudio.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryCore/StlUtils.h>
#include <CryCore/TypeInfo_impl.h>
#include <CryString/StringUtils.h>
#include "Network/Lobby/GameLobby.h"

const CGameAudio::SCommandNameTranslationTableEntry CGameAudio::CommandNamesTranslataionTable[]=
{
	{ "CacheAudio", CGameAudio::eCM_Cache_Audio },
};

const CGameAudio::CAudioSignal::SFlagTableEntry CGameAudio::CAudioSignal::SoundFlagTable[] =
{
	{ "none", eF_None },
	{ "playRandom", eF_PlayRandom },
};

#define XMLSIGNALS_ERROR(xmlFilename, XmlNode, error, ...) CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Reading File: %s Line: %d. " error, xmlFilename, XmlNode->getLine(), __VA_ARGS__ );

#if !defined(_RELEASE)
struct SSignalAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const 
	{ 
		return g_pGame->GetGameAudio()->m_SPSignalsData.audioSignals.size() + g_pGame->GetGameAudio()->m_MPSignalsData.audioSignals.size(); 
	};
	virtual const char* GetValue( int _nIndex ) const 
	{ 
		uint32 numSPSignals = g_pGame->GetGameAudio()->m_SPSignalsData.audioSignals.size();
		uint32 nIndex = (uint32)_nIndex;
		if (nIndex<numSPSignals)
			return g_pGame->GetGameAudio()->m_SPSignalsData.audioSignals[nIndex].m_signalName; 
		else
			return g_pGame->GetGameAudio()->m_MPSignalsData.audioSignals[nIndex-numSPSignals].m_signalName; 
	};
};

static SSignalAutoComplete s_signalAutoComplete;
static CAudioSignalPlayer s_debugPlaySignalOnEntitySignal;

#endif

//////////////////////////////////////////////////////////////////////////

CGameAudio::CGameAudio()
: m_pScriptbind( NULL )
, m_pUtils( NULL )
, m_MPAudioSignalsLoaded ( false )
{
	m_pScriptbind = new CScriptbind_GameAudio;
	m_pUtils = new CGameAudioUtils;

#if !defined(_RELEASE)
	if (gEnv->pConsole)
	{
		REGISTER_COMMAND("PlaySignal", CGameAudio::CmdPlaySignal, 0, "Play an signal");
		REGISTER_COMMAND("PlaySignalOnEntity", CGameAudio::CmdPlaySignalOnEntity, 0, "Play an signal on an entity");
		REGISTER_COMMAND("StopSignalsOnEntity", CGameAudio::CmdStopSignalsOnEntity, 0, "Stop playing signals on an entity");
		REGISTER_COMMAND("PlayAllSignals", CGameAudio::CmdPlayAllSignals, 0, "Play all signals...");
		REGISTER_COMMAND("CacheAudioFile", CGameAudio::CmdCacheAudioFile, 0, "CacheAudio GameHint");
		REGISTER_COMMAND("UnCacheAudioFile", CGameAudio::CmdRemoveCacheAudioFile, 0, "Remove CacheAudio GameHint");
		gEnv->pConsole->RegisterAutoComplete("PlaySignal", & s_signalAutoComplete);
		gEnv->pConsole->RegisterAutoComplete("PlaySignalOnEntity", & s_signalAutoComplete);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CGameAudio::SAudioSignalsData::Unload()
{
	stl::free_container( nameToSignalIndexMap );
	stl::free_container( audioSignals );
	for (uint32 i=0; i<commands.size(); ++i)
	{
		CCommand* pCommand = commands[i];
		delete pCommand;
	}
	stl::free_container( commands );
}


//////////////////////////////////////////////////////////////////////////

CGameAudio::~CGameAudio()
{
	SAFE_DELETE( m_pScriptbind );
	SAFE_DELETE( m_pUtils );
}


//////////////////////////////////////////////////////////////////////////

TAudioSignalID CGameAudio::GetSignalID( const char* pSignalName, bool outputWarning)
{
	CRY_ASSERT(pSignalName && pSignalName[0]);

	TAudioSignalID ID = INVALID_AUDIOSIGNAL_ID;

	TNameToSignalIndexMap::const_iterator iter = m_SPSignalsData.nameToSignalIndexMap.find( CONST_TEMP_STRING(pSignalName) );
	if (iter==m_SPSignalsData.nameToSignalIndexMap.end())
	{
		iter = m_MPSignalsData.nameToSignalIndexMap.find( CONST_TEMP_STRING(pSignalName) );
		if (iter!=m_MPSignalsData.nameToSignalIndexMap.end())
			ID = iter->second + m_SPSignalsData.audioSignals.size(); // MP IDs start after SPs ones.
	}
	else
		ID = iter->second;
		
#if !defined(_RELEASE)
	if(ID==INVALID_AUDIOSIGNAL_ID && outputWarning)
	{
		//CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignal: '%s' not found", pSignalName);
	}	
#endif
	return ID;
}

//////////////////////////////////////////////////////////////////////////

void CGameAudio::Reset()
{
	m_pScriptbind->Reset();
	m_pUtils->Reset();
}

//////////////////////////////////////////////////////////////////////////

void CGameAudio::LoadSignalsFromXMLNode( const char* xmlFilename, const XmlNodeRef& xmlNode, SAudioSignalsData& data  )
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Audio Signals XML (%s)", xmlFilename);

	const uint32 signalCount = xmlNode->getChildCount();


	for (uint32 signalIndex = 0; signalIndex < signalCount; ++signalIndex)
	{
		const XmlNodeRef currentSignalNode = xmlNode->getChild(signalIndex); 
		
		CAudioSignal audioSignal;
		audioSignal.m_signalName = currentSignalNode->getAttr("signal");
		audioSignal.m_flags = CAudioSignal::GetSignalFlagsFromXML(currentSignalNode);
		
		const uint32 signalCommandCount = currentSignalNode->getChildCount();

		size_t nSounds = 0, nCommands = 0;
		for (uint32 commandIndex = 0; commandIndex < signalCommandCount; ++commandIndex)
		{
			const XmlNodeRef currentCommandNode = currentSignalNode->getChild(commandIndex);
			string lineTag = currentCommandNode->getTag();
			if (lineTag == "Sound")
				++nSounds;
			else
				++nCommands;
		}
		//audioSignal.m_sounds.reserve(nSounds);
		audioSignal.m_commands.reserve(nCommands);

		for (uint32 commandIndex = 0; commandIndex < signalCommandCount; ++commandIndex)
		{
			const XmlNodeRef currentCommandNode = currentSignalNode->getChild(commandIndex);

			string lineTag = currentCommandNode->getTag();
			
			bool tagRecognized = false;

			if (lineTag == "Sound")
			{
				//CSound newSound;
				//newSound.m_name = currentCommandNode->getAttr("name");
				//newSound.m_semantic = TranslateNameToSemantic( currentCommandNode->getAttr("semantic") );
				//newSound.m_flags = TranslateXMLToFlags(currentCommandNode);
				//audioSignal.m_sounds.push_back(newSound);
				//
				//EPrecacheResult result = ePrecacheResult_None;
				//
				//// Only always pre-cache if we're running the editor
				//if (gEnv->IsEditor())
				//	result = gEnv->pSoundSystem->Precache(newSound.m_name, 0u, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
				//
				//if(result == ePrecacheResult_Error)
				//{
				//	CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignal: '%s' failed to Precache the sound: '%s'", audioSignal.m_signalName.c_str(), newSound.m_name.c_str());
				//}
				tagRecognized = true;
			}
			else
			{
				CCommand* pCommand = CreateCommand( lineTag );
				if (pCommand)
				{
					tagRecognized = true;
					bool isOk = pCommand->Init( currentCommandNode, this );
					if (isOk)
					{
						data.commands.push_back( pCommand );
						audioSignal.m_commands.push_back( pCommand );
					}
					else
					{
						CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignal: '%s' failed to init a command. tag: '%s'", audioSignal.m_signalName.c_str(), lineTag.c_str());
						delete pCommand;
					}
				}
			}
			
			if (!tagRecognized)
			{
				XMLSIGNALS_ERROR(xmlFilename, xmlNode, "AudioSignal: '%s'   tag: '%s'  unknown", audioSignal.m_signalName.c_str(), lineTag.c_str());
			}
		}

		data.audioSignals.push_back(audioSignal);
		
		std::pair< TNameToSignalIndexMap::iterator, bool > val = data.nameToSignalIndexMap.insert( std::make_pair( audioSignal.m_signalName, data.audioSignals.size()-1 ) );
		if (!val.second)
		{
			XMLSIGNALS_ERROR(xmlFilename, xmlNode, "Duplicated signal: %s", audioSignal.m_signalName.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const CGameAudio::CAudioSignal* CGameAudio::GetAudioSignal( TAudioSignalID signalID )
{
	if (signalID<m_SPSignalsData.audioSignals.size())
	{
		return &m_SPSignalsData.audioSignals[signalID];
	}
	else 
	{
		uint32 MPIndex = signalID - m_SPSignalsData.audioSignals.size();
		if (MPIndex<m_MPSignalsData.audioSignals.size())
			return &m_MPSignalsData.audioSignals[MPIndex];
	} 

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CGameAudio::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddContainer(m_SPSignalsData.audioSignals);
	pSizer->AddContainer(m_SPSignalsData.commands);
	pSizer->AddContainer(m_SPSignalsData.nameToSignalIndexMap);
	pSizer->AddContainer(m_MPSignalsData.audioSignals);
	pSizer->AddContainer(m_MPSignalsData.commands);
	pSizer->AddContainer(m_MPSignalsData.nameToSignalIndexMap);
}

//////////////////////////////////////////////////////////////////////////
CGameAudio::CCommand* CGameAudio::CreateCommand( const char* pCommandName )
{
	ECommand commandType = eCM_NoCommand;
	CCommand* pCommand = NULL;

	for (int i=0; i<eCM_NumCommands && commandType==eCM_NoCommand; ++i)
	{
		if (strcmpi( CommandNamesTranslataionTable[i].name, pCommandName )==0)
			commandType = CommandNamesTranslataionTable[i].command;
	}

	switch (commandType)
	{
		case eCM_Cache_Audio: pCommand = new CCacheAudio; break;
	}

	if (pCommand)
		pCommand->m_command = commandType;

	return pCommand;	
}

//////////////////////////////////////////////////////////////////////////
bool CGameAudio::CCacheAudio::Init( const XmlNodeRef& commandNode, CGameAudio* pGameAudio )
{
	const char* pGameHintName = NULL;
	bool success = commandNode->getAttr("gameHint", &pGameHintName);
	if (!success || !pGameHintName)
		return false;

	success = commandNode->getAttr("cache", m_bCache );
	if (!success)
	{
		return false;
	}

	commandNode->getAttr("now", m_bNow);

	m_gameHint = pGameHintName;
	return true;
}

void CGameAudio::CCacheAudio::Execute() const
{
	/*if (m_bCache)
	{
		CryLog("CGameAudio::CCacheAudio::Execute() is caching audioFile GameHint=%s", m_gameHint.c_str());
		gEnv->pSoundSystem->CacheAudioFile(m_gameHint.c_str(), eAFCT_GAME_HINT);
	}
	else
	{
		CryLog("CGameAudio::CCacheAudio::Execute() is uncaching audioFile GameHint=%s bNow=%d", m_gameHint.c_str(), m_bNow);
		gEnv->pSoundSystem->RemoveCachedAudioFile(m_gameHint.c_str(), m_bNow);
	}*/
}

//////////////////////////////////////////////////////////////////////////
uint32 CGameAudio::CAudioSignal::GetSignalFlagsFromXML(const XmlNodeRef& currentCommandNode)
{
	uint32 flags = eF_None;

	const uint32 flagCount = CRY_ARRAY_COUNT(SoundFlagTable);
	for (uint32 index = 0; index < flagCount; index++)
	{
		const SFlagTableEntry& tableEntry = SoundFlagTable[index];

		if (currentCommandNode->haveAttr(tableEntry.name))
		{
			int enable = 0;
			currentCommandNode->getAttr(tableEntry.name, enable);
			if(enable)
			{
				flags |= tableEntry.flag;
			}
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////

#if !defined(_RELEASE)
void CGameAudio::CmdPlaySignal(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 2)
	{
		TAudioSignalID signalId = g_pGame->GetGameAudio()->GetSignalID(pCmdArgs->GetArg(1), true);
		CAudioSignalPlayer::JustPlay(signalId);
	}
	else
	{
		CryLogAlways("Usage - PlaySignal <signal name>");
	}
}
#endif

#if !defined(_RELEASE)
void CGameAudio::CmdPlaySignalOnEntity(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 3 || pCmdArgs->GetArgCount() == 5)
	{
		const char* pEntityName = pCmdArgs->GetArg(2);
		TAudioSignalID signalId = g_pGame->GetGameAudio()->GetSignalID(pCmdArgs->GetArg(1), true);
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pEntityName);
		if(pEntity)
		{
			s_debugPlaySignalOnEntitySignal.SetSignal(signalId);

			const char* paramName = NULL;
			float paramValue = 0.0f;

			if(pCmdArgs->GetArgCount() == 5)
			{
				paramName = pCmdArgs->GetArg(3);
				paramValue = (float) atof(pCmdArgs->GetArg(4));
			}

			//s_debugPlaySignalOnEntitySignal.Play(pEntity->GetId(), paramName, paramValue);
		}
		else
		{
			CryLogAlways("Unable to find entity '%s'", pEntityName);
		}
	}
	else
	{
		CryLogAlways("Usage - PlaySignalOnEntity <signal name> <entity name> [param name] [param value]");
	}
}
#endif

#if !defined(_RELEASE)
void CGameAudio::CmdStopSignalsOnEntity(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 2)
	{
		const char* pEntityName = pCmdArgs->GetArg(1);
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pEntityName);
		if(pEntity)
		{
			//s_debugPlaySignalOnEntitySignal.Stop(pEntity->GetId());
		}
		else
		{
			CryLogAlways("Unable to find entity '%s'", pEntityName);
		}
	}
	else
	{
		CryLogAlways("Usage - StopSignalsOnEntity <entity name>");
	}
}
#endif

#if !defined(_RELEASE)
void CGameAudio::CmdPlayAllSignals(IConsoleCmdArgs* pCmdArgs)
{
	const int argCount = pCmdArgs->GetArgCount();
	if(argCount > 0 && argCount <= 2)
	{
		int count = s_signalAutoComplete.GetCount();

		if(pCmdArgs->GetArgCount() == 2)
		{
			count= atoi(pCmdArgs->GetArg(1));
		}
		for(int i = 0; i < count; i++)
		{
			TAudioSignalID signalId = g_pGame->GetGameAudio()->GetSignalID(s_signalAutoComplete.GetValue(i), true);
			CAudioSignalPlayer::JustPlay(signalId);
		}
	}
	else
	{
		CryLogAlways("Usage - PlayAllSignals <optional number of signals to play>");
	}
}
#endif

#if !defined(_RELEASE)
void CGameAudio::CmdCacheAudioFile(IConsoleCmdArgs* pCmdArgs)
{
	const int argCount = pCmdArgs->GetArgCount();
	if(argCount == 2)
	{
		//gEnv->pSoundSystem->CacheAudioFile(pCmdArgs->GetArg(1), eAFCT_GAME_HINT);
	}
	else
	{
		CryLogAlways("Usage CacheAudioFile <gamehint>");
	}
}
#endif

#if !defined(_RELEASE)
void CGameAudio::CmdRemoveCacheAudioFile(IConsoleCmdArgs* pCmdArgs)
{
	const int argCount = pCmdArgs->GetArgCount();
	if(argCount == 2)
	{
		//gEnv->pSoundSystem->RemoveCachedAudioFile(pCmdArgs->GetArg(1), false);
	}
	else
	{
		CryLogAlways("Usage UnCacheAudioFile <gamehint>");
	}
}
#endif
