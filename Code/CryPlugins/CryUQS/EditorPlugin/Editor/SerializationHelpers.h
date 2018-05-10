// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/STL.h>

//////////////////////////////////////////////////////////////////////////
// CFixedContainerSTL
//////////////////////////////////////////////////////////////////////////

template<class TContainer, class TElement>
class CFixedContainerSTL : public Serialization::ContainerSTL<TContainer, TElement>
{
	typedef Serialization::ContainerSTL<TContainer, TElement> BaseClass;
public:
	explicit CFixedContainerSTL(TContainer* pContainer = nullptr)
		: BaseClass(pContainer)
	{}

	virtual bool isFixedSize() const override { return true; }
};

template<typename TContainer>
CFixedContainerSTL<TContainer, typename TContainer::value_type> FixedContainerSTL(TContainer& container)
{
	typedef typename TContainer::value_type TElement;
	return CFixedContainerSTL<TContainer, TElement>(&container);
}

template<class TContainer, class TElement>
bool Serialize(yasli::Archive& ar, CFixedContainerSTL<TContainer, TElement>& ser, const char* szName, const char* szLabel)
{
	return ar(static_cast<Serialization::IContainer&>(ser), szName, szLabel);
}

//////////////////////////////////////////////////////////////////////////
// SerializeStringWithStringList
//////////////////////////////////////////////////////////////////////////

struct SValidatorKey
{
	template<typename T>
	static SValidatorKey UniqueFromType()
	{
		// TODO pavloi 2015.12.07: usually, we use just the address of a serialized object as validator search key.
		// But UQS editor uses many temporary objects, so the addresses start to clash. Hence, this temporary hack
		// until I will come up with better solution.
		static intptr_t counter = 0;

		const void* p = (const void*)++counter;
		return SValidatorKey(p, Serialization::TypeID::get<T>());
	}

	template<typename T>
	static SValidatorKey FromObject(const T& object)
	{
		return SValidatorKey(&object, Serialization::TypeID::get<T>());
	}

	explicit SValidatorKey(const void* pHandle, const Serialization::TypeID& typeId)
		: pHandle(pHandle)
		, typeId(typeId)
	{}

	const void*                 pHandle;
	const Serialization::TypeID typeId;
};

template<typename TSetterFunc>
bool SerializeWithStringList(
  Serialization::IArchive& archive, const char* szName, const char* szLabel,
  const Serialization::StringList& strList, const char* szOldValue, TSetterFunc setterFunc, const SValidatorKey* pRecord = nullptr)
{
	const int oldValueIdx = strList.find(szOldValue);
	const bool bIsNpos = (oldValueIdx == Serialization::StringList::npos);

	const SValidatorKey validatorKey =
	  pRecord
	  ? *pRecord
	  : SValidatorKey(szOldValue, Serialization::TypeID::get<const char*>());

	Serialization::StringListValue strValue(strList, !bIsNpos ? oldValueIdx : 0, validatorKey.pHandle, validatorKey.typeId);
	const int oldStrValueIndex = strValue.index();
	const bool bRes = archive(strValue, szName, szLabel);

	const bool bChanged = oldStrValueIndex != strValue.index();
	if (bIsNpos && !bChanged)
	{
		archive.error(validatorKey.pHandle, validatorKey.typeId, "Unable to find value <%s> in the list", szOldValue);
	}

	if (bChanged)
	{
		setterFunc(strValue.c_str());
	}

	return bRes;
}

template<typename TString>
bool SerializeStringWithStringList(
  Serialization::IArchive& archive, const char* szName, const char* szLabel,
  const Serialization::StringList& strList, TString& str)
{
	return SerializeWithStringList(archive, szName, szLabel,
	                               strList, str.c_str(), [&str](const char* szNewVal) { str = szNewVal; }, nullptr);
}

template<typename TString>
bool SerializeStringWithStringList(
  Serialization::IArchive& archive, const char* szName, const char* szLabel,
  const Serialization::StringList& strList, TString& str, const SValidatorKey& validatorKey)
{
	return SerializeWithStringList(archive, szName, szLabel,
	                               strList, str.c_str(), [&str](const char* szNewVal) { str = szNewVal; }, &validatorKey);
}

//////////////////////////////////////////////////////////////////////////
// CKeyValueStringList
//////////////////////////////////////////////////////////////////////////

template <class TData>
class CKeyValueStringList
{
public:

	template <class TFactoryDB>
	void FillFromFactoryDatabase(const TFactoryDB& factoryDB, bool bPrependEmptyEntry)
	{
		const size_t count = factoryDB.GetFactoryCount();

		if (bPrependEmptyEntry)
		{
			AddEntry("", CryGUID());
		}

		for (size_t i = 0; i < count; ++i)
		{
			const auto& factory = factoryDB.GetFactory(i);
			AddEntry(factory.GetName(), factory.GetGUID());
		}
	}

	template <class TFactory, class TFactoryDB>
	void FillFromFactoryDatabaseWithFilter(const TFactoryDB& factoryDB, bool bPrependEmptyEntry, const std::function<bool(const TFactory&)>& filterFunc)
	{
		const size_t count = factoryDB.GetFactoryCount();

		if (bPrependEmptyEntry)
		{
			AddEntry("", CryGUID());
		}

		for (size_t i = 0; i < count; ++i)
		{
			const auto& factory = factoryDB.GetFactory(i);
			if (filterFunc(factory))
			{
				AddEntry(factory.GetName(), factory.GetGUID());
			}
		}
	}

	void AddEntry(const char* szLabelForDisplayInDropDownList, const TData& dataBehind)
	{
		string label = szLabelForDisplayInDropDownList;
		m_entries.emplace_back(std::make_pair(label, dataBehind));
	}

	void Clear()
	{
		m_entries.clear();
	}

	struct SSerializeResult
	{
		bool bSuccess;
		int newIndex;
	};

	SSerializeResult SerializeByData(Serialization::IArchive& archive, const char* szName, const char* szLabel, const TData& oldValue, const std::function<void(const TData&)>& setterFunc) const
	{
		return SerializeInternal(archive, szName, szLabel, &oldValue, setterFunc, nullptr, nullptr);
	}

	SSerializeResult SerializeByLabelForDisplayInDropDownList(Serialization::IArchive& archive, const char* szName, const char* szLabel, const SValidatorKey* pRecord, const string& labelForDisplayInDropDownList) const
	{
		return SerializeInternal(archive, szName, szLabel, nullptr, nullptr, pRecord, &labelForDisplayInDropDownList);
	}

private:

	SSerializeResult SerializeInternal(Serialization::IArchive& archive, const char* szName, const char* szLabel, const TData* pOldValue, const std::function<void(const TData&)>& setterFunc, const SValidatorKey* pRecord, const string* pLabelForDisplayInDropDownList) const
	{
		assert( (pOldValue && !pLabelForDisplayInDropDownList ||
				(!pOldValue && pLabelForDisplayInDropDownList)));

		const bool bSerializeByData = (pOldValue != nullptr);

		Serialization::StringList strList;

		for (const auto& pair : m_entries)
		{
			strList.push_back(pair.first.c_str());
		}

		const int oldValueIdx = bSerializeByData ? FindIndexByData(*pOldValue) : FindIndexByLabel(pLabelForDisplayInDropDownList->c_str());

		const bool bIsNpos = (oldValueIdx == -1);

		const SValidatorKey validatorKey =
			pRecord
			? *pRecord
			: SValidatorKey(bSerializeByData ? static_cast<const void*>(pOldValue) : static_cast<const void*>(pLabelForDisplayInDropDownList), Serialization::TypeID::get<TData>());

		Serialization::StringListValue strValue(strList, !bIsNpos ? oldValueIdx : 0, validatorKey.pHandle, validatorKey.typeId);
		const int oldStrValueIndex = strValue.index();
		const bool bRes = archive(strValue, szName, szLabel);

		const bool bChanged = oldStrValueIndex != strValue.index();
		if (bIsNpos && !bChanged)
		{
			archive.error(validatorKey.pHandle, validatorKey.typeId, "Unable to find value in the list");
		}

		if (bChanged)
		{
			const int newIndex = FindIndexByLabel(strValue.c_str());
			//assert(newIndex != -1);
			if (newIndex != -1)
			{
				const TData& newValue = m_entries[newIndex].second;
				if (setterFunc)
				{
					setterFunc(newValue);
				}
			}
		}

		SSerializeResult res = { bRes, strValue.index() };
		return res;
	}

	int FindIndexByData(const TData& dataToSearchFor) const
	{
		int index = 0;

		for (const auto& pair : m_entries)
		{
			if (pair.second == dataToSearchFor)
				return index;
			++index;
		}

		return -1;
	}

	int FindIndexByLabel(const char* szLabelToSearchFor) const
	{
		int index = 0;

		for (const auto& pair : m_entries)
		{
			if (strcmp(pair.first.c_str(), szLabelToSearchFor) == 0)
				return index;
			++index;
		}

		return -1;
	}

private:

	std::vector<std::pair<string, TData>> m_entries;
};

template<typename TSetFunc>
bool SerializeGUIDWithSetter(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CryGUID& oldValue, TSetFunc setFunc)
{
	CryGUID guid = oldValue;
	const bool res = archive(guid, szName, szLabel);
	if (res && archive.isInput() && guid != oldValue)
	{
		setFunc(guid);
	}
	return res;
}
