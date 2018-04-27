/********************************************************************************
** Form generated from reading UI file 'cgeometrylodgeneratorpreviewpanel.ui'
**
** Created by: Qt User Interface Compiler version 5.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#pragma once

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "QViewport.h"
#include "QViewportEvents.h"
#include "QViewportSettings.h"
#include "DisplayViewportAdapter.h"
#include "../control/RampControl.h"

QT_BEGIN_NAMESPACE

class Ui_CGeometryLodGeneratorPreviewPanel
{
public:
    QVBoxLayout *verticalLayout_4;
    QVBoxLayout *verticalLayout;
    QViewport *LodGenPreviewWidget;
    QVBoxLayout *verticalLayout_3;
    CCRampControl *LodChainRampWidget;
    QVBoxLayout *verticalLayout_2;
    QPushButton *LodGenUVMapAndSaveButton;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *CGeometryLodGeneratorPreviewPanel)
    {
        if (CGeometryLodGeneratorPreviewPanel->objectName().isEmpty())
            CGeometryLodGeneratorPreviewPanel->setObjectName(QStringLiteral("CGeometryLodGeneratorPreviewPanel"));
        CGeometryLodGeneratorPreviewPanel->resize(587, 627);
        verticalLayout_4 = new QVBoxLayout(CGeometryLodGeneratorPreviewPanel);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));

        LodGenPreviewWidget = new QViewport(gEnv,CGeometryLodGeneratorPreviewPanel);
        LodGenPreviewWidget->setObjectName(QStringLiteral("LodGenPreviewWidget"));

        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(LodGenPreviewWidget->sizePolicy().hasHeightForWidth());
        LodGenPreviewWidget->setSizePolicy(sizePolicy);
        LodGenPreviewWidget->setMinimumSize(QSize(0, 430));
        LodGenPreviewWidget->setBaseSize(QSize(0, 0));
        verticalLayout_3 = new QVBoxLayout(LodGenPreviewWidget);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));

        verticalLayout->addWidget(LodGenPreviewWidget);

        LodChainRampWidget = new CCRampControl(CGeometryLodGeneratorPreviewPanel);
        LodChainRampWidget->setObjectName(QStringLiteral("LodChainRampWidget"));
        sizePolicy.setHeightForWidth(LodChainRampWidget->sizePolicy().hasHeightForWidth());
        LodChainRampWidget->setSizePolicy(sizePolicy);
        LodChainRampWidget->setMinimumSize(QSize(0, 60));
        verticalLayout_2 = new QVBoxLayout(LodChainRampWidget);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));

        verticalLayout->addWidget(LodChainRampWidget);

        LodGenUVMapAndSaveButton = new QPushButton(CGeometryLodGeneratorPreviewPanel);
        LodGenUVMapAndSaveButton->setObjectName(QStringLiteral("LodGenUVMapAndSaveButton"));

        verticalLayout->addWidget(LodGenUVMapAndSaveButton);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        verticalLayout_4->addLayout(verticalLayout);

		//LodGenPreviewWidget->AddConsumer(m_pGizmo.get());

		//m_pGizmo->AddCallback(this);

		SViewportState viewState = LodGenPreviewWidget->GetState();
		viewState.cameraTarget.SetFromVectors(Vec3(1,0,0), Vec3(0,0,-1), Vec3(0,1,0), Vec3(0.5f,0.5f,75.0f));
		LodGenPreviewWidget->SetState(viewState);

		SViewportSettings viewportSettings = LodGenPreviewWidget->GetSettings();
		viewportSettings.rendering.fps = false;
		viewportSettings.grid.showGrid = true;
		viewportSettings.grid.spacing  = 0.5f;
		viewportSettings.camera.moveSpeed = 0.012f;
		viewportSettings.camera.zoomSpeed = 12.5f;
		viewportSettings.camera.showViewportOrientation = false;
		viewportSettings.camera.transformRestraint = eCameraTransformRestraint_Rotation;
		viewportSettings.camera.fov = 1;
		LodGenPreviewWidget->SetSettings(viewportSettings);
		LodGenPreviewWidget->SetSceneDimensions(Vec3(100.0f, 100.0f, 100.0f));	

        retranslateUi(CGeometryLodGeneratorPreviewPanel);

        QMetaObject::connectSlotsByName(CGeometryLodGeneratorPreviewPanel);
    } // setupUi

    void retranslateUi(QWidget *CGeometryLodGeneratorPreviewPanel)
    {
        CGeometryLodGeneratorPreviewPanel->setWindowTitle(QApplication::translate("CGeometryLodGeneratorPreviewPanel", "Form", 0));
        LodGenUVMapAndSaveButton->setText(QApplication::translate("CGeometryLodGeneratorPreviewPanel", "Generate Geometry Files", 0));
    } // retranslateUi

};

namespace Ui {
    class CGeometryLodGeneratorPreviewPanel: public Ui_CGeometryLodGeneratorPreviewPanel {};
} // namespace Ui

QT_END_NAMESPACE

