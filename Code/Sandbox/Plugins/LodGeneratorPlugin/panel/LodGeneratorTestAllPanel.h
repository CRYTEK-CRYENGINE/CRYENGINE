#pragma once

#include <QWidget>
#include <QTime>
#include <vector>
#include <string>

#include "LodGeneratorOptionsPanel.h"

namespace Ui {

	enum ELodGeneratorState
	{
		eLGS_GeneratePreparePre,
		eLGS_GeneratePrepare,
		eLGS_GenerateMesh,
		eLGS_GenerateMaterial,
		eLGS_None,
	};
	struct LodInfo
	{
		int lod;
		float percentage;
		int width;
		int height;
	};
class CLodGeneratorTestAllPanel;
}

class CLodGeneratorTestAllPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CLodGeneratorTestAllPanel(QWidget *parent = 0);
    ~CLodGeneratorTestAllPanel();

	const QString LoadedFile();
	const QString MaterialFile();
	const void RefreshMaterialFile();
	void Reset();

signals:
    void SelectMeshFile_Signal(const QString);
	void SelectMateralFile_Signal(const QString);
	void UpdateProgress_Signal(int Progress);

public slots:
    void SelectMeshFile_Slot();
	void SelectObjFile_Slot();
	void SelectMaterial_Slot();
	void SelectConfig_Slot(int index);

	void AutoGenerate();
	void GenerateOption();

private:
	void OnGenerateMeshs();
	void OnGenerateMaterials();
	void OnSave();
	void OnOpenWithPathParameter(const QString& objectPath);
	void ShowMessage(std::string message);
	void timerEvent( QTimerEvent *event );
	void TaskStarted();
	void TaskFinished();
	void SetState(Ui::ELodGeneratorState lodGeneratorState);

	bool FileOpened();
	void UpdateConfig();

private:
    Ui::CLodGeneratorTestAllPanel *ui;
	int m_nTimerId;
	QTime m_StartTime;

    QString m_MeshPath;
	QString m_MaterialPath;
	Ui::ELodGeneratorState m_LodGeneratorState;

	std::vector<Ui::LodInfo> m_LodInfoList;

	CLodGeneratorOptionsPanel* m_LodGeneratorOptionsPanel;
};

