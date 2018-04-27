// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

class QLabel;
class QCheckBox;

class QXMLExporterDlg : public CEditorDialog
{
public:
	QXMLExporterDlg();

	bool GetResult(bool& bExportAnimation, bool& bExportAudio) const;

protected:
	void OnConfirmed();

	QLabel*    m_pLabel;
	QCheckBox* m_pExportAnimation;
	QCheckBox* m_pExportAudio;

	bool       m_bConfirmed;
};

