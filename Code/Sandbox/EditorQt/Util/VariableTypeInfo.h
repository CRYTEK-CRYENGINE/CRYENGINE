// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VariableTypeInfo_h__
#define __VariableTypeInfo_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Util/Variable.h"
#include "UIEnumsDatabase.h"
#include <CryCore/CryTypeInfo.h>

//////////////////////////////////////////////////////////////////////////
// Adaptors for TypeInfo-defined variables to IVariable

inline string SpacedName(const char* sName)
{
	// Split name with spaces.
	string sSpacedName = sName;
	for (int i = 1; i < sSpacedName.GetLength(); i++)
	{
		if (isupper(sSpacedName.GetAt(i)) && islower(sSpacedName.GetAt(i - 1)))
		{
			sSpacedName.Insert(i, ' ');
			i++;
		}
	}
	return sSpacedName;
}

//////////////////////////////////////////////////////////////////////////
// Scalar variable
//////////////////////////////////////////////////////////////////////////

class CVariableTypeInfo : public CVariableBase
{
public:
	// Dynamic constructor function
	static IVariable* Create(CTypeInfo::CVarInfo const& VarInfo, void* pBaseAddress, const void* pBaseAddressDefault);

	static EType      GetType(CTypeInfo const& TypeInfo)
	{
		// Translation to editor type values is currently done with some clunky type and name testing.
		if (TypeInfo.HasSubVars())
		{
			if (TypeInfo.IsType<Vec3>() && !TypeInfo.NextSubVar(0)->Type.IsType<Vec3>())
				// This is a vector type (and not a sub-classed vector type)
				return VECTOR;
			else if (TypeInfo.IsType<TRangeParam<float>>())
				return VECTOR2;
			else
				return ARRAY;
		}
		else if (TypeInfo.IsType<bool>())
			return BOOL;
		else if (TypeInfo.IsType<int>() || TypeInfo.IsType<uint>())
			return INT;
		else if (TypeInfo.IsType<float>())
			return FLOAT;
		else
			// Edit everything else as a string.
			return STRING;
	}

	CVariableTypeInfo(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault, EType eType)
		: m_pVarInfo(&VarInfo), m_pData(pAddress), m_pDefaultData(pAddressDefault)
	{
		SetName(SpacedName(VarInfo.GetName()));
		SetTypes(VarInfo.Type, eType);
		SetFlags(IVariable::UI_UNSORTED | IVariable::UI_HIGHLIGHT_EDITED);
		SetDescription(VarInfo.GetComment());
	}

	void SetTypes(CTypeInfo const& TypeInfo, EType eType)
	{
		m_pTypeInfo = &TypeInfo;
		m_eType = eType;

		SetDataType(DT_SIMPLE);
		if (m_eType == VECTOR)
		{
			if (m_name == "Color")
				SetDataType(DT_COLOR);
		}
		else if (m_eType == STRING)
		{
			if (m_name == "Texture")
				SetDataType(DT_TEXTURE);
			else if (m_name == "Material")
				SetDataType(DT_MATERIAL);
			else if (m_name == "Geometry")
				SetDataType(DT_OBJECT);
			else if (m_name == "Start Trigger" || m_name == "Stop Trigger")
				SetDataType(DT_AUDIO_TRIGGER);
			else if (m_name == "GeometryCache")
				SetDataType(DT_GEOM_CACHE);
		}
	}

	// IVariable implementation.
	virtual EType GetType() const { return m_eType; }
	virtual int   GetSize() const { return m_pTypeInfo->Size; }

	virtual void  GetLimits(float& fMin, float& fMax, float& fStep, bool& bHardMin, bool& bHardMax)
	{
		// Get hard limits from variable type, or vector element type.
		const CTypeInfo* pLimitType = m_pTypeInfo;
		if ((m_eType == VECTOR || m_eType == VECTOR2) && m_pTypeInfo->NextSubVar(0))
			pLimitType = &m_pTypeInfo->NextSubVar(0)->Type;
		bHardMin = pLimitType->GetLimit(eLimit_Min, fMin);
		bHardMax = pLimitType->GetLimit(eLimit_Max, fMax);
		pLimitType->GetLimit(eLimit_Step, fStep);

		// Check var attrs for additional limits.
		if (m_pVarInfo->GetAttr("SoftMin", fMin))
			bHardMin = false;
		else if (m_pVarInfo->GetAttr("Min", fMin))
			bHardMin = true;

		if (m_pVarInfo->GetAttr("SoftMax", fMax))
			bHardMax = false;
		else if (m_pVarInfo->GetAttr("Max", fMax))
			bHardMax = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Access operators.
	//////////////////////////////////////////////////////////////////////////

	virtual void Set(const char* value)
	{
		if (m_dataType == DT_COLOR)
		{
			Color3F col;
			if (TypeInfo(&col).FromString(&col, value))
			{
				col /= 255.f;
				m_pTypeInfo->FromValue(m_pData, col);
			}
		}
		else
			m_pTypeInfo->FromString(m_pData, value);
		OnSetValue(false);
	}
	virtual void Set(const string& value)
	{
		Set((const char*)value);
	}
	virtual void Set(float value)
	{
		m_pTypeInfo->FromValue(m_pData, value);
		OnSetValue(false);
	}
	virtual void Set(int value)
	{
		m_pTypeInfo->FromValue(m_pData, value);
		OnSetValue(false);
	}
	virtual void Set(bool value)
	{
		m_pTypeInfo->FromValue(m_pData, value);
		OnSetValue(false);
	}
	virtual void Set(const Vec2& value)
	{
		m_pTypeInfo->FromValue(m_pData, value);
		OnSetValue(false);
	}
	virtual void Set(const Vec3& value)
	{
		m_pTypeInfo->FromValue(m_pData, value);
		OnSetValue(false);
	}

	virtual void Get(CString& value) const
	{
		string svalue;
		Get(svalue);
		value = (const char*)svalue;
	}
	virtual void Get(string& value) const
	{
		if (m_dataType == DT_COLOR)
		{
			Color3F clr;
			if (m_pTypeInfo->ToValue(m_pData, clr))
			{
				clr *= 255.f;
				value = TypeInfo(&clr).ToString(&clr);
			}
		}
		else
			value = m_pTypeInfo->ToString(m_pData);
	}
	virtual void Get(float& value) const
	{
		m_pTypeInfo->ToValue(m_pData, value);
	}
	virtual void Get(int& value) const
	{
		m_pTypeInfo->ToValue(m_pData, value);
	}
	virtual void Get(bool& value) const
	{
		m_pTypeInfo->ToValue(m_pData, value);
	}
	virtual void Get(Vec2& value) const
	{
		m_pTypeInfo->ToValue(m_pData, value);
	}
	virtual void Get(Vec3& value) const
	{
		m_pTypeInfo->ToValue(m_pData, value);
	}
	virtual bool HasDefaultValue() const
	{
		return m_pTypeInfo->ValueEqual(m_pData, m_pDefaultData);
	}

	virtual IVariable* Clone(bool bRecursive) const
	{
		// Simply use a string var for universal conversion.
		IVariable* pClone = new CVariable<string>;
		string str;
		Get(str);
		pClone->Set(str);
		return pClone;
	}

	// To do: This could be more efficient ?
	virtual void CopyValue(IVariable* fromVar)
	{
		assert(fromVar);
		string str;
		fromVar->Get(str);
		Set(str);
	}

protected:
	CTypeInfo::CVarInfo const* m_pVarInfo;
	CTypeInfo const*           m_pTypeInfo;    // TypeInfo system structure for this var.
	void*                      m_pData;        // Existing address in memory. Directly modified.
	const void*                m_pDefaultData; // Address of default data for this var.
	EType                      m_eType;        // Type info for editor.
};

//////////////////////////////////////////////////////////////////////////
// Enum variable
//////////////////////////////////////////////////////////////////////////

class CVariableTypeInfoEnum : public CVariableTypeInfo
{
	struct CTypeInfoEnumList : IVarEnumList
	{
		CTypeInfo const& TypeInfo;

		CTypeInfoEnumList(CTypeInfo const& info)
			: TypeInfo(info)
		{
		}

		virtual const char* GetItemName(uint index) { return TypeInfo.EnumElem(index); }
	};

public:

	// Constructor.
	CVariableTypeInfoEnum(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault, IVarEnumList* pEnumList = 0)
		: CVariableTypeInfo(VarInfo, pAddress, pAddressDefault, UNKNOWN)
	{
		// Use custom enum, or enum defined in TypeInfo.
		m_enumList = pEnumList ? pEnumList : new CTypeInfoEnumList(VarInfo.Type);
	}

	//////////////////////////////////////////////////////////////////////////
	// Additional IVariable implementation.
	//////////////////////////////////////////////////////////////////////////

	IVarEnumList* GetEnumList() const
	{
		return m_enumList;
	}

protected:

	TSmartPtr<IVarEnumList> m_enumList;
};

//////////////////////////////////////////////////////////////////////////
// Spline variable
//////////////////////////////////////////////////////////////////////////

class CVariableTypeInfoSpline : public CVariableTypeInfo
{
	mutable ISplineInterpolator* m_pSpline;

public:

	// Constructor.
	CVariableTypeInfoSpline(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault, ISplineInterpolator* pSpline)
		: CVariableTypeInfo(VarInfo, pAddress, pAddressDefault, STRING), m_pSpline(pSpline)
	{
		if (m_pSpline && m_pSpline->GetNumDimensions() == 3)
			SetDataType(DT_CURVE | DT_COLOR);
		else
			SetDataType(DT_CURVE | DT_PERCENT);
	}
	~CVariableTypeInfoSpline()
	{
		delete m_pSpline;
	}
	virtual ISplineInterpolator* GetSpline(bool) const
	{
		delete m_pSpline;
		m_pSpline = 0;
		m_pTypeInfo->ToValue(m_pData, m_pSpline);
		return m_pSpline;
	}
};

//////////////////////////////////////////////////////////////////////////
// Struct variable
// Inherits implementation from CVariableArray
//////////////////////////////////////////////////////////////////////////

class CVariableTypeInfoStruct : public CVariableTypeInfo
{
public:

	// Constructor.
	CVariableTypeInfoStruct(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault)
		: CVariableTypeInfo(VarInfo, pAddress, pAddressDefault, ARRAY)
	{
		ProcessSubStruct(VarInfo, pAddress, pAddressDefault);
	}

	void ProcessSubStruct(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault)
	{
		ForAllSubVars(pSubVar, VarInfo.Type)
		{
			if (!*pSubVar->GetName())
			{
				EType eType = GetType(pSubVar->Type);
				if (eType == ARRAY)
				{
					// Recursively process nameless or base struct.
					ProcessSubStruct(*pSubVar, pSubVar->GetAddress(pAddress), pSubVar->GetAddress(pAddressDefault));
				}
				else if (pSubVar == VarInfo.Type.NextSubVar(0))
				{
					// Inline edit first sub-var in main field.
					SetTypes(pSubVar->Type, eType);
				}
			}
			else
			{
				IVariable* pVar = CVariableTypeInfo::Create(*pSubVar, pAddress, pAddressDefault);
				m_Vars.push_back(pVar);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// IVariable implementation.
	//////////////////////////////////////////////////////////////////////////

	virtual string GetDisplayValue() const
	{
		return (const char*)m_pTypeInfo->ToString(m_pData);
	}

	virtual void OnSetValue(bool bRecursive)
	{
		CVariableBase::OnSetValue(bRecursive);
		if (bRecursive)
		{
			for (Vars::iterator it = m_Vars.begin(); it != m_Vars.end(); ++it)
				(*it)->OnSetValue(true);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CopyValue(IVariable* fromVar)
	{
		assert(fromVar);
		if (fromVar->GetType() != IVariable::ARRAY)
			CVariableTypeInfo::CopyValue(fromVar);

		int numSrc = fromVar->GetNumVariables();
		int numTrg = m_Vars.size();
		for (int i = 0; i < numSrc && i < numTrg; i++)
		{
			// Copy Every child variable.
			m_Vars[i]->CopyValue(fromVar->GetVariable(i));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	int        GetNumVariables() const      { return m_Vars.size(); }
	IVariable* GetVariable(int index) const { return m_Vars[index]; }

protected:
	typedef std::vector<IVariablePtr> Vars;
	Vars m_Vars;
};

struct CUIEnumsDBList : IVarEnumList
{
	CUIEnumsDatabase_SEnum const* m_pEnumList;

	CUIEnumsDBList(CUIEnumsDatabase_SEnum const* pEnumList)
		: m_pEnumList(pEnumList)
	{
	}
	virtual const char* GetItemName(uint index)
	{
		if (index >= m_pEnumList->strings.size())
			return NULL;
		return m_pEnumList->strings[index];
	}
};

//////////////////////////////////////////////////////////////////////////
// CVariableTypeInfo dynamic constructor.
//////////////////////////////////////////////////////////////////////////

IVariable* CVariableTypeInfo::Create(CTypeInfo::CVarInfo const& VarInfo, void* pAddress, const void* pAddressDefault)
{
	pAddress = VarInfo.GetAddress(pAddress);
	pAddressDefault = VarInfo.GetAddress(pAddressDefault);

	EType eType = GetType(VarInfo.Type);
	if (eType == ARRAY)
		return new CVariableTypeInfoStruct(VarInfo, pAddress, pAddressDefault);

	if (VarInfo.Type.EnumElem(0))
		return new CVariableTypeInfoEnum(VarInfo, pAddress, pAddressDefault);

	ISplineInterpolator* pSpline = 0;
	if (VarInfo.Type.ToValue(pAddress, pSpline))
		return new CVariableTypeInfoSpline(VarInfo, pAddress, pAddressDefault, pSpline);

	return new CVariableTypeInfo(VarInfo, pAddress, pAddressDefault, eType);
}

#endif // __VariableTypeInfo_h__

