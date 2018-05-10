// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __XMLCONVERTER_H__
#define __XMLCONVERTER_H__

#include "IConverter.h"
#include "ICryXML.h"
#include "NameConverter.h"
#include <CrySystem/XML/XMLBinaryHeaders.h>

struct XMLFilterElement
{
	XMLBinary::IFilter::EType type;
	bool bAccept;
	string wildcards;
};

class XMLCompiler : public ICompiler
{
public:
	XMLCompiler(
		ICryXML* pCryXML,
		const std::vector<XMLFilterElement>& filter,
		const std::vector<string>& tableFilemasks,
		const NameConverter& nameConverter);

public:
	virtual void Release();

	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

private:
	int m_refCount;
	ICryXML* m_pCryXML;
	const std::vector<XMLFilterElement>* m_pFilter;
	const std::vector<string>* m_pTableFilemasks;
	const NameConverter* m_pNameConverter;
	ConvertContext m_CC;
};

class XMLConverter : public IConverter
{
public:
	explicit XMLConverter(ICryXML* pCryXML);
	~XMLConverter();

	virtual void Release();

	virtual void Init(const ConverterInitContext& context);
	virtual ICompiler* CreateCompiler();
	virtual bool SupportsMultithreading() const;
	virtual const char* GetExt(int index) const;

private:
	int m_refCount;
	ICryXML* m_pCryXML;
	std::vector<XMLFilterElement> m_filter;
	std::vector<string> m_tableFilemasks;
	NameConverter m_nameConverter;
};

#endif //__XMLCONVERTER_H__
