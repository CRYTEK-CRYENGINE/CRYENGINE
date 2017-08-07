#include "StdAfx.h"
#include "LodGeneratorOptionsPanel.h"
#include "../UIs/ui_clodgeneratoroptionspanel.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"
#include "Util/Variable.h"

using namespace LODGenerator;

CLodGeneratorOptionsPanel::CLodGeneratorOptionsPanel(QWidget *parent) :
    CGeneratorOptionsPanel(parent),
    ui(new Ui::CLodGeneratorOptionsPanel)
{
    ui->setupUi(this);

	Update();
}

CLodGeneratorOptionsPanel::~CLodGeneratorOptionsPanel()
{
    delete ui;
}

void CLodGeneratorOptionsPanel::FillProperty(SLodParameterGroup& sLodParameterGroup,CVarBlock* pVarBlock)
{
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
}

void CLodGeneratorOptionsPanel::Update()
{
	m_groups.m_propertyGroups.clear();

	ui->m_propertyTree->detach();
	
	SLodParameterGroup sLodParameterGroupG;
	FillProperty(sLodParameterGroupG,CLodGeneratorInteractionManager::Instance()->GetGeometryVarBlock());
	m_groups.m_propertyGroups["Geometry Option"] = sLodParameterGroupG;
	SLodParameterGroup sLodParameterGroupM;
	FillProperty(sLodParameterGroupM,CLodGeneratorInteractionManager::Instance()->GetMaterialVarBlock());
	m_groups.m_propertyGroups["Material Option"] = sLodParameterGroupM;

	ui->m_propertyTree->attach(Serialization::SStruct(m_groups));
	ui->m_propertyTree->expandAll();
}

void CLodGeneratorOptionsPanel::ParameterChanged(SLodParameter* parameter)
{
	ELodParamType eLodParamType = parameter->GetTODParamType();
	switch (eLodParamType)
	{
	case eFloatType:
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption(string(parameter->GetName().c_str()),parameter->GetParamFloat());
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamFloat());
			break;
		}
	case eIntType:
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption(string(parameter->GetName().c_str()),parameter->GetParamInt());
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamInt());
			break;
		}
	case eColorType:
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption(string(parameter->GetName().c_str()),parameter->GetParamColor());
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamColor());
			break;
		}
	case eStringType:
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption(string(parameter->GetName().c_str()),parameter->GetParamString());
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamString());
			break;
		}
	case eBoolType:
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption(string(parameter->GetName().c_str()),parameter->GetParamBool());
			CLodGeneratorInteractionManager::Instance()->SetMaterialOption(string(parameter->GetName().c_str()),parameter->GetParamBool());
			break;
		}
	default:
		break;
	}
}

void CLodGeneratorOptionsPanel::Reset()
{

}