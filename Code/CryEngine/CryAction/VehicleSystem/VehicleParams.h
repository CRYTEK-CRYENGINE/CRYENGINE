// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Vehicle_Params__h__
#define __Vehicle_Params__h__

#include "VehicleModificationParams.h"

class CVehicleParams
{
public:
	CVehicleParams(XmlNodeRef root, const CVehicleModificationParams& modificationParams)
		: m_xmlNode(root)
		, m_modificationParams(modificationParams)
	{
	}

	virtual ~CVehicleParams() {}

	const char* getTag() const
	{
		assert(IsValid());

		return m_xmlNode->getTag();
	}

	bool haveAttr(const char* name) const
	{
		assert(IsValid());

		return m_xmlNode->haveAttr(name);
	}

	const char* getAttr(const char* name) const
	{
		assert(IsValid());

		const char* attributeValue = m_xmlNode->getAttr(name);
		const char** attributeValueAddress = &attributeValue;
		ApplyModification(name, attributeValueAddress);

		return attributeValue;
	}

	bool getAttr(const char* name, const char** valueOut) const
	{
		return GetAttrImpl(name, valueOut);
	}

	bool getAttr(const char* name, int& valueOut) const
	{
		return GetAttrImpl(name, valueOut);
	}

	bool getAttr(const char* name, float& valueOut) const
	{
		return GetAttrImpl(name, valueOut);
	}

	bool getAttr(const char* name, bool& valueOut) const
	{
		return GetAttrImpl(name, valueOut);
	}

	bool getAttr(const char* name, Vec3& valueOut) const
	{
		return GetAttrImpl(name, valueOut);
	}

	int getChildCount() const
	{
		assert(IsValid());

		return m_xmlNode->getChildCount();
	}

	CVehicleParams getChild(int i) const
	{
		assert(IsValid());

		XmlNodeRef childNode = m_xmlNode->getChild(i);
		return CVehicleParams(childNode, m_modificationParams);
	}

	CVehicleParams findChild(const char* id) const
	{
		assert(IsValid());

		XmlNodeRef childNode = m_xmlNode->findChild(id);
		return CVehicleParams(childNode, m_modificationParams);
	}

	operator bool() const { return m_xmlNode != NULL; }

	bool IsValid() const { return m_xmlNode != NULL; }

private:
	template<typename T>
	bool GetAttrImpl(const char* name, T& valueOut) const
	{
		assert(IsValid());

		bool attributeGetSuccess = m_xmlNode->getAttr(name, valueOut);
		ApplyModification(name, valueOut);

		return attributeGetSuccess;
	}

	template<typename T>
	void ApplyModification(const char* name, T& valueOut) const
	{
		assert(IsValid());

		const char* id;
		bool hasId = m_xmlNode->getAttr("id", &id);
		if (hasId)
		{
			m_modificationParams.ApplyModification(id, name, valueOut);
		}
	}

private:
	XmlNodeRef                        m_xmlNode;
	const CVehicleModificationParams& m_modificationParams;
};

#endif
