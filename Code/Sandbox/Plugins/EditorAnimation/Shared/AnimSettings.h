// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <map>

#include <CrySerialization/BlackBox.h>

struct ICharacterInstance;
class ICryXML;
struct IPakSystem;

#include <CrySerialization/Forward.h>

struct SkeletonAlias
{
	string& alias;

	SkeletonAlias(string& alias) : alias(alias) {}
};

using std::vector;

enum EControllerEnabledState
{
	eCES_ForceDelete,
	eCES_ForceKeep,
	eCES_UseEpsilons,
};

enum { SERIALIZE_COMPRESSION_SETTINGS_AS_TREE = 1 << 31 };

struct SControllerCompressionSettings
{
	EControllerEnabledState state;
	float                   multiply;

	SControllerCompressionSettings()
		: state(eCES_UseEpsilons)
		, multiply(1.f)
	{
	}

	bool operator==(const SControllerCompressionSettings& rhs) const
	{
		return state == rhs.state && fabsf(multiply - rhs.multiply) < 0.00001f;
	}

	bool operator!=(const SControllerCompressionSettings& rhs) const
	{
		return !(*this == rhs);
	}

	void Serialize(Serialization::IArchive& ar);
};

struct SCompressionSettings
{
	float m_positionEpsilon;
	float m_rotationEpsilon;
	float m_scaleEpsilon;
	int   m_compressionValue;

	// This flag is needed to keep old RC behavior with missing
	// "CompressionSettings" xml node. In this case it uses default settings in
	// new format that has different representation.
	bool m_useNewFormatWithDefaultSettings;
	bool m_usesNameContainsInPerBoneSettings;

	typedef std::vector<std::pair<string, SControllerCompressionSettings>> TControllerSettings;
	TControllerSettings m_controllerCompressionSettings;

	SCompressionSettings();

	void                                  SetZeroCompression();
	void                                  InitializeForCharacter(ICharacterInstance* pCharacter);
	void                                  SetControllerCompressionSettings(const char* controllerName, const SControllerCompressionSettings& settings);
	const SControllerCompressionSettings& GetControllerCompressionSettings(const char* controllerName) const;

	void GetControllerNames(std::vector<string>& controllerNamesOut) const;

	void Serialize(Serialization::IArchive& ar);
};

struct SAnimationBuildSettings
{
	string               skeletonAlias;
	bool                 additive;
	SCompressionSettings compression;
	std::vector<string>  tags;

	SAnimationBuildSettings()
		: additive(false)
	{
	}

	void Serialize(Serialization::IArchive& ar);
};

struct SAnimSettings
{
	SAnimationBuildSettings build;

	void          Serialize(Serialization::IArchive& ar);
	bool          Load(const char* filename, const std::vector<string>& jointNames, ICryXML* cryXml, IPakSystem* pakSystem);
	bool          LoadXMLFromMemory(const char* data, size_t length, const std::vector<string>& jointNames, ICryXML* cryXml);
	bool          Save(const char* filename) const;
	bool          SaveOutsideBuild(const char* filename) const;

	static string GetAnimSettingsFilename(const char* animationPath);
	static string GetIntermediateFilename(const char* animationPath);
	static void   SplitTagString(std::vector<string>* tags, const char* str);
};

