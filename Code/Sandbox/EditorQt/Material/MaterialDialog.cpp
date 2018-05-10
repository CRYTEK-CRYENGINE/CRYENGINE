// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Material.h"

#include "MaterialImageListCtrl.h"
#include "MaterialManager.h"
#include "MaterialHelpers.h"
#include "MaterialLibrary.h"

#include "ViewManager.h"
#include "Util/Clipboard.h"

#include "Controls/PropertyItem.h"
#include "Util/CubemapUtils.h"
#include "Controls/SharedFonts.h"

#include <Cry3DEngine/I3DEngine.h>
#include <QtViewPane.h>

#include "MatEditPreviewDlg.h"
#include "MaterialDialog.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include "Controls/QuestionDialog.h"

#define IDW_MTL_BROWSER_PANE    AFX_IDW_CONTROLBAR_FIRST + 10
#define IDW_MTL_PROPERTIES_PANE AFX_IDW_CONTROLBAR_FIRST + 11
#define IDW_MTL_PREVIEW_PANE    AFX_IDW_CONTROLBAR_FIRST + 12

#define EDITOR_OBJECTS_PATH     CString("Objects\\Editor\\")

class CMaterialEditorClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()   override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return MATERIAL_EDITOR_NAME; };
	virtual const char*    Category()        override { return "Editor"; };
	virtual const char*    GetMenuPath()     override { return ""; }
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CMaterialDialog); };
	virtual const char*    GetPaneTitle()    override { return _T(MATERIAL_EDITOR_NAME); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CMaterialEditorClass);

IMPLEMENT_DYNCREATE(CMaterialDialog, CBaseFrameWnd);
//////////////////////////////////////////////////////////////////////////
// Material structures.
//////////////////////////////////////////////////////////////////////////

struct STextureVars
{
	// As asked by Martin Mittring, removed the amount parameter.
	//CSmartVariable<int> amount;
	CSmartVariable<bool>    is_tile[2];

	CSmartVariableEnum<int> etcgentype;
	CSmartVariableEnum<int> etcmrotatetype;
	CSmartVariableEnum<int> etcmumovetype;
	CSmartVariableEnum<int> etcmvmovetype;
	CSmartVariableEnum<int> etextype;
	CSmartVariableEnum<int> filter;

	CSmartVariable<bool>    is_tcgprojected;
	CSmartVariable<float>   tiling[3];
	CSmartVariable<float>   rotate[3];
	CSmartVariable<float>   offset[3];
	CSmartVariable<float>   tcmuoscrate;
	CSmartVariable<float>   tcmvoscrate;
	CSmartVariable<float>   tcmuoscamplitude;
	CSmartVariable<float>   tcmvoscamplitude;
	CSmartVariable<float>   tcmuoscphase;
	CSmartVariable<float>   tcmvoscphase;
	CSmartVariable<float>   tcmrotoscrate;
	CSmartVariable<float>   tcmrotoscamplitude;
	CSmartVariable<float>   tcmrotoscphase;
	CSmartVariable<float>   tcmrotosccenter[2];

	CSmartVariableArray     tableTiling;
	CSmartVariableArray     tableOscillator;
	CSmartVariableArray     tableRotator;
};

struct SMaterialLayerVars
{
	CSmartVariable<bool>        bNoDraw;  // disable layer rendering (useful in some cases)
	CSmartVariable<bool>        bFadeOut; // fade out layer rendering and parent rendering
	CSmartVariableEnum<string>  shader;   // shader layer name
};

struct SVertexWaveFormUI
{
	CSmartVariableArray     table;
	CSmartVariableEnum<int> waveFormType;
	CSmartVariable<float>   level;
	CSmartVariable<float>   amplitude;
	CSmartVariable<float>   phase;
	CSmartVariable<float>   frequency;
};

//////////////////////////////////////////////////////////////////////////
struct SVertexModUI
{
	CSmartVariableEnum<int> type;
	CSmartVariable<float>   fDividerX;
	CSmartVariable<float>   fDividerY;
	CSmartVariable<float>   fDividerZ;
	CSmartVariable<float>   fDividerW;
	CSmartVariable<Vec3>    vNoiseScale;

	SVertexWaveFormUI       wave[4];
};

//////////////////////////////////////////////////////////////////////////
struct SDetailDecalUI
{
	CSmartVariable<Vec3>  vTile[2];
	CSmartVariable<Vec3>  vOffset[2];
	CSmartVariable<float> fRotation[2];
	CSmartVariable<float> fDeformation[2];
	CSmartVariable<float> fThreshold[2];
	CSmartVariable<float> fBlending;
	CSmartVariable<float> fSSAOAmount;
};

// HACK. Write the integer value of a texture type to the status bar. The same integer may appear
// in MTL files, so this gives users a way to figure out its meaning.
static void OnSetTexType(IVariable* pVar)
{
	CRY_ASSERT(!strcmp(pVar->GetName(), "TexType"));

	CString value;
	pVar->Get(value);
	CString desc = CString(" (value = ") + value + CString(")");
	pVar->SetDescription(desc);
}

/** User Interface definition of material.
 */
class CMaterialUI
{
public:
	CSmartVariableEnum<string> shader;
	CSmartVariable<bool>        bNoShadow;
	CSmartVariable<bool>        bAdditive;
	CSmartVariable<bool>        bDetailDecal;
	CSmartVariable<bool>        bWire;
	CSmartVariable<bool>        b2Sided;
	CSmartVariable<float>       opacity;
	CSmartVariable<float>       alphaTest;
	CSmartVariable<float>       emissiveIntensity;
	CSmartVariable<float>       furAmount;
	CSmartVariable<float>       voxelCoverage;
	CSmartVariable<float>       heatAmount;
	CSmartVariable<float>       cloakAmount;
	CSmartVariable<bool>        bScatter;
	CSmartVariable<bool>        bHideAfterBreaking;
	CSmartVariable<bool>        bBlendTerrainColor;
	CSmartVariable<bool>        bTraceableTexture;
	//CSmartVariable<bool> bTranslucenseLayer;
	CSmartVariableEnum<string>  surfaceType;
	CSmartVariableEnum<string>  matTemplate;

	CSmartVariable<bool>        allowLayerActivation;

	//////////////////////////////////////////////////////////////////////////
	// Material Value Propagation for dynamic material switches, as for instance
	// used by breakable glass
	//////////////////////////////////////////////////////////////////////////
	CSmartVariableEnum<string>  matPropagate;
	CSmartVariable<bool>        bPropagateMaterialSettings;
	CSmartVariable<bool>        bPropagateOpactity;
	CSmartVariable<bool>        bPropagateLighting;
	CSmartVariable<bool>        bPropagateAdvanced;
	CSmartVariable<bool>        bPropagateTexture;
	CSmartVariable<bool>        bPropagateVertexDef;
	CSmartVariable<bool>        bPropagateShaderParams;
	CSmartVariable<bool>        bPropagateLayerPresets;
	CSmartVariable<bool>        bPropagateShaderGenParams;

	//////////////////////////////////////////////////////////////////////////
	// Lighting
	//////////////////////////////////////////////////////////////////////////
	CSmartVariable<Vec3>  diffuse;        // Diffuse color 0..1
	CSmartVariable<Vec3>  specular;       // Specular color 0..1
	CSmartVariable<float> smoothness;     // Specular shininess.
	CSmartVariable<Vec3>  emissiveCol;    // Emissive color 0..1

	//////////////////////////////////////////////////////////////////////////
	// Textures.
	//////////////////////////////////////////////////////////////////////////
	//CSmartVariableArrayT<CString> textureVars[EFTT_MAX];
	CSmartVariableArray textureVars[EFTT_MAX];
	STextureVars        textures[EFTT_MAX];

	//////////////////////////////////////////////////////////////////////////
	// Material layers settings
	//////////////////////////////////////////////////////////////////////////

	// 8 max for now. change this later
	SMaterialLayerVars materialLayers[MTL_LAYER_MAX_SLOTS];

	//////////////////////////////////////////////////////////////////////////

	SVertexModUI           vertexMod;

	CSmartVariableArray    tableShader;
	CSmartVariableArray    tableOpacity;
	CSmartVariableArray    tableLighting;
	CSmartVariableArray    tableTexture;
	CSmartVariableArray    tableAdvanced;
	CSmartVariableArray    tableVertexMod;
	CSmartVariableArray    tableEffects;
	CSmartVariableArray    tableLayerPresets;

	CSmartVariableArray    tableShaderParams;
	CSmartVariableArray    tableShaderGenParams;

	SDetailDecalUI         detailDecal;
	CSmartVariableArray    tableDetailDecal;
	CSmartVariableArray    tableDetailDecalTop;
	CSmartVariableArray    tableDetailDecalBottom;

	CVarEnumList<int>*     enumTexType;
	CVarEnumList<int>*     enumTexGenType;
	CVarEnumList<int>*     enumTexModRotateType;
	CVarEnumList<int>*     enumTexModUMoveType;
	CVarEnumList<int>*     enumTexModVMoveType;
	CVarEnumList<int>*     enumTexFilterType;

	CVarEnumList<int>*     enumVertexMod;
	CVarEnumList<int>*     enumWaveType;


	//////////////////////////////////////////////////////////////////////////
	int          texUsageMask;

	CVarBlockPtr m_vars;

	//////////////////////////////////////////////////////////////////////////
	void SetFromMaterial(CMaterial* mtl, bool bSetTemplate = false);
	void SetToMaterial(CMaterial* mtl, int propagationFlags = MTL_PROPAGATE_ALL);

	void SetShaderResources(const SInputShaderResources& srTextures, const SInputShaderResources& srTpl, bool bSetTextures = true);
	void GetShaderResources(SInputShaderResources& sr, int propagationFlags);

	void SetVertexDeform(const SInputShaderResources& sr);
	void GetVertexDeform(SInputShaderResources& sr, int propagationFlags);

	void SetDetailDecalInfo(const SInputShaderResources& sr);
	void GetDetailDecalInfo(SInputShaderResources& sr, int propagationFlags);

	void PropagateFromLinkedMaterial(CMaterial* mtl);
	void PropagateToLinkedMaterial(CMaterial* mtl, CVarBlockPtr pShaderParamsBlock);

	//////////////////////////////////////////////////////////////////////////
	CMaterialUI()
	{
	}
	~CMaterialUI()
	{
	}

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = new CVarBlock;

		//////////////////////////////////////////////////////////////////////////
		// Init enums.
		//////////////////////////////////////////////////////////////////////////
		enumTexType = new CVarEnumList<int>;
		enumTexType->AddItem(MaterialHelpers::GetNameFromTextureType(eTT_2D), eTT_2D);
		enumTexType->AddItem(MaterialHelpers::GetNameFromTextureType(eTT_Cube), eTT_Cube);
		enumTexType->AddItem(MaterialHelpers::GetNameFromTextureType(eTT_NearestCube), eTT_NearestCube);
		enumTexType->AddItem(MaterialHelpers::GetNameFromTextureType(eTT_Dyn2D), eTT_Dyn2D);
		enumTexType->AddItem(MaterialHelpers::GetNameFromTextureType(eTT_User), eTT_User);

		enumTexGenType = new CVarEnumList<int>;
		enumTexGenType->AddItem("Stream", ETG_Stream);
		enumTexGenType->AddItem("World", ETG_World);
		enumTexGenType->AddItem("Camera", ETG_Camera);

		enumTexModRotateType = new CVarEnumList<int>;
		enumTexModRotateType->AddItem("No Change", ETMR_NoChange);
		enumTexModRotateType->AddItem("Fixed Rotation", ETMR_Fixed);
		enumTexModRotateType->AddItem("Constant Rotation", ETMR_Constant);
		enumTexModRotateType->AddItem("Oscillated Rotation", ETMR_Oscillated);

		enumTexModUMoveType = new CVarEnumList<int>;
		enumTexModUMoveType->AddItem("No Change", ETMM_NoChange);
		enumTexModUMoveType->AddItem("Fixed Moving", ETMM_Fixed);
		enumTexModUMoveType->AddItem("Constant Moving", ETMM_Constant);
		enumTexModUMoveType->AddItem("Jitter Moving", ETMM_Jitter);
		enumTexModUMoveType->AddItem("Pan Moving", ETMM_Pan);
		enumTexModUMoveType->AddItem("Stretch Moving", ETMM_Stretch);
		enumTexModUMoveType->AddItem("Stretch-Repeat Moving", ETMM_StretchRepeat);

		enumTexModVMoveType = new CVarEnumList<int>;
		enumTexModVMoveType->AddItem("No Change", ETMM_NoChange);
		enumTexModVMoveType->AddItem("Fixed Moving", ETMM_Fixed);
		enumTexModVMoveType->AddItem("Constant Moving", ETMM_Constant);
		enumTexModVMoveType->AddItem("Jitter Moving", ETMM_Jitter);
		enumTexModVMoveType->AddItem("Pan Moving", ETMM_Pan);
		enumTexModVMoveType->AddItem("Stretch Moving", ETMM_Stretch);
		enumTexModVMoveType->AddItem("Stretch-Repeat Moving", ETMM_StretchRepeat);

		enumTexFilterType = new CVarEnumList<int>;
		enumTexFilterType->AddItem("Default", FILTER_NONE);
		enumTexFilterType->AddItem("Point", FILTER_POINT);
		enumTexFilterType->AddItem("Linear", FILTER_LINEAR);
		enumTexFilterType->AddItem("Bi-linear", FILTER_BILINEAR);
		enumTexFilterType->AddItem("Tri-linear", FILTER_TRILINEAR);
		enumTexFilterType->AddItem("Anisotropic 2x", FILTER_ANISO2X);
		enumTexFilterType->AddItem("Anisotropic 4x", FILTER_ANISO4X);
		enumTexFilterType->AddItem("Anisotropic 8x", FILTER_ANISO8X);
		enumTexFilterType->AddItem("Anisotropic 16x", FILTER_ANISO16X);

		//////////////////////////////////////////////////////////////////////////
		// Vertex Mods.
		//////////////////////////////////////////////////////////////////////////
		enumVertexMod = new CVarEnumList<int>;
		enumVertexMod->AddItem("None", eDT_Unknown);
		enumVertexMod->AddItem("Sin Wave", eDT_SinWave);
		enumVertexMod->AddItem("Sin Wave using vertex color", eDT_SinWaveUsingVtxColor);
		enumVertexMod->AddItem("Bulge", eDT_Bulge);
		enumVertexMod->AddItem("Squeeze", eDT_Squeeze);
		//  enumVertexMod->AddItem("Perlin 2D",eDT_Perlin2D);
		//  enumVertexMod->AddItem("Perlin 3D",eDT_Perlin3D);
		//  enumVertexMod->AddItem("From Center",eDT_FromCenter);
		enumVertexMod->AddItem("Bending", eDT_Bending);
		//  enumVertexMod->AddItem("Proc. Flare",eDT_ProcFlare);
		//  enumVertexMod->AddItem("Auto sprite",eDT_AutoSprite);
		//  enumVertexMod->AddItem("Beam",eDT_Beam);
		enumVertexMod->AddItem("FixedOffset", eDT_FixedOffset);

		//////////////////////////////////////////////////////////////////////////

		enumWaveType = new CVarEnumList<int>;
		enumWaveType->AddItem("None", eWF_None);
		enumWaveType->AddItem("Sin", eWF_Sin);
		//  enumWaveType->AddItem("Half Sin",eWF_HalfSin);
		//  enumWaveType->AddItem("Square",eWF_Square);
		//  enumWaveType->AddItem("Triangle",eWF_Triangle);
		//  enumWaveType->AddItem("Saw Tooth",eWF_SawTooth);
		//  enumWaveType->AddItem("Inverse Saw Tooth",eWF_InvSawTooth);
		//  enumWaveType->AddItem("Hill",eWF_Hill);
		//  enumWaveType->AddItem("Inverse Hill",eWF_InvHill);

		//////////////////////////////////////////////////////////////////////////
		// Fill shaders enum.
		//////////////////////////////////////////////////////////////////////////
		CVarEnumList<string>* enumShaders = new CVarEnumList<string>;
		{
			const auto& shaderList = GetIEditor()->GetMaterialManager()->GetShaderList();
			
			for(const string& shaderName : shaderList)
			{
				if (strstri(shaderName, "_Overlay") != 0)
					continue;
				enumShaders->AddItem(shaderName, shaderName);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Fill surface types.
		//////////////////////////////////////////////////////////////////////////
		CVarEnumList<string>* enumSurfaceTypes = new CVarEnumList<string>;
		{
			std::vector<string> types;
			types.push_back("");   // Push empty surface type.
			ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
			if (pSurfaceTypeEnum)
			{
				for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
				{
					types.push_back(pSurfaceType->GetName());
				}
				std::sort(types.begin(), types.end());
				for (int i = 0; i < types.size(); i++)
				{
					string name = types[i];
					if (name.Left(4) == "mat_")
						name.Delete(0, 4);
					enumSurfaceTypes->AddItem(name, types[i]);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Init tables.
		//////////////////////////////////////////////////////////////////////////
		AddVariable(m_vars, tableShader, "Material Settings");
		AddVariable(m_vars, tableOpacity, "Opacity Settings");
		AddVariable(m_vars, tableLighting, "Lighting Settings");
		AddVariable(m_vars, tableAdvanced, "Advanced");
		AddVariable(m_vars, tableTexture, "Texture Maps");
		AddVariable(m_vars, tableShaderParams, "Shader Params");
		AddVariable(m_vars, tableShaderGenParams, "Shader Generation Params");
		AddVariable(m_vars, tableVertexMod, "Vertex Deformation");
		AddVariable(m_vars, tableLayerPresets, "Layer Presets");

		tableTexture->SetFlags(tableTexture->GetFlags() | IVariable::UI_ROLLUP2);

		tableVertexMod->SetFlags(tableVertexMod->GetFlags() | IVariable::UI_ROLLUP2 | IVariable::UI_COLLAPSED);
		tableAdvanced->SetFlags(tableAdvanced->GetFlags() | IVariable::UI_COLLAPSED);
		tableShaderGenParams->SetFlags(tableShaderGenParams->GetFlags() | IVariable::UI_ROLLUP2 | IVariable::UI_COLLAPSED);
		tableShaderParams->SetFlags(tableShaderParams->GetFlags() | IVariable::UI_ROLLUP2);
		tableLayerPresets->SetFlags(tableLayerPresets->GetFlags() | IVariable::UI_ROLLUP2 | IVariable::UI_COLLAPSED);

		//
		AddVariable(tableShader, matTemplate, "Template Material", IVariable::DT_MATERIALLOOKUP);

		//////////////////////////////////////////////////////////////////////////
		// Shader.
		//////////////////////////////////////////////////////////////////////////
		AddVariable(tableShader, shader, "Shader");
		AddVariable(tableShader, surfaceType, "Surface Type");

		shader->SetEnumList(enumShaders);

		surfaceType->SetEnumList(enumSurfaceTypes);

		//////////////////////////////////////////////////////////////////////////
		// Opacity.
		//////////////////////////////////////////////////////////////////////////
		AddVariable(tableOpacity, opacity, "Opacity", IVariable::DT_PERCENT);
		AddVariable(tableOpacity, alphaTest, "AlphaTest", IVariable::DT_PERCENT);
		AddVariable(tableOpacity, bAdditive, "Additive");
		opacity->SetLimits(0, 100, 1, true, true);
		alphaTest->SetLimits(0, 100, 1, true, true);

		//////////////////////////////////////////////////////////////////////////
		// Lighting.
		//////////////////////////////////////////////////////////////////////////
		AddVariable(tableLighting, diffuse, "Diffuse Color", IVariable::DT_COLOR);
		AddVariable(tableLighting, specular, "Specular Color", IVariable::DT_COLOR);
		AddVariable(tableLighting, smoothness, "Smoothness");
		AddVariable(tableLighting, emissiveIntensity, "Emissive Intensity (kcd/m2)");
		AddVariable(tableLighting, emissiveCol, "Emissive Color", IVariable::DT_COLOR);
		emissiveIntensity->SetLimits(0, 200, 1, true, false);
		smoothness->SetLimits(0, 255, 1, true, true);

		//////////////////////////////////////////////////////////////////////////
		// Init texture variables.
		//////////////////////////////////////////////////////////////////////////
		for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
		{
			if (!MaterialHelpers::IsAdjustableTexSlot(texId))
				continue;

			InitTextureVars(texId, MaterialHelpers::LookupTexName(texId));
		}

		//AddVariable( tableAdvanced,bWire,"Wireframe" );
		AddVariable(tableAdvanced, allowLayerActivation, "Allow layer activation");
		AddVariable(tableAdvanced, b2Sided, "2 Sided");
		AddVariable(tableAdvanced, bNoShadow, "No Shadow");
		AddVariable(tableAdvanced, bScatter, "Use Scattering");
		AddVariable(tableAdvanced, bHideAfterBreaking, "Hide After Breaking");
		AddVariable(tableAdvanced, bBlendTerrainColor, "Blend Terrain Color");
		AddVariable(tableAdvanced, bTraceableTexture, "Traceable Texture");

		AddVariable(tableAdvanced, furAmount, "Fur Amount");
		furAmount->SetLimits(0, 1.0f);
		AddVariable(tableAdvanced, voxelCoverage, "Voxel Coverage");
		voxelCoverage->SetLimits(0, 1.0f);
		AddVariable(tableAdvanced, heatAmount, "Heat Amount");
		heatAmount->SetLimits(0, 4.0f);

		// to be removed: when layers presets are working properly again (post c2)
		AddVariable(tableAdvanced, cloakAmount, "Cloak Amount");
		cloakAmount->SetLimits(0, 1.0f);

		//////////////////////////////////////////////////////////////////////////
		// Material Value Propagation for dynamic material switches, as for instance
		// used by breakable glass
		//////////////////////////////////////////////////////////////////////////
		AddVariable(tableAdvanced, matPropagate, "Link to Material");
		AddVariable(tableAdvanced, bPropagateMaterialSettings, "Propagate Material Settings");
		AddVariable(tableAdvanced, bPropagateOpactity, "Propagate Opacity Settings");
		AddVariable(tableAdvanced, bPropagateLighting, "Propagate Lighting Settings");
		AddVariable(tableAdvanced, bPropagateAdvanced, "Propagate Advanced Settings");
		AddVariable(tableAdvanced, bPropagateTexture, "Propagate Texture Maps");
		AddVariable(tableAdvanced, bPropagateShaderParams, "Propagate Shader Params");
		AddVariable(tableAdvanced, bPropagateShaderGenParams, "Propagate Shader Generation");
		AddVariable(tableAdvanced, bPropagateVertexDef, "Propagate Vertex Deformation");
		AddVariable(tableAdvanced, bPropagateLayerPresets, "Propagate Layer Presets");

		//////////////////////////////////////////////////////////////////////////
		// Init Vertex Deformation.
		//////////////////////////////////////////////////////////////////////////
		vertexMod.type->SetEnumList(enumVertexMod);
		AddVariable(tableVertexMod, vertexMod.type, "Type");
		AddVariable(tableVertexMod, vertexMod.fDividerX, "Wave Length");
		//  AddVariable( tableVertexMod,vertexMod.fDividerX,"Wave Length X" );
		//  AddVariable( tableVertexMod,vertexMod.fDividerY,"Wave Length Y" );
		//  AddVariable( tableVertexMod,vertexMod.fDividerZ,"Wave Length Z" );
		//  AddVariable( tableVertexMod,vertexMod.fDividerW,"Wave Length W" );
		AddVariable(tableVertexMod, vertexMod.vNoiseScale, "Noise Scale");

		AddVariable(tableVertexMod, vertexMod.wave[0].table, "Parameters");
		//  AddVariable( tableVertexMod,vertexMod.wave[0].table,"Wave X" );
		//  AddVariable( tableVertexMod,vertexMod.wave[1].table,"Wave Y" );
		//  AddVariable( tableVertexMod,vertexMod.wave[2].table,"Wave Z" );
		//  AddVariable( tableVertexMod,vertexMod.wave[3].table,"Wave W" );

		for (int i = 0; i < 1; i++)
		//  for (int i = 0; i < 4; i++)
		{
			vertexMod.wave[i].waveFormType->SetEnumList(enumWaveType);
			AddVariable(vertexMod.wave[i].table, vertexMod.wave[i].waveFormType, "Type");
			AddVariable(vertexMod.wave[i].table, vertexMod.wave[i].level, "Level");
			AddVariable(vertexMod.wave[i].table, vertexMod.wave[i].amplitude, "Amplitude");
			AddVariable(vertexMod.wave[i].table, vertexMod.wave[i].phase, "Phase");
			AddVariable(vertexMod.wave[i].table, vertexMod.wave[i].frequency, "Frequency");
		}

		//////////////////////////////////////////////////////////////////////////
		// Material layer presets.
		//////////////////////////////////////////////////////////////////////////

		for (int nLayer = 0; nLayer < MTL_LAYER_MAX_SLOTS; ++nLayer)
		{
			AddVariable(tableLayerPresets, materialLayers[nLayer].shader, "Shader");
			AddVariable(tableLayerPresets, materialLayers[nLayer].bNoDraw, "No Draw");
			AddVariable(tableLayerPresets, materialLayers[nLayer].bFadeOut, "Fade Out");
			materialLayers[nLayer].shader->SetEnumList(enumShaders);
		}

		return m_vars;
	}

private:
	//////////////////////////////////////////////////////////////////////////
	void InitTextureVars(int id, const string& name)
	{
		textureVars[id]->SetFlags(IVariable::UI_BOLD);
		AddVariable(tableTexture, textureVars[id], name, IVariable::DT_TEXTURE);

		// As asked by Martin Mittring, removed the amount parameter.
		// Add variables from STextureVars structure.
		//if (id == EFTT_NORMALS || id == EFTT_ENV)
		//{
		//  AddVariable( textureVars[id],textures[id].amount,"Amount" );
		//  textures[id].amount->SetLimits( 0,255 );
		//}
		AddVariable(textureVars[id], textures[id].etextype, "TexType");
		AddVariable(textureVars[id], textures[id].filter, "Filter");

		if (id == EFTT_DECAL_OVERLAY)
			AddVariable(textureVars[id], bDetailDecal, "Detail Decal");

		AddVariable(textureVars[id], textures[id].is_tcgprojected, "IsProjectedTexGen");
		AddVariable(textureVars[id], textures[id].etcgentype, "TexGenType");

		//////////////////////////////////////////////////////////////////////////
		// Init Detail decals
		//////////////////////////////////////////////////////////////////////////
		if (id == EFTT_DECAL_OVERLAY)
		{
			AddVariable(textureVars[id], tableDetailDecal, "Detail decal");

			CVariableArray& table = tableDetailDecal;
			table.SetFlags(IVariable::UI_BOLD);

			AddVariable(table, detailDecal.fBlending, "Opacity");
			AddVariable(table, detailDecal.fSSAOAmount, "SSAOAmount");

			AddVariable(table, tableDetailDecalTop, "Top layer");
			AddVariable(table, tableDetailDecalBottom, "Bottom layer");

			AddVariable(tableDetailDecalTop, detailDecal.vTile[0], "Tile");
			AddVariable(tableDetailDecalTop, detailDecal.vOffset[0], "Offset");
			AddVariable(tableDetailDecalTop, detailDecal.fRotation[0], "Rotation", IVariable::DT_ANGLE);
			AddVariable(tableDetailDecalTop, detailDecal.fDeformation[0], "Deformation");
			AddVariable(tableDetailDecalTop, detailDecal.fThreshold[0], "Sorting offset");
			AddVariable(tableDetailDecalBottom, detailDecal.vTile[1], "Tile");
			AddVariable(tableDetailDecalBottom, detailDecal.vOffset[1], "Offset");
			AddVariable(tableDetailDecalBottom, detailDecal.fRotation[1], "Rotation", IVariable::DT_ANGLE);
			AddVariable(tableDetailDecalBottom, detailDecal.fDeformation[1], "Deformation");
			AddVariable(tableDetailDecalBottom, detailDecal.fThreshold[1], "Sorting offset");
		}

		//////////////////////////////////////////////////////////////////////////
		// Tiling table.
		AddVariable(textureVars[id], textures[id].tableTiling, "Tiling");
		{
			CVariableArray& table = textures[id].tableTiling;
			table.SetFlags(IVariable::UI_BOLD);
			AddVariable(table, textures[id].is_tile[0], "IsTileU");
			AddVariable(table, textures[id].is_tile[1], "IsTileV");

			AddVariable(table, textures[id].tiling[0], "TileU");
			AddVariable(table, textures[id].tiling[1], "TileV");
			AddVariable(table, textures[id].offset[0], "OffsetU");
			AddVariable(table, textures[id].offset[1], "OffsetV");
			AddVariable(table, textures[id].rotate[0], "RotateU");
			AddVariable(table, textures[id].rotate[1], "RotateV");
			AddVariable(table, textures[id].rotate[2], "RotateW");
		}

		//////////////////////////////////////////////////////////////////////////
		// Rotator tables.
		AddVariable(textureVars[id], textures[id].tableRotator, "Rotator");
		{
			CVariableArray& table = textures[id].tableRotator;
			table.SetFlags(IVariable::UI_BOLD);
			AddVariable(table, textures[id].etcmrotatetype, "Type");
			AddVariable(table, textures[id].tcmrotoscrate, "Rate");
			AddVariable(table, textures[id].tcmrotoscphase, "Phase");
			AddVariable(table, textures[id].tcmrotoscamplitude, "Amplitude");
			AddVariable(table, textures[id].tcmrotosccenter[0], "CenterU");
			AddVariable(table, textures[id].tcmrotosccenter[1], "CenterV");
		}

		//////////////////////////////////////////////////////////////////////////
		// Oscillator table
		AddVariable(textureVars[id], textures[id].tableOscillator, "Oscillator");
		{
			CVariableArray& table = textures[id].tableOscillator;
			table.SetFlags(IVariable::UI_BOLD);
			AddVariable(table, textures[id].etcmumovetype, "TypeU");
			AddVariable(table, textures[id].etcmvmovetype, "TypeV");
			AddVariable(table, textures[id].tcmuoscrate, "RateU");
			AddVariable(table, textures[id].tcmvoscrate, "RateV");
			AddVariable(table, textures[id].tcmuoscphase, "PhaseU");
			AddVariable(table, textures[id].tcmvoscphase, "PhaseV");
			AddVariable(table, textures[id].tcmuoscamplitude, "AmplitudeU");
			AddVariable(table, textures[id].tcmvoscamplitude, "AmplitudeV");
		}

		//////////////////////////////////////////////////////////////////////////
		// Assign enums tables to variable.
		//////////////////////////////////////////////////////////////////////////
		textures[id].etextype->SetEnumList(enumTexType);
		textures[id].etcgentype->SetEnumList(enumTexGenType);
		textures[id].etcmrotatetype->SetEnumList(enumTexModRotateType);
		textures[id].etcmumovetype->SetEnumList(enumTexModUMoveType);
		textures[id].etcmvmovetype->SetEnumList(enumTexModVMoveType);
		textures[id].filter->SetEnumList(enumTexFilterType);

		textures[id].etextype->AddOnSetCallback(functor(OnSetTexType));
	}
	//////////////////////////////////////////////////////////////////////////

	void AddVariable(CVariableBase& varArray, CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE)
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		varArray.AddVariable(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable(CVarBlock* vars, CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE)
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		vars->AddVariable(&var);
	}

	void   SetTextureResources(const SInputShaderResources& sr, const SInputShaderResources& srTpl, int texid, bool bSetTextures);
	void   GetTextureResources(SInputShaderResources& sr, int texid, int propagationFlags);
	Vec3   ToVec3(const ColorF& col)  { return Vec3(col.r, col.g, col.b); }
	ColorF ToCFColor(const Vec3& col) { return ColorF(col); }

};

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetShaderResources(const SInputShaderResources& srTextures, const SInputShaderResources& srTpl, bool bSetTextures)
{
	alphaTest = srTpl.m_AlphaRef;
	furAmount = (float) srTpl.m_FurAmount / 255.0f;
	voxelCoverage = (float) srTpl.m_VoxelCoverage / 255.0f;
	cloakAmount = (float) srTpl.m_CloakAmount / 255.0f;

	const float fHeatRange = MAX_HEATSCALE;
	heatAmount = (float) (srTpl.m_HeatAmount / 255.0f) * fHeatRange; // rescale range

	diffuse = ToVec3(srTpl.m_LMaterial.m_Diffuse);
	specular = ToVec3(srTpl.m_LMaterial.m_Specular);
	emissiveCol = ToVec3(srTpl.m_LMaterial.m_Emittance);
	emissiveIntensity = srTpl.m_LMaterial.m_Emittance.a;
	opacity = srTpl.m_LMaterial.m_Opacity;
	smoothness = srTpl.m_LMaterial.m_Smoothness;

	SetVertexDeform(srTpl);
	SetDetailDecalInfo(srTpl);

	for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
	{
		if (!MaterialHelpers::IsAdjustableTexSlot(texId))
			continue;

		SetTextureResources(srTextures, srTpl, texId, bSetTextures);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetShaderResources(SInputShaderResources& sr, int propagationFlags)
{
	if (propagationFlags & MTL_PROPAGATE_OPACITY)
	{
		sr.m_LMaterial.m_Opacity = opacity;
		sr.m_AlphaRef = alphaTest;
	}

	if (propagationFlags & MTL_PROPAGATE_ADVANCED)
	{
		sr.m_FurAmount = int_round(furAmount * 255.0f);
		sr.m_VoxelCoverage = int_round(voxelCoverage * 255.0f);
		sr.m_CloakAmount = int_round(cloakAmount * 255.0f);

		const float fHeatRange = MAX_HEATSCALE;
		sr.m_HeatAmount = int_round(heatAmount * 255.0f / fHeatRange);   // rescale range
	}

	if (propagationFlags & MTL_PROPAGATE_LIGHTING)
	{
		sr.m_LMaterial.m_Diffuse = ToCFColor(diffuse);
		sr.m_LMaterial.m_Specular = ToCFColor(specular);
		sr.m_LMaterial.m_Emittance = ColorF(emissiveCol, emissiveIntensity);
		sr.m_LMaterial.m_Smoothness = smoothness;
	}

	GetVertexDeform(sr, propagationFlags);
	if (bDetailDecal)
		GetDetailDecalInfo(sr, propagationFlags);

	for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
	{
		if (!MaterialHelpers::IsAdjustableTexSlot(texId))
			continue;

		GetTextureResources(sr, texId, propagationFlags);
	}
}

inline float RoundDegree(float val)
{
	//double v = floor(val*100.0f);
	//return v*0.01f;
	return (float)((int)(val * 100 + 0.5f)) * 0.01f;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetTextureResources(const SInputShaderResources& sr, const SInputShaderResources& srTpl, int tex, bool bSetTextures)
{
	/*
	   // Enable/Disable texture map, depending on the mask.
	   int flags = textureVars[tex].GetFlags();
	   if ((1 << tex) & texUsageMask)
	   flags &= ~IVariable::UI_DISABLED;
	   else
	   flags |= IVariable::UI_DISABLED;
	   textureVars[tex].SetFlags( flags );
	 */

	if (bSetTextures)
	{
		string texFilename = sr.m_Textures[tex].m_Name.c_str();
		texFilename = PathUtil::ToUnixPath(texFilename.GetString()).c_str();
		textureVars[tex]->Set(texFilename);
	}

	//textures[tex].amount = srTpl.m_Textures[tex].m_Amount;
	textures[tex].is_tile[0] = srTpl.m_Textures[tex].m_bUTile;
	textures[tex].is_tile[1] = srTpl.m_Textures[tex].m_bVTile;

	//textures[tex].amount = sr.m_Textures[tex].m_Amount;
	textures[tex].is_tile[0] = sr.m_Textures[tex].m_bUTile;
	textures[tex].is_tile[1] = sr.m_Textures[tex].m_bVTile;

	textures[tex].tiling[0] = sr.m_Textures[tex].GetTiling(0);
	textures[tex].tiling[1] = sr.m_Textures[tex].GetTiling(1);
	textures[tex].offset[0] = sr.m_Textures[tex].GetOffset(0);
	textures[tex].offset[1] = sr.m_Textures[tex].GetOffset(1);
	textures[tex].filter = (int)sr.m_Textures[tex].m_Filter;
	textures[tex].etextype = sr.m_Textures[tex].m_Sampler.m_eTexType;
	if (sr.m_Textures[tex].m_Ext.m_pTexModifier)
	{
		textures[tex].etcgentype = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_eTGType;
		textures[tex].etcmumovetype = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_eMoveType[0];
		textures[tex].etcmvmovetype = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_eMoveType[1];
		textures[tex].etcmrotatetype = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_eRotType;
		textures[tex].is_tcgprojected = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_bTexGenProjected;
		textures[tex].tcmuoscrate = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscRate[0];
		textures[tex].tcmuoscphase = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscPhase[0];
		textures[tex].tcmuoscamplitude = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscAmplitude[0];
		textures[tex].tcmvoscrate = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscRate[1];
		textures[tex].tcmvoscphase = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscPhase[1];
		textures[tex].tcmvoscamplitude = sr.m_Textures[tex].m_Ext.m_pTexModifier->m_OscAmplitude[1];

		for (int i = 0; i < 3; i++)
			textures[tex].rotate[i] = RoundDegree(Word2Degr(srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_Rot[i]));
		textures[tex].tcmrotoscrate = RoundDegree(Word2Degr(srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_RotOscRate[2]));
		textures[tex].tcmrotoscphase = RoundDegree(Word2Degr(srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_RotOscPhase[2]));
		textures[tex].tcmrotoscamplitude = RoundDegree(Word2Degr(srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_RotOscAmplitude[2]));
		textures[tex].tcmrotosccenter[0] = srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_RotOscCenter[0];
		textures[tex].tcmrotosccenter[1] = srTpl.m_Textures[tex].m_Ext.m_pTexModifier->m_RotOscCenter[1];
	}
	else
	{
		textures[tex].etcgentype = 0;
		textures[tex].etcmumovetype = 0;
		textures[tex].etcmvmovetype = 0;
		textures[tex].etcmrotatetype = 0;
		textures[tex].is_tcgprojected = false;
		textures[tex].tcmuoscrate = 0;
		textures[tex].tcmuoscphase = 0;
		textures[tex].tcmuoscamplitude = 0;
		textures[tex].tcmvoscrate = 0;
		textures[tex].tcmvoscphase = 0;
		textures[tex].tcmvoscamplitude = 0;

		for (int i = 0; i < 3; i++)
			textures[tex].rotate[i] = 0;
		textures[tex].tcmrotoscrate = 0;
		textures[tex].tcmrotoscphase = 0;
		textures[tex].tcmrotoscamplitude = 0;
		textures[tex].tcmrotosccenter[0] = 0;
		textures[tex].tcmrotosccenter[1] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
static const char* fpGetExtension(const char* in)
{
	assert(in); // if this hits, check the call site
	ptrdiff_t len = strlen(in) - 1;
	while (len)
	{
		if (in[len] == '.')
			return &in[len];
		len--;
	}
	return NULL;
}

static bool IsFlashUIFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = fpGetExtension(pFlashFileName);
		PREFAST_SUPPRESS_WARNING(6387)
		if (pExt && !stricmp(pExt, ".ui"))
			return true;
	}

	return false;
}

static bool DestroyTexOfFlashFile(const char* name)
{
	if (gEnv->pFlashUI)
	{
		IUIElement* pElement = gEnv->pFlashUI->GetUIElementByInstanceStr(name);
		if (pElement)
		{
			pElement->Unload();
			pElement->DestroyThis();

		}
	}

	return false;
}

void CMaterialUI::GetTextureResources(SInputShaderResources& sr, int tex, int propagationFlags)
{
	if ((propagationFlags & MTL_PROPAGATE_TEXTURES) == 0)
		return;

	CString texFilename;
	textureVars[tex]->Get(texFilename);
	texFilename = PathUtil::ToUnixPath(texFilename.GetString()).c_str();

	//Unload previous dynamic texture/UIElement instance
	if (IsFlashUIFile(sr.m_Textures[tex].m_Name))
	{
		DestroyTexOfFlashFile(sr.m_Textures[tex].m_Name);
	}

	sr.m_Textures[tex].m_Name = (const char*)texFilename;

	//sr.m_Textures[tex].m_Amount = textures[tex].amount;
	sr.m_Textures[tex].m_bUTile = textures[tex].is_tile[0];
	sr.m_Textures[tex].m_bVTile = textures[tex].is_tile[1];
	SEfTexModificator& texm = *sr.m_Textures[tex].AddModificator();
	texm.m_bTexGenProjected = textures[tex].is_tcgprojected;

	texm.m_Tiling[0] = textures[tex].tiling[0];
	texm.m_Tiling[1] = textures[tex].tiling[1];
	texm.m_Offs[0] = textures[tex].offset[0];
	texm.m_Offs[1] = textures[tex].offset[1];
	sr.m_Textures[tex].m_Filter = (int)textures[tex].filter;
	sr.m_Textures[tex].m_Sampler.m_eTexType = textures[tex].etextype;
	texm.m_eRotType = textures[tex].etcmrotatetype;
	texm.m_eTGType = textures[tex].etcgentype;
	texm.m_eMoveType[0] = textures[tex].etcmumovetype;
	texm.m_eMoveType[1] = textures[tex].etcmvmovetype;
	texm.m_OscRate[0] = textures[tex].tcmuoscrate;
	texm.m_OscPhase[0] = textures[tex].tcmuoscphase;
	texm.m_OscAmplitude[0] = textures[tex].tcmuoscamplitude;
	texm.m_OscRate[1] = textures[tex].tcmvoscrate;
	texm.m_OscPhase[1] = textures[tex].tcmvoscphase;
	texm.m_OscAmplitude[1] = textures[tex].tcmvoscamplitude;

	for (int i = 0; i < 3; i++)
		texm.m_Rot[i] = Degr2Word(textures[tex].rotate[i]);
	texm.m_RotOscRate[2] = Degr2Word(textures[tex].tcmrotoscrate);
	texm.m_RotOscPhase[2] = Degr2Word(textures[tex].tcmrotoscphase);
	texm.m_RotOscAmplitude[2] = Degr2Word(textures[tex].tcmrotoscamplitude);
	texm.m_RotOscCenter[0] = textures[tex].tcmrotosccenter[0];
	texm.m_RotOscCenter[1] = textures[tex].tcmrotosccenter[1];
	texm.m_RotOscCenter[2] = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetVertexDeform(const SInputShaderResources& sr)
{
	vertexMod.type = (int)sr.m_DeformInfo.m_eType;
	vertexMod.fDividerX = sr.m_DeformInfo.m_fDividerX;
	vertexMod.fDividerY = sr.m_DeformInfo.m_fDividerY;
	vertexMod.fDividerZ = sr.m_DeformInfo.m_fDividerZ;
	vertexMod.fDividerW = sr.m_DeformInfo.m_fDividerW;
	vertexMod.vNoiseScale = sr.m_DeformInfo.m_vNoiseScale;

	vertexMod.wave[0].waveFormType = sr.m_DeformInfo.m_WaveX.m_eWFType;
	vertexMod.wave[0].amplitude = sr.m_DeformInfo.m_WaveX.m_Amp;
	vertexMod.wave[0].level = sr.m_DeformInfo.m_WaveX.m_Level;
	vertexMod.wave[0].phase = sr.m_DeformInfo.m_WaveX.m_Phase;
	vertexMod.wave[0].frequency = sr.m_DeformInfo.m_WaveX.m_Freq;

	vertexMod.wave[1].waveFormType = sr.m_DeformInfo.m_WaveY.m_eWFType;
	vertexMod.wave[1].amplitude = sr.m_DeformInfo.m_WaveY.m_Amp;
	vertexMod.wave[1].level = sr.m_DeformInfo.m_WaveY.m_Level;
	vertexMod.wave[1].phase = sr.m_DeformInfo.m_WaveY.m_Phase;
	vertexMod.wave[1].frequency = sr.m_DeformInfo.m_WaveY.m_Freq;

	vertexMod.wave[2].waveFormType = sr.m_DeformInfo.m_WaveZ.m_eWFType;
	vertexMod.wave[2].amplitude = sr.m_DeformInfo.m_WaveZ.m_Amp;
	vertexMod.wave[2].level = sr.m_DeformInfo.m_WaveZ.m_Level;
	vertexMod.wave[2].phase = sr.m_DeformInfo.m_WaveZ.m_Phase;
	vertexMod.wave[2].frequency = sr.m_DeformInfo.m_WaveZ.m_Freq;

	vertexMod.wave[3].waveFormType = sr.m_DeformInfo.m_WaveW.m_eWFType;
	vertexMod.wave[3].amplitude = sr.m_DeformInfo.m_WaveW.m_Amp;
	vertexMod.wave[3].level = sr.m_DeformInfo.m_WaveW.m_Level;
	vertexMod.wave[3].phase = sr.m_DeformInfo.m_WaveW.m_Phase;
	vertexMod.wave[3].frequency = sr.m_DeformInfo.m_WaveW.m_Freq;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetVertexDeform(SInputShaderResources& sr, int propagationFlags)
{
	if ((propagationFlags & MTL_PROPAGATE_VERTEX_DEF) == 0)
		return;

	sr.m_DeformInfo.m_eType = (EDeformType)((int)vertexMod.type);
	sr.m_DeformInfo.m_fDividerX = vertexMod.fDividerX;
	sr.m_DeformInfo.m_fDividerY = vertexMod.fDividerY;
	sr.m_DeformInfo.m_fDividerZ = vertexMod.fDividerZ;
	sr.m_DeformInfo.m_fDividerW = vertexMod.fDividerW;
	sr.m_DeformInfo.m_vNoiseScale = vertexMod.vNoiseScale;

	sr.m_DeformInfo.m_WaveX.m_eWFType = (EWaveForm)((int)vertexMod.wave[0].waveFormType);
	sr.m_DeformInfo.m_WaveX.m_Amp = vertexMod.wave[0].amplitude;
	sr.m_DeformInfo.m_WaveX.m_Level = vertexMod.wave[0].level;
	sr.m_DeformInfo.m_WaveX.m_Phase = vertexMod.wave[0].phase;
	sr.m_DeformInfo.m_WaveX.m_Freq = vertexMod.wave[0].frequency;

	sr.m_DeformInfo.m_WaveY.m_eWFType = (EWaveForm)((int)vertexMod.wave[1].waveFormType);
	sr.m_DeformInfo.m_WaveY.m_Amp = vertexMod.wave[1].amplitude;
	sr.m_DeformInfo.m_WaveY.m_Level = vertexMod.wave[1].level;
	sr.m_DeformInfo.m_WaveY.m_Phase = vertexMod.wave[1].phase;
	sr.m_DeformInfo.m_WaveY.m_Freq = vertexMod.wave[1].frequency;

	sr.m_DeformInfo.m_WaveZ.m_eWFType = (EWaveForm)((int)vertexMod.wave[2].waveFormType);
	sr.m_DeformInfo.m_WaveZ.m_Amp = vertexMod.wave[2].amplitude;
	sr.m_DeformInfo.m_WaveZ.m_Level = vertexMod.wave[2].level;
	sr.m_DeformInfo.m_WaveZ.m_Phase = vertexMod.wave[2].phase;
	sr.m_DeformInfo.m_WaveZ.m_Freq = vertexMod.wave[2].frequency;

	sr.m_DeformInfo.m_WaveW.m_eWFType = (EWaveForm)((int)vertexMod.wave[3].waveFormType);
	sr.m_DeformInfo.m_WaveW.m_Amp = vertexMod.wave[3].amplitude;
	sr.m_DeformInfo.m_WaveW.m_Level = vertexMod.wave[3].level;
	sr.m_DeformInfo.m_WaveW.m_Phase = vertexMod.wave[3].phase;
	sr.m_DeformInfo.m_WaveW.m_Freq = vertexMod.wave[3].frequency;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetDetailDecalInfo(const SInputShaderResources& sr)
{
	const float fUint8RatioToFloat = 100.0f / 255.0f;
	bDetailDecal = (sr.m_ResFlags & MTL_FLAG_DETAIL_DECAL);//TODO: buggy, this is set twice from two different sources
	detailDecal.fBlending = ((float)sr.m_DetailDecalInfo.nBlending) * fUint8RatioToFloat;
	detailDecal.fSSAOAmount = ((float)sr.m_DetailDecalInfo.nSSAOAmount) * fUint8RatioToFloat;

	detailDecal.vTile[0] = Vec3(sr.m_DetailDecalInfo.vTileOffs[0].x, sr.m_DetailDecalInfo.vTileOffs[0].y, 0.0f);
	detailDecal.vTile[1] = Vec3(sr.m_DetailDecalInfo.vTileOffs[1].x, sr.m_DetailDecalInfo.vTileOffs[1].y, 0.0f);
	detailDecal.vOffset[0] = Vec3(sr.m_DetailDecalInfo.vTileOffs[0].z, sr.m_DetailDecalInfo.vTileOffs[0].w, 0.0f);
	detailDecal.vOffset[1] = Vec3(sr.m_DetailDecalInfo.vTileOffs[1].z, sr.m_DetailDecalInfo.vTileOffs[1].w, 0.0f);

	detailDecal.fThreshold[0] = ((float)sr.m_DetailDecalInfo.nThreshold[0]) * fUint8RatioToFloat;
	detailDecal.fThreshold[1] = ((float)sr.m_DetailDecalInfo.nThreshold[1]) * fUint8RatioToFloat;

	detailDecal.fRotation[0] = Word2Degr(sr.m_DetailDecalInfo.nRotation[0]);
	detailDecal.fRotation[1] = Word2Degr(sr.m_DetailDecalInfo.nRotation[1]);

	detailDecal.fDeformation[0] = ((float)sr.m_DetailDecalInfo.nDeformation[0]) * fUint8RatioToFloat;
	detailDecal.fDeformation[1] = ((float)sr.m_DetailDecalInfo.nDeformation[1]) * fUint8RatioToFloat;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetDetailDecalInfo(SInputShaderResources& sr, int propagationFlags)
{
	if ((propagationFlags & MTL_PROPAGATE_TEXTURES) == 0)
		return;

	const float fRatioToUint8 = 255.0f / 100.0f;
	sr.m_DetailDecalInfo.nBlending = (uint8)int_round(detailDecal.fBlending * fRatioToUint8);
	sr.m_DetailDecalInfo.nSSAOAmount = (uint8)int_round(detailDecal.fSSAOAmount * fRatioToUint8);

	Vec3 vTile = detailDecal.vTile[0];
	Vec3 vOffs = detailDecal.vOffset[0];
	sr.m_DetailDecalInfo.vTileOffs[0] = Vec4(vTile.x, vTile.y, vOffs.x, vOffs.y);
	sr.m_DetailDecalInfo.nThreshold[0] = (uint8) int_round(detailDecal.fThreshold[0] * fRatioToUint8);
	sr.m_DetailDecalInfo.nDeformation[0] = (uint8) int_round(detailDecal.fDeformation[0] * fRatioToUint8);
	sr.m_DetailDecalInfo.nRotation[0] = Degr2Word(detailDecal.fRotation[0]);

	vTile = detailDecal.vTile[1];
	vOffs = detailDecal.vOffset[1];
	sr.m_DetailDecalInfo.vTileOffs[1] = Vec4(vTile.x, vTile.y, vOffs.x, vOffs.y);
	sr.m_DetailDecalInfo.nThreshold[1] = (uint8) int_round(detailDecal.fThreshold[1] * fRatioToUint8);
	sr.m_DetailDecalInfo.nDeformation[1] = (uint8) int_round(detailDecal.fDeformation[1] * fRatioToUint8);
	sr.m_DetailDecalInfo.nRotation[1] = Degr2Word(detailDecal.fRotation[1]);
}

void CMaterialUI::PropagateToLinkedMaterial(CMaterial* mtl, CVarBlockPtr pShaderParams)
{
	if (!mtl)
		return;
	CMaterial* subMtl = NULL, * parentMtl = mtl->GetParent();
	const string& linkedMaterialName = matPropagate;
	int propFlags = 0;

	if (parentMtl)
	{
		for (int i = 0; i < parentMtl->GetSubMaterialCount(); ++i)
		{
			CMaterial* pMtl = parentMtl->GetSubMaterial(i);
			if (pMtl && pMtl != mtl && pMtl->GetFullName() == linkedMaterialName)
			{
				subMtl = pMtl;
				break;
			}
		}
	}
	if (!linkedMaterialName.IsEmpty() && subMtl)
	{
		// Ensure that the linked material is cleared if it can't be found anymore
		mtl->LinkToMaterial(linkedMaterialName);
	}
	// Note: It's only allowed to propagate the shader params and shadergen params
	// if we also propagate the actual shader to the linked material as well, else
	// bogus values will be set
	bPropagateShaderParams = (int)bPropagateShaderParams & - (int)bPropagateMaterialSettings;
	bPropagateShaderGenParams = (int)bPropagateShaderGenParams & - (int)bPropagateMaterialSettings;

	propFlags |= MTL_PROPAGATE_MATERIAL_SETTINGS & - (int)bPropagateMaterialSettings;
	propFlags |= MTL_PROPAGATE_OPACITY & - (int)bPropagateOpactity;
	propFlags |= MTL_PROPAGATE_LIGHTING & - (int)bPropagateLighting;
	propFlags |= MTL_PROPAGATE_ADVANCED & - (int)bPropagateAdvanced;
	propFlags |= MTL_PROPAGATE_TEXTURES & - (int)bPropagateTexture;
	propFlags |= MTL_PROPAGATE_SHADER_PARAMS & - (int)bPropagateShaderParams;
	propFlags |= MTL_PROPAGATE_SHADER_GEN & - (int)bPropagateShaderGenParams;
	propFlags |= MTL_PROPAGATE_VERTEX_DEF & - (int)bPropagateVertexDef;
	propFlags |= MTL_PROPAGATE_LAYER_PRESETS & - (int)bPropagateLayerPresets;
	mtl->SetPropagationFlags(propFlags);

	if (subMtl)
	{
		SetToMaterial(subMtl, propFlags | MTL_PROPAGATE_RESERVED);
		if (propFlags & MTL_PROPAGATE_SHADER_PARAMS)
		{
			if (CVarBlock* pPublicVars = subMtl->GetPublicVars(mtl->GetShaderResources()))
				subMtl->SetPublicVars(pPublicVars, subMtl);
		}
		if (propFlags & MTL_PROPAGATE_SHADER_GEN)
		{
			subMtl->SetShaderGenParamsVars(mtl->GetShaderGenParamsVars());
		}
		subMtl->Update();
		subMtl->UpdateMaterialLayers();
	}
}

void CMaterialUI::PropagateFromLinkedMaterial(CMaterial* mtl)
{
	if (!mtl)
		return;
	CMaterial* subMtl = NULL, * parentMtl = mtl->GetParent();
	const string& linkedMaterialName = mtl->GetLinkedMaterialName();
	//CVarEnumList<CString> *enumMtls  = new CVarEnumList<CString>;
	if (parentMtl)
	{
		for (int i = 0; i < parentMtl->GetSubMaterialCount(); ++i)
		{
			CMaterial* pMtl = parentMtl->GetSubMaterial(i);
			if (!pMtl || pMtl == mtl)
				continue;
			const string& subMtlName = pMtl->GetFullName();
			//enumMtls->AddItem(subMtlName, subMtlName);
			if (subMtlName == linkedMaterialName)
			{
				subMtl = pMtl;
				break;
			}
		}
	}
	matPropagate = string();
	//matPropagate.SetEnumList(enumMtls);
	if (!linkedMaterialName.IsEmpty() && !subMtl)
	{
		// Ensure that the linked material is cleared if it can't be found anymore
		mtl->LinkToMaterial(CString());
	}
	else
	{
		matPropagate = linkedMaterialName.GetString();
	}
	bPropagateMaterialSettings = mtl->GetPropagationFlags() & MTL_PROPAGATE_MATERIAL_SETTINGS;
	bPropagateOpactity = mtl->GetPropagationFlags() & MTL_PROPAGATE_OPACITY;
	bPropagateLighting = mtl->GetPropagationFlags() & MTL_PROPAGATE_LIGHTING;
	bPropagateTexture = mtl->GetPropagationFlags() & MTL_PROPAGATE_TEXTURES;
	bPropagateAdvanced = mtl->GetPropagationFlags() & MTL_PROPAGATE_ADVANCED;
	bPropagateVertexDef = mtl->GetPropagationFlags() & MTL_PROPAGATE_VERTEX_DEF;
	bPropagateShaderParams = mtl->GetPropagationFlags() & MTL_PROPAGATE_SHADER_PARAMS;
	bPropagateLayerPresets = mtl->GetPropagationFlags() & MTL_PROPAGATE_LAYER_PRESETS;
	bPropagateShaderGenParams = mtl->GetPropagationFlags() & MTL_PROPAGATE_SHADER_GEN;
}

void CMaterialUI::SetFromMaterial(CMaterial* mtlIn, bool bSetTemplate)
{
	CMaterial* mtl = mtlIn;
	if (mtl->GetMatTemplate().GetLength())
	{
		CMaterial* mtlTpl = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(mtl->GetMatTemplate(), false);
		if (mtlTpl)
		{
			mtl = mtlTpl;
		}
	}
	//
	string shaderName = mtl->GetShaderName();
	if (!shaderName.IsEmpty())
	{
		// Capitalize first letter.
		shaderName = string(1, (char)toupper(shaderName[0])) + shaderName.Mid(1);
	}

	shader = shaderName;

	int mtlFlags = mtl->GetFlags();
	bNoShadow = (mtlFlags & MTL_FLAG_NOSHADOW);
	bAdditive = (mtlFlags & MTL_FLAG_ADDITIVE);
	bDetailDecal = (mtlFlags & MTL_FLAG_DETAIL_DECAL);
	bWire = (mtlFlags & MTL_FLAG_WIRE);
	b2Sided = (mtlFlags & MTL_FLAG_2SIDED);
	bScatter = (mtlFlags & MTL_FLAG_SCATTER);
	bHideAfterBreaking = (mtlFlags & MTL_FLAG_HIDEONBREAK);
	bBlendTerrainColor = (mtlFlags & MTL_FLAG_BLEND_TERRAIN);
	bTraceableTexture = (mtlFlags & MTL_FLAG_TRACEABLE_TEXTURE);
	texUsageMask = mtl->GetTexmapUsageMask();

	allowLayerActivation = mtl->LayerActivationAllowed();

	// Detail, decal and custom textures are always active.
	const uint32 nDefaultFlagsEFTT = (1 << EFTT_DETAIL_OVERLAY) | (1 << EFTT_DECAL_OVERLAY) | (1 << EFTT_CUSTOM) | (1 << EFTT_CUSTOM_SECONDARY);
	texUsageMask |= nDefaultFlagsEFTT;
	if ((texUsageMask & (1 << EFTT_NORMALS)))
		texUsageMask |= 1 << EFTT_NORMALS;

	surfaceType = mtl->GetSurfaceTypeName();
	//
	if (!bSetTemplate)
	{
		matTemplate = mtlIn->GetMatTemplate();
	}
	//
	SetShaderResources(mtlIn->GetShaderResources(), mtl->GetShaderResources(), !bSetTemplate);

	// Propagate settings and properties to a sub material if edited
	PropagateFromLinkedMaterial(mtlIn);

	// set each material layer
	SMaterialLayerResources* pMtlLayerResources = mtl->GetMtlLayerResources();
	for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
	{
		materialLayers[l].shader = pMtlLayerResources[l].m_shaderName;
		materialLayers[l].bNoDraw = pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_NODRAW;
		materialLayers[l].bFadeOut = pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_FADEOUT;
	}
}

void CMaterialUI::SetToMaterial(CMaterial* mtl, int propagationFlags)
{
	if (propagationFlags & MTL_PROPAGATE_MATERIAL_SETTINGS)
		mtl->SetMatTemplate(matTemplate);
	int mtlFlags = mtl->GetFlags();

	if (propagationFlags & MTL_PROPAGATE_ADVANCED)
		if (bNoShadow)
			mtlFlags |= MTL_FLAG_NOSHADOW;
		else
			mtlFlags &= ~MTL_FLAG_NOSHADOW;

	if (propagationFlags & MTL_PROPAGATE_OPACITY)
		if (bAdditive)
			mtlFlags |= MTL_FLAG_ADDITIVE;
		else
			mtlFlags &= ~MTL_FLAG_ADDITIVE;

	if (propagationFlags & MTL_PROPAGATE_TEXTURES)
		if (bDetailDecal)
			mtlFlags |= MTL_FLAG_DETAIL_DECAL;
		else
			mtlFlags &= ~MTL_FLAG_DETAIL_DECAL;

	if (bWire)
		mtlFlags |= MTL_FLAG_WIRE;
	else
		mtlFlags &= ~MTL_FLAG_WIRE;

	if (propagationFlags & MTL_PROPAGATE_ADVANCED)
	{
		if (b2Sided)
			mtlFlags |= MTL_FLAG_2SIDED;
		else
			mtlFlags &= ~MTL_FLAG_2SIDED;

		if (bScatter)
			mtlFlags |= MTL_FLAG_SCATTER;
		else
			mtlFlags &= ~MTL_FLAG_SCATTER;

		if (bHideAfterBreaking)
			mtlFlags |= MTL_FLAG_HIDEONBREAK;
		else
			mtlFlags &= ~MTL_FLAG_HIDEONBREAK;

		if (bBlendTerrainColor)
			mtlFlags |= MTL_FLAG_BLEND_TERRAIN;
		else
			mtlFlags &= ~MTL_FLAG_BLEND_TERRAIN;

		if (bTraceableTexture)
			mtlFlags |= MTL_FLAG_TRACEABLE_TEXTURE;
		else
			mtlFlags &= ~MTL_FLAG_TRACEABLE_TEXTURE;
	}

	mtl->SetFlags(mtlFlags);

	mtl->SetLayerActivation(allowLayerActivation);

	// set each material layer
	if (propagationFlags & MTL_PROPAGATE_LAYER_PRESETS)
	{
		SMaterialLayerResources* pMtlLayerResources = mtl->GetMtlLayerResources();
		for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
		{
			pMtlLayerResources[l].SetFromUI((const string&)materialLayers[l].shader, materialLayers[l].bNoDraw, materialLayers[l].bFadeOut);
		}
	}

	if (propagationFlags & MTL_PROPAGATE_MATERIAL_SETTINGS)
	{
		mtl->SetSurfaceTypeName(surfaceType);
		// If shader name is different reload shader.
		mtl->SetShaderName(shader);
	}

	GetShaderResources(mtl->GetShaderResources(), propagationFlags);
}

//////////////////////////////////////////////////////////////////////////
class CMtlPickCallback : public IPickObjectCallback
{
public:
	CMtlPickCallback() { m_bActive = true; };
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked)
	{
		m_bActive = false;
		CMaterial* pMtl = (CMaterial*)picked->GetMaterial();
		if (pMtl)
			GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pMtl);
		delete this;
	}
	//! Called when pick mode canceled.
	virtual void OnCancelPick()
	{
		m_bActive = false;
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* filterObject)
	{
		// Check if object have material.
		if (filterObject->GetMaterial())
			return true;
		else
			return false;
	}
	static bool IsActive() { return m_bActive; };
private:
	static bool m_bActive;
};
bool CMtlPickCallback::m_bActive = false;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CMaterialDialog implementation.
//////////////////////////////////////////////////////////////////////////

// TODO : Replace the default constructor to the delegating constructor
// if C++11 available, like the following:
// CMaterialDialog::CMaterialDialog() : CMaterialDialog(AfxGetMainWnd(), CRect(0, 0, 0, 0)) {}
//
CMaterialDialog::CMaterialDialog()
//: CBaseLibraryDialog(IDD_DB_ENTITY, NULL)
	: m_bForceReloadPropsCtrl(true)
	, m_dragImage(nullptr)
	, m_pPrevSelectedItem(nullptr)
	, m_pMatManager(GetIEditorImpl()->GetMaterialManager())
	, m_vars(nullptr)
	, m_publicVars(nullptr)
	, m_publicVarsItems(nullptr)
	, m_shaderGenParamsVars(nullptr)
	, m_shaderGenParamsVarsItem(nullptr)
	, m_textureSlots(nullptr)
	, m_textureSlotsItem(nullptr)
	, m_pMaterialUI(new CMaterialUI)
	, m_hDropItem(nullptr)
	, m_hDraggedItem(nullptr)
	, m_pPreviewDlg(nullptr)
{
	m_pMaterialImageListCtrl.reset(new CMaterialImageListCtrl());

	m_pMatManager->AddListener(this);
	m_propsCtrl.SetUndoCallback(functor(*this, &CMaterialDialog::OnUndo));
	m_propsCtrl.SetStoreUndoByItems(false);

	// Immediately create dialog.
	CRect rc(0, 0, 0, 0);
	Create(WS_CHILD | WS_VISIBLE, rc, AfxGetMainWnd());
}

CMaterialDialog::CMaterialDialog(CWnd* pParent, RECT rc)
	: m_bForceReloadPropsCtrl(true)
	, m_dragImage(nullptr)
	, m_pPrevSelectedItem(nullptr)
	, m_pMatManager(GetIEditorImpl()->GetMaterialManager())
	, m_vars(nullptr)
	, m_publicVars(nullptr)
	, m_publicVarsItems(nullptr)
	, m_shaderGenParamsVars(nullptr)
	, m_shaderGenParamsVarsItem(nullptr)
	, m_textureSlots(nullptr)
	, m_textureSlotsItem(nullptr)
	, m_pMaterialUI(new CMaterialUI)
	, m_hDropItem(nullptr)
	, m_hDraggedItem(nullptr)
	, m_pPreviewDlg(nullptr)
{
	m_pMaterialImageListCtrl.reset(new CMaterialImageListCtrl());

	m_pMatManager->AddListener(this);
	m_propsCtrl.SetUndoCallback(functor(*this, &CMaterialDialog::OnUndo));
	m_propsCtrl.SetStoreUndoByItems(false);

	// Immediately create dialog.
	Create(WS_CHILD | WS_VISIBLE, rc, pParent);
}

//////////////////////////////////////////////////////////////////////////
CMaterialDialog::~CMaterialDialog()
{
	delete m_pMaterialUI;

	m_pMatManager->RemoveListener(this);
	m_wndMtlBrowser.SetImageListCtrl(NULL);

	m_propsCtrl.ClearUndoCallback();
	m_propsCtrl.RemoveAllItems();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::PostNcDestroy()
{
	delete this;
}

BEGIN_MESSAGE_MAP(CMaterialDialog, CBaseFrameWnd)
ON_COMMAND(ID_DB_ADD, OnAddItem)
ON_COMMAND(ID_DB_SAVE_ITEM, OnSaveItem)

ON_COMMAND(ID_DB_SELECTASSIGNEDOBJECTS, OnSelectAssignedObjects)
ON_COMMAND(ID_DB_ASSIGNTOSELECTION, OnAssignMaterialToSelection)
ON_COMMAND(ID_DB_GETFROMSELECTION, OnGetMaterialFromSelection)
ON_COMMAND(ID_DB_MTL_RESETMATERIAL, OnResetMaterialOnSelection)
ON_CBN_SELENDOK(IDC_MATERIAL_BROWSER_LISTTYPE, OnChangedBrowserListType)
ON_UPDATE_COMMAND_UI(ID_DB_ASSIGNTOSELECTION, OnUpdateAssignMtlToSelection)
ON_UPDATE_COMMAND_UI(ID_DB_SAVE_ITEM, OnUpdateMtlSaved)
ON_UPDATE_COMMAND_UI(ID_DB_SELECTASSIGNEDOBJECTS, OnUpdateMtlSelected)
ON_UPDATE_COMMAND_UI(ID_DB_GETFROMSELECTION, OnUpdateObjectSelected)
ON_UPDATE_COMMAND_UI(ID_DB_MTL_RESETMATERIAL, OnUpdateObjectSelected)

ON_COMMAND(ID_DB_MTL_PICK, OnPickMtl)
ON_UPDATE_COMMAND_UI(ID_DB_MTL_PICK, OnUpdatePickMtl)

ON_COMMAND(ID_DB_MTL_PREVIEW, OnMaterialPreview)

ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnDestroy()
{
	m_wndMtlBrowser.SetImageListCtrl(NULL);

	__super::OnDestroy();
}

BOOL CMaterialDialog::OnInitDialog()
{
	__super::OnInitDialog();

	if (gEnv->p3DEngine)
	{
		ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		if (pSurfaceTypeManager)
			pSurfaceTypeManager->LoadSurfaceTypes();
	}

	InitToolbar(IDR_DB_MATERIAL_BAR);

	CRect rc(0, 0, 0, 0);

	// Create status bar.
	{
		UINT indicators[] =
		{
			ID_SEPARATOR,           // status line indicator
		};
		VERIFY(m_statusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_TOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS));
		VERIFY(m_statusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)));
		m_statusBar.SetFont(CFont::FromHandle(SMFCFonts::GetInstance().hSystemFontBold));
	}

	m_propsCtrl.Create(WS_CHILD | WS_BORDER, rc, this, AFX_IDW_PANE_FIRST);
	m_vars = m_pMaterialUI->CreateVars();
	CPropertyItem* varsItems = m_propsCtrl.AddVarBlock(m_vars);

	m_publicVarsItems = m_propsCtrl.FindItemByVar(m_pMaterialUI->tableShaderParams.GetVar());
	m_shaderGenParamsVarsItem = m_propsCtrl.FindItemByVar(m_pMaterialUI->tableShaderGenParams.GetVar());
	m_textureSlotsItem = m_propsCtrl.FindItemByVar(m_pMaterialUI->tableTexture.GetVar());

	m_propsCtrl.EnableWindow(FALSE);
	m_propsCtrl.ShowWindow(SW_HIDE);

	//CXTPDockingPane *pDockPane1,*pDockPane2;

	{
		//m_wndCaption.Create(this, _T("Caption Text"), CPWS_EX_RAISED_EDGE,WS_VISIBLE|SS_CENTER|SS_CENTERIMAGE, CRect(0,0,0,0),5 );
		//m_wndCaption.SetCaptionColors(
		//GetXtremeColor(COLOR_3DFACE),    // border color.
		//GetXtremeColor(COLOR_3DSHADOW),  // background color.
		//GetXtremeColor(COLOR_WINDOW) );  // font color.
		//CXTPDockingPane *pDockPane = CreateDockingPane( "Caption",&m_wndCaption,IDW_MTL_PREVIEW_PANE+5,CRect(0,0,200,100),xtpPaneDockTop);
		//pDockPane->SetOptions(xtpPaneNoCloseable|xtpPaneNoCaption|xtpPaneNoHideable|xtpPaneNoFloatable);
	}

	//////////////////////////////////////////////////////////////////////////
	// Preview Pane
	//////////////////////////////////////////////////////////////////////////
	{
		m_pMaterialImageListCtrl->Create(ILC_STYLE_HORZ | WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, 3);

		CXTPDockingPane* pDockPane = CreateDockingPane("Preview", m_pMaterialImageListCtrl.get(), IDW_MTL_PREVIEW_PANE, CRect(0, 0, 100, 200), xtpPaneDockTop);
		pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoCaption | xtpPaneNoHideable | xtpPaneNoFloatable);
	}

	//////////////////////////////////////////////////////////////////////////
	// Browser Pane
	//////////////////////////////////////////////////////////////////////////
	{
		m_wndMtlBrowser.Create(CRect(0, 0, 210, 0), this, 2);
		m_wndMtlBrowser.SetListener(this);
		m_wndMtlBrowser.ShowWindow(SW_SHOW);
		m_wndMtlBrowser.SetImageListCtrl(m_pMaterialImageListCtrl.get());

		CXTPDockingPane* pDockPane = CreateDockingPane("Browser", &m_wndMtlBrowser, IDW_MTL_BROWSER_PANE, CRect(0, 0, 210, 300), xtpPaneDockLeft);
		pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoCaption | xtpPaneNoHideable | xtpPaneNoFloatable);
	}

	//////////////////////////////////////////////////////////////////////////
	// Properties pane
	//////////////////////////////////////////////////////////////////////////
	{
		//CXTPDockingPane *pDockPane = CreateDockingPane( "Properties",&m_propsCtrl,IDW_MTL_PROPERTIES_PANE,CRect(0,0,200,300),xtpPaneDockRight);
		//pDockPane->SetOptions(xtpPaneNoCloseable|xtpPaneNoCaption|xtpPaneNoHideable|xtpPaneNoFloatable);
	}

	m_wndMtlBrowser.ReloadItems(CMaterialBrowserCtrl::VIEW_ALL);

	RecalcLayout(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CMaterialDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CMaterialDialog::InitToolbar(UINT nToolbarResID)
{
	// Create Library toolbar.
	CXTPToolBar* pMtlToolBar = GetCommandBars()->Add(_T("MaterialToolBar"), xtpBarTop);
	pMtlToolBar->EnableCustomization(FALSE);
	VERIFY(pMtlToolBar->LoadToolBar(nToolbarResID));

	{
		CRect rc(0, 0, 150, 16);
		m_listTypeCtrl.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, rc, this, IDC_MATERIAL_BROWSER_LISTTYPE);
		m_listTypeCtrl.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
		m_listTypeCtrl.AddString("All Materials");
		m_listTypeCtrl.AddString("Used In Level");
		m_listTypeCtrl.SetCurSel(0);
	}

	// Create library control.
	{
		CXTPControl* pCtrl = pMtlToolBar->GetControls()->FindControl(xtpControlButton, IDC_MATERIAL_BROWSER_LISTTYPE, TRUE, FALSE);
		if (pCtrl)
		{
			//CXTPControlButton* pCustomCtrl = (CXTPControlButton*)pMtlToolBar->GetControls()->SetControlType(pCtrl->GetIndex(),xtpControlSplitButtonPopup);
			//pCustomCtrl->SetSize( CSize(150,16) );
			//pCustomCtrl->SetCaption( "Materials Library" );

			CXTPControlCustom* pCustomCtrl = (CXTPControlCustom*)pMtlToolBar->GetControls()->SetControlType(pCtrl->GetIndex(), xtpControlCustom);
			pCustomCtrl->SetSize(CSize(150, 16));
			pCustomCtrl->SetControl(&m_listTypeCtrl);
			pCustomCtrl->SetFlags(xtpFlagManualUpdate);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::ReloadItems()
{
	m_wndMtlBrowser.ReloadItems(CMaterialBrowserCtrl::VIEW_ALL);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnAddItem()
{
	m_wndMtlBrowser.OnAddNewMaterial();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnSaveItem()
{
	CMaterial* pMtl = GetSelectedMaterial();
	if (pMtl)
	{
		if (!pMtl->Save(false))
		{
			if (!pMtl->GetParent())
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The material file cannot be saved. The file is located in a PAK archive or access is denied"));
			}
		}

		pMtl->Reload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::SetMaterialVars(CMaterial* mtl)
{
}

//////////////////////////////////////////////////////////////////////////
//This is where it updates UI, it also updates public vars but tries to keep existing values
void CMaterialDialog::UpdateShaderParamsUI(CMaterial* pMtl)
{
	//////////////////////////////////////////////////////////////////////////
	// Shader Gen Mask.
	//////////////////////////////////////////////////////////////////////////
	if (m_shaderGenParamsVarsItem)
	{
		m_shaderGenParamsVars = pMtl->GetShaderGenParamsVars();
		m_propsCtrl.ReplaceVarBlock(m_shaderGenParamsVarsItem, m_shaderGenParamsVars);
	}

	//////////////////////////////////////////////////////////////////////////
	// Shader Public Params.
	//////////////////////////////////////////////////////////////////////////
	if (m_publicVarsItems)
	{
		bool bNeedUpdateMaterialFromUI = false;
		CVarBlockPtr pPublicVars = pMtl->GetPublicVars(pMtl->GetShaderResources());
		if (m_publicVars && pPublicVars)
		{
			// list of shader parameters depends on list of shader generation parameters
			// we need to keep values of vars which not presented in every combinations,
			// but probably adjusted by user, to keep his work.
			// m_excludedPublicVars is used for these values
			if (m_excludedPublicVars.pMaterial)
			{
				if (m_excludedPublicVars.pMaterial != pMtl)
				{
					m_excludedPublicVars.vars.DeleteAllVariables();
				}
				else
				{
					// find new presented vars in pPublicVars, which not existed in old m_publicVars
					for (int j = pPublicVars->GetNumVariables() - 1; j >= 0; --j)
					{
						IVariable* pVar = pPublicVars->GetVariable(j);
						bool isVarExist = false;
						for (int i = m_publicVars->GetNumVariables() - 1; i >= 0; --i)
						{
							IVariable* pOldVar = m_publicVars->GetVariable(i);
							if (!strcmp(pOldVar->GetName(), pVar->GetName()))
							{
								isVarExist = true;
								break;
							}
						}
						if (!isVarExist) // var exist in new pPublicVars block, but not in previous (m_publicVars)
						{
							// try to find value for this var inside "excluded vars" collection
							for (int i = m_excludedPublicVars.vars.GetNumVariables() - 1; i >= 0; --i)
							{
								IVariable* pStoredVar = m_excludedPublicVars.vars.GetVariable(i);
								if (!strcmp(pStoredVar->GetName(), pVar->GetName()) && pVar->GetDataType() == pStoredVar->GetDataType())
								{
									pVar->CopyValue(pStoredVar);
									m_excludedPublicVars.vars.DeleteVariable(pStoredVar);
									bNeedUpdateMaterialFromUI = true;
									break;
								}
							}
						}
					}
				}
			}

			m_excludedPublicVars.pMaterial = pMtl;

			// collect excluded vars from old block (m_publicVars)
			// which exist in m_publicVars but not in a new generated pPublicVars block
			for (int i = m_publicVars->GetNumVariables() - 1; i >= 0; --i)
			{
				IVariable* pOldVar = m_publicVars->GetVariable(i);
				bool isVarExist = false;
				for (int j = pPublicVars->GetNumVariables() - 1; j >= 0; --j)
				{
					IVariable* pVar = pPublicVars->GetVariable(j);
					if (!strcmp(pOldVar->GetName(), pVar->GetName()))
					{
						isVarExist = true;
						break;
					}
				}
				if (!isVarExist)
				{
					m_excludedPublicVars.vars.AddVariable(pOldVar->Clone(false));
				}
			}
		}

		m_publicVars = pPublicVars;
		if (m_publicVars)
		{
			m_publicVars->Sort();
			m_propsCtrl.ReplaceVarBlock(m_publicVarsItems, m_publicVars);
			if (bNeedUpdateMaterialFromUI)
				pMtl->SetPublicVars(m_publicVars, pMtl);
		}
	}

	if (m_textureSlotsItem)
	{
		m_textureSlots = pMtl->UpdateTextureNames(m_pMaterialUI->textureVars);
		m_propsCtrl.ReplaceVarBlock(m_textureSlotsItem, m_textureSlots);
	}

	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	static bool bNoRecursiveSelect = false;
	if (bNoRecursiveSelect)
		return;

	m_propsCtrl.Flush();

	bool bChanged = item != m_pPrevSelectedItem || bForceReload;

	if (!bChanged)
		return;

	m_pPrevSelectedItem = item;

	bNoRecursiveSelect = true;
	m_wndMtlBrowser.SelectItem(item, NULL);
	bNoRecursiveSelect = false;

	// Empty preview control.
	//m_previewCtrl.SetEntity(0);
	m_pMatManager->SetCurrentMaterial((CMaterial*)item);

	if (!item)
	{
		m_statusBar.SetWindowText("");
		m_propsCtrl.EnableWindow(FALSE);
		m_propsCtrl.ShowWindow(SW_HIDE);
		return;
	}

	m_wndMtlBrowser.SelectItem(item, NULL);

	// Render preview geometry with current material
	CMaterial* mtl = (CMaterial*)item;

	CString statusText;
	if (mtl->IsPureChild() && mtl->GetParent())
	{
		statusText = mtl->GetParent()->GetName() + " [" + mtl->GetName() + "]";
	}
	else
	{
		statusText = mtl->GetName();
	}

	if (mtl->IsFromEngine())
	{
		statusText += " (Old FC Material)";
	}
	else if (mtl->IsDummy())
	{
		statusText += " (Not Found)";
	}
	else if (!mtl->CanModify())
	{
		statusText += " (Read Only)";
	}
	m_statusBar.SetWindowText(statusText);

	if (mtl->IsMultiSubMaterial())
	{
		// Cannot edit it.
		m_propsCtrl.EnableWindow(FALSE);
		m_propsCtrl.EnableUpdateCallback(false);
		m_propsCtrl.ShowWindow(SW_HIDE);
		return;
	}

	m_propsCtrl.EnableWindow(TRUE);
	m_propsCtrl.EnableUpdateCallback(false);
	m_propsCtrl.ShowWindow(SW_SHOW);

	if (m_bForceReloadPropsCtrl)
	{
		// CPropertyCtrlEx skip OnPaint and another methods for redraw
		// OnSize method is forced to invalidate control for redraw
		m_propsCtrl.ShowWindow(SW_HIDE);
		m_propsCtrl.ShowWindow(SW_SHOW);
		m_bForceReloadPropsCtrl = false;
	}

	UpdatePreview();

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	m_pMaterialUI->SetFromMaterial(mtl);
	m_propsCtrl.EnableUpdateCallback(true);

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Set Shader Gen Params.
	//////////////////////////////////////////////////////////////////////////
	UpdateShaderParamsUI(mtl);
	//////////////////////////////////////////////////////////////////////////

	m_propsCtrl.SetUpdateCallback(functor(*this, &CMaterialDialog::OnUpdateProperties));
	m_propsCtrl.EnableUpdateCallback(true);

	if (mtl->IsDummy())
	{
		m_propsCtrl.EnableWindow(FALSE);
	}
	else
	{
		m_propsCtrl.EnableWindow(TRUE);
		m_propsCtrl.SetUpdateCallback(functor(*this, &CMaterialDialog::OnUpdateProperties));
		m_propsCtrl.EnableUpdateCallback(true);
		if (mtl->CanModify())
		{
			// Material can be modified.
			m_propsCtrl.SetFlags(m_propsCtrl.GetFlags() & (~CPropertyCtrl::F_GRAYED));
		}
		else
		{
			// Material cannot be modified.
			m_propsCtrl.SetFlags(m_propsCtrl.GetFlags() | CPropertyCtrl::F_GRAYED);
		}
	}
	if (mtl)
	{
		m_pMaterialImageListCtrl->SelectMaterial(mtl);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateProperties(IVariable* var)
{
	CMaterial* mtl = GetSelectedMaterial();
	if (!mtl)
		return;

	bool bHaveTemplate = (const string&)m_pMaterialUI->matTemplate != "";

	bool bShaderChanged = (m_pMaterialUI->shader == var);
	bool bShaderGenMaskChanged = false;
	if (m_shaderGenParamsVars)
		bShaderGenMaskChanged = m_shaderGenParamsVars->IsContainsVariable(var);

	bool bMtlLayersChanged = false;
	SMaterialLayerResources* pMtlLayerResources = mtl->GetMtlLayerResources();
	int nCurrLayer = -1;

	// Check for shader changes
	for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
	{
		if ((m_pMaterialUI->materialLayers[l].shader == var))
		{
			bMtlLayersChanged = true;
			nCurrLayer = l;
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Assign modified Shader Gen Params to shader.
	//////////////////////////////////////////////////////////////////////////
	if (bShaderGenMaskChanged && !bHaveTemplate)
	{
		mtl->SetShaderGenParamsVars(m_shaderGenParamsVars);
	}
	//////////////////////////////////////////////////////////////////////////
	// Invalidate material and save changes.
	//m_pMatManager->MarkMaterialAsModified(mtl);
	//
	// update cur material from template material
	CMaterial* pTemplateMtl = NULL;
	if (bHaveTemplate)
	{
		pTemplateMtl = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(m_pMaterialUI->matTemplate, false);
		if (pTemplateMtl && pTemplateMtl->GetMatInfo()) // Ensure loaded!
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();

			m_pMaterialUI->SetFromMaterial(pTemplateMtl, true);
			GetIEditorImpl()->GetIUndoManager()->Resume();
		}
	}

	mtl->RecordUndo("Material parameter", true);
	m_pMaterialUI->SetToMaterial(mtl);//skahn: This is where it actually sets the properties
	mtl->Update();

	//
	//////////////////////////////////////////////////////////////////////////
	// Assign new public vars to material.
	// Must be after material update.
	//////////////////////////////////////////////////////////////////////////

	GetIEditorImpl()->GetIUndoManager()->Suspend();

	if (m_publicVars != NULL && !bShaderChanged)
	{
		if (pTemplateMtl)
		{
			// update from template
			mtl->SetPublicVars(pTemplateMtl->GetPublicVars(pTemplateMtl->GetShaderResources()), pTemplateMtl);
		}
		else
		{
			mtl->SetPublicVars(m_publicVars, mtl);
		}
	}

	/*
	   bool bUpdateLayers = false;
	   for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
	   {
	    if ( m_varsMtlLayersShaderParams[l] != NULL && l != nCurrLayer)
	    {
	      SMaterialLayerResources *pCurrResource = pTemplateMtl ? &pTemplateMtl->GetMtlLayerResources()[l] : &pMtlLayerResources[l];
	      SShaderItem &pCurrShaderItem = pCurrResource->m_pMatLayer->GetShaderItem();
	      CVarBlock* pVarBlock = pTemplateMtl ? pTemplateMtl->GetPublicVars( pCurrResource->m_shaderResources ) : m_varsMtlLayersShaderParams[l];
	      mtl->SetPublicVars( pVarBlock, pCurrResource->m_shaderResources, pCurrShaderItem.m_pShaderResources, pCurrShaderItem.m_pShader);
	      bUpdateLayers = true;
	    }
	   }
	 */
	//if( bUpdateLayers )
	{
		mtl->UpdateMaterialLayers();
	}
	//
	// update derived materials
	if (((const string&)m_pMaterialUI->matTemplate == "") && (mtl->GetName() != ""))
	{
		IDataBaseItemEnumerator* pEnum = GetIEditorImpl()->GetMaterialManager()->GetItemEnumerator();
		for (IDataBaseItem* pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
		{
			CString str = pItem->GetName();
			CMaterial* pLoadedMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(str, false);
			if (pLoadedMaterial)
			{
				if (pLoadedMaterial->GetMatTemplate() == mtl->GetName())
				{
					// update mtl by templ mtl
					int flags = pLoadedMaterial->GetFlags();
					flags = (flags & ~MTL_FLAGS_TEMPLATE_MASK) | (mtl->GetFlags() & MTL_FLAGS_TEMPLATE_MASK);
					pLoadedMaterial->SetFlags(flags);
					pLoadedMaterial->SetShaderName(mtl->GetShaderName());
					pLoadedMaterial->SetShaderGenMask(mtl->GetShaderGenMask());
					pLoadedMaterial->SetSurfaceTypeName(mtl->GetSurfaceTypeName());
					//m_pMaterialUI->SetShaderResources( pLoadedMaterial->GetShaderResources(), mtl->GetShaderResources(), false );
					pLoadedMaterial->GetShaderResources().m_ShaderParams = mtl->GetShaderResources().m_ShaderParams;
					gEnv->p3DEngine->GetMaterialManager()->CopyMaterial(mtl->GetMatInfo(), pLoadedMaterial->GetMatInfo(), MTL_COPY_DEFAULT);
					//
				}
			}
		}
		pEnum->Release();
	}

	m_pMaterialUI->PropagateToLinkedMaterial(mtl, m_shaderGenParamsVars);
	if (var)
	{
		GetIEditorImpl()->GetMaterialManager()->HighlightedMaterialChanged(mtl);
	}

	GetIEditorImpl()->GetIUndoManager()->Resume();

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	if (bShaderChanged || bShaderGenMaskChanged || bMtlLayersChanged)
	{
		m_pMaterialUI->SetFromMaterial(mtl);
	}

	UpdatePreview();

	// When shader changed.
	if (bShaderChanged || bShaderGenMaskChanged || bMtlLayersChanged)
	{
		//////////////////////////////////////////////////////////////////////////
		// Set material layers params
		//////////////////////////////////////////////////////////////////////////
		/*
		    if( bMtlLayersChanged) // only update changed shader in material layers
		    {
		      SMaterialLayerResources *pCurrResource = &pMtlLayerResources[nCurrLayer];

		      // delete old property item
		      if ( m_varsMtlLayersShaderParamsItems[nCurrLayer] )
		      {
		        m_propsCtrl.DeleteItem( m_varsMtlLayersShaderParamsItems[nCurrLayer] );
		        m_varsMtlLayersShaderParamsItems[nCurrLayer] = 0;
		      }

		      m_varsMtlLayersShaderParams[nCurrLayer] = mtl->GetPublicVars( pCurrResource->m_shaderResources );

		      if ( m_varsMtlLayersShaderParams[nCurrLayer] )
		      {
		        m_varsMtlLayersShaderParamsItems[nCurrLayer] = m_propsCtrl.AddVarBlockAt( m_varsMtlLayersShaderParams[nCurrLayer], "Shader Params", m_varsMtlLayersShaderItems[nCurrLayer] );
		      }
		    }
		 */

		UpdateShaderParamsUI(mtl);
	}

	if (bShaderGenMaskChanged || bShaderChanged || bMtlLayersChanged)
		m_propsCtrl.Invalidate();

	m_pMaterialImageListCtrl->InvalidateMaterial(mtl);
	m_wndMtlBrowser.IdleSaveMaterial();
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialDialog::GetSelectedMaterial()
{
	CBaseLibraryItem* pItem = m_pMatManager->GetCurrentMaterial();
	return (CMaterial*)pItem;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnAssignMaterialToSelection()
{
	GetIEditorImpl()->ExecuteCommand("material.assign_current_to_selection");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnSelectAssignedObjects()
{
	GetIEditorImpl()->ExecuteCommand("material.select_objects_with_current");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnResetMaterialOnSelection()
{
	GetIEditorImpl()->ExecuteCommand("material.reset_selection");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnGetMaterialFromSelection()
{
	GetIEditorImpl()->ExecuteCommand("material.set_current_from_object");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::DeleteItem(CBaseLibraryItem* pItem)
{
	m_wndMtlBrowser.DeleteItem();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateMtlSelected(CCmdUI* pCmdUI)
{
	if (GetSelectedMaterial())
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateMtlSaved(CCmdUI* pCmdUI)
{
	CMaterial* mtl = GetSelectedMaterial();
	if (mtl && mtl->CanModify(false))
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateAssignMtlToSelection(CCmdUI* pCmdUI)
{
	if (GetSelectedMaterial() && (!GetIEditorImpl()->GetSelection()->IsEmpty() || GetIEditorImpl()->IsInPreviewMode()))
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateObjectSelected(CCmdUI* pCmdUI)
{
	if (!GetIEditorImpl()->GetSelection()->IsEmpty() || GetIEditorImpl()->IsInPreviewMode())
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnPickMtl()
{
	if (GetIEditorImpl()->GetEditTool() && GetIEditorImpl()->GetEditTool()->GetRuntimeClass()->m_lpszClassName == "CMaterialPickTool")
	{
		GetIEditorImpl()->SetEditTool(NULL);
	}
	else
		GetIEditorImpl()->SetEditTool("EditTool.PickMaterial");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdatePickMtl(CCmdUI* pCmdUI)
{
	if (GetIEditorImpl()->GetEditTool() && GetIEditorImpl()->GetEditTool()->GetRuntimeClass()->m_lpszClassName == "CMaterialPickTool")
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnCopy()
{
	m_wndMtlBrowser.OnCopy();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnPaste()
{
	m_wndMtlBrowser.OnPaste();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnMaterialPreview()
{
	m_pPreviewDlg = new CMatEditPreviewDlg(0, this);
	//m_pPreviewDlg->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialDialog::SetItemName(CBaseLibraryItem* item, const CString& groupName, const CString& itemName)
{
	assert(item);
	// Make prototype name.
	CString fullName = groupName + "/" + itemName;
	IDataBaseItem* pOtherItem = m_pMatManager->FindItemByName(fullName);
	if (pOtherItem && pOtherItem != item)
	{
		// Ensure uniqness of name.
		Warning("Duplicate Item Name %s", (const char*)fullName);
		return false;
	}
	else
	{
		item->SetName(fullName);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnBrowserSelectItem(IDataBaseItem* pItem)
{
	SelectItem((CBaseLibraryItem*)pItem, true);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::UpdatePreview()
{
};

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnChangedBrowserListType()
{
	int sel = m_listTypeCtrl.GetCurSel();
	if (sel == 0)
		m_wndMtlBrowser.ReloadItems(CMaterialBrowserCtrl::VIEW_ALL);
	else if (sel == 1)
		m_wndMtlBrowser.ReloadItems(CMaterialBrowserCtrl::VIEW_LEVEL);

	m_pMatManager->SetCurrentMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUndo(IVariable* pVar)
{
	if (!m_pMatManager->GetCurrentMaterial())
		return;

	CString undoName;
	if (pVar)
		undoName.Format("%s modified", pVar->GetName());
	else
		undoName = "Material parameter was modified";

	if (!CUndo::IsRecording())
	{
		if (!CUndo::IsSuspended())
		{
			CUndo undo(undoName);
			m_pMatManager->GetCurrentMaterial()->RecordUndo(undoName, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	//Note: here we used to select the item on update properties however with the advent of the new material editor this does not make sense
}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		return AfxGetMainWnd()->PreTranslateMessage(pMsg);
	}

	return __super::PreTranslateMessage(pMsg);
}

