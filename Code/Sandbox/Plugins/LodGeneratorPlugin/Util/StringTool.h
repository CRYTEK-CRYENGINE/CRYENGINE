#pragma once

class CStringTool
{
public:
	static string QStringToCString(QString str);
	static QString CStringToQString(string str);
};