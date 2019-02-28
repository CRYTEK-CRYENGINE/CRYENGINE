// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EditorCommonAPI.h"
#include <CrySerialization/Serializer.h>
#include "Serialization/Qt.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <QSplitter>
#include <QString>
#include <QTreeView>
#include <QHeaderView>

class StringQt : public Serialization::IWString
{
public:
	StringQt(QString& str) : str_(str) {}

	void                  set(const wchar_t* value) { str_.setUnicode((const QChar*)value, (int)wcslen(value)); }
	const wchar_t*        get() const               { return (wchar_t*)str_.data(); }
	const void*           handle() const            { return &str_; }
	Serialization::TypeID type() const              { return Serialization::TypeID::get<QString>(); }
private:
	QString& str_;
};

bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QString& value, const char* name, const char* label)
{
	StringQt str(value);
	return ar(static_cast<Serialization::IWString&>(str), name, label);
}

bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QByteArray& byteArray, const char* name, const char* label)
{
	std::vector<unsigned char> temp(byteArray.begin(), byteArray.end());
	if (!ar(temp, name, label))
		return false;
	if (ar.isInput())
		byteArray = QByteArray(temp.empty() ? (char*)0 : (char*)&temp[0], (int)temp.size());
	return true;
}

struct QColorSerializable
{
	QColor& color;
	QColorSerializable(QColor& color) : color(color) {}

	void Serialize(Serialization::IArchive& ar)
	{
		// this is not comprehensive, as QColor can store color components
		// in diffrent models, depending on the way they were specified
		unsigned char r = color.red();
		unsigned char g = color.green();
		unsigned char b = color.blue();
		unsigned char a = color.alpha();
		ar(r, "r", "^R");
		ar(g, "g", "^G");
		ar(b, "b", "^B");
		ar(a, "a", "^A");
		if (ar.isInput())
		{
			color.setRed(r);
			color.setGreen(g);
			color.setBlue(b);
			color.setAlpha(a);
		}
	}
};

bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QColor& color, const char* name, const char* label)
{
	QColorSerializable serializer(color);
	return ar(serializer, name, label);
}
