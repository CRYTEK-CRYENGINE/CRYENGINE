#include "StdAfx.h"
#include "AutoGenerator.h"
#include "AutoGeneratorDataBase.h"

namespace LODGenerator 
{
	CAutoGenerator::CAutoGenerator()
	{
		m_PrepareDone = false;
	}

	CAutoGenerator::~CAutoGenerator()
	{

	}

	bool CAutoGenerator::Init(string meshPath, string materialPath)
	{
		if(!CAutoGeneratorDataBase::Instance()->LoadStatObj(meshPath))
			return false;
		if(!CAutoGeneratorDataBase::Instance()->LoadMaterial(materialPath))
			return false;

		m_PrepareDone = false;
		return true;
	}

	bool CAutoGenerator::Prepare()
	{
		m_PrepareDone = false;
		m_AutoGeneratorPreparation.Generate();

		return true;
	}

	bool CAutoGenerator::IsPrepareDone()
	{
		return m_PrepareDone;
	}

	float CAutoGenerator::Tick()
	{
		if (!m_PrepareDone)
		{
			float fProgress = 0.0f;
			float fProgressPercentage = 0.0f;
			QString text;
			bool bFinished = false;

			if ( m_AutoGeneratorPreparation.Tick(&fProgress) )
			{
				text = "Finished!";
				m_PrepareDone = true;
			}
			else
			{
				m_PrepareDone = false;
				fProgressPercentage = 100.0f*fProgress;
				if ( fProgressPercentage == 0.0f )
				{
					text = "Estimating...";
				}
				else
				{
					/*
					QTime time = QTime::currentTime();

					float fExpendedTime = (time.msecsSinceStartOfDay() - m_StartTime.msecsSinceStartOfDay())/1000.0f;
					float fTotalSeconds = (fExpendedTime / fProgressPercentage) * 100.0f;
					float fTimeLeft = fTotalSeconds - fExpendedTime;
					int hours = (int)(fTimeLeft / (60.0f *60.0f));
					fTimeLeft -= hours * (60.0f *60.0f);
					int Minutes = (int)(fTimeLeft / 60.0f);
					int Seconds = (((int)fTimeLeft) % 60);
					text = QString("ETA %1 Hours %2 Minutes %3 Seconds Remaining").arg(QString::number(hours),QString::number(Minutes),QString::number(Seconds));
					*/
				}
			}
			return fProgress;
		}
		return 1.0;
	}

	bool CAutoGenerator::CreateLod(float percent,int width,int height)
	{
		for (int i=0;i<m_LodInfoList.size();i++)
		{
			if (m_LodInfoList[i].percentage == percent)
			{
				return false;
			}
		}

		CAutoGenerator::SLodInfo lodInfo;
		lodInfo.percentage = percent;
		lodInfo.width = width;
		lodInfo.height = height;
		m_LodInfoList.push_back(lodInfo);
		return true;
	}

	bool CAutoGenerator::GenerateLodMesh(int level)
	{
		if (!m_PrepareDone)
			return false;

		if (level <= 0)
			return false;

		int index = level - 1;
		CAutoGenerator::SLodInfo& lodInfo = m_LodInfoList[index];
		m_AutoGeneratorMeshCreator.SetPercentage(lodInfo.percentage);
		return m_AutoGeneratorMeshCreator.Generate(level);
	}

	bool CAutoGenerator::GenerateAllLodMesh()
	{
		if (!m_PrepareDone)
			return false;

		for (int i=0;i<m_LodInfoList.size();i++)
		{
			if (!GenerateLodMesh(i + 1))
			{
				return false;
			}
		}
		return true;
	}

	bool CAutoGenerator::GenerateLodTexture(int level)
	{
		if (!m_PrepareDone)
			return false;

		if (level <= 0)
			return false;
		int index = level - 1;
		CAutoGenerator::SLodInfo& lodInfo = m_LodInfoList[index];
		m_AutoGeneratorTextureCreator.SetWidth(lodInfo.width);
		m_AutoGeneratorTextureCreator.SetHeight(lodInfo.height);
		return m_AutoGeneratorTextureCreator.Generate(level);
	}

	bool CAutoGenerator::GenerateAllLodTexture()
	{
		if (!m_PrepareDone)
			return false;

		for (int i=0;i<m_LodInfoList.size();i++)
		{
			if (!GenerateLodTexture(i + 1))
			{
				return false;
			}
		}
		return true;
	}

	bool CAutoGenerator::GenerateLod(int level)
	{
		if (!GenerateLodMesh(level))
			return false;
		if (!GenerateLodTexture(level))
			return false;
		return true;
	}

	bool CAutoGenerator::GenerateAllLod()
	{
		if (!GenerateAllLodMesh())
			return false;
		if (!GenerateAllLodTexture())
			return false;
		return true;
	}
}