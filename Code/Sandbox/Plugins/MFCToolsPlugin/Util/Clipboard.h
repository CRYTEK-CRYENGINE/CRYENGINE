// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/** Use this class to put and get stuff from windows clipboard.
 */
class PLUGIN_API CClipboard
{
public:
	//! Put xml node into clipboard
	void       Put(XmlNodeRef& node, const string& title = "");
	//! Get xml node to clipboard.
	XmlNodeRef Get() const;

	//! Put string into Windows clipboard.
	void    PutString(const string& text, const string& title = "");
	//! Get string from Windows clipboard.
	string GetString() const;

	//! Return name of what is in clipboard now.
	string GetTitle() const;

	//! Put image into Windows clipboard.
	void PutImage(const CImageEx& img);

	//! Get image from Windows clipboard.
	bool GetImage(CImageEx& img);

	//! Return true if clipboard is empty.
	bool IsEmpty() const;
private:
	static XmlNodeRef m_node;
	static string    m_title;
};


