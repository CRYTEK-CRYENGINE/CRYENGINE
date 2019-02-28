// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#define WRAP_TYPE(baseType, type, invalid) struct type##_traits { static const baseType s_invalid = invalid; }; \
                                           typedef TemplateUtils::CTypeWrapper<baseType, type##_traits> type;

namespace TemplateUtils
{
	template <typename BASE_TYPE, typename TRAITS> class CTypeWrapper
	{
	public:

		typedef BASE_TYPE Type;

		inline CTypeWrapper()
			: m_value(s_invalid.GetValue())
		{}

		explicit inline CTypeWrapper(Type value)
		{
			m_value = value;
		}

		inline CTypeWrapper(const CTypeWrapper& rhs)
			: m_value(rhs.m_value)
		{}

		inline void SetValue(BASE_TYPE value)
		{
			m_value = value;
		}

		inline BASE_TYPE GetValue() const
		{
			return m_value;
		}

		inline void Invalidate()
		{
			m_value = s_invalid.GetValue();
		}

		inline bool IsValid() const 
		{
			return m_value != s_invalid.GetValue();
		}

		inline CTypeWrapper& operator = (const CTypeWrapper& rhs)
		{
			m_value = rhs.m_value;
			return *this;
		}

		inline CTypeWrapper& operator += (const CTypeWrapper& rhs)
		{
			m_value += rhs.m_value;
			return *this;
		}

		inline CTypeWrapper operator ++ ()
		{
			++ m_value;
			return *this;
		}

		inline CTypeWrapper operator ++ (int)
		{
			CTypeWrapper prev(m_value);
			++ m_value;
			return prev;
		}

		inline CTypeWrapper& operator -= (const CTypeWrapper& rhs)
		{
			m_value -= rhs.m_value;
			return *this;
		}

		inline CTypeWrapper operator -- ()
		{
			-- m_value;
			return *this;
		}

		inline CTypeWrapper operator -- (int)
		{
			CTypeWrapper prev(m_value);
			-- m_value;
			return prev;
		}

		inline CTypeWrapper& operator &= (const CTypeWrapper& rhs)
		{
			m_value &= rhs.m_value;
			return *this;
		}

		inline CTypeWrapper& operator %= (const CTypeWrapper& rhs)
		{
			m_value %= rhs.m_value;
			return *this;
		}

		inline bool operator == (const CTypeWrapper& rhs) const
		{
			return m_value == rhs.m_value;
		}

		inline bool operator != (const CTypeWrapper& rhs) const
		{
			return m_value != rhs.m_value;
		}

		inline bool operator < (const CTypeWrapper& rhs) const
		{
			return m_value < rhs.m_value;
		}

		inline bool operator <= (const CTypeWrapper& rhs) const
		{
			return m_value <= rhs.m_value;
		}

		inline bool operator > (const CTypeWrapper& rhs) const
		{
			return m_value > rhs.m_value;
		}

		inline bool operator >= (const CTypeWrapper& rhs) const
		{
			return m_value >= rhs.m_value;
		}

		inline CTypeWrapper operator + (const CTypeWrapper& rhs) const
		{
			CTypeWrapper result(*this);
			return result += rhs;
		}

		inline CTypeWrapper operator - (const CTypeWrapper& rhs) const
		{
			CTypeWrapper result(*this);
			return result -= rhs;
		}

		inline CTypeWrapper operator & (const CTypeWrapper& rhs) const
		{
			CTypeWrapper result(*this);
			return result &= rhs;
		}

		inline CTypeWrapper operator % (const CTypeWrapper& rhs) const
		{
			CTypeWrapper result(*this);
			return result %= rhs;
		}

		static const CTypeWrapper s_invalid;

	private:

		Type m_value;
	};

	template <typename BASE_TYPE, typename TRAITS> const CTypeWrapper<BASE_TYPE, TRAITS> CTypeWrapper<BASE_TYPE, TRAITS>::s_invalid = CTypeWrapper<BASE_TYPE, TRAITS>(TRAITS::s_invalid);

	template <typename BASE_TYPE, typename TRAITS> inline bool Serialize(Serialization::IArchive& archive, CTypeWrapper<BASE_TYPE, TRAITS>& value, const char* szName, const char* szLabel)
	{
		if(archive.isInput())
		{
			BASE_TYPE	temp;
			archive(temp, szName, szLabel);
			value.SetValue(temp);
		}
		else if(archive.isOutput())
		{
			BASE_TYPE	temp = value.GetValue();
			archive(temp, szName, szLabel);
		}
		return true;
	}
}
