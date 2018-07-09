// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __REPORT_H__
#define __REPORT_H__

class IReportField
{
public:
	virtual ~IReportField() {}

	virtual const char* GetDescription() const = 0;
	virtual const char* GetText() const = 0;
};

template<typename T, typename G> class CReportField : public IReportField
{
public:
	typedef T Object;
	typedef G TextGetter;

	CReportField(Object& object, const char* description, TextGetter& getter);
	virtual const char* GetDescription() const;
	virtual const char* GetText() const;

private:
	TextGetter m_getter;
	string     m_text;
	string     m_description;
};

class IReportRecord
{
public:
	virtual ~IReportRecord() {}
	virtual int         GetFieldCount() const = 0;
	virtual const char* GetFieldDescription(int fieldIndex) const = 0;
	virtual const char* GetFieldText(int fieldIndex) const = 0;
};

template<typename T> class CReportRecord : public IReportRecord
{
public:
	typedef T Object;

	CReportRecord(Object& object);
	virtual ~CReportRecord();
	virtual int                                   GetFieldCount() const;
	virtual const char*                           GetFieldDescription(int fieldIndex) const;
	virtual const char*                           GetFieldText(int fieldIndex) const;
	template<typename G> CReportField<Object, G>* AddField(const char* description, G& getter);

private:
	Object         m_object;
	typedef std::vector<IReportField*> FieldContainer;
	FieldContainer m_fields;
};

class CReport
{
public:
	~CReport();
	template<typename T> CReportRecord<T>* AddRecord(T& object);
	int                                    GetRecordCount() const;
	IReportRecord*                         GetRecord(int recordIndex);
	void                                   Clear();

private:
	typedef std::vector<IReportRecord*> RecordContainer;
	RecordContainer m_records;
};

template<typename T, typename G> inline CReportField<T, G>::CReportField(Object& object, const char* description, TextGetter& getter)
	: m_getter(getter),
	m_description(description)
{
	m_text = m_getter(object);
}

template<typename T, typename G> inline const char* CReportField<T, G >::GetDescription() const
{
	return m_description.c_str();
}

template<typename T, typename G> inline const char* CReportField<T, G >::GetText() const
{
	return m_text.c_str();
}

template<typename T> inline CReportRecord<T>::CReportRecord(Object& object)
	: m_object(object)
{
}

template<typename T> inline CReportRecord<T>::~CReportRecord()
{
	for (FieldContainer::iterator it = m_fields.begin(); it != m_fields.end(); ++it)
		delete (*it);
}

template<typename T> inline int CReportRecord<T >::GetFieldCount() const
{
	return m_fields.size();
}

template<typename T> inline const char* CReportRecord<T >::GetFieldDescription(int fieldIndex) const
{
	return m_fields[fieldIndex]->GetDescription();
}

template<typename T> inline const char* CReportRecord<T >::GetFieldText(int fieldIndex) const
{
	return m_fields[fieldIndex]->GetText();
}

template<typename T> template<typename G> inline CReportField<T, G>* CReportRecord<T >::AddField(const char* description, G& getter)
{
	CReportField<Object, G>* field = new CReportField<Object, G>(m_object, description, getter);
	m_fields.push_back(field);
	return field;
}

inline CReport::~CReport()
{
	Clear();
}

template<typename T> inline CReportRecord<T>* CReport::AddRecord(T& object)
{
	CReportRecord<T>* record = new CReportRecord<T>(object);
	m_records.push_back(record);
	return record;
}

inline int CReport::GetRecordCount() const
{
	return m_records.size();
}

inline IReportRecord* CReport::GetRecord(int recordIndex)
{
	return m_records[recordIndex];
}

inline void CReport::Clear()
{
	for (RecordContainer::iterator it = m_records.begin(); it != m_records.end(); ++it)
		delete (*it);
	m_records.clear();
}

#endif //__REPORT_H__

