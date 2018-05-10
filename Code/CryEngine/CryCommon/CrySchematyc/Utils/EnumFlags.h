// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Merge with CryFlags?

#pragma once

#include <CrySerialization/Enum.h>
#include <CrySerialization/Forward.h>

//namespace Schematyc
//{
template<typename VALUE_TYPE> class CEnumFlags
{
public:

	typedef VALUE_TYPE                                      ValueType;
	typedef typename std::underlying_type<VALUE_TYPE>::type UnderlyingType;

public:

	inline CEnumFlags()
	{
		Clear();
	}

	inline CEnumFlags(ValueType value)
		: m_value(value)
	{}

	inline CEnumFlags(std::initializer_list<ValueType> values)
	{
		Clear();
		for (const ValueType& value : values)
		{
			UnderlyingValue() |= static_cast<UnderlyingType>(value);
		}
	}

	inline CEnumFlags(const CEnumFlags& rhs)
		: m_value(rhs.m_value)
	{}

	explicit inline operator bool() const
	{
		return UnderlyingValue() != 0;
	}

	inline bool IsEmpty() const
	{
		return UnderlyingValue() == 0;
	}

	inline void Set(const CEnumFlags& flags)
	{
		m_value = flags.m_value;
	}

	inline void SetWithFilter(const CEnumFlags& flags, const CEnumFlags& filter)
	{
		UnderlyingValue() = (UnderlyingValue() & ~filter.UnderlyingValue()) | (flags.UnderlyingValue() & filter.UnderlyingValue());
	}

	inline void Add(const CEnumFlags& flags)
	{
		UnderlyingValue() |= flags.UnderlyingValue();
	}

	inline void AddWithFilter(const CEnumFlags& flags, const CEnumFlags& filter)
	{
		UnderlyingValue() |= flags.UnderlyingValue() & filter.UnderlyingValue();
	}

	inline void Remove(const CEnumFlags& flags)
	{
		UnderlyingValue() &= ~flags.UnderlyingValue();
	}

	inline void RemoveWithFilter(const CEnumFlags& flags, const CEnumFlags& filter)
	{
		UnderlyingValue() &= ~(flags.UnderlyingValue() & filter.UnderlyingValue());
	}

	// Check that specified flag is set.
	inline bool Check(ValueType value) const
	{
		return (UnderlyingValue() & static_cast<UnderlyingType>(value)) != 0;
	}

	// Check that any of specified flags are set.
	inline bool CheckAny(const CEnumFlags& flags) const
	{
		return (UnderlyingValue() & flags.UnderlyingValue()) != 0;
	}

	// Check that all of specified flags are set.
	inline bool CheckAll(const CEnumFlags& flags) const
	{
		return (UnderlyingValue() & flags.UnderlyingValue()) == flags.UnderlyingValue();
	}

	inline void Clear()
	{
		UnderlyingValue() = 0;
	}

	inline ValueType& Value()
	{
		return m_value;
	}

	inline const ValueType& Value() const
	{
		return m_value;
	}

	inline UnderlyingType& UnderlyingValue()
	{
		return reinterpret_cast<UnderlyingType&>(m_value);
	}

	inline const UnderlyingType& UnderlyingValue() const
	{
		return reinterpret_cast<const UnderlyingType&>(m_value);
	}

	inline void operator=(const CEnumFlags& rhs)
	{
		m_value = rhs.m_value;
	}

	inline bool operator==(const CEnumFlags& rhs) const
	{
		return m_value == rhs.m_value;
	}

	inline bool operator!=(const CEnumFlags& rhs) const
	{
		return m_value != rhs.m_value;
	}

	inline CEnumFlags operator|(const CEnumFlags& rhs) const
	{
		return CEnumFlags(UnderlyingValue() | rhs.UnderlyingValue());
	}

	inline CEnumFlags& operator|=(const CEnumFlags& rhs)
	{
		UnderlyingValue() |= rhs.UnderlyingValue();
		return *this;
	}

	inline CEnumFlags operator&(const CEnumFlags& rhs) const
	{
		return CEnumFlags(UnderlyingValue() & rhs.UnderlyingValue());
	}

	inline CEnumFlags& operator&=(const CEnumFlags& rhs)
	{
		UnderlyingValue() &= rhs.UnderlyingValue();
		return *this;
	}

	inline CEnumFlags operator^(const CEnumFlags& rhs) const
	{
		return CEnumFlags(UnderlyingValue() ^ rhs.UnderlyingValue());
	}

	inline CEnumFlags& operator^=(const CEnumFlags& rhs)
	{
		UnderlyingValue() ^= rhs.UnderlyingValue();
		return *this;
	}

	inline CEnumFlags operator~() const
	{
		return CEnumFlags(~UnderlyingValue());
	}

private:

	explicit inline CEnumFlags(UnderlyingType underlyingValue)
		: m_value(static_cast<ValueType>(underlyingValue))
	{}

private:

	ValueType m_value;
};

namespace SerializationUtils
{
template<typename VALUE_TYPE> class CEnumFlagsDecorator
{
private:

	typedef VALUE_TYPE                                      ValueType;
	typedef typename std::underlying_type<VALUE_TYPE>::type UnderlyingType;

public:

	inline CEnumFlagsDecorator(ValueType& value, ValueType base, ValueType filter)
		: m_value(value)
		, m_base(base)
		, m_filter(filter)
	{}

	void Serialize(Serialization::IArchive& archive)
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<ValueType>();
		if (archive.isInput())
		{
			m_value = m_base;
			for (int flagIdx = 0, flagCount = enumDescription.count(); flagIdx < flagCount; ++flagIdx)
			{
				const int flag = enumDescription.valueByIndex(flagIdx);
				if ((static_cast<const UnderlyingType>(m_filter) & flag) != 0)
				{
					bool bFlagValue = false;
					archive(bFlagValue, enumDescription.nameByIndex(flagIdx), enumDescription.labelByIndex(flagIdx));
					if (bFlagValue)
					{
						m_value = static_cast<ValueType>(static_cast<const UnderlyingType>(m_value) | enumDescription.valueByIndex(flagIdx));
					}
				}
			}
		}
		else if (archive.isOutput())
		{
			for (int flagIdx = 0, flagCount = enumDescription.count(); flagIdx < flagCount; ++flagIdx)
			{
				const int flag = enumDescription.valueByIndex(flagIdx);
				if ((static_cast<const UnderlyingType>(m_filter) & flag) != 0)
				{
					bool bFlagValue = (static_cast<const UnderlyingType>(m_value) & enumDescription.valueByIndex(flagIdx)) != 0;
					archive(bFlagValue, enumDescription.nameByIndex(flagIdx), enumDescription.labelByIndex(flagIdx));
				}
			}
		}
	}

private:

	ValueType&      m_value;
	const ValueType m_base;
	const ValueType m_filter;
};

template<typename VALUE_TYPE> CEnumFlagsDecorator<VALUE_TYPE> EnumFlags(VALUE_TYPE& value, VALUE_TYPE base = 0, VALUE_TYPE filter = ~0)
{
	return SerializationUtils::CEnumFlagsDecorator<VALUE_TYPE>(value, base, filter);
}

template<typename VALUE_TYPE> CEnumFlagsDecorator<VALUE_TYPE> EnumFlags(CEnumFlags<VALUE_TYPE>& value, const CEnumFlags<VALUE_TYPE>& base = CEnumFlags<VALUE_TYPE>(VALUE_TYPE(0)), const CEnumFlags<VALUE_TYPE>& filter = CEnumFlags<VALUE_TYPE>(VALUE_TYPE(~0)))
{
	return SerializationUtils::CEnumFlagsDecorator<VALUE_TYPE>(value.Value(), base.Value(), filter.Value());
}
}

template<typename VALUE_TYPE> inline bool Serialize(Serialization::IArchive& archive, CEnumFlags<VALUE_TYPE>& value, const char* szName, const char* szLabel)
{
	return archive(SerializationUtils::EnumFlags(value), szName, szLabel);
}
//} // Schematyc
