// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

XmlNodeRef FindChildNode(XmlNodeRef pParent, const int childIndexOverride, int& childIndexHint, const char* const name)
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
bool ReadChildNodeAs(XmlNodeRef pParent, const int childIndexOverride, int& childIndexHint, const char* const name, TOut& valueOut)
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
bool ReadChildNode(XmlNodeRef pParent, const int childIndexOverride, int& childIndexHint, const char* const name, T& valueOut)
{
	return ReadChildNodeAs<T>(pParent, childIndexOverride, childIndexHint, name, valueOut);
}
}

Serialization::CXmlIArchive::CXmlIArchive()
	: IArchive(INPUT | NO_EMPTY_NAMES | VALIDATION)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
}

Serialization::CXmlIArchive::CXmlIArchive(XmlNodeRef pRootNode)
	: IArchive(INPUT | NO_EMPTY_NAMES | VALIDATION)
	, m_pRootNode(pRootNode)
	, m_childIndexOverride(-1)
	, m_childIndexHint(0)
{
	CRY_ASSERT(m_pRootNode);
}

Serialization::CXmlIArchive::~CXmlIArchive()
{
}

void Serialization::CXmlIArchive::SetXmlNode(XmlNodeRef pNode)
{
	m_pRootNode = pNode;
}

XmlNodeRef Serialization::CXmlIArchive::GetXmlNode() const
{
	return m_pRootNode;
}

bool Serialization::CXmlIArchive::operator()(bool& value, const char* name, const char* label)
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

bool Serialization::CXmlIArchive::operator()(IString& value, const char* name, const char* label)
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

bool Serialization::CXmlIArchive::operator()(IWString& value, const char* name, const char* label)
{
	CryFatalError("CXmlIArchive::operator() with IWString is not implemented");
	return false;
}

bool Serialization::CXmlIArchive::operator()(float& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(double& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(int16& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(uint16& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<uint>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(int32& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(uint32& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(int64& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(uint64& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(int8& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(uint8& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<uint>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(char& value, const char* name, const char* label)
{
	return XmlUtil::ReadChildNodeAs<int>(m_pRootNode, m_childIndexOverride, m_childIndexHint, name, value);
}

bool Serialization::CXmlIArchive::operator()(const SStruct& ser, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		CXmlIArchive childArchive(pChild);
		childArchive.setFilter(getFilter());
		childArchive.setLastContext(lastContext());

		const bool serializeSuccess = ser(childArchive);
		return serializeSuccess;
	}
	return false;
}

bool Serialization::CXmlIArchive::operator()(SBlackBox& box, const char* name, const char* label)
{
	CRY_ASSERT(name);
	CRY_ASSERT(name[0]);

	XmlNodeRef pChild = XmlUtil::FindChildNode(m_pRootNode, m_childIndexOverride, m_childIndexHint, name);
	if (pChild)
	{
		box.set<XmlNodeRef>("xml", pChild);
		return true;
	}
	return false;
}

bool Serialization::CXmlIArchive::operator()(IContainer& ser, const char* name, const char* label)
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
			CXmlIArchive childArchive(pChild);
			childArchive.setFilter(getFilter());
			childArchive.setLastContext(lastContext());

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

void Serialization::CXmlIArchive::validatorMessage(bool error, const void* handle, const TypeID& type, const char* message)
{
	const EValidatorModule module = VALIDATOR_MODULE_UNKNOWN;
	const EValidatorSeverity severity = error ? VALIDATOR_ERROR : VALIDATOR_WARNING;
	CryWarning(module, severity, "CXmlIArchive: %s", message);
}
