// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"

// forward declaratsion.
struct LightingSettings;
class CMissionScript;
class CCryEditDoc;
class CXmlArchive;

struct SMinimapInfo
{
	Vec2 vCenter;
	Vec2 vExtends;
	//	float RenderBoxSize;
	int  textureWidth;
	int  textureHeight;
	int  orientation;
};

/*!
    CMission represent single Game Mission on same map.
    Multiple Missions share same map, and stored in one .cry file.

 */
class SANDBOX_API CMission
{
public:
	//! Ctor of mission.
	CMission(CCryEditDoc* doc);
	//! Dtor of mission.
	virtual ~CMission();

	void           SetName(const CString& name)       { m_name = name; }
	const CString& GetName() const                    { return m_name; }

	void           SetDescription(const CString& dsc) { m_description = dsc; }
	const CString& GetDescription() const             { return m_description; }

	XmlNodeRef     GetEnvironemnt()                   { return m_environment; };

	//! Return weapons ammo definitions for this mission.
	XmlNodeRef GetWeaponsAmmo() { return m_weaponsAmmo; };

	//! Return lighting used for this mission.
	LightingSettings* GetLighting() const { return m_lighting; };

	//! Used weapons.
	void            GetUsedWeapons(std::vector<CString>& weapons);
	void            SetUsedWeapons(const std::vector<CString>& weapons);

	void            SetTime(float time)                           { m_time = time; };
	float           GetTime() const                               { return m_time; };

	void            SetPlayerEquipPack(const CString& sEquipPack) { m_sPlayerEquipPack = sEquipPack; }
	const CString&  GetPlayerEquipPack()                          { return m_sPlayerEquipPack; }

	CMissionScript* GetScript()                                   { return m_pScript; }

	//! Call OnReset callback
	void ResetScript();

	//! Called when this mission must be synchonized with current data in Document.
	//! if bRetrieve is true, data is retrieved from Mission to global structures.
	void SyncContent(bool bRetrieve, bool bIgnoreObjects, bool bSkipLoadingAI = false);

	//! Create clone of this mission.
	CMission* Clone();

	//! Serialize mission.
	void Serialize(CXmlArchive& ar, bool bParts = true);

	//! Serialize time of day
	void SerializeTimeOfDay(CXmlArchive& ar);

	//! Serialize environment
	void SerializeEnvironment(CXmlArchive& ar);

	//! Save some elements of mission to separate files
	void SaveParts();

	//! Load some elements of mission from separate files
	void LoadParts();

	//! Export mission to game.
	void Export(XmlNodeRef& root, XmlNodeRef& objectsNode);

	//! Export mission-animations to game.
	void ExportAnimations(XmlNodeRef& root);

	//! Add shared objects to mission objects.
	void AddObjectsNode(XmlNodeRef& node);
	void SetLayersNode(XmlNodeRef& node);

	void OnEnvironmentChange();
	int  GetNumCGFObjects() const { return m_numCGFObjects; };

	//////////////////////////////////////////////////////////////////////////
	// Minimap.
	void                SetMinimap(const SMinimapInfo& info);
	const SMinimapInfo& GetMinimap() const;

private:
	//! Update used weapons in game.
	void UpdateUsedWeapons();

	//! Document owner of this mission.
	CCryEditDoc* m_doc;

	CString      m_name;
	CString      m_description;

	CString      m_sPlayerEquipPack;

	//! Mission time;
	float m_time;

	//! Root node of objects defined only in this mission.
	XmlNodeRef m_objects;

	//! Object layers.
	XmlNodeRef m_layers;

	//! Exported data of this mission.
	XmlNodeRef m_exportData;

	//! Environment settings of this mission.
	XmlNodeRef m_environment;

	//! Weapons ammo definition.
	XmlNodeRef m_weaponsAmmo;

	XmlNodeRef m_Animations; // backward compatability.

	XmlNodeRef m_timeOfDay;

	//! Weapons used by this mission.
	std::vector<CString> m_usedWeapons;

	LightingSettings*    m_lighting;

	//! MissionScript Handling
	CMissionScript* m_pScript;
	int             m_numCGFObjects;

	SMinimapInfo    m_minimap;
};


