// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct ICharacterInstance;

class QString;

QString CreateCDF(
  const QString& skeletonFilePath,
  const QString& skinFilePath,
  const QString& materialFilePath);

ICharacterInstance* CreateTemporaryCharacter(
  const QString& skeletonFilePath,
  const QString& skinFilePath,
  const QString& materialFilePath);

