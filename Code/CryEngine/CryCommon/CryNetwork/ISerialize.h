// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h> // <> required for Interfuscator
#include <CryCore/CountedValue.h>
#include <CryCore/Containers/MiniQueue.h>
#include <CryCore/Containers/VectorSet.h>
#include <CryCore/Containers/VectorMap.h>
#include <CryCore/StlUtils.h>
#include <CrySystem/TimeValue.h>

template<class T, class U>
class InterpolatedValue_tpl;

//! Network identifier for an object that has been bound to the network
//! Each application instance will maintain a map to look up entity identifiers to their networked identifiers.
struct SNetObjectID
{
	static const uint16 InvalidId = ~uint16(0);

	SNetObjectID() : id(InvalidId), salt(0) {}
	SNetObjectID(uint16 i, uint16 s) : id(i), salt(s) {}

	uint16 id;
	uint16 salt;

	ILINE bool operator!() const
	{
		return id == InvalidId;
	}
	typedef uint16 SNetObjectID::* safe_bool_idiom_type;
	ILINE operator safe_bool_idiom_type() const
	{
		return !!(*this) ? &SNetObjectID::id : NULL;
	}
	ILINE bool operator!=(const SNetObjectID& rhs) const
	{
		return !(*this == rhs);
	}
	ILINE bool operator==(const SNetObjectID& rhs) const
	{
		return id == rhs.id && salt == rhs.salt;
	}
	ILINE bool operator<(const SNetObjectID& rhs) const
	{
		return id < rhs.id || (id == rhs.id && salt < rhs.salt);
	}
	ILINE bool operator>(const SNetObjectID& rhs) const
	{
		return id > rhs.id || (id == rhs.id && salt > rhs.salt);
	}

	bool IsLegal() const
	{
		return salt != 0;
	}

	const char* GetText(char* tmpBuf = 0) const
	{
		static char singlebuf[64];
		if (!tmpBuf)
		{
			tmpBuf = singlebuf;
		}
		if (id == InvalidId)
		{
			sprintf(tmpBuf, "<nil>");
		}
		else if (!salt)
		{
			sprintf(tmpBuf, "illegal:%u:%u", id, salt);
		}
		else
		{
			sprintf(tmpBuf, "%u:%u", id, salt);
		}
		return tmpBuf;
	}

	uint32 GetAsUint32() const
	{
		return (uint32(salt) << 16) | id;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }

	AUTO_STRUCT_INFO;
};

//! \cond INTERNAL
//! This enumeration details what "kind" of serialization we are performing.
//! It does this so that classes can (if they want to) tailor the data they present
//! depending on to where the data is being written.
enum ESerializationTarget
{
	eST_SaveGame,
	eST_Network,
	eST_Script
};

//! This inner class defines an interface so that OnUpdate functions can be passed abstractly through to concrete serialization classes.
struct ISerializeUpdateFunction
{
	// <interfuscator:shuffle>
	virtual ~ISerializeUpdateFunction(){}
	virtual void Execute() = 0;
	// </interfuscator:shuffle>
};

//! Concrete implementation of IUpdateFunction for a general functor class.
template<class F_Update>
class CSerializeUpdateFunction : public ISerializeUpdateFunction
{
public:
	CSerializeUpdateFunction(F_Update& update) : m_rUpdate(update) {}

	virtual void Execute()
	{
		m_rUpdate();
	}

private:
	F_Update m_rUpdate;
};

//! Temporary class for string serialization.
struct SSerializeString
{
	AUTO_STRUCT_INFO;

	SSerializeString() {};
	SSerializeString(const SSerializeString& src) { m_str.assign(src.c_str()); };
	explicit SSerializeString(const char* sbegin, const char* send) : m_str(sbegin, send) {};
	~SSerializeString() {}

	//! Casting to const char*.
	SSerializeString(const char* s) : m_str(s) {};

	SSerializeString& operator=(const SSerializeString& src)  { m_str.assign(src.c_str()); return *this; }
	SSerializeString& operator=(const char* src)              { m_str.assign(src); return *this; }

	bool              operator!=(const SSerializeString& src) { return m_str != src.m_str; }

	size_t      size() const                            { return m_str.size(); }
	size_t      length() const                          { return m_str.length(); }
	const char* c_str() const                           { return m_str.c_str(); };
	bool        empty() const                           { return m_str.empty(); }
	void        resize(int sz)                          { m_str.resize(sz); }
	void        reserve(int sz)                         { m_str.reserve(sz); }

	void        set_string(const string& s)
	{
		m_str.assign(s.begin(), s.size());
	}
#if !defined(RESOURCE_COMPILER)
	void set_string(const CryStringLocal& s)
	{
		m_str.assign(s.begin(), s.size());
	}
#endif
	template<size_t S>
	void set_string(const CryFixedStringT<S>& s)
	{
		m_str.assign(s.begin(), s.size());
	}

	operator const string() const { return m_str; }

private:
	string m_str;
};
//! \endcond

//! ISerialize is intended to be implemented by objects that need to read and write from various data sources in such a way that
//! different tradeoffs can be balanced by the object that is being serialized, and so that objects being serialized need only write
//! a single function in order to be read from and written to.
struct ISerialize
{
	static const int ENUM_POLICY_TAG = 0xe0000000;

	ILINE ISerialize() {}

	// <interfuscator:shuffle>
	virtual ~ISerialize(){}

	//! This is for string values -- they need special support.
	virtual void ReadStringValue(const char* name, SSerializeString& curValue, uint32 policy = 0) = 0;
	virtual void WriteStringValue(const char* name, SSerializeString& buffer, uint32 policy = 0) = 0;

	//! This function should be implemented to call the passed in interface.
	//! If we are reading, and to not call it if we are writing.
	virtual void Update(ISerializeUpdateFunction* pUpdate) = 0;

	//! For network updates: notify the network engine that this value was only partially read and we.
	//! Should re-request an update from the server soon.
	virtual void FlagPartialRead() = 0;

	//////////////////////////////////////////////////////////////////////////
	// These functions should be implemented to deal with groups.
	//! Begins a serialization group - must be matched by an EndGroup.
	//! SzName is preferably as short as possible for performance reasons.
	//! Spaces in szName cause undefined behaviour, use alpha characters,underscore and numbers only for a name.
	virtual void BeginGroup(const char* szName) = 0;
	virtual bool BeginOptionalGroup(const char* szName, bool condition) = 0;
	virtual void EndGroup() = 0;
	//////////////////////////////////////////////////////////////////////////

	virtual bool                 IsReading() const = 0;
	virtual bool                 ShouldCommitValues() const = 0;
	virtual ESerializationTarget GetSerializationTarget() const = 0;
	virtual bool                 Ok() const = 0;
	// </interfuscator:shuffle>

	//! Declare all primitive Value() implementations.
#define SERIALIZATION_TYPE(T) \
  virtual void Value(const char* name, T & x, uint32 policy) = 0;
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE

	//! Declare all primitive Value() implementations.
#define SERIALIZATION_TYPE(T) \
  virtual void ValueWithDefault(const char* name, T & x, const T &defaultValue) = 0;
#include "SerializationTypes.h"
	SERIALIZATION_TYPE(SSerializeString)
#undef SERIALIZATION_TYPE

	template<class B>
	void Value(const char* name, B& x)
	{
		Value(name, x, 0);
	}

	template<class B>
	void Value(const char* name, B& x, uint32 policy);

	template<class B>
	void ValueWithDefault(const char* name, B& x, const B& defaultValue);
};

//! \cond INTERNAL
//! Provide a wrapper around ISerialize to allow easy changing of our Interface, and easy implementation of our details.
//! It is a template so that we can wrap a more specific ISerialize implementation if necessary.
//! Some of the wrappers are trivial, however for consistency, they have been made to follow the trend.
template<class TISerialize>
class CSerializeWrapper
{
public:
	// Required for SWIG to properly expose serialize functions!
	CSerializeWrapper() : m_pSerialize(nullptr) {}

	CSerializeWrapper(TISerialize* pSerialize) :
		m_pSerialize(pSerialize)
	{
	}

	//! The value function allows us to declare that a value needs to be serialized/deserialized.
	//! We can pass a serialization policy in order to compress the value, and an update function to allow us to be informed of when this value is changed.
	template<typename T_Value>
	ILINE void Value(const char* szName, T_Value& value, int policy)
	{
		m_pSerialize->Value(szName, value, policy);
	}

	template<typename T_Value>
	ILINE void Value(const char* szName, T_Value& value)
	{
		m_pSerialize->Value(szName, value);
	}

	void Value(const char* szName, string& value, int policy)
	{
		if (IsWriting())
		{
			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->WriteStringValue(szName, serializeString, policy);
		}
		else
		{
			if (GetSerializationTarget() != eST_Script)
				value = "";

			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->ReadStringValue(szName, serializeString, policy);
			value = serializeString.c_str();
		}
	}
	ILINE void Value(const char* szName, string& value)
	{
		Value(szName, value, 0);
	}
	void Value(const char* szName, const string& value, int policy)
	{
		if (IsWriting())
		{
			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->WriteStringValue(szName, serializeString, policy);
		}
		else
		{
			assert(0 && "This function can only be used for Writing");
		}
	}
	ILINE void Value(const char* szName, const string& value)
	{
		Value(szName, value, 0);
	}
	template<typename T>
	void Value(const char* szName, CryStringLocalT<T>& value, int policy)
	{
		if (IsWriting())
		{
			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->WriteStringValue(szName, serializeString, policy);
		}
		else
		{
			if (GetSerializationTarget() != eST_Script)
				value = "";

			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->ReadStringValue(szName, serializeString, policy);
			value = serializeString.c_str();
		}
	}
	template<typename T>
	ILINE void Value(const char* szName, CryStringLocalT<T>& value)
	{
		Value(szName, value, 0);
	}
	template<typename T>
	void Value(const char* szName, const CryStringLocalT<T>& value, int policy)
	{
		if (IsWriting())
		{
			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->WriteStringValue(szName, serializeString, policy);
		}
		else
		{
			assert(0 && "This function can only be used for Writing");
		}
	}
	template<typename T>
	ILINE void Value(const char* szName, const CryStringLocalT<T>& value)
	{
		Value(szName, value, 0);
	}
	template<size_t S>
	void Value(const char* szName, CryFixedStringT<S>& value, int policy)
	{
		if (IsWriting())
		{
			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->WriteStringValue(szName, serializeString, policy);
		}
		else
		{
			if (GetSerializationTarget() != eST_Script)
				value = "";

			SSerializeString& serializeString = SetSharedSerializeString(value);
			m_pSerialize->ReadStringValue(szName, serializeString, policy);
			assert(serializeString.length() <= S);
			value = serializeString.c_str();
		}
	}
	template<size_t S>
	ILINE void Value(const char* szName, CryFixedStringT<S>& value)
	{
		Value(szName, value, 0);
	}
	template<class T_Class, typename T_Value>
	void Value(const char* szName, T_Class* pInst, T_Value (T_Class::* get)() const, void (T_Class::* set)(T_Value))
	{
		if (IsWriting())
		{
			T_Value temp = (pInst->*get)();
			Value(szName, temp);
		}
		else
		{
			T_Value temp;
			Value(szName, temp);
			(pInst->*set)(temp);
		}
	}

	void Value(const char* name, IScriptTable* pTable)
	{
		ScriptAnyValue any(pTable);
		Value(name, any);
	}

	void Value(const char* name, SmartScriptTable& pTable)
	{
		ScriptAnyValue any(pTable);
		Value(name, any);
		if (IsReading())
		{
			if (any.GetType() == EScriptAnyType::Table)
				pTable = any.GetScriptTable();
			else
				pTable = SmartScriptTable();
		}
	}

	template<class T, class TT>
	void Value(const char* name, InterpolatedValue_tpl<T, TT>& val)
	{
		if (IsWriting())
		{
			T a = val.Get();
			Value(name, a);
		}

		if (IsReading())
		{
			T a;
			Value(name, a);
			val.SetGoal(a);
		}
	}

	template<class T, class TT>
	void Value(const char* name, InterpolatedValue_tpl<T, TT>& val, int policy)
	{
		if (IsWriting())
		{
			T a = val.Get();
			Value(name, a, policy);
		}

		if (IsReading())
		{
			T a;
			Value(name, a, policy);
			val.SetGoal(a);
		}
	}

	template<class T>
	void Value(const char* name, CountedValue<T>& countedValue)
	{
		if (!BeginOptionalGroup(name, true)) return;
		if (IsWriting())
		{
			T rawValue = countedValue.Peek();
			Value("Value", rawValue);
			typename CountedValue<T>::TCountedID rawId = countedValue.GetLatestID();
			Value("Id", rawId, 'ui32');
		}

		if (IsReading())
		{
			T rawValue;
			Value("Value", rawValue);
			typename CountedValue<T>::TCountedID rawId;
			Value("Id", rawId, 'ui32');
			countedValue.UpdateDuringSerializationOnly(rawValue, rawId);
		}
		EndGroup();
	}

	template<class T>
	void Value(const char* name, CountedValue<T>& countedValue, int policy)
	{
		if (!BeginOptionalGroup(name, true)) return;
		if (IsWriting())
		{
			T rawValue = countedValue.Peek();
			Value("Value", rawValue, policy);
			typename CountedValue<T>::TCountedID rawId = countedValue.GetLatestID();
			Value("Id", rawId, 'ui32');
		}

		if (IsReading())
		{
			T rawValue;
			Value("Value", rawValue, policy);
			typename CountedValue<T>::TCountedID rawId;
			Value("Id", rawId, 'ui32');
			countedValue.UpdateDuringSerializationOnly(rawValue, rawId);
		}
		EndGroup();
	}

	bool ValueChar(const char* name, char* buffer, int len)
	{
		string temp;
		if (IsReading())
		{
			Value(name, temp);
			if ((int)temp.length() > len - 1)
			{
				return false;   // truncated read
			}
			memcpy(buffer, temp.data(), temp.length() + 1);
			buffer[len - 1] = 0;
		}
		else
		{
			temp = string(buffer, buffer + len);
			Value(name, temp);
		}
		return true;
	}

	template<typename T>
	void ValueWithDefault(const char* name, T& x, const T& defaultValue)
	{
		m_pSerialize->ValueWithDefault(name, x, defaultValue);
	}

	void ValueWithDefault(const char* szName, string& value, const string& defaultValue)
	{
		static SSerializeString defaultSerializeString;

		defaultSerializeString.set_string(defaultValue);

		SSerializeString& serializeString = SetSharedSerializeString(value);
		m_pSerialize->ValueWithDefault(szName, serializeString, defaultSerializeString);
		if (IsReading())
		{
			value = serializeString.c_str();
		}
	}

	template<class T_Class, typename T_Value, class T_SerializationPolicy>
	void Value(const char* szName, T_Class* pInst, T_Value (T_Class::* get)() const, void (T_Class::* set)(T_Value),
	           const T_SerializationPolicy& policy)
	{
		if (IsWriting())
		{
			T_Value temp = (pInst->*get)();
			Value(szName, temp, policy);
		}
		else
		{
			T_Value temp;
			Value(szName, temp, policy);
			(pInst->*set)(temp);
		}
	}

	//! A value that is written by referring to a map of key/value pairs - we receive the key, and write the value.
	template<class T_Key, class T_Map>
	void MappedValue(const char* szName, T_Key& value, const T_Map& mapper)
	{
		typedef typename T_Map::ValueType T_Value;

		if (IsWriting())
		{
			T_Value write = mapper.KeyToValue(value);
			Value(szName, write);
		}
		else
		{
			T_Value read;
			Value(szName, read);
			value = mapper.ValueToKey(read);
		}
	}

#define CONTAINER_VALUE(container_type, insert_function)                                                          \
  template<typename T_Value, class Allocator>                                                                     \
  void Value(const char* name, container_type<T_Value, Allocator> &cont)                                          \
  {                                                                                                               \
    if (!BeginOptionalGroup(name, true)) return;                                                                  \
    if (IsWriting())                                                                                              \
    {                                                                                                             \
      uint32 count = cont.size();                                                                                 \
      Value("Size", count);                                                                                       \
      for (typename container_type<T_Value, Allocator>::iterator iter = cont.begin(); iter != cont.end(); ++iter) \
      {                                                                                                           \
        BeginGroup("i");                                                                                          \
        T_Value value = *iter;                                                                                    \
        Value("v", value);                                                                                        \
        EndGroup();                                                                                               \
      }                                                                                                           \
    }                                                                                                             \
    else                                                                                                          \
    {                                                                                                             \
      cont.clear();                                                                                               \
      uint32 count = 0;                                                                                           \
      Value("Size", count);                                                                                       \
      while (count--)                                                                                             \
      {                                                                                                           \
        BeginGroup("i");                                                                                          \
        T_Value temp;                                                                                             \
        Value("v", temp);                                                                                         \
        cont.insert_function(temp);                                                                               \
        EndGroup();                                                                                               \
      }                                                                                                           \
    }                                                                                                             \
    EndGroup();                                                                                                   \
  }                                                                                                               \
  template<typename T_Value, class T_Map>                                                                         \
  void MappedValue(const char* name, container_type<T_Value> &cont, const T_Map &mapper)                          \
  {                                                                                                               \
    if (!BeginOptionalGroup(name, true)) return;                                                                  \
    if (IsWriting())                                                                                              \
    {                                                                                                             \
      uint32 count = cont.size();                                                                                 \
      Value("Size", count);                                                                                       \
      for (typename container_type<T_Value>::iterator iter = cont.begin(); iter != cont.end(); ++iter)            \
      {                                                                                                           \
        BeginGroup("i");                                                                                          \
        MappedValue("v", *iter, mapper);                                                                          \
        EndGroup();                                                                                               \
      }                                                                                                           \
    }                                                                                                             \
    else                                                                                                          \
    {                                                                                                             \
      cont.clear();                                                                                               \
      uint32 count = 0;                                                                                           \
      Value("Size", count);                                                                                       \
      while (count--)                                                                                             \
      {                                                                                                           \
        BeginGroup("i");                                                                                          \
        T_Value temp;                                                                                             \
        MappedValue("v", temp, mapper);                                                                           \
        cont.insert_function(temp);                                                                               \
        EndGroup();                                                                                               \
      }                                                                                                           \
    }                                                                                                             \
    EndGroup();                                                                                                   \
  }

#define PAIR_CONTAINER_VALUE(container_type, insert_function)                                                                           \
  template<typename T_Value1, typename T_Value2, class Allocator>                                                                       \
  void Value(const char* name, container_type<std::pair<T_Value1, T_Value2>, Allocator> &cont)                                          \
  {                                                                                                                                     \
    if (!BeginOptionalGroup(name, true)) return;                                                                                        \
    if (IsWriting())                                                                                                                    \
    {                                                                                                                                   \
      uint32 count = cont.size();                                                                                                       \
      Value("Size", count);                                                                                                             \
      for (typename container_type<std::pair<T_Value1, T_Value2>, Allocator>::iterator iter = cont.begin(); iter != cont.end(); ++iter) \
      {                                                                                                                                 \
        BeginGroup("i");                                                                                                                \
        T_Value1 value1 = iter->first;                                                                                                  \
        T_Value2 value2 = iter->second;                                                                                                 \
        Value("v1", value1);                                                                                                            \
        Value("v2", value2);                                                                                                            \
        EndGroup();                                                                                                                     \
      }                                                                                                                                 \
    }                                                                                                                                   \
    else                                                                                                                                \
    {                                                                                                                                   \
      cont.clear();                                                                                                                     \
      uint32 count = 0;                                                                                                                 \
      Value("Size", count);                                                                                                             \
      while (count--)                                                                                                                   \
      {                                                                                                                                 \
        BeginGroup("i");                                                                                                                \
        T_Value1 temp1;                                                                                                                 \
        T_Value2 temp2;                                                                                                                 \
        Value("v1", temp1);                                                                                                             \
        Value("v2", temp2);                                                                                                             \
        cont.insert_function(std::pair<T_Value1, T_Value2>(temp1, temp2));                                                              \
        EndGroup();                                                                                                                     \
      }                                                                                                                                 \
    }                                                                                                                                   \
    EndGroup();                                                                                                                         \
  }                                                                                                                                     \

	CONTAINER_VALUE(std::vector, push_back);
	CONTAINER_VALUE(std::list, push_back);
	CONTAINER_VALUE(std::set, insert);
	CONTAINER_VALUE(std::deque, push_back);
	CONTAINER_VALUE(VectorSet, insert);

	PAIR_CONTAINER_VALUE(std::list, push_back);
	PAIR_CONTAINER_VALUE(std::vector, push_back);

	template<typename T_Value>
	void DummyValues(uint32 numDummyValues)
	{
		T_Value dummyValue;
		for (uint32 i = 0; i < numDummyValues; ++i)
			Value("Value", dummyValue);
	}

	template<typename T_Value, uint8 N>
	void Value(const char* name, MiniQueue<T_Value, N>& cont)
	{
		if (!BeginOptionalGroup(name, true)) return;
		if (IsWriting())
		{
			uint32 count = cont.Size();
			Value("Size", count);
			for (typename MiniQueue<T_Value, N>::SIterator iter = cont.Begin(); iter != cont.End(); ++iter)
			{
				T_Value value = *iter;
				Value("Value", value);
			}
		}
		else
		{
			cont.Clear();
			uint32 count = 0;
			Value("Size", count);
			while (count--)
			{
				T_Value temp;
				Value("Value", temp);
				cont.Push(temp);
			}
		}
		DummyValues<T_Value>(cont.Capacity() - cont.Size());
		EndGroup();
	}
	template<typename T_Value, uint8 N, class T_Map>
	void MappedValue(const char* name, MiniQueue<T_Value, N>& cont, const T_Map& mapper)
	{
		if (!BeginOptionalGroup(name, true)) return;
		if (IsWriting())
		{
			uint8 count = cont.Size();
			Value("Size", count);
			for (typename MiniQueue<T_Value, N>::SIterator iter = cont.Begin(); iter != cont.End(); ++iter)
			{
				BeginGroup("i");
				MappedValue("Value", *iter, mapper);
				EndGroup();
			}
		}
		else
		{
			cont.Clear();
			uint8 count = 0;
			Value("Size", count);
			while (count--)
			{
				BeginGroup("i");
				T_Value temp;
				MappedValue("Value", temp, mapper);
				cont.Push(temp);
				EndGroup();
			}
		}
		EndGroup();
	}

#define MAP_CONTAINER_VALUE(container_type)                                                                   \
  template<typename T_Key, typename T_Value>                                                                  \
  void Value(const char* name, container_type<T_Key, T_Value> &cont)                                          \
  {                                                                                                           \
    if (!BeginOptionalGroup(name, true)) return;                                                              \
    if (IsWriting())                                                                                          \
    {                                                                                                         \
      uint32 count = (uint32)cont.size();                                                                     \
      Value("Size", count);                                                                                   \
      for (typename container_type<T_Key, T_Value>::iterator iter = cont.begin(); iter != cont.end(); ++iter) \
      {                                                                                                       \
        T_Key tempKey = iter->first;                                                                          \
        BeginGroup("pair");                                                                                   \
        Value("k", tempKey);                                                                                  \
        Value("v", iter->second);                                                                             \
        EndGroup();                                                                                           \
      }                                                                                                       \
    }                                                                                                         \
    else                                                                                                      \
    {                                                                                                         \
      cont.clear();                                                                                           \
      uint32 count;                                                                                           \
      Value("Size", count);                                                                                   \
      while (count--)                                                                                         \
      {                                                                                                       \
        std::pair<T_Key, T_Value> temp;                                                                       \
        BeginGroup("pair");                                                                                   \
        Value("k", temp.first);                                                                               \
        Value("v", temp.second);                                                                              \
        EndGroup();                                                                                           \
        cont.insert(temp);                                                                                    \
      }                                                                                                       \
    }                                                                                                         \
    EndGroup();                                                                                               \
  }

#define HASH_CONTAINER_VALUE(container_type)                                                                                           \
  template<typename T_Key, typename T_Value, typename T_Hash, typename T_Pred, typename T_Alloc>                                       \
  void Value(const char* name, container_type<T_Key, T_Value, T_Hash, T_Pred, T_Alloc> &cont)                                          \
  {                                                                                                                                    \
    if (!BeginOptionalGroup(name, true)) return;                                                                                       \
    if (IsWriting())                                                                                                                   \
    {                                                                                                                                  \
      uint32 count = (uint32)cont.size();                                                                                              \
      Value("Size", count);                                                                                                            \
      for (typename container_type<T_Key, T_Value, T_Hash, T_Pred, T_Alloc>::iterator iter = cont.begin(); iter != cont.end(); ++iter) \
      {                                                                                                                                \
        T_Key tempKey = iter->first;                                                                                                   \
        BeginGroup("pair");                                                                                                            \
        Value("k", tempKey);                                                                                                           \
        Value("v", iter->second);                                                                                                      \
        EndGroup();                                                                                                                    \
      }                                                                                                                                \
    }                                                                                                                                  \
    else                                                                                                                               \
    {                                                                                                                                  \
      cont.clear();                                                                                                                    \
      uint32 count;                                                                                                                    \
      Value("Size", count);                                                                                                            \
      while (count--)                                                                                                                  \
      {                                                                                                                                \
        std::pair<T_Key, T_Value> temp;                                                                                                \
        BeginGroup("pair");                                                                                                            \
        Value("k", temp.first);                                                                                                        \
        Value("v", temp.second);                                                                                                       \
        EndGroup();                                                                                                                    \
        cont.insert(temp);                                                                                                             \
      }                                                                                                                                \
    }                                                                                                                                  \
    EndGroup();                                                                                                                        \
  }
	MAP_CONTAINER_VALUE(std::map);
	MAP_CONTAINER_VALUE(std::multimap);
	MAP_CONTAINER_VALUE(VectorMap);

	HASH_CONTAINER_VALUE(std::unordered_map);

	template<typename T_Value>
	ILINE void EnumValue(const char* szName, T_Value& value,
	                     T_Value first, T_Value last)
	{
		int32 nValue = int32(value) - first;
		Value(szName, nValue, ISerialize::ENUM_POLICY_TAG | (last - first));
		value = T_Value(nValue + first);
	}
	template<typename T_Value, class T_Class>
	ILINE void EnumValue(const char* szName,
	                     T_Class* pClass, T_Value (T_Class::* GetValue)() const, void (T_Class::* SetValue)(T_Value),
	                     T_Value first, T_Value last)
	{
		bool w = IsWriting();
		int nValue;
		if (w)
			nValue = int32((pClass->*GetValue)()) - first;
		Value(szName, nValue, ISerialize::ENUM_POLICY_TAG | (last - first));
		if (!w)
			(pClass->*SetValue)(T_Value(nValue + first));
	}
	/*
	   //! We can request that a functor be called whenever our values are being updated by calling this function.
	   template <class F_Update>
	   ILINE void OnUpdate( F_Update& update )
	   {
	   CUpdateFunction<F_Update> func(update);
	   m_pSerialize->Update( &func );
	   }
	   template <class T>
	   ILINE void OnUpdate( T * pCls, void (T::*func)() )
	   {
	   class CFunc : public IUpdateFunction
	   {
	   public:
	    CFunc( T * pCls, void (T::*func)() ) : m_pCls(pCls), m_func(func) {}
	    virtual void Execute()
	    {
	      (m_pCls->*m_func)();
	    }
	   private:
	    T * m_pCls;
	    void (T::*m_func)();
	   };
	   CFunc ifunc( pCls, func );
	   m_pSerialize->Update( &ifunc );
	   }
	 */
	//! Groups help us find common data.
	ILINE void BeginGroup(const char* szName)
	{
		m_pSerialize->BeginGroup(szName);
	}
	ILINE bool BeginOptionalGroup(const char* szName, bool condition)
	{
		return m_pSerialize->BeginOptionalGroup(szName, condition);
	}
	ILINE void EndGroup()
	{
		m_pSerialize->EndGroup();
	}

	//! Fetch the serialization target.
	ILINE ESerializationTarget GetSerializationTarget() const
	{
		return m_pSerialize->GetSerializationTarget();
	}

	//! Returns true if this serialization context is currently performing a serialization pass (writing data from object to the serialization context).
	ILINE bool IsWriting() const
	{
		return !m_pSerialize->IsReading();
	}

	//! Returns true if this serialization context is currently performing a deserialization pass (reading data from serialization context to the object).
	ILINE bool IsReading() const
	{
		return m_pSerialize->IsReading();
	}

	ILINE bool ShouldCommitValues() const
	{
		assert(m_pSerialize->IsReading());
		return m_pSerialize->ShouldCommitValues();
	}

	ILINE bool Ok() const
	{
		return m_pSerialize->Ok();
	}

	friend ILINE TISerialize* GetImpl(CSerializeWrapper<TISerialize> ser)
	{
		return ser.m_pSerialize;
	}

	ILINE void FlagPartialRead()
	{
		m_pSerialize->FlagPartialRead();
	}

	operator CSerializeWrapper<ISerialize>()
	{
		return CSerializeWrapper<ISerialize>(m_pSerialize);
	}

	SSerializeString& SetSharedSerializeString(const string& str)
	{
		static SSerializeString serializeString;
		serializeString.set_string(str);
		return serializeString;
	}

#if !defined(RESOURCE_COMPILER)
	SSerializeString& SetSharedSerializeString(const CryStringLocal& str)
	{
		static SSerializeString serializeString;
		serializeString.set_string(str);
		return serializeString;
	}
#endif

	template<size_t S>
	SSerializeString& SetSharedSerializeString(const CryFixedStringT<S>& str)
	{
		static SSerializeString serializeString;
		serializeString.set_string(str);
		return serializeString;
	}

private:
	TISerialize* m_pSerialize;
};
//! \endcond

//! Default serialize class to use.
typedef CSerializeWrapper<ISerialize> TSerialize;

//! Simple struct to declare something serializable - useful for exposition.
struct ISerializable
{
	virtual ~ISerializable(){}
	virtual void SerializeWith(TSerialize) = 0;
};

template<class B>
void ISerialize::Value(const char* name, B& x, uint32 policy)
{
	if (!BeginOptionalGroup(name, true)) return;
	TSerialize ser(this);
	x.Serialize(ser);
	EndGroup();
}

//! Based off ValueWithDefault in SimpleSerialize.h.
template<class B>
void ISerialize::ValueWithDefault(const char* name, B& x, const B& defaultValue)
{
	if (BeginOptionalGroup(name, x != defaultValue))
	{
		TSerialize ser(this);
		x.Serialize(ser);
		EndGroup();
	}
	else if (IsReading())
	{
		x = defaultValue;
	}
}

//! \cond INTERNAL
//! Used to automatically Begin/End group in serialization stream.
struct SSerializeScopedBeginGroup
{
	SSerializeScopedBeginGroup(TSerialize& ser, const char* sGroupName)
	{
		m_pSer = &ser;
		m_pSer->BeginGroup(sGroupName);
	}
	~SSerializeScopedBeginGroup()
	{
		m_pSer->EndGroup();
	}
private:
	TSerialize* m_pSer;
};
//! \endcond