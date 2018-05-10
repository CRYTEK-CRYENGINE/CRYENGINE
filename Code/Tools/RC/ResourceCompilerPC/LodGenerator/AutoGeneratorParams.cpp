// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorParams.h"

namespace LODGenerator 
{
	CAutoGeneratorParams::CAutoGeneratorParams()
	{
		m_pMaterialVarBlock = new CVarBlock();
		m_pMaterialVarBlock->AddVariable(m_fRayLength,"RayTestLength");
		m_fRayLength->SetHumanName("Ray Test Length");
		m_fRayLength->SetDescription("The length of the ray used to hit the mesh surface for sampling");
		m_fRayLength->SetLimits(0.001f, 100.0f, 0.25f, true, false);
		m_fRayLength=0.5f;

		m_pMaterialVarBlock->AddVariable(m_fRayStartDist,"RayStartDist");
		m_fRayStartDist->SetHumanName("Ray Start Distance");
		m_fRayStartDist->SetDescription("The distance from the case to start the ray tracing");
		m_fRayStartDist->SetLimits(0.001f, 100.0f, 0.25f, true, false);
		m_fRayStartDist=0.25f;

		m_pMaterialVarBlock->AddVariable(m_bBakeAlpha,"BakeAlpha");
		m_bBakeAlpha->SetHumanName("Bake the Alpha Channel");
		m_bBakeAlpha->SetDescription("Enables baking of alpha into the alpha channel");
		m_bBakeAlpha=false;

		m_pMaterialVarBlock->AddVariable(m_bBakeSpec,"BakeUniqueSpecularMap");
		m_bBakeSpec->SetHumanName("Bake A Unique Specular Map");
		m_bBakeSpec->SetDescription("toggles the baking of a full unique specular map per material lod");
		m_bBakeSpec=false;


		m_pMaterialVarBlock->AddVariable(m_bSmoothCage,"SmoothCage");
		m_bSmoothCage->SetHumanName("Smooth Cage");
		m_bSmoothCage->SetDescription("Uses averge face normal instead of vertex normal for ray casting");
		m_bSmoothCage=false;

		m_pMaterialVarBlock->AddVariable(m_bDilationPass,"DilationPass");
		m_bDilationPass->SetHumanName("Dilation Pass");
		m_bDilationPass->SetDescription("Reduces filtering errors by filling in none baked areas");
		m_bDilationPass=true;

		m_pMaterialVarBlock->AddVariable(m_cBackgroundColour,"BackgroundColour");
		m_cBackgroundColour->SetHumanName("Background Colour");
		m_cBackgroundColour->SetDescription("Used to fill any areas of the texture that didn't get filled by the ray casting");
		m_cBackgroundColour->SetDataType(IVariable::DT_COLOR);
		m_cBackgroundColour=0;

		m_pMaterialVarBlock->AddVariable(m_strExportPath,"ExportPath");
		m_strExportPath->SetHumanName("Export Path");
		m_strExportPath->SetDescription("Where to export the baked textures");

		m_pMaterialVarBlock->AddVariable(m_strFilename,"FilenameTemplate");
		m_strFilename->SetHumanName("Filename Pattern");
		m_strFilename->SetDescription("Filename pattern template to use for the newly created texture filenames");

		m_pMaterialVarBlock->AddVariable(m_strPresetDiffuse,"DiffuseTexturePreset");
		m_strPresetDiffuse->SetHumanName("Diffuse Preset");
		m_strPresetDiffuse->SetDescription("The RC preset for the Albedo Map texture");

		m_pMaterialVarBlock->AddVariable(m_strPresetNormal,"NormalTexturePreset");
		m_strPresetNormal->SetHumanName("Normal Preset");
		m_strPresetNormal->SetDescription("The RC preset for the Normal Map texture");

		m_pMaterialVarBlock->AddVariable(m_strPresetSpecular,"SpecularTexturePreset");
		m_strPresetSpecular->SetHumanName("Specular Preset");
		m_strPresetSpecular->SetDescription("The RC preset for the Reflectance Map texture");

		m_pMaterialVarBlock->AddVariable(m_useAutoTextureSize,"UseAutoTextureSize");
		m_useAutoTextureSize->SetHumanName("Use Auto Texture Size");
		m_useAutoTextureSize->SetDescription("Choose the texture size based on the size of the object");
		m_useAutoTextureSize->SetFlags(IVariable::UI_INVISIBLE);

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius1,"AutoTextureRadius1");
		m_autoTextureRadius1->SetHumanName("Auto Texture Radius 1");
		m_autoTextureRadius1->SetDescription("The radius for auto size option 1");
		m_autoTextureRadius1->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius1=8.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize1,"AutoTextureSize1");
		m_autoTextureSize1->SetHumanName("Auto Texture Size 1");
		m_autoTextureSize1->SetDescription("The texture size for auto size option 1");
		m_autoTextureSize1->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize1=1024;

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius2,"AutoTextureRadius2");
		m_autoTextureRadius2->SetHumanName("Auto Texture Radius 2");
		m_autoTextureRadius2->SetDescription("The radius for auto size option 2");
		m_autoTextureRadius2->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius2=4.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize2,"AutoTextureSize2");
		m_autoTextureSize2->SetHumanName("Auto Texture Size 2");
		m_autoTextureSize2->SetDescription("The texture size for auto size option 2");
		m_autoTextureSize2->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize2=512;

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius3,"AutoTextureRadius3");
		m_autoTextureRadius3->SetHumanName("Auto Texture Radius 3");
		m_autoTextureRadius3->SetDescription("The radius for auto size option 3");
		m_autoTextureRadius3->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius3=0.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize3,"AutoTextureSize3");
		m_autoTextureSize3->SetHumanName("Auto Texture Size 3");
		m_autoTextureSize3->SetDescription("The texture size for auto size option 3");
		m_autoTextureSize3->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize3=128;

		m_strPresetDiffuse = "Albedo /reduce=1";
		m_strPresetNormal = "NormalsWithSmoothness /reduce=1";
		m_strPresetSpecular = "Reflectance /reduce=1";

		m_pGeometryVarBlock = new CVarBlock();

		m_pGeometryVarBlock->AddVariable(m_nSourceLod,"nSourceLod");
		m_nSourceLod->SetHumanName("Source Lod");
		m_nSourceLod->SetDescription("Source Lod to generate the Lod chain from");

		m_pGeometryVarBlock->AddVariable(m_bObjectHasBase, "bObjectHasBase");
		m_bObjectHasBase->SetHumanName("Object has base");
		m_bObjectHasBase->SetDescription("If object doesn't have a base then we won't have use any views looking up at it when calculating error.");
		m_bObjectHasBase=false;

		m_pGeometryVarBlock->AddVariable(m_fViewResolution, "fViewResolution");
		m_fViewResolution->SetHumanName("Pixels per metre");
		m_fViewResolution->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the resolution of these views. A low value means quicker results but potentially worst accuracy. This is in pixels per meter.");
		m_fViewResolution=25;
		m_fViewResolution->SetLimits(0.0001f, 1024.0f, 1.0f, true, false);

		m_pGeometryVarBlock->AddVariable(m_nViewsAround, "nViewsAround");
		m_nViewsAround->SetHumanName("Views around");
		m_nViewsAround->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the number of views around the object. Each elevation has a ring of this many views. Less views means quicker results but potentially worst accuracy.");
		m_nViewsAround->SetLimits(1, 128);
		m_nViewsAround=12;

		m_pGeometryVarBlock->AddVariable(m_nViewElevations, "nViewElevations");
		m_nViewElevations->SetHumanName("View elevations");
		m_nViewElevations->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the number of levels at which views are taken. Less views means quicker results but potentially worst accuracy.");
		m_nViewElevations=3;
		m_nViewElevations->SetLimits(1, 128);

		m_pGeometryVarBlock->AddVariable(m_fSilhouetteWeight, "fSilhouetteWeight");
		m_fSilhouetteWeight->SetHumanName("Silhouette weighting");
		m_fSilhouetteWeight->SetDescription("Changes the weighting given to silhouette changes. A low value means a silhouette change isn't considered much than other changes in depth. A high value means the silhouette is preserved above all else.");
		m_fSilhouetteWeight=5.0f;
		m_fSilhouetteWeight->SetLimits(0.0f, 1000.0f, 1.0f, true, false);

		m_pGeometryVarBlock->AddVariable(m_fVertexWelding, "fVertexWelding");
		m_fVertexWelding->SetHumanName("Vertex weld distance");
		m_fVertexWelding->SetDescription("Before starting the LOD process vertices within this this distance will be welded together.");
		m_fVertexWelding=0.001f;
		m_fVertexWelding->SetLimits(0.0f, 0.1f, 0.001f, true, false);

		m_pGeometryVarBlock->AddVariable(m_bCheckTopology, "bCheckTopology");
		m_bCheckTopology->SetHumanName("Check topology is correct");
		m_bCheckTopology->SetDescription("If a move makes a bad bit topology (a bow tie or more than two triangles sharing a single end) then that move is rejected. This can cause problems if the topology is bad to begin with (or caused by vertex welding).");
		m_bCheckTopology=true;

		m_pGeometryVarBlock->AddVariable(m_bWireframe, "bWireframe");
		m_bWireframe = true;
		m_bWireframe->SetDescription("Toggles wireframe preview of the generated lod");
		m_bWireframe->SetHumanName("Toggle Wireframe");

		m_pGeometryVarBlock->AddVariable(m_bExportObj, "bExportObj");
		m_bExportObj = true;
		m_bExportObj->SetDescription("Toggles saving the generated geometry and uvs to an obj file");
		m_bExportObj->SetHumanName("Toggle obj export");

		m_pGeometryVarBlock->AddVariable(m_bAddToParentMaterial, "bAddToParentMaterial");
		m_bAddToParentMaterial = true;
		m_bAddToParentMaterial->SetDescription("If true, adds the new lod material to the parent else creates a separate lod material");
		m_bAddToParentMaterial->SetHumanName("Add to parent material");

		m_pGeometryVarBlock->AddVariable(m_bUseCageMesh, "bUseCageMesh");
		m_bUseCageMesh = false;
		m_bUseCageMesh->SetDescription("If true, will use a lod cage for generating the lod");
		m_bUseCageMesh->SetHumanName("Use lod cage");

		m_pGeometryVarBlock->AddVariable(m_bPreviewSourceLod, "bSourceLod");
		m_bPreviewSourceLod->SetHumanName("PreviewSourceLod");
		m_bPreviewSourceLod->SetDescription("Toggles on the preview of the Source Lod");
	}

	CAutoGeneratorParams::~CAutoGeneratorParams()
	{

	}

	void CAutoGeneratorParams::EnableUpdateCallbacks(bool enable)
	{
		m_pGeometryVarBlock->EnableUpdateCallbacks(enable);
		m_pMaterialVarBlock->EnableUpdateCallbacks(enable);
	}
}