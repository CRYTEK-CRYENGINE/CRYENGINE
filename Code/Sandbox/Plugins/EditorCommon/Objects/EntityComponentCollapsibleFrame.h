// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QCollapsibleFrame.h>

struct IEntityComponent;

//! Specialized implementation of a collapsible frame header for entity components
//! This is responsible for exposing specific functionality such as component icons, ability to remove components etc.
class CEntityComponentCollapsibleFrameHeader : public CCollapsibleFrameHeader
{
	Q_OBJECT

public:
	CEntityComponentCollapsibleFrameHeader(const QString& title, QCollapsibleFrame* pParentCollapsible, IEntityComponent* pComponent);

protected:
	QString m_title;
};

class EDITOR_COMMON_API CEntityComponentCollapsibleFrame : public QCollapsibleFrame
{
	Q_OBJECT

public:
	CEntityComponentCollapsibleFrame(const QString& title, IEntityComponent* pComponent);
	virtual ~CEntityComponentCollapsibleFrame() {}
};

