// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_DEBUG_OPTIONS_DIALOG_H__
#define __MANN_DEBUG_OPTIONS_DIALOG_H__
#pragma once

#include "MannequinBase.h"

class CMannequinModelViewport;
class CPropertiesPanel;

class CMannequinDebugOptionsDialog : public CXTResizeDialog
{
public:
	CMannequinDebugOptionsDialog(CMannequinModelViewport* pModelViewport, CWnd* pParent);

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();

	std::auto_ptr<CPropertiesPanel> m_pPanel;
	CMannequinModelViewport*        m_pModelViewport;
};

#endif

