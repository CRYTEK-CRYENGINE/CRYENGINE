// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef PARTICLEATTRIBUTES_H
#define PARTICLEATTRIBUTES_H

#pragma once

#include <CrySerialization/Color.h>
#include <CrySerialization/Enum.h>

namespace pfx2
{

SERIALIZATION_ENUM_DECLARE(EAttributeType, ,
                           Boolean = IParticleAttributes::ET_Boolean,
                           Integer = IParticleAttributes::ET_Integer,
                           Float = IParticleAttributes::ET_Float,
                           Color = IParticleAttributes::ET_Color
                           )

SERIALIZATION_ENUM_DECLARE(EAttributeScope, ,
                           PerEmitter,
                           PerEffect
                           )

class CAttributeInstance;
struct SAttributeType;

struct SAttributeValue
{
	SAttributeValue() = default;
	explicit SAttributeValue(ColorB value) : m_asColor(value) {}
	explicit SAttributeValue(int value) : m_asInt(value) {}
	explicit SAttributeValue(float value) : m_asFloat(value) {}
	explicit SAttributeValue(bool value) : m_asBool(value) {}
	ColorB   m_asColor = ColorB(~0u);
	int      m_asInt   = 0;
	float    m_asFloat = 0.0f;
	bool     m_asBool  = true;
	bool     m_edited  = false;
};

struct SAttributeDesc
{
	void Serialize(Serialization::IArchive& ar);

	string          m_name;
	EAttributeType  m_type      = EAttributeType::Float;
	EAttributeScope m_scope     = EAttributeScope::PerEmitter;
	ColorB          m_asColor   = ColorB(~0u);
	uint            m_id        = -1;
	int             m_minInt    = 0;
	int             m_maxInt    = 10;
	float           m_asFloat   = 1.0f;
	float           m_minFloat  = 0.0f;
	float           m_maxFloat  = 1.0f;
	int             m_asInt     = 0;
	bool            m_asBoolean = true;
	bool            m_useMin    = false;
	bool            m_useMax    = false;
};

struct SAttributeEdit
{
	SAttributeEdit(CAttributeInstance* pAttributes, SAttributeType* pTypeHandler, uint attributeIndex);

	void Serialize(Serialization::IArchive& ar);
	void Reset();

	SAttributeType*     m_pTypeHandler;
	CAttributeInstance* m_pAttributes;
	uint                m_attributeIndex;
};

class CAttributeTable
{
public:
	uint                  GetNumAttributes() const       { return m_attributes.size(); }
	const SAttributeDesc& GetAttribute(uint index) const { return m_attributes[index]; }
	uint                  FindAttribyteIndexById(uint id) const;
	void                  Serialize(Serialization::IArchive& ar);
	uint                  GetEditVersion() const         { return m_editVersion; }

private:
	std::vector<SAttributeDesc> m_attributes;
	uint                    m_editVersion = 0;
};

typedef std::shared_ptr<const CAttributeTable> TAttributeTablePtr;

class CAttributeInstance : public IParticleAttributes
{
public:
	struct SIndex
	{
		SIndex() = default;
		SIndex(uint index, uint id)
			: m_index(index), m_id(id) {}
		uint m_index;
		uint m_id;
	};

public:
	CAttributeInstance();

	// IParticleAttributes
	virtual void         Reset(const IParticleAttributes* pCopySource = nullptr);
	virtual void         Serialize(Serialization::IArchive& ar);
	virtual void         TransferInto(IParticleAttributes* pReceiver) const;
	virtual TAttributeId FindAttributeIdByName(cstr name) const;
	virtual uint         GetNumAttributes() const;
	virtual cstr         GetAttributeName(TAttributeId idx) const;
	virtual EType        GetAttributeType(TAttributeId idx) const;
	virtual bool         GetAsBoolean(TAttributeId id, bool defaultValue) const;
	virtual int          GetAsInteger(TAttributeId id, int defaultValue) const;
	virtual float        GetAsFloat(TAttributeId id, float defaultValue) const;
	virtual ColorB       GetAsColorB(TAttributeId id, ColorB defaultValue) const;
	virtual ColorF       GetAsColorF(TAttributeId id, ColorF defaultValue) const;
	virtual void         SetAsBoolean(TAttributeId id, bool value);
	virtual int          SetAsInteger(TAttributeId id, int value);
	virtual float        SetAsFloat(TAttributeId id, float value);
	virtual void         SetAsColor(TAttributeId id, ColorB value);
	virtual void         SetAsColor(TAttributeId id, ColorF value);
	// ~IParticleAttributes

	SAttributeValue&       GetValue(TAttributeId id) { return m_attributeValues[id]; }
	const SAttributeValue& GetValue(TAttributeId id) const { return m_attributeValues[id]; }
	SAttributeEdit&        GetEdit(TAttributeId id) { return m_attributesEdit[id]; }
	void                   Reset(TAttributeTablePtr pTable, EAttributeScope scope);
	const SAttributeDesc&  GetAttributeValue(TAttributeId attributeId) const;

private:
	bool            IsDirty() const;
	void            Compile();
	SAttributeValue SetAttributeValue(TAttributeId id, const SAttributeValue& input, IParticleAttributes::EType type);
	SAttributeValue GetAttributeValue(TAttributeId id, const SAttributeValue& defaultValue, IParticleAttributes::EType type) const;

private:
	std::vector<SIndex>                  m_attributeIndices;
	std::vector<SAttributeEdit>          m_attributesEdit;
	std::vector<SAttributeValue>         m_attributeValues;
	std::weak_ptr<const CAttributeTable> m_pAttributeTable;
	uint                                 m_editVersion = 0;
};

}

#endif
