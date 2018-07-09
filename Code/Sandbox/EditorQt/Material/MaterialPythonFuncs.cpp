// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Material/MaterialManager.h"
#include "Material/MaterialHelpers.h"
#include "Util/BoostPythonHelpers.h"
#include "Util/MFCUtil.h"

namespace
{
void PyMaterialCreate()
{
	GetIEditorImpl()->GetMaterialManager()->Command_Create();
}

void PyMaterialCreateMulti()
{
	GetIEditorImpl()->GetMaterialManager()->Command_CreateMulti();
}

void PyMaterialConvertToMulti()
{
	GetIEditorImpl()->GetMaterialManager()->Command_ConvertToMulti();
}

void PyMaterialDuplicateCurrent()
{
	GetIEditorImpl()->GetMaterialManager()->Command_Duplicate();
}

void PyMaterialMergeSelection()
{
	GetIEditorImpl()->GetMaterialManager()->Command_Merge();
}

void PyMaterialDeleteCurrent()
{
	GetIEditorImpl()->GetMaterialManager()->Command_Delete();
}

void PyCreateTerrainLayer()
{
	GetIEditorImpl()->GetMaterialManager()->Command_CreateTerrainLayer();
}

void PyMaterialAssignCurrentToSelection()
{
	CUndo undo("Assign Material To Selection");
	GetIEditorImpl()->GetMaterialManager()->Command_AssignToSelection();
}

void PyMaterialResetSelection()
{
	GetIEditorImpl()->GetMaterialManager()->Command_ResetSelection();
}

void PyMaterialSelectObjectsWithCurrent()
{
	CUndo undo("Select Objects With Current Material");
	GetIEditorImpl()->GetMaterialManager()->Command_SelectAssignedObjects();
}

void PyMaterialSetCurrentFromObject()
{
	GetIEditorImpl()->GetMaterialManager()->Command_SelectFromObject();
}

std::vector<std::string> PyGetSubMaterial(const char* pMaterialPath)
{
	string materialPath = pMaterialPath;
	CMaterial* pMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(pMaterialPath, false);
	if (!pMaterial)
	{
		throw std::runtime_error("Invalid multi material.");
	}

	std::vector<std::string> result;
	for (int i = 0; i < pMaterial->GetSubMaterialCount(); i++)
	{
		if (pMaterial->GetSubMaterial(i))
		{
			result.push_back(static_cast<std::string>(materialPath + "\\" + pMaterial->GetSubMaterial(i)->GetName()));
		}
	}
	return result;
}

CMaterial* TryLoadingMaterial(const char* pPathAndMaterialName)
{
	CString varMaterialPath = pPathAndMaterialName;
	std::deque<string> splittedMaterialPath;
	int currentPos = 0;
	for (string token = varMaterialPath.Tokenize("/\\", currentPos); !token.IsEmpty(); token = varMaterialPath.Tokenize("/\\", currentPos))
	{
		splittedMaterialPath.push_back(token);
	}

	CMaterial* pMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(varMaterialPath, false);
	if (!pMaterial)
	{
		string subMaterialName = splittedMaterialPath.back();
		bool isSubMaterialExist(false);

		varMaterialPath.Delete((varMaterialPath.GetLength() - subMaterialName.GetLength() - 1), varMaterialPath.GetLength());
		string test = varMaterialPath;
		pMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(varMaterialPath, false);

		if (!pMaterial)
		{
			throw std::runtime_error("Invalid multi material.");
		}

		for (int i = 0; i < pMaterial->GetSubMaterialCount(); i++)
		{
			if (pMaterial->GetSubMaterial(i)->GetName() == subMaterialName)
			{
				pMaterial = pMaterial->GetSubMaterial(i);
				isSubMaterialExist = true;
			}
		}

		if (!pMaterial || !isSubMaterialExist)
		{
			throw std::runtime_error(string("\"") + subMaterialName + "\" is an invalid sub material.");
		}
	}
	GetIEditorImpl()->GetMaterialManager()->SetCurrentMaterial(pMaterial);
	return pMaterial;
}

std::deque<string> PreparePropertyPath(const char* pPathAndPropertyName)
{
	string varPathProperty = pPathAndPropertyName;
	std::deque<string> splittedPropertyPath;
	int currentPos = 0;
	for (string token = varPathProperty.Tokenize("/\\", currentPos); !token.IsEmpty(); token = varPathProperty.Tokenize("/\\", currentPos))
	{
		splittedPropertyPath.push_back(token);
	}

	return splittedPropertyPath;
}

//////////////////////////////////////////////////////////////////////////
// Converter: Enum -> string (human readable)
//////////////////////////////////////////////////////////////////////////

string TryConvertingSEFResTextureToCString(SEfResTexture* pResTexture)
{
	if (pResTexture)
	{
		return pResTexture->m_Name.c_str();
	}
	return "";
}

string TryConvertingETEX_TypeToCString(const uint8& texTypeName)
{
	if (MaterialHelpers::IsTextureTypeExposed(texTypeName))
	{
		return MaterialHelpers::GetNameFromTextureType(texTypeName);
	}
	throw std::runtime_error("Invalid tex type.");
}

string TryConvertingTexFilterToCString(const int& iFilterName)
{
	switch (iFilterName)
	{
	case FILTER_NONE:
		return "Default";
	case FILTER_POINT:
		return "Point";
	case FILTER_LINEAR:
		return "Linear";
	case FILTER_BILINEAR:
		return "Bi-linear";
	case FILTER_TRILINEAR:
		return "Tri-linear";
	case FILTER_ANISO2X:
		return "Anisotropic 2x";
	case FILTER_ANISO4X:
		return "Anisotropic 4x";
	case FILTER_ANISO8X:
		return "Anisotropic 8x";
	case FILTER_ANISO16X:
		return "Anisotropic 16x";
	default:
		throw std::runtime_error("Invalid tex filter.");
	}
}

string TryConvertingETexGenTypeToCString(const uint8& texGenType)
{
	switch (texGenType)
	{
	case ETG_Stream:
		return "Stream";
	case ETG_World:
		return "World";
	case ETG_Camera:
		return "Camera";
	default:
		throw std::runtime_error("Invalid tex gen type.");
	}
}

string TryConvertingETexModRotateTypeToCString(const uint8& rotateType)
{
	switch (rotateType)
	{
	case ETMR_NoChange:
		return "No Change";
	case ETMR_Fixed:
		return "Fixed Rotation";
	case ETMR_Constant:
		return "Constant Rotation";
	case ETMR_Oscillated:
		return "Oscillated Rotation";
	default:
		throw std::runtime_error("Invalid rotate type.");
	}
}

string TryConvertingETexModMoveTypeToCString(const uint8& oscillatorType)
{
	switch (oscillatorType)
	{
	case ETMM_NoChange:
		return "No Change";
	case ETMM_Fixed:
		return "Fixed Moving";
	case ETMM_Constant:
		return "Constant Moving";
	case ETMM_Jitter:
		return "Jitter Moving";
	case ETMM_Pan:
		return "Pan Moving";
	case ETMM_Stretch:
		return "Stretch Moving";
	case ETMM_StretchRepeat:
		return "Stretch-Repeat Moving";
	default:
		throw std::runtime_error("Invalid oscillator type.");
	}
}

string TryConvertingEDeformTypeToCString(const EDeformType& deformType)
{
	switch (deformType)
	{
	case eDT_Unknown:
		return "None";
	case eDT_SinWave:
		return "Sin Wave";
	case eDT_SinWaveUsingVtxColor:
		return "Sin Wave using vertex color";
	case eDT_Bulge:
		return "Bulge";
	case eDT_Squeeze:
		return "Squeeze";
	case eDT_Perlin2D:
		return "Perlin 2D";
	case eDT_Perlin3D:
		return "Perlin 3D";
	case eDT_FromCenter:
		return "From Center";
	case eDT_Bending:
		return "Bending";
	case eDT_ProcFlare:
		return "Proc. Flare";
	case eDT_AutoSprite:
		return "Auto sprite";
	case eDT_Beam:
		return "Beam";
	case eDT_FixedOffset:
		return "FixedOffset";
	default:
		throw std::runtime_error("Invalid deform type.");
	}
}

string TryConvertingEWaveFormToCString(const EWaveForm& waveForm)
{
	switch (waveForm)
	{
	case eWF_None:
		return "None";
	case eWF_Sin:
		return "Sin";
	case eWF_HalfSin:
		return "Half Sin";
	case eWF_Square:
		return "Square";
	case eWF_Triangle:
		return "Triangle";
	case eWF_SawTooth:
		return "Saw Tooth";
	case eWF_InvSawTooth:
		return "Inverse Saw Tooth";
	case eWF_Hill:
		return "Hill";
	case eWF_InvHill:
		return "Inverse Hill";
	default:
		throw std::runtime_error("Invalid wave form.");
	}
}

//////////////////////////////////////////////////////////////////////////
// Converter: string -> Enum
//////////////////////////////////////////////////////////////////////////

EEfResTextures TryConvertingCStringToEEfResTextures(const string& resTextureName)
{
	if (resTextureName == "Diffuse")
	{
		return EFTT_DIFFUSE;
	}
	else if (resTextureName == "Specular")
	{
		return EFTT_SPECULAR;
	}
	else if (resTextureName == "Bumpmap")
	{
		return EFTT_NORMALS;
	}
	else if (resTextureName == "Heightmap")
	{
		return EFTT_HEIGHT;
	}
	else if (resTextureName == "Environment")
	{
		return EFTT_ENV;
	}
	else if (resTextureName == "Detail")
	{
		return EFTT_DETAIL_OVERLAY;
	}
	else if (resTextureName == "Opacity")
	{
		return EFTT_OPACITY;
	}
	else if (resTextureName == "Decal")
	{
		return EFTT_DECAL_OVERLAY;
	}
	else if (resTextureName == "SubSurface")
	{
		return EFTT_SUBSURFACE;
	}
	else if (resTextureName == "Custom")
	{
		return EFTT_CUSTOM;
	}
	else if (resTextureName == "[1] Custom")
	{
		return EFTT_DIFFUSE;
	}
	else if (resTextureName == "Emittance")
	{
		return EFTT_EMITTANCE;
	}
	throw std::runtime_error("Invalid texture name.");
}

ETEX_Type TryConvertingCStringToETEX_Type(const string& texTypeName)
{
	ETEX_Type texType = (ETEX_Type)MaterialHelpers::GetTextureTypeFromName(texTypeName);
	if (texType == eTT_MaxTexType || !MaterialHelpers::IsTextureTypeExposed(texType))
	{
		throw std::runtime_error("Invalid tex type name.");
	}
	return texType;
}

signed char TryConvertingCStringToTexFilter(string filterName)
{
	if (filterName == "Default")
	{
		return FILTER_NONE;
	}
	else if (filterName == "Point")
	{
		return FILTER_POINT;
	}
	else if (filterName == "Linear")
	{
		return FILTER_LINEAR;
	}
	else if (filterName == "Bi-linear")
	{
		return FILTER_BILINEAR;
	}
	else if (filterName == "Tri-linear")
	{
		return FILTER_TRILINEAR;
	}
	else if (filterName == "Anisotropic 2x")
	{
		return FILTER_ANISO2X;
	}
	else if (filterName == "Anisotropic 4x")
	{
		return FILTER_ANISO4X;
	}
	else if (filterName == "Anisotropic 8x")
	{
		return FILTER_ANISO8X;
	}
	else if (filterName == "Anisotropic 16x")
	{
		return FILTER_ANISO16X;
	}
	throw std::runtime_error("Invalid filter name.");
}

ETexGenType TryConvertingCStringToETexGenType(const string& texGenType)
{
	if (texGenType == "Stream")
	{
		return ETG_Stream;
	}
	else if (texGenType == "World")
	{
		return ETG_World;
	}
	else if (texGenType == "Camera")
	{
		return ETG_Camera;
	}
	throw std::runtime_error("Invalid tex gen type name.");
}

ETexModRotateType TryConvertingCStringToETexModRotateType(const string& rotateType)
{
	if (rotateType == "No Change")
	{
		return ETMR_NoChange;
	}
	else if (rotateType == "Fixed Rotation")
	{
		return ETMR_Fixed;
	}
	else if (rotateType == "Constant Rotation")
	{
		return ETMR_Constant;
	}
	else if (rotateType == "Oscillated Rotation")
	{
		return ETMR_Oscillated;
	}
	throw std::runtime_error("Invalid rotate type name.");
}

ETexModMoveType TryConvertingCStringToETexModMoveType(const string& oscillatorType)
{
	if (oscillatorType == "No Change")
	{
		return ETMM_NoChange;
	}
	else if (oscillatorType == "Fixed Moving")
	{
		return ETMM_Fixed;
	}
	else if (oscillatorType == "Constant Moving")
	{
		return ETMM_Constant;
	}
	else if (oscillatorType == "Jitter Moving")
	{
		return ETMM_Jitter;
	}
	else if (oscillatorType == "Pan Moving")
	{
		return ETMM_Pan;
	}
	else if (oscillatorType == "Stretch Moving")
	{
		return ETMM_Stretch;
	}
	else if (oscillatorType == "Stretch-Repeat Moving")
	{
		return ETMM_StretchRepeat;
	}
	throw std::runtime_error("Invalid oscillator type.");
}

EDeformType TryConvertingCStringToEDeformType(const string& deformType)
{
	if (deformType == "None")
	{
		return eDT_Unknown;
	}
	else if (deformType == "Sin Wave")
	{
		return eDT_SinWave;
	}
	else if (deformType == "Sin Wave using vertex color")
	{
		return eDT_SinWaveUsingVtxColor;
	}
	else if (deformType == "Bulge")
	{
		return eDT_Bulge;
	}
	else if (deformType == "Squeeze")
	{
		return eDT_Squeeze;
	}
	else if (deformType == "Perlin 2D")
	{
		return eDT_Perlin2D;
	}
	else if (deformType == "Perlin 3D")
	{
		return eDT_Perlin3D;
	}
	else if (deformType == "From Center")
	{
		return eDT_FromCenter;
	}
	else if (deformType == "Bending")
	{
		return eDT_Bending;
	}
	else if (deformType == "Proc. Flare")
	{
		return eDT_ProcFlare;
	}
	else if (deformType == "Auto sprite")
	{
		return eDT_AutoSprite;
	}
	else if (deformType == "Beam")
	{
		return eDT_Beam;
	}
	else if (deformType == "FixedOffset")
	{
		return eDT_FixedOffset;
	}
	throw std::runtime_error("Invalid deform type.");
}

EWaveForm TryConvertingCStringToEWaveForm(const string& waveForm)
{
	if (waveForm == "None")
	{
		return eWF_None;
	}
	else if (waveForm == "Sin")
	{
		return eWF_Sin;
	}
	else if (waveForm == "Half Sin")
	{
		return eWF_HalfSin;
	}
	else if (waveForm == "Square")
	{
		return eWF_Square;
	}
	else if (waveForm == "Triangle")
	{
		return eWF_Triangle;
	}
	else if (waveForm == "Saw Tooth")
	{
		return eWF_SawTooth;
	}
	else if (waveForm == "Inverse Saw Tooth")
	{
		return eWF_InvSawTooth;
	}
	else if (waveForm == "Hill")
	{
		return eWF_Hill;
	}
	else if (waveForm == "Inverse Hill")
	{
		return eWF_InvHill;
	}
	throw std::runtime_error("Invalid wave form.");
}

//////////////////////////////////////////////////////////////////////////
// Script parser
//////////////////////////////////////////////////////////////////////////

string ParseUINameFromPublicParamsScript(const char* sUIScript)
{
	string uiscript = sUIScript;
	string element[3];
	int p1 = 0;
	string itemToken = uiscript.Tokenize(";", p1);
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

		if (stricmp(element[1], "UIName") == 0)
		{
			return element[2].c_str();
		}
		itemToken = uiscript.Tokenize(";", p1);
	}
	return "";
}

std::map<string, float> ParseValidRangeFromPublicParamsScript(const char* sUIScript)
{
	string uiscript = sUIScript;
	std::map<string, float> result;
	bool isUIMinExist(false);
	bool isUIMaxExist(false);
	string element[3];
	int p1 = 0;
	string itemToken = uiscript.Tokenize(";", p1);
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

		if (stricmp(element[1], "UIMin") == 0)
		{
			result["UIMin"] = atof(element[2]);
			isUIMinExist = true;
		}
		if (stricmp(element[1], "UIMax") == 0)
		{
			result["UIMax"] = atof(element[2]);
			isUIMaxExist = true;
		}
		itemToken = uiscript.Tokenize(";", p1);
	}
	if (!isUIMinExist || !isUIMaxExist)
	{
		throw std::runtime_error("Invalid range for shader param.");
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
// Set Flags
//////////////////////////////////////////////////////////////////////////

void SetMaterialFlag(CMaterial* pMaterial, const EMaterialFlags& eFlag, const bool& bFlag)
{
	if (pMaterial->GetFlags() & eFlag && bFlag == false)
	{
		pMaterial->SetFlags(pMaterial->GetFlags() - eFlag);
	}
	else if (!(pMaterial->GetFlags() & eFlag) && bFlag == true)
	{
		pMaterial->SetFlags(pMaterial->GetFlags() | eFlag);
	}
}

void SetPropagationFlag(CMaterial* pMaterial, const eMTL_PROPAGATION& eFlag, const bool& bFlag)
{
	if (pMaterial->GetPropagationFlags() & eFlag && bFlag == false)
	{
		pMaterial->SetPropagationFlags(pMaterial->GetPropagationFlags() - eFlag);
	}
	else if (!(pMaterial->GetPropagationFlags() & eFlag) && bFlag == true)
	{
		pMaterial->SetPropagationFlags(pMaterial->GetPropagationFlags() | eFlag);
	}
}

//////////////////////////////////////////////////////////////////////////

bool ValidateProperty(CMaterial* pMaterial, const std::deque<string>& splittedPropertyPathParam, const SPyWrappedProperty& value)
{
	std::deque<string> splittedPropertyPath = splittedPropertyPathParam;
	std::deque<string> splittedPropertyPathSubCategory = splittedPropertyPathParam;
	string categoryName = splittedPropertyPath.front();
	string subCategoryName = "";
	string subSubCategoryName = "";
	string currentPath = "";
	string propertyName = splittedPropertyPath.back();

	string errorMsgInvalidValue = string("Invalid value for property \"") + propertyName + "\"";
	string errorMsgInvalidDataType = string("Invalid data type for property \"") + propertyName + "\"";
	string errorMsgInvalidPropertyPath = string("Invalid property path");

	const int iMinColorValue = 0;
	const int iMaxColorValue = 255;

	if (splittedPropertyPathSubCategory.size() == 3)
	{
		splittedPropertyPathSubCategory.pop_back();
		subCategoryName = splittedPropertyPathSubCategory.back();
		currentPath = string("") + categoryName + "/" + subCategoryName;

		if (
		  subCategoryName != "TexType" &&
		  subCategoryName != "Filter" &&
		  subCategoryName != "IsProjectedTexGen" &&
		  subCategoryName != "TexGenType" &&
		  subCategoryName != "Wave X" &&
		  subCategoryName != "Wave Y" &&
		  subCategoryName != "Wave Z" &&
		  subCategoryName != "Wave W" &&
		  subCategoryName != "Shader1" &&
		  subCategoryName != "Shader2" &&
		  subCategoryName != "Shader3" &&
		  subCategoryName != "Tiling" &&
		  subCategoryName != "Rotator" &&
		  subCategoryName != "Oscillator" &&
		  subCategoryName != "Diffuse" &&
		  subCategoryName != "Specular" &&
		  subCategoryName != "Bumpmap" &&
		  subCategoryName != "Heightmap" &&
		  subCategoryName != "Environment" &&
		  subCategoryName != "Detail" &&
		  subCategoryName != "Opacity" &&
		  subCategoryName != "Decal" &&
		  subCategoryName != "SubSurface" &&
		  subCategoryName != "Custom" &&
		  subCategoryName != "[1] Custom"
		  )
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath + " (" + currentPath + ")");
		}
	}
	else if (splittedPropertyPathSubCategory.size() == 4)
	{
		splittedPropertyPathSubCategory.pop_back();
		subCategoryName = splittedPropertyPathSubCategory.back();
		splittedPropertyPathSubCategory.pop_back();
		subSubCategoryName = splittedPropertyPathSubCategory.back();
		currentPath = categoryName + "/" + subSubCategoryName + "/" + subCategoryName;

		if (
		  subSubCategoryName != "Diffuse" &&
		  subSubCategoryName != "Specular" &&
		  subSubCategoryName != "Bumpmap" &&
		  subSubCategoryName != "Heightmap" &&
		  subSubCategoryName != "Environment" &&
		  subSubCategoryName != "Detail" &&
		  subSubCategoryName != "Opacity" &&
		  subSubCategoryName != "Decal" &&
		  subSubCategoryName != "SubSurface" &&
		  subSubCategoryName != "Custom" &&
		  subSubCategoryName != "[1] Custom"
		  )
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath + " (" + currentPath + ")");
		}
		else if (subCategoryName != "Tiling" && subCategoryName != "Rotator" && subCategoryName != "Oscillator")
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath + " (" + currentPath + ")");
		}
	}
	else
	{
		currentPath = categoryName;
	}

	if (
	  categoryName == "Material Settings" ||
	  categoryName == "Opacity Settings" ||
	  categoryName == "Lighting Settings" ||
	  categoryName == "Advanced" ||
	  categoryName == "Texture Maps" ||
	  categoryName == "Vertex Deformation" ||
	  categoryName == "Layer Presets")
	{
		if (propertyName == "Opacity" || propertyName == "AlphaTest" || propertyName == "Glow Amount")
		{
			// int: 0 < x < 100
			if (value.type != SPyWrappedProperty::eType_Int)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.intValue < 0 || value.property.intValue > 100)
			{
				throw std::runtime_error(errorMsgInvalidValue);
			}
			return true;
		}
		else if (propertyName == "Glossiness")
		{
			// int: 0 < x < 255
			if (value.type != SPyWrappedProperty::eType_Int)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.intValue < iMinColorValue || value.property.intValue > iMaxColorValue)
			{
				throw std::runtime_error(errorMsgInvalidValue);
			}
			return true;
		}
		else if (propertyName == "Specular Level" || propertyName == "Heat Amount")
		{
			// float: 0.0 < x < 4.0
			if (value.type != SPyWrappedProperty::eType_Float)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.floatValue < 0.0f || value.property.floatValue > 4.0f)
			{
				throw std::runtime_error(errorMsgInvalidValue);
			}
			return true;
		}
		else if (propertyName == "Fur Amount" || propertyName == "Cloak Amount")
		{
			// float: 0.0 < x < 1.0
			if (value.type != SPyWrappedProperty::eType_Float)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.floatValue < 0.0f || value.property.floatValue > 1.0f)
			{
				throw std::runtime_error(errorMsgInvalidValue);
			}
			return true;
		}
		else if (
		  propertyName == "TileU" ||
		  propertyName == "TileV" ||
		  propertyName == "OffsetU" ||
		  propertyName == "OffsetV" ||
		  propertyName == "RotateU" ||
		  propertyName == "RotateV" ||
		  propertyName == "RotateW" ||
		  propertyName == "Rate" ||
		  propertyName == "Phase" ||
		  propertyName == "Amplitude" ||
		  propertyName == "CenterU" ||
		  propertyName == "CenterV" ||
		  propertyName == "RateU" ||
		  propertyName == "RateV" ||
		  propertyName == "PhaseU" ||
		  propertyName == "PhaseV" ||
		  propertyName == "AmplitudeU" ||
		  propertyName == "AmplitudeV" ||
		  propertyName == "Wave Length X" ||
		  propertyName == "Wave Length Y" ||
		  propertyName == "Wave Length Z" ||
		  propertyName == "Wave Length W" ||
		  propertyName == "Level" ||
		  propertyName == "Amplitude" ||
		  propertyName == "Phase" ||
		  propertyName == "Frequency")
		{
			// float: 0.0 < x < 100.0
			if (value.type != SPyWrappedProperty::eType_Float)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.floatValue < 0.0f || value.property.floatValue > 100.0f)
			{
				throw std::runtime_error(errorMsgInvalidValue);
			}
			return true;
		}
		else if (propertyName == "Diffuse Color" || propertyName == "Specular Color" || propertyName == "Emissive Color")
		{
			// intVector(RGB): 0 < x < 255
			if (value.type != SPyWrappedProperty::eType_Color)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			if (value.property.colorValue.r < iMinColorValue || value.property.colorValue.r > iMaxColorValue)
			{
				throw std::runtime_error(errorMsgInvalidValue + " (red)");
			}
			else if (value.property.colorValue.g < iMinColorValue || value.property.colorValue.g > iMaxColorValue)
			{
				throw std::runtime_error(errorMsgInvalidValue + " (green)");
			}
			else if (value.property.colorValue.b < iMinColorValue || value.property.colorValue.b > iMaxColorValue)
			{
				throw std::runtime_error(errorMsgInvalidValue + " (blue)");
			}
			return true;
		}
		else if (
		  propertyName == "Template Material" ||
		  propertyName == "Link to Material" ||
		  propertyName == "Surface Type" ||
		  propertyName == "Diffuse" ||
		  propertyName == "Specular" ||
		  propertyName == "Bumpmap" ||
		  propertyName == "Heightmap" ||
		  propertyName == "Environment" ||
		  propertyName == "Detail" ||
		  propertyName == "Opacity" ||
		  propertyName == "Decal" ||
		  propertyName == "SubSurface" ||
		  propertyName == "Custom" ||
		  propertyName == "[1] Custom" ||
		  propertyName == "TexType" ||
		  propertyName == "Filter" ||
		  propertyName == "TexGenType" ||
		  propertyName == "Type" ||
		  propertyName == "TypeU" ||
		  propertyName == "TypeV")
		{
			// string
			if (value.type != SPyWrappedProperty::eType_String)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}
			return true;
		}
		else if (
		  propertyName == "Additive" ||
		  propertyName == "Allow layer activation" ||
		  propertyName == "2 Sided" ||
		  propertyName == "No Shadow" ||
		  propertyName == "Use Scattering" ||
		  propertyName == "Hide After Breaking" ||
		  propertyName == "Traceable Texture" ||
		  propertyName == "Propagate Material Settings" ||
		  propertyName == "Propagate Opacity Settings" ||
		  propertyName == "Propagate Lighting Settings" ||
		  propertyName == "Propagate Advanced Settings" ||
		  propertyName == "Propagate Texture Maps" ||
		  propertyName == "Propagate Shader Params" ||
		  propertyName == "Propagate Shader Generation" ||
		  propertyName == "Propagate Vertex Deformation" ||
		  propertyName == "Propagate Layer Presets" ||
		  propertyName == "IsProjectedTexGen" ||
		  propertyName == "IsTileU" ||
		  propertyName == "IsTileV" ||
		  propertyName == "No Draw")
		{
			// bool
			if (value.type != SPyWrappedProperty::eType_Bool)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}
			return true;
		}
		else if (propertyName == "Shader" || propertyName == "Shader1" || propertyName == "Shader2" || propertyName == "Shader3")
		{
			// string && valid shader
			if (value.type != SPyWrappedProperty::eType_String)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}

			const auto& shaderList = GetIEditorImpl()->GetMaterialManager()->GetShaderList();
			if (std::find(shaderList.begin(), shaderList.end(), value.stringValue) != shaderList.end())
				return true;
		}
		else if (propertyName == "Noise Scale")
		{
			// FloatVec: undefined < x < undefined
			if (value.type != SPyWrappedProperty::eType_Vec3)
			{
				throw std::runtime_error(errorMsgInvalidDataType);
			}
			return true;
		}
	}
	else if (categoryName == "Shader Params")
	{
		DynArray<SShaderParam>& shaderParams = pMaterial->GetShaderResources().m_ShaderParams;
		for (int i = 0; i < shaderParams.size(); i++)
		{
			if (propertyName == ParseUINameFromPublicParamsScript(shaderParams[i].m_Script))
			{
				if (shaderParams[i].m_Type == eType_FLOAT)
				{
					// float: valid range (from script)
					if (value.type != SPyWrappedProperty::eType_Float)
					{
						throw std::runtime_error(errorMsgInvalidDataType);
					}
					std::map<string, float> range = ParseValidRangeFromPublicParamsScript(shaderParams[i].m_Script);
					if (value.property.floatValue < range["UIMin"] || value.property.floatValue > range["UIMax"])
					{
						string errorMsg;
						errorMsg.Format("Invalid value for shader param \"%s\" (min: %f, max: %f)", propertyName, range["UIMin"], range["UIMax"]);
						throw std::runtime_error(errorMsg);
					}
					return true;
				}
				else if (shaderParams[i].m_Type == eType_FCOLOR)
				{
					// intVector(RGB): 0 < x < 255
					if (value.type != SPyWrappedProperty::eType_Color)
					{
						throw std::runtime_error(errorMsgInvalidDataType);
					}

					if (value.property.colorValue.r < iMinColorValue || value.property.colorValue.r > iMaxColorValue)
					{
						throw std::runtime_error(errorMsgInvalidValue + " (red)");
					}
					else if (value.property.colorValue.g < iMinColorValue || value.property.colorValue.g > iMaxColorValue)
					{
						throw std::runtime_error(errorMsgInvalidValue + " (green)");
					}
					else if (value.property.colorValue.b < iMinColorValue || value.property.colorValue.b > iMaxColorValue)
					{
						throw std::runtime_error(errorMsgInvalidValue + " (blue)");
					}
					return true;
				}
			}
		}
	}
	else if (categoryName == "Shader Generation Params")
	{
		for (int i = 0; i < pMaterial->GetShaderGenParamsVars()->GetNumVariables(); i++)
		{
			if (propertyName == pMaterial->GetShaderGenParamsVars()->GetVariable(i)->GetHumanName())
			{
				if (value.type != SPyWrappedProperty::eType_Bool)
				{
					throw std::runtime_error(errorMsgInvalidDataType);
				}
				return true;
			}
		}
	}
	else
	{
		throw std::runtime_error(errorMsgInvalidPropertyPath + " (" + currentPath + ")");
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

SPyWrappedProperty PyGetProperty(const char* pPathAndMaterialName, const char* pPathAndPropertyName)
{
	CMaterial* pMaterial = TryLoadingMaterial(pPathAndMaterialName);
	std::deque<string> splittedPropertyPath = PreparePropertyPath(pPathAndPropertyName);
	std::deque<string> splittedPropertyPathCategory = splittedPropertyPath;
	string categoryName = splittedPropertyPath.front();
	string subCategoryName = "None";
	string subSubCategoryName = "None";
	string propertyName = splittedPropertyPath.back();
	string errorMsgInvalidPropertyPath = "Invalid property path.";
	SPyWrappedProperty value;

	if (splittedPropertyPathCategory.size() == 3)
	{
		splittedPropertyPathCategory.pop_back();
		subCategoryName = splittedPropertyPathCategory.back();
	}
	else if (splittedPropertyPathCategory.size() == 4)
	{
		splittedPropertyPathCategory.pop_back();
		subCategoryName = splittedPropertyPathCategory.back();
		splittedPropertyPathCategory.pop_back();
		subSubCategoryName = splittedPropertyPathCategory.back();
	}

	// ########## Material Settings ##########
	if (categoryName == "Material Settings")
	{
		if (propertyName == "Template Material")
		{
			value.type = SPyWrappedProperty::eType_String;
			value.stringValue = pMaterial->GetMatTemplate();
		}
		else if (propertyName == "Shader")
		{
			value.type = SPyWrappedProperty::eType_String;
			value.stringValue = pMaterial->GetShaderName();
		}
		else if (propertyName == "Surface Type")
		{
			value.type = SPyWrappedProperty::eType_String;
			value.stringValue = pMaterial->GetSurfaceTypeName();
			if (value.stringValue.Find("mat_") == 0)
			{
				value.stringValue.Delete(0, 4);
			}
		}
		else
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid material setting.");
		}
	}
	// ########## Opacity Settings ##########
	else if (categoryName == "Opacity Settings")
	{
		if (propertyName == "Opacity")
		{
			value.type = SPyWrappedProperty::eType_Int;
			value.property.floatValue = pMaterial->GetShaderResources().m_LMaterial.m_Opacity;
			value.property.intValue = value.property.floatValue * 100.0f;
		}
		else if (propertyName == "AlphaTest")
		{
			value.type = SPyWrappedProperty::eType_Int;
			value.property.floatValue = pMaterial->GetShaderResources().m_AlphaRef;
			value.property.intValue = value.property.floatValue * 100.0f;
		}
		else if (propertyName == "Additive")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_ADDITIVE;
		}
		else
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid opacity setting.");
		}
	}
	// ########## Lighting Settings ##########
	else if (categoryName == "Lighting Settings")
	{
		if (propertyName == "Diffuse Color")
		{
			value.type = SPyWrappedProperty::eType_Color;
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Diffuse.r,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Diffuse.g,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Diffuse.b));
			value.property.colorValue.r = (int)GetRValue(col);
			value.property.colorValue.g = (int)GetGValue(col);
			value.property.colorValue.b = (int)GetBValue(col);
		}
		else if (propertyName == "Specular Color")
		{
			value.type = SPyWrappedProperty::eType_Color;
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Specular.r / pMaterial->GetShaderResources().m_LMaterial.m_Specular.a,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Specular.g / pMaterial->GetShaderResources().m_LMaterial.m_Specular.a,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Specular.b / pMaterial->GetShaderResources().m_LMaterial.m_Specular.a));
			value.property.colorValue.r = (int)GetRValue(col);
			value.property.colorValue.g = (int)GetGValue(col);
			value.property.colorValue.b = (int)GetBValue(col);
		}
		else if (propertyName == "Glossiness")
		{
			value.type = SPyWrappedProperty::eType_Int;
			value.property.floatValue = pMaterial->GetShaderResources().m_LMaterial.m_Smoothness;
			value.property.intValue = (int)value.property.floatValue;
		}
		else if (propertyName == "Specular Level")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = pMaterial->GetShaderResources().m_LMaterial.m_Specular.a;
		}
		else if (propertyName == "Emissive Color")
		{
			value.type = SPyWrappedProperty::eType_Color;
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Emittance.r,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Emittance.g,
			                                    pMaterial->GetShaderResources().m_LMaterial.m_Emittance.b));
			value.property.colorValue.r = (int)GetRValue(col);
			value.property.colorValue.g = (int)GetGValue(col);
			value.property.colorValue.b = (int)GetBValue(col);
		}
		else if (propertyName == "Emissive Intensity")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = pMaterial->GetShaderResources().m_LMaterial.m_Emittance.a;
		}
		else
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid lighting setting.");
		}
	}
	// ########## Advanced ##########
	else if (categoryName == "Advanced")
	{
		if (propertyName == "Allow layer activation")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->LayerActivationAllowed();
		}
		else if (propertyName == "2 Sided")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_2SIDED;
		}
		else if (propertyName == "No Shadow")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_NOSHADOW;
		}
		else if (propertyName == "Use Scattering")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_SCATTER;
		}
		else if (propertyName == "Hide After Breaking")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_HIDEONBREAK;
		}
		else if (propertyName == "Traceable Texture")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetFlags() & MTL_FLAG_TRACEABLE_TEXTURE;
		}
		else if (propertyName == "Fur Amount")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = (float)pMaterial->GetShaderResources().m_FurAmount / 255.0f;
		}
		else if (propertyName == "Voxel Coverage")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = (float)pMaterial->GetShaderResources().m_VoxelCoverage / 255.0f;
		}
		else if (propertyName == "Heat Amount")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = ((float)pMaterial->GetShaderResources().m_HeatAmount / 255.0f) * 4.0f;
		}
		else if (propertyName == "Cloak Amount")
		{
			value.type = SPyWrappedProperty::eType_Float;
			value.property.floatValue = (float)pMaterial->GetShaderResources().m_CloakAmount / 255.0f;
		}
		else if (propertyName == "Link to Material")
		{
			value.type = SPyWrappedProperty::eType_String;
			value.stringValue = pMaterial->GetMatInfo()->GetMaterialLinkName();
		}
		else if (propertyName == "Propagate Material Settings")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_MATERIAL_SETTINGS;
		}
		else if (propertyName == "Propagate Opacity Settings")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_OPACITY;
		}
		else if (propertyName == "Propagate Lighting Settings")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_LIGHTING;
		}
		else if (propertyName == "Propagate Advanced Settings")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_ADVANCED;
		}
		else if (propertyName == "Propagate Texture Maps")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_TEXTURES;
		}
		else if (propertyName == "Propagate Shader Params")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_SHADER_PARAMS;
		}
		else if (propertyName == "Propagate Shader Generation")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_SHADER_GEN;
		}
		else if (propertyName == "Propagate Vertex Deformation")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_VERTEX_DEF;
		}
		else if (propertyName == "Propagate Layer Presets")
		{
			value.type = SPyWrappedProperty::eType_Bool;
			value.property.boolValue = pMaterial->GetPropagationFlags() & MTL_PROPAGATE_LAYER_PRESETS;
		}
		else
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid advanced setting.");
		}
	}
	// ########## Texture Maps ##########
	else if (categoryName == "Texture Maps")
	{
		// ########## Texture Maps / [name] ##########
		if (splittedPropertyPath.size() == 2)
		{
			value.type = SPyWrappedProperty::eType_String;
			value.stringValue = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(propertyName)].m_Name.c_str();
		}
		// ########## Texture Maps / [TexType | Filter | IsProjectedTexGen | TexGenType ] ##########
		else if (splittedPropertyPath.size() == 3)
		{
			SEfResTexture& currentResTexture = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(subCategoryName)];

			if (propertyName == "TexType")
			{
				value.type = SPyWrappedProperty::eType_String;
				value.stringValue = TryConvertingETEX_TypeToCString(currentResTexture.m_Sampler.m_eTexType);
			}
			else if (propertyName == "Filter")
			{
				value.type = SPyWrappedProperty::eType_String;
				value.stringValue = TryConvertingTexFilterToCString(currentResTexture.m_Filter);
			}
			else if (propertyName == "IsProjectedTexGen")
			{
				value.type = SPyWrappedProperty::eType_Bool;
				value.property.boolValue = currentResTexture.AddModificator()->m_bTexGenProjected;
			}
			else if (propertyName == "TexGenType")
			{
				value.type = SPyWrappedProperty::eType_String;
				value.stringValue = TryConvertingETexGenTypeToCString(currentResTexture.AddModificator()->m_eTGType);
			}
			else
			{
				throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
			}
		}
		// ########## Texture Maps / [Tiling | Rotator | Oscillator] ##########
		else if (splittedPropertyPath.size() == 4)
		{
			SEfResTexture& currentResTexture = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(subSubCategoryName)];

			if (subCategoryName == "Tiling")
			{
				if (propertyName == "IsTileU")
				{
					value.type = SPyWrappedProperty::eType_Bool;
					value.property.boolValue = currentResTexture.m_bUTile;
				}
				else if (propertyName == "IsTileV")
				{
					value.type = SPyWrappedProperty::eType_Bool;
					value.property.boolValue = currentResTexture.m_bVTile;
				}
				else if (propertyName == "TileU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_Tiling[0];
				}
				else if (propertyName == "TileV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_Tiling[1];
				}
				else if (propertyName == "OffsetU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_Offs[0];
				}
				else if (propertyName == "OffsetV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_Offs[1];
				}
				else if (propertyName == "RotateU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_Rot[0]);
				}
				else if (propertyName == "RotateV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_Rot[1]);
				}
				else if (propertyName == "RotateW")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_Rot[2]);
				}
				else
				{
					throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
				}
			}
			else if (subCategoryName == "Rotator")
			{
				if (propertyName == "Type")
				{
					value.type = SPyWrappedProperty::eType_String;
					value.stringValue = TryConvertingETexModRotateTypeToCString(currentResTexture.AddModificator()->m_eRotType);
				}
				else if (propertyName == "Rate")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_RotOscRate[2]);
				}
				else if (propertyName == "Phase")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_RotOscPhase[2]);
				}
				else if (propertyName == "Amplitude")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = Word2Degr(currentResTexture.AddModificator()->m_RotOscAmplitude[2]);
				}
				else if (propertyName == "CenterU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_RotOscCenter[0];
				}
				else if (propertyName == "CenterV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_RotOscCenter[1];
				}
				else
				{
					throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
				}
			}
			else if (subCategoryName == "Oscillator")
			{
				if (propertyName == "TypeU")
				{
					value.type = SPyWrappedProperty::eType_String;
					value.stringValue = TryConvertingETexModMoveTypeToCString(currentResTexture.AddModificator()->m_eMoveType[0]);
				}
				else if (propertyName == "TypeV")
				{
					value.type = SPyWrappedProperty::eType_String;
					value.stringValue = TryConvertingETexModMoveTypeToCString(currentResTexture.AddModificator()->m_eMoveType[1]);
				}
				else if (propertyName == "RateU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscRate[0];
				}
				else if (propertyName == "RateV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscRate[1];
				}
				else if (propertyName == "PhaseU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscPhase[0];
				}
				else if (propertyName == "PhaseV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscPhase[1];
				}
				else if (propertyName == "AmplitudeU")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscAmplitude[0];
				}
				else if (propertyName == "AmplitudeV")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentResTexture.AddModificator()->m_OscAmplitude[1];
				}
				else
				{
					throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
				}
			}
			else
			{
				throw std::runtime_error(string("\"") + subCategoryName + "\" is an invalid sub category.");
			}
		}
		else
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath);
		}
	}
	// ########## Shader Params ##########
	else if (categoryName == "Shader Params")
	{
		DynArray<SShaderParam>& shaderParams = pMaterial->GetShaderResources().m_ShaderParams;
		bool isPropertyFound(false);

		for (int i = 0; i < shaderParams.size(); i++)
		{
			if (propertyName == ParseUINameFromPublicParamsScript(shaderParams[i].m_Script))
			{
				if (shaderParams[i].m_Type == eType_FLOAT)
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = shaderParams[i].m_Value.m_Float;
					isPropertyFound = true;
					break;
				}
				else if (shaderParams[i].m_Type == eType_FCOLOR)
				{
					value.type = SPyWrappedProperty::eType_Color;
					COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(
					                                    shaderParams[i].m_Value.m_Vector[0],
					                                    shaderParams[i].m_Value.m_Vector[1],
					                                    shaderParams[i].m_Value.m_Vector[2]));
					value.property.colorValue.r = (int)GetRValue(col);
					value.property.colorValue.g = (int)GetGValue(col);
					value.property.colorValue.b = (int)GetBValue(col);
					isPropertyFound = true;
					break;
				}
			}
		}

		if (!isPropertyFound)
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid shader param.");
		}
	}
	// ########## Shader Generation Params ##########
	else if (categoryName == "Shader Generation Params")
	{
		value.type = SPyWrappedProperty::eType_Bool;
		bool isPropertyFound(false);
		for (int i = 0; i < pMaterial->GetShaderGenParamsVars()->GetNumVariables(); i++)
		{
			if (propertyName == pMaterial->GetShaderGenParamsVars()->GetVariable(i)->GetHumanName())
			{
				isPropertyFound = true;
				pMaterial->GetShaderGenParamsVars()->GetVariable(i)->Get(value.property.boolValue);
				break;
			}
		}

		if (!isPropertyFound)
		{
			throw std::runtime_error(string("\"") + propertyName + "\" is an invalid shader generation param.");
		}
	}
	// ########## Vertex Deformation ##########
	else if (categoryName == "Vertex Deformation")
	{
		// ########## Vertex Deformation / [ Type | Wave Length X | Wave Length Y | Wave Length Z | Wave Length W | Noise Scale ] ##########
		if (splittedPropertyPath.size() == 2)
		{
			if (propertyName == "Type")
			{
				value.type = SPyWrappedProperty::eType_String;
				value.stringValue = TryConvertingEDeformTypeToCString(pMaterial->GetShaderResources().m_DeformInfo.m_eType);
			}
			else if (propertyName == "Wave Length X")
			{
				value.type = SPyWrappedProperty::eType_Float;
				value.property.floatValue = pMaterial->GetShaderResources().m_DeformInfo.m_fDividerX;
			}
			else if (propertyName == "Wave Length Y")
			{
				value.type = SPyWrappedProperty::eType_Float;
				value.property.floatValue = pMaterial->GetShaderResources().m_DeformInfo.m_fDividerY;
			}
			else if (propertyName == "Wave Length Z")
			{
				value.type = SPyWrappedProperty::eType_Float;
				value.property.floatValue = pMaterial->GetShaderResources().m_DeformInfo.m_fDividerZ;
			}
			else if (propertyName == "Wave Length W")
			{
				value.type = SPyWrappedProperty::eType_Float;
				value.property.floatValue = pMaterial->GetShaderResources().m_DeformInfo.m_fDividerW;
			}
			else if (propertyName == "Noise Scale")
			{
				value.type = SPyWrappedProperty::eType_Vec3;
				value.property.vecValue.x = pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[0];
				value.property.vecValue.y = pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[1];
				value.property.vecValue.z = pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[2];
			}
			else
			{
				throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
			}
		}
		// ########## Vertex Deformation / [ Wave X | Wave Y | Wave Z | Wave W ] ##########
		else if (splittedPropertyPath.size() == 3)
		{
			if (subCategoryName == "Wave X" || subCategoryName == "Wave Y" || subCategoryName == "Wave Z" || subCategoryName == "Wave W")
			{
				SWaveForm2 currentWaveForm;
				if (subCategoryName == "Wave X")
				{
					currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveX;
				}
				else if (subCategoryName == "Wave Y")
				{
					currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveY;
				}
				else if (subCategoryName == "Wave Z")
				{
					currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveZ;
				}
				else if (subCategoryName == "Wave W")
				{
					currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveW;
				}

				if (propertyName == "Type")
				{
					value.type = SPyWrappedProperty::eType_String;
					value.stringValue = TryConvertingEWaveFormToCString(currentWaveForm.m_eWFType);
				}
				else if (propertyName == "Level")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentWaveForm.m_Level;
				}
				else if (propertyName == "Amplitude")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentWaveForm.m_Amp;
				}
				else if (propertyName == "Phase")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentWaveForm.m_Phase;
				}
				else if (propertyName == "Frequency")
				{
					value.type = SPyWrappedProperty::eType_Float;
					value.property.floatValue = currentWaveForm.m_Freq;
				}
				else
				{
					throw std::runtime_error(string("\"") + propertyName + "\" is an invalid property.");
				}
			}
			else
			{
				throw std::runtime_error(string("\"") + categoryName + "\" is an invalid category.");
			}
		}
		else
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath);
		}
	}
	// ########## Layer Presets ##########
	else if (categoryName == "Layer Presets")
	{
		// names are "Shader1", "Shader2" and "Shader3", bacause all have the name "Shader" in material editor
		if (splittedPropertyPath.size() == 2)
		{
			value.type = SPyWrappedProperty::eType_String;

			int shaderNumber = -1;
			if (propertyName == "Shader1")
			{
				shaderNumber = 0;
			}
			else if (propertyName == "Shader2")
			{
				shaderNumber = 1;
			}
			else if (propertyName == "Shader3")
			{
				shaderNumber = 2;
			}
			else
			{
				throw std::runtime_error("Invalid shader.");
			}

			value.stringValue = pMaterial->GetMtlLayerResources()[shaderNumber].m_shaderName;
		}
		else if (splittedPropertyPath.size() == 3)
		{
			if (propertyName == "No Draw")
			{
				value.type = SPyWrappedProperty::eType_Bool;

				int shaderNumber = -1;
				if (subCategoryName == "Shader1")
				{
					shaderNumber = 0;
				}
				else if (subCategoryName == "Shader2")
				{
					shaderNumber = 1;
				}
				else if (subCategoryName == "Shader3")
				{
					shaderNumber = 2;
				}
				else
				{
					throw std::runtime_error("Invalid shader.");
				}
				value.property.boolValue = pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags & MTL_LAYER_USAGE_NODRAW;
			}
		}
		else
		{
			throw std::runtime_error(errorMsgInvalidPropertyPath);
		}
	}
	else
	{
		throw std::runtime_error(errorMsgInvalidPropertyPath);
	}

	return value;
}

void PySetProperty(const char* pPathAndMaterialName, const char* pPathAndPropertyName, const SPyWrappedProperty& value)
{
	CMaterial* pMaterial = TryLoadingMaterial(pPathAndMaterialName);
	std::deque<string> splittedPropertyPath = PreparePropertyPath(pPathAndPropertyName);
	std::deque<string> splittedPropertyPathCategory = splittedPropertyPath;
	string categoryName = splittedPropertyPath.front();
	string subCategoryName = "None";
	string subSubCategoryName = "None";
	string propertyName = splittedPropertyPath.back();
	string errorMsgInvalidPropertyPath = "Invalid property path.";

	if (!ValidateProperty(pMaterial, splittedPropertyPath, value))
	{
		throw std::runtime_error("Invalid property.");
	}

	string undoMsg = "Set Material Property";
	CUndo undo(undoMsg);
	pMaterial->RecordUndo(undoMsg, true);

	if (splittedPropertyPathCategory.size() == 3)
	{
		splittedPropertyPathCategory.pop_back();
		subCategoryName = splittedPropertyPathCategory.back();
	}
	else if (splittedPropertyPathCategory.size() == 4)
	{
		splittedPropertyPathCategory.pop_back();
		subCategoryName = splittedPropertyPathCategory.back();
		splittedPropertyPathCategory.pop_back();
		subSubCategoryName = splittedPropertyPathCategory.back();
	}

	// ########## Material Settings ##########
	if (categoryName == "Material Settings")
	{
		if (propertyName == "Template Material")
		{
			pMaterial->SetMatTemplate(value.stringValue);
		}
		else if (propertyName == "Shader")
		{
			pMaterial->SetShaderName(value.stringValue);
		}
		else if (propertyName == "Surface Type")
		{
			bool isSurfaceExist(false);
			string realSurfacename = "";
			ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
			if (pSurfaceTypeEnum)
			{
				for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
				{
					string surfaceName = pSurfaceType->GetName();
					realSurfacename = surfaceName;
					if (surfaceName.Left(4) == "mat_")
					{
						surfaceName.Delete(0, 4);
					}
					if (surfaceName == value.stringValue)
					{
						isSurfaceExist = true;
						pMaterial->SetSurfaceTypeName(realSurfacename);
					}
				}

				if (!isSurfaceExist)
				{
					throw std::runtime_error("Invalid surface type name.");
				}
			}
			else
			{
				throw std::runtime_error("Surface Type Enumerator corrupted.");
			}
		}
	}
	// ########## Opacity Settings ##########
	else if (categoryName == "Opacity Settings")
	{
		if (propertyName == "Opacity")
		{
			pMaterial->GetShaderResources().m_LMaterial.m_Opacity = static_cast<float>(value.property.intValue) / 100.0f;
		}
		else if (propertyName == "AlphaTest")
		{
			pMaterial->GetShaderResources().m_AlphaRef = static_cast<float>(value.property.intValue) / 100.0f;
		}
		else if (propertyName == "Additive")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_ADDITIVE, value.property.boolValue);
		}
	}
	// ########## Lighting Settings ##########
	else if (categoryName == "Lighting Settings")
	{
		if (propertyName == "Diffuse Color")
		{
			ColorB color(value.property.colorValue.r, value.property.colorValue.g, value.property.colorValue.b);
			pMaterial->GetShaderResources().m_LMaterial.m_Diffuse = CMFCUtils::ColorGammaToLinear(RGB(color.r, color.g, color.b));
		}
		else if (propertyName == "Specular Color")
		{
			ColorB color(value.property.colorValue.r, value.property.colorValue.g, value.property.colorValue.b);
			ColorF colorFloat = CMFCUtils::ColorGammaToLinear(RGB(color.r, color.g, color.b));
			colorFloat.a = pMaterial->GetShaderResources().m_LMaterial.m_Specular.a;
			colorFloat.r *= colorFloat.a;
			colorFloat.g *= colorFloat.a;
			colorFloat.b *= colorFloat.a;
			pMaterial->GetShaderResources().m_LMaterial.m_Specular = colorFloat;
		}
		else if (propertyName == "Glossiness")
		{
			pMaterial->GetShaderResources().m_LMaterial.m_Smoothness = static_cast<float>(value.property.intValue);
		}
		else if (propertyName == "Specular Level")
		{
			float tempFloat = pMaterial->GetShaderResources().m_LMaterial.m_Specular.a;
			float tempFloat2 = value.property.floatValue;

			ColorF colorFloat = pMaterial->GetShaderResources().m_LMaterial.m_Specular;
			colorFloat.a = value.property.floatValue;
			colorFloat.r *= colorFloat.a;
			colorFloat.g *= colorFloat.a;
			colorFloat.b *= colorFloat.a;
			colorFloat.a = 1.0f;
			pMaterial->GetShaderResources().m_LMaterial.m_Specular = colorFloat;

		}
		else if (propertyName == "Emissive Color")
		{
			ColorB color(value.property.colorValue.r, value.property.colorValue.g, value.property.colorValue.b);
			float emissiveIntensity = pMaterial->GetShaderResources().m_LMaterial.m_Emittance.a;
			pMaterial->GetShaderResources().m_LMaterial.m_Emittance = CMFCUtils::ColorGammaToLinear(RGB(color.r, color.g, color.b));
			pMaterial->GetShaderResources().m_LMaterial.m_Emittance.a = emissiveIntensity;
		}
		else if (propertyName == "Emissive Intensity")
		{
			pMaterial->GetShaderResources().m_LMaterial.m_Emittance.a = value.property.floatValue;
		}
	}
	// ########## Advanced ##########
	else if (categoryName == "Advanced")
	{
		if (propertyName == "Allow layer activation")
		{
			pMaterial->SetLayerActivation(value.property.boolValue);
		}
		else if (propertyName == "2 Sided")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_2SIDED, value.property.boolValue);
		}
		else if (propertyName == "No Shadow")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_NOSHADOW, value.property.boolValue);
		}
		else if (propertyName == "Use Scattering")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_SCATTER, value.property.boolValue);
		}
		else if (propertyName == "Hide After Breaking")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_HIDEONBREAK, value.property.boolValue);
		}
		else if (propertyName == "Traceable Texture")
		{
			SetMaterialFlag(pMaterial, MTL_FLAG_TRACEABLE_TEXTURE, value.property.boolValue);
		}
		else if (propertyName == "Fur Amount")
		{
			pMaterial->GetShaderResources().m_FurAmount = static_cast<uint8>(value.property.floatValue * 255.0f);
		}
		else if (propertyName == "Voxel Coverage")
		{
			pMaterial->GetShaderResources().m_VoxelCoverage = static_cast<uint8>(value.property.floatValue * 255.0f);
		}
		else if (propertyName == "Heat Amount")
		{
			pMaterial->GetShaderResources().m_HeatAmount = static_cast<uint8>((value.property.floatValue / 4.0f) * 255.0f);
		}
		else if (propertyName == "Cloak Amount")
		{
			pMaterial->GetShaderResources().m_CloakAmount = static_cast<uint8>((value.property.floatValue * 255.0f));
		}
		else if (propertyName == "Link to Material")
		{
			pMaterial->GetMatInfo()->SetMaterialLinkName(value.stringValue);
		}
		else if (propertyName == "Propagate Material Settings")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_MATERIAL_SETTINGS, value.property.boolValue);
		}
		else if (propertyName == "Propagate Opacity Settings")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_OPACITY, value.property.boolValue);
		}
		else if (propertyName == "Propagate Lighting Settings")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_LIGHTING, value.property.boolValue);
		}
		else if (propertyName == "Propagate Advanced Settings")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_ADVANCED, value.property.boolValue);
		}
		else if (propertyName == "Propagate Texture Maps")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_TEXTURES, value.property.boolValue);
		}
		else if (propertyName == "Propagate Shader Params")
		{
			if (value.property.boolValue == true)
			{
				SetPropagationFlag(pMaterial, MTL_PROPAGATE_MATERIAL_SETTINGS, true);
			}
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_SHADER_PARAMS, value.property.boolValue);
		}
		else if (propertyName == "Propagate Shader Generation")
		{
			if (value.property.boolValue == true)
			{
				SetPropagationFlag(pMaterial, MTL_PROPAGATE_MATERIAL_SETTINGS, true);
			}
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_SHADER_GEN, value.property.boolValue);
		}
		else if (propertyName == "Propagate Vertex Deformation")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_VERTEX_DEF, value.property.boolValue);
		}
		else if (propertyName == "Propagate Layer Presets")
		{
			SetPropagationFlag(pMaterial, MTL_PROPAGATE_LAYER_PRESETS, value.property.boolValue);
		}
	}
	// ########## Texture Maps ##########
	else if (categoryName == "Texture Maps")
	{
		// ########## Texture Maps / [name] ##########
		if (splittedPropertyPath.size() == 2)
		{
			SEfResTexture& currentResTexture = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(propertyName)];
			currentResTexture.m_Name = value.stringValue;
		}
		// ########## Texture Maps / [TexType | Filter | IsProjectedTexGen | TexGenType ] ##########
		else if (splittedPropertyPath.size() == 3)
		{
			SEfResTexture& currentResTexture = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(subCategoryName)];

			if (propertyName == "TexType")
			{
				currentResTexture.m_Sampler.m_eTexType = TryConvertingCStringToETEX_Type(value.stringValue);
			}
			else if (propertyName == "Filter")
			{
				currentResTexture.m_Filter = TryConvertingCStringToTexFilter(value.stringValue);
			}
			else if (propertyName == "IsProjectedTexGen")
			{
				currentResTexture.AddModificator()->m_bTexGenProjected = value.property.boolValue;
			}
			else if (propertyName == "TexGenType")
			{
				currentResTexture.AddModificator()->m_eTGType = TryConvertingCStringToETexGenType(value.stringValue);
			}
		}
		// ########## Texture Maps / [Tiling | Rotator | Oscillator] ##########
		else if (splittedPropertyPath.size() == 4)
		{
			SEfResTexture& currentResTexture = pMaterial->GetShaderResources().m_Textures[TryConvertingCStringToEEfResTextures(subSubCategoryName)];

			if (subCategoryName == "Tiling")
			{
				if (propertyName == "IsTileU")
				{
					currentResTexture.m_bUTile = value.property.boolValue;
				}
				else if (propertyName == "IsTileV")
				{
					currentResTexture.m_bVTile = value.property.boolValue;
				}
				else if (propertyName == "TileU")
				{
					currentResTexture.AddModificator()->m_Tiling[0] = value.property.floatValue;
				}
				else if (propertyName == "TileV")
				{
					currentResTexture.AddModificator()->m_Tiling[1] = value.property.floatValue;
				}
				else if (propertyName == "OffsetU")
				{
					currentResTexture.AddModificator()->m_Offs[0] = value.property.floatValue;
				}
				else if (propertyName == "OffsetV")
				{
					currentResTexture.AddModificator()->m_Offs[1] = value.property.floatValue;
				}
				else if (propertyName == "RotateU")
				{
					currentResTexture.AddModificator()->m_Rot[0] = Degr2Word(value.property.floatValue);
				}
				else if (propertyName == "RotateV")
				{
					currentResTexture.AddModificator()->m_Rot[1] = Degr2Word(value.property.floatValue);
				}
				else if (propertyName == "RotateW")
				{
					currentResTexture.AddModificator()->m_Rot[2] = Degr2Word(value.property.floatValue);
				}
			}
			else if (subCategoryName == "Rotator")
			{
				if (propertyName == "Type")
				{
					currentResTexture.AddModificator()->m_eRotType = TryConvertingCStringToETexModRotateType(value.stringValue);
				}
				else if (propertyName == "Rate")
				{
					currentResTexture.AddModificator()->m_RotOscRate[2] = Degr2Word(value.property.floatValue);
				}
				else if (propertyName == "Phase")
				{
					currentResTexture.AddModificator()->m_RotOscPhase[2] = Degr2Word(value.property.floatValue);
				}
				else if (propertyName == "Amplitude")
				{
					currentResTexture.AddModificator()->m_RotOscAmplitude[2] = Degr2Word(value.property.floatValue);
				}
				else if (propertyName == "CenterU")
				{
					currentResTexture.AddModificator()->m_RotOscCenter[0] = value.property.floatValue;
				}
				else if (propertyName == "CenterV")
				{
					currentResTexture.AddModificator()->m_RotOscCenter[1] = value.property.floatValue;
				}
			}
			else if (subCategoryName == "Oscillator")
			{
				if (propertyName == "TypeU")
				{
					currentResTexture.AddModificator()->m_eMoveType[0] = TryConvertingCStringToETexModMoveType(value.stringValue);
				}
				else if (propertyName == "TypeV")
				{
					currentResTexture.AddModificator()->m_eMoveType[1] = TryConvertingCStringToETexModMoveType(value.stringValue);
				}
				else if (propertyName == "RateU")
				{
					currentResTexture.AddModificator()->m_OscRate[0] = value.property.floatValue;
				}
				else if (propertyName == "RateV")
				{
					currentResTexture.AddModificator()->m_OscRate[1] = value.property.floatValue;
				}
				else if (propertyName == "PhaseU")
				{
					currentResTexture.AddModificator()->m_OscPhase[0] = value.property.floatValue;
				}
				else if (propertyName == "PhaseV")
				{
					currentResTexture.AddModificator()->m_OscPhase[1] = value.property.floatValue;
				}
				else if (propertyName == "AmplitudeU")
				{
					currentResTexture.AddModificator()->m_OscAmplitude[0] = value.property.floatValue;
				}
				else if (propertyName == "AmplitudeV")
				{
					currentResTexture.AddModificator()->m_OscAmplitude[1] = value.property.floatValue;
				}
			}
		}
	}
	// ########## Shader Params ##########
	else if (categoryName == "Shader Params")
	{
		DynArray<SShaderParam>& shaderParams = pMaterial->GetShaderResources().m_ShaderParams;

		for (int i = 0; i < shaderParams.size(); i++)
		{
			if (propertyName == ParseUINameFromPublicParamsScript(shaderParams[i].m_Script))
			{
				if (shaderParams[i].m_Type == eType_FLOAT)
				{
					shaderParams[i].m_Value.m_Float = value.property.floatValue;
					break;
				}
				else if (shaderParams[i].m_Type == eType_FCOLOR)
				{
					ColorF colorLinear = CMFCUtils::ColorGammaToLinear(
					  RGB(
					    (uint8)value.property.colorValue.r,
					    (uint8)value.property.colorValue.g,
					    (uint8)value.property.colorValue.b
					    ));

					shaderParams[i].m_Value.m_Vector[0] = colorLinear.r;
					shaderParams[i].m_Value.m_Vector[1] = colorLinear.g;
					shaderParams[i].m_Value.m_Vector[2] = colorLinear.b;
					break;
				}
				else
				{
					throw std::runtime_error("Invalid data type (Shader Params)");
				}
			}
		}
	}
	// ########## Shader Generation Params ##########
	else if (categoryName == "Shader Generation Params")
	{
		for (int i = 0; i < pMaterial->GetShaderGenParamsVars()->GetNumVariables(); i++)
		{
			if (propertyName == pMaterial->GetShaderGenParamsVars()->GetVariable(i)->GetHumanName())
			{
				CVarBlock* shaderGenBlock = pMaterial->GetShaderGenParamsVars();
				shaderGenBlock->GetVariable(i)->Set(value.property.boolValue);
				pMaterial->SetShaderGenParamsVars(shaderGenBlock);
				break;
			}
		}
	}
	// ########## Vertex Deformation ##########
	else if (categoryName == "Vertex Deformation")
	{
		// ########## Vertex Deformation / [ Type | Wave Length X | Wave Length Y | Wave Length Z | Wave Length W | Noise Scale ] ##########
		if (splittedPropertyPath.size() == 2)
		{
			if (propertyName == "Type")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_eType = TryConvertingCStringToEDeformType(value.stringValue);
			}
			else if (propertyName == "Wave Length X")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_fDividerX = value.property.floatValue;
			}
			else if (propertyName == "Wave Length Y")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_fDividerY = value.property.floatValue;
			}
			else if (propertyName == "Wave Length Z")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_fDividerZ = value.property.floatValue;
			}
			else if (propertyName == "Wave Length W")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_fDividerW = value.property.floatValue;
			}
			else if (propertyName == "Noise Scale")
			{
				pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[0] = value.property.vecValue.x;
				pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[1] = value.property.vecValue.y;
				pMaterial->GetShaderResources().m_DeformInfo.m_vNoiseScale[2] = value.property.vecValue.z;
			}
		}
		// ########## Vertex Deformation / [ Wave X | Wave Y | Wave Z | Wave W ] ##########
		else if (splittedPropertyPath.size() == 3)
		{
			if (subCategoryName == "Wave X")
			{
				SWaveForm2& currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveX;

				if (propertyName == "Type")
				{
					currentWaveForm.m_eWFType = TryConvertingCStringToEWaveForm(value.stringValue);
				}
				else if (propertyName == "Level")
				{
					currentWaveForm.m_Level = value.property.floatValue;
				}
				else if (propertyName == "Amplitude")
				{
					currentWaveForm.m_Amp = value.property.floatValue;
				}
				else if (propertyName == "Phase")
				{
					currentWaveForm.m_Phase = value.property.floatValue;
				}
				else if (propertyName == "Frequency")
				{
					currentWaveForm.m_Freq = value.property.floatValue;
				}
			}
			else if (subCategoryName == "Wave Y")
			{
				SWaveForm2& currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveY;

				if (propertyName == "Type")
				{
					currentWaveForm.m_eWFType = TryConvertingCStringToEWaveForm(value.stringValue);
				}
				else if (propertyName == "Level")
				{
					currentWaveForm.m_Level = value.property.floatValue;
				}
				else if (propertyName == "Amplitude")
				{
					currentWaveForm.m_Amp = value.property.floatValue;
				}
				else if (propertyName == "Phase")
				{
					currentWaveForm.m_Phase = value.property.floatValue;
				}
				else if (propertyName == "Frequency")
				{
					currentWaveForm.m_Freq = value.property.floatValue;
				}
			}
			else if (subCategoryName == "Wave Z")
			{
				SWaveForm2& currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveZ;

				if (propertyName == "Type")
				{
					currentWaveForm.m_eWFType = TryConvertingCStringToEWaveForm(value.stringValue);
				}
				else if (propertyName == "Level")
				{
					currentWaveForm.m_Level = value.property.floatValue;
				}
				else if (propertyName == "Amplitude")
				{
					currentWaveForm.m_Amp = value.property.floatValue;
				}
				else if (propertyName == "Phase")
				{
					currentWaveForm.m_Phase = value.property.floatValue;
				}
				else if (propertyName == "Frequency")
				{
					currentWaveForm.m_Freq = value.property.floatValue;
				}
			}
			else if (subCategoryName == "Wave W")
			{
				SWaveForm2& currentWaveForm = pMaterial->GetShaderResources().m_DeformInfo.m_WaveW;

				if (propertyName == "Type")
				{
					currentWaveForm.m_eWFType = TryConvertingCStringToEWaveForm(value.stringValue);
				}
				else if (propertyName == "Level")
				{
					currentWaveForm.m_Level = value.property.floatValue;
				}
				else if (propertyName == "Amplitude")
				{
					currentWaveForm.m_Amp = value.property.floatValue;
				}
				else if (propertyName == "Phase")
				{
					currentWaveForm.m_Phase = value.property.floatValue;
				}
				else if (propertyName == "Frequency")
				{
					currentWaveForm.m_Freq = value.property.floatValue;
				}
			}
		}
	}
	// ########## Layer Presets ##########
	else if (categoryName == "Layer Presets")
	{
		// names are "Shader1", "Shader2" and "Shader3", bacause all have the name "Shader" in material editor
		if (splittedPropertyPath.size() == 2)
		{
			int shaderNumber = -1;
			if (propertyName == "Shader1")
			{
				shaderNumber = 0;
			}
			else if (propertyName == "Shader2")
			{
				shaderNumber = 1;
			}
			else if (propertyName == "Shader3")
			{
				shaderNumber = 2;
			}

			pMaterial->GetMtlLayerResources()[shaderNumber].m_shaderName = value.stringValue;
		}
		else if (splittedPropertyPath.size() == 3)
		{
			if (propertyName == "No Draw")
			{
				int shaderNumber = -1;
				if (subCategoryName == "Shader1")
				{
					shaderNumber = 0;
				}
				else if (subCategoryName == "Shader2")
				{
					shaderNumber = 1;
				}
				else if (subCategoryName == "Shader3")
				{
					shaderNumber = 2;
				}

				if (pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags & MTL_LAYER_USAGE_NODRAW && value.property.boolValue == false)
				{
					pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags = pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags - MTL_LAYER_USAGE_NODRAW;
				}
				else if (!(pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags & MTL_LAYER_USAGE_NODRAW) && value.property.boolValue == true)
				{
					pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags = pMaterial->GetMtlLayerResources()[shaderNumber].m_nFlags | MTL_LAYER_USAGE_NODRAW;
				}
			}
		}
	}

	pMaterial->Update();
	pMaterial->Save();
	GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(pMaterial);
}
}

DECLARE_PYTHON_MODULE(material);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialCreate, material, create,
                                     "Creates a material.",
                                     "material.create()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialCreateMulti, material, create_multi,
                                     "Creates a multi-material.",
                                     "material.create_multi()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialConvertToMulti, material, convert_to_multi,
                                     "Converts the selected material to a multi-material.",
                                     "material.convert_to_multi()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialDuplicateCurrent, material, duplicate_current,
                                     "Duplicates the current material.",
                                     "material.duplicate_current()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialMergeSelection, material, merge_selection,
                                     "Merges the selected materials.",
                                     "material.merge_selection()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialDeleteCurrent, material, delete_current,
                                     "Deletes the current material.",
                                     "material.delete_current()");
REGISTER_PYTHON_COMMAND(PyCreateTerrainLayer, material, create_terrain_layer, "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialAssignCurrentToSelection, material, assign_current_to_selection,
                                     "Assigns the current material to the selection.",
                                     "material.assign_current_to_selection()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialResetSelection, material, reset_selection,
                                     "Resets the materials of the selection.",
                                     "material.reset_selection()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialSelectObjectsWithCurrent, material, select_objects_with_current,
                                     "Selects the objects which have the current material.",
                                     "material.select_objects_with_current()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaterialSetCurrentFromObject, material, set_current_from_object,
                                     "Sets the current material to the material of a selected object.",
                                     "material.set_current_from_object()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetSubMaterial, material, get_submaterial,
                                     "Gets sub materials of an material.",
                                     "material.get_submaterial()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetProperty, material, get_property,
                                          "Gets a property of a material.",
                                          "material.get_property(str materialPath/materialName, str propertyPath/propertyName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetProperty, material, set_property,
                                          "Sets a property of a material.",
                                          "material.set_property(str materialPath/materialName, str propertyPath/propertyName, [ str | (int, int, int) | (float, float, float) | int | float | bool ] value)");

