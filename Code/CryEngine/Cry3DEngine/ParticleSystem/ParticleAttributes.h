// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PARTICLEATTRIBUTES_H
#define PARTICLEATTRIBUTES_H

#pragma once

#include "Features/ParamTraits.h"
#include <CrySerialization/Color.h>
#include <CrySerialization/Enum.h>
#include <CryCore/CryVariant.h>
#include <CryString/CryName.h>

namespace pfx2
{

// Description of an attribute, including default value
struct SAttributeDesc
{
	void Serialize(IArchive& ar);

	CCryName                     m_name;
	IParticleAttributes::TValue  m_defaultValue;

	TValue<int, TDefaultMin<>>   m_minInt;
	TValue<int, TDefaultMax<>>   m_maxInt;
	TValue<float, TDefaultMin<>> m_minFloat;
	TValue<float, TDefaultMax<>> m_maxFloat;

	IParticleAttributes::EType	GetType() const { return m_defaultValue.Type(); }
};

// Instance of an attribute value
struct SAttributeEdit
{
	SAttributeEdit()
		{}
	SAttributeEdit(const CCryName& name, const IParticleAttributes::TValue& value)
		: m_name(name), m_value(value){}

	CCryName                    m_name;
	IParticleAttributes::TValue m_value;

	void Serialize(IArchive& ar);
};

// List of attribute descriptions
class CAttributeTable
{
public:
	CAttributeTable()
		: m_enumDesc(yasli::TypeID::get<CAttributeTable>()) {}
                    
	uint                  GetNumAttributes() const       { return m_attributes.size(); }
	const SAttributeDesc& GetAttribute(uint index) const { return m_attributes[index]; }
	int                   FindAttributeIdByName(const CCryName& name) const;

	void                  Serialize(IArchive& ar);
	bool                  SerializeName(IArchive& ar, CCryName& value, cstr name, cstr label) const;

	void                  AddAttribute(const SAttributeDesc& desc) { m_attributes.push_back(desc);  }

private:
	std::vector<SAttributeDesc> m_attributes;
	yasli::EnumDescription      m_enumDesc; // TODO: Streamline, pass name list to archive more directly
};

typedef std::shared_ptr<const CAttributeTable> TAttributeTablePtr;

// List of attribute value instances
class CAttributeInstance : public IParticleAttributes
{
public:
	CAttributeInstance() {}
	CAttributeInstance(TAttributeTablePtr pTable) { Reset(pTable); }

	// IParticleAttributes
	void          Reset(const IParticleAttributes* pCopySource = nullptr) override;
	void          Serialize(IArchive& ar) override;
	void          TransferInto(IParticleAttributes* pReceiver) const override;
	TAttributeId  FindAttributeIdByName(cstr name) const override { return FindAttributeIdByName(CCryName(name)); }
	uint          GetNumAttributes() const override;
	cstr          GetAttributeName(TAttributeId idx) const override;
	EType         GetAttributeType(TAttributeId idx) const override;
	const TValue& GetValue(TAttributeId idx) const override;
	TValue        GetValue(TAttributeId idx, const TValue& defaultVal) const override;
	bool          SetValue(TAttributeId idx, const TValue& value) override;
	// ~IParticleAttributes

	void                   Reset(TAttributeTablePtr pTable);
	void                   ResetValue(TAttributeId id);
	SAttributeEdit*        FindEditById(TAttributeId id);
	const SAttributeEdit*  FindEditById(TAttributeId id) const { return non_const(this)->FindEditById(id); }
	const SAttributeDesc&  GetDesc(TAttributeId attributeId) const;

	void                   AddAttribute(const SAttributeEdit& edit) { m_attributesEdit.push_back(edit); }
	TAttributeId           FindAttributeIdByName(const CCryName& name) const;
	
	bool                   WasChanged() { return m_changed && ((m_changed = false), true); }

private:
	std::vector<SAttributeEdit>          m_attributesEdit;
	std::weak_ptr<const CAttributeTable> m_pAttributeTable;
	bool                                 m_changed = true;
};

class CAttributeReference
{
public:
	const CCryName& Name() const { return m_name; }

	bool Serialize(Serialization::IArchive& ar, cstr name, cstr label);

	template<typename T>
	T GetValueAs(const CAttributeInstance& attributes, T defaultValue) const
	{
		auto id = attributes.FindAttributeIdByName(m_name);
		return attributes.GetValueAs(id, defaultValue);
	}

private:
	CCryName m_name;
};

inline bool Serialize(Serialization::IArchive& ar, CAttributeReference& attr, cstr name, cstr label)
{
	return attr.Serialize(ar, name, label);
}


}

#endif
