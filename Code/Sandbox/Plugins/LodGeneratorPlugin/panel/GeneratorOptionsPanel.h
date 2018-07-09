#pragma once

#include <QWidget>
#include "../Util/LodParameter.h"

namespace Ui {
class CGeneratorOptionsPanel;
}

class CGeneratorOptionsPanel : public QWidget
{
	Q_OBJECT

public:
	explicit CGeneratorOptionsPanel(QWidget *parent = 0) : QWidget(parent)
	{
	}

	~CGeneratorOptionsPanel()
	{
	}

public:
	virtual void ParameterChanged(SLodParameter* parameter) = 0;

};

