#pragma once

#include <QWidget>
// #include "../OGF/OGFTool/TextureAtlasGenerator.h"
// #include "../Control/MeshBakerUVPreview.h"

namespace Ui {
class cABFPannel;
}

class cABFPannel : public QWidget
{
    Q_OBJECT

public:
    explicit cABFPannel(QWidget *parent = 0);
    ~cABFPannel();

public slots:
	void TestABF_Slot();
	void UpdateObj(IStatObj * pObj);

// 	void OnRender(const SRenderContext& rc);
// 	void OnKeyEvent(const SKeyEvent&);
// 	void OnMouseEvent(const SMouseEvent&);

// 	void OnRenderUV(const SRenderContext& rc);
// 	void OnKeyEventUV(const SKeyEvent&);
// 	void OnMouseEventUV(const SMouseEvent&);

public:
	void OnIdle();

private:
    Ui::cABFPannel *ui;
//	LODGenerator::TextureAtlasGenerator* textureAtlasGenerator;

// 	CMeshBakerUVPreview* m_pUVPreview;
// 	CMeshBakerUVPreview* m_pMeshPreview;
};

