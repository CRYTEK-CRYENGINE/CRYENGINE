// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VolumesManager.h"

bool CVolumesManager::RegisterArea(const char* volumeName, NavigationVolumeID& outVolumeId)
{
	NavigationVolumeID volumeId = NavigationVolumeID();

	{
		VolumesMap::const_iterator it = m_loadedVolumeAreas.find(volumeName);
		if (it != m_loadedVolumeAreas.end())
		{
			volumeId = it->second;
			AILogComment("CVolumesManager::RegisterArea: area '%s' is already loaded with the id = %u", volumeName, (uint32)volumeId);
		}
	}

	VolumesMap::const_iterator it = m_volumeAreas.find(volumeName);
	if (it == m_volumeAreas.end())
	{
		m_volumeAreas[volumeName] = volumeId;
		outVolumeId = volumeId;
		AILogComment("CVolumesManager::RegisterArea: registering new area '%s' with the id = %u", volumeName, (uint32)volumeId);
		return true;
	}

	AIWarning("You are trying to register the area %s but it's already registered with id = %u.", volumeName, (uint32)volumeId);
	return false;
}

void CVolumesManager::RegisterAreaFromLoadedData(const char* szVolumeName, NavigationVolumeID id)
{
	if (m_loadedVolumeAreas.find(szVolumeName) != m_loadedVolumeAreas.end())
	{
		AIWarning("You are trying to register the loaded area '%s' but it's already registered.", szVolumeName);
	}
	m_loadedVolumeAreas[szVolumeName] = id;
}

void CVolumesManager::ClearLoadedAreas()
{
	m_loadedVolumeAreas.clear();
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
