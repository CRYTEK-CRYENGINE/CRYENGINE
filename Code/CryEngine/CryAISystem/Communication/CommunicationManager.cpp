// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommunicationManager.h"
#include "CommunicationPlayer.h"

#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <CryString/StringUtils.h>
#include <CryEntitySystem/IEntitySystem.h>

#include "DebugDrawContext.h"

const char* CCommunicationManager::m_kTargetIsAbove_VariableName = "TargetIsAbove";
const char* CCommunicationManager::m_kTargetIsBelow_VariableName = "TargetIsBelow";

CCommunicationManager::CCommunicationManager(const char* configurationFileName /* = NULL */)
	: m_rootFolder("")
	, m_configurationFileName(configurationFileName)
	, m_playGenID(0)
	, m_globalListeners(10)
{
	m_randomPool.reserve(MaxVariationCount);
}

uint32 CCommunicationManager::GetConfigCount() const
{
	return m_configs.size();
}

const char* CCommunicationManager::GetConfigName(uint32 index) const
{
	assert(index < m_configs.size());

	Configs::const_iterator it = m_configs.begin();
	std::advance(it, (int)index);

	return it->second.name.c_str();
}

CommConfigID CCommunicationManager::GetConfigIDByIndex(uint32 index) const
{
	assert(index < m_configs.size());

	Configs::const_iterator it = m_configs.begin();
	std::advance(it, (int)index);

	return it->first;
}

void CCommunicationManager::LoadConfigurationAndScanRootFolder(const char* rootFolder)
{
	Reset();

	if (!m_configurationFileName.empty())
		LoadCommunicationSettingsXML(m_configurationFileName);

	m_rootFolder = rootFolder;
	ScanFolder(m_rootFolder);
}

void CCommunicationManager::ScanFolder(const char* folderName)
{
	string folder(PathUtil::MakeGamePath(string(folderName)));
	folder += "/";

	string searchString(folder + "*.xml");

	_finddata_t fd;
	intptr_t handle = 0;

	ICryPak* pPak = gEnv->pCryPak;
	handle = pPak->FindFirst(searchString.c_str(), &fd);

	if (handle > -1)
	{
		do
		{
			if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
				continue;

			string completeFilePathAndName(folder + fd.name);
			if (!m_configurationFileName.compareNoCase(completeFilePathAndName))
				continue;

			if (fd.attrib & _A_SUBDIR)
				ScanFolder(completeFilePathAndName.c_str());
			else
				LoadCommunicationSettingsXML(completeFilePathAndName.c_str());

		}
		while (pPak->FindNext(handle, &fd) >= 0);

		pPak->FindClose(handle);
	}
}

void CCommunicationManager::LoadVariables(const XmlNodeRef& rootNode, const char* fileName)
{
	if (m_variablesDeclaration.LoadFromXML(rootNode, fileName))
	{
		m_variables = m_variablesDeclaration.GetDefaults();
		m_variables.ResetChanged(true);
	}
}

void CCommunicationManager::LoadGlobalConfiguration(const XmlNodeRef& rootNode)
{
	if (!rootNode)
		return;

	const char* tagName = rootNode->getTag();

	if (!stricmp(tagName, "GlobalConfiguration"))
	{
		int childCount = rootNode->getChildCount();

		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef childNode = rootNode->getChild(i);

			if (!stricmp(childNode->getTag(), "WWISE"))
			{
				const char* prefixForPlayTriggerAttribute = "prefixForPlayTrigger";
				if (childNode->haveAttr(prefixForPlayTriggerAttribute))
				{
					m_wiseConfiguration.prefixForPlayTrigger = childNode->getAttr(prefixForPlayTriggerAttribute);
				}

				const char* prefixForStopTriggerAttribute = "prefixForStopTrigger";
				if (childNode->haveAttr(prefixForStopTriggerAttribute))
				{
					m_wiseConfiguration.prefixForStopTrigger = childNode->getAttr(prefixForStopTriggerAttribute);
				}

				const char* switchNameForCharacterVoiceAttribute = "switchNameForCharacterVoice";
				if (childNode->haveAttr(switchNameForCharacterVoiceAttribute))
				{
					m_wiseConfiguration.switchNameForCharacterVoice = childNode->getAttr(switchNameForCharacterVoiceAttribute);
				}

				const char* switchNameForCharacterTypeAttribute = "switchNameForCharacterType";
				if (childNode->haveAttr(switchNameForCharacterTypeAttribute))
				{
					m_wiseConfiguration.switchNameForCharacterType = childNode->getAttr(switchNameForCharacterTypeAttribute);
				}
			}
		}
	}
}

bool CCommunicationManager::LoadCommunicationSettingsXML(const char* fileName)
{
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(fileName);
	if (!rootNode)
	{
		AIWarning("Failed to open XML file '%s'...", fileName);

		return false;
	}

	const char* tagName = rootNode->getTag();

	if (!stricmp(tagName, "Communications"))
	{
		int childCount = rootNode->getChildCount();

		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef childNode = rootNode->getChild(i);

			if (!stricmp(childNode->getTag(), "GlobalConfiguration"))
			{
				LoadGlobalConfiguration(childNode);
			}
			else if (!stricmp(childNode->getTag(), "Variables"))
			{
				LoadVariables(childNode, fileName);
			}
			else if (!stricmp(childNode->getTag(), "ChannelConfig"))
			{
				int channelCount = childNode->getChildCount();

				for (int c = 0; c < channelCount; ++c)
				{
					XmlNodeRef channelNode = childNode->getChild(c);
					if (!m_channels.LoadChannel(channelNode, CommChannelID(0)))
						return false;
				}
			}
			else if (!stricmp(childNode->getTag(), "Config"))
			{
				if (!childNode->haveAttr("name"))
				{
					AIWarning("Missing 'name' attribute for 'Config' tag at line %d...",
					          childNode->getLine());

					return false;
				}

				const char* name;
				childNode->getAttr("name", &name);

				std::pair<Configs::iterator, bool> ConfigResult = m_configs.insert(
				  Configs::value_type(GetConfigID(name), CommunicationsConfig()));

				if (!ConfigResult.second)
				{
					if (ConfigResult.first->second.name == name)
						AIWarning("Config '%s' redefinition at line %d...", name, childNode->getLine());
					else
						AIWarning("Config name '%s' hash collision!", name);

					return false;
				}

				CommunicationsConfig& config = ConfigResult.first->second;
				config.name = name;

				int commCount = childNode->getChildCount();

				for (int c = 0; c < commCount; ++c)
				{
					XmlNodeRef commNode = childNode->getChild(c);

					if (!stricmp(commNode->getTag(), "Communication"))
					{
						SCommunication comm;
						if (!LoadCommunication(commNode, comm))
							return false;

						std::pair<Communications::iterator, bool> iresult = config.comms.insert(
						  Communications::value_type(GetCommunicationID(comm.name.c_str()), comm));

						if (!iresult.second)
						{
							if (iresult.first->second.name == comm.name)
								AIWarning("Communication '%s' redefinition at line %d while parsing Communication XML '%s'...",
								          name, commNode->getLine(), fileName);
							else
								AIWarning("Communication name '%s' hash collision! (existing name=%s)", comm.name.c_str(), iresult.first->second.name.c_str());

							return false;
						}
					}
					else
					{
						AIWarning(
						  "Unexpected tag '%s' found at line %d while parsing Communication XML '%s'...",
						  commNode->getTag(), commNode->getLine(), fileName);
					}
				}
			}
		}

		return true;
	}
	else
	{
		AIWarning(
		  "Unexpected tag '%s' found at line %d while parsing Communication XML '%s'...",
		  rootNode->getTag(), rootNode->getLine(), fileName);
	}

	return false;
}

bool FlagAttr(const XmlNodeRef& node, const char* attribute, uint32& flags, uint32 flag)
{
	XMLUtils::BoolType flagValue = XMLUtils::GetBoolType(node, attribute, (flags & flag) ? XMLUtils::True : XMLUtils::False);

	if (flagValue == XMLUtils::Invalid)
	{
		AIWarning("Invalid value for attribute '%s' tag '%s' at line %d...",
		          attribute, node->getTag(), node->getLine());

		return false;
	}

	flags &= ~flag;
	if (flagValue == XMLUtils::True)
		flags |= flag;

	return true;
}

bool FinishMethod(const XmlNodeRef& node, const char* attribute, uint32& flags)
{
	if (node->haveAttr(attribute))
	{
		const char* finishMethod;
		node->getAttr(attribute, &finishMethod);

		flags &= ~SCommunication::FinishAll;

		stack_string methodList(finishMethod);
		methodList.MakeLower();

		int start = 0;
		stack_string method = methodList.Tokenize(",", start);

		while (!method.empty())
		{
			method.Trim();

			if (method == "animation")
				flags |= SCommunication::FinishAnimation;
			else if (method == "sound")
				flags |= SCommunication::FinishSound;
			else if (method == "voice")
				flags |= SCommunication::FinishVoice;
			else if (method == "timeout")
				flags |= SCommunication::FinishTimeout;
			else if (method == "all")
				flags |= SCommunication::FinishAll;
			else
			{
				AIWarning("Invalid value '%s' for attribute '%s' tag '%s' at line %d...",
				          method.c_str(), attribute, node->getTag(), node->getLine());

				return false;
			}

			method = methodList.Tokenize(",", start);
		}
	}

	return true;
}

bool Blocking(const XmlNodeRef& node, const char* attribute, uint32& flags)
{
	if (node->haveAttr(attribute))
	{
		const char* blocking;
		node->getAttr(attribute, &blocking);

		flags &= ~SCommunication::BlockAll;

		stack_string methodList(blocking);
		methodList.MakeLower();

		int start = 0;
		stack_string method = methodList.Tokenize(",", start);

		while (!method.empty())
		{
			method.Trim();

			if (method == "movement")
				flags |= SCommunication::BlockMovement;
			else if (method == "fire")
				flags |= SCommunication::BlockFire;
			else if (method == "all")
				flags |= SCommunication::BlockAll;
			else if (method == "none")
			{
				flags &= ~SCommunication::BlockAll;
			}
			else
			{
				AIWarning("Invalid value '%s' for attribute '%s' tag '%s' at line %d...",
				          method.c_str(), attribute, node->getTag(), node->getLine());

				return false;
			}

			method = methodList.Tokenize(",", start);
		}
	}

	return true;
}

bool ChoiceMethod(const XmlNodeRef& node, const char* attribute, SCommunication::EVariationChoiceMethod& method)
{
	const char* choiceMethod;

	if (node->haveAttr(attribute))
	{
		node->getAttr(attribute, &choiceMethod);

		if (!stricmp(choiceMethod, "random"))
			method = SCommunication::Random;
		else if (!stricmp(choiceMethod, "sequence"))
			method = SCommunication::Sequence;
		else if (!stricmp(choiceMethod, "randomsequence"))
			method = SCommunication::RandomSequence;
		else if (!stricmp(choiceMethod, "match"))
			method = SCommunication::Match;
		else
		{
			AIWarning("Invalid value for attribute '%s' tag '%s' at line %d...",
			          attribute, node->getTag(), node->getLine());

			return false;
		}
	}
	return true;
}

bool AnimationType(const XmlNodeRef& node, const char* attribute, uint32& flags)
{
	if (node->haveAttr(attribute))
	{
		const char* animationType;
		node->getAttr(attribute, &animationType);

		flags &= ~SCommunication::AnimationAction;

		if (!stricmp(animationType, "action"))
			flags |= SCommunication::AnimationAction;
		else if (stricmp(animationType, "signal"))
		{
			AIWarning("Invalid value for attribute '%s' tag '%s' at line %d...",
			          attribute, node->getTag(), node->getLine());

			return false;
		}
	}

	return true;
}

bool CCommunicationManager::LoadCommunication(const XmlNodeRef& commNode, SCommunication& comm)
{
	if (!commNode->haveAttr("name"))
	{
		AIWarning("Missing 'name' attribute for 'Communication' tag at line %d...",
		          commNode->getLine());

		return false;
	}

	const char* name = 0;
	commNode->getAttr("name", &name);

	comm.name = name;
	comm.choiceMethod = SCommunication::RandomSequence;
	comm.responseChoiceMethod = SCommunication::RandomSequence;

	if (!ChoiceMethod(commNode, "choiceMethod", comm.choiceMethod))
		return false;

	if (commNode->haveAttr("responseName"))
	{
		const char* responseName;
		commNode->getAttr("responseName", &responseName);
		comm.responseID = GetCommunicationID(responseName);
	}
	else
	{
		comm.responseID = CommID(0);
	}

	if (!ChoiceMethod(commNode, "responseChoiceMethod", comm.responseChoiceMethod))
		return false;

	SCommunicationVariation deflt;
	deflt.flags = SCommunication::FinishAll;

	if (!LoadVariation(commNode, deflt))
		return false;

	int variationCount = commNode->getChildCount();

	comm.hasAnimation = false;
	comm.forceAnimation = false;

	commNode->getAttr("forceAnimation", comm.forceAnimation);

	for (int i = 0; i < variationCount; ++i)
	{
		XmlNodeRef varNode = commNode->getChild(i);

		if (stricmp(varNode->getTag(), "Variation"))
		{
			AIWarning("Unexpected tag '%s' found at line %d...", varNode->getTag(), varNode->getLine());

			return false;
		}

		SCommunicationVariation variation(deflt);
		if (!LoadVariation(varNode, variation))
			return false;

		if (!variation.animationName.empty())
			comm.hasAnimation = true;

		comm.variations.push_back(variation);

		if (comm.variations.size() == MaxVariationCount)
		{
			if (variationCount > MaxVariationCount)
			{
				AIWarning("Maximum number of variations reached for '%s' found at line %d...", commNode->getTag(),
				          commNode->getLine());
			}

			break;
		}
	}

	return true;
}

bool CCommunicationManager::LoadVariation(const XmlNodeRef& varNode, SCommunicationVariation& variation)
{
	if (varNode->haveAttr("animationName"))
	{
		const char* animationName;
		varNode->getAttr("animationName", &animationName);
		variation.animationName = animationName;
	}

	if (varNode->haveAttr("soundName"))
	{
		const char* soundName;
		varNode->getAttr("soundName", &soundName);
		variation.soundName = soundName;
	}

	if (varNode->haveAttr("voiceName"))
	{
		const char* voiceName;
		varNode->getAttr("voiceName", &voiceName);
		variation.voiceName = voiceName;
	}

	if (!FlagAttr(varNode, "lookAtTarget", variation.flags, SCommunication::LookAtTarget))
		return false;

	if (!FinishMethod(varNode, "finishMethod", variation.flags))
		return false;

	if (!Blocking(varNode, "blocking", variation.flags))
		return false;

	if (!AnimationType(varNode, "animationType", variation.flags))
		return false;

	if (varNode->haveAttr("timeout"))
	{
		float timeout = 0.0f;
		if (varNode->getAttr("timeout", timeout))
			variation.timeout = timeout;
	}

	if (varNode->haveAttr("condition"))
	{
		stack_string condition = varNode->getAttr("condition");
		if (condition.empty())
			return false;

		variation.condition.reset(new Variables::Expression(condition, m_variablesDeclaration));
	}

	return true;
}

void CCommunicationManager::Reload()
{
	Reset();
	m_testManager.Reset();

	m_configs.clear();

	m_channels.Clear();
	m_player.Clear();

	if (!m_configurationFileName.empty())
		LoadCommunicationSettingsXML(m_configurationFileName);

	ScanFolder(m_rootFolder);
}

void CCommunicationManager::Reset()
{
	m_variablesDeclaration = Variables::Declarations();
	{
		stl::reconstruct(m_variables);
	}

	m_channels.Reset();
	m_player.Reset();

	ResetHistory();

	m_playing.clear();
	m_orderedQueue.clear();
	m_unorderedQueue.clear();

	m_restrictedActors.clear();
	m_actorPropertiesMap.clear();

	m_debugHistoryMap.clear();
	//Don't clear the stats, so they don't get reset on level load
	//m_debugStatisticsMap.clear();

	m_testManager.Reset();

	stl::free_container(m_unfinalized);
}

void CCommunicationManager::ResetHistory()
{
	Configs::iterator it = m_configs.begin();
	Configs::iterator end = m_configs.end();

	for (; it != end; ++it)
	{
		CommunicationsConfig& config = it->second;

		Communications::iterator cit = config.comms.begin();
		Communications::iterator cend = config.comms.end();

		for (; cit != cend; ++cit)
		{
			SCommunication& comm = cit->second;
			comm.history = SCommunication::History();
		}
	}
}

void CCommunicationManager::Update(float updateTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_channels.Update(updateTime);
	m_player.Update(updateTime);
	m_testManager.Update(updateTime);

	UpdateActorRestrictions(updateTime);

	UpdateQueue(m_orderedQueue, updateTime);
	UpdateQueue(m_unorderedQueue, updateTime);

	ProcessQueues();

	CullPlayingCommunications();

	if (gAIEnv.CVars.DebugDrawCommunication > 1 && gAIEnv.CVars.DebugDraw > 0 && gAIEnv.CVars.DebugDrawCommunication != 6)
		UpdateDebugHistory(updateTime);
}

///Looks for an existing record in the debug history, and either inserts a new record if not found, or updates an existing one
void CCommunicationManager::UpdateDebugHistoryEntry(IEntity* pEntity, const CommDebugEntry& entryToUpdate)
{
	std::vector<CommDebugEntry>& targetValues = m_debugHistoryMap[pEntity->GetId()];

	std::vector<CommDebugEntry>::iterator existingEntry = std::find_if(targetValues.begin(), targetValues.end(), FindMatchingRecord(entryToUpdate));

	if (existingEntry != targetValues.end())
	{
		existingEntry->m_type = entryToUpdate.m_type;

		existingEntry->UpdateChannelInfo(entryToUpdate);

		//Reinitialize this entry's display status as its now being rerun
		existingEntry->m_displayed = false;
	}
	else
		targetValues.push_back(entryToUpdate);

}

///Updates the time displayed of all tracked debug records
void CCommunicationManager::UpdateDebugHistory(float updateTime)
{
	for (TDebugMapHistory::iterator iter(m_debugHistoryMap.begin()); iter != m_debugHistoryMap.end(); ++iter)
	{
		std::vector<CommDebugEntry>& entityEntries = iter->second;

		///Update times and display status for existing entries
		for (std::vector<CommDebugEntry>::iterator entryIt = entityEntries.begin(); entryIt != entityEntries.end(); ++entryIt)
		{
			CommDebugEntry& entry = *entryIt;

			entry.m_displayed = (entryIt->m_timeTracked > 2.0f);
			entry.m_timeTracked += updateTime;

		}

		///Clear out old records that haven't been played for awhile
		CommDebugEntry searchEntry;
		searchEntry.m_timeTracked = 20.0f;
		entityEntries.erase(std::remove_if(entityEntries.begin(), entityEntries.end(), FindMatchingRecord(searchEntry)), entityEntries.end());
	}

	///Finds empty entity entries in the debug map
	TDebugMapHistory::iterator iter = m_debugHistoryMap.begin();
	TDebugMapHistory::iterator endIter = m_debugHistoryMap.end();

	for (; iter != endIter; )
	{
		if (iter->second.empty())
			m_debugHistoryMap.erase(iter++);
		else
			++iter;
	}
}

void CCommunicationManager::DebugDraw()
{
	typedef std::map<IEntity*, string> TDebugMap;
	TDebugMap debugInfoMap;
	string temp;

	// Write playing
	for (PlayingCommunications::const_iterator it(m_playing.begin()); it != m_playing.end(); ++it)
	{
		CommPlayID playID = it->first;
		const PlayingCommunication& playingComm = it->second;

		CommID commID = m_player.GetCommunicationID(playID);
		if (const char* commName = GetCommunicationName(commID))
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(playingComm.actorID))
			{
				CommChannelID channelID(0);

				if (playingComm.channel)
					channelID = playingComm.channel->GetID();

				temp.Format("P: %s: %u\n", commName, channelID.id);
				debugInfoMap[pEntity].append(temp);

				CommDebugEntry newEntry(commName, playID, "Playing", channelID);
				UpdateDebugHistoryEntry(pEntity, newEntry);

			}
			else
			{
				CDebugDrawContext dc;
				dc->Draw2dLabel(40.0f, 40.0f, 3.0f, ColorB(255, 128, 0), false, "No entity found for playing comm: %s", commName);
			}
		}
		else
		{
			CDebugDrawContext dc;
			dc->Draw2dLabel(40.0f, 50.0f, 3.0f, ColorB(255, 128, 0), false, "Abandoned communication in playing set: playID[%d] for entityID[%d]", playID.id, playingComm.actorID);
		}
	}

	// Write ordered queued
	for (QueuedCommunications::const_iterator it(m_orderedQueue.begin()); it != m_orderedQueue.end(); ++it)
	{
		const QueuedCommunication& queuedComm = *it;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(queuedComm.request.actorID);
		const char* commName = GetCommunicationName(queuedComm.request.commID);

		if (pEntity)
		{
			temp.Format("Q[%" PRI_PTRDIFF_T "] %s\n", std::distance<QueuedCommunications::const_iterator>(m_orderedQueue.begin(), it), commName);
			debugInfoMap[pEntity].append(temp);

			CommDebugEntry newEntry(commName, queuedComm.playID, "QueuedOrdered", queuedComm.request.channelID);
			UpdateDebugHistoryEntry(pEntity, newEntry);

		}
	}

	// Write unordered queued
	for (QueuedCommunications::const_iterator it(m_unorderedQueue.begin()); it != m_unorderedQueue.end(); ++it)
	{
		const QueuedCommunication& queuedComm = *it;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(queuedComm.request.actorID);
		const char* commName = GetCommunicationName(queuedComm.request.commID);

		if (pEntity)
		{
			temp.Format("U: %s\n", commName);
			debugInfoMap[pEntity].append(temp);

			CommDebugEntry newEntry(commName, queuedComm.playID, "QueuedOrdered", queuedComm.request.channelID);
			UpdateDebugHistoryEntry(pEntity, newEntry);

		}
	}

	// Draw the text for all active comms for each entity as a 3d label near their location
	CDebugDrawContext dc;
	for (TDebugMap::const_iterator iter(debugInfoMap.begin()); iter != debugInfoMap.end(); ++iter)
	{
		IEntity* pEntity = iter->first;
		string text(iter->second);

		dc->Draw3dLabel(pEntity->GetWorldPos() + Vec3(0, 0, 0.2f), 1.2f, "%s", text.c_str());
	}

	// Draw the history of comms for each entity as 2d labels
	if (gAIEnv.CVars.DebugDrawCommunication > 1 && gAIEnv.CVars.DebugDrawCommunication != 6)
	{
		float offset = 0.0f;

		for (TDebugMapHistory::iterator iter(m_debugHistoryMap.begin()); iter != m_debugHistoryMap.end(); ++iter)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

			CRY_ASSERT(pEntitySystem);

			IEntity* pEntity = pEntitySystem->GetEntity(iter->first);
			std::vector<CommDebugEntry>& entityEntries = iter->second;

			if (!pEntity)
			{
				CryLogAlways("CommunicationManager::DisplayDebugHistory: Entity no longer valid with ID:%i Skipping", iter->first);
				continue;
			}

			dc->Draw2dLabel(40.0f, 80.0f + offset, 2.0f, ColorB(0, 255, 0), false, "Communication History for ENTITY:%s", pEntity->GetName());
			offset += 22;

			int numberDisplayed = 0;
			for (std::vector<CommDebugEntry>::reverse_iterator entryIt = entityEntries.rbegin();
			     entryIt != entityEntries.rend() && numberDisplayed < gAIEnv.CVars.DebugDrawCommunicationHistoryDepth;
			     ++entryIt, ++numberDisplayed)
			{
				CommDebugEntry& entry = *entryIt;
				ColorB color = ColorB(255, 128, 0);
				string status = "Done";
				//If this entry is active to the history then give it a different color and use its type
				if (!entry.m_displayed)
				{
					color = ColorB(255, 255, 0);
					status = entry.m_type;
				}

				string channels;

				if (!entry.m_channels.empty())
				{
					for (std::set<CommChannelID>::iterator channelIt = entry.m_channels.begin(); channelIt != entry.m_channels.end(); ++channelIt)
					{
						if (channels != "")
							channels += ",";

						const string& channelName = m_channels.GetChannelParams(*channelIt).name;
						if (channelName == "")
							channels += "Unknown";
						else
							channels += channelName;
					}
				}

				dc->Draw2dLabel(60.0f, 80.0f + offset, 2.0f, color, false, "Communication: %s Status: %s Channels:%s", entry.m_commName.c_str(), status.c_str(), channels.c_str());

				offset += 20;
			}
		}
	}

}

CommChannelID CCommunicationManager::GetChannelID(const char* name) const
{
	return m_channels.GetChannelID(name);
}

size_t CCommunicationManager::GetPriority(const CommPlayID& playID) const
{
	PlayingCommunications::const_iterator playIt = m_playing.find(playID);

	if (playIt != m_playing.end())
	{
		const PlayingCommunication& playingComm = playIt->second;

		return playingComm.channel->GetPriority();
	}

	//Assuming the playID is no longer valid, assume priority 0
	return 0;
}

CommConfigID CCommunicationManager::GetConfigID(const char* name) const
{
	return CommConfigID(CryStringUtils::CalculateHashLowerCase(name));
}

CommID CCommunicationManager::GetCommunicationID(const char* name) const
{
	return CommID(CCrc32::ComputeLowercase(name));
}

const char* CCommunicationManager::GetCommunicationName(const CommID& communicationID) const
{
	Configs::const_iterator cit = m_configs.begin();
	Configs::const_iterator cend = m_configs.end();

	for (; cit != cend; ++cit)
	{
		const CommunicationsConfig& config = cit->second;

		Communications::const_iterator commIt = config.comms.find(communicationID);
		if (commIt == config.comms.end())
			continue;

		const SCommunication& comm = commIt->second;
		return comm.name.c_str();
	}

	return 0;
}

const char* CCommunicationManager::GetConfigCommunicationNameByIndex(const CommConfigID& configID, uint32 index) const
{
	Configs::const_iterator configIt = m_configs.find(configID);

	if (configIt != m_configs.end())
	{
		const CommunicationsConfig& config = configIt->second;

		assert(index < config.comms.size());

		Communications::const_iterator it = config.comms.begin();
		std::advance(it, (int)index);

		return it->second.name.c_str();
	}

	return 0;
}

uint32 CCommunicationManager::GetConfigCommunicationCount(const CommConfigID& configID) const
{
	Configs::const_iterator configIt = m_configs.find(configID);

	uint32 communicationCount = 0;

	if (configIt != m_configs.end())
	{
		const CommunicationsConfig& config = configIt->second;
		communicationCount = config.comms.size();
	}

	return communicationCount;
}

bool CCommunicationManager::CanCommunicationPlay(const SCommunicationRequest& request, float* estimatedWaitTime)
{
	if (estimatedWaitTime)
		*estimatedWaitTime = FLT_MAX;

	CommunicationChannel::Ptr channel = m_channels.GetChannel(request.channelID, request.actorID);
	if (!channel)
		return false;

	if (channel->IsFree())
	{
		if (estimatedWaitTime)
			*estimatedWaitTime = 0.0f;

		return true;
	}

	return false;
}

bool CCommunicationManager::CanForceAnimation(const SCommunication& comm, const SCommunicationRequest& request)
{
	//If this comm doesn't support animation forcing or the request specified to skip the animation, then return
	if (!comm.forceAnimation || request.skipCommAnimation)
		return false;

	//Assumes active communications of lower priority then queried request have already been flushed out of the system.

	//Otherwise check for playing communications to see if any of them have an animation. If so then return false.
	PlayingCommunications::const_iterator playingIt = m_playing.begin();
	PlayingCommunications::const_iterator playingEnd = m_playing.end();
	for (; playingIt != playingEnd; ++playingIt)
	{
		const PlayingCommunication& playingComm = playingIt->second;

		if (request.actorID == playingComm.actorID && !playingComm.animName.empty())
			return false;

	}

	//Also need to check for unfinalized animations.
	CommunicationVec::const_iterator unfinalizedIt = m_unfinalized.begin();
	CommunicationVec::const_iterator unfinalizedEnd = m_unfinalized.end();
	for (; unfinalizedIt != unfinalizedEnd; ++unfinalizedIt)
	{
		const PlayingCommunication& unfinalizedComm = *unfinalizedIt;

		if (request.actorID == unfinalizedComm.actorID && !unfinalizedComm.animName.empty())
			return false;
	}

	return true;

}

CommPlayID CCommunicationManager::PlayCommunication(SCommunicationRequest& request)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Configs::iterator cit = m_configs.find(request.configID);
	if (cit == m_configs.end())
		return CommPlayID(0);

	CommunicationsConfig& config = cit->second;
	Communications::iterator commIt = config.comms.find(request.commID);
	if (commIt == config.comms.end())
		return CommPlayID(0);

	SCommunication& comm = commIt->second;

	// If this is an animated communication
	if (comm.hasAnimation)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.actorID);
		IAIObject* pAIObject = pEntity ? pEntity->GetAI() : 0;

		if (IAIActorProxy* aiProxy = pAIObject ? pAIObject->GetProxy() : 0)
		{
			// Ensure no SO transition is in progress
			if (aiProxy->IsPlayingSmartObjectAction())
				return CommPlayID(0);
		}
	}

	CommunicationChannel::Ptr channel = m_channels.GetChannel(request.channelID, request.actorID);

	if (channel)
	{
		++m_playGenID.id;
		if (!m_playGenID)
			++m_playGenID.id;

		if (gAIEnv.CVars.RecordCommunicationStats > 0)
		{
			//Record the request here if statistics are being generated
			CommDebugCount& commCount = m_debugStatisticsMap[CommKeys(request.configID, request.commID)];
			++commCount.requestCount;
		}

		FlushCommunications(request, channel);

		SActorReadabilitySettings actorSettings;
		GetActorData(request.actorID, actorSettings);

		//Skip animations for this request if actor ignores them
		if (actorSettings.m_bIgnoreAnimations || IsAnimationRestricted(request.actorID))
		{
			request.skipCommAnimation = true;
		}
		//Only skip communication sounds for channels that don't ignore actor restrictions.
		if ((actorSettings.m_bIgnoreVoice || IsVoiceRestricted(request.actorID)) && !channel->IgnoresActorSilence())
		{
			request.skipCommSound = true;
		}

		//If a combination of request settings no longer has a component, then don't bother to try playing/queueing it.
		if (request.skipCommAnimation && request.skipCommSound)
			return CommPlayID(0);

		bool forceAnimations = CanForceAnimation(comm, request);

		// Plays communication if the channel is free or forcing animations.
		if (forceAnimations || channel->IsFree())
		{
			//Make sure to always flush the queues of lower priority items
			FlushQueue(m_orderedQueue, channel, request.actorID);
			FlushQueue(m_unorderedQueue, channel, request.actorID);

			//If the channel is not free, but the comm forcibly plays the animation, then request skip sound portion
			if (forceAnimations && !channel->IsFree())
			{
				request.skipCommSound = true;
			}

			Play(m_playGenID, request, channel, comm);
		}
		else
		{
			//Make sure queue not insert into gets flushed as well
			if (request.ordering == SCommunicationRequest::Ordered)
			{
				FlushQueue(m_orderedQueue, channel, request.actorID);
				FlushQueue(m_unorderedQueue, channel, request.actorID);

				Queue(m_orderedQueue, m_playGenID, request);
			}
			else
			{
				FlushQueue(m_orderedQueue, channel, request.actorID);
				FlushQueue(m_unorderedQueue, channel, request.actorID);

				Queue(m_unorderedQueue, m_playGenID, request);
			}
		}

		return m_playGenID;
	}

	return CommPlayID(0);
}

void CCommunicationManager::FlushCommunications(const SCommunicationRequest& request, CommunicationChannel::Ptr channel)
{
	//Need to stop every playing communication with a lower priority. Assume ordered, since previous higher entries would clear out ones earlier in the queue
	PlayingCommunications::iterator pit = m_playing.begin();
	PlayingCommunications::iterator pend = m_playing.end();
	while (pit != pend)
	{
		const CommunicationChannel::Ptr targetChannel = pit->second.channel;
		if (channel->GetPriority() > targetChannel->GetPriority())
		{
			bool commRemoved = false;
			if (channel->GetType() == SCommunicationChannelParams::Personal)
			{
				if (request.actorID == pit->second.actorID)
				{
					commRemoved = true;
				}
			}
			//For group/global assume it should be replaced
			else
			{
				commRemoved = true;
			}

			if (commRemoved)
			{
				PlayingCommunications::iterator stopIt = pit++;
				StopCommunication(stopIt->first);

				//If the channel is no longer playing a communication, but still has an imposed silence time, then reset the channel silence to the flush time now
				if (!channel->IsOccupied() && !channel->IsFree())
					channel->ResetSilence();

				continue;
			}
		}

		++pit;
	}

	//Need to stop unfinalized communications with a lower priority. Assume ordered, since previous higher entries would clear out ones earlier in the queue
	for (size_t commIndex = 0; commIndex < m_unfinalized.size(); )
	{
		PlayingCommunication& comm = m_unfinalized[commIndex];

		const CommunicationChannel::Ptr targetChannel = comm.channel;
		if (channel->GetPriority() > targetChannel->GetPriority())
		{
			bool commRemoved = false;
			if (channel->GetType() == SCommunicationChannelParams::Personal)
			{
				if (request.actorID == comm.actorID)
				{
					commRemoved = true;
				}
			}
			//For group/global assume it should be replaced
			else
			{
				commRemoved = true;
			}

			if (commRemoved)
			{
				//Make sure to flush the animation signal/action of the target actor if the new communication has an animation
				if (!comm.animName.empty())
				{
					SendAnimationGraphReset(request.actorID);
				}
				//If the channel is no longer playing a communication, but still has an imposed silence time, then reset the silence to the flush time now
				if (!channel->IsOccupied() && !channel->IsFree())
					channel->ResetSilence();

				m_unfinalized[commIndex] = m_unfinalized.back();
				m_unfinalized.pop_back();
				continue;
			}
		}
		//Check the anim graph state against this animation. If it doesn't match, then this unfinalized comm is now finalized and can be removed.
		else if (!comm.animName.empty() && !CheckAnimationGraphState(request.actorID, comm.animName))
		{
			m_unfinalized[commIndex] = m_unfinalized.back();
			m_unfinalized.pop_back();
			continue;
		}

		++commIndex;
	}
}

bool CCommunicationManager::CheckAnimationGraphState(EntityId actorId, const string& name)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(actorId);
	IAIObject* aiObject = entity ? entity->GetAI() : 0;

	if (IAIActorProxy* aiProxy = aiObject ? aiObject->GetProxy() : 0)
	{
		IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();

		return commHandler->IsInAGState(name.c_str());
	}

	return false;
}

void CCommunicationManager::SendAnimationGraphReset(EntityId actorId)
{

	IEntity* entity = gEnv->pEntitySystem->GetEntity(actorId);
	IAIObject* aiObject = entity ? entity->GetAI() : 0;

	if (IAIActorProxy* aiProxy = aiObject ? aiObject->GetProxy() : 0)
	{
		IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();

		commHandler->ResetAnimationState();
	}

}

void CCommunicationManager::StopCommunication(const CommPlayID& playID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_player.IsPlaying(playID))
	{
		PlayingCommunications::iterator it = m_playing.find(playID);
		assert(it != m_playing.end());

		PlayingCommunication& playing = it->second;

		if (playing.eventListener)
			playing.eventListener->OnCommunicationEvent(CommunicationCancelled, playing.actorID, playID);

		UpdateGlobalListeners(CommunicationCancelled, playing.actorID, m_player.GetCommunicationID(playID));

		if (gAIEnv.CVars.DebugDrawCommunication == 5)
		{
			CommID commID = m_player.GetCommunicationID(playID);
			CryLogAlways("CommunicationManager::StopCommunication: %s[%u] as playID[%u]", GetCommunicationName(commID), commID.id, playID.id);
		}

		//Erasing first will corrupt the playID handle.
		//m_playing.erase(it); // erase before stopping - to simplify handling in the OnCommunicationFinished

		m_player.Stop(playID);
	}

	if (RemoveFromQueue(m_orderedQueue, playID))
		return;

	if (RemoveFromQueue(m_unorderedQueue, playID))
		return;
}

bool CCommunicationManager::IsPlaying(const CommPlayID& playID, float* timeRemaining) const
{
	return m_player.IsPlaying(playID, timeRemaining);
}

bool CCommunicationManager::IsQueued(const CommPlayID& playID, float* estimatedWaitTime) const
{
	QueuedCommunications::const_iterator oit = m_orderedQueue.begin();
	QueuedCommunications::const_iterator oend = m_orderedQueue.end();

	for (; oit != oend; ++oit)
	{
		if (oit->playID == playID)
			return true;
	}

	QueuedCommunications::const_iterator uit = m_unorderedQueue.begin();
	QueuedCommunications::const_iterator uend = m_unorderedQueue.end();

	for (; uit != uend; ++uit)
	{
		if (uit->playID == playID)
			return true;
	}

	return false;
}

void CCommunicationManager::OnCommunicationFinished(const CommPlayID& playID, uint8 stateFlags)
{
	PlayingCommunications::iterator it = m_playing.find(playID);
	if (it != m_playing.end())
	{
		PlayingCommunication& playing = it->second;

		if (!playing.skipSound)
		{
			assert(!playing.channel->IsFree());
			playing.channel->Occupy(false, playing.minSilence);
		}

		if (playing.eventListener)
			playing.eventListener->OnCommunicationEvent(CommunicationFinished, playing.actorID, playID);

		UpdateGlobalListeners(CommunicationFinished, playing.actorID, m_player.GetCommunicationID(playID));

		if (gAIEnv.CVars.DebugDrawCommunication == 5)
		{
			CommID commID = m_player.GetCommunicationID(playID);
			CryLogAlways("CommunicationManager::OnCommunicationFinished: %s[%u] as playID[%u]", GetCommunicationName(commID), commID.id, playID.id);
		}

		if (stateFlags != CommunicationPlayer::PlayState::FinishedAll)
		{
			m_unfinalized.push_back(playing);
		}

		m_playing.erase(it);

	}
	else if (gAIEnv.CVars.DebugDrawCommunication == 5)
	{
		CommID commID = m_player.GetCommunicationID(playID);
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "CommunicationManager::OnCommunicationFinished - for unknown playID: %s[%u] as playID[%u]", GetCommunicationName(commID), commID.id, playID.id);
	}
}

const CCommunicationManager::CommunicationsConfig& CCommunicationManager::GetConfig(const CommConfigID& configID) const
{
	Configs::const_iterator cit = m_configs.find(configID);
	if (cit == m_configs.end())
	{
		static CommunicationsConfig empty;
		return empty;
	}

	const CommunicationsConfig& config = cit->second;

	return config;
}

CommunicationTestManager& CCommunicationManager::GetTestManager()
{
	return m_testManager;
}

bool CCommunicationManager::Play(const CommPlayID& playID, SCommunicationRequest& request,
                                 const CommunicationChannel::Ptr& channel, SCommunication& comm)
{
	bool playResult = false;

	UpdateStateOfInternalVariables(request);

	uint32 variation = 0;
	bool canChooseVariation = ChooseVariation(comm, comm.choiceMethod, 0, variation);

	if (!canChooseVariation)
	{
		// If we cannot choose any variation let's try to reset the history and choose again
		// because maybe with the current variables state only the default variations are available
		// and they were already all played
		ResetHistory();
		canChooseVariation = ChooseVariation(comm, comm.choiceMethod, 0, variation);
	}

	if (canChooseVariation && m_player.Play(playID, request, comm, variation, this, (void*)(EXPAND_PTR)request.channelID.id))
	{
		if (gAIEnv.CVars.RecordCommunicationStats > 0)
		{
			//Record the request here if statistics are being generated
			CommDebugCount& commCount = m_debugStatisticsMap[CommKeys(request.configID, request.commID)];
			++commCount.playCount;
		}

		UpdateHistory(comm, variation);

		//If sound component of readability was skipped then don't occupy any channel
		//Also skip if the readability contained no sound/voice
		const SCommunicationVariation& variationEntry = comm.variations[variation];
		if (variationEntry.soundName.empty() && variationEntry.voiceName.empty())
			request.skipCommSound = true;

		if (!request.skipCommSound)
		{
			channel->Occupy(true);

			//Impose minimum actor silence time based on channel's param
			float actorSilence = channel->GetActorSilence();
			if (actorSilence > 0)
			{
				SetRestrictedDuration(request.actorID, actorSilence, 0.0f);
			}
		}

		PlayingCommunication& playing = stl::map_insert_or_get(m_playing, playID, PlayingCommunication());
		playing.actorID = request.actorID;
		playing.channel = channel;
		playing.eventListener = request.eventListener;
		playing.animName = comm.variations[variation].animationName;
		playing.skipSound = request.skipCommSound;
		playing.minSilence = request.minSilence;
		playing.startTime = gEnv->pTimer->GetCurrTime();

		if (request.eventListener)
			request.eventListener->OnCommunicationEvent(CommunicationStarted, request.actorID, playID);

		UpdateGlobalListeners(CommunicationStarted, playing.actorID, m_player.GetCommunicationID(playID));

		playResult = true;
	}

	if (gAIEnv.CVars.DebugDrawCommunication == 5 || gAIEnv.CVars.DebugDrawCommunication == 6)
	{
		stack_string communicationPlayResult = canChooseVariation ? "PLAYED" : "NOT PLAYED";
		stack_string variationIndex;
		if (canChooseVariation)
		{
			variationIndex.Format("(Variation index %u)", variation);
		}
		else
		{
			variationIndex.Format("(The choice of a variation didn't return any valid result)");
		}
		CryLogAlways("Playing Communication '%s' Status=%s: [%u] as playID[%u] %s",
		             GetCommunicationName(request.commID), communicationPlayResult.c_str(), request.commID.id, playID.id, variationIndex.c_str());
	}

	ResetStateOfInternalVariables();

	return playResult;
}

void CCommunicationManager::FlushQueue(QueuedCommunications& queue, const CommunicationChannel::Ptr& channel, EntityId targetId)
{
	QueuedCommunications::iterator qit = queue.begin();
	QueuedCommunications::iterator end = queue.end();

	while (qit != end)
	{
		QueuedCommunication& queued = *qit;

		CommunicationChannel::Ptr targetChannel = m_channels.GetChannel(queued.request.channelID, queued.request.actorID);
		if (channel->GetPriority() > targetChannel->GetPriority())
		{
			bool commFlushed = false;
			if (channel->GetType() == SCommunicationChannelParams::Personal)
			{
				if (queued.request.actorID == targetId)
				{
					commFlushed = true;
				}
			}
			//For group/global assume it should be flushed
			else
			{
				commFlushed = true;
			}

			if (commFlushed)
			{
				if (queued.request.eventListener)
					queued.request.eventListener->OnCommunicationEvent(CommunicationCancelled, queued.request.actorID, queued.playID);

				UpdateGlobalListeners(CommunicationCancelled, queued.request.actorID, queued.request.commID);

				qit = queue.erase(qit);
				continue;
			}
		}

		++qit;
	}

}

void CCommunicationManager::Queue(QueuedCommunications& queue,
                                  const CommPlayID& playID,
                                  const SCommunicationRequest& request)
{
	assert(!IsQueued(playID, 0));

	queue.push_back(QueuedCommunication());
	QueuedCommunication& back = queue.back();

	back.playID = playID;
	back.age = 0.0f;
	back.request = request;

	if (request.eventListener)
		request.eventListener->OnCommunicationEvent(CommunicationQueued, request.actorID, playID);

	UpdateGlobalListeners(CommunicationQueued, request.actorID, request.commID);

	assert((m_unorderedQueue.size() + m_orderedQueue.size()) < 64);
}

bool CCommunicationManager::RemoveFromQueue(QueuedCommunications& queue, const CommPlayID& playID)
{
	QueuedCommunications::iterator it = queue.begin();
	QueuedCommunications::iterator end = queue.end();

	for (; it != end; ++it)
	{
		QueuedCommunication& queued = *it;

		if (queued.playID == playID)
		{
			if (queued.request.eventListener)
				queued.request.eventListener->OnCommunicationEvent(CommunicationCancelled, queued.request.actorID, queued.playID);

			UpdateGlobalListeners(CommunicationCancelled, queued.request.actorID, queued.request.commID);

			queue.erase(it);
			return true;
		}
	}

	return false;
}

void CCommunicationManager::UpdateQueue(QueuedCommunications& queue, float updateTime)
{
	QueuedCommunications::iterator it = queue.begin();
	QueuedCommunications::iterator end = queue.end();

	for (; it != end; )
	{
		QueuedCommunication& queued = *it;
		queued.age += updateTime;

		if (queued.age <= queued.request.contextExpiry)
			++it;
		else
		{
			if (queued.request.eventListener)
				queued.request.eventListener->OnCommunicationEvent(CommunicationExpired, queued.request.actorID, queued.playID);

			UpdateGlobalListeners(CommunicationExpired, queued.request.actorID, queued.request.commID);

			QueuedCommunications::iterator erased = it++;
			queue.erase(erased);
		}
	}
}

bool CCommunicationManager::PlayFromQueue(QueuedCommunication& queued)
{
	// sanity checks are performed before playing from the queue
	Configs::iterator cit = m_configs.find(queued.request.configID);
	CommunicationsConfig& config = cit->second;

	Communications::iterator commIt = config.comms.find(queued.request.commID);
	SCommunication& comm = commIt->second;

	CommunicationChannel::Ptr channel = m_channels.GetChannel(queued.request.channelID, queued.request.actorID);
	if (!channel->IsFree())
		return false;

	if (IsAnimationRestricted(queued.request.actorID))
	{
		queued.request.skipCommAnimation = true;
	}

	if (!channel->IgnoresActorSilence())
	{
		if (IsVoiceRestricted(queued.request.actorID))
		{
			queued.request.skipCommSound = true;
		}
	}

	//If there is still a component to play
	if (!queued.request.skipCommAnimation || !queued.request.skipCommSound)
	{
		Play(queued.playID, queued.request, channel, comm);
	}
	// Else do nothing, drop this from the queue, as the actor is restricted, try the next entry

	return true;
}

void CCommunicationManager::ProcessQueues()
{
	// Ordered queue has priority since it's usually more important
	while (!m_orderedQueue.empty())
	{
		QueuedCommunication& queued = m_orderedQueue.front();

		if (!PlayFromQueue(queued))
			break;

		m_orderedQueue.erase(m_orderedQueue.begin());
	}

	while (!m_unorderedQueue.empty())
	{
		QueuedCommunication& queued = m_unorderedQueue.front();

		if (!PlayFromQueue(queued))
			break;

		m_unorderedQueue.erase(m_unorderedQueue.begin());
	}
}

bool CCommunicationManager::ChooseVariation(const SCommunication& comm, const SCommunication::EVariationChoiceMethod method,
                                            const uint32 index, uint32& outputIndex) const
{
	const uint32 variationCount = comm.variations.size();

	switch (method)
	{
	case SCommunication::RandomSequence:
		{
			m_randomPool.resize(0);

			for (uint32 i = 0; i < variationCount; ++i)
			{
				const bool canVariationBePlayed = CanVariationBePlayed(comm, i);
				if ((comm.history.played & (1 << i)) == 0 && canVariationBePlayed)
					m_randomPool.push_back(i);
			}

			const uint32 size = m_randomPool.size();
			if (!m_randomPool.empty())
			{
				outputIndex = m_randomPool[cry_random(0U, size - 1)];
				return true;
			}
			return false;
		}
		break;
	case SCommunication::Sequence:
		{
			uint32 played = comm.history.played;
			uint32 next = 0;

			while (played >>= 1)
				++next;

			// We arrived to the first element that we didn't play
			// Now check which is the first element we CAN play
			while (next < variationCount && !CanVariationBePlayed(comm, next))
			{
				++next;
			}

			outputIndex = next;
			return true;
		}
		break;
	case SCommunication::Match:
		{
			outputIndex = index;
			if (outputIndex < variationCount && CanVariationBePlayed(comm, outputIndex))
			{
				return true;
			}
			else
			{
				outputIndex = index % variationCount;
				if (CanVariationBePlayed(comm, outputIndex))
					return true;
				else
					return false;
			}
		}
		break;
	default:
	case SCommunication::Random:
		{
			m_randomPool.resize(0);

			for (uint32 i = 0; i < variationCount; ++i)
			{
				const bool canVariationBePlayed = CanVariationBePlayed(comm, i);
				if (canVariationBePlayed)
					m_randomPool.push_back(i);
			}

			uint32 size = m_randomPool.size();
			if (!m_randomPool.empty())
			{
				outputIndex = m_randomPool[cry_random(0U, size - 1)];
				return true;
			}
			return false;
		}
	}
}

bool CCommunicationManager::CanVariationBePlayed(const SCommunication& comm, const uint32 variationIndex) const
{
	if (variationIndex < comm.variations.size())
	{
		const SCommunicationVariation& variation = comm.variations[variationIndex];
		if (variation.condition.get())
			return variation.condition->Evaluate(m_variables);
		else
			return true;
	}

	return false;
}

void CCommunicationManager::UpdateHistory(SCommunication& comm, uint32 variation)
{
	uint32 variationCount = comm.variations.size();
	assert(variation < variationCount);

	comm.history.played |= 1 << variation;

	// Reset everything except the one currently played,
	// to prevent it from being picked again next play
	if (variationCount > 1)
	{
		if ((comm.history.played & ((1 << variationCount) - 1)) == comm.history.played)
			comm.history.played = 1 << variation;
	}
}

void CCommunicationManager::SetRestrictedDuration(EntityId actorId, float voiceDuration, float animDuration)
{
	SRestrictedActorParams& actorParams = m_restrictedActors[actorId];
	if (voiceDuration > 0.0f)
	{
		actorParams.m_voiceRestrictedTime = voiceDuration;
	}
	if (animDuration > 0.0f)
	{
		actorParams.m_animRestrictedTime = animDuration;
	}
}

void CCommunicationManager::AddActorRestriction(EntityId actorId, bool restrictVoice, bool restrictAnimation)
{
	SRestrictedActorParams& actorParams = m_restrictedActors[actorId];

	if (restrictVoice)
	{
		++actorParams.m_voiceRestricted;
	}
	if (restrictAnimation)
	{
		++actorParams.m_animRestricted;
	}
}

void CCommunicationManager::RemoveActorRestriction(EntityId actorId, bool unrestrictVoice, bool unrestrictAnimation)
{
	TRestrictedActorMap::iterator foundActor = m_restrictedActors.find(actorId);

	//If removing the restriction, then erase the actor from the restricted map
	if (foundActor != m_restrictedActors.end())
	{
		SRestrictedActorParams& params = foundActor->second;

		if (unrestrictVoice)
		{
			--params.m_voiceRestricted;
		}

		if (unrestrictAnimation)
		{
			--params.m_animRestricted;
		}
		//Next update call will handle removal of now unrestricted actors
		assert(params.m_voiceRestricted >= 0 && params.m_animRestricted >= 0);
	}
}

void CCommunicationManager::SetVariableValue(const char* variableName, const bool newVariableValue)
{
	Variables::VariableID variableId = Variables::GetVariableID(variableName);
	m_variables.SetVariable(variableId, newVariableValue);
}

void CCommunicationManager::GetVariablesNames(const char** variableNames, const size_t maxSize, size_t& actualSize) const
{
	m_variablesDeclaration.GetVariablesNames(variableNames, maxSize, actualSize);
}

void CCommunicationManager::UpdateActorRestrictions(float updateTime)
{
	TRestrictedActorMap::iterator actorIt = m_restrictedActors.begin();
	TRestrictedActorMap::iterator endIt = m_restrictedActors.end();

	while (actorIt != endIt)
	{
		SRestrictedActorParams& actorParams = actorIt->second;
		actorParams.Update(updateTime);

		if (!actorParams.IsRestricted())
		{
			m_restrictedActors.erase(actorIt++);
			continue;
		}

		++actorIt;
	}
}

bool CCommunicationManager::IsVoiceRestricted(EntityId actorId)
{
	TRestrictedActorMap::const_iterator foundActor = m_restrictedActors.find(actorId);

	if (foundActor != m_restrictedActors.end())
	{
		const SRestrictedActorParams& actorParams = foundActor->second;
		return actorParams.IsVoiceRestricted();
	}

	return false;
}

bool CCommunicationManager::IsAnimationRestricted(EntityId actorId)
{
	TRestrictedActorMap::const_iterator foundActor = m_restrictedActors.find(actorId);

	if (foundActor != m_restrictedActors.end())
	{
		const SRestrictedActorParams& actorParams = foundActor->second;
		return actorParams.IsAnimationRestricted();
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------------------
void CCommunicationManager::GetActorData(EntityId actorId, SActorReadabilitySettings& actorReadabilitySettings)
{
	TActorReadabilityProperties::iterator ait = m_actorPropertiesMap.find(actorId);

	//Check here to see if the actor already retrieved its properties. If so just return that data
	if (ait != m_actorPropertiesMap.end())
	{
		actorReadabilitySettings = ait->second;
		return;
	}

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(actorId);

	SActorReadabilitySettings tempActorReadabilitySettings;

	if (IEntityArchetype* pEntityArchetype = pEntity->GetArchetype())
	{
		if (IScriptTable* pProperties = pEntityArchetype->GetProperties())
		{
			ReadDataFromTable(pProperties, tempActorReadabilitySettings);
			actorReadabilitySettings = tempActorReadabilitySettings;
		}
	}

	SmartScriptTable ssInstanceTable;
	bool bFound = pEntity->GetScriptTable()->GetValue("PropertiesInstance", ssInstanceTable);
	if (!bFound)
	{
		bFound = pEntity->GetScriptTable()->GetValue("Properties", ssInstanceTable);
	}

	if (bFound)
	{
		SmartScriptTable ssReadabilityInstanceTable;
		if (ssInstanceTable->GetValue("Readability", ssReadabilityInstanceTable))
		{
			bool bOverrideArchetype = true;
			bool bExists = ssReadabilityInstanceTable->GetValue("bOverrideArchetype", bOverrideArchetype);

			if (!bExists || bOverrideArchetype)
			{
				ReadDataFromTable(ssInstanceTable, tempActorReadabilitySettings);
				actorReadabilitySettings = tempActorReadabilitySettings;
			}
		}
	}

	m_actorPropertiesMap[actorId] = actorReadabilitySettings;
}

//------------------------------------------------------------------------------------------------------------------------
void CCommunicationManager::ReadDataFromTable(const SmartScriptTable& ssTable, SActorReadabilitySettings& actorReadabilitySettings)
{
	SmartScriptTable ssReadabilityTable;

	if (ssTable->GetValue("Readability", ssReadabilityTable))
	{
		ssReadabilityTable->GetValue("bIgnoreAnimations", actorReadabilitySettings.m_bIgnoreAnimations);
		ssReadabilityTable->GetValue("bIgnoreVoice", actorReadabilitySettings.m_bIgnoreVoice);
	}
}

void CCommunicationManager::RemoveInstanceListener(const CommPlayID& playID)
{
	PlayingCommunications::iterator it = m_playing.find(playID);
	if (it != m_playing.end() && it->second.eventListener)
		it->second.eventListener = NULL;

	//Could also be in one of the queues.

	for (QueuedCommunications::iterator ordIt = m_orderedQueue.begin(); ordIt != m_orderedQueue.end(); ++ordIt)
	{
		if (playID == ordIt->playID)
		{
			ordIt->request.eventListener = NULL;
			break;
		}
	}

	for (QueuedCommunications::iterator unordIt = m_unorderedQueue.begin(); unordIt != m_unorderedQueue.end(); ++unordIt)
	{
		if (playID == unordIt->playID)
		{
			unordIt->request.eventListener = NULL;
			break;
		}
	}

}

void CCommunicationManager::WriteStatistics()
{
#if !defined(_RELEASE)
	static const char* logFilename = "communication_statistics.txt";

	#if defined(USE_FILE_HANDLE_CACHE)
	FILE* pLogFile = ::fopen(logFilename, "wt", FileIoWrapper::GENERAL, true);
	#else
	FILE* pLogFile = fxopen(logFilename, "wt");
	#endif

	CryLogAlways("Dumping Communication Statistics log to '%s'\n", logFilename);

	string lastConfig;
	for (TDebugMapStatistics::const_iterator statIt = m_debugStatisticsMap.begin(); statIt != m_debugStatisticsMap.end(); ++statIt)
	{
		string currentConfig = GetConfig(statIt->first.configId).name;
		//Add a title line to separate configs' stats
		if (currentConfig != lastConfig)
		{
			if (!lastConfig.empty())
			{
				fprintf(pLogFile, "\n");
			}
			fprintf(pLogFile, "Config: %s Requested Played\n", currentConfig.c_str());
			lastConfig = currentConfig;
		}
		const char* pName = GetCommunicationName(statIt->first.commId);
		fprintf(pLogFile, "Comm: %s %u %u\n", pName, statIt->second.requestCount, statIt->second.playCount);
	}

	fclose(pLogFile);

	m_debugStatisticsMap.clear();
#endif
}

void CCommunicationManager::CullPlayingCommunications()
{
	// If communications have been playing for some
	// time we consider it stuck and remove it.

	const float maxTimeAllowedPlaying = 7.0f;
	float now = gEnv->pTimer->GetCurrTime();

	PlayingCommunications::iterator it = m_playing.begin();
	PlayingCommunications::iterator end = m_playing.end();

	for (; it != end; ++it)
	{
		PlayingCommunication& comm = it->second;

		float elapsed = now - comm.startTime;

		if (elapsed > maxTimeAllowedPlaying)
		{
			const CommPlayID& playID = it->first;

			StopCommunication(playID);

			// It's important that we return here because the m_playing
			// container has been modified and the iterators might no
			// longer be valid.
			return;
		}
	}
}

void CCommunicationManager::UpdateStateOfInternalVariables(const SCommunicationRequest& request)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.actorID);
	if (pEntity)
	{
		CAIActor* pAIActor = CastToCAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			const IAIObject* pAttTarget = pAIActor->GetAttentionTarget();
			if (pAttTarget)
			{
				UpdateStateOfTargetAboveBelowVariables(pEntity->GetPos(), pAttTarget->GetPos());
			}
		}
	}
}

void CCommunicationManager::UpdateStateOfTargetAboveBelowVariables(const Vec3& entityPos, const Vec3& targetPos)
{
	const float heightThreshold = gAIEnv.CVars.CommunicationManagerHeightThresholdForTargetPosition;
	const float heightDifference = targetPos.z - entityPos.z;
	if (heightDifference > heightThreshold)
	{
		SetVariableValue(m_kTargetIsAbove_VariableName, true);
	}
	else if (heightDifference < -heightThreshold)
	{
		SetVariableValue(m_kTargetIsBelow_VariableName, true);
	}
}

void CCommunicationManager::ResetStateOfInternalVariables()
{
	// Player is above
	SetVariableValue(m_kTargetIsAbove_VariableName, false);

	// Player is below
	SetVariableValue(m_kTargetIsBelow_VariableName, false);
}
