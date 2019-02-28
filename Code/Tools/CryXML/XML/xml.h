// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XML_HEADER__
#define __XML_HEADER__

#include <vector>
#include <set>
#include <algorithm>

#include <CrySystem/XML/IXml.h>

struct IXmlBufferSource;

struct IXmlStringPool
{
public:
	IXmlStringPool() { m_refCount = 0; }
	virtual ~IXmlStringPool() {};
	void AddRef() { m_refCount++; };
	void Release() {
		if (--m_refCount <= 0) 
			delete this; 
	};
	virtual char* AddString( const char *str ) = 0;
private:
	int m_refCount;
};

/************************************************************************/
/* XmlParser class, Parse xml and return root xml node if success.      */
/************************************************************************/
class XmlParser
{
public:
	explicit XmlParser(bool bRemoveNonessentialSpacesFromContent);
	~XmlParser();

	//! Parse xml file.
	XmlNodeRef parse( const char *fileName );

	//! Parse xml from memory buffer.
	XmlNodeRef parseBuffer( const char *buffer );

	XmlNodeRef parseSource(const IXmlBufferSource* source);

	const char* getErrorString() const { return m_errorString; }

private:
	// Prevent copy construction
	XmlParser(const XmlParser&);
	XmlParser& operator = (const XmlParser&);
	
private:
	XmlString m_errorString;
	class XmlParserImp *m_pImpl;
};

// Compare function for string comparasion, can be strcmp or stricmp
typedef int (__cdecl *XmlStrCmpFunc)( const char *str1,const char *str2 );
extern XmlStrCmpFunc g_pXmlStrCmp;

//////////////////////////////////////////////////////////////////////////
// XmlAttribute class
//////////////////////////////////////////////////////////////////////////
struct XmlAttribute
{
	const char* key;
	const char* value;

	bool operator<( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) < 0; }
	bool operator>( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) > 0; }
	bool operator==( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) == 0; }
	bool operator!=( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) != 0; }
};

//! Xml node attributes class.
typedef std::vector<XmlAttribute>	XmlAttributes;
typedef XmlAttributes::iterator XmlAttrIter;
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
	explicit CXmlNode( const char *tag );
	//! Destructor.
	~CXmlNode();

	virtual void DeleteThis();

	//! Create new XML node.
	XmlNodeRef createNode( const char *tag );

	//! Get XML node tag.
	const char *getTag() const { return m_tag; };
	void	setTag( const char *tag );

	//! Return true if given tag equal to node tag.
	bool isTag( const char *tag ) const;

	//! Get XML Node attributes.
	virtual int getNumAttributes() const { return (int)m_attributes.size(); };
	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex( int index,const char **key,const char **value );

	virtual void copyAttributes( XmlNodeRef fromNode );

	//! Get XML Node attribute for specified key.
	const char* getAttr( const char *key ) const;

	//! Get XML Node attribute for specified key.
	// Returns true if the attribute existes, alse otherwise.
	bool getAttr(const char *key, const char **value) const;

	//! Check if attributes with specified key exist.
	bool haveAttr( const char *key ) const;

	//! Creates new xml node and add it to childs list.
	XmlNodeRef newChild( const char *tagName );

	//! Adds new child node.
	void addChild( const XmlNodeRef &node );
	//! Remove child node.
	void removeChild( const XmlNodeRef &node );

	void insertChild( int nIndex, const XmlNodeRef &node );
	void replaceChild( int nIndex, const XmlNodeRef &node );

	//! Remove all child nodes.
	void removeAllChilds();

	//! Get number of child XML nodes.
	int	getChildCount() const { return (int)m_childs.size(); };

	//! Get XML Node child nodes.
	XmlNodeRef getChild( int i ) const;

	//! Find node with specified tag.
	XmlNodeRef findChild( const char *tag ) const;
	void deleteChild( const char *tag );
	void deleteChildAt( int nIndex );

	//! Get parent XML node.
	XmlNodeRef getParent() const { return m_parent; }
	void setParent(const XmlNodeRef &inRef);

	//! Returns content of this node.
	const char* getContent() const { return m_content.c_str(); };
	void setContent( const char *str );

	XmlNodeRef	clone();

	//! Returns line number for XML tag.
	int getLine() const { return m_line; };
	//! Set line number in xml.
	void setLine( int line ) { m_line = line; };

	//! Returns XML of this node and sub nodes.
	virtual IXmlStringData* getXMLData( int nReserveMem=0 ) const;
	XmlString getXML( int level=0 ) const;
	bool saveToFile( const char *fileName );

	//! Set new XML Node attribute (or override attribute with same key).
	void setAttr( const char* key,const char* value );
	void setAttr( const char* key,int value );
	void setAttr( const char* key,unsigned int value );
	void setAttr( const char* key,int64 value );
	void setAttr( const char* key,uint64 value,bool useHexFormat = true  );
	void setAttr( const char* key,float value );
	void setAttr( const char* key,double value );
	void setAttr( const char* key,const Vec2& value );
	void setAttr( const char* key,const Vec2d& value );
	void setAttr( const char* key,const Ang3& value );
	void setAttr( const char* key,const Vec3& value );
	void setAttr( const char* key,const Vec4& value );
	void setAttr( const char* key,const Vec3d& value );
	void setAttr( const char* key,const Quat &value );

	//! Delete attrbute.
	void delAttr( const char* key );
	//! Remove all node attributes.
	void removeAllAttributes();

	//! Get attribute value of node.
	bool getAttr( const char *key,int &value ) const;
	bool getAttr( const char *key,unsigned int &value ) const;
	bool getAttr( const char *key,int64 &value ) const;
	bool getAttr( const char *key,uint64 &value,bool useHexFormat = true  /*ignored*/) const;
	bool getAttr( const char *key,float &value ) const;
	bool getAttr( const char *key,double &value ) const;
	bool getAttr( const char *key,bool &value ) const;
	bool getAttr( const char *key,XmlString &value ) const { XmlString v; if (v=getAttr(key)) { value = v; return true; } else return false; }
	bool getAttr( const char *key,Vec2& value ) const;
	bool getAttr( const char *key,Vec2d& value ) const;
	bool getAttr( const char *key,Ang3& value ) const;
	bool getAttr( const char *key,Vec3& value ) const;
	bool getAttr( const char *key,Vec3d& value ) const;
	bool getAttr( const char *key,Vec4& value ) const;
	bool getAttr( const char *key,Quat &value ) const;
	bool getAttr( const char *key,ColorB &value ) const;
	//	bool getAttr( const char *key,string &value ) const { XmlString v; if (getAttr(key,v)) { value = (const char*)v; return true; } else return false; }

private:
	void AddToXmlString( XmlString &xml,int level ) const;
	XmlString MakeValidXmlString( const XmlString &xml ) const;
	bool IsValidXmlString( const char *str ) const;
	XmlAttrConstIter GetAttrConstIterator( const char *key ) const
	{
		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::const_iterator it = std::find( m_attributes.begin(),m_attributes.end(),tempAttr );
		return it;

		/*
		XmlAttributes::const_iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		return it;
		return m_attributes.end();
		*/
	}
	XmlAttrIter GetAttrIterator( const char *key )
	{
		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::iterator it = std::find( m_attributes.begin(),m_attributes.end(),tempAttr );
		return it;

		//		XmlAttributes::iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		//if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		//return it;
		//return m_attributes.end();
	}
	const char* GetValue( const char *key ) const
	{
		XmlAttrConstIter it = GetAttrConstIterator(key);
		if (it != m_attributes.end())
			return it->value;
		return 0;
	}

private:
	//! Line in XML file where this node firstly appeared (usefull for debugging).
	int m_line;

	//! Tag of XML node.
	const char *m_tag;
	//! Content of XML node.
	XmlString m_content;
	//! Parent XML node.
	CXmlNode *m_parent;

	// String pool used by this node.
	IXmlStringPool *m_pStringPool;

	typedef std::vector<XmlNodeRef>	XmlNodes;
	XmlNodes m_childs;
	//! Xml node attributes.
	XmlAttributes m_attributes;

	friend class XmlParserImp;
};

#endif // __XML_HEADER__


/*
#ifndef __XML_HEADER__
#define __XML_HEADER__



/*
class CXmlNode : public IXmlNode
{
public:
	//! Constructor.
	CXmlNode( const char *tag );
	//! Destructor.
	~CXmlNode();

	//////////////////////////////////////////////////////////////////////////
	//! Reference counting.
	void AddRef() { m_refCount++; };
	//! When ref count reach zero XML node dies.
	void Release();

	//! Create new XML node.
	XmlNodeRef createNode( const char *tag );

	//! Get XML node tag.
	const char *getTag() const { return m_tag; };
	void	setTag( const char *tag ) { m_tag = tag; }

	//! Return true if givven tag equal to node tag.
	bool isTag( const char *tag ) const;

	//! Get XML Node attributes.
	virtual int getNumAttributes() const { return (int)m_attributes.size(); };
	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex( int index,const char **key,const char **value );

	virtual void* getFirstAttribute();
	virtual bool getNextAttribute( void** pIterator,const char **key,const char **value );

	virtual void copyAttributes( XmlNodeRef fromNode );

	//! Get XML Node attribute for specified key.
	const char* getAttr( const char *key ) const;
	//! Check if attributes with specified key exist.
	bool haveAttr( const char *key ) const;
	
	//! Adds new child node.
	void addChild( const XmlNodeRef &node );

	//! Creates new xml node and add it to childs list.
	XmlNodeRef newChild( const char *tagName );

	//! Remove child node.
	void removeChild( const XmlNodeRef &node );

	//! Remove all child nodes.
	void removeAllChilds();

	//! Get number of child XML nodes.
	int	getChildCount() const { return (int)m_childs.size(); };
	
	//! Get XML Node child nodes.
	XmlNodeRef getChild( int i ) const;

	//! Find node with specified tag.
	XmlNodeRef findChild( const char *tag ) const;

	//! Get parent XML node.
	XmlNodeRef	getParent() const { return m_parent; }

	//! Returns content of this node.
	const char* getContent() const { return m_content; };
	void setContent( const char *str ) { m_content = str; };
	void addContent( const char *str ) { m_content += str; };

	XmlNodeRef	clone();

	//! Returns line number for XML tag.
	int getLine() const { return m_line; };
	//! Set line number in xml.
	void setLine( int line ) { m_line = line; };

	//! Returns XML of this node and sub nodes.
	XmlString getXML( int level=0 ) const;
	XmlString getBinaryXML() const;
	bool saveToFile( const char *fileName, bool bBinary = false );
	bool saveToSink(IXMLDataSink* pSink, bool bBinary = false);

	//! Set new XML Node attribute (or override attribute with same key).
	void setAttr( const char* key,const char* value );
	void setAttr( const char* key,int value );
	void setAttr( const char* key,unsigned int value );
	void setAttr( const char* key,uint64 value );
	void setAttr( const char* key,float value );
	void setAttr( const char* key,const Ang3& value );
	void setAttr( const char* key,const Vec3& value );
	void setAttr( const char* key,const Quat &value );

	//! Delete attribute.
	void delAttr( const char* key );
	//! Remove all node attributes.
	void removeAllAttributes();

	//! Get attribute value of node.
	bool getAttr( const char *key,int &value ) const;
	bool getAttr( const char *key,unsigned int &value ) const;
	bool getAttr( const char *key,uint64 &value ) const;
	bool getAttr( const char *key,float &value ) const;
	bool getAttr( const char *key,Ang3& value ) const;
	bool getAttr( const char *key,Vec3& value ) const;
	bool getAttr( const char *key,Quat &value ) const;
	bool getAttr( const char *key,bool &value ) const;
	bool getAttr( const char *key,XmlString &value ) const { XmlString v; if (v=getAttr(key)) { value = v; return true; } else return false; }
//	bool getAttr( const char *key,string &value ) const { XmlString v; if (getAttr(key,v)) { value = (const char*)v; return true; } else return false; }

	// Add an attribute structure directly.
	void addAttr(XmlAttribute& attribute);

	void SetBuffer(StringBuffer* pStringBuffer);

private:
	void AddToXmlString( XmlString &xml,int level ) const;

private:
	StringBuffer* m_pStringBuffer;

	//! Ref count itself, its zeroed on node creation.
	int m_refCount;

	//! Line in XML file where this node firstly appeared (usefull for debuggin).
	int m_line;
	//! Tag of XML node.
	XmlString m_tag;

	//! Content of XML node.
	XmlString m_content;
	//! Parent XML node.
	CXmlNode *m_parent;
	//! Next XML node in same hierarchy level.

	typedef std::vector<XmlNodeRef>	XmlNodes;
	XmlNodes m_childs;
	//! Xml node attributes.
	XmlAttributes m_attributes;
	static XmlAttribute tempAttr;
};

#endif // __XML_HEADER__
*/