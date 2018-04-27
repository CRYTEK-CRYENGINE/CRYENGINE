#pragma once

#include <QWidget>
#include "../Util/LodParameter.h"
#include "GeneratorOptionsPanel.h"

namespace Ui {
class CMaterialLODGeneratorOptionsPanel;
}

class CMaterialLODGeneratorOptionsPanel : public CGeneratorOptionsPanel
{
    Q_OBJECT

public:
    explicit CMaterialLODGeneratorOptionsPanel(QWidget *parent = 0);
    ~CMaterialLODGeneratorOptionsPanel();

public:
	void ParameterChanged(SLodParameter* parameter);
	void Update();
	void Reset();

private:
	void LoadPropertiesTree();

private:
    Ui::CMaterialLODGeneratorOptionsPanel *ui;

	SLodParameterGroupSet m_groups;
};

