// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryExtension/ClassWeaver.h>

#include <CrySerialization/STL.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/BlackBox.h>

#include "XmlOArchive.h"

namespace XmlUtil
{
XmlNodeRef CreateChildNode(XmlNodeRef pParent, const char* const name)
{
	CRY_ASSERT(pParent);
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = pParent->createNode(name);
	CRY_ASSERT(pChild);

	pParent->addChild(pChild);
	return pChild;
}

template<typename T, typename TIn>
bool WriteChildNodeAs(XmlNodeRef pParent, const char* const name, const TIn& value)
{
	XmlNodeRef pChild = XmlUtil::CreateChildNode(pParent, name);
	CRY_ASSERT(pChild);

	pChild->setAttr("value", static_cast<T>(value));
	return true;
}

template<typename T>
bool WriteChildNode(XmlNodeRef pParent, const char* const name, const T& value)
{
	return WriteChildNodeAs<T>(pParent, name, value);
}
}

Serialization::CXmlOArchiveVer1::CXmlOArchiveVer1()
	: IArchive(OUTPUT | NO_EMPTY_NAMES | XML_VERSION_1)
{
}

Serialization::CXmlOArchiveVer1::CXmlOArchiveVer1(XmlNodeRef pRootNode)
	: IArchive(OUTPUT | NO_EMPTY_NAMES | XML_VERSION_1)
	, m_pRootNode(pRootNode)
{
	CRY_ASSERT(m_pRootNode);
}

Serialization::CXmlOArchiveVer1::~CXmlOArchiveVer1()
{
}

void Serialization::CXmlOArchiveVer1::SetXmlNode(XmlNodeRef pNode)
{
	m_pRootNode = pNode;
}

XmlNodeRef Serialization::CXmlOArchiveVer1::GetXmlNode() const
{
	return m_pRootNode;
}

bool Serialization::CXmlOArchiveVer1::operator()(bool& value, const char* name, const char* label)
{
	const char* const stringValue = value ? "true" : "false";
	return XmlUtil::WriteChildNode(m_pRootNode, name, stringValue);
}

bool Serialization::CXmlOArchiveVer1::operator()(IString& value, const char* name, const char* label)
{
	const char* const stringValue = value.get();
	return XmlUtil::WriteChildNode(m_pRootNode, name, stringValue);
}

bool Serialization::CXmlOArchiveVer1::operator()(IWString& value, const char* name, const char* label)
{
	CryFatalError("CXmlOArchive::operator() with IWString is not implemented");
	return false;
}

bool Serialization::CXmlOArchiveVer1::operator()(float& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(double& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(int16& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNodeAs<int>(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(uint16& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNodeAs<uint>(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(int32& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(uint32& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(int64& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(uint64& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNode(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(int8& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNodeAs<int>(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(uint8& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNodeAs<uint>(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(char& value, const char* name, const char* label)
{
	return XmlUtil::WriteChildNodeAs<int>(m_pRootNode, name, value);
}

bool Serialization::CXmlOArchiveVer1::operator()(const SStruct& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::CreateChildNode(m_pRootNode, name);
	CXmlOArchiveVer1 childArchive(pChild);
	childArchive.setFilter(getFilter());
	childArchive.setLastContext(lastContext());

	const bool serializeSuccess = ser(childArchive);

	return serializeSuccess;
}

bool Serialization::CXmlOArchiveVer1::operator()(SBlackBox& box, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);
	CRY_ASSERT(m_pRootNode->isTag(name));

	if ((strcmp(box.format, "xml") != 0) || !box.data)
		return false;

	m_pRootNode->addChild(*static_cast<XmlNodeRef*>(box.data));

	return true;
}

bool Serialization::CXmlOArchiveVer1::operator()(IContainer& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	bool serializeSuccess = true;

	XmlNodeRef pChild = XmlUtil::CreateChildNode(m_pRootNode, name);
	CXmlOArchiveVer1 childArchive(pChild);
	childArchive.setFilter(getFilter());
	childArchive.setLastContext(lastContext());

	const size_t containerSize = ser.size();
	if (0 < containerSize)
	{
		do
		{
			serializeSuccess &= ser(childArchive, "Element", "Element");
		}
		while (ser.next());
	}

	return serializeSuccess;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Serialization::CXmlOutputArchive::CXmlOutputArchive()
	: IArchive(OUTPUT | NO_EMPTY_NAMES)
{
}

Serialization::CXmlOutputArchive::CXmlOutputArchive(const CXmlOutputArchive &ar)
	: IArchive(ar.caps_)
	, m_pRootNode(ar.m_pRootNode)
{
	filter_ = ar.filter_;
	modifiedRow_ = ar.modifiedRow_;
	lastContext_ = ar.lastContext_;
	CRY_ASSERT(m_pRootNode);
}

Serialization::CXmlOutputArchive::~CXmlOutputArchive()
{
}

void Serialization::CXmlOutputArchive::SetXmlNode(XmlNodeRef pNode)
{
	m_pRootNode = pNode;
}

XmlNodeRef Serialization::CXmlOutputArchive::GetXmlNode() const
{
	return m_pRootNode;
}

bool Serialization::CXmlOutputArchive::operator()(bool& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	const char* const stringValue = value ? "true" : "false";
	node->setAttr(name, stringValue);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(IString& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	const char* const stringValue = value.get();
	node->setAttr(name, stringValue);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(IWString& value, const char* name, const char* label)
{
	CRY_ASSERT_MESSAGE(0,"CXmlOutputArchive::operator() with IWString is not implemented");
	CryFatalError("CXmlOutputArchive::operator() with IWString is not implemented");
	return false;
}

bool Serialization::CXmlOutputArchive::operator()(float& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(double& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(int16& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(uint16& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(int32& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(uint32& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(int64& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(uint64& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(int8& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(uint8& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(char& value, const char* name, const char* label)
{
	XmlNodeRef node = (!m_bArray) ? m_pRootNode : XmlUtil::CreateChildNode(m_pRootNode, name);
	node->setAttr(name, value);
	return true;
}

bool Serialization::CXmlOutputArchive::operator()(const SStruct& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::CreateChildNode(m_pRootNode, name);
	CXmlOutputArchive childArchive(*this);
	childArchive.SetXmlNode(pChild);

	const bool serializeSuccess = ser(childArchive);

	return serializeSuccess;
}

bool Serialization::CXmlOutputArchive::operator()(SBlackBox& box, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);
	CRY_ASSERT(m_pRootNode->isTag(name));

	if ((strcmp(box.format, "xml") != 0) || !box.data)
		return false;

	m_pRootNode->addChild(*static_cast<XmlNodeRef*>(box.data));

	return true;
}

bool Serialization::CXmlOutputArchive::operator()(IContainer& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	bool serializeSuccess = true;

	CXmlOutputArchive childArchive(*this);
	childArchive.SetXmlNode(XmlUtil::CreateChildNode(m_pRootNode, name));
	childArchive.m_bArray = true;

	const size_t containerSize = ser.size();
	if (0 < containerSize)
	{
		do
		{
			serializeSuccess &= ser(childArchive, "element", "element");
		} while (ser.next());
	}

	return serializeSuccess;
}
