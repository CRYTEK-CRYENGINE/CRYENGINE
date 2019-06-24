#pragma once

#include <QWidget>
#include "../control/MeshBakerPopupPreview.h"
class CCRampControl;

namespace Ui {
class CGeometryLodGeneratorPreviewPanel;
}

class CGeometryLodGeneratorPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CGeometryLodGeneratorPreviewPanel(QWidget *parent = 0);
    ~CGeometryLodGeneratorPreviewPanel();

signals:
	void OnMaterialGeneratePrepare_Signal();

public slots:
	void OnLodChainGenerationFinished();

	void OnLodValueChanged();
	void OnLodDeleted();
	void OnLodAdded(float fPercentage);
	void OnLodSelected();
	void OnSelectionCleared();

	void OnLodRemoved(int iLod);


	void OnRender(const SRenderContext& rc);
	void OnKeyEvent(const SKeyEvent&);
	void OnMouseEvent(const SMouseEvent&);

	void UpdateObj(IStatObj * pObj);
	void UpdateMaterial(IMaterial* pMaterial);

	void OnGenerateLods(bool checked = false);

public:
	void Reset(bool release = true);
	void CreateExistingLodKeys();
	void OnIdle();

private:	
	void CreateLod(float fPercentage);
	void SelectFirst();
	void EnableExport();

private:
    Ui::CGeometryLodGeneratorPreviewPanel *ui;
	CCMeshBakerPopupPreview* m_pPreview;
};

