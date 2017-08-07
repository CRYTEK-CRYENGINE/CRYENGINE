// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAttachment.h>
#include <vector>
#include <array>
#include <CrySerialization/Forward.h>

struct ICharacterInstance;

namespace CharacterTool
{

using std::vector;

struct CharacterDefinition;

struct SJointPhysics
{
	enum class EType
	{
		None  = -1, // chosen to match GetPhysInfoProperties_ROPE parameter
		Rope  = 0,
		Cloth = 1,
	};

	enum EPropertySetIndex
	{
		ePropertySetIndex_Alive,
		ePropertySetIndex_Ragdoll,

		ePropertySetIndex_COUNT
	};

	typedef std::array<DynArray<SJointProperty>, ePropertySetIndex_COUNT> JointProperties;

	EType           type;
	JointProperties ropeProperties;
	JointProperties clothProperties;

	SJointPhysics()
		: type(EType::None)
		, ropeProperties()
		, clothProperties()
	{
	}

	void Serialize(Serialization::IArchive& ar);
	void LoadFromXml(const XmlNodeRef& node);
	void SaveToXml(XmlNodeRef node) const;
};

struct CharacterAttachment
{

	enum TransformSpace
	{
		SPACE_CHARACTER,
		SPACE_JOINT
	};

	enum ProxyPurpose
	{
		AUXILIARY,
		CLOTH,
		RAGDOLL
	};

	AttachmentTypes      m_attachmentType;
	string               m_strSocketName;
	TransformSpace       m_positionSpace;
	TransformSpace       m_rotationSpace;
	QuatT                m_characterSpacePosition;
	QuatT                m_jointSpacePosition;
	string               m_strJointName;
	string               m_strGeometryFilepath;
	string               m_strMaterial;
	Vec4                 m_ProxyParams;
	ProxyPurpose         m_ProxyPurpose;
	SimulationParams     m_simulationParams;

	string               m_strRowJointName;
	RowSimulationParams  m_rowSimulationParams;

	SVClothParams        m_vclothParams;
	float                m_viewDistanceMultiplier;
	int                  m_nFlags;

	SJointPhysics        m_jointPhysics;

	CharacterDefinition* m_definition;

	CharacterAttachment()
		: m_attachmentType(CA_BONE)
		, m_positionSpace(SPACE_JOINT)
		, m_rotationSpace(SPACE_JOINT)
		, m_characterSpacePosition(IDENTITY)
		, m_jointSpacePosition(IDENTITY)
		, m_ProxyParams(0, 0, 0, 0)
		, m_ProxyPurpose(AUXILIARY)
		, m_viewDistanceMultiplier(1.0f)
		, m_nFlags(0)
		, m_definition()
	{
	}

	void Serialize(Serialization::IArchive& ar);
};

struct ICryAnimation;

struct CharacterDefinition
{
	bool                        m_initialized;
	string                      skeleton;
	string                      materialPath;
	string                      physics;
	string                      rig;
	vector<CharacterAttachment> attachments;
	DynArray<string>            m_arrAllProcFunctions;  //all procedural functions

	IAnimationPoseModifierSetupPtr modifiers;

	CharacterDefinition()
	{
		m_initialized = false;
	}

	bool        LoadFromXml(const XmlNodeRef& root);
	bool        LoadFromXmlFile(const char* filename);
	void        ApplyToCharacter(bool* skinSetChanged, ICharacterInstance* character, ICharacterManager* cryAnimation, bool showDebug);
	void        ApplyBoneAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, ICharacterInstance* pICharacterInstance, bool showDebug) const;
	void        ApplyFaceAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, bool showDebug) const;
	void        ApplySkinAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, ICharacterInstance* pICharacterInstance, bool* skinChanged) const;

	void        SynchModifiers(ICharacterInstance& character);

	void        Serialize(Serialization::IArchive& ar);
	bool        Save(const char* filename);
	XmlNodeRef  SaveToXml();
	static void ExportBoneAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportFaceAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportSkinAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportProxyAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportPClothAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportVClothAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments);
	static void ExportSimulation(const CharacterAttachment& attach, XmlNodeRef nodeAttach);

	bool        SaveToMemory(vector<char>* buffer);
};

}
