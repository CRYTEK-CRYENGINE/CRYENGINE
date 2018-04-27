// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VolumesManager.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"

CVolumesManager::CVolumesManager()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "NavigationVolumesManager");
}

CVolumesManager::~CVolumesManager()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
}

void CVolumesManager::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (event == ESYSTEM_EVENT_LEVEL_LOAD_END)
	{
		// Check if all loaded markups have still their owning entity in a level		
		for (auto iter = m_registeredEntityMarkupsMap.cbegin(); iter != m_registeredEntityMarkupsMap.cend();)
		{
			if (gEnv->pEntitySystem->FindEntityByGuid(iter->first) == INVALID_ENTITYID)
			{
				// Entity doesn't exist, remove markups
				for (const auto& nameAndId : iter->second)
				{
					gEnv->pAISystem->GetNavigationSystem()->DestroyMarkupVolume(nameAndId.second);
				}

				iter = m_registeredEntityMarkupsMap.erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
}

bool CVolumesManager::RegisterArea(const char* volumeName, NavigationVolumeID& outVolumeId)
{
	NavigationVolumeID volumeId = NavigationVolumeID();
	bool bResult = false;

	VolumesMap::const_iterator it = m_volumeAreas.find(volumeName);
	if (it == m_volumeAreas.end())
	{
		{
			VolumesMap::const_iterator it = m_loadedVolumeAreas.find(volumeName);
			if (it != m_loadedVolumeAreas.end())
			{
				volumeId = it->second;
				AILogComment("CVolumesManager::RegisterArea: area '%s' is already loaded with the id = %u", volumeName, (uint32)volumeId);
				m_loadedVolumeAreas.erase(it);
			}
		}

		m_volumeAreas[volumeName] = volumeId;

		AILogComment("CVolumesManager::RegisterArea: registering new area '%s' with the id = %u", volumeName, (uint32)volumeId);
		bResult = true;
	}
	else
	{
		AIWarning("You are trying to register the area %s but it's already registered with id = %u.", volumeName, it->second);
		bResult = false;
	}

	outVolumeId = volumeId;
	return bResult;
}

void CVolumesManager::RegisterAreaFromLoadedData(const char* szVolumeName, NavigationVolumeID id)
{
	if (m_loadedVolumeAreas.find(szVolumeName) != m_loadedVolumeAreas.end())
	{
		AIWarning("You are trying to register the loaded area '%s' but it's already registered.", szVolumeName);
	}
	AILogComment("CVolumesManager::RegisterAreaFromLoadedData %s = %u", szVolumeName, (uint32)id);
	m_loadedVolumeAreas[szVolumeName] = id;
}

void CVolumesManager::ClearLoadedAreas()
{
	m_loadedVolumeAreas.clear();
}

void CVolumesManager::ValidateAndSanitizeLoadedAreas(INavigationSystem& navigationSystem)
{
	for (auto iter = m_loadedVolumeAreas.begin(); iter != m_loadedVolumeAreas.end(); )
	{
		if (!navigationSystem.ValidateVolume(iter->second))
		{
			AIWarning("CVolumesManager::ValidateAndSanitizeLoadedAreas: Loaded area '%s' referenced volume '%u', which was not created for loaded navigation mesh. Reference is removed.",
			          iter->first.c_str(), (uint32)iter->second);

			iter = m_loadedVolumeAreas.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

bool CVolumesManager::SetAreaID(const char* volumeName, NavigationVolumeID id)
{
	if (IsAreaPresent(volumeName))
	{
		m_volumeAreas[volumeName] = id;
	}
	else
	{
		AIWarning("The area %s is not registered in the system.", volumeName);
		return false;
	}
	return true;
}

void CVolumesManager::UnRegisterArea(const char* volumeName)
{
	VolumesMap::iterator it = m_volumeAreas.find(volumeName);
	if (it != m_volumeAreas.end())
		m_volumeAreas.erase(it);
}

void CVolumesManager::Clear()
{
	m_volumeAreas.clear();
	m_loadedVolumeAreas.clear();

	m_registeredEntityMarkupsMap.clear();
}

bool CVolumesManager::IsAreaPresent(const char* volumeName) const
{
	VolumesMap::const_iterator it = m_volumeAreas.find(volumeName);
	return it != m_volumeAreas.end();
}

NavigationVolumeID CVolumesManager::GetAreaID(const char* volumeName) const
{
	NavigationVolumeID areaID;
	VolumesMap::const_iterator it = m_volumeAreas.find(volumeName);
	if (it != m_volumeAreas.end())
		areaID = it->second;
	return areaID;
}

bool CVolumesManager::GetAreaName(NavigationVolumeID id, string& name) const
{
	VolumesMap::const_iterator it = m_volumeAreas.begin();
	VolumesMap::const_iterator end = m_volumeAreas.end();
	for (; it != end; ++it)
	{
		if (it->second == id)
		{
			name = it->first;
			return true;
		}
	}
	return false;
}

void CVolumesManager::UpdateNameForAreaID(const NavigationVolumeID id, const char* newName)
{
	VolumesMap::const_iterator it = m_volumeAreas.begin();
	VolumesMap::const_iterator end = m_volumeAreas.end();
	for (; it != end; ++it)
	{
		if (it->second == id)
		{
			m_volumeAreas.erase(it->first);
			break;
		}
	}
	m_volumeAreas[newName] = id;
}

void CVolumesManager::InvalidateID(NavigationVolumeID id)
{
	VolumesMap::iterator it = m_volumeAreas.begin();
	VolumesMap::iterator end = m_volumeAreas.end();
	for (; it != end; ++it)
	{
		if (it->second == id)
		{
			it->second = NavigationVolumeID();
			return;
		}
	}
	AIWarning("There is no navigation shape with assigned the ID:%d", (uint32) id);
}

void CVolumesManager::GetVolumesNames(std::vector<string>& names) const
{
	names.reserve(m_volumeAreas.size());
	VolumesMap::const_iterator it = m_volumeAreas.begin();
	VolumesMap::const_iterator end = m_volumeAreas.end();
	for (; it != end; ++it)
	{
		names.push_back(it->first);
	}
}

bool CVolumesManager::IsLoadedAreaPresent(const char* volumeName) const
{
	VolumesMap::const_iterator it = m_loadedVolumeAreas.find(volumeName);
	return it != m_loadedVolumeAreas.end();
}

NavigationVolumeID CVolumesManager::GetLoadedAreaID(const char* volumeName) const
{
	NavigationVolumeID areaID;
	VolumesMap::const_iterator it = m_loadedVolumeAreas.find(volumeName);
	if (it != m_loadedVolumeAreas.end())
		areaID = it->second;
	return areaID;
}

void CVolumesManager::GetLoadedUnregisteredVolumes(std::vector<NavigationVolumeID>& volumes) const
{
	volumes.clear();
	if (m_loadedVolumeAreas.size() > m_volumeAreas.size())
	{
		volumes.reserve(m_loadedVolumeAreas.size() - m_volumeAreas.size());
	}

	for (const auto& volumeNameIdPair : m_loadedVolumeAreas)
	{
		const string& name = volumeNameIdPair.first;
		if (m_volumeAreas.find(name) == m_volumeAreas.end())
		{
			volumes.push_back(volumeNameIdPair.second);
		}
	}
}

bool CVolumesManager::RegisterEntityMarkups(INavigationSystem& navigationSystem, const IEntity& entity, const char** pShapeNamesArray, const size_t count, NavigationVolumeID* pOutIds)
{
	if (count == 0)
		return false;
	
	const EntityGUID entityGuid = entity.GetGuid();
	if (entityGuid.IsNull())
		return false;

	VolumesMap& shapeNameToIdMap = m_registeredEntityMarkupsMap[entityGuid];

	// Remove markup volumes with names, that aren't in pShapeNamesArray
	for (VolumesMap::const_iterator it = shapeNameToIdMap.cbegin(); it != shapeNameToIdMap.cend();)
	{
		bool isRegistering = false;
		for (size_t i = 0; i < count; ++i)
		{
			if (it->first.compare(pShapeNamesArray[i]) == 0)
			{
				isRegistering = true;
				break;
			}
		}
		if (!isRegistering)
		{
			navigationSystem.DestroyMarkupVolume(it->second);
			it = shapeNameToIdMap.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (size_t i = 0; i < count; ++i)
	{
		NavigationVolumeID volumeId = NavigationVolumeID();
		
		bool isDuplicate = false;
		const char* szShapeName = pShapeNamesArray[i];
		for (size_t j = 0; j < i; ++j)
		{
			if (strcmp(szShapeName, pShapeNamesArray[j]) == 0)
			{
				isDuplicate = true;
				break;
			}
		}
		if (!isDuplicate)
		{
			const VolumesMap::const_iterator findIt = shapeNameToIdMap.find(szShapeName);
			if (findIt != shapeNameToIdMap.end())
			{
				volumeId = findIt->second;
			}
			else
			{
				volumeId = navigationSystem.CreateMarkupVolume(NavigationVolumeID());
				shapeNameToIdMap[szShapeName] = volumeId;
			}
		}
		pOutIds[i] = volumeId;
	}
	return true;
}

bool CVolumesManager::UnregisterEntityMarkups(INavigationSystem& navigationSystem, const IEntity& entity)
{
	const EntityGUID entityGuid = entity.GetGuid();
	if (entityGuid.IsNull())
		return false;

	const EntitiesWithMarkupsMap::const_iterator findIt = m_registeredEntityMarkupsMap.find(entityGuid);
	if (findIt == m_registeredEntityMarkupsMap.end())
		return false;

	const VolumesMap& namesAndIds = findIt->second;
	for (const auto& nameAndId : namesAndIds)
	{
		navigationSystem.DestroyMarkupVolume(nameAndId.second);
	}
	m_registeredEntityMarkupsMap.erase(findIt);

	return true;
}

void CVolumesManager::SaveData(CCryFile& file, const uint16 version) const
{
	// Note: change eBAINavigationFileVersion in NavigationSystem.cpp after any new change
	if (version >= NavigationSystem::eBAINavigationFileVersion::ENTITY_MARKUP_GUIDS)
	{
		const uint32 entitiesCount = static_cast<uint32>(m_registeredEntityMarkupsMap.size());

		file.WriteType(&entitiesCount);
		for (const auto& guidAndMarkups : m_registeredEntityMarkupsMap)
		{
			const EntityGUID& guid = guidAndMarkups.first;

			file.WriteType(&guid);

			const uint32 shapesCount = static_cast<uint32>(guidAndMarkups.second.size());
			file.WriteType(&shapesCount);

			for (const auto& nameAndId : guidAndMarkups.second)
			{
				const string& name = nameAndId.first;
				const uint32 nameSize = static_cast<uint32>(name.size());

				file.WriteType(&nameSize);
				file.WriteType(name.c_str(), nameSize);

				const uint32 volumeId = static_cast<uint32>(nameAndId.second);
				file.WriteType(&volumeId);
			}
		}
	}
}

void CVolumesManager::LoadData(CCryFile& file, const uint16 version)
{
	if (version >= NavigationSystem::eBAINavigationFileVersion::ENTITY_MARKUP_GUIDS)
	{
		std::vector<char> nameBuffer;
		string name;
		uint32 nameSize;

		uint32 entitiesCount = 0;
		file.ReadType(&entitiesCount);
		for (uint32 eIdx = 0; eIdx < entitiesCount; ++eIdx)
		{
			EntityGUID entityGuid;
			file.ReadTypeRaw(&entityGuid);

			VolumesMap& namesAndIds = m_registeredEntityMarkupsMap[entityGuid];

			uint32 shapesCount = 0;
			file.ReadType(&shapesCount);

			for (uint32 sIdx = 0; sIdx < shapesCount; ++sIdx)
			{
				file.ReadType(&nameSize);
				if (nameSize > 0)
				{
					nameBuffer.resize(nameSize, '\0');
					file.ReadType(&nameBuffer.front(), nameSize);

					name.assign(&nameBuffer.front(), (&nameBuffer.back()) + 1);
				}
				else
				{
					name.clear();
				}

				uint32 volumeId = 0;
				file.ReadType(&volumeId);

				namesAndIds.emplace(name, NavigationVolumeID(volumeId));
			}
		}
	}
}
