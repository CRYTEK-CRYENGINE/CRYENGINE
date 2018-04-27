// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QString>
#include <QColor>

// Note: This is just a temporary solution for now!

class CDataTypeItem
{
public:
	static const CDataTypeItem& Empty();

	CDataTypeItem(QString name, const QColor& color, const CryGUID& guid);
	virtual ~CDataTypeItem();

	const QString&          GetName() const  { return m_name; }
	const QColor&           GetColor() const { return m_color; }

	const CryGUID& GetGUID() const  { return m_guid; }

	bool                    operator==(const CDataTypeItem& other) const;
	bool                    operator!=(const CDataTypeItem& other) const;

protected:
	QString          m_name;
	QColor           m_color;

	CryGUID m_guid;
};

inline bool CDataTypeItem::operator==(const CDataTypeItem& other) const
{
	if (this != &other || m_guid != other.m_guid)
	{
		return false;
	}
	return true;
}

inline bool CDataTypeItem::operator!=(const CDataTypeItem& other) const
{
	return !(*this == other);
}

class CDataTypesModel
{
	typedef std::unordered_map<CryGUID, CDataTypeItem*> TypesByGuid;
	typedef std::vector<CDataTypeItem*>                          TypesByIndex;

public:
	static CDataTypesModel& GetInstance();

	uint32                  GetTypeItemsCount() const { return m_typesByIndex.size(); }
	CDataTypeItem*          GetTypeItemByIndex(uint32 index) const;
	CDataTypeItem*          GetTypeItemByGuid(const CryGUID& guid) const;

private:
	CDataTypesModel();
	~CDataTypesModel();

	void GenerateTypeInfo();

private:
	TypesByGuid  m_typesByGuid;
	TypesByIndex m_typesByIndex;
};

