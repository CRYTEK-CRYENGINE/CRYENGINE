// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QViewport.h>
#include <QViewportConsumer.h>

template<class T> struct QuatT_tpl;
typedef QuatT_tpl<float> QuatT;

struct SDisplayOptions;
class CDisplayOptionsWidget;
class QMenuComboBox;

class QBoxLayout;
class QHBoxLayout;
class QCheckBox;
class QLabel;

class CSplitViewport : public QWidget
{
	Q_OBJECT
public:
	enum ESplitDirection
	{
		eSplitDirection_Horizontal,
		eSplitDirection_Vertical
	};

	CSplitViewport(QWidget* parent);

	QViewport* GetSecondaryViewport() { return m_pSecondaryViewport; }
	QViewport* GetPrimaryViewport()   { return m_pPrimaryViewport; }

	bool       IsSplit() const        { return m_bSplit; }
	void       SetSplit(bool bSplit);
	void       SetSplitDirection(ESplitDirection splitDirection);
	void       FixLayout();
private:
	void       ResetLayout();
protected slots:
	void       OnCameraMoved(const QuatT& m);
private:
	QViewport*      m_pSecondaryViewport;
	QViewport*      m_pPrimaryViewport;
	QBoxLayout*     m_pLayout;
	bool            m_bSplit;
	ESplitDirection m_splitDirection;
};

class CSplitViewportContainer : public QWidget
{
	Q_OBJECT
public:
	CSplitViewportContainer(QWidget* pParent = nullptr);

	CDisplayOptionsWidget* GetDisplayOptionsWidget();
	const SDisplayOptions& GetDisplayOptions() const;

	CSplitViewport*        GetSplitViewport();

	void                   SetHeaderWidget(QWidget* pHeader);

	// QWidget implementation.
	virtual QSize minimumSizeHint() const override;
private:
	CSplitViewport*        m_pSplitViewport;
	CDisplayOptionsWidget* m_pDisplayOptionsWidget;
	QHBoxLayout*           m_pHeaderLayout;
	QWidget*               m_pHeader;

	enum { kDefaultWidthOfDisplayOptionWidget = 200 };
};

