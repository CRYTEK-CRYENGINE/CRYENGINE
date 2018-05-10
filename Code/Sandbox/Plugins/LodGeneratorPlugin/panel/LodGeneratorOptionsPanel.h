#pragma once

#include <QWidget>
#include "../Util/LodParameter.h"
#include "GeneratorOptionsPanel.h"

namespace Ui 
{
	class CLodGeneratorOptionsPanel;
}

class CLodGeneratorOptionsPanel : public CGeneratorOptionsPanel
{
	Q_OBJECT

public:
	explicit CLodGeneratorOptionsPanel(QWidget *parent = 0);
	~CLodGeneratorOptionsPanel();

public:
	void ParameterChanged(SLodParameter* parameter);
	void Update();
	void Reset();

private:
	void LoadPropertiesTree();
	void FillProperty(SLodParameterGroup& sLodParameterGroup,CVarBlock* pVarBlock);

private:
	Ui::CLodGeneratorOptionsPanel *ui;

	SLodParameterGroupSet m_groups;
};

