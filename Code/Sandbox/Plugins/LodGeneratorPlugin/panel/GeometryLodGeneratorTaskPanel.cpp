#include "StdAfx.h"
#include "GeometryLodGeneratorTaskPanel.h"
#include "../UIs/ui_cgeometrylodgeneratortaskpanel.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"

using namespace LODGenerator;

CGeometryLodGeneratorTaskPanel::CGeometryLodGeneratorTaskPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CGeometryLodGeneratorTaskPanel)
{
    ui->setupUi(this);

	ui->GenerateLodChainButtonProgressBar->setRange(0,100);
	connect(ui->GenerateLodChainButton, SIGNAL(clicked()), this, SLOT(LodGenGenerate()));
	connect(ui->CanelGenerateLodChainButton, SIGNAL(clicked()), this, SLOT(LodGenCancel()));
	//connect(this,SIGNAL(UpdateTime_Signal(const QString)),ui->GenerateLodChainButtonLabel,SLOT(setText(const QString)));
	connect(this,SIGNAL(UpdateProgress_Signal(int)),ui->GenerateLodChainButtonProgressBar,SLOT(setValue(int)));
	//connect(this,SIGNAL(EnbleProgress_Signal(bool)),ui->GenerateLodChainButtonProgressBar,SLOT(setEnabled(int)));

	ui->GenerateLodChainButton->show();
	ui->GenerateLodChainButton->setEnabled(true);
	ui->CanelGenerateLodChainButton->hide();
	ui->CanelGenerateLodChainButton->setEnabled(false);
	
}

CGeometryLodGeneratorTaskPanel::~CGeometryLodGeneratorTaskPanel()
{
    delete ui;
}

void CGeometryLodGeneratorTaskPanel::LodGenGenerate()
{
	if ( CLodGeneratorInteractionManager::Instance()->LodGenGenerate() )
	{
		TaskStarted();
	}
}

void CGeometryLodGeneratorTaskPanel::LodGenCancel()
{
	CWaitCursor wait;
	CLodGeneratorInteractionManager::Instance()->LogGenCancel();

	TaskFinished();
	Reset();
}

void CGeometryLodGeneratorTaskPanel::Reset()
{
	UpdateProgress_Signal(0);
	ui->GenerateLodChainButtonProgressBar->setEnabled(false);

	QString str = "Waiting for task..";
	ui->GenerateLodChainButtonLabel->setText(str);
	//UpdateTime_Signal(str);

	TaskFinished();
}

void CGeometryLodGeneratorTaskPanel::timerEvent( QTimerEvent *event )

{
	float fProgress = 0.0f;
	float fProgressPercentage = 0.0f;
	QString text;
	bool bFinished = false;

	if ( CLodGeneratorInteractionManager::Instance()->LodGenGenerateTick(&fProgress) )
	{
		text = "Finished!";
		bFinished = true;
		TaskFinished();
		OnLodChainGenerationFinished();
	}
	else
	{
		bFinished = false;
		fProgressPercentage = 100.0f*fProgress;
		if ( fProgressPercentage == 0.0f )
		{
			text = "Estimating...";
		}
		else
		{
			QTime time = QTime::currentTime();

			float fExpendedTime = (time.msecsSinceStartOfDay() - m_StartTime.msecsSinceStartOfDay())/1000.0f;
			float fTotalSeconds = (fExpendedTime / fProgressPercentage) * 100.0f;
			float fTimeLeft = fTotalSeconds - fExpendedTime;
			int hours = (int)(fTimeLeft / (60.0f *60.0f));
			fTimeLeft -= hours * (60.0f *60.0f);
			int Minutes = (int)(fTimeLeft / 60.0f);
			int Seconds = (((int)fTimeLeft) % 60);
			text = QString("ETA %1 Hours %2 Minutes %3 Seconds Remaining").arg(QString::number(hours),QString::number(Minutes),QString::number(Seconds));
		}
	}

	ui->GenerateLodChainButtonLabel->setText(text);
	//UpdateTime_Signal(StringTool::CStringToQString(text));

	UpdateProgress_Signal(fProgressPercentage);
	//EnbleProgress_Signal(bFinished);
}

void CGeometryLodGeneratorTaskPanel::TaskStarted()
{
	setCursor(QCursor(Qt::WaitCursor));

	ui->GenerateLodChainButtonProgressBar->setEnabled(true);

	m_StartTime = QTime::currentTime();

	ui->GenerateLodChainButton->hide();
	ui->GenerateLodChainButton->setEnabled(false);
	ui->CanelGenerateLodChainButton->show();
	ui->CanelGenerateLodChainButton->setEnabled(true);

	m_nTimerId = startTimer(1000);
}

void CGeometryLodGeneratorTaskPanel::TaskFinished()
{
	ui->GenerateLodChainButton->show();
	ui->GenerateLodChainButton->setEnabled(true);
	ui->CanelGenerateLodChainButton->hide();
	ui->CanelGenerateLodChainButton->setEnabled(false);

	if ( m_nTimerId != 0 )
		killTimer(m_nTimerId);
	m_nTimerId = 0;

	ui->GenerateLodChainButtonProgressBar->setEnabled(false);

	setCursor(QCursor(Qt::ArrowCursor));
}