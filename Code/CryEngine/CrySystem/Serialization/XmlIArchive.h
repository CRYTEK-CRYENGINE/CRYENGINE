// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XML_I_ARCHIVE__H__
#define __XML_I_ARCHIVE__H__

#include <CrySerialization/IArchive.h>

namespace Serialization
{
class CXmlIArchiveVer1 : public IArchive
{
public:
	CXmlIArchiveVer1();
	CXmlIArchiveVer1(const CXmlIArchiveVer1 &parent,const XmlNodeRef &pRootNode);
	~CXmlIArchiveVer1();

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
	bool operator()(SBlackBox& ser, const char* name = "", const char* label = 0) override;
	bool operator()(IContainer& ser, const char* name = "", const char* label = 0) override;

	void validatorMessage(bool error, const void* handle, const TypeID& type, const char* message) override;
	// ~IArchive

	using IArchive::operator();

private:
	XmlNodeRef m_pRootNode;
	int        m_childIndexOverride;
	int        m_childIndexHint;
};

class CXmlInputArchive : public IArchive
{
public:
	CXmlInputArchive();
	CXmlInputArchive(const CXmlInputArchive &parent,const XmlNodeRef &pRootNode);
	~CXmlInputArchive();

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
	bool operator()(SBlackBox& ser, const char* name = "", const char* label = 0) override;
	bool operator()(IContainer& ser, const char* name = "", const char* label = 0) override;

	void validatorMessage(bool error, const void* handle, const TypeID& type, const char* message) override;
	// ~IArchive

	using IArchive::operator();

private:
	XmlNodeRef m_pRootNode;
	int        m_childIndexOverride;
	int        m_childIndexHint;
};
}

#endif
