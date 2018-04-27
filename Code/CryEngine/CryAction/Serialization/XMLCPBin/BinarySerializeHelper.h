// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Manages one-off reader/writer usages for binary serialization

   -------------------------------------------------------------------------
   History:
   - 02:06:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __BINARYREADERWRITER_H__
#define __BINARYREADERWRITER_H__

#include <CryNetwork/ISerializeHelper.h>

#include "Serialization/SerializeWriterXMLCPBin.h"
#include "Serialization/SerializeReaderXMLCPBin.h"
#include "Serialization/XMLCPBin/Writer/XMLCPB_WriterInterface.h"
#include "Serialization/XMLCPBin/Reader/XMLCPB_ReaderInterface.h"

class CBinarySerializedObject : public ISerializedObject
{
public:
	CBinarySerializedObject(const char* szSection);
	virtual ~CBinarySerializedObject();

	enum { GUID = 0xBDE84A9A };
	virtual uint32 GetGUID() const { return GUID; }
	virtual void   GetMemoryUsage(ICrySizer* pSizer) const;

	virtual void   AddRef()  { ++m_nRefCount; }
	virtual void   Release() { if (--m_nRefCount <= 0) delete this; }

	virtual bool   IsEmpty() const;
	virtual void   Reset();
	virtual void   Serialize(TSerialize& serialize);

	bool           FinishWriting(XMLCPB::CWriterInterface& Writer);
	bool           PrepareReading(XMLCPB::CReaderInterface& Reader);
	const char*    GetSectionName() const { return m_sSection.c_str(); }

private:
	void FreeData();

	string m_sSection;

	int    m_nRefCount;
	uint32 m_uSerializedDataSize;
	uint8* m_pSerializedData;
};

class CBinarySerializeHelper : public ISerializeHelper
{
public:
	CBinarySerializeHelper();
	virtual ~CBinarySerializeHelper();

	virtual void                          GetMemoryUsage(ICrySizer* pSizer) const;

	virtual void                          AddRef()  { ++m_nRefCount; }
	virtual void                          Release() { if (--m_nRefCount <= 0) delete this; }

	virtual _smart_ptr<ISerializedObject> CreateSerializedObject(const char* szSection);
	virtual bool                          Write(ISerializedObject* pObject, TSerializeFunc serializeFunc, void* pArgument = NULL);
	virtual bool                          Read(ISerializedObject* pObject, TSerializeFunc serializeFunc, void* pArgument = NULL);

private:
	static CBinarySerializedObject* GetBinarySerializedObject(ISerializedObject* pObject);

private:
	int m_nRefCount;
};

#endif //__BINARYREADERWRITER_H__
