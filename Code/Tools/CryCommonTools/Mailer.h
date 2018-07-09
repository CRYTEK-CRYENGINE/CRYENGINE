// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Mailer_h__
#define __Mailer_h__

#if _MSC_VER > 1000
#pragma once
#endif

class CMailer
{
public:
	static bool SendMail(   const char* _subject,             // E-Mail Subject
						              const char* _messageBody,         // Message Text
													const std::vector<const char*> &_recipients,  // All Recipients' Addresses
						              const std::vector<const char*> &_attachments, // All File Attachments
						              bool bShowDialog);   // Whether to allow editing by user
};

#endif // __Mailer_h__
