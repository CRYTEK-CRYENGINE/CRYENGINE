// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimSettings.h"
#include "Serialization.h"
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CryCore/Assert/CryAssert.h>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryString/CryPath.h>

#ifdef RESOURCE_COMPILER
	#include "../../../Tools/CryXml/ICryXML.h"
	#include "../../../Tools/CryXml/IXMLSerializer.h"
	#include "../../../Tools/CryCommonTools/PakXmlFileBufferSource.h"
#endif

#include <CryCore/Platform/CryWindows.h>

using std::vector;
using std::pair;

// ---------------------------------------------------------------------------

namespace
{
template<typename T>
bool ReadValueFromXmlChildNode(XmlNodeRef parentNode, const char* childNodeName, T& valueOut)
{
	assert(parentNode);
	assert(childNodeName);

	XmlNodeRef childNode = parentNode->findChild(childNodeName);
	if (!childNode)
	{
		return false;
	}

	return childNode->getAttr("value", valueOut);
}

bool ReadValueFromXmlChildNode(XmlNodeRef parentNode, const char* childNodeName, string& valueOut)
{
	assert(parentNode);
	assert(childNodeName);

	XmlNodeRef childNode = parentNode->findChild(childNodeName);
	if (!childNode)
	{
		return false;
	}

	const char* str = "";
	if (!childNode->getAttr("value", &str))
		return false;

	valueOut = str;
	return true;
}

}

string SAnimSettings::GetAnimSettingsFilename(const char* animationPath)
{
	return PathUtil::ReplaceExtension(animationPath, "animsettings");
}

string SAnimSettings::GetIntermediateFilename(const char* animationPath)
{
	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameFolder());
	const string animationFilePath = gameFolderPath + animationPath;

	const string animSettingsFilename = PathUtil::ReplaceExtension(animationFilePath, "i_caf");
	return animSettingsFilename;
}

bool SAnimSettings::Save(const char* filename) const
{
	string outputFilename = string(gEnv->pCryPak->GetGameFolder()) + "\\" + filename;
	return SaveOutsideBuild(outputFilename.c_str());
}

bool SAnimSettings::SaveOutsideBuild(const char* filename) const
{
	yasli::JSONOArchive oa;
	oa(*this);
	return oa.save(filename);
}

static string TrimString(const char* start, const char* end)
{
	while (start != end && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n'))
		++start;
	while (end != start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n'))
		--end;
	return string(start, end);
}

void SAnimSettings::SplitTagString(vector<string>* tags, const char* str)
{
	if (!tags)
		return;
	if (!str)
		return;

	tags->clear();
	if (str[0] == '\0')
		return;

	const char* tag_start = str;
	for (const char* p = str; *p != '\0'; ++p)
	{
		if (*p == ',')
		{
			tags->push_back(TrimString(tag_start, p));
			tag_start = p + 1;
		}
	}
	tags->push_back(TrimString(tag_start, str + strlen(str)));
}

static bool DetectXML(const char* text, size_t size)
{
	const char* end = text + size;
	while (text != end && (*text == ' ' || *text == '\r' || *text == '\n' || *text == '\t'))
		++text;
	return text != end && *text == '<';
}

static bool LoadFile(vector<char>* buffer, const char* filename, IPakSystem* pakSystem)
{
#ifdef RESOURCE_COMPILER
	PakSystemFile* f = pakSystem->Open(filename, "rb");
	if (!f)
		return false;

	size_t size = pakSystem->GetLength(f);
	buffer->resize(size);
	bool result = pakSystem->Read(f, &(*buffer)[0], size) == size;
	pakSystem->Close(f);
#else
	FILE* f = gEnv->pCryPak->FOpen(filename, "rb");
	if (!f)
		return false;

	gEnv->pCryPak->FSeek(f, 0, SEEK_END);
	size_t size = gEnv->pCryPak->FTell(f);
	gEnv->pCryPak->FSeek(f, 0, SEEK_SET);

	buffer->resize(size);
	bool result = gEnv->pCryPak->FRead(&(*buffer)[0], size, f) == size;
	gEnv->pCryPak->FClose(f);
#endif
	return result;
}

bool SAnimSettings::Load(const char* filename, const vector<string>& jointNames, ICryXML* cryXml, IPakSystem* pakSystem)
{
	vector<char> content;
	if (!LoadFile(&content, filename, pakSystem))
		return false;

	if (content.empty())
		return true;

	if (DetectXML(&content[0], content.size()))
	{
		return LoadXMLFromMemory(&content[0], content.size(), jointNames, cryXml);
	}
	else
	{
		yasli::JSONIArchive ia;
		if (!ia.open(&content[0], content.size()))
			return false;

		ia(*this);
		return true;
	}
}

bool SAnimSettings::LoadXMLFromMemory(const char* data, size_t length, const vector<string>& jointNames, ICryXML* cryXml)
{
#ifdef RESOURCE_COMPILER
	char error[1024];
	XmlNodeRef xmlRoot = cryXml->GetXMLSerializer()->Read(PakXmlBufferSource(data, length), true, sizeof(error), error);
#else
	XmlNodeRef xmlRoot = GetISystem()->LoadXmlFromBuffer(data, length);
#endif

	if (!xmlRoot)
	{
		return false;
	}

	build = SAnimationBuildSettings();
	SCompressionSettings& compression = build.compression;

	ReadValueFromXmlChildNode(xmlRoot, "AdditiveAnimation", build.additive);
	ReadValueFromXmlChildNode(xmlRoot, "Skeleton", build.skeletonAlias);

	XmlNodeRef xmlCompressionSettings = xmlRoot->findChild("CompressionSettings");
	if (xmlCompressionSettings)
	{
		int version = 0;
		if (ReadValueFromXmlChildNode(xmlCompressionSettings, "Version", version))
		{
			if (version == 0)
			{
				ReadValueFromXmlChildNode(xmlCompressionSettings, "Compression", compression.m_compressionValue);
				ReadValueFromXmlChildNode(xmlCompressionSettings, "RotEpsilon", compression.m_rotationEpsilon);
				ReadValueFromXmlChildNode(xmlCompressionSettings, "PosEpsilon", compression.m_positionEpsilon);
				ReadValueFromXmlChildNode(xmlCompressionSettings, "SclEpsilon", compression.m_scaleEpsilon);
			}
		}

		const SControllerCompressionSettings defaultBoneSettings;

		XmlNodeRef xmlPerBoneCompression = xmlCompressionSettings->findChild("PerBoneCompression");
		if (xmlPerBoneCompression)
		{
			vector<pair<string, SControllerCompressionSettings>> obsoleteEntries;

			const int boneSettingsCount = xmlPerBoneCompression->getChildCount();
			for (int i = 0; i < boneSettingsCount; ++i)
			{
				XmlNodeRef xmlBone = xmlPerBoneCompression->getChild(i);

				SControllerCompressionSettings boneSettings;

				int forceDelete = 0;
				xmlBone->getAttr("Delete", forceDelete);
				if (forceDelete == 1)
				{
					boneSettings.state = eCES_ForceDelete;
				}

				xmlBone->getAttr("Multiply", boneSettings.multiply);
				const char* boneName = xmlBone->getAttr("Name");
				if (boneName[0] == '\0')
				{
					const char* nameContains = xmlBone->getAttr("NameContains");
					if (nameContains[0] != '\0')
					{
						obsoleteEntries.push_back(std::make_pair(string(nameContains), boneSettings));
					}
					continue;
				}

				if (boneSettings != defaultBoneSettings)
				{
					compression.SetControllerCompressionSettings(boneName, boneSettings);
				}
			}

			// As NameContains uses substring matching we replace it with Name (full
			// name) specification, but we still to convert old entries with 'NameContains'
			// we do this by expanding NameContains to full controller names.
			if (!obsoleteEntries.empty())
			{
				if (jointNames.empty())
				{
					compression.m_usesNameContainsInPerBoneSettings = true;
				}
				else
				{
					for (size_t i = 0; i < jointNames.size(); ++i)
					{
						for (size_t j = 0; j < obsoleteEntries.size(); ++j)
						{
							string name = jointNames[i].c_str();
							name.MakeLower();
							string nameContains = obsoleteEntries[j].first.c_str();
							nameContains.MakeLower();
							const SControllerCompressionSettings& boneSettings = obsoleteEntries[j].second;
							if (strstr(name, nameContains) != 0)
							{
								if (boneSettings != defaultBoneSettings)
								{
									compression.SetControllerCompressionSettings(jointNames[i].c_str(), boneSettings);
								}
								break;
							}
						}
					}
				}
				// *pConvertedOut = true;
			}
		}
	}
	else
	{
		compression.m_useNewFormatWithDefaultSettings = true;
	}

	build.tags.clear();
	XmlNodeRef xmlTags = xmlRoot->findChild("Tags");
	if (xmlTags)
	{
		string tagString = xmlTags->getContent();
		SplitTagString(&build.tags, tagString);
	}

	return true;
}

// ---------------------------------------------------------------------------

void SAnimationBuildSettings::Serialize(Serialization::IArchive& ar)
{
	ar(additive, "additive", "Additive");
	ar(SkeletonAlias(skeletonAlias), "skeletonAlias", "Skeleton Alias");
	ar(compression, "compression", "Compression");
	ar(tags, "tags", "Tags");
}

void SAnimSettings::Serialize(Serialization::IArchive& ar)
{
	ar(build, "build", "Build");
}

// ---------------------------------------------------------------------------
SCompressionSettings::SCompressionSettings()
	: m_useNewFormatWithDefaultSettings(false)
	, m_usesNameContainsInPerBoneSettings(false)
{
	m_positionEpsilon = 1.0e-3f;
	m_rotationEpsilon = 1.0e-5f;
	m_scaleEpsilon = 1.0e-5f;
	m_compressionValue = 10;

	m_controllerCompressionSettings.clear();
}

void SCompressionSettings::InitializeForCharacter(ICharacterInstance* pCharacter)
{
	*this = SCompressionSettings();

	if (pCharacter == NULL)
	{
		return;
	}

	ISkeletonPose& skeletonPose = *pCharacter->GetISkeletonPose();
	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();

	const uint32 jointCount = rIDefaultSkeleton.GetJointCount();
	for (uint32 jointId = 0; jointId < jointCount; ++jointId)
	{
		const char* jointName = rIDefaultSkeleton.GetJointNameByID(jointId);
		m_controllerCompressionSettings.push_back(std::make_pair(jointName, SControllerCompressionSettings()));
	}
}

void SCompressionSettings::SetZeroCompression()
{
	m_positionEpsilon = 0.0f;
	m_rotationEpsilon = 0.0f;
	m_scaleEpsilon = 0.0f;
	m_compressionValue = 0;
	m_controllerCompressionSettings.clear();
}

void SCompressionSettings::SetControllerCompressionSettings(const char* controllerName, const SControllerCompressionSettings& settings)
{
	assert(controllerName);
	if (!controllerName)
		return;

	for (size_t i = 0; i < m_controllerCompressionSettings.size(); ++i)
		if (stricmp(m_controllerCompressionSettings[i].first.c_str(), controllerName) == 0)
		{
			m_controllerCompressionSettings[i].second = settings;
			return;
		}

	m_controllerCompressionSettings.push_back(std::make_pair(controllerName, settings));
}

static SControllerCompressionSettings* FindSettings(SCompressionSettings::TControllerSettings& settings, const char* controllerName)
{
	for (size_t i = 0; i < settings.size(); ++i)
	{
		if (stricmp(settings[i].first.c_str(), controllerName) == 0)
			return &settings[i].second;
	}
	return 0;
}

static const SControllerCompressionSettings* FindSettings(const SCompressionSettings::TControllerSettings& settings, const char* controllerName)
{
	for (size_t i = 0; i < settings.size(); ++i)
	{
		if (stricmp(settings[i].first.c_str(), controllerName) == 0)
			return &settings[i].second;
	}
	return 0;
}

const SControllerCompressionSettings& SCompressionSettings::GetControllerCompressionSettings(const char* controllerName) const
{
	assert(controllerName);

	if (controllerName)
	{
		if (const SControllerCompressionSettings* settings = FindSettings(m_controllerCompressionSettings, controllerName))
			return *settings;
	}

	static SControllerCompressionSettings s_defaultSettings;
	return s_defaultSettings;
}

void SCompressionSettings::GetControllerNames(std::vector<string>& controllerNamesOut) const
{
	controllerNamesOut.clear();

	for (size_t i = 0; i < m_controllerCompressionSettings.size(); ++i)
		controllerNamesOut.push_back(m_controllerCompressionSettings[i].first);
}

SERIALIZATION_ENUM_BEGIN(EControllerEnabledState, "Controller Compression")
SERIALIZATION_ENUM(eCES_ForceDelete, "delete", "Delete")
SERIALIZATION_ENUM(eCES_ForceKeep, "keep", "Keep")
SERIALIZATION_ENUM(eCES_UseEpsilons, "useEpsilons", "Auto")
SERIALIZATION_ENUM_END()

void SControllerCompressionSettings::Serialize(Serialization::IArchive& ar)
{
	ar(multiply, "multiply", state == eCES_UseEpsilons ? ">60>^Multiply" : 0);
	if (!ar(state, "state", "^"))
	{
		bool deleteController = false;
		ar(deleteController, "delete", "");
		state = deleteController ? eCES_ForceDelete : eCES_UseEpsilons;
	}
}

struct SPerBoneSettingsTree
{
	struct SNode
	{
		const char*                    name;
		bool                           isRoot;
		vector<std::unique_ptr<SNode>> children;
		SControllerCompressionSettings settings;

		SNode() : name(""), isRoot(false) {}

		void Serialize(Serialization::IArchive& ar)
		{
			string limiter;
			ar(limiter, "limiter", "!^<");

			if (!ar(settings.state, "state", "^>60>"))
			{
				bool deleteController = false;
				ar(deleteController, "delete", "");
				settings.state = deleteController ? eCES_ForceDelete : eCES_UseEpsilons;
			}

			ar(Serialization::Range(settings.multiply, 0.0f, 1000.f), "multiply", settings.state != eCES_ForceDelete ? "^>40> *" : 0);

			for (size_t i = 0; i < children.size(); ++i)
			{
				ar(*children[i], children[i]->name, children[i]->name);
			}
		}
	};

	SCompressionSettings::TControllerSettings* controllerMap;
	IDefaultSkeleton*                          skeleton;

	SPerBoneSettingsTree(SCompressionSettings::TControllerSettings* controllerMap, IDefaultSkeleton* skeleton)
		: controllerMap(controllerMap), skeleton(skeleton)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		SNode root;
		root.name = skeleton->GetJointNameByID(0);
		SControllerCompressionSettings* rootSettings = FindSettings(*controllerMap, root.name);
		if (rootSettings)
			root.settings = *rootSettings;
		root.isRoot = true;

		vector<SNode*> allJoints;
		int numJoints = skeleton->GetJointCount();
		allJoints.resize(numJoints);
		allJoints[0] = &root;
		for (int i = 1; i < numJoints; ++i)
		{
			allJoints[i] = new SNode();
			allJoints[i]->name = skeleton->GetJointNameByID(i);
			SControllerCompressionSettings* controller = FindSettings(*controllerMap, allJoints[i]->name);
			if (controller)
				allJoints[i]->settings = *controller;
		}

		for (int i = 0; i < numJoints; ++i)
		{
			int parentId = skeleton->GetJointParentIDByID(i);
			if (parentId >= 0)
				allJoints[parentId]->children.push_back(std::unique_ptr<SNode>(allJoints[i]));
		}

		ar(root, "root", root.name);

		if (ar.isInput())
		{
			controllerMap->clear();
			for (int i = 0; i < numJoints; ++i)
				if (allJoints[i]->settings != SControllerCompressionSettings())
				{
					SControllerCompressionSettings* controller = FindSettings(*controllerMap, allJoints[i]->name);
					if (controller)
						*controller = allJoints[i]->settings;
					else
						controllerMap->push_back(std::make_pair(allJoints[i]->name, allJoints[i]->settings));
				}
		}
	}
};

struct SEditableJointSettings : std::pair<string, SControllerCompressionSettings>
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(JointName(first), "joint", "^");
		ar(second, "settings", "^");
	}
};

void SCompressionSettings::Serialize(Serialization::IArchive& ar)
{
	using Serialization::Range;
	ar(Range(m_compressionValue, 0, 1000000), "compressionValue", "Compression Value");
	if (ar.openBlock("removalThreshold", "+Controller Auto Removal Threshold"))
	{
		ar(Range(m_positionEpsilon, 0.0f, 0.2f), "positionEpsilon", "Position");
		ar(Range(m_rotationEpsilon, 0.0f, 3.1415926f * 0.25f), "rotationEpsilon", "Rotation");
		ar(Range(m_scaleEpsilon, 0.0f, 0.2f), "scaleEpsilon", "Scale");
		ar.closeBlock();
	}

	if (ar.isEdit())
	{
		IDefaultSkeleton* skeleton = ar.context<IDefaultSkeleton>();
		if (skeleton && ar.getFilter() && ar.filter(SERIALIZE_COMPRESSION_SETTINGS_AS_TREE))
		{
			SPerBoneSettingsTree tree(&m_controllerCompressionSettings, skeleton);
			ar(tree, "perJointSettings", "Per-Joint Settings");
		}
		else
		{
			ar((vector<SEditableJointSettings> &)m_controllerCompressionSettings, "perJointSettings", "Per-Joint Settings");
		}
	}
	else
	{
		ar(m_controllerCompressionSettings, "perJointSettings");
	}
}

