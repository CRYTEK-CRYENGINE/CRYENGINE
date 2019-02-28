// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move utility functions to Any namespace?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/IAny.h"
#include "CrySchematyc2/Utils/DefaultInit.h"
#include "CrySchematyc2/Utils/SharedArray.h"
#include "CrySchematyc2/Utils/ToString.h"

namespace Schematyc2
{
	template <typename TYPE> class CAnyArrayExtension;

	// Requirements of TYPE parameter are:
	//   - void Serialize(Serialization::IArchive&); // Create IsSerializable test to display helpful compile errors?
	//   - TYPE& operator = (const TYPE& rhs);       // Use std::is_assignable to display helpful compile errors?
	//   - bool operator == (const TYPE& rhs) const;
	template <typename TYPE> class CAny : public IAny
	{
	public:

		static_assert(std::is_copy_assignable<TYPE>::value, "Type must be copy assignable!");

		inline CAny(const TYPE& value)
			: m_value(value)
		{}

		// IAny

		virtual uint32 GetSize() const override
		{
			return sizeof(CAny);
		}

		virtual CTypeInfo GetTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<TYPE>();
		}

		virtual IAny* Clone(void* pPlacement) const override
		{
			return new (pPlacement) CAny(m_value);
		}

		virtual IAnyPtr Clone() const override
		{
			return IAnyPtr(std::make_shared<CAny>(m_value));
		}

		virtual bool ToString(const CharArrayView& output) const override
		{
			return Schematyc2::ToString(m_value, output);
		}

		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) override
		{
			archive(m_value, szName, szLabel);
			return true;
		}

		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const override
		{
			return GameSerialization::IContextPtr(std::make_shared<GameSerialization::CContext<const TYPE> >(archive, m_value));
		}

		virtual IAnyExtension* QueryExtension(EAnyExtension extension) override
		{
			return nullptr;
		}

		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const override
		{
			return nullptr;
		}

		virtual void* ToVoidPtr() override
		{
			return &m_value;
		}

		virtual const void* ToVoidPtr() const override
		{
			return &m_value;
		}

		virtual IAny& operator = (const IAny& rhs) override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				m_value = *static_cast<const TYPE*>(rhs.ToVoidPtr());
			}
			return *this;
		}

		virtual bool operator == (const IAny& rhs) const override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				return m_value == *static_cast<const TYPE*>(rhs.ToVoidPtr());
			}
			return false;
		}
	
		// ~IAny

		inline TYPE& Value()
		{
			return m_value;
		}

		inline const TYPE& Value() const
		{
			return m_value;
		}

	private:

		TYPE m_value;
	};

	template <typename TYPE, uint32 SIZE> class CAnyArrayExtension<TYPE[SIZE]> : public IAnyArrayExtension
	{
	public:

		typedef TYPE ArrayType[SIZE];

		inline CAnyArrayExtension() {}

		inline CAnyArrayExtension(const ArrayType& value)
		{
			std::copy(value, value + SIZE, m_value);
		}

		// IAnyArrayExtension

		virtual CTypeInfo GetElementTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<TYPE>();
		}

		virtual uint32 GetSize() const override
		{
			return SIZE;
		}

		virtual bool IsFixedSize() const override
		{
			return true;
		}

		virtual IAny* CreateElement(void* pPlacement) const override
		{
			return new (pPlacement) CAny<TYPE>();
		}

		virtual IAnyPtr CreateElement() const override
		{
			return IAnyPtr(std::make_shared<CAny<TYPE> >());
		}

		virtual IAny* CloneElement(uint32 idx, void* pPlacement) const override
		{
			return idx < SIZE ? new (pPlacement) CAny<TYPE>(m_value[idx]) : nullptr;
		}

		virtual IAnyPtr CloneElement(uint32 idx) const override
		{
			return idx < SIZE ? IAnyPtr(std::make_shared<CAny<TYPE> >(m_value[idx])) : IAnyPtr();
		}

		virtual bool PushBack(const IAny& value) override
		{
			return false;
		}

		virtual bool PopBack() override
		{
			return false;
		}

		// ~IAnyArrayExtension

		ArrayType& Value()
		{
			return m_value;
		}

		const ArrayType& Value() const
		{
			return m_value;
		}

	private:

		ArrayType m_value;
	};

	template <typename TYPE, uint32 SIZE> class CAny<TYPE[SIZE]> : public IAny
	{
	public:

		typedef TYPE ArrayType[SIZE];

		inline CAny(const ArrayType &value)
			: m_arrayExtension(value)
		{}

		// IAny

		virtual uint32 GetSize() const override
		{
			return sizeof(CAny);
		}

		virtual CTypeInfo GetTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<ArrayType>();
		}

		virtual IAny* Clone(void* pPlacement) const override
		{
			return new (pPlacement) CAny(m_arrayExtension);
		}

		virtual IAnyPtr Clone() const override
		{
			return IAnyPtr(std::make_shared<CAny>(m_arrayExtension));
		}

		virtual bool ToString(const CharArrayView& output) const override
		{
			return Schematyc2::ToString(m_arrayExtension, output);
		}

		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) override
		{
			archive(m_arrayExtension, szName, szLabel);
			return true;
		}

		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const override
		{
			return GameSerialization::IContextPtr(std::make_shared<GameSerialization::CContext<const TYPE> >(archive, m_arrayExtension));
		}

		virtual IAnyExtension* QueryExtension(EAnyExtension extension) override
		{
			return extension == EAnyExtension::Array ? &m_arrayExtension : nullptr;
		}

		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const override
		{
			return extension == EAnyExtension::Array ? &m_arrayExtension : nullptr;
		}

		virtual void* ToVoidPtr() override
		{
			return &m_arrayExtension.Value();
		}

		virtual const void* ToVoidPtr() const override
		{
			return &m_arrayExtension.Value();
		}

		virtual IAny& operator = (const IAny& rhs) override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				m_arrayExtension.Value() = *static_cast<const ArrayType*>(rhs.ToVoidPtr());
			}
			return *this;
		}

		virtual bool operator == (const IAny& rhs) const override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				return m_arrayExtension.Value() == *static_cast<const ArrayType*>(rhs.ToVoidPtr());
			}
			return false;
		}

		// ~IAny

		ArrayType& Value()
		{
			return m_arrayExtension.Value();
		}

		const ArrayType& Value() const
		{
			return m_arrayExtension.Value();
		}

	private:

		CAnyArrayExtension<ArrayType> m_arrayExtension;
	};

	template <typename TYPE> class CAnyArrayExtension<CSharedArray<TYPE> > : public IAnyArrayExtension
	{
	public:

		typedef CSharedArray<TYPE> ArrayType;

		inline CAnyArrayExtension() {}

		inline CAnyArrayExtension(const ArrayType& value)
			: m_value(value)
		{}

		// IAnyArrayExtension

		virtual CTypeInfo GetElementTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<TYPE>();
		}

		virtual uint32 GetSize() const override
		{
			return m_value.GetSize();
		}

		virtual bool IsFixedSize() const override
		{
			return false;
		}

		virtual IAny* CreateElement(void* pPlacement) const override
		{
			return new (pPlacement) CAny<TYPE>();
		}

		virtual IAnyPtr CreateElement() const override
		{
			return IAnyPtr(std::make_shared<CAny<TYPE> >());
		}

		virtual IAny* CloneElement(uint32 idx, void* pPlacement) const override
		{
			return idx < m_value.GetSize() ? new (pPlacement) CAny<TYPE>(m_value.GetElement(idx)) : nullptr;
		}

		virtual IAnyPtr CloneElement(uint32 idx) const override
		{
			return idx < m_value.GetSize() ? IAnyPtr(std::make_shared<CAny<TYPE> >(m_value.GetElement(idx))) : IAnyPtr();
		}

		virtual bool PushBack(const IAny& value) override
		{
			if(value.GetTypeInfo() != GetElementTypeInfo())
			{
				return false;
			}
			m_value.PushBack(*static_cast<const TYPE*>(value.ToVoidPtr()));
			return true;
		}

		virtual bool PopBack() override
		{
			m_value.PopBack();
			return true;
		}

		// ~IAnyArrayExtension

		ArrayType& Value()
		{
			return m_value;
		}

		const ArrayType& Value() const
		{
			return m_value;
		}

	private:

		ArrayType m_value;
	};

	template <typename TYPE> class CAny<CSharedArray<TYPE> > : public IAny
	{
	public:

		typedef CSharedArray<TYPE> ArrayType;

		inline CAny(const ArrayType &value)
			: m_arrayExtension(value)
		{}

		// IAny

		virtual CTypeInfo GetTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<ArrayType>();
		}

		virtual uint32 GetSize() const override
		{
			return sizeof(CAny);
		}

		virtual IAny* Clone(void* pPlacement) const override
		{
			return new (pPlacement) CAny(m_arrayExtension);
		}

		virtual IAnyPtr Clone() const override
		{
			return IAnyPtr(std::make_shared<CAny>(m_arrayExtension));
		}

		virtual bool ToString(const CharArrayView& output) const override
		{
			return Schematyc2::ToString(m_arrayExtension, output);
		}

		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) override
		{
			archive(m_arrayExtension, szName, szLabel);
			return true;
		}

		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const override
		{
			return GameSerialization::IContextPtr(std::make_shared<GameSerialization::CContext<const TYPE> >(archive, m_arrayExtension));
		}

		virtual IAnyExtension* QueryExtension(EAnyExtension extension) override
		{
			return extension == EAnyExtension::Array ? &m_arrayExtension : nullptr;
		}

		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const override
		{
			return extension == EAnyExtension::Array ? &m_arrayExtension : nullptr;
		}

		virtual void* ToVoidPtr() override
		{
			return &m_arrayExtension.Value();
		}

		virtual const void* ToVoidPtr() const override
		{
			return &m_arrayExtension.Value();
		}

		virtual IAny& operator = (const IAny& rhs) override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				m_arrayExtension.Value() = *static_cast<const ArrayType*>(rhs.ToVoidPtr());
			}
			return this;
		}

		virtual bool operator == (const IAny& rhs) const override
		{
			if(GetEnvTypeId<TYPE>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId())
			{
				return m_arrayExtension.Value() == *static_cast<const ArrayType*>(rhs.ToVoidPtr());
			}
			return false;
		}

		// ~IAny

		ArrayType& Value()
		{
			return m_arrayExtension.Value();
		}

		const ArrayType& Value() const
		{
			return m_arrayExtension.Value();
		}

	private:

		CAnyArrayExtension<ArrayType> m_arrayExtension;
	};

	template <> class CAny<void> : public IAny // #SchematycTODO : Do we still need a void specialization?
	{
	public:

		// IAny

		virtual uint32 GetSize() const override
		{
			return sizeof(CAny);
		}

		virtual CTypeInfo GetTypeInfo() const override
		{
			return Schematyc2::GetTypeInfo<void>();
		}

		virtual IAny* Clone(void* pPlacement) const override
		{
			return new (pPlacement) CAny();
		}

		virtual IAnyPtr Clone() const override
		{
			return IAnyPtr(std::make_shared<CAny>());
		}

		virtual bool ToString(const CharArrayView& output) const override
		{
			return false;
		}

		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) override
		{
			return true;
		}

		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const override
		{
			return GameSerialization::IContextPtr();
		}

		virtual IAnyExtension* QueryExtension(EAnyExtension extension) override
		{
			return nullptr;
		}

		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const override
		{
			return nullptr;
		}

		virtual void* ToVoidPtr() override
		{
			return nullptr;
		}

		virtual const void* ToVoidPtr() const override
		{
			return nullptr;
		}

		virtual IAny& operator = (const IAny& rhs) override
		{
			return *this;
		}

		virtual bool operator == (const IAny& rhs) const override
		{
			return GetEnvTypeId<void>() == rhs.GetTypeInfo().GetTypeId().AsEnvTypeId();
		}

		// ~IAny
	};

	template <typename TYPE> inline CAny<TYPE> MakeAny()
	{
		return CAny<TYPE>(DefaultInit<TYPE>());
	}

	template <typename TYPE> inline CAny<TYPE> MakeAny(const TYPE& value)
	{
		return CAny<TYPE>(value);
	}

	template <typename TYPE, uint32 SIZE> CAny<TYPE[SIZE]> MakeAny()
	{
		return CAny<TYPE[SIZE]>(DefaultInit<TYPE[SIZE]>());
	}

	template <typename TYPE, uint32 SIZE> CAny<TYPE[SIZE]> MakeAny(const TYPE (&value)[SIZE])
	{
		return CAny<TYPE[SIZE]>(value);
	}

	template <typename TYPE> inline IAnyPtr MakeAnyShared()
	{
		return IAnyPtr(std::make_shared<CAny<TYPE> >(DefaultInit<TYPE>()));
	}

	template <typename TYPE> inline IAnyPtr MakeAnyShared(const TYPE& value)
	{
		return IAnyPtr(std::make_shared<CAny<TYPE> >(value));
	}

	template <typename TYPE, uint32 SIZE> std::shared_ptr<CAny<TYPE[SIZE]> > MakeAnyShared()
	{
		return IAnyPtr(std::make_shared<CAny<TYPE[SIZE]> >(DefaultInit<TYPE[SIZE]>()));
	}

	template <typename TYPE, uint32 SIZE> std::shared_ptr<CAny<TYPE[SIZE]> > MakeAnyShared(const TYPE (&value)[SIZE])
	{
		return IAnyPtr(std::make_shared<CAny<TYPE[SIZE]> >(value));
	}

	template <typename TYPE> inline bool Serialize(Serialization::IArchive& archive, CAny<TYPE>& value, const char* szName, const char* szLabel)
	{
		return value.Serialize(archive, szName, szLabel);
	}
}
