// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __UIEnumsDatabase_h__
#define __UIEnumsDatabase_h__
#pragma once

struct EDITOR_COMMON_API CUIEnumsDatabase_SEnum
{
	string              name;
	std::vector<string> strings; // Display Strings.
	std::vector<string> values;  // Corresponding Values.

	const string& NameToValue(const string& name);
	const string& ValueToName(const string& value);
	const char* NameToValue(const char* name) { return NameToValue(string(name)); } // for CString conversion
	const char* ValueToName(const char* value) { return ValueToName(string(value)); } // for CString conversion
};

//////////////////////////////////////////////////////////////////////////
// Stores string associates to the enumeration collections for UI.
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CUIEnumsDatabase
{
public:
	CUIEnumsDatabase();
	~CUIEnumsDatabase();

	void                    SetEnumStrings(const string& enumName, const std::vector<string>& sStringsArray);
	CUIEnumsDatabase_SEnum* FindEnum(const string& enumName) const;

private:
	typedef std::map<string, CUIEnumsDatabase_SEnum*> Enums;
	Enums m_enums;
};

#endif // __UIEnumsDatabase_h__

