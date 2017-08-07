#pragma once

#include <QWidget>
#include <QTime>

namespace Ui {
class CGeometryLodGeneratorTaskPanel;
}

class CGeometryLodGeneratorTaskPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CGeometryLodGeneratorTaskPanel(QWidget *parent = 0);
    ~CGeometryLodGeneratorTaskPanel();

	void Reset();

signals:
	void UpdateTime_Signal(const QString);
	void UpdateProgress_Signal(int Progress);
	void EnbleProgress_Signal(bool enable);
	void OnLodChainGenerationFinished();

public slots:

	void LodGenGenerate();
	void LodGenCancel();

private:

	void timerEvent( QTimerEvent *event );
	void TaskStarted();
	void TaskFinished();

	

private:
    Ui::CGeometryLodGeneratorTaskPanel *ui;

	QTime m_StartTime;

private:
	int m_nTimerId;
};

