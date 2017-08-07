// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>

#include <CrySerialization/CryStrings.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/StringList.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// NavigationAgentTypeID
		//
		//===================================================================================

		bool NavigationAgentTypeID_Serialize(Serialization::IArchive& archive, NavigationAgentTypeID& value, const char* szName, const char* szLabel)
		{
			// TODO pavloi 2016.09.26: move this code to the CryCommon CrySerialization as a decorator.

			INavigationSystem* pNavSystem = gEnv->pAISystem->GetNavigationSystem();
			if (!pNavSystem)
			{
				return false;
			}

			stack_string agentName;
			if (archive.isOutput())
			{
				agentName = pNavSystem->GetAgentTypeName(value);
			}

			bool bResult = false;

			if (archive.isEdit())
			{
				const size_t agentTypesAmount = pNavSystem->GetAgentTypeCount();

				Serialization::StringList nameStringList;
				nameStringList.reserve(agentTypesAmount + 1); // first item in the list is an empty name
				nameStringList.resize(1);  

				int agentNameIndex = 0; // empty by default

				for (size_t agentTypeIndex = 0; agentTypeIndex < agentTypesAmount; agentTypeIndex++)
				{
					const NavigationAgentTypeID agentTypeID = pNavSystem->GetAgentTypeID(agentTypeIndex);
					nameStringList.push_back(string(pNavSystem->GetAgentTypeName(agentTypeID)));

					if (agentTypeID == value)
					{
						agentNameIndex = agentTypeIndex + 1;
					}
				}

				Serialization::SStruct validatorKey = Serialization::SStruct::forEdit(value);
				Serialization::StringListValue stringListValue(nameStringList, agentNameIndex, validatorKey.pointer(), validatorKey.type());
				bResult = archive(stringListValue, szName, szLabel);

				if (archive.isInput())
				{
					agentName = (stringListValue.index() != Serialization::StringList::npos)
						? stringListValue.c_str()
						: "";
				}
			}
			else
			{
				bResult = archive(agentName, "name", "^");
			}

			if (archive.isInput())
			{
				value = pNavSystem->GetAgentTypeID(agentName.c_str());
			}

			if (value == NavigationAgentTypeID())
			{
				if (archive.isInput())
				{
					bResult = false;
				}

				if (archive.isValidation())
				{
					archive.warning(value, "Unknown navigation agent type '%s'", agentName.c_str());
				}
			}

			return bResult;
		}

	}
}
