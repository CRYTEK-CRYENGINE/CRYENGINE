#include "StdAfx.h"
#include "MaterialLodGeneratorOptionsPanel.h"
#include "../UIs/ui_cmateriallodgeneratoroptionspanel.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"

using namespace LODGenerator;

CMaterialLODGeneratorOptionsPanel::CMaterialLODGeneratorOptionsPanel(QWidget *parent) :
    CGeneratorOptionsPanel(parent),
    ui(new Ui::CMaterialLODGeneratorOptionsPanel)
{
    ui->setupUi(this);
}

CMaterialLODGeneratorOptionsPanel::~CMaterialLODGeneratorOptionsPanel()
{
    delete ui;
}


void CMaterialLODGeneratorOptionsPanel::Update()
{
	m_groups.m_propertyGroups.clear();

	ui->m_propertyTree->detach();


	SLodParameterGroup sLodParameterGroup;
	CVarBlock* pVarBlock = CLodGeneratorInteractionManager::Instance()->GetMaterialVarBlock();
	int iVarSize = pVarBlock->GetNumVariables();
	for (int i=0;i<iVarSize;i++)
	{
		IVariable* pCurVariable = pVarBlock->GetVariable(i);
		SLodParameter sLodParameter;
		sLodParameter.SetOptionsPanel(this);

		sLodParameter.SetName(pCurVariable->GetName());
		sLodParameter.SetLabel(pCurVariable->GetName());
		//sLodParameter.SetLabel(pCurVariable->GetDescription());

		ELodParamType eLodParamType = SLodParameter::GetLodParamType(pCurVariable);
		sLodParameter.SetTODParamType(eLodParamType);
		int type = pCurVariable->GetDataType();
		switch (eLodParamType)
		{
		case ELodParamType::eFloatType:
			{
				float value;
				pCurVariable->Get(value);
				sLodParameter.SetParamFloat(value);
				break;
			}
		case ELodParamType::eBoolType:
			{
				bool value;
				pCurVariable->Get(value);
				sLodParameter.SetParamBool(value);
				break;
			}
		case ELodParamType::eColorType:
			{
				Vec4 value;
				pCurVariable->Get(value);
				sLodParameter.SetParamFloat3(Vec3(value.x,value.y,value.z));
				break;
			}
		case ELodParamType::eStringType:
			{
				string value;
				pCurVariable->Get(value);
				sLodParameter.SetParamString(CStringTool::CStringToQString(value).toStdString().c_str());
				break;
			}
		case ELodParamType::eIntType:
			{
				int value;
				pCurVariable->Get(value);
				sLodParameter.SetParamInt(value);
				break;
			}		
		default:
			{
				CryLog("Unknow Type");
				break;	
			}
		}		

		sLodParameterGroup.m_Params.push_back(sLodParameter);
	}

	m_groups.m_propertyGroups["Common"] = sLodParameterGroup;
	ui->m_propertyTree->attach(Serialization::SStruct(m_groups));
	ui->m_propertyTree->expandAll();
}

void CMaterialLODGeneratorOptionsPanel::ParameterChanged(SLodParameter* parameter)
{
	ELodParamType eLodParamType = parameter->GetTODParamType();
	switch (eLodParamType)
	{
	case eFloatType:
		{
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamFloat());
			break;
		}
	case eIntType:
		{
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamInt());
			break;
		}
	case eColorType:
		{
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamColor());
			break;
		}
	case eStringType:
		{
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamString());
			break;
		}
	case eBoolType:
		{
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamBool());
			break;
		}
	default:
		break;
	}

	
}

void CMaterialLODGeneratorOptionsPanel::Reset()
{

}