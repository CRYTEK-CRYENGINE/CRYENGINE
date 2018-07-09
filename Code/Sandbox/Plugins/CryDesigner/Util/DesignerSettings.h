// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

class DesignerObject;

namespace Designer
{
struct ConsoleVarsForExcluisveMode
{
	ConsoleVarsForExcluisveMode()
	{
		memset(this, 0, sizeof(*this));
	}
	int r_DisplayInfo;
	int r_HDRRendering;
	int r_PostProcessEffects;
	int r_ssdo;
	int e_Vegetation;
	int e_WaterOcean;
	int e_WaterVolumes;
	int e_Terrain;
	int e_Shadows;
	int e_Particles;
	int e_Clouds;
	int e_SkyBox;
};

struct DesignerExclusiveMode
{
	DesignerExclusiveMode()
	{
		m_OldTimeOfDay = NULL;
		m_OldTimeOfTOD = 0;
		m_OldCameraTM = Matrix34::CreateIdentity();
		m_OldObjectHideMask = 0;
		m_bOldLockCameraMovement = false;

		m_bExclusiveMode = false;
	}

	void EnableExclusiveMode(bool bEnable);
	void SetTimeOfDayForExclusiveMode();
	void SetCVForExclusiveMode();
	void SetObjectsFlagForExclusiveMode();

	void RestoreTimeOfDay();
	void RestoreCV();
	void RestoreObjectsFlag();

	void CenterCameraForExclusiveMode();

	void SetTime(ITimeOfDay* pTOD, float fTime);

	ConsoleVarsForExcluisveMode  m_OldConsoleVars;
	std::map<CBaseObject*, bool> m_ObjectHiddenFlagMap;
	XmlNodeRef                   m_OldTimeOfDay;
	float                        m_OldTimeOfTOD;
	Matrix34                     m_OldCameraTM;
	int                          m_OldObjectHideMask;
	bool                         m_bOldLockCameraMovement;

	bool                         m_bExclusiveMode;
};

struct DesignerSettings
{
	bool  bExclusiveMode;
	bool  bSeamlessEdit;
	bool  bDisplayBackFaces;
	bool  bKeepPivotCentered;
	bool  bHighlightElements;
	float fHighlightBoxSize;
	bool  bDisplayDimensionHelper;
	bool  bDisplayTriangulation;
	bool  bDisplaySubdividedResult;
	bool  bDisplayVertexNormals;
	bool  bDisplayPolygonNormals;

	void Load();
	void Save();

	void Update(bool continuous);

	DesignerSettings();
	void Serialize(Serialization::IArchive& ar);
};

extern DesignerExclusiveMode gExclusiveModeSettings;
extern DesignerSettings gDesignerSettings;
}

