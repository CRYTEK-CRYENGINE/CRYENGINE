// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Util/Variable.h"
#include "Serialization.h"

namespace Serialization
{
class CVariableIArchive
	: public IArchive
{
public:
	CVariableIArchive(const _smart_ptr<IVariable>& pVariable);
	virtual ~CVariableIArchive();

	// IArchive
	virtual bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(IString& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(IWString& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(float& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(double& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(int16& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(uint16& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(int32& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(uint32& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(int64& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(uint64& value, const char* name = "", const char* label = 0) override;

	virtual bool operator()(int8& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(uint8& value, const char* name = "", const char* label = 0) override;
	virtual bool operator()(char& value, const char* name = "", const char* label = 0);

	virtual bool operator()(const SStruct& ser, const char* name = "", const char* label = 0) override;
	virtual bool operator()(IContainer& ser, const char* name = "", const char* label = 0) override;
	//virtual bool operator()( IPointer& ptr, const char* name = "", const char* label = 0 ) override;
	// ~IArchive

	using IArchive::operator();

private:
	bool SerializeResourceSelector(const SStruct& ser, const char* name, const char* label);

	bool SerializeStruct(const SStruct& ser, const char* name, const char* label);

	bool SerializeStringListStaticValue(const SStruct& ser, const char* name, const char* label);

	bool SerializeRangeFloat(const SStruct& ser, const char* name, const char* label);
	bool SerializeRangeInt(const SStruct& ser, const char* name, const char* label);
	bool SerializeRangeUInt(const SStruct& ser, const char* name, const char* label);

private:
	_smart_ptr<IVariable> m_pVariable;
	int                   m_childIndexOverride;

	typedef bool (CVariableIArchive::*                 StructHandlerFunctionPtr)(const SStruct&, const char*, const char*);
	typedef std::map<string, StructHandlerFunctionPtr> HandlersMap;
	HandlersMap m_structHandlers;   // TODO: have only one of these.
};
}

