#include "StdAfx.h"
#include "LodGeneratorAllPanel.h"
#include "../UIs/ui_clodgeneratorallpanel.h"
#include "qfiledialog.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"
#include <Map>
#include <string>
#include "Controls/QuestionDialog.h"

using namespace LODGenerator;

static std::map<int,QString> ConfigMap;
CLodGeneratorAllPanel::CLodGeneratorAllPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CLodGeneratorAllPanel)
{   
    ui->setupUi(this);

    connect(ui->SelectMeshButton, SIGNAL(clicked()), this, SLOT(SelectMeshFile_Slot()));
	connect(ui->SelectObjButton, SIGNAL(clicked()), this, SLOT(SelectObjFile_Slot()));
	//connect(ui->SelectMaterialButton, SIGNAL(clicked()), this, SLOT(SelectMaterial_Slot()));

	connect(ui->AutoGenerateButton, SIGNAL(clicked()), this, SLOT(AutoGenerate()));
	connect(ui->GenerateOptionButton, SIGNAL(clicked()), this, SLOT(GenerateOption()));

	connect(ui->GenerateConifgCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(SelectConfig_Slot(int)));

	connect(this,SIGNAL(SelectMeshFile_Signal(const QString)),ui->SelectMeshPathEdit,SLOT(setText(const QString)));
	//connect(this,SIGNAL(SelectMateralFile_Signal(const QString)),ui->SelectMaterialPathEdit,SLOT(setText(const QString)));

	//-------------------------------------------------------------------------------------------------------------

	connect(this,SIGNAL(UpdateProgress_Signal(int)),ui->AutoGenerateProgress,SLOT(setValue(int)));


	m_LodGeneratorState = Ui::eLGS_None;

	UpdateProgress_Signal(0);

	m_LodGeneratorOptionsPanel = new CLodGeneratorOptionsPanel(NULL);
	m_LodGeneratorOptionsPanel->hide();

	ConfigMap[6] = "Six Lods";
	ConfigMap[5] = "Five Lods";
	ConfigMap[4] = "Four Lods";
	ConfigMap[3] = "Tree Lods";
	ConfigMap[2] = "Two Lods";
	ConfigMap[1] = "One Lods";
	ConfigMap[0] = "No Lods";

	for (int i=0;i<ConfigMap.size();i++)
	{
		ui->GenerateConifgCombo->addItem(ConfigMap[i]);
	}

	//-------------------------------------------------------------------------------------------------------------
}

CLodGeneratorAllPanel::~CLodGeneratorAllPanel()
{
    delete ui;

	if (m_LodGeneratorOptionsPanel != NULL)
		delete m_LodGeneratorOptionsPanel;
}

void CLodGeneratorAllPanel::UpdateConfig()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	const int nLods = pInstance->NumberOfLods();
	if(nLods >=1 && nLods <= ConfigMap.size())
		ui->GenerateConifgCombo->setCurrentIndex(nLods - 1);
}

void CLodGeneratorAllPanel::SelectConfig_Slot(int index)
{
	m_LodInfoList.clear();
	int allCount = index;
	for (int i=0;i<allCount;i++)
	{
		int level = pow(2,i+1);
		Ui::LodInfo lodInfo;
		lodInfo.percentage = 100.0/level;
		lodInfo.width = 2048/level;
		lodInfo.height = 2048/level;
		m_LodInfoList.push_back(lodInfo);
	}
}

void CLodGeneratorAllPanel::SetState(Ui::ELodGeneratorState lodGeneratorState)
{
	m_LodGeneratorState = lodGeneratorState;
}

void CLodGeneratorAllPanel::ShowMessage(std::string message)
{
	CryLog("Lod Generator Auto: %s",message.c_str());
	//ui->MessageShow->append(message);
}

void CLodGeneratorAllPanel::SelectMaterial_Slot()
{
	setCursor(QCursor(Qt::WaitCursor));  
	CLodGeneratorInteractionManager::Instance()->OpenMaterialEditor();
	setCursor(QCursor(Qt::ArrowCursor));  
}

void CLodGeneratorAllPanel::SelectMeshFile_Slot()
{
    m_MeshPath = QFileDialog::getOpenFileName(this,  tr("Select File"),  "",  tr("Mesh (*.cgf)"));
	string material(CLodGeneratorInteractionManager::Instance()->GetDefaultBrushMaterial(CStringTool::QStringToCString(m_MeshPath)));

    SelectMeshFile_Signal(m_MeshPath);
	SelectMateralFile_Signal(CStringTool::CStringToQString(material));

	FileOpened();

	UpdateConfig();
}

bool CLodGeneratorAllPanel::FileOpened()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	string filepath(CStringTool::QStringToCString(LoadedFile()));
	if(!pInstance->LoadStatObj(filepath))
	{
		CQuestionDialog::SWarning("File Load Failed", "Failed to load file into the engine, please consult the editor.log");
		return false;
	}

	string materialpath(CStringTool::QStringToCString(MaterialFile()));
	if(!pInstance->LoadMaterial(materialpath))
	{
		CQuestionDialog::SWarning("File Load Failed", "Failed to load material file into the engine, please consult the editor.log");
		return false;
	}

	return TRUE;
}

void CLodGeneratorAllPanel::AutoGenerate()
{
	TaskStarted();
	SetState(Ui::eLGS_GeneratePreparePre);	
}

void CLodGeneratorAllPanel::GenerateOption()
{
	if (m_LodGeneratorOptionsPanel->isHidden())
	{
		m_LodGeneratorOptionsPanel->show();
	} 
	else
	{
		m_LodGeneratorOptionsPanel->hide();
	}
}

void CLodGeneratorAllPanel::TaskStarted()
{
	m_nTimerId = startTimer(1000);
}

void CLodGeneratorAllPanel::TaskFinished()
{
	if ( m_nTimerId != 0 )
		killTimer(m_nTimerId);
	m_nTimerId = 0;
}

void CLodGeneratorAllPanel::timerEvent( QTimerEvent *event )
{
	switch (m_LodGeneratorState)
	{
	case Ui::eLGS_GeneratePreparePre:
		{
			ShowMessage("Before Generate Pre Prepare");
			CLodGeneratorInteractionManager::Instance()->LodGenGenerate();		
			m_StartTime = QTime::currentTime();
			SetState(Ui::eLGS_GeneratePrepare);
			ShowMessage("After Generate Pre Prepare");
			ShowMessage("Before Generate Prepare");

			UpdateProgress_Signal(0);
		}
		break;
	case Ui::eLGS_GeneratePrepare:
		{
			float fProgress = 0.0f;
			float fProgressPercentage = 0.0f;
			QString text;
			bool bFinished = false;

			if ( CLodGeneratorInteractionManager::Instance()->LodGenGenerateTick(&fProgress) )
			{
				text = "Finished!";
				bFinished = true;
				SetState(Ui::eLGS_GenerateMesh);
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

				UpdateProgress_Signal(80.0f*fProgress);
			}
		}
		break;
	case Ui::eLGS_GenerateMesh:
		{
			ShowMessage("After Generate Prepare");
			ShowMessage("Before Generate Mesh");
			OnGenerateMeshs();
			SetState(Ui::eLGS_GenerateMaterial);
			ShowMessage("After Generate Mesh");
			UpdateProgress_Signal(85);
		}
		break;
	case Ui::eLGS_GenerateMaterial:
		{
			ShowMessage("Before Generate Material");
			OnGenerateMaterials();
			SetState(Ui::eLGS_None);
			ShowMessage("After Generate Material");
			UpdateProgress_Signal(100);
		}
		break;
	case Ui::eLGS_None:
		{
			TaskFinished();
			ShowMessage("Generate All Done");
		}
		break;
	default:
		break;
	}
}

void CLodGeneratorAllPanel::OnGenerateMeshs()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	int highestLod = pInstance->GetHighestLod();

	if ( highestLod > 0 )
	{
		// TODO
		/*if ( CryMessageBox("Lods already exist for this model, generating will overwrite any existing lods.","Overwrite existing lods?",MB_OKCANCEL) == IDCANCEL )
		{
		}*/
	}

	const int lods = m_LodInfoList.size();

	pInstance->ClearUnusedLods(lods);

	int nSourceLod = pInstance->GetGeometryOption<int>("nSourceLod");
	for ( int idx = 1; idx <= lods ; ++idx )
	{
		Ui::LodInfo& lodInfo = m_LodInfoList[idx - 1];
		pInstance->GenerateLod(nSourceLod+idx,lodInfo.percentage);
		lodInfo.lod = idx;
	}
	pInstance->SaveSettings();
	pInstance->ReloadModel();
}


void CLodGeneratorAllPanel::OnGenerateMaterials()
{
	CLodGeneratorInteractionManager::Instance()->ClearResults();
	const int nNumLods = m_LodInfoList.size();
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		Ui::LodInfo& lodInfo = m_LodInfoList[idx];
		int nLodId = lodInfo.lod;
		int nWidth = lodInfo.width;
		int nHeight = lodInfo.height;

		CLodGeneratorInteractionManager::Instance()->RunProcess(nLodId,nWidth,nHeight);
	}
	
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		Ui::LodInfo& lodInfo = m_LodInfoList[idx];
		int nLodId = lodInfo.lod;
		CLodGeneratorInteractionManager::Instance()->SaveTextures(nLodId);
	}

	CLodGeneratorInteractionManager::Instance()->ClearResults();
}

void CLodGeneratorAllPanel::OnOpenWithPathParameter(const QString& objectPath )
{
	if(!objectPath.isEmpty())
	{
		CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
		m_MaterialPath = CStringTool::CStringToQString(pInstance->GetDefaultBrushMaterial(CStringTool::QStringToCString(objectPath)));

		m_MeshPath = objectPath;

		SelectMeshFile_Signal(m_MeshPath);
		SelectMateralFile_Signal(m_MaterialPath);

		pInstance->SetParameterFilePath("");
	}
}

void CLodGeneratorAllPanel::SelectObjFile_Slot()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	string filepath(pInstance->GetParameterFilePath());

	if( filepath.IsEmpty() )
		filepath = pInstance->GetSelectedBrushFilepath();
	else
	{
		OnOpenWithPathParameter(CStringTool::CStringToQString(pInstance->GetParameterFilePath()));
		return;
	}

	if ( filepath.IsEmpty() )
		return;

	setCursor(QCursor(Qt::WaitCursor)); 

	m_MeshPath = CStringTool::CStringToQString(filepath);
	m_MaterialPath = CStringTool::CStringToQString(pInstance->GetSelectedBrushMaterial());

	SelectMeshFile_Signal(m_MeshPath);
	SelectMateralFile_Signal(m_MaterialPath);

	FileOpened();

	setCursor(QCursor(Qt::ArrowCursor));  

	UpdateConfig();
}

const QString CLodGeneratorAllPanel::LoadedFile()
{
	return m_MeshPath;
}

const QString CLodGeneratorAllPanel::MaterialFile()
{
	return m_MaterialPath;
}

const void CLodGeneratorAllPanel::RefreshMaterialFile()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();

	setCursor(QCursor(Qt::WaitCursor));  
	m_MaterialPath = CStringTool::CStringToQString(CLodGeneratorInteractionManager::Instance()->GetDefaultBrushMaterial(CStringTool::QStringToCString(m_MeshPath)));

	SelectMateralFile_Signal(m_MaterialPath);
	setCursor(QCursor(Qt::ArrowCursor)); 
}

void CLodGeneratorAllPanel::Reset()
{

}