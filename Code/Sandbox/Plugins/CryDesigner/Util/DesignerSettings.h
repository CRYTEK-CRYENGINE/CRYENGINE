// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

class DesignerObject;

namespace Designer
{
struct DesignerSettings
{
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

extern DesignerSettings gDesignerSettings;
}
