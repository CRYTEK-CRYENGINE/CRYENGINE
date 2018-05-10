// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryReflection/IReflection.h>
#include <CryReflection/IFunctionDesc.h>
#include <CryReflection/ITypeDesc.h>
#include <CryExtension/CryGUID.h>
#include <CryString/CryString.h>

#include "DescExtension.h"

namespace Cry {
namespace Reflection {

class CFunctionParameterDesc : public IFunctionParameterDesc
{
public:
	CFunctionParameterDesc(const SFunctionParameterDesc& paramDesc)
		: m_flags(paramDesc.flags)
		, m_typeId(paramDesc.typeId)
		, m_index(paramDesc.index)
	{}

	// IFunctionParameterDesc
	virtual CryTypeId          GetTypeId() const override                  { return m_typeId; }
	virtual uint32             GetIndex() const override                   { return m_index; }

	virtual FunctionParamFlags GetFlags() const override                   { return m_flags; }
	virtual void               SetFlags(FunctionParamFlags flags) override { m_flags = flags; }

	virtual bool               IsConst() const override                    { return m_flags.Check(EFunctionParamFlags::IsConst); }
	virtual bool               IsReference() const override                { return m_flags.Check(EFunctionParamFlags::IsReference); }
	// ~IFunctionParameterDesc

private:
	FunctionParamFlags m_flags;
	CryTypeId          m_typeId;
	uint32             m_index;
};

class CReflectedFunctionDesc : public CExtensibleDesc<IFunctionDesc>
{
public:
	CReflectedFunctionDesc(const ITypeDesc* pReturnType, const CFunction& func, const ParamArray& params, const char* szLabel, const CryGUID& guid);
	CReflectedFunctionDesc(const ITypeDesc* pObjectType, CryTypeId returnTypeId, const CFunction& func, const ParamArray& params, const char* szLabel, const CryGUID& guid);

	// IFunctionDesc
	virtual const CryGUID&                GetGuid() const override                           { return m_guid; }

	virtual const char*                   GetLabel() const override                          { return m_label.c_str(); }
	virtual void                          SetLabel(const char* szLabel)     override         { m_label = szLabel; }

	virtual const char*                   GetDescription() const override                    { return m_description.c_str(); }
	virtual void                          SetDescription(const char* szDescription) override { m_description = szDescription; }

	virtual bool                          IsMemberFunction() const override                  { return (m_pObjectType != nullptr); }

	virtual const CFunction&              GetFunction() const override                       { return m_function; }

	virtual const ITypeDesc*              GetObjectTypeDesc() const override;
	virtual CryTypeId                     GetReturnTypeId() const override      { return m_returnTypeId; }

	virtual size_t                        GetParamCount() const override        { return m_params.size(); }
	virtual const IFunctionParameterDesc* GetParam(size_t index) const override { return index < m_params.size() ? &m_params[index] : nullptr; }
	// ~IFunctionDesc

private:
	CryGUID                             m_guid;
	CFunction                           m_function;

	string                              m_label;
	string                              m_description;

	const ITypeDesc*                    m_pObjectType;
	CryTypeId                           m_returnTypeId;
	std::vector<CFunctionParameterDesc> m_params;

	bool                                m_isConstFunction;
};

inline CReflectedFunctionDesc::CReflectedFunctionDesc(const ITypeDesc* pReturnType, const CFunction& func, const ParamArray& params, const char* szLabel, const CryGUID& guid)
	: m_function(func)
	, m_label(szLabel)
	, m_isConstFunction(false)
	, m_guid(guid)
{
	for (const SFunctionParameterDesc& desc : params)
	{
		m_params.emplace_back(desc);
	}
}

inline CReflectedFunctionDesc::CReflectedFunctionDesc(const ITypeDesc* pObjectType, CryTypeId returnTypeId, const CFunction& func, const ParamArray& params, const char* szLabel, const CryGUID& guid)
	: m_pObjectType(pObjectType)
	, m_returnTypeId(returnTypeId)
	, m_function(func)
	, m_label(szLabel)
	, m_isConstFunction(false)
	, m_guid(guid)
{
	for (const SFunctionParameterDesc& desc : params)
	{
		m_params.emplace_back(desc);
	}
}

} // ~Cry namespace
} // ~Reflection namespace
