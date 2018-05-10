// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Deprecated/Stack.h"

namespace Schematyc2
{
	template <typename TYPE> inline void Pop(CStack& stack, TYPE& value);

	inline void Pop(CStack& stack, bool& value)
	{
		value = stack.Top().AsBool();
		stack.Pop();
	}

	inline void Pop(CStack& stack, int8& value)
	{
		value = stack.Top().AsInt8();
		stack.Pop();
	}

	inline void Pop(CStack& stack, uint8& value)
	{
		value = stack.Top().AsUInt8();
		stack.Pop();
	}

	inline void Pop(CStack& stack, int16& value)
	{
		value = stack.Top().AsInt16();
		stack.Pop();
	}

	inline void Pop(CStack& stack, uint16& value)
	{
		value = stack.Top().AsUInt16();
		stack.Pop();
	}

	inline void Pop(CStack& stack, int32& value)
	{
		value = stack.Top().AsInt32();
		stack.Pop();
	}

	inline void Pop(CStack& stack, uint32& value)
	{
		value = stack.Top().AsUInt32();
		stack.Pop();
	}

	inline void Pop(CStack& stack, int64& value)
	{
		value = stack.Top().AsInt64();
		stack.Pop();
	}

	inline void Pop(CStack& stack, uint64& value)
	{
		value = stack.Top().AsUInt64();
		stack.Pop();
	}

	inline void Pop(CStack& stack, float& value)
	{
		value = stack.Top().AsFloat();
		stack.Pop();
	}

	inline void Pop(CStack& stack, CPoolString& value)
	{
		value = stack.Top().c_str();
		stack.Pop();
	}

	template <class TYPE> inline void Pop(CStack& stack, CryStringT<TYPE>& value)
	{
		value = stack.Top().c_str();
		stack.Pop();
	}

	template <class TYPE, size_t SIZE> inline void Pop(CStack& stack, CryStackStringT<TYPE, SIZE>& value)
	{
		value = stack.Top().c_str();
		stack.Pop();
	}

	class CStackIArchive : public Serialization::IArchive
	{
	public:

		inline CStackIArchive(CStack& stack)
			: Serialization::IArchive(Serialization::IArchive::INPUT | Serialization::IArchive::INPLACE)
			, m_stack(stack)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
			return true;
		}

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			Pop(m_stack, value);
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
			Pop(m_stack, value);
		}

	private:

		CStack& m_stack;
	};

	template <typename TYPE> inline void Pop(CStack& stack, TYPE& value)
	{
		CStackIArchive archive(stack);
		archive(value);
	}
}
