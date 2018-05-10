// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/*
   This class is needed to keep track of the navigation shapes created in Sandbox and their
   respective IDs. It's necessary to keep consistency with the exported Navigation data
   since the AISystem has no knowledge of the Editor's shape
 */

class CVolumesManager : public ISystemEventListener
{
public:
	CVolumesManager();
	~CVolumesManager();

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	bool               RegisterArea(const char* volumeName, NavigationVolumeID& outVolumeId);
	void               UnRegisterArea(const char* volumeName);

	void               Clear();

	void               RegisterAreaFromLoadedData(const char* szVolumeName, NavigationVolumeID id);
	void               ClearLoadedAreas();
	void               ValidateAndSanitizeLoadedAreas(INavigationSystem& navigationSystem);

	bool               SetAreaID(const char* volumeName, NavigationVolumeID id);
	void               InvalidateID(NavigationVolumeID id);
	void               UpdateNameForAreaID(const NavigationVolumeID id, const char* newName);

	bool               IsAreaPresent(const char* volumeName) const;
	NavigationVolumeID GetAreaID(const char* volumeName) const;
	bool               GetAreaName(NavigationVolumeID id, string& name) const;

	void               GetVolumesNames(std::vector<string>& names) const;

	bool               IsLoadedAreaPresent(const char* volumeName) const;
	NavigationVolumeID GetLoadedAreaID(const char* volumeName) const;
	void               GetLoadedUnregisteredVolumes(std::vector<NavigationVolumeID>& volumes) const;

	bool               RegisterEntityMarkups(INavigationSystem& navigationSystem, const IEntity& entity, const char** pShapeNamesArray, const size_t count, NavigationVolumeID* pOutIds);
	bool               UnregisterEntityMarkups(INavigationSystem& navigationSystem, const IEntity& entity);

	void               SaveData(CCryFile& file, const uint16 version) const;
	void               LoadData(CCryFile& file, const uint16 version);

private:
	typedef std::map<string, NavigationVolumeID> VolumesMap;
	typedef std::unordered_map<EntityGUID, VolumesMap> EntitiesWithMarkupsMap;
	VolumesMap m_volumeAreas;
	VolumesMap m_loadedVolumeAreas;

	EntitiesWithMarkupsMap m_registeredEntityMarkupsMap;
};

