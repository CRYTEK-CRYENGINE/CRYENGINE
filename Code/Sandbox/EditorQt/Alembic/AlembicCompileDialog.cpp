// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AlembicCompileDialog.h"

IMPLEMENT_DYNAMIC(CAlembicCompileDialog, CDialog)
BEGIN_MESSAGE_MAP(CAlembicCompileDialog, CDialog)
ON_COMMAND(IDC_RADIO_YUP, OnRadioYUp)
ON_COMMAND(IDC_RADIO_ZUP, OnRadioZUp)
ON_COMMAND(IDC_RADIO_BASE0, OnFaceSetNumberingBase0)
ON_COMMAND(IDC_RADIO_BASE1, OnFaceSetNumberingBase1)
ON_COMMAND(IDC_PLAYBACKFROMMEMORY, OnPlaybackFromMemory)
ON_CBN_SELENDOK(IDC_BLOCKCOMPRESSION, OnBlockCompressionSelected)
ON_COMMAND(IDC_MESHPREDICTION, OnMeshPredictionCheckBox)
ON_COMMAND(IDC_USEBFRAMES, OnUseBFramesCheckBox)
ON_EN_CHANGE(IDC_INDEXFRAMEDISTANCE, OnIndexFrameDistanceChanged)
ON_EN_CHANGE(IDC_PRECISION, OnPositionPrecisionChanged)
ON_CBN_SELENDOK(IDC_PRESET, OnPresetSelected)
END_MESSAGE_MAP()

CAlembicCompileDialog::CAlembicCompileDialog(const XmlNodeRef config)
	: CDialog(IDD_ABCCOMPILE)
{
	m_config = LoadConfig("", config);
}

BOOL CAlembicCompileDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_blockCompressionFormatCombo.AddString("store");
	m_blockCompressionFormatCombo.AddString("deflate");
	m_blockCompressionFormatCombo.AddString("lz4hc");

	std::vector<string> presetFiles;
	const char* const filePattern = "*.cbc";

	SDirectoryEnumeratorHelper dirHelper;
	dirHelper.ScanDirectoryRecursive("", "%EDITOR%/Presets/GeomCache", filePattern, presetFiles);

	for (auto iter = presetFiles.begin(); iter != presetFiles.end(); ++iter)
	{
		const string& file = *iter;
		m_presets.push_back(LoadConfig(PathUtil::GetFileName(file).c_str(), XmlHelpers::LoadXmlFromFile(file)));
		m_presetComboBox.AddString(m_presets.back().m_name);
	}

	m_presetComboBox.AddString("(Custom)");

	SetFromConfig(m_config);
	UpdatePresetSelection();
	UpdateEnabledStates();

	return TRUE;
}

void CAlembicCompileDialog::SetFromConfig(const SConfig& config)
{
	if (config.m_blockCompressionFormat.CompareNoCase("deflate") == 0)
	{
		m_blockCompressionFormatCombo.SetCurSel(1);
	}
	else if (config.m_blockCompressionFormat.CompareNoCase("lz4hc") == 0)
	{
		m_blockCompressionFormatCombo.SetCurSel(2);
	}
	else
	{
		m_blockCompressionFormatCombo.SetCurSel(0);
	}

	if (config.m_upAxis.CompareNoCase("Y") == 0)
	{
		m_yUpRadio.SetCheck(BST_CHECKED);
		m_zUpRadio.SetCheck(BST_UNCHECKED);
	}
	else
	{
		m_yUpRadio.SetCheck(BST_UNCHECKED);
		m_zUpRadio.SetCheck(BST_CHECKED);
	}

	if (config.m_faceSetNumberingBase.CompareNoCase("0") == 0)
	{
		m_faceSetNumberingBase0Radio.SetCheck(BST_CHECKED);
		m_faceSetNumberingBase1Radio.SetCheck(BST_UNCHECKED);
	}
	else
	{
		m_faceSetNumberingBase0Radio.SetCheck(BST_UNCHECKED);
		m_faceSetNumberingBase1Radio.SetCheck(BST_CHECKED);
	}

	if (config.m_playbackFromMemory.CompareNoCase("1") == 0)
	{
		m_playbackFromMemoryCheckBox.SetCheck(BST_CHECKED);
	}
	else
	{
		m_playbackFromMemoryCheckBox.SetCheck(BST_UNCHECKED);
	}

	if (config.m_meshPrediction.CompareNoCase("1") == 0)
	{
		m_meshPredictionCheckBox.SetCheck(BST_CHECKED);
	}
	else
	{
		m_meshPredictionCheckBox.SetCheck(BST_UNCHECKED);
	}

	if (config.m_useBFrames.CompareNoCase("1") == 0)
	{
		m_useBFramesCheckBox.SetCheck(BST_CHECKED);
	}
	else
	{
		m_useBFramesCheckBox.SetCheck(BST_UNCHECKED);
	}

	CString indexFrameDistanceString;
	indexFrameDistanceString.Format("%d", config.m_indexFrameDistance);
	m_indexFrameDistanceEdit.SetWindowText(indexFrameDistanceString);

	CString positionPrecisionString;
	positionPrecisionString.Format("%g", config.m_positionPrecision);
	m_positionPrecisionEdit.SetWindowText(positionPrecisionString);
}

void CAlembicCompileDialog::UpdateEnabledStates()
{
	m_meshPredictionCheckBox.EnableWindow(FALSE);
	m_useBFramesCheckBox.EnableWindow(FALSE);
	m_indexFrameDistanceEdit.EnableWindow(FALSE);

	if (m_config.m_blockCompressionFormat.CompareNoCase("store") != 0)
	{
		m_meshPredictionCheckBox.EnableWindow(TRUE);
		m_useBFramesCheckBox.EnableWindow(TRUE);

		if (m_config.m_useBFrames.CompareNoCase("1") == 0)
		{
			m_indexFrameDistanceEdit.EnableWindow(TRUE);
		}
	}
}

void CAlembicCompileDialog::UpdatePresetSelection()
{
	for (uint i = 0; i < m_presets.size(); ++i)
	{
		if (m_presets[i] == m_config)
		{
			m_presetComboBox.SetCurSel(i);
			return;
		}
	}

	m_presetComboBox.SetCurSel(m_presets.size());
}

void CAlembicCompileDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_RADIO_YUP, m_yUpRadio);
	DDX_Control(pDX, IDC_RADIO_ZUP, m_zUpRadio);
	DDX_Control(pDX, IDC_RADIO_BASE0, m_faceSetNumberingBase0Radio);
	DDX_Control(pDX, IDC_RADIO_BASE1, m_faceSetNumberingBase1Radio);
	DDX_Control(pDX, IDC_PLAYBACKFROMMEMORY, m_playbackFromMemoryCheckBox);
	DDX_Control(pDX, IDC_PRESET, m_presetComboBox);
	DDX_Control(pDX, IDC_BLOCKCOMPRESSION, m_blockCompressionFormatCombo);
	DDX_Control(pDX, IDC_MESHPREDICTION, m_meshPredictionCheckBox);
	DDX_Control(pDX, IDC_USEBFRAMES, m_useBFramesCheckBox);
	DDX_Control(pDX, IDC_INDEXFRAMEDISTANCE, m_indexFrameDistanceEdit);
	DDX_Control(pDX, IDC_PRECISION, m_positionPrecisionEdit);
}

CString CAlembicCompileDialog::GetUpAxis() const
{
	return m_config.m_upAxis;
}

CString CAlembicCompileDialog::GetPlaybackFromMemory() const
{
	return m_config.m_playbackFromMemory;
}

CString CAlembicCompileDialog::GetBlockCompressionFormat() const
{
	return m_config.m_blockCompressionFormat;
}

CString CAlembicCompileDialog::GetMeshPrediction() const
{
	return m_config.m_meshPrediction;
}

CString CAlembicCompileDialog::GetUseBFrames() const
{
	return m_config.m_useBFrames;
}

CString CAlembicCompileDialog::GetFaceSetNumberingBase() const
{
	return m_config.m_faceSetNumberingBase;
}

uint CAlembicCompileDialog::GetIndexFrameDistance() const
{
	return m_config.m_indexFrameDistance;
}

double CAlembicCompileDialog::GetPositionPrecision() const
{
	return m_config.m_positionPrecision;
}

void CAlembicCompileDialog::OnRadioYUp()
{
	m_config.m_upAxis = "Y";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnFaceSetNumberingBase0()
{
	m_config.m_faceSetNumberingBase = "0";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnFaceSetNumberingBase1()
{
	m_config.m_faceSetNumberingBase = "1";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnRadioZUp()
{
	m_config.m_upAxis = "Z";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnPlaybackFromMemory()
{
	m_config.m_playbackFromMemory = m_playbackFromMemoryCheckBox.GetCheck() == BST_CHECKED ? "1" : "0";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnBlockCompressionSelected()
{
	m_blockCompressionFormatCombo.GetLBText(m_blockCompressionFormatCombo.GetCurSel(), m_config.m_blockCompressionFormat);
	UpdatePresetSelection();
	UpdateEnabledStates();
}

void CAlembicCompileDialog::OnMeshPredictionCheckBox()
{
	m_config.m_meshPrediction = m_meshPredictionCheckBox.GetCheck() == BST_CHECKED ? "1" : "0";
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnUseBFramesCheckBox()
{
	m_config.m_useBFrames = m_useBFramesCheckBox.GetCheck() == BST_CHECKED ? "1" : "0";
	UpdatePresetSelection();
	UpdateEnabledStates();
}

void CAlembicCompileDialog::OnIndexFrameDistanceChanged()
{
	CString indexFrameDistanceString;
	m_indexFrameDistanceEdit.GetWindowText(indexFrameDistanceString);
	m_config.m_indexFrameDistance = atoi(indexFrameDistanceString.GetString());
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnPositionPrecisionChanged()
{
	CString positionPrecisionString;
	m_positionPrecisionEdit.GetWindowText(positionPrecisionString);
	m_config.m_positionPrecision = atof(positionPrecisionString.GetString());
	UpdatePresetSelection();
}

void CAlembicCompileDialog::OnPresetSelected()
{
	uint presetIndex = m_presetComboBox.GetCurSel();
	if (presetIndex < m_presets.size())
	{
		m_config = m_presets[presetIndex];
		SetFromConfig(m_config);
	}

	m_presetComboBox.SetCurSel(presetIndex);
	UpdateEnabledStates();
}

CAlembicCompileDialog::SConfig CAlembicCompileDialog::LoadConfig(const CString& fileName, XmlNodeRef xml) const
{
	SConfig config;
	config.m_name = fileName;

	if (xml)
	{
		xml->getAttr("Name", config.m_name);
		xml->getAttr("BlockCompressionFormat", config.m_blockCompressionFormat);
		xml->getAttr("UpAxis", config.m_upAxis);
		xml->getAttr("PlaybackFromMemory", config.m_playbackFromMemory);
		xml->getAttr("MeshPrediction", config.m_meshPrediction);
		xml->getAttr("UseBFrames", config.m_useBFrames);
		xml->getAttr("FaceSetNumberingBase", config.m_faceSetNumberingBase);
		xml->getAttr("IndexFrameDistance", config.m_indexFrameDistance);
		xml->getAttr("PositionPrecision", config.m_positionPrecision);
	}

	return config;
}

