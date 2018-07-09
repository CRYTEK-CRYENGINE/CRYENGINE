// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include <CrySerialization/StringList.h>

//! Serializable object that is used to translate the CMaterial into a property-tree UI
class CMaterialSerializer : public _reference_target_t
{
public:
	CMaterialSerializer(CMaterial* pMaterial, bool readOnly);

	void Serialize(Serialization::IArchive& ar);


	QWidget* CreatePropertyTree();

	//Shader param info
	struct SShaderParamInfo
	{
		SShaderParamInfo()
			: bHasMin(false)
			, bHasMax(false)
			, bHasStep(false)
			, m_min(FLT_MIN)
			, m_max(FLT_MAX)
			, m_step(0.1f)
		{}

		int8 bHasMin : 1;
		int8 bHasMax : 1;
		int8 bHasStep : 1;
		float m_min;
		float m_max;
		float m_step;
		string m_description;
		string m_uiName;
	};

private:

	class CMaterialPropertyTree;

	//////////////////////////////////////////////////////////////////////////

	void SerializeTextureSlots(Serialization::IArchive& ar, bool& bHasDetailDecalOut);
	void SerializeShaderParams(Serialization::IArchive& ar, bool bShaderChanged);
	void SerializeShaderGenParams(Serialization::IArchive& ar);

	SShaderParamInfo ParseShaderParamScript(const SShaderParam& param);

	void PopulateStringLists();

	//////////////////////////////////////////////////////////////////////////

	_smart_ptr<CMaterial> m_pMaterial;
	bool m_bIsReadOnly;

	//String lists used in serialization, refreshed only on creation
	Serialization::StringList m_shaderStringList;
	Serialization::StringList m_shaderStringListWithEmpty;
	Serialization::StringList m_surfaceTypeStringList;
	Serialization::StringList m_textureTypeStringList;

	//Strings saved because the property tree cannot use temporary strings
	string m_textureSlotLabels[EFTT_MAX];

	//Cache of shader parameters infos
	std::map<string, SShaderParamInfo> m_shaderParamInfoCache;

	bool m_bIsBeingChanged;
};

