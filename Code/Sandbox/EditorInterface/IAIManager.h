// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryMath/Cry_Geo.h>

class CBaseObject;
class CSOTemplate;

//! DEPRECATED, only there for smart object support
//! map of all known smart object templates mapped by template id
typedef std::map<int, CSOTemplate*> MapTemplates;


//! Interface for AIManager implementation in Sandbox 
struct IAIManager
{
public:
	virtual ~IAIManager() {}

	//! DEPRECATED, only there for smart object plugin support
	virtual const MapTemplates& GetMapTemplates() const = 0;

	//! DEPRECATED, only there for smart object plugin support
	virtual bool		NewAction(string& filename) = 0;

	virtual void GetSmartObjectActions(std::vector<string>& values) const = 0;

	//! Method for notifying AI system that area in the world has changed. This should be called when there is no corresponding physics event that AI could be listening to. 
	//! Set modifiedByObject parameter to object that triggered the change or null if there is no such object.
	virtual void OnAreaModified(const AABB& aabb, const CBaseObject* modifiedByObject = nullptr) = 0;

	//! Returns false when the AI system isn't ready for exporting game data and the user has canceled the export request
	virtual bool IsReadyToGameExport(unsigned int& adjustedExportFlags) const = 0;
};

