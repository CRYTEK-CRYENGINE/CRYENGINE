/********************************************************************************
** Form generated from reading UI file 'cabfpannel.ui'
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
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "QViewport.h"
#include "QViewportEvents.h"
#include "QViewportSettings.h"
#include "DisplayViewportAdapter.h"

QT_BEGIN_NAMESPACE

class Ui_cABFPannelNew
{
public:
    QVBoxLayout *verticalLayout;
    QPushButton *TestDecimator;
    QPushButton *TestABFButton;
    QHBoxLayout *horizontalLayout_2;
    QViewport *UVViwer;
    QVBoxLayout *verticalLayout_2;
    QViewport *MeshViwer;
    QVBoxLayout *verticalLayout_3;

    void setupUi(QWidget *cABFPannel)
    {
        if (cABFPannel->objectName().isEmpty())
            cABFPannel->setObjectName(QStringLiteral("cABFPannel"));
        cABFPannel->resize(668, 514);
        verticalLayout = new QVBoxLayout(cABFPannel);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        TestDecimator = new QPushButton(cABFPannel);
        TestDecimator->setObjectName(QStringLiteral("TestDecimator"));

        verticalLayout->addWidget(TestDecimator);

        TestABFButton = new QPushButton(cABFPannel);
        TestABFButton->setObjectName(QStringLiteral("TestABFButton"));

        verticalLayout->addWidget(TestABFButton);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        UVViwer = new QViewport(gEnv,cABFPannel);
        UVViwer->setObjectName(QStringLiteral("UVViwer"));
        verticalLayout_2 = new QVBoxLayout(UVViwer);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));

        horizontalLayout_2->addWidget(UVViwer);

		MeshViwer = new QViewport(gEnv,cABFPannel);
		MeshViwer->setObjectName(QStringLiteral("MeshViwer"));
		verticalLayout_3 = new QVBoxLayout(MeshViwer);
		verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));

        horizontalLayout_2->addWidget(MeshViwer);

		//-----------------------------------------------------------------------------------------

		SViewportState viewState = UVViwer->GetState();
		viewState.cameraTarget.SetFromVectors(Vec3(1,0,0), Vec3(0,0,-1), Vec3(0,1,0), Vec3(0.5f,0.5f,75.0f));
		UVViwer->SetState(viewState);

		SViewportSettings viewportSettings = UVViwer->GetSettings();
		viewportSettings.rendering.fps = false;
		viewportSettings.grid.showGrid = true;
		viewportSettings.grid.spacing  = 0.5f;
		viewportSettings.camera.moveSpeed = 0.002f;
		viewportSettings.camera.zoomSpeed = 1.5f;
		viewportSettings.camera.showViewportOrientation = false;
		viewportSettings.camera.transformRestraint = eCameraTransformRestraint_Rotation;
		viewportSettings.camera.fov = 1;
		UVViwer->SetSettings(viewportSettings);
		UVViwer->SetSceneDimensions(Vec3(100.0f, 100.0f, 100.0f));	

// 		SViewportState viewState2 = MeshViwer->GetState();
// 		viewState2.cameraTarget.SetFromVectors(Vec3(1,0,0), Vec3(0,0,-1), Vec3(0,1,0), Vec3(0.5f,0.5f,175.0f));
// 		UVViwer->SetState(viewState2);
// 
// 		SViewportSettings viewportSettings2 = MeshViwer->GetSettings();
// 		viewportSettings2.rendering.fps = false;
// 		viewportSettings2.grid.showGrid = false;
// 		viewportSettings2.grid.spacing  = 0.5f;
// 		viewportSettings2.camera.moveSpeed = 0.012f;
// 		viewportSettings2.camera.zoomSpeed = 12.5f;
// 		viewportSettings2.camera.showViewportOrientation = false;
// 		viewportSettings2.camera.transformRestraint = eCameraTransformRestraint_Rotation;
// 		viewportSettings2.camera.fov = 1;
// 		MeshViwer->SetSettings(viewportSettings2);
// 		MeshViwer->SetSceneDimensions(Vec3(500.0f, 500.0f, 500.0f));	

		//-----------------------------------------------------------------------------------------


        verticalLayout->addLayout(horizontalLayout_2);
        retranslateUi(cABFPannel);

        QMetaObject::connectSlotsByName(cABFPannel);
    } // setupUi

    void retranslateUi(QWidget *cABFPannel)
    {
        cABFPannel->setWindowTitle(QApplication::translate("cABFPannel", "Form", 0));
        TestDecimator->setText(QApplication::translate("cABFPannel", "TestDecimator", 0));
        TestABFButton->setText(QApplication::translate("cABFPannel", "TestABF", 0));
    } // retranslateUi

};

namespace Ui {
    class cABFPannelNew: public Ui_cABFPannelNew {};
} // namespace Ui

QT_END_NAMESPACE

