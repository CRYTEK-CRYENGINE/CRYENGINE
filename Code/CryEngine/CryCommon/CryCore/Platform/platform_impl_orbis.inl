// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <common_dialog.h>
#include <message_dialog.h>
#include <libsysmodule.h>
#include <system_service.h>

void CryFindEngineRootFolder(unsigned int engineRootPathSize, char* szEngineRootPath)
{
	cry_strcpy(szEngineRootPath, engineRootPathSize, ".");
}

void CryGetExecutableFolder(unsigned int nBufferLength, char* lpBuffer)
{
	CryFindEngineRootFolder(nBufferLength, lpBuffer);
}

EQuestionResult CryMessageBoxImpl(const char* szText, const char* szCaption, EMessageBox type)
{
#if	!defined(_RELEASE)
	sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	sceCommonDialogInitialize();
	sceMsgDialogInitialize();
	SceMsgDialogParam params;
	sceMsgDialogParamInitialize(&params);
	params.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	SceMsgDialogUserMessageParam userMsgParam;
	memset(&userMsgParam, 0, sizeof(userMsgParam));
	userMsgParam.msg = szText != nullptr ? szText : " ";

	switch (type)
	{
	case eMB_Error:
	case eMB_Info:
		userMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
		break;
	case eMB_YesCancel:
		userMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL;
		break;
	case eMB_YesNoCancel:
	case eMB_AbortRetryIgnore:
		userMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL;
		break;
	default:
		userMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL;
	}

	params.userMsgParam = &userMsgParam;
	sceMsgDialogOpen(&params);
	while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
		;
	sceMsgDialogClose();
	SceMsgDialogResult ret;
	sceMsgDialogGetResult(&ret);

	switch (ret.buttonId)
	{
	case SCE_MSG_DIALOG_BUTTON_ID_OK:
		return eQR_Yes;
	case SCE_MSG_DIALOG_BUTTON_ID_NO:
	default:
		return eQR_No;
	}
#else
	return eQR_No;
#endif
}