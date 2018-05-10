// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XMLWRITER_H__
#define __XMLWRITER_H__

#include "Exceptions.h"

#include <cstdio>
#include <vector>
#include <cassert>

class IXMLSink
{
public:
	// Define an exception type to throw when file opening fails.
	struct OpenFailedErrorTag {};
	typedef Exception<OpenFailedErrorTag> OpenFailedError;

	virtual void Write(const char* text) = 0;
};

class XMLFileSink : public IXMLSink
{
public:
	explicit XMLFileSink(const string& name);
	~XMLFileSink();

	virtual void Write(const char* text);

private:
	FILE* m_file;
};

class XMLWriter
{
public:
	explicit XMLWriter(IXMLSink* sink);

	class Element
	{
	public:
		Element(XMLWriter& writer, const string& name, bool output=true);
		~Element();

		template <typename T> void Attribute(const string& name, const T& value);
		void Child(const string& name, const string& value);
		void Content(const string& text);
		void ContentLine(const string& text);
		template <typename T> void ContentArrayElement(const T& value);
		void ContentArrayFloat24(const float floatBuffer[24], const int entryCount );

		void WriteDirectText(const char* text)
		{
			if (!isParent)
			{
				isParent = true;
				if (m_output)
					m_writer.CloseElement(m_name, false);
			}
			m_writer.WriteDirectText(text);
		}

	private:
		XMLWriter& m_writer;
		string m_name;
		bool isParent;
		bool m_output;
	};

	void WriteDirectText(const char* text)
	{
		m_sink->Write(text);
	}

private:
	void IncreaseIndentation();
	void DecreaseIndentation();

	void BeginElement(const string& name);
	void EndElement(const string& name);
	void CloseElement(const string& name, bool newLine);
	void CloseLeafElement(const string& name);

	void WriteAttribute(const string& name, const string& value);
	static void SerializeAttribute(char* buffer, const string& value);
	static void SerializeAttribute(char* buffer, float value);
	static void SerializeAttribute(char* buffer, int value);
	static void SerializeArrayElement(char* buffer, float value);
	static void SerializeArrayElement(char* buffer, const string& value);
	static void SerializeArrayElement(char* buffer, int value);
	void WriteContent(const string& text);
	void WriteContentLine(const string& text);

	void WriteText(const char* format, ...);

	IXMLSink* m_sink;
	int m_indentationSize;

	std::vector<Element*> m_elements;
	bool m_newLine;
};

template <typename T> void XMLWriter::Element::Attribute(const string& name, const T& value)
{
	assert(!isParent);
	char buffer[1024];
	XMLWriter::SerializeAttribute(buffer, value);
	if (m_output)
		m_writer.WriteAttribute(name, buffer);
}

template <typename T> void XMLWriter::Element::ContentArrayElement(const T& value)
{
	if (!m_output)
		return;

	char buffer[1024];
	buffer[0] = ' ';
	char *bufferPtr = &buffer[1];
	if (!isParent)
	{
		isParent = true;
		m_writer.CloseElement(m_name, false);
	}

	XMLWriter::SerializeArrayElement(bufferPtr, value);
	m_writer.WriteDirectText(buffer);
}

inline void XMLWriter::Element::ContentArrayFloat24(const float floatBuffer[24], const int entryCount )
{
	if(!m_output )
		return;
	if (!isParent)
	{
		isParent = true;
		m_writer.CloseElement(m_name, false);
	}

	if( entryCount == 24 )
	{
		char buffer[2048];
		sprintf(buffer," %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e %.10e",
				floatBuffer[0],floatBuffer[1],floatBuffer[2],floatBuffer[3],floatBuffer[4],floatBuffer[5],floatBuffer[6],floatBuffer[7],
				floatBuffer[8],floatBuffer[9],floatBuffer[10],floatBuffer[11],floatBuffer[12],floatBuffer[13],floatBuffer[14],floatBuffer[15],
				floatBuffer[16],floatBuffer[17],floatBuffer[18],floatBuffer[19],floatBuffer[20],floatBuffer[21],floatBuffer[22],floatBuffer[23]);
		m_writer.WriteDirectText(buffer);
	}
	else
	{
		for( int i = 0;i<entryCount;i++ )
		{
			char buffer[1024];
			sprintf(buffer," %.10e",floatBuffer[i]);
			m_writer.WriteDirectText(buffer);
		}
	}
}

#endif //__XMLWRITER_H__
