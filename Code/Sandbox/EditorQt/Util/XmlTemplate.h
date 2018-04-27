// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XmlTemplate_h__
#define __XmlTemplate_h__

#if _MSC_VER > 1000
	#pragma once
#endif

/*!
 *	CXmlTemplate is XML base template of parameters.
 *
 */
class CXmlTemplate
{
public:
	//! Scans properties of XML template,
	//! for each property try to find corresponding attribute in specified XML node, and copy
	//! value to Value attribute of template.
	static void GetValues(XmlNodeRef& templateNode, const XmlNodeRef& fromNode);

	//! Scans properties of XML template, fetch Value attribute of each and put as Attribute in
	//! specified XML node.
	static void SetValues(const XmlNodeRef& templateNode, XmlNodeRef& toNode);
	static bool SetValues(const XmlNodeRef& templateNode, XmlNodeRef& toNode, const XmlNodeRef& modifiedNode);

	//! Add parameter to template.
	static void AddParam(XmlNodeRef& templ, const char* paramName, bool value);
	static void AddParam(XmlNodeRef& templ, const char* paramName, int value, int min = 0, int max = 10000);
	static void AddParam(XmlNodeRef& templ, const char* paramName, float value, float min = -10000, float max = 10000);
	static void AddParam(XmlNodeRef& templ, const char* paramName, const char* sValue);
};

/*!
 *	CXmlTemplateRegistry is a collection of all registred templates.
 */
class CXmlTemplateRegistry
{
public:
	CXmlTemplateRegistry();

	void       LoadTemplates(const string& path);
	void       AddTemplate(const string& name, XmlNodeRef& tmpl);

	XmlNodeRef FindTemplate(const string& name);

private:
	std::map<string, XmlNodeRef> m_templates;
};

#endif // __XmlTemplate_h__

