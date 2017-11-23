//////////////////////////////////////////////////////////////////////////

// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CryAISystem/NavigationSystem/Annotation.h>

inline bool Serialize(Serialization::IArchive& archive, SNavMeshQueryFilterDefault& value, const char* szName, const char* szLabel)
{
	return archive(NavigationSerialization::NavigationQueryFilter(value), szName, szLabel);
}

inline bool Serialize(Serialization::IArchive& archive, NavigationVolumeID& value, const char* szName, const char* szLabel)
{
	static_assert(sizeof(uint32) == sizeof(NavigationVolumeID), "Unsupported NavigationVolumeID size!");
	uint32& id = reinterpret_cast<uint32&>(value);
	return archive(id, szName, szLabel);
}

inline bool Serialize(Serialization::IArchive& archive, NavigationAgentTypeID& value, const char* szName, const char* szLabel)
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	const size_t agentTypeCount = pNavigationSystem->GetAgentTypeCount();

	Serialization::StringList agentTypeNamesList;
	agentTypeNamesList.reserve(agentTypeCount);

	for (size_t i = 0; i < agentTypeCount; ++i)
	{
		const NavigationAgentTypeID id = pNavigationSystem->GetAgentTypeID(i);
		const char* szName = pNavigationSystem->GetAgentTypeName(id);
		if (szName)
		{
			agentTypeNamesList.push_back(szName);
		}
	}

	stack_string stringValue;
	bool bResult = false;
	if (archive.isInput())
	{
		Serialization::StringListValue temp(agentTypeNamesList, 0);
		bResult = archive(temp, szName, szLabel);
		stringValue = temp.c_str();
		value = pNavigationSystem->GetAgentTypeID(stringValue);
	}
	else if (archive.isOutput())
	{
		if (value == NavigationAgentTypeID() && agentTypeNamesList.size())
		{
			stringValue = agentTypeNamesList[0].c_str();
			value = pNavigationSystem->GetAgentTypeID(stringValue);
		}
		else
		{
			stringValue = pNavigationSystem->GetAgentTypeName(value);
		}

		const int pos = agentTypeNamesList.find(stringValue);
		bResult = archive(Serialization::StringListValue(agentTypeNamesList, pos), szName, szLabel);
	}
	return bResult;
}

inline bool Serialize(Serialization::IArchive& archive, NavigationAreaTypeID& value, const char* szName, const char* szLabel)
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	if (archive.isEdit())
	{
		const MNM::IAnnotationsLibrary& annotationsLib = pNavigationSystem->GetAnnotationLibrary();
		const size_t areaTypeCount = annotationsLib.GetAreaTypeCount();

		Serialization::StringList areaTypeNamesList;
		areaTypeNamesList.reserve(areaTypeCount);

		for (size_t i = 0; i < areaTypeCount; ++i)
		{
			const MNM::SAreaType* pAreaType = annotationsLib.GetAreaType(i);
			if (pAreaType)
			{
				areaTypeNamesList.push_back(pAreaType->name.c_str());
			}
		}

		stack_string stringValue;
		bool bResult = false;
		if (archive.isInput())
		{
			Serialization::StringListValue temp(areaTypeNamesList, 0);
			bResult = archive(temp, szName, szLabel);
			stringValue = temp.c_str();
			value = annotationsLib.GetAreaTypeID(stringValue);
		}
		else if (archive.isOutput())
		{
			if (!value.IsValid() && areaTypeNamesList.size())
			{
				stringValue = areaTypeNamesList[0].c_str();
				value = annotationsLib.GetAreaTypeID(stringValue);
			}
			else
			{
				stringValue = annotationsLib.GetAreaTypeName(value);
			}

			const int pos = areaTypeNamesList.find(stringValue);
			bResult = archive(Serialization::StringListValue(areaTypeNamesList, pos), szName, szLabel);
		}
		return bResult;
	}

	uint32 id = value;
	bool bResult = archive(id, szName, szLabel);
	if (archive.isInput())
	{
		value = NavigationAreaTypeID(id);
	}
	return bResult;
}

inline bool Serialize(Serialization::IArchive& archive, NavigationAreaFlagID& value, const char* szName, const char* szLabel)
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
	const MNM::IAnnotationsLibrary& annotationsLib = pNavigationSystem->GetAnnotationLibrary();

	const size_t flagsCount = annotationsLib.GetAreaFlagCount();

	Serialization::StringList flagNamesList;
	flagNamesList.reserve(flagsCount);

	for (size_t i = 0; i < flagsCount; ++i)
	{
		const MNM::SAreaFlag* pAreaFlag = annotationsLib.GetAreaFlag(i);
		if (pAreaFlag)
		{
			flagNamesList.push_back(pAreaFlag->name.c_str());
		}
	}

	stack_string stringValue;
	bool bResult = false;
	if (archive.isInput())
	{
		Serialization::StringListValue temp(flagNamesList, 0);
		bResult = archive(temp, szName, szLabel);
		stringValue = temp.c_str();
		value = annotationsLib.GetAreaFlagID(stringValue);
	}
	else if (archive.isOutput())
	{
		if (!value.IsValid() && flagNamesList.size())
		{
			stringValue = flagNamesList[0].c_str();
			value = annotationsLib.GetAreaFlagID(stringValue);
		}
		else
		{
			stringValue = annotationsLib.GetAreaFlagName(value);
		}

		const int pos = flagNamesList.find(stringValue);
		bResult = archive(Serialization::StringListValue(flagNamesList, pos), szName, szLabel);
	}
	return bResult;
}

namespace NavigationSerialization
{
	inline void NavigationAreaCost::Serialize(Serialization::IArchive& archive)
	{
		archive(index, "id", "");
		archive(cost, "cost", "^");
	}

	inline void NavigationAreaFlagsMask::Serialize(Serialization::IArchive& archive)
	{
		if (!archive.isEdit())
		{
			archive(value, "mask");
			return;
		}

		INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
		const MNM::IAnnotationsLibrary& annotationsLib = pNavigationSystem->GetAnnotationLibrary();

		size_t flagsCount = annotationsLib.GetAreaFlagCount();
		if (archive.isOutput())
		{
			MNM::AreaAnnotation::value_type allValue = 0;
			for (size_t i = 0; i < flagsCount; ++i)
			{
				const MNM::SAreaFlag* pAreaFlag = annotationsLib.GetAreaFlag(i);
				CRY_ASSERT(pAreaFlag);
				allValue |= pAreaFlag->value;
			}

			bool allFlags = (value & allValue) == allValue;
			bool noFlags = (value == 0);
			archive(allFlags, "All", "All");
			archive(noFlags, "None", "None");

			for (size_t i = 0; i < flagsCount; ++i)
			{
				const MNM::SAreaFlag* pAreaFlag = annotationsLib.GetAreaFlag(i);
				CRY_ASSERT(pAreaFlag);

				bool flag = (pAreaFlag->value & value) != 0;
				archive(flag, pAreaFlag->name.c_str(), pAreaFlag->name.c_str());
			}
		}
		else
		{
			bool flag = false;
			MNM::AreaAnnotation::value_type allValue = 0;
			MNM::AreaAnnotation::value_type newValue = 0;

			for (size_t i = 0; i < flagsCount; ++i)
			{
				const MNM::SAreaFlag* pAreaFlag = annotationsLib.GetAreaFlag(i);
				CRY_ASSERT(pAreaFlag);

				archive(flag, pAreaFlag->name.c_str(), pAreaFlag->name.c_str());
				allValue |= pAreaFlag->value;
				if (flag)
				{
					newValue |= pAreaFlag->value;
				}
			}

			if (newValue != value)
			{
				value = newValue;
				return;
			}

			bool allFlag = false;
			bool noneFlag = false;
			archive(allFlag, "All", "All");
			archive(noneFlag, "None", "None");

			if (allFlag && newValue != allValue)
			{
				value = allValue;
			}
			else if (noneFlag && newValue != 0)
			{
				value = 0;
			}
		}
	}

	inline void NavigationQueryFilter::Serialize(Serialization::IArchive& archive)
	{
		INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
		const MNM::IAnnotationsLibrary& annotationsLib = pNavigationSystem->GetAnnotationLibrary();

		if (archive.openBlock("Flags", "Area Costs"))
		{
			if (archive.isOutput())
			{
				size_t areasCount = annotationsLib.GetAreaTypeCount();
				archive(areasCount, "AreasCount", "");

				for (size_t i = 0; i < areasCount; ++i)
				{
					const MNM::SAreaType* pAreaType = annotationsLib.GetAreaType(i);
					CRY_ASSERT(pAreaType);

					uint32 id = pAreaType->id;
					archive(NavigationAreaCost(id, variable.costs[pAreaType->id]), "AreaCost", pAreaType->name.c_str());
				}
			}
			else
			{
				size_t areasCount = 0;
				archive(areasCount, "AreasCount", "");

				for (size_t i = 0; i < areasCount; ++i)
				{
					uint32 id = -1;
					float cost = 1.0f;
					archive(NavigationAreaCost(id, cost), "AreaCost", "");

					if (id < MNM::AreaAnnotation::MaxAreasCount())
					{
						variable.costs[id] = cost;
					}
				}
			}
			archive.closeBlock();
		}

		archive(NavigationAreaFlagsMask(variable.includeFlags), "IncludeFlags", "IncludeFlags");
		archive(NavigationAreaFlagsMask(variable.excludeFlags), "ExcludeFlags", "ExcludeFlags");
	}
}