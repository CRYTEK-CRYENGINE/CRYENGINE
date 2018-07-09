// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryMath/ISplines.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

//////////////////////////////////////////////////////////////////////////
inline string& operator<<(string& str, const char* s)
{
	str += s;
	return str;
}
inline string& operator<<(string& str, const string& s)
{
	str += s;
	return str;
}
inline string& operator<<(string& str, char c)
{
	str += c;
	return str;
}
inline string& operator<<(string& str, int i)
{
	char buffer[99];
	sprintf(buffer, "%d", i);
	str += buffer;
	return str;
}
inline string& operator<<(string& str, float f)
{
	char buffer[99];
	for (int digits = 7; digits < 10; digits++)
	{
		if (f >= powf(10.f, float(digits)))
			sprintf(buffer, "%.0f", f);
		else
			sprintf(buffer, "%.*g", digits, f);
		if (float(atof(buffer)) == f)
			break;
	}

	str += buffer;
	return str;
}

inline char* tokenize(char*& str, char token)
{
	char* s = str;
	char* e = s;
	for (; *e; e++)
	{
		if (*e == token)
		{
			*e++ = 0;
			break;
		}
	}
	str = e;
	return *s ? s : 0;
}

inline void remove_trailing(string& str, char c)
{
	auto size = str.size();
	while (size > 0 && str[size - 1] == c)
		size--;
	str.resize(size);
}

namespace spline
{

// Float4SplineKey implementation

void Float4SplineKey::ToString(string& str, Formatting format) const
{
	str << this->time << format.field;
	ToString(this->value, str, format);

	str << format.field;
	if (this->flags)
		str << (int)this->flags;

	// Save tangent value(s)
	str << format.field;
	if (this->flags.inTangentType == ETangentType::Custom)
		ToString(this->ds, str, format);
	str << format.field;
	if (this->flags.outTangentType == ETangentType::Custom)
		ToString(this->dd, str, format);

	remove_trailing(str, format.field);
}

bool Float4SplineKey::FromString(char* str, Formatting format)
{
	if (char* s = tokenize(str, format.field))
	{
		ZeroStruct(*this);
		this->time = (float)atof(s);

		if (FromString(this->value, str, format))
		{
			if (s = tokenize(str, format.field))
				this->flags = atoi(s);

			// TangentType = Custom implies tangent data follows; otherwise reset to Auto
			if (FromString(this->ds, str, format))
			{
				// if (this->flags.inTangentType == ETangentType::Auto)
				if (this->flags.inTangentType == ETangentType::Smooth)
					this->flags.inTangentType = ETangentType::Custom;
			}
			else if (this->flags.inTangentType == ETangentType::Custom)
				//	this->flags.inTangentType = ETangentType::Auto;
				this->flags.inTangentType = ETangentType::Smooth;

			if (FromString(this->dd, str, format))
			{
				// if (this->flags.outTangentType == ETangentType::Auto)
				if (this->flags.outTangentType == ETangentType::Smooth)
					this->flags.outTangentType = ETangentType::Custom;
			}
			else if (this->flags.outTangentType == ETangentType::Custom)
				//	this->flags.outTangentType = ETangentType::Auto;
				this->flags.outTangentType = ETangentType::Smooth;

			return true;
		}
		return false;
	}

	return false;
}

bool Float4SplineKey::ParseFromString(char*& str, Formatting format)
{
	if (char* skey = tokenize(str, format.key))
	{
		return FromString(skey, format);
	}
	return false;
}

void Float4SplineKey::ToString(const value_type& val, string& str, Formatting format)
{
	int non_zero_dim = DIM;
	while (non_zero_dim > 1 && val[non_zero_dim - 1] == 0.0f)
		non_zero_dim--;
	if (non_zero_dim > 1 && format.field == format.component)
		str << '(';
	for (int d = 0; d < non_zero_dim; d++)
	{
		if (d > 0)
			str << format.component;
		str << val[d];
	}
	if (non_zero_dim > 1 && format.field == format.component)
		str << ')';
}

bool Float4SplineKey::FromString(value_type& val, char*& str, Formatting format)
{
	char* s;
	if (*str == '(')
	{
		str++;
		s = tokenize(str, ')');
		if (*str == format.field)
			str++;
	}
	else
	{
		s = tokenize(str, format.field);
	}
	if (s)
	{
		for (int d = 0; d < DIM; ++d)
		{
			if (char* c = tokenize(s, format.component))
				val[d] = (float)atof(c);
			else
				break;
		}
		return true;
	}
	return false;
}

// Used for default ISplineEvaluator backup and string functions
struct ConversionSpline : CSplineKeyInterpolator<SplineKey<Vec4>>
{
	ConversionSpline(Formatting format)
		: m_format(format) {}

protected:
	Formatting m_format;
};

};

// ISplineEvaluator default implementations
ISplineBackup* ISplineEvaluator::Backup()
{
	spline::ConversionSpline sp(GetFormatting());
	ISplineBackup* pb = new spline::SSplineBackup<spline::ConversionSpline>(sp);
	pb->GetSpline()->FromSpline(*this);
	return pb;
}
void ISplineEvaluator::Restore(ISplineBackup* pb)
{
	if (pb)
		FromSpline(*pb->GetSpline());
}

ISplineEvaluator::Formatting ISplineEvaluator::GetFormatting() const
{
	// Default formatting separators
	return Formatting(",:/");
}

cstr ISplineEvaluator::ToString() const
{
	static string str;
	str = "";
	Formatting format = GetFormatting();
	KeyType key;
	for (int i = 0, n = GetKeyCount(); i < n; i++)
	{
		if (i > 0)
			str << format.key;
		GetKey(i, key);
		key.ToString(str, format);
	}
	return str.c_str();
}

bool ISplineEvaluator::FromString(cstr str)
{
	assert(GetNumDimensions() <= 4);
	spline::ConversionSpline sp(GetFormatting());
	if (!sp.FromString(str))
		return false;
	FromSpline(sp);
	return true;
}

bool ISplineEvaluator::Serialize(Serialization::IArchive& ar, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		return ar(*this, name, label);
	}
	else
	{
		string str;
		if (ar.isOutput())
			str = ToString();
		if (!ar(str, name, label))
			return false;
		if (ar.isInput())
			return FromString(str);
		return true;
	}
}

void ISplineEvaluator::SerializeSpline(XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		FromString(node->getAttr("Keys"));
	}
	else
	{
		node->setAttr("Keys", ToString());
	}
}
