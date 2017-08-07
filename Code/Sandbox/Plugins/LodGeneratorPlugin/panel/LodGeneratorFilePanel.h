#pragma once

#include <QWidget>

namespace Ui {
class CLodGeneratorFilePanel;
}

class CLodGeneratorFilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CLodGeneratorFilePanel(QWidget *parent = 0);
    ~CLodGeneratorFilePanel();

	const QString LoadedFile();
	const QString MaterialFile();
	const void RefreshMaterialFile();
	void Reset();

signals:
    void SelectMeshFile_Signal(const QString);
	void SelectMateralFile_Signal(const QString);
	bool FileOpened_Signal();
	void MaterialChanged_Signal(const QString);

public slots:
    void SelectMeshFile_Slot();
	void SelectObjFile_Slot();
	void SelectMaterial_Slot();
	void ChangeMaterial_Slot();

private:
	void OnOpenWithPathParameter(const QString& objectPath);

private:
    Ui::CLodGeneratorFilePanel *ui;

private:
    QString m_MeshPath;
	QString m_MaterialPath;
};

