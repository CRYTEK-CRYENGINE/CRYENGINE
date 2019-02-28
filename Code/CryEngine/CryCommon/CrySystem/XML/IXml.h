// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ixml.h
//  Version:     v1.00
//  Created:     16/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryCore/Platform/platform.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>

class ICrySizer;
struct CryGUID;

#if defined(_AFX) && !defined(RESOURCE_COMPILER)
#include <CryCore/ToolsHelpers/GuidUtil.h>
#endif

class IXMLBinarySerializer;
struct IReadWriteXMLSink;
struct ISerialize;

/*
   This is wrapper around expat library to provide DOM type of access for xml.
   Do not use IXmlNode class directly instead always use XmlNodeRef wrapper that
   takes care of memory management issues.

   Usage Example:
   -------------------------------------------------------
   void testXml(bool bReuseStrings)
   {
   XmlParser xml(bReuseStrings);
   XmlNodeRef root = xml.ParseFile("test.xml", true);

   if (root)
   {
    for (int i = 0; i < root->getChildCount(); ++i)
    {
      XmlNodeRef child = root->getChild(i);
      if (child->isTag("world"))
      {
        if (child->getAttr("name") == "blah")
        {
          ....
        }
      }
    }
   }
   }
 */

//! Special string wrapper for xml nodes.
class XmlString : public string
{
public:
	XmlString() {};
	XmlString(const char* str) : string(str) {};
#ifdef  _AFX
	XmlString(const CString& str) : string((const char*)str) {};
#endif

	operator const char*() const { return c_str(); }

};

//! \cond INTERNAL
//! XML string data.
struct IXmlStringData
{
	// <interfuscator:shuffle>
	virtual ~IXmlStringData(){}
	virtual void        AddRef() = 0;
	virtual void        Release() = 0;
	virtual const char* GetString() = 0;
	virtual size_t      GetStringLength() = 0;
	// </interfuscator:shuffle>
};
//! \endcond

class IXmlNode;

//! XmlNodeRef, wrapper class implementing reference counting for IXmlNode.
class XmlNodeRef
{
private:
	IXmlNode* p;
public:
	XmlNodeRef() : p(NULL) {}
	XmlNodeRef(IXmlNode* p_);
	XmlNodeRef(const XmlNodeRef& p_);
	XmlNodeRef(XmlNodeRef&& other);

	~XmlNodeRef();
	
	bool     isValid() const   { return p != nullptr; }
	operator IXmlNode*() const { return p; }

	IXmlNode&   operator*() const      { return *p; }
	IXmlNode*   operator->(void) const { return p; }

	XmlNodeRef& operator=(IXmlNode* newp);
	XmlNodeRef& operator=(const XmlNodeRef& newp);

	XmlNodeRef& operator=(XmlNodeRef&& other);

#if !defined(RESOURCE_COMPILER)
	template<typename Sizer>
	void GetMemoryUsage(Sizer* pSizer) const
	{
		pSizer->AddObject(p);
	}
#endif
};

//! Never use IXmlNode directly - instead use reference counted XmlNodeRef.
class IXmlNode
{
protected:
	int m_nRefCount;

protected:
	// <interfuscator:shuffle>
	virtual void DeleteThis() = 0;
	virtual ~IXmlNode() {};
	// </interfuscator:shuffle>

public:

	// <interfuscator:shuffle>
	//! Creates new XML node.
	virtual XmlNodeRef createNode(const char* tag) = 0;

	// AddRef/Release need to be virtual to permit overloading from CXMLNodePool.

	//! Reference counting.
	virtual void AddRef() { m_nRefCount++; };

	//! When ref count reaches zero, the XML node dies.
	virtual void Release()           { if (--m_nRefCount <= 0) DeleteThis(); };
	virtual int  GetRefCount() const { return m_nRefCount; };

	//! Get XML node tag.
	virtual const char* getTag() const = 0;

	//! Sets XML node tag.
	virtual void setTag(const char* tag) = 0;

	//! Return true if a given tag equal to node tag.
	virtual bool isTag(const char* tag) const = 0;

	//! Get XML Node attributes.
	virtual int getNumAttributes() const = 0;

	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex(int index, const char** key, const char** value) = 0;

	//! Copy attributes to this node from a given node.
	virtual void copyAttributes(XmlNodeRef fromNode) = 0;

	//! Get XML Node attribute for specified key.
	virtual const char* getAttr(const char* key) const = 0;

	//! Gets XML Node attribute for specified key.
	//! \return true if the attribute exists, false otherwise.
	virtual bool getAttr(const char* key, const char** value) const = 0;

	//! Checks if attributes with specified key exist.
	virtual bool haveAttr(const char* key) const = 0;

	//! Add new child node.
	virtual void addChild(const XmlNodeRef& node) = 0;

	//! Create new xml node and add it to childs list.
	virtual XmlNodeRef newChild(const char* tagName) = 0;

	//! Remove child node.
	virtual void removeChild(const XmlNodeRef& node) = 0;

	//! Insert child node.
	virtual void insertChild(int nIndex, const XmlNodeRef& node) = 0;

	//! Replaces a specified child with the passed one.
	//! Not supported by all node implementations.
	virtual void replaceChild(int nIndex, const XmlNodeRef& fromNode) = 0;

	//! Remove all child nodes.
	virtual void removeAllChilds() = 0;

	//! Get number of child XML nodes.
	//! \par Example
	//! \include CrySystem/Examples/XmlParsing.cpp
	virtual int getChildCount() const = 0;

	//! Get XML Node child nodes.
	//! \par Example
	//! \include CrySystem/Examples/XmlParsing.cpp
	virtual XmlNodeRef getChild(int i) const = 0;

	//! Find node with specified tag.
	virtual XmlNodeRef findChild(const char* tag) const = 0;

	//! Get parent XML node.
	virtual XmlNodeRef getParent() const = 0;

	//! Set parent XML node.
	virtual void setParent(const XmlNodeRef& inRef) = 0;

	//! Return content of this node.
	virtual const char* getContent() const = 0;

	//! Set content of this node.
	virtual void setContent(const char* str) = 0;

	//! Deep clone of this and all child xml nodes.
	virtual XmlNodeRef clone() = 0;

	//! Return line number for XML tag.
	virtual int getLine() const = 0;

	//! Set line number in xml.
	virtual void setLine(int line) = 0;

	//! Return XML of this node and sub nodes.
	//! \note IXmlStringData pointer must be release when string is not needed anymore.
	//! \see IXmlStringData
	virtual IXmlStringData* getXMLData(int nReserveMem = 0) const = 0;

	//! Returns XML of this node and sub nodes.
	virtual XmlString getXML(int level = 0) const = 0;
	//! Saves the XML node to disk
	//! \par Example
	//! \include CrySystem/Examples/XmlWriting.cpp
	virtual bool      saveToFile(const char* fileName) = 0;

	//! Set new XML Node attribute (or override attribute with same key).
	//! @{
	virtual void setAttr(const char* key, const char* value) = 0;
	virtual void setAttr(const char* key, int value) = 0;
	virtual void setAttr(const char* key, unsigned int value) = 0;
	virtual void setAttr(const char* key, int64 value) = 0;
	virtual void setAttr(const char* key, uint64 value, bool useHexFormat = true) = 0;
	virtual void setAttr(const char* key, float value) = 0;
	virtual void setAttr(const char* key, double value) = 0;
	virtual void setAttr(const char* key, const Vec2& value) = 0;
	virtual void setAttr(const char* key, const Vec2d& value) = 0;
	virtual void setAttr(const char* key, const Ang3& value) = 0;
	virtual void setAttr(const char* key, const Vec3& value) = 0;
	virtual void setAttr(const char* key, const Vec4& value) = 0;
	virtual void setAttr(const char* key, const Vec3d& value) = 0;
	virtual void setAttr(const char* key, const Quat& value) = 0;
#if (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_APPLE
	//! Compatability functions, on Linux and Mac long int is the default int64_t.
	ILINE void setAttr(const char* key, unsigned long int value, bool useHexFormat = true)
	{
		setAttr(key, (uint64)value, useHexFormat);
	}

	ILINE void setAttr(const char* key, long int value)
	{
		setAttr(key, (int64)value);
	}
#endif
	//! @}.

	//! Delete attribute.
	virtual void delAttr(const char* key) = 0;

	//! Remove all node attributes.
	virtual void removeAllAttributes() = 0;

	//! Get attribute value of node.
	//! @{
	virtual bool getAttr(const char* key, int& value) const = 0;
	virtual bool getAttr(const char* key, unsigned int& value) const = 0;
	virtual bool getAttr(const char* key, int64& value) const = 0;
	virtual bool getAttr(const char* key, uint64& value, bool useHexFormat = true) const = 0;
	virtual bool getAttr(const char* key, float& value) const = 0;
	virtual bool getAttr(const char* key, double& value) const = 0;
	virtual bool getAttr(const char* key, Vec2& value) const = 0;
	virtual bool getAttr(const char* key, Vec2d& value) const = 0;
	virtual bool getAttr(const char* key, Ang3& value) const = 0;
	virtual bool getAttr(const char* key, Vec3& value) const = 0;
	virtual bool getAttr(const char* key, Vec4& value) const = 0;
	virtual bool getAttr(const char* key, Vec3d& value) const = 0;
	virtual bool getAttr(const char* key, Quat& value) const = 0;
	virtual bool getAttr(const char* key, bool& value) const = 0;
	virtual bool getAttr(const char* key, XmlString& value) const = 0;
	virtual bool getAttr(const char* key, ColorB& value) const = 0;

#if (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_APPLE
	//! Compatability functions, on Linux and Mac long int is the default int64_t.
	ILINE bool getAttr(const char* key, unsigned long int& value, bool useHexFormat = true) const
	{
		return getAttr(key, (uint64&)value, useHexFormat);
	}

	ILINE bool getAttr(const char* key, long int& value) const
	{
		return getAttr(key, (int64&)value);
	}
#endif

	// </interfuscator:shuffle>

#if !defined(RESOURCE_COMPILER)
	// <interfuscator:shuffle>
	//! Collect all allocated memory
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Copy children to this node from a given node.
	//! Children are reference copied (shallow copy) and the children's parent is NOT set to this
	//! node, but left with its original parent (which is still the parent).
	virtual void shareChildren(const XmlNodeRef& fromNode) = 0;

	//! Remove child node at known position.
	virtual void deleteChildAt(int nIndex) = 0;

	//! Return XML of this node and sub nodes into tmpBuffer without XML checks (much faster).
	virtual XmlString getXMLUnsafe(int level, char* tmpBuffer, uint32 sizeOfTmpBuffer) const { return getXML(level); }

	//! Save in small memory chunks.
	virtual bool saveToFile(const char* fileName, size_t chunkSizeBytes, FILE* file = NULL) = 0;
	// </interfuscator:shuffle>
#endif
	//! @}.

	//! Inline Helpers.
	//! @{
#if !(CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) && !CRY_PLATFORM_APPLE
	bool getAttr(const char* key, long& value) const           { int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	bool getAttr(const char* key, unsigned long& value) const  { unsigned int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	void setAttr(const char* key, unsigned long value)         { setAttr(key, (unsigned int)value); };
	void setAttr(const char* key, long value)                  { setAttr(key, (int)value); };
#endif
	bool getAttr(const char* key, unsigned short& value) const { unsigned int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	bool getAttr(const char* key, unsigned char& value) const  { unsigned int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	bool getAttr(const char* key, short& value) const          { int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	bool getAttr(const char* key, char& value) const           { int v; if (getAttr(key, v)) { value = v; return true; } else return false; }
	//! @}.

#ifndef RESOURCE_COMPILER
#ifdef _AFX
	//! Gets CString attribute.
	bool getAttr(const char* key, CString& value) const
	{
		if (!haveAttr(key))
			return false;
		value = getAttr(key);
		return true;
	}

	//! Gets string attribute.
	bool getAttr(const char* key, string& value) const
	{
		if (!haveAttr(key))
			return false;
		value = getAttr(key);
		return true;
	}

	//! Sets GUID attribute.
	void setAttr(const char* key, REFGUID value)
	{
		const char* str = GuidUtil::ToString(value);
		setAttr(key, str);
	};

	//! Gets GUID from attribute.
	bool getAttr(const char* key, GUID& value) const
	{
		if (!haveAttr(key))
			return false;
		const char* guidStr = getAttr(key);
		value = GuidUtil::FromString(guidStr);
		if (value.Data1 == 0)
		{
			memset(&value, 0, sizeof(value));
			// If bad GUID, use old guid system.
			value.Data1 = atoi(guidStr);
		}
		return true;
	}
#endif //_AFX
	// Those are defined as abstract here because CryGUID header has access to gEnv, which breaks compilation
	virtual void setAttr(const char* key, const CryGUID& value) = 0;

	virtual bool getAttr(const char* key, CryGUID& value) const = 0;
#endif //RESOURCE_COMPILER

	// Lets be friendly to him.
	friend class XmlNodeRef;
};

/*
   //! Inline Implementation of XmlNodeRef
   inline XmlNodeRef::XmlNodeRef(const char *tag, IXmlNode *node)
   {
   if (node)
   {
    p = node->createNode(tag);
   }
   else
   {
    p = new XmlNode(tag);
   }
   p->AddRef();
   }
 */

//////////////////////////////////////////////////////////////////////////
inline XmlNodeRef::XmlNodeRef(IXmlNode* p_) : p(p_)
{
	if (p) p->AddRef();
}

inline XmlNodeRef::XmlNodeRef(const XmlNodeRef& p_) : p(p_.p)
{
	if (p) p->AddRef();
}

// Move constructor
inline XmlNodeRef::XmlNodeRef(XmlNodeRef&& other)
{
	if (this != &other)
	{
		p = other.p;
		other.p = nullptr;
	}
}

inline XmlNodeRef::~XmlNodeRef()
{
	if (p) p->Release();
}

inline XmlNodeRef& XmlNodeRef::operator=(XmlNodeRef&& other)
{
	if (this != &other)
	{
		if (p) p->Release();
		p = other.p;
		other.p = nullptr;
	}
	return *this;
}

inline XmlNodeRef& XmlNodeRef::operator=(IXmlNode* newp)
{
	if (newp) newp->AddRef();
	if (p) p->Release();
	p = newp;
	return *this;
}

inline XmlNodeRef& XmlNodeRef::operator=(const XmlNodeRef& newp)
{
	if (newp.p) newp.p->AddRef();
	if (p) p->Release();
	p = newp.p;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
struct IXmlSerializer
{
	// <interfuscator:shuffle>
	virtual ~IXmlSerializer(){}
	virtual void        AddRef() = 0;
	virtual void        Release() = 0;

	virtual ISerialize* GetWriter(XmlNodeRef& node) = 0;
	virtual ISerialize* GetReader(XmlNodeRef& node) = 0;
	// </interfuscator:shuffle>
#if !defined(RESOURCE_COMPILER)
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
#endif
};

#if !defined(RESOURCE_COMPILER)
//////////////////////////////////////////////////////////////////////////
//! XML Parser interface.
//! \cond INTERNAL
struct IXmlParser
{
	// <interfuscator:shuffle>
	virtual ~IXmlParser(){}
	virtual void AddRef() = 0;
	virtual void Release() = 0;

	//! Parse xml file.
	virtual XmlNodeRef ParseFile(const char* filename, bool bCleanPools) = 0;

	//! Parse xml from memory buffer.
	virtual XmlNodeRef ParseBuffer(const char* buffer, int nBufLen, bool bCleanPools) = 0;

	virtual void       GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////
//! XML Table Reader interface.
//! Can be used to read tables exported from Excel in .xml format.
//! Supports reading CRYENGINE's version of those Excel .xml tables (produced by RC).
//! Usage:
//! \code
//! p->Begin(rootNode);
//! while (p->ReadRow(...))
//! {
//!     while (p->ReadCell(...))
//!     {
//!         ...
//!     }
//! }
//! \endcode
struct IXmlTableReader
{
	// <interfuscator:shuffle>
	virtual ~IXmlTableReader(){}

	virtual void Release() = 0;

	//! Return false if XML tree is not in supported table format.
	virtual bool Begin(XmlNodeRef rootNode) = 0;

	//! Return estimated number of rows (estimated number of ReadRow() calls returning true).
	//! Returned number is equal *or greater* than real number, because it's impossible to
	//! know real number in advance in case of Excel XML.
	virtual int GetEstimatedRowCount() = 0;

	//! Prepare next row for reading by ReadCell().
	//! \note Empty rows are skipped sometimes, so use returned rowIndex if you need to know absolute row index.
	//! \return true and sets rowIndex if the row was prepared successfully, false if no rows left.
	virtual bool ReadRow(int& rowIndex) = 0;

	//! Read next cell in the current row.
	//! \note Empty cells are skipped sometimes, so use returned cellIndex if you need to know absolute cell index (i.e. column).
	//! \return true and sets columnIndex, pContent, contenSize if the cell was read successfully, false if no cells left in the row.
	virtual bool ReadCell(int& columnIndex, const char*& pContent, size_t& contentSize) = 0;
	// </interfuscator:shuffle>
};
//! \endcond
#endif

//////////////////////////////////////////////////////////////////////////
//! IXmlUtils structure.
struct IXmlUtils
{
	// <interfuscator:shuffle>
	virtual ~IXmlUtils(){}

	//! Load xml file, returns 0 if load failed.
	virtual XmlNodeRef LoadXmlFromFile(const char* sFilename, bool bReuseStrings = false, bool bEnablePatching = true) = 0;

	//! Load xml from memory buffer, returns 0 if load failed.
	virtual XmlNodeRef LoadXmlFromBuffer(const char* buffer, size_t size, bool bReuseStrings = false) = 0;

	//! Create an MD5 hash of an XML file.
	virtual const char* HashXml(XmlNodeRef node) = 0;

	//! Get an object that can read a xml into a IReadXMLSink and writes a xml from a IWriteXMLSource
	virtual IReadWriteXMLSink* GetIReadWriteXMLSink() = 0;

	//! Create XML Writer for ISerialize interface.
	virtual IXmlSerializer* CreateXmlSerializer() = 0;
	// </interfuscator:shuffle>

#if !defined(RESOURCE_COMPILER)
	// <interfuscator:shuffle>
	//! Create XML Parser.
	//! \note IXmlParser does not normally support recursive XML loading, all nodes loaded by this parser are invalidated on loading new file.
	//! This is a specialized interface for fast loading of many XMLs,
	//! After use it must be released with call to Release method.
	virtual IXmlParser* CreateXmlParser() = 0;

	//! Create XML to file in the binary form.
	virtual bool SaveBinaryXmlFile(const char* sFilename, XmlNodeRef root) = 0;

	//! Read XML data from file in the binary form.
	virtual XmlNodeRef LoadBinaryXmlFile(const char* sFilename, bool bEnablePatching = true) = 0;

	//! Enable or disables checking for binary xml files.
	//! \return Previous status.
	virtual bool EnableBinaryXmlLoading(bool bEnable) = 0;

	//! After use it must be released with call to Release method.
	virtual IXmlTableReader* CreateXmlTableReader() = 0;

	//! Init xml stats nodes pool.
	virtual void InitStatsXmlNodePool(uint32 nPoolSize) = 0;

	//! Creates new xml node for statistics.
	virtual XmlNodeRef CreateStatsXmlNode(const char* sNodeName) = 0;

	//! Set owner thread.
	virtual void SetStatsOwnerThread(threadID threadId) = 0;

	//! Free memory held on to by xml pool if empty.
	virtual void FlushStatsXmlNodePool() = 0;

	//! Sets the patch which is used to transform loaded XML files.
	//! The patch itself is encoded into XML.
	//! Set to NULL to clear an existing transform and disable further patching.
	virtual void SetXMLPatcher(XmlNodeRef* pPatcher) = 0;
	// </interfuscator:shuffle>
#endif
};
