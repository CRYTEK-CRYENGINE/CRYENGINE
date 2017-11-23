// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEditor.h>
#include <AssetSystem/AssetEditor.h>

class QPushButton;
class CAbstractDictionaryEntry;

namespace Cry { 
namespace SchematycEd { 

	class CGenericWidget;
	class CLegacyOpenDlg;

} // namespace SchematycEd
} // namespace Cry

namespace Cry {
namespace SchematycEd {

class CMainWindow : public CAssetEditor, public IEditorNotifyListener
{
	Q_OBJECT

public:
	CMainWindow();
	virtual ~CMainWindow();

public:
	// CEditor
	virtual const char* GetEditorName() const override                               { return "Schematyc Editor"; };
	virtual void        CreateDefaultLayout(CDockableContainer* pSender) override;
	// ~CEditor

protected:
	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override              { return;       }
	// ~IEditorNotifyListener

	// CAssetEditor
	virtual bool OnOpenAsset(CAsset* pAsset) override                                { return false; }
	virtual bool OnSaveAsset(CEditableAsset& editAsset) override                     { return false; }
	virtual void OnCloseAsset()  override                                            { return;       }
	// ~CAssetEditor

protected:
	virtual void OnOpenLegacy();
	virtual void OnOpenLegacyClass(CAbstractDictionaryEntry& classEntry);

private:
	void InitLegacyOpenDlg();
	void FinishLegacyOpenDlg();
	void InitGenericWidgets();
	void FinishGenericWidgets();

	QWidget* CreateWidgetTemplateFunc(const char* name);	

private:		
	CLegacyOpenDlg*              m_pLegacyOpenDlg;	
	QPushButton*                 m_pAddComponentButton;
	std::vector<CGenericWidget*> m_genericWidgets;
};

} // namespace SchematycEd
} // namespace Cry
