// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Remote command system implementation
   -------------------------------------------------------------------------
   History:
   - 10/04/2013   : Tomasz Jonarski, Created

*************************************************************************/
#include "StdAfx.h"
#include <CryNetwork/IServiceNetwork.h>
#include "RemoteCommand.h"
#include "RemoteCommandHelpers.h"

//-----------------------------------------------------------------------------

// remote system internal logging
#ifdef RELEASE
	#define LOG_VERBOSE(level, txt, ...)
#else
	#define LOG_VERBOSE(level, txt, ...) if (GetManager()->CheckVerbose(level)) { GetManager()->Log(txt, __VA_ARGS__); }
#endif

//-----------------------------------------------------------------------------

CRemoteCommandManager::CRemoteCommandManager()
{
	// Create the CVAR
	m_pVerboseLevel = REGISTER_INT("rc_debugVerboseLevel", 0, VF_DEV_ONLY, "");
}

CRemoteCommandManager::~CRemoteCommandManager()
{
	// Release the CVar
	if (NULL != m_pVerboseLevel)
	{
		m_pVerboseLevel->Release();
		m_pVerboseLevel = NULL;
	}
}

IRemoteCommandServer* CRemoteCommandManager::CreateServer(uint16 localPort)
{
	// Create the listener
	IServiceNetworkListener* listener = gEnv->pServiceNetwork->CreateListener(localPort);
	if (NULL == listener)
	{
		return NULL;
	}

	// Create the wrapper
	return new CRemoteCommandServer(this, listener);
}

IRemoteCommandClient* CRemoteCommandManager::CreateClient()
{
	// Create the wrapper
	return new CRemoteCommandClient(this);
}

void CRemoteCommandManager::RegisterCommandClass(IRemoteCommandClass& commandClass)
{
	// Make sure command class is not already registered
	const string& className(commandClass.GetName());
	TClassMap::const_iterator it = m_pClasses.find(className);
	if (it != m_pClasses.end())
	{
		LOG_VERBOSE(1, "Class '%s' is already registered",
		            className.c_str());

		return;
	}

	const uint32 classID = m_pClassesByID.size();
	m_pClassesByID.push_back(&commandClass);
	m_pClassesMap[className] = classID;
	m_pClasses[className] = &commandClass;

	// Verbose
	LOG_VERBOSE(1, "Registered command class '%s' with id %d",
	            className.c_str(),
	            classID);
}

#ifndef RELEASE
bool CRemoteCommandManager::CheckVerbose(const uint32 level) const
{
	const int verboseLevel = m_pVerboseLevel->GetIVal();
	return (int)level < verboseLevel;
}

void CRemoteCommandManager::Log(const char* txt, ...)  const
{
	// format the print buffer
	char buffer[512];
	va_list args;
	va_start(args, txt);
	cry_vsprintf(buffer, txt, args);
	va_end(args);

	// pass to log
	gEnv->pLog->LogAlways("%s", buffer);
}
#endif

void CRemoteCommandManager::BuildClassMapping(const std::vector<string>& classNames, std::vector<IRemoteCommandClass*>& outClasses)
{
	LOG_VERBOSE(3, "Building class mapping for %d classes",
	            classNames.size());

	// Output list size has the same size as class names array
	const uint32 numClasses = classNames.size();
	outClasses.resize(numClasses);

	// Match the classes
	for (size_t i = 0; i < numClasses; ++i)
	{
		// Find the matching class
		const string& className = classNames[i];
		TClassMap::const_iterator it = m_pClasses.find(className);
		if (it != m_pClasses.end())
		{
			CRY_ASSERT(className == it->second->GetName());
			CRY_ASSERT(it->second != NULL);
			outClasses[i] = it->second;

			// Report class mapping in heavy verbose mode
			LOG_VERBOSE(3, "Class[%d] = %s",
			            i,
			            className.c_str());
		}
		else
		{
			outClasses[i] = NULL;

			// Class not mapped (this can cause errors)
			LOG_VERBOSE(0, "Remote command class '%s' not found on this machine",
			            className.c_str());
		}
	}
}

void CRemoteCommandManager::SetVerbosityLevel(const uint32 level)
{
	// propagate the value to CVar (so it is consistent across the engine)
	if (NULL != m_pVerboseLevel)
	{
		m_pVerboseLevel->Set((int)level);
	}
}

void CRemoteCommandManager::GetClassList(std::vector<string>& outClassNames) const
{
	const uint32 numClasses = m_pClassesByID.size();
	outClassNames.resize(numClasses);
	for (size_t id = 0; id < numClasses; ++id)
	{
		IRemoteCommandClass* theClass = m_pClassesByID[id];
		if (NULL != theClass)
		{
			outClassNames[id] = theClass->GetName();
		}
	}
}

bool CRemoteCommandManager::FindClassId(IRemoteCommandClass* commandClass, uint32& outClassId) const
{
	// Local search (linear, slower)
	TClassIDMap::const_iterator it = m_pClassesMap.find(commandClass->GetName());
	if (it != m_pClassesMap.end())
	{
		outClassId = it->second;
		return true;
	}

	// Not found
	return false;
}

//-----------------------------------------------------------------------------

// Do not remove (can mess up the uber file builds)
#undef LOG_VERBOSE

//-----------------------------------------------------------------------------
