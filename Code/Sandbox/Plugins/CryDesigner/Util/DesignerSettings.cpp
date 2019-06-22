// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerSettings.h"

#include "Objects/AreaSolidObject.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"

#include "Objects/EnvironmentProbeObject.h"
#include "Objects/ObjectLoader.h"
#include "CryEditDoc.h"
#include "Mission.h"

#include <Preferences/ViewportPreferences.h>
#include <RenderViewport.h>

#include <Cry3DEngine/ITimeOfDay.h>
#include <CrySerialization/Decorators/Range.h>

namespace Designer
{
DesignerSettings gDesignerSettings;

DesignerSettings::DesignerSettings()
{
	bDisplayBackFaces = false;
	bKeepPivotCentered = false;
	bHighlightElements = true;
	fHighlightBoxSize = 24 * 0.001f;
	bSeamlessEdit = false;
	bDisplayTriangulation = false;
	bDisplayDimensionHelper = true;
	bDisplaySubdividedResult = true;
	bDisplayVertexNormals = false;
	bDisplayPolygonNormals = false;
}

void DesignerSettings::Load()
{
	LoadSettings(Serialization::SStruct(*this), "DesignerSetting");
}

void DesignerSettings::Save()
{
	SaveSettings(Serialization::SStruct(*this), "DesignerSetting");
}

void DesignerSettings::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("designer_properties", "Properties"))
	{
		if (ar.isEdit())
		{
			ar(bDisplayBackFaces, "DisplayBackFaces", "Display Back Facing Polygons");
		}
		ar(bSeamlessEdit, "SeamlessEdit", "Seamless Edit");
		ar(bKeepPivotCentered, "KeepPivotCenter", "Keep Pivot Centered");
		ar(bHighlightElements, "HighlightElements", "Highlight Elements");
		ar(Serialization::Range(fHighlightBoxSize, 0.005f, 2.0f), "HighlightBoxSize", "Highlight Box Size");
		ar(bDisplayDimensionHelper, "DisplayDimensionHelper", "Display Dimension Helper");
		ar(bDisplayTriangulation, "DisplayTriangulation", "Display Triangulation");
		ar(bDisplaySubdividedResult, "DisplaySubdividedResult", "Display Subdivided Result");
		ar(bDisplayVertexNormals, "DisplayVertexNormals", "Display Vertex Normals");
		ar(bDisplayPolygonNormals, "DisplayPolygonNormals", "Display Polygon Normals");

		ar.closeBlock();
	}
}

void DesignerSettings::Update(bool continuous)
{
	if (bKeepPivotCentered)
		ApplyPostProcess(DesignerSession::GetInstance()->GetMainContext(), ePostProcess_CenterPivot | ePostProcess_Mesh);
	Save();
}
}
