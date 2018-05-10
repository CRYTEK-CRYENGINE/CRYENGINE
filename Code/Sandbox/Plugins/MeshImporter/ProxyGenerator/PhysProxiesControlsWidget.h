// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include "ProxyGenerator.h"

struct SPhysProxies;

class QMenuComboBox;

class QGroupBox;
class QCheckBox;
class QLabel;
class QPushButton;

class CPhysProxiesControlsWidget : public QWidget
{
	Q_OBJECT
public:
	CPhysProxiesControlsWidget(QWidget* pParent = nullptr);

	void OnProxyIslandsChanged(const SPhysProxies* pProx);

	CProxyGenerator::EVisibility GetSourceVisibility() const;
	CProxyGenerator::EVisibility GetMeshesVisibility() const;
	CProxyGenerator::EVisibility GetPrimitivesVisibility() const;
	bool IsVoxelsVisible() const;
signals:
	void SigCloseHoles();
	void SigReopenHoles();
	void SigSelectAll();
	void SigSelectNone();
	void SigGenerateProxies();
	void SigResetProxies();
private:
	void SetupUI();

	QGroupBox*                             m_pPhysProxies;
	QPushButton*                           m_pCloseHoles;
	QPushButton*                           m_pReopenHoles;
	QPushButton*                           m_pGenerateProxies;
	QLabel*                                m_pLabel;
	QMenuComboBox*                             m_pShowSrc;
	QMenuComboBox*                             m_pShowMeshes;
	QMenuComboBox*                             m_pShowPrims;
	QMenuComboBox*                             m_pShowVoxels;
	QPushButton*                           m_pSelectAll;
	QPushButton*                           m_pSelectNone;
	QPushButton*                           m_pResetProxies;
};
