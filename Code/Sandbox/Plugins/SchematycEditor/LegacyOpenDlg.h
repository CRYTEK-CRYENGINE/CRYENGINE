// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <Controls/EditorDialog.h>

//////////////////////////////////////////////////////////////////////////
class CDictionaryWidget;

namespace Cry {
namespace SchematycEd {

class CLegacyOpenDlgModel;

} // namespace SchematycEd
} // namespace Cry

//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

class CLegacyOpenDlg : public CEditorDialog
{
public:
	CLegacyOpenDlg();
	virtual ~CLegacyOpenDlg();

	CDictionaryWidget* GetDialogDictWidget() const;

private:
	CLegacyOpenDlgModel* m_pDialogDict;
	CDictionaryWidget*   m_pDialogDictWidget;
};

} // namespace SchematycEd
} // namespace Cry
