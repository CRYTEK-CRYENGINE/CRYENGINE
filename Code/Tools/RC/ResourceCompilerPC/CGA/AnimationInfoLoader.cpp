// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AnimationInfoLoader.h"
#include "../../../CryXML/ICryXML.h"
#include "../../../CryXML/IXMLSerializer.h"

#include "PakXmlFileBufferSource.h"
#include "FileUtil.h"
#include "PathHelpers.h"
#include "StringHelpers.h"
#include "Util.h"
#include "Plugins/EditorAnimation/Shared/AnimSettings.h"

#include <cstring>     // strchr()


string SAnimationDefinition::s_OverrideAnimationSettingsFilename;


static int GetNodeIDFromName(const char * name, const XmlNodeRef& node)
{
	uint32 numChildren= node->getChildCount();

	for (uint32 child = 0; child < numChildren; ++child)
	{
		if (stricmp(name, node->getChild(child)->getTag()) == 0)
		{
			return child;
		}
	}

	return -1;
}


static bool ParseDeleteValue(SBoneCompressionValues::EDelete& result, const char* pValue)
{
	if (!pValue || !pValue[0] || StringHelpers::EqualsIgnoreCase(pValue, "AutoDelete"))
	{
		result = SBoneCompressionValues::eDelete_Auto;
	}
	else if (StringHelpers::EqualsIgnoreCase(pValue, "DontDelete"))
	{
		result = SBoneCompressionValues::eDelete_No;
	}
	else if (StringHelpers::EqualsIgnoreCase(pValue, "Delete"))
	{
		result = SBoneCompressionValues::eDelete_Yes;
	}
	else
	{
		result = SBoneCompressionValues::eDelete_Auto;
		return false;
	}

	return true;
}


void SAnimationDesc::LoadBoneSettingsFromXML(const XmlNodeRef& pRoot)
{
	m_perBoneCompressionDesc.clear();

	const uint32 id = GetNodeIDFromName("PerBoneCompression", pRoot);
	if (id == -1)
	{
		return;
	}

	const XmlNodeRef node = pRoot->getChild(id);

	const uint32 count = node ? node->getChildCount() : 0;
	for (uint32 i = 0; i < count; ++i)
	{
		XmlNodeRef boneNode = node->getChild(i);
		if (!boneNode)
		{
			continue;
		}

		const char* name = boneNode->getAttr("Name");
		const char* nameContains = boneNode->getAttr("NameContains");
		if (nameContains[0] == '\0' && name[0] == '\0')
		{
			RCLogError("AnimationInfoLoader: PerBoneCompression element must contain either non-empty \"Name\" or non-empty \"NameContains\"");
		}

		SBoneCompressionDesc boneDesc;

		if (name[0] != '\0')
		{
			boneDesc.m_namePattern = name;
		}
		else if (nameContains[0] == '\0')
		{
			boneDesc.m_namePattern = "*";
		}
		else if (strchr(nameContains, '*') || strchr(nameContains, '?'))
		{
			boneDesc.m_namePattern = nameContains;
		}
		else
		{
			boneDesc.m_namePattern = string("*") + string(nameContains) + string("*");
		}

		if (m_bNewFormat)
		{
			const char* autodeletePos = boneNode->getAttr("autodeletePos");
			const char* autodeleteRot = boneNode->getAttr("autodeleteRot");
			const char* autodeleteScl = boneNode->getAttr("autodeleteScl");

			const char* compressPosEps = boneNode->getAttr("compressPosEps");
			const char* compressRotEps = boneNode->getAttr("compressRotEps");
			const char* compressSclEps = boneNode->getAttr("compressSclEps");

			if (!autodeletePos || !autodeleteRot || !compressPosEps || !compressRotEps)
			{
				RCLogError("PerBoneCompression must contain \"autodeletePos\", \"autodeleteRot\", \"compressPosEps\", \"compressRotEps\"");
			}
			
			// Reasonable scaling defaults.
			autodeleteScl = autodeleteScl ? autodeleteScl : "AutoDelete";
			compressSclEps = compressSclEps ? compressSclEps : "1.0e-5";

			boneDesc.newFmt.m_compressPosEps = float(atof(compressPosEps));
			boneDesc.newFmt.m_compressRotEps = float(atof(compressRotEps));
			boneDesc.newFmt.m_compressSclEps = float(atof(compressSclEps));

			if (!ParseDeleteValue(boneDesc.m_eDeletePos, autodeletePos) ||
				!ParseDeleteValue(boneDesc.m_eDeleteRot, autodeleteRot) ||
				!ParseDeleteValue(boneDesc.m_eDeleteScl, autodeleteScl))
			{
				continue;
			}
		}
		else
		{
			const char* mult = boneNode->getAttr("Multiply");
			const char* remove = boneNode->getAttr("Delete");

			if (!mult && !remove)
			{
				RCLogError("AnimationInfoLoader: PerBoneCompression element must contain \"Multiply\" and/or \"Delete\"");
			}

			if (mult && mult[0])
			{
				boneDesc.oldFmt.m_mult = float(atof(mult));
			}
			else
			{
				boneDesc.oldFmt.m_mult = 1.0f;
			}

			const SBoneCompressionValues::EDelete boneDeletionFlag = (atoi(remove) != 0) ? SBoneCompressionValues::eDelete_Yes : SBoneCompressionValues::eDelete_Auto;
			boneDesc.m_eDeletePos = boneDeletionFlag;
			boneDesc.m_eDeleteRot = boneDeletionFlag;
			boneDesc.m_eDeleteScl = boneDeletionFlag;
		}

		m_perBoneCompressionDesc.push_back(boneDesc);
	}	
}


SBoneCompressionValues SAnimationDesc::GetBoneCompressionValues(const char* boneName, float platformMultiplier) const
{
	SBoneCompressionValues bc;

	bc.m_eAutodeletePos = SBoneCompressionValues::eDelete_Auto;
	bc.m_eAutodeleteRot = SBoneCompressionValues::eDelete_Auto;
	bc.m_eAutodeleteScl = SBoneCompressionValues::eDelete_Auto;

	float posErr;
	float rotErr;
	float sclErr;

	if (m_bNewFormat)
	{
		bc.m_autodeletePosEps = newFmt.m_autodeletePosEps;
		bc.m_autodeleteRotEps = newFmt.m_autodeleteRotEps;
		bc.m_autodeleteSclEps = newFmt.m_autodeleteSclEps;

		posErr = 0.0f;
		rotErr = 0.0f;
		sclErr = 0.0f;
	}
	else
	{
		bc.m_autodeletePosEps = oldFmt.m_fPOS_EPSILON;
		bc.m_autodeleteRotEps = oldFmt.m_fROT_EPSILON;
		bc.m_autodeleteSclEps = oldFmt.m_fSCL_EPSILON;

		posErr = 0.00000001f * (float)oldFmt.m_CompressionQuality;
		rotErr = 0.00000001f * (float)oldFmt.m_CompressionQuality;
		sclErr = 0.00000001f * (float)oldFmt.m_CompressionQuality;
	}

	if (boneName)
	{
		const size_t count = m_perBoneCompressionDesc.size();
		for (size_t i = 0; i < count; ++i)
		{
			const SBoneCompressionDesc& desc = m_perBoneCompressionDesc[i];

			if (StringHelpers::MatchesWildcardsIgnoreCase(boneName, desc.m_namePattern))
			{
				bc.m_eAutodeletePos = desc.m_eDeletePos;
				bc.m_eAutodeleteRot = desc.m_eDeleteRot;
				bc.m_eAutodeleteScl = desc.m_eDeleteScl;

				if (m_bNewFormat)
				{
					posErr = desc.newFmt.m_compressPosEps * newFmt.m_compressPosMul;
					rotErr = desc.newFmt.m_compressRotEps * newFmt.m_compressRotMul;
					sclErr = desc.newFmt.m_compressSclEps * newFmt.m_compressSclMul;
				}
				else
				{
					posErr *= desc.oldFmt.m_mult;
					rotErr *= desc.oldFmt.m_mult;
					sclErr *= desc.oldFmt.m_mult;
				}

				break;
			}
		}
	}

	posErr *= platformMultiplier;
	rotErr *= platformMultiplier;
	sclErr *= platformMultiplier;

	if (!m_bNewFormat)
	{
		// squared distance -> distance
		posErr = sqrtf(Util::getMax(posErr, 0.0f));

		// old error tolerance value -> tolerance in degrees
		const float angleInRadians = 2 * sqrtf(Util::getMax(rotErr, 0.0f));
		const float angleInDegrees = Util::getClamped(RAD2DEG(angleInRadians), 0.0f, 180.0f);
		rotErr = angleInDegrees;
	}

	bc.m_compressPosTolerance = posErr;
	bc.m_compressRotToleranceInDegrees = rotErr;
	bc.m_compressSclTolerance = sclErr;

	return bc;
}


const SAnimationDesc& SAnimationDefinition::GetAnimationDesc(const string& name) const
{
	string searchname(name);
	UnifyPath(searchname);

	uint32 numOverAnims = m_OverrideAnimations.size();
	for (uint32 i=0; i<numOverAnims; ++i)
	{
		string mapname(m_OverrideAnimations[i].GetAnimString());
		if ((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0))
		{
			return m_OverrideAnimations[i];
		}
	}

	return m_MainDesc;
}


void SAnimationDefinition::SetOverrideAnimationSettingsFilename(const string& animationSettingsFilename)
{
	s_OverrideAnimationSettingsFilename = animationSettingsFilename;
}


string SAnimationDefinition::GetAnimationSettingsFilename(const string& animationFilename, EPreferredAnimationSettingsFile flag)
{
	if (flag == ePASF_OVERRIDE_FILE && !s_OverrideAnimationSettingsFilename.empty())
	{
		return s_OverrideAnimationSettingsFilename;
	}

	const char* const ext = 
		(flag == ePASF_UPTODATECHECK_FILE)
		? ".$animsettings"
		: ".animsettings";

	return PathUtil::ReplaceExtension(animationFilename, ext);
}


bool SAnimationDefinition::GetDescFromAnimationSettingsFile(SAnimationDesc* desc, bool* errorReported, bool* usesNameContains, IPakSystem* pPakSystem, ICryXML* pXmlParser, const string& animationFilename, const std::vector<string>& jointNames)
{
	if (pXmlParser == NULL || pPakSystem == NULL)
	{
		return false;
	}

	const string animationSettingsFilename = GetAnimationSettingsFilename(animationFilename, ePASF_OVERRIDE_FILE);

	if (!FileUtil::FileExists(animationSettingsFilename.c_str()))
	{
		return false;
	}

	SAnimSettings animSettings;
	if (!animSettings.Load(animationSettingsFilename, jointNames, pXmlParser, pPakSystem))
	{
		RCLogError("Failed to load animsettings: %s", animationSettingsFilename.c_str());
		return false;
	}

	if (usesNameContains)
		*usesNameContains = animSettings.build.compression.m_usesNameContainsInPerBoneSettings;

	*desc = SAnimationDesc();
	{
		// The skip SkipSaveToDatabase value makes no sense in the context of the animation settings file.
		// Since we're creating an SAnimationDesc structure from the settings file and that information is simply not there,
		// but it's needed to fully initialize the SAnimationDesc structure properly, we use the value incoming from the
		// defaultDesc description instead.
		// TODO: Remove this, it should have no meaning!
		desc->m_bSkipSaveToDatabase = false;
	}

	desc->m_bAdditiveAnimation = animSettings.build.additive;
	desc->m_skeletonName = animSettings.build.skeletonAlias;


	if (animSettings.build.compression.m_useNewFormatWithDefaultSettings)
	{
		// This case is used only for compatibility with old AnimSettings that miss
		// CompressionSettings block.
		desc->m_bNewFormat = true;
		desc->newFmt.m_autodeletePosEps = 0.0f;
		desc->newFmt.m_autodeleteRotEps = 0.0f;
		desc->newFmt.m_autodeleteSclEps = 0.0f;
		desc->newFmt.m_compressPosMul = 1.0f;
		desc->newFmt.m_compressRotMul = 1.0f;
		desc->newFmt.m_compressSclMul = 1.0f;
	}
	else
	{
		desc->m_bNewFormat = false;
		desc->oldFmt.m_CompressionQuality = animSettings.build.compression.m_compressionValue;
		desc->oldFmt.m_fROT_EPSILON = animSettings.build.compression.m_rotationEpsilon;
		desc->oldFmt.m_fPOS_EPSILON = animSettings.build.compression.m_positionEpsilon;
		desc->oldFmt.m_fSCL_EPSILON = animSettings.build.compression.m_scaleEpsilon;
	}
	desc->m_tags = animSettings.build.tags;

	const SCompressionSettings::TControllerSettings& controllers = animSettings.build.compression.m_controllerCompressionSettings;
	for (SCompressionSettings::TControllerSettings::const_iterator it = controllers.begin();it != controllers.end(); ++it)
	{
		const char* controllerName = it->first.c_str();
		const SControllerCompressionSettings& controller = it->second;
		const SBoneCompressionValues::EDelete boneDeletionFlag = // TODO: This functionality is also implemented in CAnimationCompiler, we might want to provide a conversion function between these two enum types.
			(controller.state == eCES_ForceDelete) ? SBoneCompressionValues::eDelete_Yes :
			(controller.state == eCES_ForceKeep) ? SBoneCompressionValues::eDelete_No : 
			SBoneCompressionValues::eDelete_Auto;

		SBoneCompressionDesc boneDesc;
		boneDesc.m_namePattern = controllerName;
		boneDesc.m_eDeletePos = boneDeletionFlag;
		boneDesc.m_eDeleteRot = boneDeletionFlag;
		boneDesc.m_eDeleteScl = boneDeletionFlag;
		boneDesc.oldFmt.m_mult = controller.multiply;
		desc->m_perBoneCompressionDesc.push_back(boneDesc);
	}

	return true;
}


bool SAnimationDefinition::FindIdentical(const string& name, bool checkLen )
{
	string searchname(name);
	UnifyPath(searchname);

	uint32 numOverAnims = m_OverrideAnimations.size();
	for (uint32 i=0; i<numOverAnims; ++i)
	{
		string mapname(m_OverrideAnimations[i].GetAnimString());
		if ((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0))
		{
			if (checkLen)
			{
				if (mapname.length() == searchname.length())
					return true;
			}
			else
				return true;
		}
	}

	return false;
}
