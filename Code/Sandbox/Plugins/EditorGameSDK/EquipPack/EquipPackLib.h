// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <map>

class CEquipPack;
typedef std::map<string, CEquipPack*> TEquipPackMap;

class CEquipPackLib
{
public:
	CEquipPackLib();
	~CEquipPackLib();
	CEquipPack* CreateEquipPack(const string& name);
	// currently we ignore the bDeleteFromDisk.
	// will have to be manually removed via Source control
	bool                 RemoveEquipPack(const string& name, bool bDeleteFromDisk = false);
	bool                 RenameEquipPack(const string& name, const string& newName);
	CEquipPack*          FindEquipPack(const string& name);

	void                 Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bResetWhenLoad = true);
	bool                 LoadLibs(bool bExportToGame);
	bool                 SaveLibs(bool bExportToGame);
	void                 ExportToGame();
	void                 Reset();
	int                  Count() const             { return m_equipmentPacks.size(); }
	const TEquipPackMap& GetEquipmentPacks() const { return m_equipmentPacks; }
	static CEquipPackLib& GetRootEquipPack() { return s_rootEquips; }
private:
	void                 UpdateEnumDatabase();

	static CEquipPackLib s_rootEquips;
	TEquipPackMap        m_equipmentPacks;
};

