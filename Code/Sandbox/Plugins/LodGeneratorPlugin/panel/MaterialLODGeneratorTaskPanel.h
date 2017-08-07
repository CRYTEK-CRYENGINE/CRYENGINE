#pragma once

#include <QWidget>

namespace Ui {
class CMaterialLODGeneratorTaskPanel;
}

class CMaterialLODGeneratorTaskPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CMaterialLODGeneratorTaskPanel(QWidget *parent = 0);
    ~CMaterialLODGeneratorTaskPanel();

signals:
	bool OnGenerateMaterial();

public slots:
	void OnButtonGenerate(bool checked = false);

private:
    Ui::CMaterialLODGeneratorTaskPanel *ui;
};

