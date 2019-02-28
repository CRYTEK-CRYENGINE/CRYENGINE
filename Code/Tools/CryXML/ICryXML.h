// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ICRYXML_H__
#define __ICRYXML_H__

class IXMLSerializer;

class ICryXML
{
public:
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual IXMLSerializer* GetXMLSerializer() = 0;
};

// Prototype for the function that is exported by the DLL - use this function to
// get a pointer to an ICryXML object. The function is exported by name as GetICryXML().
typedef ICryXML* (* FnGetICryXML)();

#endif //__ICRYXML_H__
