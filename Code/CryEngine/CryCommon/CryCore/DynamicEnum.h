// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Enum.h>

//
// Types for associating info with each enum entry
//

// Default enum info class is empty
struct EmptyEnumInfo
{
	// All enum infos must return a step value (amount to increment enum counter after entry)
	static int step() { return 1; }
};

///////////////////////////////////////////////////////////////////////////////
// Create a unique DynamicEnum type from a unique Info type. Examples:
//   using ESomething = DynamicEnum<struct SDummySomething, uint8>;  // no per-enum info
//   struct SThingInfo {...}; // define enum info
//   using EThing = DynamicEnum<SThingInfo>;
template<typename Tag, typename V = int, typename I = EmptyEnumInfo>
class DynamicEnum
{
public:

	typedef V Value;
	typedef I Info;

	// static Container maintains enum names, values, and info
	class Container
	{
	public:
		Container(Serialization::EnumDescription& desc)
			: _desc(desc), _nextValue(0) {}

		Value add(Value value, cstr name, cstr label = 0, const Info& info = Info())
		{
			if (name)
			{
				if (!label)
				{
					string labelStr = Serialization::NameToLabel(name);
					if (labelStr == name)
						label = name;
					else
					{
						_customLabels.push_back(labelStr);
						label = _customLabels.back();
					}
				}

				_desc.add(value, name, label);
			}

			_infos.set(value, info);

			_nextValue = value + info.step();
			return value;
		}

		Value add(cstr name, cstr label = 0, const Info& info = Info())
		{
			return add(_nextValue, name, label, info);
		}

		Value       count() const            { return _desc.count(); }             // Number of elements
		Value       size() const             { return _nextValue; }                // Upper bound of values
		Value       find(cstr name) const    { return _desc.value(name); }         // Look up from name
		Value       value(Value index) const { return _desc.valueByIndex(index); } // Get value from index
		const Info& info(Value value) const  { return _infos.get(value); }         // Retrieve per-value info

		// Serialization
		bool Serialize(Serialization::IArchive& ar, Value& value, cstr name, cstr label) const
		{
			int i = check_cast<int>(value);
			if (!_desc.serialize(ar, i, name, label))
				return false;
			if (ar.isInput())
				value = check_cast<Value>(i);
			return true;
		}

	private:

		// Type for maintaining per-enum info
		struct FullInfos : FastDynArray<Info>
		{
			void set(size_t i, const Info& info)
			{
				size_t dim = info.step();
				if (i + dim > (size_t)this->size())
					this->resize(i + dim);
				while (dim-- > 0)
					(*this)[i + dim] = info;
			}
			const Info& get(size_t i) const
			{
				return (*this)[i];
			}
		};

		struct EmptyInfos
		{
			static void        set(size_t i, const Info&) {}
			static const Info& get(size_t i)              { static Info info; return info; }
		};

		using Infos = typename std::conditional<
		        std::is_empty<Info>::value,
		        EmptyInfos,
		        FullInfos
		        >::type;

		Serialization::EnumDescription& _desc;
		Value                           _nextValue;
		Infos                           _infos;
		std::vector<string>             _customLabels;
	};

	// Static functions
	ILINE static Serialization::EnumDescription& desc()
	{
		return Serialization::getEnumDescription<DynamicEnum<Tag, V, I>>();
	}

	ILINE static Container& container()
	{
		static Container _container(desc());
		return _container;
	}

	ILINE static Value       count() { return container().count(); }
	ILINE static DynamicEnum size()  { return DynamicEnum(container().size()); }

	// Construction
	DynamicEnum(cstr name, cstr label, const Info& info = Info())
		: _value(container().add(name, label, info)) {}
	DynamicEnum(Value val, cstr name, cstr label, const Info& info = Info())
		: _value(container().add(val, name, label, info)) {}

	template<typename Name>
	DynamicEnum(Name*, cstr name, cstr label = 0, const Info& info = Info())
	{
		static Value val = container().add(name, label, info);
		_value = val;
	}

	// Convert to/from Value
	explicit DynamicEnum(Value i = 0) : _value(i) {}

	operator Value() const { return _value; }
	Value value() const { return _value; }

	// Retrieve info
	cstr        name() const  { return desc().name(_value); }
	cstr        label() const { return desc().label(_value); }
	const Info& info() const  { return container().info(_value); }

	// Convenient iteration:
	//   for (auto e : ESomething::indices()) {}     // iterate all indices up to max value
	//   for (auto e : ESomething::values()) {}      // iterate only the added enum values
	template<bool bValues>
	struct iterator
	{
		Value index;
		iterator(Value i) : index(i) {}
		void        operator++()                 { ++index; }
		void        operator--()                 { --index; }
		bool        operator==(iterator r) const { return index == r.index; }
		bool        operator!=(iterator r) const { return index != r.index; }
		DynamicEnum operator*() const            { return DynamicEnum(bValues ? container().value(index) : index); }
	};

	struct indices
	{
		static iterator<false> begin() { return iterator<false>(0); }
		static iterator<false> end()   { return iterator<false>(container().size()); }
	};

	struct values
	{
		static iterator<true> begin() { return iterator<true>(0); }
		static iterator<true> end()   { return iterator<true>(container().count()); }
	};

	DynamicEnum operator+(Value i) const { return DynamicEnum(_value + i); }
	DynamicEnum operator-(Value i) const { return DynamicEnum(_value - i); }

private:

	Value _value;
};

template<typename Tag, typename Value, typename Info>
bool Serialize(Serialization::IArchive& ar, DynamicEnum<Tag, Value, Info>& value, cstr name, cstr label)
{
	return value.container().Serialize(ar, reinterpret_cast<Value&>(value), name, label);
}

///////////////////////////////////////////////////////////////////////////////
// DynamicEnum helper classes:
// Arrays of data sized to DynamicEnum size, replacements for staticly sized: T EnumData[EType::count];

//! Array which sizes itself to current DynamicEnum size on initialization.
template<typename T, typename Enum>
class StaticEnumArray : public DynArray<T>
{
public:

	StaticEnumArray()
	{
		this->resize(Enum::size(), T());
	}
};

//! Array which sizes itself to current DynamicEnum size on initialization, and every read/write.
template<typename T, typename Enum>
class DynamicEnumArray : public StaticEnumArray<T, Enum>
{
public:

	// override access
	T operator[](size_t i) const
	{
		return i < this->size() ? this->at(i) : T();
	}
	T& operator[](size_t i)
	{
		assert(i <= Enum::size());
		if (i >= this->size())
			this->resize(Enum::size(), T());
		return this->at(i);
	}
};
