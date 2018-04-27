// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <QAbstractItemModel>

struct IAnimationSet;

class EDITOR_COMMON_API CCharacterAnimationsModel : public QAbstractItemModel
{
public:
	CCharacterAnimationsModel(IAnimationSet& animationSet, QObject* pParent = nullptr)
		: m_animationSet(animationSet)
	{}

	virtual ~CCharacterAnimationsModel()
	{}

	// QAbstractItemModel
	virtual int         rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int         columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant    data(const QModelIndex& index, int role) const override;
	virtual QVariant    headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;
	// ~QAbstractItemModel

private:
	IAnimationSet& m_animationSet;
};

