// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryExtension/ClassWeaver.h>

#include <CrySerialization/STL.h>
#include <CrySerialization/ClassFactory.h>

#include "XmlIArchive.h"

#include <CrySerialization/BlackBox.h>

namespace XmlUtil
{
int g_hintSuccess = 0;
int g_hintFail = 0;

XmlNodeRef FindChildNode(const XmlNodeRef &pParent, const int childIndexOverride, int& childIndexHint, const char* const name)
{
	CRY_ASSERT(pParent);

	if (0 <= childIndexOverride)
	{
		CRY_ASSERT(childIndexOverride < pParent->getChildCount());
		return pParent->getChild(childIndexOverride);
	}
	else
	{
		CRY_ASSERT(name);
		CRY_ASSERT(name[0]);
		CRY_ASSERT(0 <= childIndexHint);

		const int childCount = pParent->getChildCount();
		const bool hasValidChildHint = (childIndexHint < childCount);
		if (hasValidChildHint)
		{
			XmlNodeRef pChildNode = pParent->getChild(childIndexHint);
			if (pChildNode->isTag(name))
			{
				g_hintSuccess++;
				const int nextChildIndexHint = childIndexHint + 1;
				childIndexHint = (nextChildIndexHint < childCount) ? nextChildIndexHint : 0;
				return pChildNode;
			}
			else
			{
				g_hintFail++;
			}
		}

		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pChildNode = pParent->getChild(i);
			if (pChildNode->isTag(name))
			{
				const int nextChildIndexHint = i + 1;
				childIndexHint = (nextChildIndexHint < childCount) ? nextChildIndexHint : 0;
				return pChildNode;
			}
		}
	}
	return XmlNodeRef();
}

template<typename T, typename TOut>
bool ReadChildNodeAs(const XmlNodeRef &pParent, const int childIndexOverride, int& childIndexHint, const char* const name, TOut& valueOut)
{
	XmlNodeRef pChild = FindChildNode(pParent, childIndexOverride, childIndexHint, name);
	if (pChild)
	{
		T tmp;
		const bool readValueSuccess = pChild->getAttr("value", tmp);
		if (readValueSuccess)
		{
			valueOut = tmp;
		}
		return readValueSuccess;
	}
	return false;
}

template<typename T>
bool ReadChildNode(const XmlNodeRef &pParent, const int childIndexOverride, int& childIndexHint, const char* const name, T& valueOut)
{
	return ReadChildNodeAs<T>(pParent, childIndexOverride, childIndexHint, name, valueOut);
}
}

Serialization::CXmlIArchiveVer1::CXmlIArchiveVer1()
	: IArchive(INPUT | NO_EMPTY_NAMES | VALIDATION | XML_VERSION_1)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
}

Serialization::CXmlIArchiveVer1::CXmlIArchiveVer1(const CXmlIArchiveVer1 &parent,const XmlNodeRef &pRootNode)
	: IArchive(parent.caps_)
	, m_pRootNode(pRootNode)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
	filter_ = parent.filter_;
	modifiedRow_ = parent.modifiedRow_;
	lastContext_ = parent.lastContext_;

	CRY_ASSERT(m_pRootNode);
}

Serialization::CXmlIArchiveVer1::~CXmlIArchiveVer1()
{
}

void Serialization::CXmlIArchiveVer1::SetXmlNode(XmlNodeRef pNode)
{
	m_pRootNode = pNode;
}

XmlNodeRef Serialization::CXmlIArchiveVer1::GetXmlNode() const
{
	return m_pRootNode;
}

bool Serialization::CXmlIArchiveVer1::operator()(bool& value, const char* name, const char* label)
{
	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		const char* const stringValue = pChild->getAttr("value");
		if (stringValue)
		{
			value = (strcmp("true", stringValue) == 0);
			value = value || (strcmp("1", stringValue) == 0);
			return true;
		}
		return false;
	}
	return false;
}

bool Serialization::CXmlIArchiveVer1::operator()(IString& value, const char* name, const char* label)
{
	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		const char* const stringValue = pChild->getAttr("value");
		if (stringValue)
		{
			value.set(stringValue);
			return true;
		}
		return false;
	}
	return false;
}

bool Serialization::CXmlIArchiveVer1::operator()(IWString& value, const char* name, const char* label)
{
	CryFatalError("CXmlIArchive::operator() with IWString is not implemented");
	return false;
}

bool Serialization::CXmlIArchiveVer1::operator()(float& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(double& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(int16& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(uint16& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<uint>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(int32& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(uint32& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(int64& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(uint64& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(int8& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(uint8& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<uint>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(char& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchiveVer1::operator()(const SStruct& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		CXmlIArchiveVer1 childArchive(*this,pChild);

		const bool serializeSuccess = ser(childArchive);
		return serializeSuccess;
	}
	return false;
}

bool Serialization::CXmlIArchiveVer1::operator()(SBlackBox& box, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		box.set<XmlNodeRef>("xml", pChild);
		box.xmlVersion = 1;
		return true;
	}
	return false;
}

bool Serialization::CXmlIArchiveVer1::operator()(IContainer& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		bool serializeSuccess = true;

		const int elementCount = pChild->getChildCount();
		ser.resize(elementCount);

		if (0 < elementCount)
		{
			CXmlIArchiveVer1 childArchive(*this,pChild);

			for (int i = 0; i < elementCount; ++i)
			{
				childArchive.m_childIndexOverride = i;

				serializeSuccess &= ser(childArchive, "Element", "Element");
				ser.next();
			}
		}

		return serializeSuccess;
	}
	return false;
}

void Serialization::CXmlIArchiveVer1::validatorMessage(bool error, const void* handle, const TypeID& type, const char* message)
{
	const EValidatorModule module = VALIDATOR_MODULE_UNKNOWN;
	const EValidatorSeverity severity = error ? VALIDATOR_ERROR : VALIDATOR_WARNING;
	CryWarning(module, severity, "CXmlIArchive: %s", message);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Serialization::CXmlInputArchive::CXmlInputArchive()
	: IArchive(INPUT | NO_EMPTY_NAMES | VALIDATION)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
}

Serialization::CXmlInputArchive::CXmlInputArchive(const CXmlInputArchive &parent,const XmlNodeRef &pRootNode)
	: IArchive(parent.caps_)
	, m_pRootNode(pRootNode)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
	filter_ = parent.filter_;
	modifiedRow_ = parent.modifiedRow_;
	lastContext_ = parent.lastContext_;

	CRY_ASSERT(m_pRootNode);
}

Serialization::CXmlInputArchive::~CXmlInputArchive()
{
}

void Serialization::CXmlInputArchive::SetXmlNode(XmlNodeRef pNode)
{
	m_pRootNode = pNode;
}

XmlNodeRef Serialization::CXmlInputArchive::GetXmlNode() const
{
	return m_pRootNode;
}

bool Serialization::CXmlInputArchive::operator()(bool& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(IString& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	if (!node->haveAttr(name))
		return false;
	
	const char* stringValue = node->getAttr(name);
	if (stringValue)
	{
		value.set(stringValue);
		return true;
	}
	return false;
}

bool Serialization::CXmlInputArchive::operator()(IWString& value, const char* name, const char* label)
{
	CRY_ASSERT_MESSAGE(0,"CXmlInputArchive::operator() with IWString is not implemented");
	CryFatalError("CXmlInputArchive::operator() with IWString is not implemented");
	return false;
}

bool Serialization::CXmlInputArchive::operator()(float& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(double& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(int16& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(uint16& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(int32& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(uint32& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
 	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(int64& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(uint64& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(int8& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	int32 v = value;
	bool res = node->getAttr(name,v);
	value = v;
	return res;
}

bool Serialization::CXmlInputArchive::operator()(uint8& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(char& value, const char* name, const char* label)
{
	XmlNodeRef node = (m_childIndexOverride >= 0) ? XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name) : m_pRootNode;
	return node->getAttr(name,value);
}

bool Serialization::CXmlInputArchive::operator()(const SStruct& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		CXmlInputArchive childArchive(*this,pChild);

		const bool serializeSuccess = ser(childArchive);
		return serializeSuccess;
	}
	return false;
}

bool Serialization::CXmlInputArchive::operator()(SBlackBox& box, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		pChild->AddRef();
		box.set<XmlNodeRef>("xml", pChild);
		box.xmlVersion = 2;
		return true;
	}
	return false;
}

bool Serialization::CXmlInputArchive::operator()(IContainer& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		bool serializeSuccess = true;

		const int elementCount = pChild->getChildCount();
		ser.resize(elementCount);

		if (0 < elementCount)
		{
			CXmlInputArchive childArchive(*this,pChild);

			for (int i = 0; i < elementCount; ++i)
			{
				childArchive.m_childIndexOverride = i;

				serializeSuccess &= ser(childArchive, "element", "element");
				ser.next();
			}
		}

		return serializeSuccess;
	}
	return false;
}

void Serialization::CXmlInputArchive::validatorMessage(bool error, const void* handle, const TypeID& type, const char* message)
{
	const EValidatorModule module = VALIDATOR_MODULE_UNKNOWN;
	const EValidatorSeverity severity = error ? VALIDATOR_ERROR : VALIDATOR_WARNING;
	CryWarning(module, severity, "CXmlInputArchive: %s", message);
}
