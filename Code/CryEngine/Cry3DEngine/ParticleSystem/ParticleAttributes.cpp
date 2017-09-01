// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySerialization/Decorators/ActionButton.h>
#include "ParticleCommon.h"
#include "ParticleAttributes.h"
#include "ParticleMath.h"

namespace pfx2
{



struct SAttributeType
{
	virtual void Serialize(Serialization::IArchive& ar, CAttributeInstance* pAttributes, uint attributeIndex) = 0;
	virtual void Reset(CAttributeInstance* pAttributes, uint attributeIndex) = 0;
};



SAttributeEdit::SAttributeEdit(CAttributeInstance* pAttributes, SAttributeType* pTypeHandler, uint attributeIndex)
	: m_pTypeHandler(pTypeHandler)
	, m_pAttributes(pAttributes)
	, m_attributeIndex(attributeIndex)
{
}



void SAttributeEdit::Serialize(Serialization::IArchive& ar)
{
	m_pTypeHandler->Serialize(ar, m_pAttributes, m_attributeIndex);
}



void SAttributeEdit::Reset()
{
	m_pTypeHandler->Reset(m_pAttributes, m_attributeIndex);
}



namespace
{
	const uint numTypes = IParticleAttributes::ET_Count;
	typedef SAttributeValue(*TAttribConverterFn)(SAttributeValue);



	SAttributeValue ConvertAttrib_Bypass(SAttributeValue data)
	{
		return data;
	}

	SAttributeValue ConvertAttrib_BoolToInt(SAttributeValue data)
	{
		data.m_asInt = data.m_asBool ? 1 : 0;
		return data;
	}
	SAttributeValue ConvertAttrib_BoolToFloat(SAttributeValue data)
	{
		data.m_asFloat = data.m_asBool ? 1.0f : 0.0f;
		return data;
	}
	SAttributeValue ConvertAttrib_BoolToColor(SAttributeValue data)
	{
		data.m_asColor.r = data.m_asColor.g = data.m_asColor.b = data.m_asBool ? 0xff : 0x00;
		data.m_asColor.a = 0xff;
		return data;
	}

	SAttributeValue ConvertAttrib_IntToBool(SAttributeValue data)
	{
		data.m_asBool = data.m_asInt > 0 ? true : false;
		return data;
	}
	SAttributeValue ConvertAttrib_IntToFloat(SAttributeValue data)
	{
		data.m_asFloat = float(data.m_asInt);
		return data;
	}
	SAttributeValue ConvertAttrib_IntToColor(SAttributeValue data)
	{
		data.m_asColor.r = data.m_asColor.g = data.m_asColor.b = uint8(CLAMP(data.m_asInt, 0, 0xff));
		data.m_asColor.a = 0xff;
		return data;
	}

	SAttributeValue ConvertAttrib_FloatToBool(SAttributeValue data)
	{
		data.m_asBool = data.m_asFloat > 0.0f ? true : false;
		return data;
	}
	SAttributeValue ConvertAttrib_FloatToInt(SAttributeValue data)
	{
		data.m_asInt = int(data.m_asFloat);
		return data;
	}
	SAttributeValue ConvertAttrib_FloatToColor(SAttributeValue data)
	{
		data.m_asColor.r = data.m_asColor.g = data.m_asColor.b = uint8(SATURATE(data.m_asFloat)*255);
		data.m_asColor.a = 0xff;
		return data;
	}

	float Brightness(ColorB color)
	{
		return (color.r * 0.299f + color.g * 0.587f + color.b * 0.114f) / 255.0f;
	}
	SAttributeValue ConvertAttrib_ColorToBool(SAttributeValue data)
	{
		data.m_asBool = Brightness(data.m_asColor) > 0.0f ? true : false;
		return data;
	}
	SAttributeValue ConvertAttrib_ColorToInt(SAttributeValue data)
	{
		data.m_asInt = int(Brightness(data.m_asColor) * 255.0f);
		return data;
	}
	SAttributeValue ConvertAttrib_ColorToFloat(SAttributeValue data)
	{
		data.m_asFloat = Brightness(data.m_asColor);
		return data;
	}

	TAttribConverterFn s_AttributeConverterTable[numTypes][numTypes] =
	{
		{ ConvertAttrib_Bypass      , ConvertAttrib_BoolToInt  , ConvertAttrib_BoolToFloat  , ConvertAttrib_BoolToColor },
		{ ConvertAttrib_IntToBool   , ConvertAttrib_Bypass     , ConvertAttrib_IntToFloat   , ConvertAttrib_IntToColor },
		{ ConvertAttrib_FloatToBool , ConvertAttrib_FloatToInt , ConvertAttrib_Bypass       , ConvertAttrib_FloatToColor },
		{ ConvertAttrib_ColorToBool , ConvertAttrib_ColorToInt , ConvertAttrib_ColorToFloat , ConvertAttrib_Bypass }
	};
	
	template<typename T>
	void SerializeData(Serialization::IArchive& ar, CAttributeInstance* pAttributes, SAttributeEdit* pEdit, uint attributeIndex, T& data)
	{
		SAttributeValue& value = pAttributes->GetValue(attributeIndex);
		auto resetFn = [pEdit]() { pEdit->Reset(); };
		T current = data;
		ar(data, "value", "^");
		if (value.m_edited)
			ar(Serialization::ActionButton(resetFn), "Reset", "^Reset");
		else
			ar(Serialization::ActionButton(resetFn), "Reset", "!^Reset");
		if (ar.isInput() && current != data)
			value.m_edited = true;
		if (!ar.isEdit())
			ar(value.m_edited, "edited");
	}
	
	struct SBooleanAttribute : public SAttributeType
	{
		virtual void Serialize(Serialization::IArchive& ar, CAttributeInstance* pAttributes, uint attributeIndex)
		{
			SerializeData(
				ar, pAttributes, &pAttributes->GetEdit(attributeIndex),
				attributeIndex, pAttributes->GetValue(attributeIndex).m_asBool);
		}
		virtual void Reset(CAttributeInstance* pAttributes, uint attributeIndex)
		{
			const SAttributeDesc& attribute = pAttributes->GetAttributeValue(attributeIndex);
			SAttributeValue& value = pAttributes->GetValue(attributeIndex);
			value.m_asBool = attribute.m_asBoolean;
			value.m_edited = false;
		}
	};

	struct SIntegerAttribute : public SAttributeType
	{
		virtual void Serialize(Serialization::IArchive& ar, CAttributeInstance* pAttributes, uint attributeIndex)
		{
			SerializeData(
				ar, pAttributes, &pAttributes->GetEdit(attributeIndex),
				attributeIndex, pAttributes->GetValue(attributeIndex).m_asInt);
		}
		virtual void Reset(CAttributeInstance* pAttributes, uint attributeIndex)
		{
			const SAttributeDesc& attribute = pAttributes->GetAttributeValue(attributeIndex);
			SAttributeValue& value = pAttributes->GetValue(attributeIndex);
			value.m_asInt = attribute.m_asInt;
			value.m_edited = false;
		}
	};

	struct SFloatAttribute : public SAttributeType
	{
		virtual void Serialize(Serialization::IArchive& ar, CAttributeInstance* pAttributes, uint attributeIndex)
		{
			SerializeData(
				ar, pAttributes, &pAttributes->GetEdit(attributeIndex),
				attributeIndex, pAttributes->GetValue(attributeIndex).m_asFloat);
		}
		virtual void Reset(CAttributeInstance* pAttributes, uint attributeIndex)
		{
			const SAttributeDesc& attribute = pAttributes->GetAttributeValue(attributeIndex);
			SAttributeValue& value = pAttributes->GetValue(attributeIndex);
			value.m_asFloat = attribute.m_asFloat;
			value.m_edited = false;
		}
	};

	struct SColorAttribute : public SAttributeType
	{
		virtual void Serialize(Serialization::IArchive& ar, CAttributeInstance* pAttributes, uint attributeIndex)
		{
			SerializeData(
				ar, pAttributes, &pAttributes->GetEdit(attributeIndex),
				attributeIndex, pAttributes->GetValue(attributeIndex).m_asColor);
		}
		virtual void Reset(CAttributeInstance* pAttributes, uint attributeIndex)
		{
			const SAttributeDesc& attribute = pAttributes->GetAttributeValue(attributeIndex);
			SAttributeValue& value = pAttributes->GetValue(attributeIndex);
			value.m_asColor = attribute.m_asColor;
			value.m_edited = false;
		}
	};



	std::shared_ptr<SAttributeType> s_AttributeTypeHandlers[] =
	{
		std::make_shared<SBooleanAttribute>(),
		std::make_shared<SIntegerAttribute>(),
		std::make_shared<SFloatAttribute>(),
		std::make_shared<SColorAttribute>()
	};

}

void SAttributeDesc::Serialize(Serialization::IArchive& ar)
{
	ar(m_type, "Type", "^>85>");
	ar(m_name, "Name", "^");
	ar(m_scope, "Scope", "Scope");
	switch (m_type)
	{
	case EAttributeType::Boolean:
		ar(m_asBoolean, "Value", "^");
		break;
	case EAttributeType::Integer:
		ar(m_asInt, "Value", "^");
		ar(m_useMin, "UseMin", "Use Min");
		ar(m_useMax, "UseMax", "Use Max");
		if (m_useMin)
			ar(m_minInt, "MinValue", "Minimum Value");
		if (m_useMax)
			ar(m_maxInt, "MaxValue", "Maximum Value");
		break;
	case EAttributeType::Float:
		ar(m_asFloat, "Value", "^");
		ar(m_useMin, "UseMin", "Use Min");
		ar(m_useMax, "UseMax", "Use Max");
		if (m_useMin)
			ar(m_minFloat, "MinValue", "Minimum Value");
		if (m_useMax)
			ar(m_maxFloat, "MaxValue", "Maximum Value");
		break;
	case EAttributeType::Color:
		ar(m_asColor, "Value", "^");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

uint CAttributeTable::FindAttribyteIndexById(uint id) const
{
	auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [id](const SAttributeDesc& attribute)
	{
		return attribute.m_id == id;
	});
	if (it == m_attributes.end())
		return -1;
	return uint(it - m_attributes.begin());
}

void CAttributeTable::Serialize(Serialization::IArchive& ar)
{
	ar(m_attributes, "Attributes", "Attributes");
	if (ar.isInput())
	{
		uint nextId = 0;
		for (auto& attribute : m_attributes)
		{
			if (attribute.m_id != -1)
				nextId = max(nextId, attribute.m_id);
		}
		for (auto& attribute : m_attributes)
		{
			if (attribute.m_id == -1)
				attribute.m_id = ++nextId;
		}
	}
	if (ar.isInput())
		++m_editVersion;
}

//////////////////////////////////////////////////////////////////////////

CAttributeInstance::CAttributeInstance()
{
}

void CAttributeInstance::Reset(const IParticleAttributes* pCopySource)
{
	if (!pCopySource)
	{
		m_attributeIndices.clear();
		m_attributeValues.clear();
		m_attributeIndices.shrink_to_fit();
		m_attributeValues.shrink_to_fit();
		m_pAttributeTable.reset();
	}
	else
	{
		const CAttributeInstance* pCCopySource = static_cast<const CAttributeInstance*>(pCopySource);
		m_attributeIndices = pCCopySource->m_attributeIndices;
		m_attributeValues = pCCopySource->m_attributeValues;
		m_attributeIndices = pCCopySource->m_attributeIndices;
		m_pAttributeTable = pCCopySource->m_pAttributeTable;
	}
}

void CAttributeInstance::Reset(TAttributeTablePtr pTable, EAttributeScope scope)
{
	CRY_PFX2_ASSERT(pTable != nullptr);

	if (m_pAttributeTable.lock() != pTable)
	{
		m_attributeIndices.clear();
		m_attributeValues.clear();
	}
	m_pAttributeTable = pTable;
	if (IsDirty())
		Compile();
}

void CAttributeInstance::Serialize(Serialization::IArchive& ar)
{
	if (IsDirty())
		Compile();
	
	for (uint attributeId = 0; attributeId < m_attributeIndices.size(); ++attributeId)
	{
		const SAttributeDesc& attribute = GetAttributeValue(attributeId);
		const char* name = attribute.m_name.c_str();
		ar(m_attributesEdit[attributeId], name, name);
	}
}

void CAttributeInstance::TransferInto(IParticleAttributes* pReceiver) const
{
	CAttributeInstance* pCReceiver = static_cast<CAttributeInstance*>(pReceiver);
	auto pReceiverTable = pCReceiver->m_pAttributeTable.lock();
	auto pSourceTable = m_pAttributeTable.lock();
	CRY_PFX2_ASSERT(pSourceTable); // this instance over-lived the table

	if (pReceiverTable != pSourceTable)
	{
		pCReceiver->m_pAttributeTable = m_pAttributeTable;
		pCReceiver->m_attributeIndices = m_attributeIndices;
		pCReceiver->m_attributeValues = m_attributeValues;
	}
	else if (pCReceiver->m_attributeIndices.size() < m_attributeIndices.size())
	{
		pCReceiver->m_attributeIndices.insert(
			pCReceiver->m_attributeIndices.end(),
			m_attributeIndices.begin() + pCReceiver->m_attributeIndices.size(),
			m_attributeIndices.end());
		pCReceiver->m_attributeValues.insert(
			pCReceiver->m_attributeValues.end(),
			m_attributeValues.begin() + pCReceiver->m_attributeValues.size(),
			m_attributeValues.end());
	}
	else
	{
		pCReceiver->m_attributeIndices.resize(m_attributeIndices.size());
		pCReceiver->m_attributeValues.resize(m_attributeValues.size());
	}
}

auto CAttributeInstance::FindAttributeIdByName(cstr name) const->TAttributeId
{
	auto pAttributeTable = m_pAttributeTable.lock();
	if (!pAttributeTable)
		return gInvalidId;

	auto it = std::find_if(
	  m_attributeIndices.begin(), m_attributeIndices.end(),
	  [pAttributeTable, name](const SIndex index)
		{
			const SAttributeDesc& attribute = pAttributeTable->GetAttribute(index.m_index);
			return strcmp(name, attribute.m_name.c_str()) == 0;
	  });
	if (it == m_attributeIndices.end())
		return gInvalidId;
	return TAttributeId(it - m_attributeIndices.begin());
}

uint CAttributeInstance::GetNumAttributes() const
{
	return uint(m_attributeIndices.size());
}

cstr CAttributeInstance::GetAttributeName(uint idx) const
{
	if (idx >= m_attributeIndices.size())
		return nullptr;
	return GetAttributeValue(m_attributeIndices[idx].m_index).m_name.c_str();
}

auto CAttributeInstance::GetAttributeType(uint idx) const->EType
{
	if (idx >= m_attributeIndices.size())
		return ET_Float;
	return EType(GetAttributeValue(m_attributeIndices[idx].m_index).m_type);
}

const SAttributeDesc& CAttributeInstance::GetAttributeValue(TAttributeId attributeId) const
{
	auto pAttributeTable = m_pAttributeTable.lock();
	CRY_PFX2_ASSERT(pAttributeTable); // this instance over-lived the table
	CRY_PFX2_ASSERT(attributeId < m_attributeIndices.size());
	const uint entryIndex = m_attributeIndices[attributeId].m_index;
	const SAttributeDesc& attribute = pAttributeTable->GetAttribute(entryIndex);
	return attribute;
}

void CAttributeInstance::SetAsBoolean(TAttributeId id, bool value)
{
	SetAttributeValue(id, SAttributeValue(value), IParticleAttributes::ET_Boolean);
}

bool CAttributeInstance::GetAsBoolean(TAttributeId id, bool defaultValue) const
{
	return GetAttributeValue(id, SAttributeValue(defaultValue), IParticleAttributes::ET_Boolean).m_asBool;
}

int CAttributeInstance::SetAsInteger(TAttributeId id, int value)
{
	return SetAttributeValue(id, SAttributeValue(value), IParticleAttributes::ET_Integer).m_asInt;
}

int CAttributeInstance::GetAsInteger(TAttributeId id, int defaultValue) const
{
	return GetAttributeValue(id, SAttributeValue(defaultValue), IParticleAttributes::ET_Integer).m_asInt;
}

float CAttributeInstance::SetAsFloat(TAttributeId id, float value)
{
	return SetAttributeValue(id, SAttributeValue(value), IParticleAttributes::ET_Float).m_asFloat;
}

float CAttributeInstance::GetAsFloat(TAttributeId id, float defaultValue) const
{
	return GetAttributeValue(id, SAttributeValue(defaultValue), IParticleAttributes::ET_Float).m_asFloat;
}

void CAttributeInstance::SetAsColor(TAttributeId id, ColorB value)
{
	SetAttributeValue(id, SAttributeValue(value), IParticleAttributes::ET_Color);
}

void CAttributeInstance::SetAsColor(TAttributeId id, ColorF value)
{
	SetAsColor(id, ColorB(value));
}

ColorB CAttributeInstance::GetAsColorB(TAttributeId id, ColorB defaultValue) const
{
	return GetAttributeValue(id, SAttributeValue(defaultValue), IParticleAttributes::ET_Color).m_asColor;
}

ColorF CAttributeInstance::GetAsColorF(TAttributeId id, ColorF defaultValue) const
{
	const ColorB color = GetAsColorB(id, ColorB(defaultValue));
	return ColorF(color.pack_abgr8888());
}

bool CAttributeInstance::IsDirty() const
{
	auto pTable = m_pAttributeTable.lock();
	return pTable && m_editVersion != pTable->GetEditVersion();
}

void CAttributeInstance::Compile()
{
	auto pTable = m_pAttributeTable.lock();
	CRY_PFX2_ASSERT(pTable); // this instance over-lived the table

	std::vector<SAttributeEdit> attributesEdit;
	std::vector<SIndex> attributeIndices;
	std::vector<SAttributeValue> data;

	const uint curNumInstanceAttributes = m_attributeIndices.size();
	const uint numTableAttrbibutes = pTable->GetNumAttributes();
	for (uint i = 0; i < numTableAttrbibutes; ++i)
	{
		const TAttributeId attributeId = attributesEdit.size();
		const SAttributeDesc& attribute = pTable->GetAttribute(i);

		auto it = std::find_if(m_attributeIndices.begin(), m_attributeIndices.end(), [attribute, this](const SIndex index)
		{
			return index.m_id == attribute.m_id;
		});
		uint existingIndex = uint(it - m_attributeIndices.begin());

		if (it == m_attributeIndices.end() || !m_attributeValues[existingIndex].m_edited)
		{
			SAttributeValue newData;
			switch (attribute.m_type)
			{
			case EAttributeType::Boolean:
				newData.m_asBool = attribute.m_asBoolean;
				break;
			case EAttributeType::Integer:
				newData.m_asInt = attribute.m_asInt;
				break;
			case EAttributeType::Float:
				newData.m_asFloat = attribute.m_asFloat;
				break;
			case EAttributeType::Color:
				newData.m_asColor = attribute.m_asColor;
				break;
			}			
			data.push_back(newData);
			SAttributeType* pTypeHandler = s_AttributeTypeHandlers[uint(attribute.m_type)].get();
			attributesEdit.emplace_back(this, pTypeHandler, attributeId);
		}
		else
		{
			data.push_back(m_attributeValues[existingIndex]);
			attributesEdit.push_back(m_attributesEdit[existingIndex]);
			attributesEdit.back().m_attributeIndex = attributeId;
		}
		attributeIndices.emplace_back(i, attribute.m_id);
	}

	std::swap(m_attributeIndices, attributeIndices);
	std::swap(m_attributeValues, data);
	std::swap(m_attributesEdit, attributesEdit);

	m_editVersion = pTable->GetEditVersion();
}

SAttributeValue CAttributeInstance::SetAttributeValue(TAttributeId id, const SAttributeValue& input, IParticleAttributes::EType type)
{
	const SAttributeDesc& attribute = GetAttributeValue(id);
	TAttribConverterFn converter = s_AttributeConverterTable[uint(type)][uint(attribute.m_type)];
	SAttributeValue output = converter(input);

	switch (attribute.m_type)
	{
	case EAttributeType::Integer:
		if (attribute.m_useMin)
			output.m_asInt = max(attribute.m_minInt, output.m_asInt);
		if (attribute.m_useMax)
			output.m_asInt = min(attribute.m_maxInt, output.m_asInt);
		break;
	case EAttributeType::Float:
		if (attribute.m_useMin)
			output.m_asFloat = max(attribute.m_minFloat, output.m_asFloat);
		if (attribute.m_useMax)
			output.m_asFloat = min(attribute.m_maxFloat, output.m_asFloat);
		break;
	}

	m_attributeValues[id] = output;

	return output;
}

SAttributeValue CAttributeInstance::GetAttributeValue(TAttributeId id, const SAttributeValue& defaultValue, IParticleAttributes::EType type) const
{
	if (id == gInvalidId)
		return defaultValue;
	const SAttributeDesc& attribute = GetAttributeValue(id);
	TAttribConverterFn converter = s_AttributeConverterTable[uint(attribute.m_type)][uint(type)];
	return converter(m_attributeValues[id]);
}

}
