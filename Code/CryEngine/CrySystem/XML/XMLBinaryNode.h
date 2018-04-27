// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   xml.h
//  Created:     21/04/2006 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __XML_NODE_HEADER__
#define __XML_NODE_HEADER__

#include <algorithm>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/XML/XMLBinaryHeaders.h>

// Compare function for string comparison, can be strcmp or stricmp
typedef int (__cdecl * XmlStrCmpFunc)(const char* str1, const char* str2);
extern XmlStrCmpFunc g_pXmlStrCmp;

class CBinaryXmlNode;

//////////////////////////////////////////////////////////////////////////
class CBinaryXmlData
{
public:
	const XMLBinary::Node*      pNodes;
	const XMLBinary::Attribute* pAttributes;
	const XMLBinary::NodeIndex* pChildIndices;
	const char*                 pStringData;

	const char*                 pFileContents;
	size_t                      nFileSize;
	bool                        bOwnsFileContentsMemory;

	CBinaryXmlNode*             pBinaryNodes;

	int                         nRefCount;

	CBinaryXmlData();
	~CBinaryXmlData();

	void GetMemoryUsage(ICrySizer* pSizer) const;
};

// forward declaration
namespace XMLBinary
{
class XMLBinaryReader;
};

//////////////////////////////////////////////////////////////////////////
// CBinaryXmlNode class only used for fast read only binary XML import
//////////////////////////////////////////////////////////////////////////
class CBinaryXmlNode : public IXmlNode
{
public:

	// collect allocated memory  informations
	void GetMemoryUsage(ICrySizer* pSizer) const;

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete with pool allocator.
	//////////////////////////////////////////////////////////////////////////
	//void* operator new( size_t nSize );
	//void operator delete( void *ptr );

	virtual void DeleteThis() {}

	//! Create new XML node.
	XmlNodeRef createNode(const char* tag);

	// Summary:
	//	 Reference counting.
	virtual void AddRef() { ++m_pData->nRefCount; };
	// Notes:
	//	 When ref count reach zero XML node dies.
	virtual void Release() { if (--m_pData->nRefCount <= 0) delete m_pData; };

	//! Get XML node tag.
	const char* getTag() const          { return _string(_node()->nTagStringOffset); };
	void        setTag(const char* tag) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Return true if given tag is equal to node tag.
	bool isTag(const char* tag) const;

	//! Get XML Node attributes.
	virtual int  getNumAttributes() const { return (int)_node()->nAttributeCount; };
	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex(int index, const char** key, const char** value);
	//! Return attribute key and value by attribute index, string version.
	virtual bool getAttributeByIndex(int index, XmlString& key, XmlString& value);

	virtual void shareChildren(const XmlNodeRef& fromNode) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	virtual void copyAttributes(XmlNodeRef fromNode)       { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Get XML Node attribute for specified key.
	const char* getAttr(const char* key) const;

	//! Get XML Node attribute for specified key.
	// Returns true if the attribute exists, false otherwise.
	bool getAttr(const char* key, const char** value) const;

	//! Check if attributes with specified key exist.
	bool       haveAttr(const char* key) const;

	XmlNodeRef newChild(const char* tagName)                     { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); return 0; };
	void       replaceChild(int inChild, const XmlNodeRef& node) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void       insertChild(int inChild, const XmlNodeRef& node)  { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void       addChild(const XmlNodeRef& node)                  { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void       removeChild(const XmlNodeRef& node)               { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Remove all child nodes.
	void removeAllChilds() { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Get number of child XML nodes.
	int getChildCount() const { return (int)_node()->nChildCount; };

	//! Get XML Node child nodes.
	XmlNodeRef getChild(int i) const;

	//! Find node with specified tag.
	XmlNodeRef findChild(const char* tag) const;
	void       deleteChild(const char* tag) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void       deleteChildAt(int nIndex)    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Get parent XML node.
	XmlNodeRef getParent() const;

	//! Returns content of this node.
	const char* getContent() const          { return _string(_node()->nContentStringOffset); };
	void        setContent(const char* str) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Creates a clone of the XML node. Because XMLBinaryNode is read only this creates a normal XML node (converted from the binary).
	XmlNodeRef  clone()    { return Convert(this); }

	//! Returns line number for XML tag.
	int  getLine() const   { return 0; };
	//! Set line number in xml.
	void setLine(int line) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Returns XML of this node and sub nodes.
	virtual IXmlStringData* getXMLData(int nReserveMem = 0) const                                      { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); return 0; };
	XmlString               getXML(int level = 0) const                                                { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); return ""; };
	bool                    saveToFile(const char* fileName)                                           { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); return false; }; // saves in one huge chunk
	bool                    saveToFile(const char* fileName, size_t chunkSizeBytes, FILE* file = NULL) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); return false; }; // save in small memory chunks

	//! Set new XML Node attribute (or override attribute with same key).
	void setAttr(const char* key, const char* value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, int value)                                            { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, unsigned int value)                                   { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, int64 value)                                          { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, uint64 value, bool useHexFormat = true /* ignored */) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, float value)                                          { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, double value)                                         { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Vec2& value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Vec2d& value)                                   { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Ang3& value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Vec3& value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Vec4& value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Vec3d& value)                                   { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const Quat& value)                                    { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void delAttr(const char* key)                                                       { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };
	void setAttr(const char* key, const CryGUID& value);
	void removeAllAttributes()                                                          { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); };

	//! Get attribute value of node.
	bool getAttr(const char* key, int& value) const;
	bool getAttr(const char* key, unsigned int& value) const;
	bool getAttr(const char* key, int64& value) const;
	bool getAttr(const char* key, uint64& value, bool useHexFormat = true /* ignored */) const;
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
	//	bool getAttr( const char *key,CString &value ) const { XmlString v; if (getAttr(key,v)) { value = (const char*)v; return true; } else return false; }

private:
	//////////////////////////////////////////////////////////////////////////
	// INTERNAL METHODS
	//////////////////////////////////////////////////////////////////////////
	const char* GetValue(const char* key) const
	{
		const XMLBinary::Attribute* const pAttributes = m_pData->pAttributes;
		const char* const pStringData = m_pData->pStringData;

		const int nFirst = _node()->nFirstAttributeIndex;
		const int nLast = nFirst + _node()->nAttributeCount;
		for (int i = nFirst; i < nLast; i++)
		{
			const char* const attrKey = pStringData + pAttributes[i].nKeyStringOffset;
			if (g_pXmlStrCmp(key, attrKey) == 0)
			{
				const char* attrValue = pStringData + pAttributes[i].nValueStringOffset;
				return attrValue;
			}
		}
		return 0;
	}

	// Return current node in binary data.
	const XMLBinary::Node* _node() const
	{
		// cppcheck-suppress thisSubtraction
		return &m_pData->pNodes[this - m_pData->pBinaryNodes];
	}

	const char* _string(int nIndex) const
	{
		return m_pData->pStringData + nIndex;
	}

	// Converts the binary source node to a normal XML node.
	XmlNodeRef Convert(XmlNodeRef source);

protected:
	virtual void setParent(const XmlNodeRef& inRef) { CRY_ASSERT_MESSAGE(0, "XMLBinaryNode is read only. Use clone() to create an editable node."); }

	//////////////////////////////////////////////////////////////////////////
private:
	CBinaryXmlData* m_pData;

	friend class XMLBinary::XMLBinaryReader;
};

#endif // __XML_NODE_HEADER__
