// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "XMLWriter.h"
#include "StringHelpers.h"
#include <cstdarg>

XMLWriter::XMLWriter(IXMLSink* sink)
{
	m_indentationSize = -1;
	m_sink = sink;
	m_newLine = true;

	WriteText("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
}

void XMLWriter::BeginElement(const string& name)
{
	// Write the indentation.
	if (m_newLine)
	{
		for (int i = 0; i < m_indentationSize; ++i)
			WriteText("    ");
	}

	WriteText("<%s", name.c_str());
	m_newLine = false;
}

void XMLWriter::EndElement(const string& name)
{
	// Write the indentation.
	if (m_newLine)
	{
		for (int i = 0; i < m_indentationSize; ++i)
			WriteText("    ");
	}

	WriteText("</%s>\n", name.c_str());
	m_newLine = true;
}

void XMLWriter::CloseElement(const string& name, bool newLine)
{
	if (newLine)
		WriteText(">\n");
	else
		WriteText(">");
	m_newLine = newLine;
}

void XMLWriter::CloseLeafElement(const string& name)
{
	WriteText(" />\n");
	m_newLine = true;
}

void XMLWriter::IncreaseIndentation()
{
	++m_indentationSize;
}

void XMLWriter::DecreaseIndentation()
{
	--m_indentationSize;
}

void XMLWriter::WriteAttribute(const string& name, const string& value)
{
	WriteText(" %s=\"%s\"", name.c_str(), value.c_str());
}

void XMLWriter::SerializeAttribute(char* buffer, const string& value)
{
	// TODO: Escape string.
	strcpy(buffer, value.c_str());
}

void XMLWriter::SerializeAttribute(char* buffer, float value)
{
	sprintf(buffer, "%.10e", value);
	//sprintf(buffer, "%g", value);
}

void XMLWriter::SerializeAttribute(char* buffer, int value)
{
	sprintf(buffer, "%d", value);
}

void XMLWriter::SerializeArrayElement(char* buffer, float value)
{
	sprintf(buffer, "%.10e", value);
	//sprintf(buffer, "%g", value);
}

void XMLWriter::SerializeArrayElement(char* buffer, const string& value)
{
	strcpy(buffer, value.c_str());
}

void XMLWriter::SerializeArrayElement(char* buffer, int value)
{
	sprintf(buffer, "%d", value);
}

void XMLWriter::WriteContent(const string& text)
{
	WriteText("%s", text.c_str());
}

void XMLWriter::WriteContentLine(const string& text)
{
	// Write the indentation.
	if (m_newLine)
	{
		for (int i = 0; i < m_indentationSize; ++i)
			WriteText("    ");
	}

	WriteText("%s\n", text.c_str());
	m_newLine = true;
}

void XMLWriter::WriteText(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[40000];
	_vsnprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, format, args);
	m_sink->Write(buffer);
	va_end(args);
}

XMLWriter::Element::Element(XMLWriter& writer, const string& name, bool output)
: m_writer(writer), m_name(name), m_output(output), isParent(false)
{
	if (!m_writer.m_elements.empty())
	{
		Element* parent = m_writer.m_elements.back();
		if (!parent->isParent)
		{
			parent->isParent = true;
			if (parent->m_output)
				m_writer.CloseElement(m_name, true);
		}
	}
	m_writer.m_elements.push_back(this);
	if (m_output)
		m_writer.IncreaseIndentation();
	if (m_output)
		m_writer.BeginElement(m_name);
}

XMLWriter::Element::~Element()
{
	if (m_output)
	{
		if (isParent)
			m_writer.EndElement(m_name);
		else
			m_writer.CloseLeafElement(m_name);
	}
	m_writer.m_elements.pop_back();
	if (m_output)
		m_writer.DecreaseIndentation();
}

void XMLWriter::Element::Child(const string& name, const string& value)
{
	Element child(m_writer, name);
	child.Content(value);
}

void XMLWriter::Element::Content(const string& text)
{
	if (m_output)
	{
		assert(!isParent);
		if (!isParent)
		{
			isParent = true;
			m_writer.CloseElement(m_name, false);
		}
		m_writer.WriteContent(text);
	}
}

void XMLWriter::Element::ContentLine(const string& text)
{
	if (!isParent)
	{
		isParent = true;
		if (m_output)
			m_writer.CloseElement(m_name, true);
	}
	if (m_output)
		m_writer.WriteContentLine(text);
}

XMLFileSink::XMLFileSink(const string& filename)
{
	m_file = std::fopen(filename.c_str(), "w");
	if (!m_file)
		throw OpenFailedError("Unable to open file.");
}

XMLFileSink::~XMLFileSink()
{
	if (m_file)
		fclose(m_file);
}

void XMLFileSink::Write(const char* text)
{
	if (m_file)
	{
		string asciiText = StringHelpers::ConvertString<string>(text);
		fwrite(asciiText.c_str(), 1, asciiText.size(), m_file);
	}
}
