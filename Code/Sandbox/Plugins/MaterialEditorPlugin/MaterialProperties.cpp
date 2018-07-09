// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialProperties.h"
#include "QAdvancedPropertyTree.h"

#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Decorators/Resources.h>

#include "IUndoManager.h"

//////////////////////////////////////////////////////////////////////////

//! Property tree displaying the serialized material properties
class CMaterialSerializer::CMaterialPropertyTree : public QAdvancedPropertyTree, public IDataBaseManagerListener
{
public:
	CMaterialPropertyTree(CMaterialSerializer* pMaterialSerializer);
	~CMaterialPropertyTree();

public slots:
	void OnUndoPush();
	void OnChanged();

private:
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event) override;

	_smart_ptr<CMaterialSerializer> m_serializer;
};

CMaterialSerializer::CMaterialPropertyTree::CMaterialPropertyTree(CMaterialSerializer* pMaterialSerializer)
	: QAdvancedPropertyTree("MaterialProperties")
	, m_serializer(pMaterialSerializer)
{
	setExpandLevels(2);
	setValueColumnWidth(0.2f);
	setAggregateMouseEvents(false);
	setFullRowContainers(true);
	setSizeToContent(true);
	setUndoEnabled(false);

	QObject::connect(this, &CMaterialPropertyTree::signalPushUndo, this, &CMaterialPropertyTree::OnUndoPush);
	QObject::connect(this, &CMaterialPropertyTree::signalChanged, this, &CMaterialPropertyTree::OnChanged);

	attach(Serialization::SStruct(*m_serializer.get()));

	GetIEditor()->GetMaterialManager()->AddListener(this);

	setEnabled(!m_serializer->m_bIsReadOnly);
}

CMaterialSerializer::CMaterialPropertyTree::~CMaterialPropertyTree()
{
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
}

void CMaterialSerializer::CMaterialPropertyTree::OnUndoPush()
{
	GetIEditor()->GetIUndoManager()->Begin();
	m_serializer->m_pMaterial->RecordUndo("Material Edited");
}

void CMaterialSerializer::CMaterialPropertyTree::OnChanged()
{
	GetIEditor()->GetIUndoManager()->Accept("Material Edited");
}

void CMaterialSerializer::CMaterialPropertyTree::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (!pItem || event == EDB_ITEM_EVENT_SELECTED)
		return;

	if (pItem == (IDataBaseItem*)m_serializer->m_pMaterial.get())
	{
		switch (event)
		{
		case EDB_ITEM_EVENT_CHANGED:
		case EDB_ITEM_EVENT_UPDATE_PROPERTIES:
			if (!m_serializer->m_bIsBeingChanged)//TODO: there should be a better way to identify changes that come from this property tree
				revert();
			break;
		default:
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////

//Serialization of IShader enums for UI only

//EDeformType: many values are intentionally omitted from the UI as they were omitted from the old material editor
SERIALIZATION_ENUM_BEGIN(EDeformType, "Deform Type")
SERIALIZATION_ENUM(eDT_Unknown, "none", "None")
SERIALIZATION_ENUM(eDT_SinWave, "sin", "Sin Wave");
SERIALIZATION_ENUM(eDT_SinWaveUsingVtxColor, "aipoint", "Sin Wave Using Vertex Color");
SERIALIZATION_ENUM(eDT_Bulge, "bulge", "Bulge");
SERIALIZATION_ENUM(eDT_Squeeze, "squeeze", "Squeeze");
SERIALIZATION_ENUM(eDT_Bending, "bending", "Bending");
SERIALIZATION_ENUM(eDT_FixedOffset, "fixedOffset", "Fixed Offset");
SERIALIZATION_ENUM_END()

//EWaveForm: many values are intentionally omitted from the UI as they were omitted from the old material editor
SERIALIZATION_ENUM_BEGIN(EWaveForm, "Wave Type")
SERIALIZATION_ENUM(eWF_None, "none", "None")
SERIALIZATION_ENUM(eWF_Sin, "sin", "Sin")
SERIALIZATION_ENUM_END()


SERIALIZATION_ENUM_BEGIN(ETexGenType, "Texture Gen Type")
SERIALIZATION_ENUM(ETG_Stream, "stream", "Stream")
SERIALIZATION_ENUM(ETG_World, "world", "World")
SERIALIZATION_ENUM(ETG_Camera, "camera", "Camera")
SERIALIZATION_ENUM_END()


SERIALIZATION_ENUM_BEGIN(ETexModRotateType, "Texture Mod Rotate Type")
SERIALIZATION_ENUM(ETMR_NoChange, "nochange", "No Change")
SERIALIZATION_ENUM(ETMR_Fixed, "fixed", "Fixed")
SERIALIZATION_ENUM(ETMR_Constant, "constant", "Constant")
SERIALIZATION_ENUM(ETMR_Oscillated, "oscillated", "Oscillated")
SERIALIZATION_ENUM_END()


SERIALIZATION_ENUM_BEGIN(ETexModMoveType, "Texture Mod Move Type")
SERIALIZATION_ENUM(ETMM_NoChange, "nochange", "No Change")
SERIALIZATION_ENUM(ETMM_Fixed, "fixed", "Fixed")
SERIALIZATION_ENUM(ETMM_Constant, "constant", "Constant")
SERIALIZATION_ENUM(ETMM_Jitter, "jitter", "Jitter")
SERIALIZATION_ENUM(ETMM_Pan, "pan", "Pan")
SERIALIZATION_ENUM(ETMM_Stretch, "stretch", "Stretch")
SERIALIZATION_ENUM(ETMM_StretchRepeat, "stretch_repeat", "Stretch Repeat")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EFilterPreset, "Texture Filter Preset")
SERIALIZATION_ENUM(EFilterPreset::Unspecified, "default", "Default")
SERIALIZATION_ENUM(EFilterPreset::Point, "point", "Point")
SERIALIZATION_ENUM(EFilterPreset::Linear, "linear", "Linear")
SERIALIZATION_ENUM(EFilterPreset::Bilinear, "bilinear", "Bilinear")
SERIALIZATION_ENUM(EFilterPreset::Trilinear, "trilinear", "Trilinear")
SERIALIZATION_ENUM(EFilterPreset::Anisotropic2x, "a2x", "Anisotropic2x")
SERIALIZATION_ENUM(EFilterPreset::Anisotropic4x, "a4x", "Anisotropic4x")
SERIALIZATION_ENUM(EFilterPreset::Anisotropic8x, "a8x", "Anisotropic8x")
SERIALIZATION_ENUM(EFilterPreset::Anisotropic16x, "a16x", "Anisotropic16x")
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////


void SerializeWordToDegree(Serialization::IArchive& ar, uint16& word, char* name, char* label)
{
	float degree = Word2Degr(word);

	//round degree value
	degree = (float)((int)(degree * 100 + 0.5f)) * 0.01f;

	ar(Serialization::Decorators::Range<float>(degree, 0, 360, 0.5f), name, label);

	if(ar.isInput())
		word = Degr2Word(degree);
}

template<typename T>
void SetFlag(T& bitFieldInOut, uint64 mask, bool set)
{
	static_assert(std::is_integral<T>::value, "Type must be an integer type");
	if (set)
		bitFieldInOut |= mask;
	else
		bitFieldInOut &= ~mask;
}


CMaterialSerializer::CMaterialSerializer(CMaterial* pMaterial, bool readOnly)
	: m_pMaterial(pMaterial)
	, m_bIsBeingChanged(false)
	, m_bIsReadOnly(readOnly)
{
	//Makes sure the material's mat info is loaded as well as the shader
	m_pMaterial->GetMatInfo();

	//This is currently done every time the serializer is recreated, if this becomes too expensive it can be cached
	PopulateStringLists();
}

void CMaterialSerializer::Serialize(Serialization::IArchive& ar)
{
	//TODO : Add tooltips for help here!! ar.doc("tooltip of the last serialized field")

	//This should only exist in an editing context
	CRY_ASSERT(ar.isEdit());

	if (ar.isInput())
	{
		m_bIsBeingChanged = true;
	}

	CMaterial* pMtl = m_pMaterial;
	SInputShaderResources& shaderResources = m_pMaterial->GetShaderResources();
	int mtlFlags = pMtl->GetFlags();
	bool bShaderChanged = false;
	bool bHasDetailDecal = false;

	{
		ar.openBlock("materialSettings", "Material Settings");

		/* Notes on template system:

		Observation from usage:
		* Setting the template will govern most parameters of the material (should really disable these fields in UI), this is confirmed by CMaterialUI::SetFromMaterial code
		* Only a few parameters can be changed from template, texture maps and shader params seems like some of them
		* Changing shaders or shader gen params lead to crash

		Observation from code:
		* CMatInfo::m_sMatTemplate is defined out in non editor targets and unused in engine, there may not be a runtime system to support templates
		* Material template seems to work from an engine perspective
		* Template recursion (template that has template) seems to be poorly handled by ui
		* SetFromMaterial(mtl, false) is called on select which sets the template (parameter if backwards)
		* SetFromMaterial(template, true) is called before setting properties  > this does not set the template, I assume this is meant to handle template recursion ?
		* On property change, if shader changed, shader gen mask changed or mtlLayers changed, it calls SetFromMaterial(mtl, false) again (which i suspect leads to crashes)

		How do to it properly:
		* Template may should not be set in the property tree but rather in the menus / toolbars
		* Once set the template should appear in the property tree and all the fields inherited should be greyed out

		*/
		/*string mtlTemplate = ;
		ar(Serialization::MaterialPicker(mtlTemplate), "mtl_template", "Template Material");*/

		
		{
			//Note: If a material was saved and the shader removed, we will get asserts, this is unavoidable with yasli StringListValue...
			string shader = pMtl->GetShaderName();
			//Note: the shader list always has capitalized letters therefore we must do the same here
			shader.SetAt(0, toupper(shader[0]));
			Serialization::StringListValue value(m_shaderStringList, shader);
			const auto oldIndex = value.index();

			//value not found in list, let's try to resolve it to something else
			if (oldIndex == -1)
			{
				for (int i = 0; i < m_shaderStringList.size(); i++)
				{
					if (shader.CompareNoCase(m_shaderStringList[i]) == 0)
					{
						value = i;
						break;
					}
				}

				//if not found default to Nodraw
				if (value.index() == -1)
				{
					value = "Nodraw";
				}
			}

			ar(value, "shader", "Shader");

			if (ar.isInput() && value.index() != oldIndex)
			{
				pMtl->SetShaderName(value.c_str());
				bShaderChanged = true;
			}
		}

		{
			string surfaceType = pMtl->GetSurfaceTypeName();

			//Remove "mat_" from the string
			if (surfaceType.Left(4) == "mat_" && surfaceType.length() > 4)
				surfaceType = surfaceType.Mid(4);
			else if (surfaceType == "")
				surfaceType = "None";

			Serialization::StringListValue value(m_surfaceTypeStringList, surfaceType);
			const auto oldIndex = value.index();

			if (oldIndex == -1)
			{
				value = "None";
			}

			ar(value, "surfaceType", "Surface Type");

			if(ar.isInput() && value.index() != oldIndex)
			{
				surfaceType = value.c_str();
				if (surfaceType == "None")
					surfaceType = "";
				else
				{
					//note that surface types without "mat_" prefix will not work here
					surfaceType.insert(0, "mat_");
				}

				//Do not update mat info yet
				pMtl->SetSurfaceTypeName(surfaceType, false);
			}
		}

		ar.closeBlock();
	}

	{
		ar.openBlock("opacitySettings", "Opacity Settings");

		ar(Serialization::Decorators::Range<float>(shaderResources.m_LMaterial.m_Opacity, 0, 1, 0.01), "opacity", "Opacity");
		ar(Serialization::Decorators::Range<float>(shaderResources.m_AlphaRef, 0, 1, 0.01), "alphaTest", "Alpha Test");

		bool bAdditive = (mtlFlags & MTL_FLAG_ADDITIVE);
		ar(bAdditive, "bAdditive", "Additive");
		SetFlag(mtlFlags, MTL_FLAG_ADDITIVE, bAdditive);

		ar.closeBlock();
	}

	{
		ar.openBlock("lightingSettings", "Lighting Settings");

		ar(shaderResources.m_LMaterial.m_Diffuse, "diffuseColor", "Diffuse Color");
		ar(shaderResources.m_LMaterial.m_Specular, "specularColor", "Specular Color");

		ColorF emissiveColor = shaderResources.m_LMaterial.m_Emittance;
		emissiveColor.a = 1.f; //alpha is used for emissive intensity
		ar(emissiveColor, "emissiveColor", "Emissive Color");

		float emissiveIntensity = shaderResources.m_LMaterial.m_Emittance.a;
		//soft cap at 200
		ar(Serialization::Decorators::Range<float>(emissiveIntensity, 0.f, FLT_MAX, 0.f, 200.f, 1.f), "emissiveIntensity", "Emissive Intensity (kcd/m2)");

		emissiveColor.a = emissiveIntensity;
		shaderResources.m_LMaterial.m_Emittance = emissiveColor;

		//Note: smoothness is a float but is between 0 and 255 apparently, let's display it between 0 and 1
		float smoothness = shaderResources.m_LMaterial.m_Smoothness / 255.f;
		ar(Serialization::Decorators::Range<float>(smoothness, 0.f, 1.f, 0.01f), "smoothness", "Smoothness");
		shaderResources.m_LMaterial.m_Smoothness = smoothness * 255.f;

		ar.closeBlock();
	}

	SerializeTextureSlots(ar, bHasDetailDecal);

	{
		ar.openBlock("advanced", "Advanced");

		//Note : in old material editor we also handled wireframe flag in code but not in UI, omitting it for now

		bool bAllowLayerActivation = pMtl->LayerActivationAllowed();
		ar(bAllowLayerActivation, "bAllowLayerActivation", "Allow Layer Activation");
		pMtl->SetLayerActivation(bAllowLayerActivation);

		bool b2Sided = (mtlFlags & MTL_FLAG_2SIDED);
		ar(b2Sided, "b2Sided", "Two sided");
		SetFlag(mtlFlags, MTL_FLAG_2SIDED, b2Sided);

		bool bNoShadow = (mtlFlags & MTL_FLAG_NOSHADOW);
		ar(bNoShadow, "bNoShadow", "No Shadow");
		SetFlag(mtlFlags, MTL_FLAG_NOSHADOW, bNoShadow);

		bool bScatter = (mtlFlags & MTL_FLAG_SCATTER);
		ar(bScatter, "bScatter", "Use Scattering");
		SetFlag(mtlFlags, MTL_FLAG_SCATTER, bScatter);

		bool bHideAfterBreaking = (mtlFlags & MTL_FLAG_HIDEONBREAK);
		ar(bHideAfterBreaking, "bHideAfterBreaking", "Hide After Breaking");
		SetFlag(mtlFlags, MTL_FLAG_HIDEONBREAK, bHideAfterBreaking);

		bool bBlendTerrainColor = (mtlFlags & MTL_FLAG_BLEND_TERRAIN);
		ar(bBlendTerrainColor, "bBlendTerrainColor", "Blend Terrain Color");
		SetFlag(mtlFlags, MTL_FLAG_BLEND_TERRAIN, bBlendTerrainColor);

		bool bTraceableTexture = (mtlFlags & MTL_FLAG_TRACEABLE_TEXTURE);
		ar(bTraceableTexture, "bTraceableTexture", "Traceable Texture");
		SetFlag(mtlFlags, MTL_FLAG_TRACEABLE_TEXTURE, bTraceableTexture);

		if (bHasDetailDecal)
		{
			bool bDetailDecal = (mtlFlags & MTL_FLAG_DETAIL_DECAL);
			ar(bDetailDecal, "bDetailDecal", "Detail Decal");
			SetFlag(mtlFlags, MTL_FLAG_DETAIL_DECAL, bDetailDecal);
		}

		//TODO : These features are not handled in the renderer, need clean up here and in old material editor
		float furAmount = (float)shaderResources.m_FurAmount / 255.0f;
		ar(Serialization::Decorators::Range<float>(furAmount, 0, 1), "furAmount", "Fur Amount");
		shaderResources.m_FurAmount = int_round(furAmount * 255.0f);

		float voxelCoverage = (float)shaderResources.m_VoxelCoverage / 255.0f;
		ar(Serialization::Decorators::Range<float>(voxelCoverage, 0, 1), "voxelCoverage", "Voxel Coverage");
		shaderResources.m_VoxelCoverage = int_round(voxelCoverage * 255.0f);

		const float fHeatRange = MAX_HEATSCALE;
		float heatAmount = (float)(shaderResources.m_HeatAmount / 255.0f) * fHeatRange; // rescale range
		ar(Serialization::Decorators::Range<float>(heatAmount, 0, 4), "heatAmount", "Heat Amount");
		shaderResources.m_HeatAmount = int_round(heatAmount * 255.0f / fHeatRange);

		float cloakAmount = (float)shaderResources.m_CloakAmount / 255.0f;
		ar(Serialization::Decorators::Range<float>(cloakAmount, 0, 1), "cloakAmount", "Cloak Amount");
		shaderResources.m_CloakAmount = int_round(cloakAmount * 255.0f);

		ar.closeBlock();
	}

	SerializeShaderParams(ar, bShaderChanged);

	SerializeShaderGenParams(ar);

	{
		ar.openBlock("vertexDeform", "-Vertex Deformation");

		//Note: Vertex deform structure is only partially serialized, there are fields that are present in engine structures but not displayed in UI
		//The old material editor's behavior was followed here however a better approach would be 
		//to make engine structures serializable and remove the unused fields or enum values there

		ar(shaderResources.m_DeformInfo.m_eType, "deformType", "Deform Type");

		if (shaderResources.m_DeformInfo.m_eType != eDT_Unknown)//None selected : don't display parameters
		{
			ar(shaderResources.m_DeformInfo.m_fDividerX, "waveLength", "Wave Length");
			ar(shaderResources.m_DeformInfo.m_vNoiseScale, "noiseScale", "Noise Scale");

			//TODO: "none" here never makes sense, and since there is only sin, maybe remove this field altogether and always set it to sine wave ?
			ar(shaderResources.m_DeformInfo.m_WaveX.m_eWFType, "waveType", "Wave Type");

			if (shaderResources.m_DeformInfo.m_WaveX.m_eWFType != eWF_None)
			{
				//TODO: those parameters probably need a range, especially phase
				ar.openBlock("waveParams", "Wave Parameters");
				
				ar(shaderResources.m_DeformInfo.m_WaveX.m_Level, "level", "Level");
				ar(shaderResources.m_DeformInfo.m_WaveX.m_Amp, "amplitude", "Amplitude");
				ar(shaderResources.m_DeformInfo.m_WaveX.m_Phase, "phase", "Phase");
				ar(shaderResources.m_DeformInfo.m_WaveX.m_Freq, "frequency", "Frequency");

				ar.closeBlock();
			}
		}

		ar.closeBlock();
	}

	{
		//TODO: this system is apparently unused in production, should this be removed?
		//looking at the code of the old material editor, it seems most of the logic is commented out
		ar.openBlock("layerPresets", "-Layer Presets");

		SMaterialLayerResources* pMtlLayerResources = pMtl->GetMtlLayerResources();

		for (int i = 0; i < MTL_LAYER_MAX_SLOTS; i++)
		{
			string shaderName = pMtlLayerResources[i].m_shaderName;
			Serialization::StringListValue shaderNameValue(m_shaderStringListWithEmpty, shaderName);
			ar(shaderNameValue, "shader", "Shader");

			bool noDraw = pMtlLayerResources[i].m_nFlags & MTL_LAYER_USAGE_NODRAW;
			ar(noDraw, "noDraw", "No Draw");

			bool fadeOut = pMtlLayerResources[i].m_nFlags & MTL_LAYER_USAGE_FADEOUT;
			ar(fadeOut, "fadeOut", "Fade Out");

			pMtlLayerResources[i].SetFromUI(shaderNameValue.c_str(), noDraw, fadeOut);
		}

		ar.closeBlock();
	}

	//Propagation system currently disabled, is not supported by this material editor and not used in production
	/*{
		
		ar.openBlock("propagation", "-Propagation");

		int mtlPropagationFlags = pMtl->GetPropagationFlags();

		string linkedMaterial = pMtl->GetLinkedMaterialName();
		ar(Serialization::MaterialPicker(linkedMaterial), "linkedMaterial", "Linked Material");

		bool bPropagateMaterialSettings = mtlPropagationFlags & MTL_PROPAGATE_MATERIAL_SETTINGS;
		ar(bPropagateMaterialSettings, "bPropagateMaterialSettings", "Propagate Material Settings");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_MATERIAL_SETTINGS, bPropagateMaterialSettings);

		bool bPropagateOpactity = mtlPropagationFlags & MTL_PROPAGATE_OPACITY;
		ar(bPropagateOpactity, "bPropagateOpactity", "Propagate Opacity Settings");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_OPACITY, bPropagateOpactity);

		bool bPropagateLighting = mtlPropagationFlags & MTL_PROPAGATE_LIGHTING;
		ar(bPropagateLighting, "bPropagateLighting", "Propagate Lighting Settings");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_LIGHTING, bPropagateLighting);

		bool bPropagateAdvanced = mtlPropagationFlags & MTL_PROPAGATE_ADVANCED;
		ar(bPropagateAdvanced, "bPropagateAdvanced", "Propagate Advanced Settings");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_ADVANCED, bPropagateAdvanced);

		bool bPropagateTexture = mtlPropagationFlags & MTL_PROPAGATE_TEXTURES;
		ar(bPropagateTexture, "bPropagateTexture", "Propagate Texture Maps");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_TEXTURES, bPropagateTexture);

		bool bPropagateShaderParams = mtlPropagationFlags & MTL_PROPAGATE_SHADER_PARAMS;
		ar(bPropagateShaderParams, "bPropagateShaderParams", "Propagate Shader Params");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_SHADER_PARAMS, bPropagateShaderParams);

		bool bPropagateShaderGenParams = mtlPropagationFlags & MTL_PROPAGATE_SHADER_GEN;
		ar(bPropagateShaderGenParams, "bPropagateShaderGenParams", "Propagate Shader Generation");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_SHADER_GEN, bPropagateShaderGenParams);

		bool bPropagateVertexDef = mtlPropagationFlags & MTL_PROPAGATE_VERTEX_DEF;
		ar(bPropagateVertexDef, "bPropagateVertexDef", "Propagate Vertex Deformation");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_VERTEX_DEF, bPropagateVertexDef);

		bool bPropagateLayerPresets = mtlPropagationFlags & MTL_PROPAGATE_LAYER_PRESETS;
		ar(bPropagateLayerPresets, "bPropagateLayerPresets", "Propagate Layer Presets");
		SetFlag(mtlPropagationFlags, MTL_PROPAGATE_LAYER_PRESETS, bPropagateLayerPresets);

		pMtl->SetPropagationFlags(mtlPropagationFlags);

		ar.closeBlock();
	}*/

	if (ar.isInput())//writing to material
	{
		pMtl->SetFlags(mtlFlags);

		pMtl->Update();
		pMtl->UpdateMaterialLayers();

		pMtl->NotifyPropertiesUpdated();

		m_bIsBeingChanged = false;
	}
}

void CMaterialSerializer::SerializeTextureSlots(Serialization::IArchive& ar, bool& bHasDetailDecalOut)
{
	IShader* shader = m_pMaterial->GetShaderItem().m_pShader;
	if (!shader)
		return;

	const int nTech = max(0, m_pMaterial->GetShaderItem().m_nTechnique);
	const SShaderTexSlots* pShaderSlots = shader->GetUsedTextureSlots(nTech);

	const auto& materialHelpers = GetIEditor()->Get3DEngine()->GetMaterialHelpers();

	SInputShaderResources& shaderResources = m_pMaterial->GetShaderResources();


	ar.openBlock("textureMaps", "Texture Maps");

	for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
	{
		if (!materialHelpers.IsAdjustableTexSlot(texId))
			continue;

		const SShaderTextureSlot* pSlot = pShaderSlots ? pShaderSlots->m_UsedSlots[texId] : nullptr;
		if (pShaderSlots && !pSlot) //texture slot is not used for this shader
			continue;

		//Note: m_Textures is not cleaned up when changing shaders, this could potentially lead to having incorrect data in the material?
		auto& shaderTexture = shaderResources.m_Textures[texId];

		{
			//Get name from the texture slot or default name, TODO: cache these
			//Note: "Bumpmap" slot is actually "normal map" but because of backwards compatibility madness we are leaving it "bumpmap" in the ui...
			//See MaterialHelpers.cpp (Cry3DEngine)
			const string& textureName = pSlot && pSlot->m_Name.length() ? pSlot->m_Name : materialHelpers.LookupTexName(texId);
			if (textureName == "Bumpmap")
				m_textureSlotLabels[texId] = "-Normal";
			else
			{
				m_textureSlotLabels[texId].Format("-%s", textureName.c_str());

				//The following characters have special meaning in yasli
				m_textureSlotLabels[texId].replace("[", "(");
				m_textureSlotLabels[texId].replace("]", ")");
			}
				

			if (!ar.openBlock(m_textureSlotLabels[texId].c_str(), m_textureSlotLabels[texId].c_str()))
				continue;

			string textureFilePath = shaderTexture.m_Name.c_str();

			//New material editor only deals with dds files, not tifs
			if (!textureFilePath.IsEmpty() && stricmp(PathUtil::GetExt(textureFilePath), "dds") != 0)
				textureFilePath = PathUtil::ReplaceExtension(textureFilePath, "dds").c_str();

			ar(Serialization::TextureFilename(textureFilePath), "textureName", "^Texture Name");
			if (pSlot)
				ar.doc(pSlot->m_Description);

			//Old material editor was destroying UI resource before changing texture, 
			//though this seems dangerous without any refcount as the resource may be used by something else
			//Not to mention this is not the responsibility of the material editor but rather the UI system
			//TODO: Confirm this can be left out
			/*
			//Unload previous dynamic texture/UIElement instance
			if (IsFlashUIFile(sr.m_Textures[tex].m_Name))
			{
				DestroyTexOfFlashFile(sr.m_Textures[tex].m_Name);
			}
			*/
			shaderTexture.SetName(textureFilePath.c_str());
		}


		{
			string textureType = materialHelpers.GetNameFromTextureType(shaderTexture.m_Sampler.m_eTexType);
			Serialization::StringListValue textureTypeVal(m_textureTypeStringList, textureType);
			const auto oldIndex = textureTypeVal.index();

			ar(textureTypeVal, "texType", "Texture Type");

			if(textureTypeVal.index() != oldIndex)
				shaderTexture.m_Sampler.m_eTexType = materialHelpers.GetTextureTypeFromName(textureTypeVal.c_str());
		}

		{
			EFilterPreset filter = (EFilterPreset)shaderTexture.m_Filter;
			ar(filter, "filter", "Filter");
			shaderTexture.m_Filter = (signed char)filter;
		}

		//In old editor AddModificator() was only called when modifying the texture. In practice this means we could call this all the time but
		//imitating old behavior for now.
		//TODO: understand what this does and if fields that necessitate pTexModifier can be hidden if not present
		//Note that values used to be zeroed when modificator not present but the editor always made it present
		SEfTexModificator* pTexModifier = nullptr;
		/*if (ar.isOutput())
			pTexModifier = shaderTexture.GetModificator();
		else if (ar.isInput())*/
			pTexModifier = shaderTexture.AddModificator();

		if (pTexModifier)
		{
			ar(pTexModifier->m_bTexGenProjected, "bIsProjectedTexGen", "Is Projected Tex Gen");

			ETexGenType texGenType = (ETexGenType)pTexModifier->m_eTGType;
			ar(texGenType, "texGenType", "Texture Gen Type");
			pTexModifier->m_eTGType = uint8(texGenType);
		}

		if (texId == EFTT_DECAL_OVERLAY)
		{
			const float fUint8RatioToFloat = 100.0f / 255.0f;

			//TODO: In old editor this is obviously buggy as it is set both from shader resource and from mtl flag, however it only sets the material flag
			//Confirm with rendering engineers how this should work
			bool bIsDetailDecal = (shaderResources.m_ResFlags & MTL_FLAG_DETAIL_DECAL);

			bHasDetailDecalOut = true;

			ar.openBlock("detailDecal", "Detail Decal");
			
			//ar(bIsDetailDecal, "bIsDetailDecal", "^^Detail Decal");

			//if (bIsDetailDecal)
			{
				float opacity = (float)shaderResources.m_DetailDecalInfo.nBlending * fUint8RatioToFloat;
				ar(opacity, "opacity", "Opacity");
				shaderResources.m_DetailDecalInfo.nBlending = (uint8)int_round(opacity / fUint8RatioToFloat);

				float ssao = (float)shaderResources.m_DetailDecalInfo.nSSAOAmount * fUint8RatioToFloat;
				ar(ssao, "ssao", "SSAO Amount");
				shaderResources.m_DetailDecalInfo.nSSAOAmount = (uint8)int_round(ssao / fUint8RatioToFloat);

				if (ar.openBlock("top", "-Top Layer"))
				{
					Vec2 tile(shaderResources.m_DetailDecalInfo.vTileOffs[0].x, shaderResources.m_DetailDecalInfo.vTileOffs[0].y);
					ar(tile, "tile", "Tile");

					Vec2 offset(shaderResources.m_DetailDecalInfo.vTileOffs[0].z, shaderResources.m_DetailDecalInfo.vTileOffs[0].w);
					ar(offset, "offset", "Offset");

					shaderResources.m_DetailDecalInfo.vTileOffs[0] = Vec4(tile.x, tile.y, offset.x, offset.y);

					SerializeWordToDegree(ar, shaderResources.m_DetailDecalInfo.nRotation[0], "rotation", "Rotation");

					float deformation = (float)shaderResources.m_DetailDecalInfo.nDeformation[0] * fUint8RatioToFloat;
					ar(deformation, "deformation", "Deformation");
					shaderResources.m_DetailDecalInfo.nDeformation[0] = (uint8)int_round(deformation / fUint8RatioToFloat);

					float sortingOffset = (float)shaderResources.m_DetailDecalInfo.nThreshold[0] * fUint8RatioToFloat;
					ar(sortingOffset, "sortingOffset", "Sorting Offset");
					shaderResources.m_DetailDecalInfo.nThreshold[0] = (uint8)int_round(sortingOffset / fUint8RatioToFloat);

					ar.closeBlock();
				}

				if (ar.openBlock("bottom", "-Bottom Layer"))
				{
					Vec2 tile(shaderResources.m_DetailDecalInfo.vTileOffs[1].x, shaderResources.m_DetailDecalInfo.vTileOffs[1].y);
					ar(tile, "tile", "Tile");

					Vec2 offset(shaderResources.m_DetailDecalInfo.vTileOffs[1].z, shaderResources.m_DetailDecalInfo.vTileOffs[1].w);
					ar(offset, "offset", "Offset");

					shaderResources.m_DetailDecalInfo.vTileOffs[1] = Vec4(tile.x, tile.y, offset.x, offset.y);

					SerializeWordToDegree(ar, shaderResources.m_DetailDecalInfo.nRotation[1], "rotation", "Rotation");

					float deformation = (float)shaderResources.m_DetailDecalInfo.nDeformation[1] * fUint8RatioToFloat;
					ar(deformation, "deformation", "Deformation");
					shaderResources.m_DetailDecalInfo.nDeformation[1] = (uint8)int_round(deformation / fUint8RatioToFloat);

					float sortingOffset = (float)shaderResources.m_DetailDecalInfo.nThreshold[1] * fUint8RatioToFloat;
					ar(sortingOffset, "sortingOffset", "Sorting Offset");
					shaderResources.m_DetailDecalInfo.nThreshold[1] = (uint8)int_round(sortingOffset / fUint8RatioToFloat);

					ar.closeBlock();
				}

				ar.closeBlock();
			}
		}

		if (pTexModifier)
		{
			if (ar.openBlock("tiling", "-Tiling"))
			{
				ar.openBlock("isTilingUV", "Is Tiling UV");
				ar(shaderTexture.m_bUTile, "isTilingU", "^Is Tiling U");

				ar(shaderTexture.m_bVTile, "isTilingV", "^Is Tiling V");
				ar.closeBlock();

				Vec2 tilingUV(shaderTexture.GetTiling(0), shaderTexture.GetTiling(1));
				ar(tilingUV, "tilingUV", "Tiling UV");
				pTexModifier->m_Tiling[0] = tilingUV.x;
				pTexModifier->m_Tiling[1] = tilingUV.y;

				Vec2 offsetUV(shaderTexture.GetOffset(0), shaderTexture.GetOffset(1));
				ar(offsetUV, "offsetUV", "Offset UV");
				pTexModifier->m_Offs[0] = offsetUV.x;
				pTexModifier->m_Offs[1] = offsetUV.y;

				//Using inline controls to imitate vector

				ar.openBlock("rotationUVW", "Rotation UVW");

				SerializeWordToDegree(ar, pTexModifier->m_Rot[0], "rotateU", "^Rotation U");
				SerializeWordToDegree(ar, pTexModifier->m_Rot[1], "rotateV", "^Rotation V");
				SerializeWordToDegree(ar, pTexModifier->m_Rot[2], "rotateW", "^Rotation W");

				ar.closeBlock();

				ar.closeBlock();
			}
		
			if (ar.openBlock("rotator", "-Rotator"))
			{
				ETexModRotateType type = (ETexModRotateType)pTexModifier->m_eRotType;
				ar(type, "type", "Type");
				pTexModifier->m_eRotType = (uint8)type;

				SerializeWordToDegree(ar, pTexModifier->m_RotOscRate[2], "rate", "Rate");
				SerializeWordToDegree(ar, pTexModifier->m_RotOscPhase[2], "phase", "Phase");
				SerializeWordToDegree(ar, pTexModifier->m_RotOscAmplitude[2], "amplitude", "Amplitude");

				Vec2 centerUV(pTexModifier->m_RotOscCenter[0], pTexModifier->m_RotOscCenter[1]);
				ar(centerUV, "centerUV", "Center UV");

				pTexModifier->m_RotOscCenter[0] = centerUV.x;
				pTexModifier->m_RotOscCenter[1] = centerUV.y;
				pTexModifier->m_RotOscCenter[2] = 0.0f;

				ar.closeBlock();
			}

			if (ar.openBlock("oscillator", "-Oscillator"))
			{
				ar.openBlock("typeUV", "Type UV");

				ETexModMoveType typeU = (ETexModMoveType)pTexModifier->m_eMoveType[0];
				ar(typeU, "typeU", "^Type U");
				pTexModifier->m_eMoveType[0] = (uint8)typeU;

				ETexModMoveType typeV = (ETexModMoveType)pTexModifier->m_eMoveType[1];
				ar(typeV, "typeV", "^Type V");
				pTexModifier->m_eMoveType[1] = (uint8)typeV;

				ar.closeBlock();

				Vec2 rateUV(pTexModifier->m_OscRate[0], pTexModifier->m_OscRate[1]);
				ar(rateUV, "rateUV", "Rate UV");
				pTexModifier->m_OscRate[0] = rateUV.x;
				pTexModifier->m_OscRate[1] = rateUV.y;

				Vec2 phaseUV(pTexModifier->m_OscPhase[0], pTexModifier->m_OscPhase[1]);
				ar(phaseUV, "phaseUV", "Phase UV");
				pTexModifier->m_OscPhase[0] = phaseUV.x;
				pTexModifier->m_OscPhase[1] = phaseUV.y;

				Vec2 amplitudeUV(pTexModifier->m_OscAmplitude[0], pTexModifier->m_OscAmplitude[1]);
				ar(amplitudeUV, "amplitudeUV", "Amplitude UV");
				pTexModifier->m_OscAmplitude[0] = amplitudeUV.x;
				pTexModifier->m_OscAmplitude[1] = amplitudeUV.y;

				ar.closeBlock();
			}

		}

		ar.closeBlock();
	}

	ar.closeBlock();
}

CMaterialSerializer::SShaderParamInfo CMaterialSerializer::ParseShaderParamScript(const SShaderParam& param)
{
	SShaderParamInfo info;

	if (param.m_Script.size())
	{
		string element[3];
		int p1 = 0;
		string itemToken = param.m_Script.Tokenize(";", p1);
		while (!itemToken.empty())
		{
			int nElements = 0;
			int p2 = 0;
			string token = itemToken.Tokenize(" \t\r\n=", p2);
			while (!token.empty())
			{
				element[nElements++] = token;
				if (nElements == 2)
				{
					element[nElements] = itemToken.substr(p2);
					element[nElements].Trim(" =\t\"");
					break;
				}
				token = itemToken.Tokenize(" \t\r\n=", p2);
			}

			//UI widget is not handled as it is based on serializable type
			/*if (stricmp(element[1], "UIWidget") == 0)
			{
				if (stricmp(element[2], "Color") == 0)
				if (stricmp(element[2], "Slider") == 0)
			}*/

			if (stricmp(element[1], "UIHelp") == 0)
			{
				info.m_description = element[2];
				info.m_description.replace("\\n", "\n");
			}
			else if (stricmp(element[1], "UIName") == 0)
			{
				info.m_uiName = element[2];
			}
			else if (stricmp(element[1], "UIMin") == 0)
			{
				info.m_min = atof(element[2]);
				info.bHasMin = true;
			}
			else if (stricmp(element[1], "UIMax") == 0)
			{
				info.m_max = atof(element[2]);
				info.bHasMax = true;
			}
			else if (stricmp(element[1], "UIStep") == 0)
			{
				info.m_step = atof(element[2]);
				info.bHasStep = true;
			}

			itemToken = param.m_Script.Tokenize(";", p1);
		}
	}

	if(info.m_uiName.size() == 0)
		info.m_uiName = param.m_Name;

	//fix step for integer values
	if (!info.bHasStep && param.m_Type != eType_FLOAT)
		info.m_step = 1; 

	return info;
}

template<typename T>
void SerializeShaderParam(Serialization::IArchive& ar, T& value, const char* name, const CMaterialSerializer::SShaderParamInfo& paramInfo)
{
	
	if (paramInfo.bHasMin || paramInfo.bHasMax)
	{
		ar(Serialization::Decorators::Range<T>(value, (T)paramInfo.m_min, (T)paramInfo.m_max, (T)paramInfo.m_step), name, paramInfo.m_uiName.c_str());
	}
	else
	{
		ar(value, name, paramInfo.m_uiName);
	}

	if (paramInfo.m_description)
		ar.doc(paramInfo.m_description.c_str());
}

void CMaterialSerializer::SerializeShaderParams(Serialization::IArchive& ar, bool bShaderChanged)
{
	auto& shaderParams = m_pMaterial->GetShaderResources().m_ShaderParams;
	if (shaderParams.empty())
		return;

	if (ar.isInput() && bShaderChanged)
	{
		//Shader params must be set with the new shader loaded
		m_pMaterial->Update();
		m_shaderParamInfoCache.clear();
	}

	bool bParseScript = m_shaderParamInfoCache.size() == 0;
	bool bShaderParamsChanged = false;

	ar.openBlock("shaderParams", "Shader Params");

	const int count = shaderParams.size();
	for (int i = 0; i < count; i++)
	{
		SShaderParam& param = shaderParams[i];

		if(bParseScript)
			m_shaderParamInfoCache[param.m_Name] = ParseShaderParamScript(param);

		const auto& info = m_shaderParamInfoCache[param.m_Name];

		switch (param.m_Type)
		{
		case eType_BYTE:
		{
			int value = (int)param.m_Value.m_Byte;
			SerializeShaderParam(ar, value, param.m_Name, info);
			if (value != (int)param.m_Value.m_Byte)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Byte = value;
			}
			break;
		}
		case eType_SHORT:
		{
			int value = (int)param.m_Value.m_Short;
			SerializeShaderParam(ar, value, param.m_Name, info);
			if (value != (int)param.m_Value.m_Short)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Short = value;
			}
			break;
		}
		case eType_INT:
		{
			int value = param.m_Value.m_Int;
			SerializeShaderParam(ar, value, param.m_Name, info);
			if (value != param.m_Value.m_Int)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Int = value;
			}
			break;
		}
		case eType_FLOAT:
		{
			float value = param.m_Value.m_Float;
			SerializeShaderParam(ar, value, param.m_Name, info);
			if (value != param.m_Value.m_Float)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Float = value;
			}
			break;
		}
		case eType_FCOLOR:
		{
			ColorF color(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2], param.m_Value.m_Color[3]);
			const ColorF oldColor = color;
			ar(color, param.m_Name, info.m_uiName);
			if (info.m_description.size())
				ar.doc(info.m_description.c_str());
			if (color != oldColor)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Color[0] = color.r;
				param.m_Value.m_Color[1] = color.g;
				param.m_Value.m_Color[2] = color.b;
				param.m_Value.m_Color[3] = color.a;
			}

			break;
		}
		case eType_VECTOR:
		{
			Vec3 vec(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
			const Vec3 oldVec = vec;
			ar(vec, param.m_Name, info.m_uiName);
			if (info.m_description.size())
				ar.doc(info.m_description.c_str());
			if (vec != oldVec)
			{
				bShaderParamsChanged = true;
				param.m_Value.m_Vector[0] = vec.x;
				param.m_Value.m_Vector[1] = vec.y;
				param.m_Value.m_Vector[2] = vec.z;
			}
			break;
		}
		default:
			//Apparently unknown type parameters are a frequent occurrence, do not warn
			//CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unknown shader parameter type found in material %s: %s", m_pMaterial->GetName().c_str(), param.m_Name);
			break;
		}
	}

	if (ar.isInput() && bShaderParamsChanged)
	{
		if (m_pMaterial->GetShaderItem().m_pShaderResources)
			m_pMaterial->GetShaderItem().m_pShaderResources->SetShaderParams(&m_pMaterial->GetShaderResources(), m_pMaterial->GetShaderItem().m_pShader);
	}

	ar.closeBlock();
}

void CMaterialSerializer::SerializeShaderGenParams(Serialization::IArchive& ar)
{
	uint64 shaderGenMask = m_pMaterial->GetShaderGenMask();
	IShader* shader = m_pMaterial->GetShaderItem().m_pShader;
	if (!shader)
		return;

	const SShaderGen* shaderGen = shader->GetGenerationParams();
	if (!shaderGen)
		return;

	const int count = shaderGen->m_BitMask.size();
	if (count <= 0)
		return;
	
	ar.openBlock("shaderGenParams", "Shader Generation Params");

	for (int i = 0; i < count; i++)
	{
		const SShaderGenBit* pGenBit = shaderGen->m_BitMask[i];
		if (pGenBit->m_Flags & SHGF_HIDDEN)
			continue;

		if (!pGenBit->m_ParamProp.empty())
		{
			bool flag = (pGenBit->m_Mask & shaderGenMask) != 0;
			ar(flag, pGenBit->m_ParamProp.c_str(), pGenBit->m_ParamProp.c_str());
			ar.doc(pGenBit->m_ParamDesc.c_str());

			SetFlag(shaderGenMask, pGenBit->m_Mask, flag);
		}
	}

	ar.closeBlock();

	m_pMaterial->SetShaderGenMask(shaderGenMask);
}

QWidget* CMaterialSerializer::CreatePropertyTree()
{
	return new CMaterialSerializer::CMaterialPropertyTree(this);
}

void CMaterialSerializer::PopulateStringLists()
{
	//////////////////////////////////////////////////////////////////////////
	//shaders
	m_shaderStringList.clear();

	const auto& shaderList = GetIEditor()->GetMaterialManager()->GetShaderList();
	m_shaderStringList.reserve(shaderList.size());
	
	for (const auto& shader : shaderList)
	{
		if (strstri(shader,"_Overlay") != 0)
			continue;
		m_shaderStringList.push_back(shader);
	}
	//Note : shader list is already sorted in material manager

	m_shaderStringListWithEmpty = m_shaderStringList;
	m_shaderStringListWithEmpty.insert(0, "");

	//////////////////////////////////////////////////////////////////////////
	//surface types
	m_surfaceTypeStringList.clear();

	m_surfaceTypeStringList.push_back("None"); //Empty surface type

	ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
	if (pSurfaceTypeEnum)
	{
		for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
		{
			const char* name = pSurfaceType->GetName();
			if (strstr(name, "mat") == name && strlen(name) > 4)
				m_surfaceTypeStringList.push_back(name + 4);
			else
				m_surfaceTypeStringList.push_back(name);
		}

	}

	std::sort(m_surfaceTypeStringList.begin(), m_surfaceTypeStringList.end());


	//////////////////////////////////////////////////////////////////////////
	// Texture Type
	m_textureTypeStringList.clear();
	m_textureTypeStringList.push_back(GetIEditor()->Get3DEngine()->GetMaterialHelpers().GetNameFromTextureType(eTT_2D));
	m_textureTypeStringList.push_back(GetIEditor()->Get3DEngine()->GetMaterialHelpers().GetNameFromTextureType(eTT_Cube));
	m_textureTypeStringList.push_back(GetIEditor()->Get3DEngine()->GetMaterialHelpers().GetNameFromTextureType(eTT_NearestCube));
	m_textureTypeStringList.push_back(GetIEditor()->Get3DEngine()->GetMaterialHelpers().GetNameFromTextureType(eTT_Dyn2D));
	m_textureTypeStringList.push_back(GetIEditor()->Get3DEngine()->GetMaterialHelpers().GetNameFromTextureType(eTT_User));
}

