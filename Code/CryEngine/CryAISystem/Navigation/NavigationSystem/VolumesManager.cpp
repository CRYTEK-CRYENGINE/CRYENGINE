// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "VolumesManager.h"

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

void CVolumesManager::ValidateAndSanitizeLoadedAreas(const INavigationSystem& navigationSystem)
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
