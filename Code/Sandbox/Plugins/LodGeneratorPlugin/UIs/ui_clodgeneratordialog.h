/********************************************************************************
** Form generated from reading UI file 'clodgeneratordialog.ui'
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
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "../panel/GeometryLodGeneratorOptionsPanel.h"
#include "../panel/GeometryLodGeneratorPreviewPanel.h"
#include "../panel/GeometryLodGeneratorTaskPanel.h"
#include "../panel/LodGeneratorFilePanel.h"
#include "../panel/MaterialLODGeneratorOptionsPanel.h"
#include "../panel/MaterialLODGeneratorLodItemOptionsPanel.h"
#include "../panel/MaterialLODGeneratorTaskPanel.h"
#include "../panel/ABFPannel.h"
#include "../panel/ABFPannelNew.h"


QT_BEGIN_NAMESPACE

class Ui_CLodGeneratorDialog
{
public:
    QWidget *centralWidget;
    QHBoxLayout *horizontalLayout;
    QScrollArea *scrollArea;
    QWidget *LodGeneratorToolContents;
    QHBoxLayout *horizontalLayout_2;
    QToolBox *LodGeneratorToolBox;
    QWidget *SourceFileWidget;
    QVBoxLayout *verticalLayout_3;
    CLodGeneratorFilePanel *cSourceFileWidget;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *verticalSpacer;
    QWidget *GeometryBakeOptonsWidge;
    QVBoxLayout *verticalLayout_2;
    CGeometryLodGeneratorOptionsPanel *cGeometryBakeOptonsWidge;
    QVBoxLayout *verticalLayout;
    QWidget *GeometryTaskPanelWidget;
    QVBoxLayout *verticalLayout_4;
    CGeometryLodGeneratorTaskPanel *cGeometryTaskPanelWidget;
    QVBoxLayout *verticalLayout_5;
    QSpacerItem *verticalSpacer_2;
    QWidget *GeometryGenerationPanelWidget;
    QVBoxLayout *verticalLayout_6;
    CGeometryLodGeneratorPreviewPanel *cGeometryGenerationPanelWidget;
    QVBoxLayout *verticalLayout_7;
    QWidget *MaterialBakeOptionsWidget;
    QVBoxLayout *verticalLayout_10;
    CMaterialLODGeneratorOptionsPanel *cMaterialBakeOptionsWidget;
    QVBoxLayout *verticalLayout_8;
    QWidget *MaterialTaskWidget;
    QVBoxLayout *verticalLayout_11;
    CMaterialLODGeneratorTaskPanel *cMaterialTaskWidget;
    QVBoxLayout *verticalLayout_9;
    QSpacerItem *verticalSpacer_3;
    QWidget *TestABF;
    QVBoxLayout *verticalLayout_13;
    cABFPannel *cABFWidget;
    QVBoxLayout *verticalLayout_14;
    QSpacerItem *verticalSpacer_4;
	std::vector<int> m_vLodPanelsIdx;
	std::vector<CMaterialLODGeneratorLodItemOptionsPanel*> m_vLodPanels;
	std::vector<QWidget*> m_vLodPanelParents;
    QWidget *TestABFNew;
    QVBoxLayout *verticalLayout_16;
    cABFPannelNew *cABFWidgetNew;
    QVBoxLayout *verticalLayout_15;
    QSpacerItem *verticalSpacer_5;

    void setupUi(QWidget *CLodGeneratorDialog)
    {
        if (CLodGeneratorDialog->objectName().isEmpty())
            CLodGeneratorDialog->setObjectName(QStringLiteral("CLodGeneratorDialog"));
        CLodGeneratorDialog->resize(457, 593);
        centralWidget = new QWidget(CLodGeneratorDialog);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        horizontalLayout = new QHBoxLayout(centralWidget);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        scrollArea = new QScrollArea(centralWidget);
        scrollArea->setObjectName(QStringLiteral("scrollArea"));
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setFrameShadow(QFrame::Raised);
        scrollArea->setWidgetResizable(true);
        LodGeneratorToolContents = new QWidget();
        LodGeneratorToolContents->setObjectName(QStringLiteral("LodGeneratorToolContents"));
        LodGeneratorToolContents->setGeometry(QRect(0, 0, 457, 593));
        horizontalLayout_2 = new QHBoxLayout(LodGeneratorToolContents);
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(9, 9, 9, 9);
        LodGeneratorToolBox = new QToolBox(LodGeneratorToolContents);
        LodGeneratorToolBox->setObjectName(QStringLiteral("LodGeneratorToolBox"));
        LodGeneratorToolBox->setFrameShape(QFrame::Panel);
        SourceFileWidget = new QWidget();
        SourceFileWidget->setObjectName(QStringLiteral("SourceFileWidget"));
        SourceFileWidget->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_3 = new QVBoxLayout(SourceFileWidget);
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setContentsMargins(11, 11, 11, 11);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        cSourceFileWidget = new CLodGeneratorFilePanel(SourceFileWidget);
        cSourceFileWidget->setObjectName(QStringLiteral("cSourceFileWidget"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(cSourceFileWidget->sizePolicy().hasHeightForWidth());
        cSourceFileWidget->setSizePolicy(sizePolicy);
        horizontalLayout_3 = new QHBoxLayout(cSourceFileWidget);
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(0, 0, 0, 0);

        verticalLayout_3->addWidget(cSourceFileWidget);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);

        LodGeneratorToolBox->addItem(SourceFileWidget, QStringLiteral("SourceFile"));
        GeometryBakeOptonsWidge = new QWidget();
        GeometryBakeOptonsWidge->setObjectName(QStringLiteral("GeometryBakeOptonsWidge"));
        GeometryBakeOptonsWidge->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_2 = new QVBoxLayout(GeometryBakeOptonsWidge);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        cGeometryBakeOptonsWidge = new CGeometryLodGeneratorOptionsPanel(GeometryBakeOptonsWidge);
        cGeometryBakeOptonsWidge->setObjectName(QStringLiteral("cGeometryBakeOptonsWidge"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(cGeometryBakeOptonsWidge->sizePolicy().hasHeightForWidth());
        cGeometryBakeOptonsWidge->setSizePolicy(sizePolicy1);
        verticalLayout = new QVBoxLayout(cGeometryBakeOptonsWidge);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);

        verticalLayout_2->addWidget(cGeometryBakeOptonsWidge);

        LodGeneratorToolBox->addItem(GeometryBakeOptonsWidge, QStringLiteral("GeometryBakeOptons"));
        GeometryTaskPanelWidget = new QWidget();
        GeometryTaskPanelWidget->setObjectName(QStringLiteral("GeometryTaskPanelWidget"));
        GeometryTaskPanelWidget->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_4 = new QVBoxLayout(GeometryTaskPanelWidget);
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setContentsMargins(11, 11, 11, 11);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        cGeometryTaskPanelWidget = new CGeometryLodGeneratorTaskPanel(GeometryTaskPanelWidget);
        cGeometryTaskPanelWidget->setObjectName(QStringLiteral("cGeometryTaskPanelWidget"));
        verticalLayout_5 = new QVBoxLayout(cGeometryTaskPanelWidget);
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setContentsMargins(11, 11, 11, 11);
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);

        verticalLayout_4->addWidget(cGeometryTaskPanelWidget);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_4->addItem(verticalSpacer_2);

        LodGeneratorToolBox->addItem(GeometryTaskPanelWidget, QStringLiteral("GeometryTaskPanel"));
        GeometryGenerationPanelWidget = new QWidget();
        GeometryGenerationPanelWidget->setObjectName(QStringLiteral("GeometryGenerationPanelWidget"));
        GeometryGenerationPanelWidget->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_6 = new QVBoxLayout(GeometryGenerationPanelWidget);
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setContentsMargins(11, 11, 11, 11);
        verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
        verticalLayout_6->setContentsMargins(0, 0, 0, 0);
        cGeometryGenerationPanelWidget = new CGeometryLodGeneratorPreviewPanel(GeometryGenerationPanelWidget);
        cGeometryGenerationPanelWidget->setObjectName(QStringLiteral("cGeometryGenerationPanelWidget"));
        verticalLayout_7 = new QVBoxLayout(cGeometryGenerationPanelWidget);
        verticalLayout_7->setSpacing(6);
        verticalLayout_7->setContentsMargins(11, 11, 11, 11);
        verticalLayout_7->setObjectName(QStringLiteral("verticalLayout_7"));
        verticalLayout_7->setContentsMargins(0, 0, 0, 0);

        verticalLayout_6->addWidget(cGeometryGenerationPanelWidget);

        LodGeneratorToolBox->addItem(GeometryGenerationPanelWidget, QStringLiteral("GeometryGenerationPanel"));
        MaterialBakeOptionsWidget = new QWidget();
        MaterialBakeOptionsWidget->setObjectName(QStringLiteral("MaterialBakeOptionsWidget"));
        MaterialBakeOptionsWidget->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_10 = new QVBoxLayout(MaterialBakeOptionsWidget);
        verticalLayout_10->setSpacing(6);
        verticalLayout_10->setContentsMargins(11, 11, 11, 11);
        verticalLayout_10->setObjectName(QStringLiteral("verticalLayout_10"));
        verticalLayout_10->setContentsMargins(0, 0, 0, 0);
        cMaterialBakeOptionsWidget = new CMaterialLODGeneratorOptionsPanel(MaterialBakeOptionsWidget);
        cMaterialBakeOptionsWidget->setObjectName(QStringLiteral("cMaterialBakeOptionsWidget"));
        verticalLayout_8 = new QVBoxLayout(cMaterialBakeOptionsWidget);
        verticalLayout_8->setSpacing(6);
        verticalLayout_8->setContentsMargins(11, 11, 11, 11);
        verticalLayout_8->setObjectName(QStringLiteral("verticalLayout_8"));
        verticalLayout_8->setContentsMargins(0, 0, 0, 0);

        verticalLayout_10->addWidget(cMaterialBakeOptionsWidget);

        LodGeneratorToolBox->addItem(MaterialBakeOptionsWidget, QStringLiteral("MaterialBakeOptions"));
        MaterialTaskWidget = new QWidget();
        MaterialTaskWidget->setObjectName(QStringLiteral("MaterialTaskWidget"));
        MaterialTaskWidget->setGeometry(QRect(0, 0, 437, 411));
        verticalLayout_11 = new QVBoxLayout(MaterialTaskWidget);
        verticalLayout_11->setSpacing(6);
        verticalLayout_11->setContentsMargins(11, 11, 11, 11);
        verticalLayout_11->setObjectName(QStringLiteral("verticalLayout_11"));
        verticalLayout_11->setContentsMargins(0, 0, 0, 0);
        cMaterialTaskWidget = new CMaterialLODGeneratorTaskPanel(MaterialTaskWidget);
        cMaterialTaskWidget->setObjectName(QStringLiteral("cMaterialTaskWidget"));
        verticalLayout_9 = new QVBoxLayout(cMaterialTaskWidget);
        verticalLayout_9->setSpacing(6);
        verticalLayout_9->setContentsMargins(11, 11, 11, 11);
        verticalLayout_9->setObjectName(QStringLiteral("verticalLayout_9"));
        verticalLayout_9->setContentsMargins(0, 0, 0, 0);

        verticalLayout_11->addWidget(cMaterialTaskWidget);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_11->addItem(verticalSpacer_3);

        LodGeneratorToolBox->addItem(MaterialTaskWidget, QStringLiteral("MaterialGenerationPanel"));

        horizontalLayout_2->addWidget(LodGeneratorToolBox);

        scrollArea->setWidget(LodGeneratorToolContents);

        horizontalLayout->addWidget(scrollArea);
		CLodGeneratorDialog->layout()->addWidget(centralWidget);
        //CLodGeneratorDialog->setCentralWidget(centralWidget);

        retranslateUi(CLodGeneratorDialog);

        LodGeneratorToolBox->setCurrentIndex(5);


        QMetaObject::connectSlotsByName(CLodGeneratorDialog);
    } // setupUi

    void retranslateUi(QWidget *CLodGeneratorDialog)
    {
        CLodGeneratorDialog->setWindowTitle(QApplication::translate("CLodGeneratorDialog", "CLodGeneratorDialog", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(SourceFileWidget), QApplication::translate("CLodGeneratorDialog", "SourceFile", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(GeometryBakeOptonsWidge), QApplication::translate("CLodGeneratorDialog", "GeometryBakeOptons", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(GeometryTaskPanelWidget), QApplication::translate("CLodGeneratorDialog", "GeometryTaskPanel", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(GeometryGenerationPanelWidget), QApplication::translate("CLodGeneratorDialog", "GeometryGenerationPanel", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(MaterialBakeOptionsWidget), QApplication::translate("CLodGeneratorDialog", "MaterialBakeOptions", 0));
        LodGeneratorToolBox->setItemText(LodGeneratorToolBox->indexOf(MaterialTaskWidget), QApplication::translate("CLodGeneratorDialog", "MaterialGenerationPanel", 0));
    } // retranslateUi

};

namespace Ui {
    class CLodGeneratorDialog: public Ui_CLodGeneratorDialog {};
} // namespace Ui

QT_END_NAMESPACE

