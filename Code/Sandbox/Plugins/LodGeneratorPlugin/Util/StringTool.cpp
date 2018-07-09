#include "StdAfx.h"
#include "StringTool.h"

string CStringTool::QStringToCString(QString str)
{
	string strTmp = str.toLocal8Bit().data();  
	string cstrTmp = strTmp.c_str(); 
	return cstrTmp;
}

QString CStringTool::CStringToQString(string str)
{
	QString qstrTemp;
#ifdef _UNICODE 
	qstrTemp = QString::fromWCharArray((LPCTSTR)str, str.GetLength());
#else
	qstrTemp = QString((LPCTSTR)str);
#endif

	return qstrTemp;
}