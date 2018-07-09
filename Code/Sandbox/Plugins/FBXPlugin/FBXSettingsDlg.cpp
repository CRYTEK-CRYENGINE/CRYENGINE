// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FBXSettingsDlg.h"
#include "FBXExporter.h"

#include "Controls/EditorDialog.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/Enum.h>

namespace Private_FBXSettingsDlg
{

enum EFbxFormat
{
	eFbxFormat_Bin,
	eFbxFormat_Ascii,
};

YASLI_ENUM_BEGIN(EFbxFormat, "FbxFormat")
YASLI_ENUM(EFbxFormat::eFbxFormat_Bin, "bin", "Binary");
YASLI_ENUM(EFbxFormat::eFbxFormat_Ascii, "ascii", "Text (ascii)");
YASLI_ENUM_END()

YASLI_ENUM_BEGIN(EAxis, "Axis")
YASLI_ENUM(EAxis::eAxis_Y, "Y", "Y");
YASLI_ENUM(EAxis::eAxis_Z, "Z", "Z");
YASLI_ENUM_END()

struct SSerializer
{
	SSerializer(SFBXSettings& value) : value(value) {}

	bool Serialize(yasli::Archive& ar)
	{
		ar(value.bCopyTextures, "textures", "Copy textures into folder with FBX file");
		ar(value.bEmbedded, "embedded", "Embedded data (Put textures inside FBX file)");

		EFbxFormat format = value.bAsciiFormat ? eFbxFormat_Ascii : eFbxFormat_Bin;
		ar(format, "ascii", "Format of FBX file");
		value.bAsciiFormat = format;

		ar(value.axis, "up", "Up Axis");
		return true;
	}

	SFBXSettings& value;
};

class CSettingsDialog : public CEditorDialog
{
public:
	CSettingsDialog(SSerializer& settings):
		CEditorDialog("FBXExportSettings")
	{
		setWindowTitle(tr("FBX Export Settings"));

		QPropertyTree* pPropertyTree = new QPropertyTree(this);

		PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
		treeStyle.propertySplitter = true;
		treeStyle.groupRectangle = false;
		pPropertyTree->setTreeStyle(treeStyle);
		pPropertyTree->setCompact(false);
		pPropertyTree->setExpandLevels(1);
		pPropertyTree->setSliderUpdateDelay(5);
		pPropertyTree->setValueColumnWidth(0.6f);
		pPropertyTree->attach(Serialization::SStruct(settings));

		QVBoxLayout* pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->addWidget(pPropertyTree);

		auto pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
		pLayout->addWidget(pButtons, 0);

		connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

		setLayout(pLayout);
	}
};

}

bool OpenFBXSettingsDlg(struct SFBXSettings& settings)
{
	using namespace Private_FBXSettingsDlg;

	SSerializer serializer(settings);
	CSettingsDialog dialog(serializer);

	return dialog.Execute();
}

