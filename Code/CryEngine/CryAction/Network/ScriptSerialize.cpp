// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Helper classes for implementing serialization in script

   -------------------------------------------------------------------------
   History:
   - 24:11:2004   11:30 : Created by Craig Tiller

*************************************************************************/
#include "StdAfx.h"
#include "ScriptSerialize.h"

//
// CScriptSerializeAny
//

CScriptSerializeAny::CScriptSerializeAny(EScriptSerializeType type)
	: m_type(type)
{
	m_buffer[0] = 0;

	switch (m_type)
	{
	case eSST_StringTable:
	case eSST_String:
		new(Ptr<string>())string();
		break;
	}
}

CScriptSerializeAny::CScriptSerializeAny(const CScriptSerializeAny& other)
{
	m_type = other.m_type;
	switch (m_type)
	{
	case eSST_StringTable:
	case eSST_String:
		new(Ptr<string>())string(*other.Ptr<string>());
		break;

	default:
		memcpy(m_buffer, other.m_buffer, BUFFER_SIZE);
	}
}

CScriptSerializeAny::~CScriptSerializeAny()
{
	switch (m_type)
	{
	case eSST_StringTable:
	case eSST_String:
		Ptr<string>()->~string();
		break;
	}
}

CScriptSerializeAny& CScriptSerializeAny::operator=(const CScriptSerializeAny& other)
{
	// this isn't general enough to be used in every place... but for what
	// we have for now it should be fine
	if (&other == this)
		return *this;
	this->~CScriptSerializeAny();
	new(this)CScriptSerializeAny(other);
	return *this;
}

bool CScriptSerializeAny::SetFromFunction(
  IFunctionHandler* pFH,
  int param)
{
	switch (m_type)
	{
	case eSST_Bool:
		if (!pFH->GetParam(param, *Ptr<bool>()))
			return false;
		break;
	case eSST_Float:
		if (!pFH->GetParam(param, *Ptr<float>()))
			return false;
		break;
	case eSST_Int8:
		{
			int temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<int8>() = temp;
		}
		break;
	case eSST_Int16:
		{
			int temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<int16>() = temp;
		}
		break;
	case eSST_Int32:
		{
			int temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<int32>() = temp;
		}
		break;
	case eSST_StringTable:
	case eSST_String:
		{
			const char* temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<string>() = temp;
		}
		break;
	case eSST_EntityId:
		{
			ScriptHandle temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<EntityId>() = (EntityId)temp.n;
		}
		break;
	case eSST_Vec3:
		{
			Vec3 temp;
			if (!pFH->GetParam(param, temp))
				return false;
			*Ptr<Vec3>() = temp;
		}
	case eSST_Void:
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}
	return true;
}

void CScriptSerializeAny::PushFuncParam(IScriptSystem* pSS) const
{
	switch (m_type)
	{
	case eSST_Bool:
		pSS->PushFuncParam(*Ptr<bool>());
		break;
	case eSST_Float:
		pSS->PushFuncParam(*Ptr<float>());
		break;
	case eSST_Int8:
		pSS->PushFuncParam((int)*Ptr<int8>());
		break;
	case eSST_Int16:
		pSS->PushFuncParam((int)*Ptr<int16>());
		break;
	case eSST_Int32:
		pSS->PushFuncParam((int)*Ptr<int32>());
		break;
	case eSST_StringTable:
	case eSST_String:
		pSS->PushFuncParam(Ptr<string>()->c_str());
		break;
	case eSST_EntityId:
		{
			ScriptHandle temp;
			temp.n = *Ptr<EntityId>();
			pSS->PushFuncParam(temp);
		}
		break;
	case eSST_Vec3:
		pSS->PushFuncParam(*Ptr<Vec3>());
		break;
	case eSST_Void:
		pSS->PushFuncParam(0);
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}
}

void CScriptSerializeAny::SerializeWith(TSerialize ser, const char* name)
{
#define SER_NAME(n) (name ? name : n)

	switch (m_type)
	{
	case eSST_Bool:
		ser.Value(SER_NAME("bool"), *Ptr<bool>());
		break;
	case eSST_Float:
		ser.Value(SER_NAME("float"), *Ptr<float>());
		break;
	case eSST_Int8:
		ser.Value(SER_NAME("int8"), *Ptr<int8>());
		break;
	case eSST_Int16:
		ser.Value(SER_NAME("int16"), *Ptr<int16>());
		break;
	case eSST_Int32:
		ser.Value(SER_NAME("int32"), *Ptr<int32>());
		break;
	case eSST_String:
		ser.Value(SER_NAME("string"), *Ptr<string>());
		break;
	case eSST_StringTable:
		ser.Value(SER_NAME("string"), *Ptr<string>(), 'stab');
		break;
	case eSST_EntityId:
		ser.Value(SER_NAME("entityid"), *Ptr<EntityId>(), 'eid');
		break;
	case eSST_Vec3:
		ser.Value(SER_NAME("vec3"), *Ptr<Vec3>());
		break;
	case eSST_Void:
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}

#undef SER_NAME
}

bool CScriptSerializeAny::CopyFromTable(SmartScriptTable& tbl, const char* name)
{
	bool ok = false;

	switch (m_type)
	{
	case eSST_Bool:
		ok = tbl->GetValue(name, *Ptr<bool>());
		break;
	case eSST_Float:
		ok = tbl->GetValue(name, *Ptr<float>());
		break;
	case eSST_Int8:
		{
			int temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<int8>() = temp;
		}
		break;
	case eSST_Int16:
		{
			int temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<int16>() = temp;
		}
		break;
	case eSST_Int32:
		{
			int temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<int32>() = temp;
		}
		break;
	case eSST_StringTable:
	case eSST_String:
		{
			const char* temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<string>() = temp;
		}
		break;
	case eSST_EntityId:
		{
			ScriptHandle temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<EntityId>() = (EntityId)temp.n;
		}
		break;
	case eSST_Vec3:
		{
			Vec3 temp;
			ok = tbl->GetValue(name, temp);
			*Ptr<Vec3>() = temp;
		}
		break;
	case eSST_Void:
		ok = true;
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}

	if (!ok)
		GameWarning("Unable to read property %s of type %c", name, (char)m_type);

	return ok;
}

void CScriptSerializeAny::CopyToTable(SmartScriptTable& tbl, const char* name)
{
	switch (m_type)
	{
	case eSST_Bool:
		tbl->SetValue(name, *Ptr<bool>());
		break;
	case eSST_Float:
		tbl->SetValue(name, *Ptr<float>());
		break;
	case eSST_Int8:
		tbl->SetValue(name, (int)*Ptr<int8>());
		break;
	case eSST_Int16:
		tbl->SetValue(name, (int)*Ptr<int16>());
		break;
	case eSST_Int32:
		tbl->SetValue(name, (int)*Ptr<int32>());
		break;
	case eSST_StringTable:
	case eSST_String:
		tbl->SetValue(name, Ptr<string>()->c_str());
		break;
	case eSST_EntityId:
		{
			ScriptHandle temp;
			temp.n = *Ptr<EntityId>();
			tbl->SetValue(name, temp);
		}
		break;
	case eSST_Vec3:
		tbl->SetValue(name, *Ptr<Vec3>());
		break;
	case eSST_Void:
		tbl->SetToNull(name);
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}
}

string CScriptSerializeAny::DebugInfo() const
{
	string output;

	switch (m_type)
	{
	case eSST_Bool:
		output = *Ptr<bool>() ? "true" : "false";
		break;
	case eSST_Float:
		output.Format("%f", *Ptr<float>());
		break;
	case eSST_Int8:
		output.Format("%i", (int)*Ptr<int8>());
		break;
	case eSST_Int16:
		output.Format("%i", (int)*Ptr<int16>());
		break;
	case eSST_Int32:
		output.Format("%i", (int)*Ptr<int32>());
		break;
	case eSST_StringTable:
	case eSST_String:
		output = *Ptr<string>();
		break;
	case eSST_EntityId:
		{
			EntityId id = *Ptr<EntityId>();
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
			const char* name = pEntity ? pEntity->GetName() : "<no such entity>";
			output.Format("entity[%u]:%s", id, name);
		}
		break;
	case eSST_Vec3:
		{
			Vec3 temp = *Ptr<Vec3>();
			output.Format("%f,%f,%f", temp.x, temp.y, temp.z);
		}
		break;
	case eSST_Void:
		output = "void";
		break;
	default:
		CRY_ASSERT(!"need to add the type enum to this switch");
	}

	return output;
}

//
// CScriptRMISerialize
//

CScriptRMISerialize::CScriptRMISerialize(const char* format)
{
	while (*format)
	{
		m_values.push_back((EScriptSerializeType) * format);
		format++;
	}
}

void CScriptRMISerialize::SerializeWith(TSerialize ser)
{
	for (TValueVec::iterator i = m_values.begin(); i != m_values.end(); ++i)
	{
		i->SerializeWith(ser);
	}
}

bool CScriptRMISerialize::SetFromFunction(
  IFunctionHandler* pFH,
  int firstParam)
{
	for (size_t i = 0; i < m_values.size(); i++)
	{
		if (!m_values[i].SetFromFunction(pFH, firstParam + i))
			return false;
	}
	return true;
}

void CScriptRMISerialize::PushFunctionParams(IScriptSystem* pSS)
{
	for (TValueVec::iterator i = m_values.begin(); i != m_values.end(); ++i)
	{
		i->PushFuncParam(pSS);
	}
}

string CScriptRMISerialize::DebugInfo()
{
	bool first = true;
	string out;
	for (TValueVec::iterator i = m_values.begin(); i != m_values.end(); ++i)
	{
		if (!first)
			out += ", ";
		out += i->DebugInfo();
		first = false;
	}
	return out;
}

//
// CScriptSerialize
//

bool CScriptSerialize::ReadValue(const char* name, EScriptSerializeType type, TSerialize ser, IScriptTable* pTable)
{
	switch (type)
	{
	case eSST_Bool:
		{
			bool temp;
			ser.Value(name, temp /*, 'bool'*/);
			pTable->SetValue(name, temp);
		}
		break;
	case eSST_Float:
		{
			float temp;
			ser.Value(name, temp);
			pTable->SetValue(name, temp);
		}
		break;
	case eSST_Int8:
		{
			int8 temp;
			ser.Value(name, temp, 'i8');
			pTable->SetValue(name, (int)temp);
		}
		break;
	case eSST_Int16:
		{
			int16 temp;
			ser.Value(name, temp, 'i16');
			pTable->SetValue(name, (int)temp);
		}
		break;
	case eSST_Int32:
		{
			int32 temp;
			ser.Value(name, temp, 'i32');
			pTable->SetValue(name, (int)temp);
		}
		break;
	case eSST_String:
		{
			ser.Value(name, m_tempString);
			pTable->SetValue(name, m_tempString.c_str());
		}
		break;
	case eSST_StringTable:
		{
			ser.Value(name, m_tempString, 'stab');
			pTable->SetValue(name, m_tempString.c_str());
		}
		break;
	case eSST_EntityId:
		{
			EntityId id;
			ScriptHandle hdl;
			ser.Value(name, id, 'eid');
			hdl.n = id;
			pTable->SetValue(name, hdl);
		}
		break;
	case eSST_Vec3:
		{
			Vec3 temp;
			ser.Value(name, temp);
			pTable->SetValue(name, temp);
		}
		break;
	default:
		CRY_ASSERT(!"type not added to ReadValue");
		return false;
	}
	return true;
}

bool CScriptSerialize::WriteValue(const char* name, EScriptSerializeType type, TSerialize ser, IScriptTable* pTable)
{
	bool ok = false;
	switch (type)
	{
	case eSST_Bool:
		{
			bool temp = false;
			if (ok = pTable->GetValue(name, temp))
				ser.Value(name, temp /*, 'bool'*/);
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp);
				ok = true;
			}
		}
		break;
	case eSST_Float:
		{
			float temp = 0;
			if (ok = pTable->GetValue(name, temp))
				ser.Value(name, temp);
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp);
				ok = true;
			}
		}
		break;
	case eSST_Int8:
		{
			int temp1;
			int8 temp2 = 0;
			if (ok = pTable->GetValue(name, temp1))
			{
				if (temp1 < -128 || temp1 > 127)
					ok = false;
				else
				{
					temp2 = temp1;
					ser.Value(name, temp2, 'i8');
				}
			}
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp2, 'i8');
				ok = true;
			}
		}
		break;
	case eSST_Int16:
		{
			int temp1;
			int16 temp2 = 0;
			if (ok = pTable->GetValue(name, temp1))
			{
				if (temp1 < -32768 || temp1 > 32767)
					ok = false;
				else
				{
					temp2 = temp1;
					ser.Value(name, temp2, 'i16');
				}
			}
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp2, 'i16');
				ok = true;
			}
		}
		break;
	case eSST_Int32:
		{
			int32 temp1;
			int temp2 = 0;
			if (ok = pTable->GetValue(name, temp1))
			{
				temp2 = temp1;
				ser.Value(name, temp2, 'i32');
			}
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp2, 'i32');
				ok = true;
			}
		}
		break;
	case eSST_StringTable:
	case eSST_String:
		{
			const char* temp;
			if (ok = pTable->GetValue(name, temp))
			{
				m_tempString = temp;
				ser.Value(name, m_tempString);
			}
			else if (!pTable->HaveValue(name))
			{
				m_tempString = string();
				ser.Value(name, m_tempString);
				ok = true;
			}
		}
		break;
	case eSST_EntityId:
		{
			EntityId id = 0;
			ScriptHandle hdl;
			if (ok = pTable->GetValue(name, hdl))
			{
				id = (EntityId)hdl.n;
				ser.Value(name, id, 'eid');
			}
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, id, 'eid');
				ok = true;
			}
		}
		break;
	case eSST_Vec3:
		{
			Vec3 temp = ZERO;
			if (ok = pTable->GetValue(name, temp))
			{
				ser.Value(name, temp);
			}
			else if (!pTable->HaveValue(name))
			{
				ser.Value(name, temp);
				ok = true;
			}
		}
	default:
		CRY_ASSERT(!"type not added to WriteValue");
		ok = false;
	}
	return ok;
}
