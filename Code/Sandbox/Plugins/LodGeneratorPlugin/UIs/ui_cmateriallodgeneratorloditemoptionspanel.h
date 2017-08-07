/********************************************************************************
** Form generated from reading UI file 'cmateriallodgeneratorloditemoptionspanel.ui'
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
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "QViewport.h"
#include "QViewportEvents.h"
#include "QViewportSettings.h"
#include "DisplayViewportAdapter.h"
#include "../control/MeshBakerTextureControl.h"

QT_BEGIN_NAMESPACE

class Ui_CMaterialLODGeneratorLodItemOptionsPanel
{
public:
    QVBoxLayout *verticalLayout_5;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_7;
    QViewport *CageView;
    QVBoxLayout *verticalLayout_6;
    QViewport *UVView;
    QVBoxLayout *verticalLayout_7;
    QHBoxLayout *horizontalLayout_6;
    QCheckBox *Bake_CheckBox;
    QSpacerItem *horizontalSpacer_3;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label;
    QLabel *TextureSize_Label;
    QSpacerItem *horizontalSpacer_2;
    QHBoxLayout *horizontalLayout_4;
    QSlider *TextureSize_Slider;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_4;
    QLabel *Slot_Label;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout;
    QLabel *label_6;
    QLabel *label_7;
    QLabel *label_8;
    QHBoxLayout *horizontalLayout_2;
    CCMeshBakerTextureControl *DiffuseView;
    QVBoxLayout *verticalLayout_2;
    CCMeshBakerTextureControl *NormalView;
    QVBoxLayout *verticalLayout_3;
    CCMeshBakerTextureControl *SpecularView;
    QVBoxLayout *verticalLayout_4;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *CMaterialLODGeneratorLodItemOptionsPanel)
    {
        if (CMaterialLODGeneratorLodItemOptionsPanel->objectName().isEmpty())
            CMaterialLODGeneratorLodItemOptionsPanel->setObjectName(QStringLiteral("CMaterialLODGeneratorLodItemOptionsPanel"));
        CMaterialLODGeneratorLodItemOptionsPanel->resize(457, 640);
        verticalLayout_5 = new QVBoxLayout(CMaterialLODGeneratorLodItemOptionsPanel);
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName(QStringLiteral("horizontalLayout_7"));
        CageView = new QViewport(gEnv,CMaterialLODGeneratorLodItemOptionsPanel);
        CageView->setObjectName(QStringLiteral("CageView"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(CageView->sizePolicy().hasHeightForWidth());
        CageView->setSizePolicy(sizePolicy);
        CageView->setMinimumSize(QSize(200, 200));
        verticalLayout_6 = new QVBoxLayout(CageView);
        verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));

        horizontalLayout_7->addWidget(CageView);

        UVView = new QViewport(gEnv,CMaterialLODGeneratorLodItemOptionsPanel);
        UVView->setObjectName(QStringLiteral("UVView"));
        UVView->setMinimumSize(QSize(200, 200));
        verticalLayout_7 = new QVBoxLayout(UVView);
        verticalLayout_7->setObjectName(QStringLiteral("verticalLayout_7"));

        horizontalLayout_7->addWidget(UVView);


        verticalLayout->addLayout(horizontalLayout_7);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName(QStringLiteral("horizontalLayout_6"));
        Bake_CheckBox = new QCheckBox(CMaterialLODGeneratorLodItemOptionsPanel);
        Bake_CheckBox->setObjectName(QStringLiteral("Bake_CheckBox"));
        QFont font;
        font.setPointSize(10);
        Bake_CheckBox->setFont(font);

        horizontalLayout_6->addWidget(Bake_CheckBox);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_3);


        verticalLayout->addLayout(horizontalLayout_6);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QStringLiteral("horizontalLayout_5"));
        label = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        label->setObjectName(QStringLiteral("label"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy1);
        QFont font1;
        font1.setPointSize(12);
        label->setFont(font1);

        horizontalLayout_5->addWidget(label);

        TextureSize_Label = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        TextureSize_Label->setObjectName(QStringLiteral("TextureSize_Label"));
        sizePolicy1.setHeightForWidth(TextureSize_Label->sizePolicy().hasHeightForWidth());
        TextureSize_Label->setSizePolicy(sizePolicy1);
        TextureSize_Label->setFont(font1);

        horizontalLayout_5->addWidget(TextureSize_Label);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout_5);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        horizontalLayout_4->setSizeConstraint(QLayout::SetMinimumSize);
        TextureSize_Slider = new QSlider(CMaterialLODGeneratorLodItemOptionsPanel);
        TextureSize_Slider->setObjectName(QStringLiteral("TextureSize_Slider"));
        TextureSize_Slider->setOrientation(Qt::Horizontal);

        horizontalLayout_4->addWidget(TextureSize_Slider);


        verticalLayout->addLayout(horizontalLayout_4);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        horizontalLayout_3->setSizeConstraint(QLayout::SetDefaultConstraint);
        label_4 = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        label_4->setObjectName(QStringLiteral("label_4"));
        sizePolicy1.setHeightForWidth(label_4->sizePolicy().hasHeightForWidth());
        label_4->setSizePolicy(sizePolicy1);
        label_4->setFont(font1);

        horizontalLayout_3->addWidget(label_4);

        Slot_Label = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        Slot_Label->setObjectName(QStringLiteral("Slot_Label"));
        sizePolicy1.setHeightForWidth(Slot_Label->sizePolicy().hasHeightForWidth());
        Slot_Label->setSizePolicy(sizePolicy1);
        Slot_Label->setFont(font1);

        horizontalLayout_3->addWidget(Slot_Label);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        label_6 = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        label_6->setObjectName(QStringLiteral("label_6"));
        sizePolicy1.setHeightForWidth(label_6->sizePolicy().hasHeightForWidth());
        label_6->setSizePolicy(sizePolicy1);
        label_6->setFont(font1);

        horizontalLayout->addWidget(label_6);

        label_7 = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        label_7->setObjectName(QStringLiteral("label_7"));
        sizePolicy1.setHeightForWidth(label_7->sizePolicy().hasHeightForWidth());
        label_7->setSizePolicy(sizePolicy1);
        label_7->setFont(font1);

        horizontalLayout->addWidget(label_7);

        label_8 = new QLabel(CMaterialLODGeneratorLodItemOptionsPanel);
        label_8->setObjectName(QStringLiteral("label_8"));
        sizePolicy1.setHeightForWidth(label_8->sizePolicy().hasHeightForWidth());
        label_8->setSizePolicy(sizePolicy1);
        label_8->setFont(font1);

        horizontalLayout->addWidget(label_8);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        DiffuseView = new CCMeshBakerTextureControl(CMaterialLODGeneratorLodItemOptionsPanel);
        DiffuseView->setObjectName(QStringLiteral("DiffuseView"));
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(DiffuseView->sizePolicy().hasHeightForWidth());
        DiffuseView->setSizePolicy(sizePolicy2);
        DiffuseView->setMinimumSize(QSize(141, 141));
        verticalLayout_2 = new QVBoxLayout(DiffuseView);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));

        horizontalLayout_2->addWidget(DiffuseView);

        NormalView = new CCMeshBakerTextureControl(CMaterialLODGeneratorLodItemOptionsPanel);
        NormalView->setObjectName(QStringLiteral("NormalView"));
        sizePolicy2.setHeightForWidth(NormalView->sizePolicy().hasHeightForWidth());
        NormalView->setSizePolicy(sizePolicy2);
        NormalView->setMinimumSize(QSize(141, 141));
        verticalLayout_3 = new QVBoxLayout(NormalView);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));

        horizontalLayout_2->addWidget(NormalView);

        SpecularView = new CCMeshBakerTextureControl(CMaterialLODGeneratorLodItemOptionsPanel);
        SpecularView->setObjectName(QStringLiteral("SpecularView"));
        sizePolicy2.setHeightForWidth(SpecularView->sizePolicy().hasHeightForWidth());
        SpecularView->setSizePolicy(sizePolicy2);
        SpecularView->setMinimumSize(QSize(141, 141));
        verticalLayout_4 = new QVBoxLayout(SpecularView);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));

        horizontalLayout_2->addWidget(SpecularView);


        verticalLayout->addLayout(horizontalLayout_2);



        verticalLayout_5->addLayout(verticalLayout);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_5->addItem(verticalSpacer);


		//-----------------------------------------------------------------------------------------

		SViewportState viewState = UVView->GetState();
		viewState.cameraTarget.SetFromVectors(Vec3(1,0,0), Vec3(0,0,-1), Vec3(0,1,0), Vec3(0.5f,0.5f,75.0f));
		UVView->SetState(viewState);

		SViewportSettings viewportSettings = UVView->GetSettings();
		viewportSettings.rendering.fps = false;
		viewportSettings.grid.showGrid = true;
		viewportSettings.grid.spacing  = 0.5f;
		viewportSettings.camera.moveSpeed = 0.012f;
		viewportSettings.camera.zoomSpeed = 1.25f;
		viewportSettings.camera.showViewportOrientation = false;
		viewportSettings.camera.transformRestraint = eCameraTransformRestraint_Rotation;
		viewportSettings.camera.fov = 1;
		UVView->SetSettings(viewportSettings);
		UVView->SetSceneDimensions(Vec3(10000.0f, 10000.0f, 10000.0f));	

		//----------------------------------------------------------------------------------------

        retranslateUi(CMaterialLODGeneratorLodItemOptionsPanel);

        QMetaObject::connectSlotsByName(CMaterialLODGeneratorLodItemOptionsPanel);
    } // setupUi

    void retranslateUi(QWidget *CMaterialLODGeneratorLodItemOptionsPanel)
    {
        CMaterialLODGeneratorLodItemOptionsPanel->setWindowTitle(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Form", 0));
        Bake_CheckBox->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Bake this LODs Materials", 0));
        label->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Out put Texture Size:", 0));
        TextureSize_Label->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "1024X1024", 0));
        label_4->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Sub-Material Slot:", 0));
        Slot_Label->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "6", 0));
        label_6->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Diffuse:", 0));
        label_7->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Normal:", 0));
        label_8->setText(QApplication::translate("CMaterialLODGeneratorLodItemOptionsPanel", "Specular:", 0));
    } // retranslateUi

};

namespace Ui {
    class CMaterialLODGeneratorLodItemOptionsPanel: public Ui_CMaterialLODGeneratorLodItemOptionsPanel {};
} // namespace Ui

QT_END_NAMESPACE
