// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/STL.h>

namespace Schematyc
{
namespace SerializationUtils
{
template<class CONTAINER_TYPE, class ELEMENT_TYPE, typename PREDICATE> class CContainerSerializer : public Serialization::IContainer
{
private:

	typedef typename CONTAINER_TYPE::iterator Iterator;

	struct SElementInitializer
	{
		inline SElementInitializer()
			: value()
		{}

		ELEMENT_TYPE value;
	};

public:

	explicit inline CContainerSerializer(CONTAINER_TYPE& container, PREDICATE predicate)
		: m_container(container)
		, m_predicate(predicate)
		, m_size(container.size())
		, m_pos(container.begin())
	{}

	// Serialization::IContainer

	virtual size_t size() const override
	{
		return m_container.size();
	}

	virtual size_t resize(size_t size) override
	{
		m_container.resize(size);
		m_size = size;
		m_pos = m_container.begin();
		return size;
	}

	virtual bool isFixedSize() const override
	{
		return false;
	}

	virtual void* pointer() const override
	{
		return reinterpret_cast<void*>(&m_container);
	}

	virtual Serialization::TypeID elementType() const override
	{
		return Serialization::TypeID::get<ELEMENT_TYPE>();
	}

	virtual Serialization::TypeID containerType() const override
	{
		return Serialization::TypeID::get<CONTAINER_TYPE>();
	}

	virtual bool next() override
	{
		if (m_pos != m_container.end())
		{
			++m_pos;
		}
		return m_pos != m_container.end();
	}

	virtual void* elementPointer() const override
	{
		return &*m_pos;
	}

	virtual bool operator()(Serialization::IArchive& archive, const char* szName, const char* szLabel) override
	{
		if (m_pos == m_container.end())
		{
			m_pos = m_container.insert(m_container.end(), ELEMENT_TYPE());
		}
		return m_predicate(archive, *m_pos, szName, szLabel);
	}

	virtual operator bool() const override
	{
		return true;
	}

	virtual void serializeNewElement(Serialization::IArchive& archive, const char* szName = "", const char* szLabel = nullptr) const override
	{
		SElementInitializer element;
		m_predicate(archive, element.value, szName, szLabel);
	}

	// ~Serialization::IContainer

protected:

	CONTAINER_TYPE& m_container;
	PREDICATE       m_predicate;
	size_t          m_size;
	Iterator        m_pos;
};

template<typename CONTAINER_TYPE, typename PREDICATE_TYPE> struct SContainerDecorator
{
	inline SContainerDecorator(CONTAINER_TYPE& _container, PREDICATE_TYPE _predicate)
		: container(_container)
		, predicate(_predicate)
	{}

	CONTAINER_TYPE& container;
	PREDICATE_TYPE  predicate;
};

template<typename CONTAINER_TYPE, typename PREDICATE_TYPE> bool Serialize(Serialization::IArchive& archive, SContainerDecorator<CONTAINER_TYPE, PREDICATE_TYPE>& value, const char* szName, const char* szLabel)
{
	CContainerSerializer<CONTAINER_TYPE, typename CONTAINER_TYPE::value_type, PREDICATE_TYPE> serializer(value.container, value.predicate);
	return archive(static_cast<Serialization::IContainer&>(serializer), szName, szLabel);
}

template<typename CONTAINER_TYPE, typename PREDICATE_TYPE> SContainerDecorator<CONTAINER_TYPE, PREDICATE_TYPE> Container(CONTAINER_TYPE& container, PREDICATE_TYPE predicate)
{
	return SContainerDecorator<CONTAINER_TYPE, PREDICATE_TYPE>(container, predicate);
}
} // SerializationUtils
} // Schematyc
