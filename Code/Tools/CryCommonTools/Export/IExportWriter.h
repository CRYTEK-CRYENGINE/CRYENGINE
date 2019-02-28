// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IEXPORTWRITER_H__
#define __IEXPORTWRITER_H__

class IExportSource;
class IExportContext;

class IExportWriter
{
public:
	virtual void Export(IExportSource* source, IExportContext* context) = 0;
};

#endif //__IEXPORTWRITER_H__
