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
#ifndef __SCRIPTSERIALIZE_H__
#define __SCRIPTSERIALIZE_H__

#pragma once

#include <vector>

enum EScriptSerializeType
{
	eSST_Bool        = 'B',
	eSST_Float       = 'f',
	eSST_Int8        = 'b',
	eSST_Int16       = 's',
	eSST_Int32       = 'i',
	eSST_String      = 'S',
	eSST_EntityId    = 'E',
	eSST_Void        = '0',
	eSST_Vec3        = 'V',
	eSST_StringTable = 'T'
};

class CScriptSerialize
{
public:
	bool ReadValue(const char* name, EScriptSerializeType type, TSerialize, IScriptTable*);
	bool WriteValue(const char* name, EScriptSerializeType type, TSerialize, IScriptTable*);

private:
	string m_tempString;
};

class CScriptSerializeAny
{
public:
	CScriptSerializeAny()
		: m_type(eSST_Void)
	{
		m_buffer[0] = 0;
	}
	CScriptSerializeAny(EScriptSerializeType type);
	CScriptSerializeAny(const CScriptSerializeAny&);
	~CScriptSerializeAny();
	CScriptSerializeAny& operator=(const CScriptSerializeAny&);

	void SerializeWith(TSerialize, const char* name = 0);
	bool   SetFromFunction(IFunctionHandler* pFH, int param);
	void   PushFuncParam(IScriptSystem* pSS) const;
	bool   CopyFromTable(SmartScriptTable& tbl, const char* name);
	void   CopyToTable(SmartScriptTable& tbl, const char* name);
	string DebugInfo() const;

private:
	static const size_t  BUFFER_SIZE = sizeof(string) > sizeof(Vec3) ? sizeof(string) : sizeof(Vec3);
	char                 m_buffer[BUFFER_SIZE];
	EScriptSerializeType m_type;

	template<class T>
	T* Ptr()
	{
		CRY_ASSERT(BUFFER_SIZE >= sizeof(T));
		return reinterpret_cast<T*>(m_buffer);
	}
	template<class T>
	const T* Ptr() const
	{
		CRY_ASSERT(BUFFER_SIZE >= sizeof(T));
		return reinterpret_cast<const T*>(m_buffer);
	}
};

// this class helps provide a bridge between script and ISerialize
class CScriptRMISerialize : public ISerializable
{
public:
	CScriptRMISerialize(const char* format);

	void SerializeWith(TSerialize);
	bool   SetFromFunction(IFunctionHandler* pFH, int firstParam);
	void   PushFunctionParams(IScriptSystem* pSS);
	string DebugInfo();

private:
	typedef std::vector<CScriptSerializeAny> TValueVec;
	TValueVec m_values;
};

#endif
