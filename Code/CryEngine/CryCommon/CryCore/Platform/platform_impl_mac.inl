// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

EQuestionResult CryMessageBoxImpl(const char* szText, const char* szCaption, EMessageBox type)
{
	CFStringRef strText = CFStringCreateWithCString(NULL, lpText, strlen(lpText));
	CFStringRef strCaption = CFStringCreateWithCString(NULL, lpCaption, strlen(lpCaption));

	CFOptionFlags kResult;
	CFUserNotificationDisplayAlert(
		0,                                 // no timeout
		kCFUserNotificationNoteAlertLevel, //change it depending message_type flags ( MB_ICONASTERISK.... etc.)
		NULL,                              //icon url, use default, you can change it depending message_type flags
		NULL,                              //not used
		NULL,                              //localization of strings
		strText,                           //header text
		strCaption,                        //message text
		NULL,                              //default "ok" text in button
		CFSTR("Cancel"),                   //alternate button title
		NULL,                              //other button title, null--> no other button
		&kResult                           //response flags
	);

	CFRelease(strCaption);
	CFRelease(strText);

	if (kResult == kCFUserNotificationDefaultResponse)
		return eQR_Yes;
	else
		return eQR_Cancel;
}