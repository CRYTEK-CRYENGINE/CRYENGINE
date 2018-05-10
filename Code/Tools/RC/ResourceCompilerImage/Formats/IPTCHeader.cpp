// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "stdafx.h"
#include "IPTCHeader.h"

void CIPTCHeader::Parse(const unsigned char* buffer, int length)
{
	m_fields.clear();

	int position = 0;
	while (position + 5 < length)
	{
		if (buffer[position++] != 0x1C)
		{
			return;
		}
		char c = buffer[position++];
		int type = buffer[position++];
		int fieldLength = (buffer[position++] << 8);
		fieldLength += buffer[position++];
		if (c == 0x01)
		{
			position += fieldLength;
			continue;
		}
		if (c == 0x02)
		{
			FieldContainer::iterator itField = m_fields.insert(std::make_pair(type, std::vector<unsigned char>()));
			std::vector<unsigned char>& data = (*itField).second;
			data.resize(fieldLength);
			memcpy(&data[0], &buffer[position], fieldLength);
			position += fieldLength;
			continue;
		}
		return;
	}
}

void CIPTCHeader::GetCombinedFields(FieldType field, std::vector<unsigned char>& buffer, const string& fieldSeparator) const
{
	buffer.clear();

	std::pair<FieldContainer::const_iterator, FieldContainer::const_iterator> range = m_fields.equal_range(field);
	for (FieldContainer::const_iterator itField = range.first; itField != range.second; ++itField)
	{
		const std::vector<unsigned char>& fieldData = (*itField).second;
		if (!buffer.empty())
		{
			buffer.insert(buffer.end(), fieldSeparator.begin(), fieldSeparator.end());
		}
		buffer.insert(buffer.end(), fieldData.begin(), fieldData.end());
	}

	buffer.push_back('\0');
}

void CIPTCHeader::GetHeader(std::vector<unsigned char>& buffer) const
{
	buffer.clear();
	for (FieldContainer::const_iterator itField = m_fields.begin(); itField != m_fields.end(); ++itField)
	{
		int type = (*itField).first;
		const std::vector<unsigned char>& fieldData = (*itField).second;
		buffer.push_back('\x1C');
		buffer.push_back('\x02');
		buffer.push_back(type);
		buffer.push_back((fieldData.size() >> 8) & 0xFF);
		buffer.push_back(fieldData.size() & 0xFF);
		buffer.insert(buffer.end(), fieldData.begin(), fieldData.end());
	}
}
