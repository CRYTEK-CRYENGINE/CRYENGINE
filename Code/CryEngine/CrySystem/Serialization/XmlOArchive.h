// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XML_O_ARCHIVE__H__
#define __XML_O_ARCHIVE__H__

#include <CrySerialization/IArchive.h>

namespace Serialization
{
class CXmlOArchiveVer1 : public IArchive
{
public:
	CXmlOArchiveVer1();
	CXmlOArchiveVer1(XmlNodeRef pRootNode);
	~CXmlOArchiveVer1();

	void       SetXmlNode(XmlNodeRef pNode);
	XmlNodeRef GetXmlNode() const;

	// IArchive
	bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	bool operator()(IString& value, const char* name = "", const char* label = 0) override;
	bool operator()(IWString& value, const char* name = "", const char* label = 0) override;
	bool operator()(float& value, const char* name = "", const char* label = 0) override;
	bool operator()(double& value, const char* name = "", const char* label = 0) override;
	bool operator()(int16& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint16& value, const char* name = "", const char* label = 0) override;
	bool operator()(int32& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint32& value, const char* name = "", const char* label = 0) override;
	bool operator()(int64& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint64& value, const char* name = "", const char* label = 0) override;

	bool operator()(int8& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint8& value, const char* name = "", const char* label = 0) override;
	bool operator()(char& value, const char* name = "", const char* label = 0) override;

	bool operator()(const SStruct& ser, const char* name = "", const char* label = 0) override;
	bool operator()(IContainer& ser, const char* name = "", const char* label = 0) override;
	bool operator()(SBlackBox& box, const char* name = "", const char* label = 0) override;
	// ~IArchive

	using IArchive::operator();

private:
	XmlNodeRef m_pRootNode;
};

class CXmlOutputArchive : public IArchive
{
public:
	CXmlOutputArchive();
	CXmlOutputArchive(const CXmlOutputArchive &ar);
	~CXmlOutputArchive();

	void       SetXmlNode(XmlNodeRef pNode);
	XmlNodeRef GetXmlNode() const;

	// IArchive
	bool operator()(bool& value, const char* name = "", const char* label = 0) override;
	bool operator()(IString& value, const char* name = "", const char* label = 0) override;
	bool operator()(IWString& value, const char* name = "", const char* label = 0) override;
	bool operator()(float& value, const char* name = "", const char* label = 0) override;
	bool operator()(double& value, const char* name = "", const char* label = 0) override;
	bool operator()(int16& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint16& value, const char* name = "", const char* label = 0) override;
	bool operator()(int32& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint32& value, const char* name = "", const char* label = 0) override;
	bool operator()(int64& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint64& value, const char* name = "", const char* label = 0) override;

	bool operator()(int8& value, const char* name = "", const char* label = 0) override;
	bool operator()(uint8& value, const char* name = "", const char* label = 0) override;
	bool operator()(char& value, const char* name = "", const char* label = 0) override;

	bool operator()(const SStruct& ser, const char* name = "", const char* label = 0) override;
	bool operator()(IContainer& ser, const char* name = "", const char* label = 0) override;
	bool operator()(SBlackBox& box, const char* name = "", const char* label = 0) override;
	// ~IArchive

	using IArchive::operator();

private:
	XmlNodeRef m_pRootNode;
	bool m_bArray = false;
};
}

#endif
