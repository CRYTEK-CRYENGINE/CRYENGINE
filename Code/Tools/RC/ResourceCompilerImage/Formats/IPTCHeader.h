// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#ifndef __IPTCHEADER_H__
#define __IPTCHEADER_H__

class CIPTCHeader
{
public:
	enum FieldType
	{
		FieldByline                         = 0x50,
		FieldBylineTitle                    = 0x55,
		FieldCredits                        = 0x6E,
		FieldSource                         = 0x73,
		FieldObjectName                     = 0x05,
		FieldDateCreated                    = 0x37,
		FieldCity                           = 0x5A,
		FieldState                          = 0x5F,
		FieldCountry                        = 0x65,
		FieldOriginalTransmissionReference  = 0x67,
		FieldCopyrightNotice                = 0x74,
		FieldCaption                        = 0x78,
		FieldCaptionWriter                  = 0x7A,
		FieldHeadline                       = 0x69,
		FieldSpecialInstructions            = 0x28,
		FieldCategory                       = 0x0F,
		FieldSupplementalCategories         = 0x14,
		FieldKeywords                       = 0x19
	};

	void Parse(const unsigned char* buffer, int length);
	void GetCombinedFields(FieldType field, std::vector<unsigned char>& buffer, const string& fieldSeparator) const;
	void GetHeader(std::vector<unsigned char>& buffer) const;

private:
	typedef std::multimap<int, std::vector<unsigned char> > FieldContainer;
	FieldContainer m_fields;
};

#endif //__IPTCHEADER_H__
