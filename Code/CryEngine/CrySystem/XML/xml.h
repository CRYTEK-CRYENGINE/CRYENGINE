// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   xml.h
//  Created:     21/04/2006 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __XML_HEADER__
#define __XML_HEADER__

#include <algorithm>
#include <CryMemory/PoolAllocator.h>

#include <CrySystem/XML/IXml.h>

#include <CryCore/Platform/IPlatformOS.h>

// track some XML stats. only to find persistent XML nodes in the system
// slow, so disable by default
//#define CRY_COLLECT_XML_NODE_STATS
//#undef CRY_COLLECT_XML_NODE_STATS

struct IXmlStringPool
{
public:
	IXmlStringPool() { m_refCount = 0; }
	virtual ~IXmlStringPool() {};
	void AddRef() { m_refCount++; };
	void Release()
	{
		if (--m_refCount <= 0)
			delete this;
	};
	virtual const char* AddString(const char* str) = 0;
	virtual void        GetMemoryUsage(ICrySizer* pSizer) const = 0;
private:
	int                 m_refCount;
};

/************************************************************************/
/* XmlParser class, Parse xml and return root xml node if success.      */
/************************************************************************/
class XmlParser : public IXmlParser
{
public:
	explicit XmlParser(bool bReuseStrings);
	~XmlParser();

	void AddRef()
	{
		++m_nRefCount;
	}

	void Release()
	{
		if (--m_nRefCount <= 0)
			delete this;
	}

	virtual XmlNodeRef ParseFile(const char* filename, bool bCleanPools);

	virtual XmlNodeRef ParseBuffer(const char* buffer, int nBufLen, bool bCleanPools);

	const char*        getErrorString() const { return m_errorString; }

	void               GetMemoryUsage(ICrySizer* pSizer) const;

private:
	int                 m_nRefCount;
	XmlString           m_errorString;
	class XmlParserImp* m_pImpl;
};

// Compare function for string comparasion, can be strcmp or stricmp
typedef int (__cdecl * XmlStrCmpFunc)(const char* str1, const char* str2);
extern XmlStrCmpFunc g_pXmlStrCmp;

//////////////////////////////////////////////////////////////////////////
// XmlAttribute class
//////////////////////////////////////////////////////////////////////////
struct XmlAttribute
{
	const char* key;
	const char* value;

	XmlAttribute()
		: key(nullptr)
		, value(nullptr)
	{}

	void GetMemoryUsage(ICrySizer* pSizer) const    {}

	bool operator<(const XmlAttribute& attr) const  { return g_pXmlStrCmp(key, attr.key) < 0; }
	bool operator>(const XmlAttribute& attr) const  { return g_pXmlStrCmp(key, attr.key) > 0; }
	bool operator==(const XmlAttribute& attr) const { return g_pXmlStrCmp(key, attr.key) == 0; }
	bool operator!=(const XmlAttribute& attr) const { return g_pXmlStrCmp(key, attr.key) != 0; }
};

//! Xml node attributes class.

typedef std::vector<XmlAttribute>     XmlAttributes;
typedef XmlAttributes::iterator       XmlAttrIter;
typedef XmlAttributes::const_iterator XmlAttrConstIter;

/**
 ******************************************************************************
 * CXmlNode class
 * Never use CXmlNode directly instead use reference counted XmlNodeRef.
 ******************************************************************************
 */

class CXmlNode : public IXmlNode
{
public:
	//! Constructor.
	CXmlNode();
	CXmlNode(const char* tag, bool bReuseStrings);
	//! Destructor.
	~CXmlNode();

	// collect allocated memory  informations
	void GetMemoryUsage(ICrySizer* pSizer) const;

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete with pool allocator.
	//////////////////////////////////////////////////////////////////////////
	//void* operator new( size_t nSize );
	//void operator delete( void *ptr );

	virtual void DeleteThis();

	//! Create new XML node.
	XmlNodeRef createNode(const char* tag);

	//! Get XML node tag.
	const char* getTag() const { return m_tag; };
	void        setTag(const char* tag);

	//! Return true if given tag equal to node tag.
	bool isTag(const char* tag) const;

	//! Get XML Node attributes.
	virtual int  getNumAttributes() const { return m_pAttributes ? (int)m_pAttributes->size() : 0; };
	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex(int index, const char** key, const char** value);

	//! Return attribute key and value by attribute index, string version.
	virtual bool getAttributeByIndex(int index, XmlString& key, XmlString& value);

	virtual void copyAttributes(XmlNodeRef fromNode);
	virtual void shareChildren(const XmlNodeRef& fromNode);

	//! Get XML Node attribute for specified key.
	const char* getAttr(const char* key) const;

	//! Get XML Node attribute for specified key.
	// Returns true if the attribute existes, alse otherwise.
	bool getAttr(const char* key, const char** value) const;

	//! Check if attributes with specified key exist.
	bool haveAttr(const char* key) const;

	//! Creates new xml node and add it to childs list.
	XmlNodeRef newChild(const char* tagName);

	//! Adds new child node.
	void addChild(const XmlNodeRef& node);
	//! Remove child node.
	void removeChild(const XmlNodeRef& node);

	void insertChild(int nIndex, const XmlNodeRef& node);
	void replaceChild(int nIndex, const XmlNodeRef& node);

	//! Remove all child nodes.
	void removeAllChilds();

	//! Get number of child XML nodes.
	int getChildCount() const { return m_pChilds ? (int)m_pChilds->size() : 0; };

	//! Get XML Node child nodes.
	XmlNodeRef getChild(int i) const;

	//! Find node with specified tag.
	XmlNodeRef findChild(const char* tag) const;
	void       deleteChild(const char* tag);
	void       deleteChildAt(int nIndex);

	//! Get parent XML node.
	XmlNodeRef getParent() const { return m_parent; }
	void       setParent(const XmlNodeRef& inRef);

	//! Returns content of this node.
	const char* getContent() const { return m_content; };
	void        setContent(const char* str);

	XmlNodeRef  clone();

	//! Returns line number for XML tag.
	int  getLine() const   { return m_line; };
	//! Set line number in xml.
	void setLine(int line) { m_line = line; };

	//! Returns XML of this node and sub nodes.
	virtual IXmlStringData* getXMLData(int nReserveMem = 0) const;
	XmlString               getXML(int level = 0) const;
	XmlString               getXMLUnsafe(int level, char* tmpBuffer, uint32 sizeOfTmpBuffer) const;
	bool                    saveToFile(const char* fileName);                                           // saves in one huge chunk
	bool                    saveToFile(const char* fileName, size_t chunkSizeBytes, FILE* file = NULL); // save in small memory chunks

	//! Set new XML Node attribute (or override attribute with same key).
	void setAttr(const char* key, const char* value);
	void setAttr(const char* key, int value);
	void setAttr(const char* key, unsigned int value);
	void setAttr(const char* key, int64 value);
	void setAttr(const char* key, uint64 value, bool useHexFormat = true);
	void setAttr(const char* key, float value);
	void setAttr(const char* key, double value);
	void setAttr(const char* key, const Vec2& value);
	void setAttr(const char* key, const Vec2d& value);
	void setAttr(const char* key, const Ang3& value);
	void setAttr(const char* key, const Vec3& value);
	void setAttr(const char* key, const Vec4& value);
	void setAttr(const char* key, const Vec3d& value);
	void setAttr(const char* key, const Quat& value);
	void setAttr(const char* key, const CryGUID& value);

	//! Delete attrbute.
	void delAttr(const char* key);
	//! Remove all node attributes.
	void removeAllAttributes();

	//! Get attribute value of node.
	bool getAttr(const char* key, int& value) const;
	bool getAttr(const char* key, unsigned int& value) const;
	bool getAttr(const char* key, int64& value) const;
	bool getAttr(const char* key, uint64& value, bool useHexFormat = true /*ignored*/) const;
	bool getAttr(const char* key, float& value) const;
	bool getAttr(const char* key, double& value) const;
	bool getAttr(const char* key, bool& value) const;

	bool getAttr(const char* key, XmlString& value) const { const char* v(NULL); bool boHasAttribute(getAttr(key, &v)); value = v; return boHasAttribute; }

	bool getAttr(const char* key, Vec2& value) const;
	bool getAttr(const char* key, Vec2d& value) const;
	bool getAttr(const char* key, Ang3& value) const;
	bool getAttr(const char* key, Vec3& value) const;
	bool getAttr(const char* key, Vec4& value) const;
	bool getAttr(const char* key, Vec3d& value) const;
	bool getAttr(const char* key, Quat& value) const;
	bool getAttr(const char* key, ColorB& value) const;
	bool getAttr(const char* key, CryGUID& value) const;

protected:

private:
	CXmlNode(const CXmlNode&);
	CXmlNode& operator=(const CXmlNode&);

private:
	void             ReleaseChild(IXmlNode* pChild);
	void             removeAllChildsImpl();

	void             AddToXmlString(XmlString& xml, int level, FILE* pFile = 0, IPlatformOS::ISaveWriterPtr pSaveWriter = IPlatformOS::ISaveWriterPtr(), size_t chunkSizeBytes = 0) const;
	char*            AddToXmlStringUnsafe(char* xml, int level, char* endPtr, FILE* pFile = 0, IPlatformOS::ISaveWriterPtr pSaveWriter = IPlatformOS::ISaveWriterPtr(), size_t chunkSizeBytes = 0) const;
	XmlString        MakeValidXmlString(const XmlString& xml) const;
	bool             IsValidXmlString(const char* str) const;
	XmlAttrConstIter GetAttrConstIterator(const char* key) const
	{
		assert(m_pAttributes);

		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::const_iterator it = std::find(m_pAttributes->begin(), m_pAttributes->end(), tempAttr);
		return it;

		/*
		   XmlAttributes::const_iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		   if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		   return it;
		   return m_attributes.end();
		 */
	}
	XmlAttrIter GetAttrIterator(const char* key)
	{
		assert(m_pAttributes);

		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::iterator it = std::find(m_pAttributes->begin(), m_pAttributes->end(), tempAttr);
		return it;

		//		XmlAttributes::iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		//if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		//return it;
		//return m_attributes.end();
	}
	const char* GetValue(const char* key) const
	{
		if (m_pAttributes)
		{
			XmlAttrConstIter it = GetAttrConstIterator(key);
			if (it != m_pAttributes->end())
				return it->value;
		}
		return 0;
	}

protected:
	// String pool used by this node.
	IXmlStringPool* m_pStringPool;

	//! Tag of XML node.
	const char* m_tag;

private:

	//! Content of XML node.
	const char* m_content;
	//! Parent XML node.
	IXmlNode*   m_parent;

	//typedef DynArray<CXmlNode*,XmlDynArrayAlloc>	XmlNodes;
	typedef std::vector<IXmlNode*> XmlNodes;
	//XmlNodes m_childs;
	XmlNodes* m_pChilds;

	//! Xml node attributes.
	//XmlAttributes m_attributes;
	XmlAttributes* m_pAttributes;

	//! Line in XML file where this node firstly appeared (useful for debugging).
	int m_line;

	friend class XmlParserImp;
};

typedef stl::PoolAllocatorNoMT<sizeof(CXmlNode)> CXmlNode_PoolAlloc;
extern CXmlNode_PoolAlloc* g_pCXmlNode_PoolAlloc;

#ifdef CRY_COLLECT_XML_NODE_STATS
typedef std::set<CXmlNode*> TXmlNodeSet; // yes, slow, but really only for one-shot debugging
struct SXmlNodeStats
{
	SXmlNodeStats() : nAllocs(0), nFrees(0) {}
	TXmlNodeSet nodeSet;
	uint32      nAllocs;
	uint32      nFrees;
};
extern SXmlNodeStats* g_pCXmlNode_Stats;
#endif

/*
   //////////////////////////////////////////////////////////////////////////
   inline void* CXmlNode::operator new( size_t nSize )
   {
   void *ptr = g_pCXmlNode_PoolAlloc->Allocate();
   if (ptr)
   {
    memset( ptr,0,nSize ); // Clear objects memory.
   #ifdef CRY_COLLECT_XML_NODE_STATS
    g_pCXmlNode_Stats->nodeSet.insert(reinterpret_cast<CXmlNode*> (ptr));
   ++g_pCXmlNode_Stats->nAllocs;
   #endif
   }
   return ptr;
   }

   //////////////////////////////////////////////////////////////////////////
   inline void CXmlNode::operator delete( void *ptr )
   {
   if (ptr)
   {
    g_pCXmlNode_PoolAlloc->Deallocate(ptr);
   #ifdef CRY_COLLECT_XML_NODE_STATS
    g_pCXmlNode_Stats->nodeSet.erase(reinterpret_cast<CXmlNode*> (ptr));
   ++g_pCXmlNode_Stats->nFrees;
   #endif
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
//
// Reusable XmlNode for XmlNode pool with shared xml string pool
//
//////////////////////////////////////////////////////////////////////////
class CXmlNodePool;

class CXmlNodeReuse : public CXmlNode
{
public:
	CXmlNodeReuse(const char* tag, CXmlNodePool* pPool);
	virtual void Release();

protected:
	CXmlNodePool* m_pPool;
};

//////////////////////////////////////////////////////////////////////////
//
// Pool of reusable XML nodes with shared string pool
//
//////////////////////////////////////////////////////////////////////////
class CXmlNodePool
{
public:
	CXmlNodePool(unsigned int nBlockSize, bool bReuseStrings);
	virtual ~CXmlNodePool();

	XmlNodeRef GetXmlNode(const char* sNodeName);
	bool       empty() const { return (m_nAllocated == 0); }

protected:
	virtual void    OnRelease(int iRefCount, void* pThis);
	IXmlStringPool* GetStringPool() { return m_pStringPool; }

private:
	friend class CXmlNodeReuse;

	IXmlStringPool*            m_pStringPool;
	unsigned int               m_nAllocated;
	std::stack<CXmlNodeReuse*> m_pNodePool;
};

#endif // __XML_HEADER__
