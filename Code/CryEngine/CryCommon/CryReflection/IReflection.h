// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>
#include <CryExtension/CryGUID.h>
#include <CryType/Type.h>

#ifdef CRYREFLECTION_EXPORTS
	#define CRY_REFLECTION_API DLL_EXPORT
#else
	#define CRY_REFLECTION_API DLL_IMPORT
#endif

namespace Cry {
namespace Reflection {

struct IReflection;
struct ITypeDesc;
class CDescExtension;

inline IReflection& GetReflectionRegistry()
{
	CRY_ASSERT_MESSAGE(gEnv->pReflection, "Reflection module not yet initialized.");
	return *gEnv->pReflection;
}

template<typename TYPE, TYPE INVALID_INDEX>
struct Index
{
	typedef TYPE ValueType;

	static const ValueType Invalid = INVALID_INDEX;

	Index()
		: m_value(Invalid)
	{}

	Index(ValueType index)
		: m_value(index)
	{}

	Index(const Index& rh)
		: m_value(rh.m_value)
	{}

	bool IsValid() const
	{
		return (m_value != Invalid);
	}

	operator ValueType() const
	{
		return m_value;
	}

	Index& operator++()
	{
		++m_value;
		return *this;
	}

	Index& operator--()
	{
		--m_value;
		return *this;
	}

	Index operator++(int)
	{
		Index tmp(m_value);
		++m_value;
		return tmp;
	}

	Index operator--(int)
	{
		Index tmp(m_value);
		--m_value;
		return tmp;
	}

	Index& operator+=(Index index)
	{
		m_value += index.m_value;
		return *this;
	}

	Index& operator-=(Index index)
	{
		m_value -= index.m_value;
		return *this;
	}

	Index& operator+=(ValueType value)
	{
		m_value += value;
		return *this;
	}

	Index& operator-=(ValueType value)
	{
		m_value -= value;
		return *this;
	}

	Index operator+(Index index) const
	{
		return Index(m_value + index.m_value);
	}

	Index& operator+(ValueType value) const
	{
		return Index(m_value + value);
	}

	Index operator-(Index index) const
	{
		return Index(m_value - index.m_value);
	}

	Index& operator-(ValueType value) const
	{
		return Index(m_value - value);
	}

private:
	ValueType m_value;
};

typedef Index<size_t, ~0> TypeIndex;
typedef Index<size_t, ~0> ExtensionIndex;

struct SSourceFileInfo
{
	SSourceFileInfo()
		: m_line(~0)
	{}

	SSourceFileInfo(const char* szFile, uint32 line, const char* szFunction = "")
		: m_file(szFile)
		, m_line(line)
		, m_function(szFunction)
	{}

	SSourceFileInfo(const SSourceFileInfo& srcPos)
		: m_file(srcPos.GetFile())
		, m_line(srcPos.GetLine())
		, m_function(srcPos.GetFunction())
	{}

	const char* GetFile() const     { return m_file.c_str(); }
	uint32      GetLine() const     { return m_line; }
	const char* GetFunction() const { return m_function.c_str(); }

	// Operators
	SSourceFileInfo& operator=(const SSourceFileInfo& rhs)
	{
		m_file = rhs.GetFile();
		m_line = rhs.GetLine();
		m_function = rhs.GetFunction();

		return *this;
	}

	bool operator==(const SSourceFileInfo& rhs) const
	{
		return (m_file == rhs.m_file && m_line == rhs.m_line && m_function == rhs.m_function);
	}

	bool operator!=(const SSourceFileInfo& rhs) const
	{
		return (m_file != rhs.m_file || m_line != rhs.m_line || m_function != rhs.m_function);
	}

private:
	string m_file;
	string m_function;
	int32  m_line;
};

#define __SOURCE_INFO__ Cry::Reflection::SSourceFileInfo(__FILE__, __LINE__, __FUNCTION__)

struct ISystemTypeRegistry
{
	virtual ~ISystemTypeRegistry() {}

	virtual bool                 UseType(const ITypeDesc& typeDesc) = 0;

	virtual const CryGUID&       GetGuid() const = 0;
	virtual const char*          GetLabel() const = 0;
	virtual const CryTypeDesc&   GetTypeDesc() const = 0;
	virtual CryTypeId            GetTypeId() const = 0;

	virtual TypeIndex::ValueType GetTypeCount() const = 0;
	virtual const ITypeDesc*     FindTypeByIndex(TypeIndex index) const = 0;
	virtual const ITypeDesc*     FindTypeByGuid(const CryGUID& guid) const = 0;
	virtual const ITypeDesc*     FindTypeById(CryTypeId typeId) const = 0;
};

typedef void (* ReflectTypeFunction)(ITypeDesc& typeDesc);

struct IReflection : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IReflection, "4465B4A8-4E5F-4813-85E0-A187A849AA7B"_cry_guid);

	virtual ~IReflection() {}

	virtual ITypeDesc*           Register(const CryTypeDesc& typeDesc, const CryGUID& guid, ReflectTypeFunction pReflectFunc, const SSourceFileInfo& srcPos) = 0;

	virtual TypeIndex::ValueType GetTypeCount() const = 0;
	virtual const ITypeDesc*     FindTypeByIndex(TypeIndex index) const = 0;
	virtual const ITypeDesc*     FindTypeByGuid(CryGUID guid) const = 0;
	virtual const ITypeDesc*     FindTypeById(CryTypeId typeId) const = 0;

	virtual size_t               GetSystemRegistriesCount() const = 0;
	virtual ISystemTypeRegistry* FindSystemRegistryByIndex(size_t index) const = 0;
	virtual ISystemTypeRegistry* FindSystemRegistryByGuid(const CryGUID& guid) const = 0;
	virtual ISystemTypeRegistry* FindSystemRegistryById(CryTypeId typeId) const = 0;

	virtual void                 RegisterSystemRegistry(ISystemTypeRegistry* pSystemRegistry) = 0;
};

struct IObject
{
	virtual ~IObject() {}

	virtual bool             IsReflected() const = 0;

	virtual CryTypeId        GetTypeId() const = 0;
	virtual TypeIndex        GetTypeIndex() const = 0;
	virtual const ITypeDesc* GetTypeDesc() const = 0;

	virtual bool             IsEqualType(const IObject* pObject) const = 0;
	virtual bool             IsEqualObject(const IObject* pOtherObject) const = 0;
};

} // ~Reflection namespace
} // ~Cry namespace
