#ifndef SINGLEGENERATEPANEL_H
#define SINGLEGENERATEPANEL_H

#include <QWidget>

#include "AutoGenerator.h"

namespace Ui {
class SingleGeneratePanel;
}

class CSingleGeneratePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CSingleGeneratePanel(QWidget *parent = 0);
    ~CSingleGeneratePanel();

signals:
	void UpdateProgress_Signal(int Progress);

public slots:
	void SelectObj_Slot();
	void Prepare_Slot();
	void AddLod_Slot();
	void GenerateMesh_Slot();
	void GenerateTexture_Slot();
	void SingleAutoGenerate_Slot();
	void LodIndex_Slot(const QString str);
	

private:
	void ShowMessage(std::string message);
	void timerEvent( QTimerEvent *event );

private:
    Ui::SingleGeneratePanel *ui;
	LODGenerator::CAutoGenerator m_AutoGenerator;
	int OriginWidth;
	int OriginHeight;
	float OriginPercent;
	int index;
	int m_lod;
	bool m_PrepareDone;
	int m_nTimerId;
};

#endif // SINGLEGENERATEPANEL_H
