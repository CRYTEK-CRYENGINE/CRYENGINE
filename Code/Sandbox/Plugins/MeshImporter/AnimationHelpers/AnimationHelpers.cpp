// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimationHelpers.h"
#include "DialogCommon.h"  // CreateTemporaryDirectory

#include <IEditor.h>
#include <CryAnimation/ICryAnimation.h>

#include <QTemporaryDir>
#include <QTextStream>

#include <memory.h>

struct STemporaryFile
{
	std::unique_ptr<QTemporaryDir> m_pTempDir;
	QString                        m_filePath;
};

extern void LogPrintf(const char* szFormat, ...);

namespace Private_AnimationHelpers
{

void WriteCDF(STemporaryFile& tempFile, const QString& cdf)
{
	if (cdf.isEmpty())
	{
		return;
	}

	tempFile.m_pTempDir = std::move(CreateTemporaryDirectory());

	const QString filePath = tempFile.m_pTempDir->path() + "/override.cdf";

	QFile file(filePath);
	if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
	{
		return;
	}
	QTextStream out(&file);
	out << cdf;
	file.close();

	tempFile.m_filePath = filePath;
}

ICharacterInstance* LoadCharacter(const QString& cdfFilePath)
{
	ICharacterManager* const pCharManager = GetIEditor()->GetSystem()->GetIAnimationSystem();
	const char* const szFilename = cdfFilePath.toLocal8Bit().constData();
	ICharacterInstance* const pCharInstance = pCharManager->CreateInstance(szFilename, 0);
	if (!pCharInstance)
	{
		LogPrintf("%s: Unable to create character instance from file '%s'.\n", __FUNCTION__, szFilename);
	}
	return pCharInstance;
}

} // namespace Private_AnimationHelpers

QString CreateCDF(
  const QString& skeletonFilePath,
  const QString& skinFilePath,
  const QString& materialFilePath)
{
	static const char* const szCdfContent =
	  "<CharacterDefinition>\n"
	  " <Model File=\"%1\"/>\n"
	  " %2"
	  "</CharacterDefinition>\n";

	static const char* const szSkinContent =
	  " <AttachmentList>\n"
	  "  <Attachment Type=\"CA_SKIN\" AName=\"the_skin\" Binding=\"%1\" %2 Flags=\"0\"/>\n"
	  " </AttachmentList>\n";

	if (skeletonFilePath.isEmpty())
	{
		return QString();
	}

	QString attachments;

	QString material;
	if (!materialFilePath.isEmpty())
	{
		material = QString("Material=\"%1\"").arg(materialFilePath);
	}

	if (!skinFilePath.isEmpty())
	{
		attachments = QString(szSkinContent).arg(skinFilePath).arg(material);
	}

	return QString(szCdfContent).arg(skeletonFilePath).arg(attachments);
}

ICharacterInstance* CreateTemporaryCharacter(
  const QString& skeletonFilePath,
  const QString& skinFilePath,
  const QString& materialFilePath)
{
	using namespace Private_AnimationHelpers;

	static std::unique_ptr<QTemporaryDir> pTempDir;

	if (!pTempDir)
	{
		// Make sure that we have a path that does not conflict with any other CDFs.
		// Note that all instances of the MI can use the same cdf file path, because
		// we do not reference the CDF once the character has been loaded.
		pTempDir = CreateTemporaryDirectory();
	}

	const QString cdfFilePath = pTempDir->path() + "/override.cdf";

	const QString cdfContent = CreateCDF(skeletonFilePath, skinFilePath, materialFilePath);

	ICharacterManager* const pCharManager = GetIEditor()->GetSystem()->GetIAnimationSystem();
	pCharManager->InjectCDF(cdfFilePath.toLocal8Bit().constData(), cdfContent.toLocal8Bit().constData(), cdfContent.length());

	return LoadCharacter(cdfFilePath);
}

