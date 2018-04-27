// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <CryAISystem/INavigation.h>
#include <CryAISystem/IAISystem.h>
#include "CAISystem.h"

class CNavigation
{
public:
	explicit CNavigation(ISystem* pSystem);
	~CNavigation();

	const ShapeMap& GetDesignerPaths() const { return m_mapDesignerPaths; }

	// copies a designer path into provided list if a path of such name is found
	bool GetDesignerPath(const char* szName, SShape& path) const;

	bool Init();
	void Reset(IAISystem::EResetReason reason);
	void ShutDown();
	void FlushSystemNavigation();
	// Gets called after loading the mission
	void OnMissionLoaded();
	void ValidateNavigation();

	void Serialize(TSerialize ser);

	// writes areas into file
	void WritePolygonArea(CCryFile& file, const string& name, const ListPositions& pts);
	void WriteAreasIntoFile(const char* fileName);

	// Prompt the exporting of data from AI - these should require not-too-much processing - the volume
	// stuff requires more processing so isn't done for a normal export
	void ExportData(const char* navFileName, const char* areasFileName, const char* roadsFileName,
		const char* vertsFileName, const char* volumeFileName,
		const char* flightFileName);

	void          FlushAllAreas();
	/// Check all the forbidden areas are sensible size etc
	bool          ValidateAreas();
	
	/// Checks if navigation shape exists - called by editor
	bool        DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road = false);
	
	bool CreateNavigationShape(const SNavigationShapeParams& params);
	/// Deletes designer created path/shape - called by editor
	void DeleteNavigationShape(const char* szName);

	void GetMemoryStatistics(ICrySizer* pSizer);

	/// Meant for debug draws, there shouldn't be anything else to update here.
	void Update() const;

private:

	PerceptionModifierShapeMap m_mapPerceptionModifiers;
	ShapeMap                   m_mapOcclusionPlanes;
	ShapeMap                   m_mapGenericShapes;
	ShapeMap                   m_mapDesignerPaths;

	ISystem*                   m_pSystem;
};

#endif // NAVIGATION_H

