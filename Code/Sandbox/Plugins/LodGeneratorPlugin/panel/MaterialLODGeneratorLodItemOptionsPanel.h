#pragma once

#include <QWidget>
#include "../control/MeshBakerPopupPreview.h"
#include "../Control/MeshBakerUVPreview.h"

namespace Ui {
class CMaterialLODGeneratorLodItemOptionsPanel;
}

class CMaterialLODGeneratorLodItemOptionsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CMaterialLODGeneratorLodItemOptionsPanel(QWidget *parent = 0);
    ~CMaterialLODGeneratorLodItemOptionsPanel();

signals:
	void OnTextureSizeChanged(int nWidth, int nHeight);

public slots:
	void OnRender(const SRenderContext& rc);
	void OnKeyEvent(const SKeyEvent&);
	void OnMouseEvent(const SMouseEvent&);
	void SetIsBakingEnabled(int bEnable);
	void UpdateSizeControl(int value);


	void OnRenderUV(const SRenderContext& rc);
	void OnKeyEventUV(const SKeyEvent&);
	void OnMouseEventUV(const SMouseEvent&);


public:
	void SetParams(int nLodId, int nSubmatId, bool bAllowControl);
	void UpdateMesh();
	void UpdateMaterial();
	void UpdateUI();
	void UpdateLOD();
	void OnIdle();
	int LodId();
	int SubMatId();
	void Serialize(XmlNodeRef xml,bool load);
	bool IsBakingEnabled();
	int Width();
	int Height();
	void SetTextureSize(int nWidth, int nHeight);
	void UpdateUITextures();

private:
	void UpdateUITexture(ITexture* pTex, int type);
	void SetCameraLookAt( float fRadiusScale,const Vec3 &fromDir );
	void FitToScreen();

public:
	const static char * kPanelCaption;

private:
	CMeshBakerUVPreview* m_pMeshPreview;
	CMeshBakerUVPreview* m_pUVPreview;

	int m_nLodId;
	int m_nSubMaterialId;
	int m_bAllowControl;
	int m_bEnabled;

	CTopologyGraph * m_pMeshMap;

private:
    Ui::CMaterialLODGeneratorLodItemOptionsPanel *ui;
};

