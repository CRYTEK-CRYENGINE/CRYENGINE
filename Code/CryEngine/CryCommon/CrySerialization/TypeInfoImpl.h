// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryTypeInfo.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Enum.h>
#include <CryMath/ISplines.h>
#include <CryString/StringUtils.h>
#include <CryParticleSystem/ParticleParams.h>
#include <CrySerialization/Color.h>
#include <CryMath/Cry_Color.h>
#include "Decorators/Resources.h"

struct SPrivateTypeInfoInstanceLevel
{
	SPrivateTypeInfoInstanceLevel(const CTypeInfo* typeInfo, void* object, STypeInfoInstance* instance)
		: m_pTypeInfo(typeInfo)
		, m_pObject(object)
		, m_instance(instance)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ForAllSubVars(pVar, *m_pTypeInfo)
		{
			string group;
			if (pVar->GetAttr("Group", group))
			{
				if (!m_sCurrentGroup.empty())
				{
					ar.closeBlock();
				}
				const char* name = m_instance->m_persistentStrings.insert(group).first->c_str();
				ar.openBlock(name, name);
				m_sCurrentGroup = name;
			}
			else
			{
				const char* name = pVar->GetName();
				if (!*name)
				{
					name = pVar->Type.Name;
				}

				const char* label = pVar->GetName();
				if (!*label)
				{
					string n = "^";
					n += pVar->GetName();
					label = m_instance->m_persistentStrings.insert(n).first->c_str();
				}

				SerializeVariable(pVar, m_pObject, ar, name, label);
			}
		}

		if (!m_sCurrentGroup.empty())
		{
			ar.closeBlock();
			m_sCurrentGroup.clear();
		}
	}

	template<class T>
	void SerializeT(const CTypeInfo::CVarInfo* pVar, void* pParentAddress, Serialization::IArchive& ar, const char* name, const char* label)
	{
		T value;
		const CTypeInfo& type = pVar->Type;
		type.ToValue(pVar->GetAddress(pParentAddress), value);
		ar(value, name, label);
		if (ar.isInput())
		{
			type.FromValue(pVar->GetAddress(pParentAddress), value);
		}
	}

	template<class T>
	void SerializeNumericalT(const CTypeInfo::CVarInfo* pVar, void* pParentAddress, Serialization::IArchive& ar, const char* name, const char* label)
	{
		T value;
		const CTypeInfo& type = pVar->Type;
		type.ToValue(pVar->GetAddress(pParentAddress), value);

		float limMin, limMax;
		if (pVar->GetLimit(eLimit_Min, limMin) && pVar->GetLimit(eLimit_Max, limMax))
		{
			ar(Serialization::Range<T>(value, limMin, limMax), name, label);
		}
		else
		{
			ar(value, name, label);
		}
		if (ar.isInput())
		{
			type.FromValue(pVar->GetAddress(pParentAddress), value);
		}
	}

	void SerializeVariable(const CTypeInfo::CVarInfo* pVar, void* pParentAddress, Serialization::IArchive& ar, const char* name, const char* label)
	{
		const CTypeInfo& type = pVar->Type;

		if (type.HasSubVars())
		{
			if (strcmp(name, "Color") == 0)
			{
				Color3F value;
				const CTypeInfo& type = pVar->Type;
				type.ToValue(pVar->GetAddress(pParentAddress), value);
				ColorF colour = value;
				ar(colour, name, label);
				if (ar.isInput())
				{
					value = Color3F(colour.r, colour.g, colour.b);
					type.FromValue(pVar->GetAddress(pParentAddress), value);
				}
			}
			else
			{
				// load params of sub-variables (variable is a struct or vector)
				SPrivateTypeInfoInstanceLevel instance(&type, pVar->GetAddress(pParentAddress), m_instance);
				ar(instance, name, label);
			}
		}
		else
		{
			if (type.IsType<bool>())
			{
				SerializeT<bool>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.IsType<unsigned char>())
			{
				SerializeNumericalT<unsigned char>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.IsType<char>())
			{
				SerializeNumericalT<char>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.IsType<int>())
			{
				SerializeNumericalT<int>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.IsType<uint>())
			{
				SerializeNumericalT<uint>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.IsType<float>())
			{
				SerializeNumericalT<float>(pVar, pParentAddress, ar, name, label);
			}
			else if (type.EnumElem(0))
			{
				Serialization::StringList stringList;
				const char* enumType = type.EnumElem(0);
				for (int i = 1; enumType; ++i)
				{
					stringList.push_back(enumType);
					enumType = type.EnumElem(i);
				}

				string enumValue = pVar->ToString(pParentAddress);
				int index = std::max(stringList.find(enumValue.c_str()), 0);

				Serialization::StringListValue stringListValue(stringList, index);
				ar(stringListValue, name, label);
				if (ar.isInput())
				{
					pVar->FromString(pParentAddress, stringListValue.c_str());
				}
			}
			else
			{
				ISplineInterpolator* pSpline = 0;
				if (type.ToValue(pVar->GetAddress(pParentAddress), pSpline))
				{
					// TODO: Curve field
				}
				else
				{
					string value;
					value = pVar->ToString(pParentAddress);

					if (strcmp(name, "Texture") == 0)
					{
						ar(Serialization::TextureFilename(value), name, label);
					}
					else if (strcmp(name, "Material") == 0)
					{
						ar(Serialization::MaterialPicker(value), name, label);
					}
					else if (strcmp(name, "Geometry") == 0)
					{
						ar(Serialization::ModelFilename(value), name, label);
					}
					else if (strcmp(name, "Sound") == 0)
					{
						ar(Serialization::SoundName(value), name, label);
					}
					else if (strcmp(name, "GeomCache") == 0)
					{
						ar(Serialization::GeomCachePicker(value), name, label);
					}
					else
					{
						ar(value, name, label);
					}
					if (ar.isInput())
					{
						pVar->FromString(pParentAddress, value.c_str());
					}
				}
			}
		}
	}

private:
	const CTypeInfo*   m_pTypeInfo;
	void*              m_pObject;
	string             m_sCurrentGroup;
	STypeInfoInstance* m_instance;
};

inline void STypeInfoInstance::Serialize(Serialization::IArchive& ar)
{
	SPrivateTypeInfoInstanceLevel instance(m_pTypeInfo, m_pObject, this);
	instance.Serialize(ar);
}
