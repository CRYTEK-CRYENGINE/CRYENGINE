// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Deprecated/Stack.h"

namespace Schematyc2
{
	template <typename TYPE> inline void Push(CStack& stack, const TYPE& value);

	inline void Push(CStack& stack, const bool& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const int8& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const uint8& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const int16& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const uint16& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const int32& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const uint32& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const int64& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const uint64& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const float& value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const char* value)
	{
		stack.Push(CVariant(value));
	}

	inline void Push(CStack& stack, const CPoolString& value)
	{
		stack.Push(CVariant(value.c_str()));
	}

	template <class TYPE> inline void Push(CStack& stack, const CryStringT<TYPE>& value)
	{
		stack.Push(CVariant(value.c_str()));
	}

	template <class TYPE, size_t SIZE> inline void Push(CStack& stack, const CryStackStringT<TYPE, SIZE>& value)
	{
		stack.Push(CVariant(value.c_str()));
	}

	class CStackOArchive : public Serialization::IArchive
	{
	public:

		inline CStackOArchive(CStack& stack)
			: Serialization::IArchive(Serialization::IArchive::OUTPUT | Serialization::IArchive::INPLACE)
			, m_stack(stack)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value);
			return true;
		}

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Push(m_stack, value.get());
			return true;
		}

		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			value(*this);
			return true;
		}

		virtual bool operator () (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			if(value.isFixedSize())
			{
				if(value.size())
				{
					do
					{
						value(*this, szName, szLabel);
					} while(value.next());
				}
				return true;
			}
			else
			{
				CryFatalError("Serialization of dynamic containers is not yet supported!");
				return false;
			}
		}

		using Serialization::IArchive::operator ();

		// ~Serialization::IArchive

		inline void operator () (const char*& value)
		{
			Push(m_stack, value);
		}

	private:

		CStack& m_stack;
	};

	template <typename TYPE> inline void Push(CStack& stack, const TYPE& value)
	{
		CStackOArchive archive(stack);
		archive(const_cast<TYPE&>(value));
	}
}
