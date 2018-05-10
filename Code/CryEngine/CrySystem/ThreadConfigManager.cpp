// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ThreadConfigManager.h"
#include <CrySystem/IConsole.h>
#include "System.h"
#include <CryString/StringUtils.h>
#include <CryCore/CryCustomTypes.h>

namespace
{
const char* sCurThreadConfigFilename = "";
const uint32 sPlausibleStackSizeLimitKB = (1024 * 100); // 100mb
}

//////////////////////////////////////////////////////////////////////////
CThreadConfigManager::CThreadConfigManager()
{
	m_defaultConfig.szThreadName = "CryThread_Unnamed";
	m_defaultConfig.stackSizeBytes = 0;
	m_defaultConfig.affinityFlag = -1;
	m_defaultConfig.priority = THREAD_PRIORITY_NORMAL;
	m_defaultConfig.bDisablePriorityBoost = false;
	m_defaultConfig.paramActivityFlag = (SThreadConfig::TThreadParamFlag)~0;
}

//////////////////////////////////////////////////////////////////////////
const SThreadConfig* CThreadConfigManager::GetThreadConfig(const char* szThreadName, ...)
{
	va_list args;
	va_start(args, szThreadName);

	// Format thread name
	char strThreadName[THREAD_NAME_LENGTH_MAX];
	if (!cry_vsprintf(strThreadName, szThreadName, args))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: ThreadName \"%s\" has been truncated to \"%s\". Max characters allowed: %i. ", szThreadName, strThreadName, (int)sizeof(strThreadName) - 1);
	}

	// Get Thread Config
	const SThreadConfig* retThreasdConfig = GetThreadConfigImpl(strThreadName);

	va_end(args);
	return retThreasdConfig;
}

//////////////////////////////////////////////////////////////////////////
const SThreadConfig* CThreadConfigManager::GetThreadConfigImpl(const char* szThreadName)
{
	// Get thread config for platform
	ThreadConfigMapConstIter threatRet = m_threadConfig.find(CryFixedStringT<THREAD_NAME_LENGTH_MAX>(szThreadName));
	if (threatRet == m_threadConfig.end())
	{
		// Search in wildcard setups
		ThreadConfigMapConstIter wildCardIter = m_wildcardThreadConfig.begin();
		ThreadConfigMapConstIter wildCardIterEnd = m_wildcardThreadConfig.end();
		for (; wildCardIter != wildCardIterEnd; ++wildCardIter)
		{
			if (CryStringUtils::MatchWildcard(szThreadName, wildCardIter->second.szThreadName))
			{
				// Store new thread config
				SThreadConfig threadConfig = wildCardIter->second;
				std::pair<ThreadConfigMapIter, bool> res;
				res = m_threadConfig.insert(ThreadConfigMapPair(CryFixedStringT<THREAD_NAME_LENGTH_MAX>(szThreadName), threadConfig));

				// Store name (ref to key)
				SThreadConfig& rMapThreadConfig = res.first->second;
				rMapThreadConfig.szThreadName = res.first->first.c_str();

				// Return new thread config
				return &res.first->second;
			}
		}

		// Failure case, no match found
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadInfo>: Unable to find config for thread:%s", szThreadName);
		return &m_defaultConfig;
	}

	// Return thread config
	return &threatRet->second;
}

//////////////////////////////////////////////////////////////////////////
const SThreadConfig* CThreadConfigManager::GetDefaultThreadConfig() const
{
	return &m_defaultConfig;
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::LoadConfig(const char* pcPath)
{
	// Adjust filename for OnDisk or in .pak file loading
	char szFullPathBuf[ICryPak::g_nMaxPath];
	gEnv->pCryPak->AdjustFileName(pcPath, szFullPathBuf, 0);

	// Open file
	XmlNodeRef xmlRoot = GetISystem()->LoadXmlFromFile(szFullPathBuf);
	if (!xmlRoot)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: File \"%s\" not found!", pcPath);
		return false;
	}

	// Load config for active platform
	sCurThreadConfigFilename = pcPath;
	const char* strPlatformId = IdentifyPlatform();
	CryFixedStringT<32> tmpPlatformStr;
	bool retValueCommon = false;
	bool retValueCPU = false;

	// Try load common platform settings
	tmpPlatformStr.Format("%s_Common", strPlatformId);
	retValueCommon = LoadPlatformConfig(xmlRoot, tmpPlatformStr.c_str());
	if (retValueCommon)
	{
		CryLogAlways("<ThreadConfigInfo>: Thread profile loaded: \"%s\" (%s)  ", tmpPlatformStr.c_str(), pcPath);
	}

#if defined(CRY_PLATFORM_DESKTOP)
	// Handle PC specifically as we do not know the core setup of the executing machine.
	// Try and find the next power of 2 core setup. Otherwise fallback to a lower power of 2 core setup spec

	// Try and load next pow of 2 setup for active pc core configuration
	const unsigned int numCPUs = ((CSystem*)GetISystem())->GetCPUFeatures()->GetLogicalCPUCount();
	uint32 i = numCPUs;
	for (; i > 0; --i)
	{
		tmpPlatformStr.Format("%s_%i", strPlatformId, i);
		retValueCPU = LoadPlatformConfig(xmlRoot, tmpPlatformStr.c_str());
		if (retValueCPU)
		{
			break;
		}
	}

	if (retValueCPU && i != numCPUs)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: (%s: %u core) Unable to find platform config \"%s\". Next valid config found was %s_%u.",
		           strPlatformId, numCPUs, tmpPlatformStr.c_str(), strPlatformId, i);
	}

#else
	tmpPlatformStr.Format("%s", strPlatformId);
	retValueCPU = LoadPlatformConfig(xmlRoot, strPlatformId);
#endif

	// Print out info
	if (retValueCPU)
	{
		CryLogAlways("<ThreadConfigInfo>: Thread profile loaded: \"%s\" (%s)  ", tmpPlatformStr.c_str(), pcPath);
	}
	
	if (!retValueCommon && !retValueCPU)
	{
		// Could not find any matching platform
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: Active platform identifier string \"%s\" not found in config \"%s\".", strPlatformId, sCurThreadConfigFilename);
	}

	sCurThreadConfigFilename = "";
	return (retValueCommon || retValueCPU);
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::ConfigLoaded() const
{
	return !m_threadConfig.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::LoadPlatformConfig(const XmlNodeRef& rXmlRoot, const char* sPlatformId)
{

	// Validate node
	if (!rXmlRoot->isTag("ThreadConfig"))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: Unable to find root xml node \"ThreadConfig\"");
		return false;
	}

	// Find active platform
	const uint32 numPlatforms = rXmlRoot->getChildCount();
	for (uint32 i = 0; i < numPlatforms; ++i)
	{
		const XmlNodeRef xmlPlatformNode = rXmlRoot->getChild(i);

		// Is platform node
		if (!xmlPlatformNode->isTag("Platform"))
		{
			continue;
		}

		// Is has Name attribute
		if (!xmlPlatformNode->haveAttr("Name"))
		{
			continue;
		}

		// Is platform of interest
		const char* platformName = xmlPlatformNode->getAttr("Name");
		if (_stricmp(sPlatformId, platformName) == 0)
		{
			// Load platform
			LoadThreadDefaultConfig(xmlPlatformNode);
			LoadPlatformThreadConfigs(xmlPlatformNode);
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadPlatformThreadConfigs(const XmlNodeRef& rXmlPlatformRef)
{
	// Get thread configurations for active platform
	const uint32 numThreads = rXmlPlatformRef->getChildCount();
	for (uint32 j = 0; j < numThreads; ++j)
	{
		const XmlNodeRef xmlThreadNode = rXmlPlatformRef->getChild(j);

		if (!xmlThreadNode->isTag("Thread"))
		{
			continue;
		}

		// Ensure thread config has name
		if (!xmlThreadNode->haveAttr("Name"))
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Thread node without \"name\" attribute encountered.");
			continue;
		}

		// Load thread config
		SThreadConfig loadedThreadConfig = SThreadConfig(m_defaultConfig);
		LoadThreadConfig(xmlThreadNode, loadedThreadConfig);

		// Get thread name and check if it contains wildcard characters
		const char* szThreadName = xmlThreadNode->getAttr("Name");
		bool bWildCard = strchr(szThreadName, '*') ? true : false;
		ThreadConfigMap& threadConfig = bWildCard ? m_wildcardThreadConfig : m_threadConfig;

		// Check for duplicate and override it with new config if found
		if (threadConfig.find(szThreadName) != threadConfig.end())
		{
			CryLogAlways("<ThreadConfigInfo>: [XML Parsing] Thread with name \"%s\" already loaded. Overriding with new configuration", szThreadName);
			threadConfig[szThreadName] = loadedThreadConfig;
			continue;
		}

		// Store new thread config
		std::pair<ThreadConfigMapIter, bool> res;
		res = threadConfig.insert(ThreadConfigMapPair(CryFixedStringT<THREAD_NAME_LENGTH_MAX>(szThreadName), loadedThreadConfig));

		// Store name (ref to key)
		SThreadConfig& rMapThreadConfig = res.first->second;
		rMapThreadConfig.szThreadName = res.first->first.c_str();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CThreadConfigManager::LoadThreadDefaultConfig(const XmlNodeRef& rXmlPlatformRef)
{
	// Find default thread config node
	const uint32 numNodes = rXmlPlatformRef->getChildCount();
	for (uint32 j = 0; j < numNodes; ++j)
	{
		const XmlNodeRef xmlNode = rXmlPlatformRef->getChild(j);

		// Load default config
		if (xmlNode->isTag("ThreadDefault"))
		{
			LoadThreadConfig(xmlNode, m_defaultConfig);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadAffinity(const XmlNodeRef& rXmlThreadRef, uint32& rAffinity, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
{
	const char* szValidCharacters = "-,0123456789";
	uint32 affinity = 0;

	// Validate node
	if (!rXmlThreadRef->haveAttr("Affinity"))
		return;

	// Validate token
	CryFixedStringT<32> affinityRawStr(rXmlThreadRef->getAttr("Affinity"));
	if (affinityRawStr.empty())
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"Affinity\" encountered");
		return;
	}

	if (affinityRawStr.compareNoCase("ignore") == 0)
	{
		// Param is inactive, clear bit
		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_Affinity;
		return;
	}

	CryFixedStringT<32>::size_type nPos = affinityRawStr.find_first_not_of(" -,0123456789");
	if (nPos != CryFixedStringT<32>::npos)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING,
		           "<ThreadConfigInfo>: [XML Parsing] Invalid character \"%c\" encountered in \"Affinity\" attribute. Valid characters:\"%s\" Offending token:\"%s\"", affinityRawStr.at(nPos),
		           szValidCharacters, affinityRawStr.c_str());
		return;
	}

	// Tokenize comma separated string
	int pos = 0;
	CryFixedStringT<32> affnityTokStr = affinityRawStr.Tokenize(",", pos);
	while (!affnityTokStr.empty())
	{
		affnityTokStr.Trim();

		long affinityId = strtol(affnityTokStr.c_str(), NULL, 10);
		if (affinityId == LONG_MAX || affinityId == LONG_MIN)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unknown value \"%s\" encountered for attribute \"Affinity\"", affnityTokStr.c_str());
			return;
		}

		// Allow scheduler to pick thread
		if (affinityId == -1)
		{
			affinity = ~0;
			break;
		}

		// Set affinity bit
		affinity |= BIT(affinityId);

		// Move to next token
		affnityTokStr = affinityRawStr.Tokenize(",", pos);
	}

	// Set affinity reference
	rAffinity = affinity;
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadPriority(const XmlNodeRef& rXmlThreadRef, int32& rPriority, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
{
	const char* szValidCharacters = "-,0123456789";

	// Validate node
	if (!rXmlThreadRef->haveAttr("Priority"))
		return;

	// Validate token
	CryFixedStringT<32> threadPrioStr(rXmlThreadRef->getAttr("Priority"));
	threadPrioStr.Trim();
	if (threadPrioStr.empty())
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"Priority\" encountered");
		return;
	}

	if (threadPrioStr.compareNoCase("ignore") == 0)
	{
		// Param is inactive, clear bit
		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_Priority;
		return;
	}

	// Test for character string (no numbers allowed)
	if (threadPrioStr.find_first_of(szValidCharacters) == CryFixedStringT<32>::npos)
	{
		threadPrioStr.MakeLower();

		// Set priority
		if (threadPrioStr.compare("below_normal") == 0)
		{
			rPriority = THREAD_PRIORITY_BELOW_NORMAL;
		}
		else if (threadPrioStr.compare("normal") == 0)
		{
			rPriority = THREAD_PRIORITY_NORMAL;
		}
		else if (threadPrioStr.compare("above_normal") == 0)
		{
			rPriority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
		else if (threadPrioStr.compare("idle") == 0)
		{
			rPriority = THREAD_PRIORITY_IDLE;
		}
		else if (threadPrioStr.compare("lowest") == 0)
		{
			rPriority = THREAD_PRIORITY_LOWEST;
		}
		else if (threadPrioStr.compare("highest") == 0)
		{
			rPriority = THREAD_PRIORITY_HIGHEST;
		}
		else if (threadPrioStr.compare("time_critical") == 0)
		{
			rPriority = THREAD_PRIORITY_TIME_CRITICAL;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Platform unsupported value \"%s\" encountered for attribute \"Priority\"", threadPrioStr.c_str());
			return;
		}
	}
	// Test for number string (no alphabetical characters allowed)
	else if (threadPrioStr.find_first_not_of(szValidCharacters) == CryFixedStringT<32>::npos)
	{
		long numValue = strtol(threadPrioStr.c_str(), NULL, 10);
		if (numValue == LONG_MAX || numValue == LONG_MIN)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported number type \"%s\" for for attribute \"Priority\"", threadPrioStr.c_str());
			return;
		}

		// Set priority
		rPriority = numValue;
	}
	else
	{
		// String contains characters and numbers
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported type \"%s\" encountered for attribute \"Priority\". Token containers numbers and characters", threadPrioStr.c_str());
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadDisablePriorityBoost(const XmlNodeRef& rXmlThreadRef, bool& rPriorityBoost, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
{
	const char* sValidCharacters = "-,0123456789";

	// Validate node
	if (!rXmlThreadRef->haveAttr("DisablePriorityBoost"))
	{
		return;
	}

	// Extract bool info
	CryFixedStringT<16> sAttribToken(rXmlThreadRef->getAttr("DisablePriorityBoost"));
	sAttribToken.Trim();
	sAttribToken.MakeLower();

	if (sAttribToken.compare("ignore") == 0)
	{
		// Param is inactive, clear bit
		rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_PriorityBoost;
		return;
	}
	else if (sAttribToken.compare("true") == 0 || sAttribToken.compare("1") == 0)
	{
		rPriorityBoost = true;
	}
	else if (sAttribToken.compare("false") == 0 || sAttribToken.compare("0") == 0)
	{
		rPriorityBoost = false;
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Unsupported bool type \"%s\" encountered for attribute \"DisablePriorityBoost\"",
		           rXmlThreadRef->getAttr("DisablePriorityBoost"));
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadStackSize(const XmlNodeRef& rXmlThreadRef, uint32& rStackSize, SThreadConfig::TThreadParamFlag& rParamActivityFlag)
{
	const char* sValidCharacters = "0123456789";

	if (rXmlThreadRef->haveAttr("StackSizeKB"))
	{
		int32 nPos = 0;

		// Read stack size
		CryFixedStringT<32> stackSize(rXmlThreadRef->getAttr("StackSizeKB"));

		// Validate stack size
		if (stackSize.empty())
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Empty attribute \"StackSize\" encountered");
			return;
		}
		else if (stackSize.compareNoCase("ignore") == 0)
		{
			// Param is inactive, clear bit
			rParamActivityFlag &= ~SThreadConfig::eThreadParamFlag_StackSize;
			return;
		}
		else if (stackSize.find_first_not_of(sValidCharacters) == CryFixedStringT<32>::npos)
		{
			// Convert string to long
			long stackSizeVal = strtol(stackSize.c_str(), NULL, 10);
			if (stackSizeVal == LONG_MAX || stackSizeVal == LONG_MIN)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] Invalid number for \"StackSize\" encountered. \"%s\"", stackSize.c_str());
				return;
			}
			else if (stackSizeVal <= 0 || stackSizeVal > sPlausibleStackSizeLimitKB)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<ThreadConfigInfo>: [XML Parsing] \"StackSize\" value not plausible \"%" PRId64 "KB\"", (int64)stackSizeVal);
				return;
			}

			// Set stack size
			rStackSize = stackSizeVal * 1024; // Convert to bytes
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::LoadThreadConfig(const XmlNodeRef& rXmlThreadRef, SThreadConfig& rThreadConfig)
{
	LoadAffinity(rXmlThreadRef, rThreadConfig.affinityFlag, rThreadConfig.paramActivityFlag);
	LoadPriority(rXmlThreadRef, rThreadConfig.priority, rThreadConfig.paramActivityFlag);
	LoadDisablePriorityBoost(rXmlThreadRef, rThreadConfig.bDisablePriorityBoost, rThreadConfig.paramActivityFlag);
	LoadStackSize(rXmlThreadRef, rThreadConfig.stackSizeBytes, rThreadConfig.paramActivityFlag);
}

//////////////////////////////////////////////////////////////////////////
const char* CThreadConfigManager::IdentifyPlatform()
{
#if defined(DURANGO)
	return "durango";
#elif defined(ORBIS)
	return "orbis";
	// ANDROID **needs** to be tested before LINUX because both are defined for android
#elif defined(ANDROID)
	return "android";
#elif defined(LINUX)
	return "linux";
#elif defined(APPLE)
	return "mac";
#elif defined(WIN32) || defined(WIN64)
	return "pc";
#else
	#error "Undefined platform"
#endif
}

//////////////////////////////////////////////////////////////////////////
void CThreadConfigManager::DumpThreadConfigurationsToLog()
{
#if !defined(RELEASE)

	// Print header
	CryLogAlways("== Thread Startup Config List (\"%s\") ==", IdentifyPlatform());

	// Print loaded default config
	CryLogAlways("  (Default) 1. \"%s\" (StackSize:%uKB | Affinity:%u | Priority:%i | PriorityBoost:\"%s\")", m_defaultConfig.szThreadName, m_defaultConfig.stackSizeBytes / 1024,
	             m_defaultConfig.affinityFlag, m_defaultConfig.priority, m_defaultConfig.bDisablePriorityBoost ? "disabled" : "enabled");

	// Print loaded thread configs
	int listItemCounter = 1;
	ThreadConfigMapConstIter iter = m_threadConfig.begin();
	ThreadConfigMapConstIter iterEnd = m_threadConfig.end();
	for (; iter != iterEnd; ++iter)
	{
		const SThreadConfig& threadConfig = iter->second;
		CryLogAlways("%3d.\"%s\" %s (StackSize:%uKB %s | Affinity:%u %s | Priority:%i %s | PriorityBoost:\"%s\" %s)", ++listItemCounter,
		             threadConfig.szThreadName, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_ThreadName) ? "" : "(ignored)",
		             threadConfig.stackSizeBytes / 1024u, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) ? "" : "(ignored)",
		             threadConfig.affinityFlag, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) ? "" : "(ignored)",
		             threadConfig.priority, (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) ? "" : "(ignored)",
		             !threadConfig.bDisablePriorityBoost ? "enabled" : "disabled", (threadConfig.paramActivityFlag & SThreadConfig::eThreadParamFlag_PriorityBoost) ? "" : "(ignored)");
	}
#endif
}
