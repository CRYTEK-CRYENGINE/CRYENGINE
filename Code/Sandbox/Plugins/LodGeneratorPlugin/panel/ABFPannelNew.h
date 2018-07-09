#pragma once

#include <QWidget>
#include "../Control/MeshBakerUVPreview.h"
#include "../Util/AtlasGenerator.h"
#include "../Util/Decimator.h"

namespace Ui {
class cABFPannelNew;
}

class cABFPannelNew : public QWidget
{
    Q_OBJECT

public:
    explicit cABFPannelNew(QWidget *parent = 0);
    ~cABFPannelNew();

public slots:
	void TestABF_Slot();
	void TestDecimator_Slot();
	void UpdateObj(IStatObj * pObj);

	void OnRender(const SRenderContext& rc);
	void OnKeyEvent(const SKeyEvent&);
	void OnMouseEvent(const SMouseEvent&);

	void OnRenderUV(const SRenderContext& rc);
	void OnKeyEventUV(const SKeyEvent&);
	void OnMouseEventUV(const SMouseEvent&);

public:
	void OnIdle();

private:
    Ui::cABFPannelNew *ui;
	LODGenerator::CAtlasGenerator* atlasgenerator_;
	LODGenerator::CDecimator* decimator;

	CMeshBakerUVPreview* m_pUVPreview;
	CMeshBakerUVPreview* m_pMeshPreview;
};
