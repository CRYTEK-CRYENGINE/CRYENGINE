#pragma once

#include <QWidget>
#include "../Util/LodParameter.h"
#include "GeneratorOptionsPanel.h"

namespace Ui 
{
	class CGeometryLodGeneratorOptionsPanel;
}

class CGeometryLodGeneratorOptionsPanel : public CGeneratorOptionsPanel
{
	Q_OBJECT

public:
	explicit CGeometryLodGeneratorOptionsPanel(QWidget *parent = 0);
	~CGeometryLodGeneratorOptionsPanel();

public:
	void ParameterChanged(SLodParameter* parameter);
	void Update();
	void Reset();

private:
	void LoadPropertiesTree();

private:
	Ui::CGeometryLodGeneratorOptionsPanel *ui;

	SLodParameterGroupSet m_groups;
};

