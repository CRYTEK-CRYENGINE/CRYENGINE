// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Manages one-off reader/writer usages for Xml serialization

   -------------------------------------------------------------------------
   History:
   - 02:06:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __XMLREADERWRITER_H__
#define __XMLREADERWRITER_H__

#include <CryNetwork/ISerializeHelper.h>

class CXmlSerializedObject : public ISerializedObject
{
public:
	CXmlSerializedObject(const char* szSection);
	virtual ~CXmlSerializedObject() {}

	enum { GUID = 0xD6BEE847 };
	virtual uint32    GetGUID() const { return GUID; }
	virtual void      GetMemoryUsage(ICrySizer* pSizer) const;

	virtual void      AddRef()  { ++m_nRefCount; }
	virtual void      Release() { if (--m_nRefCount <= 0) delete this; }

	virtual bool      IsEmpty() const;
	virtual void      Reset();
	virtual void      Serialize(TSerialize& serialize);

	void              CreateRootNode();

	XmlNodeRef&       GetXmlNode()       { return m_XmlNode; }
	const XmlNodeRef& GetXmlNode() const { return m_XmlNode; }

private:
	string     m_sTag;
	XmlNodeRef m_XmlNode;
	int        m_nRefCount;
};

class CXmlSerializeHelper : public ISerializeHelper
{
public:
	CXmlSerializeHelper();
	virtual ~CXmlSerializeHelper();

	virtual void                          GetMemoryUsage(ICrySizer* pSizer) const;

	virtual void                          AddRef()  { ++m_nRefCount; }
	virtual void                          Release() { if (--m_nRefCount <= 0) delete this; }

	virtual _smart_ptr<ISerializedObject> CreateSerializedObject(const char* szSection);
	virtual bool                          Write(ISerializedObject* pObject, TSerializeFunc serializeFunc, void* pArgument = NULL);
	virtual bool                          Read(ISerializedObject* pObject, TSerializeFunc serializeFunc, void* pArgument = NULL);

	// Local versions which work with XmlNodes directly
	ISerialize* GetWriter(XmlNodeRef& node);
	ISerialize* GetReader(XmlNodeRef& node);

private:
	static CXmlSerializedObject* GetXmlSerializedObject(ISerializedObject* pObject);

private:
	int                        m_nRefCount;
	_smart_ptr<IXmlSerializer> m_pSerializer;
};

#endif //__XMLREADERWRITER_H__
