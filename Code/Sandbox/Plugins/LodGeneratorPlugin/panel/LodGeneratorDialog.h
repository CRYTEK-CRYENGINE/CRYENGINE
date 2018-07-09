#pragma once

#include "EditorFramework/Editor.h"
#include "IEditor.h"

namespace Ui {
class CLodGeneratorDialog;
}
struct IStatObj;
struct IMaterial;

class CLodGeneratorDialog : public CDockableEditor, public IEditorNotifyListener
{
    Q_OBJECT

public:
    explicit CLodGeneratorDialog(QWidget *parent = 0);
    ~CLodGeneratorDialog();

	void OnEditorNotifyEvent( EEditorNotifyEvent event ) override;

signals:
	void UpdateObj_Signal(IStatObj * pObj);
	void UpdateMaterial_Signal(IMaterial* pMaterial);	

public slots:
	bool FileOpened();
	bool OnMaterialChange(const QString& materialPath);
	void OnTextureSizeChanged(int nWidth, int nHeight);
	bool OnGenerateMaterial();
	void OnMaterialGeneratePrepare();

private:
	void Reset(bool value);
	void ClearLodPanels();
	void GenerateLodPanels();
	bool OnSave();
	bool LoadMaterialHelper(const QString& materialPath);

	virtual const char* GetEditorName() const override;

private:
    Ui::CLodGeneratorDialog *ui;
};

