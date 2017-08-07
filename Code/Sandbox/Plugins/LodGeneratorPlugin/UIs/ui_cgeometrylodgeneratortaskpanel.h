/********************************************************************************
** Form generated from reading UI file 'cgeometrylodgeneratortaskpanel.ui'
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
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CGeometryLodGeneratorTaskPanel
{
public:
    QHBoxLayout *horizontalLayout_4;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *GenerateLodChainButton;
    QPushButton *CanelGenerateLodChainButton;
    QHBoxLayout *horizontalLayout;
    QProgressBar *GenerateLodChainButtonProgressBar;
    QHBoxLayout *horizontalLayout_2;
    QLabel *GenerateLodChainButtonLabel;

    void setupUi(QWidget *CGeometryLodGeneratorTaskPanel)
    {
        if (CGeometryLodGeneratorTaskPanel->objectName().isEmpty())
            CGeometryLodGeneratorTaskPanel->setObjectName(QStringLiteral("CGeometryLodGeneratorTaskPanel"));
        CGeometryLodGeneratorTaskPanel->resize(543, 141);
        horizontalLayout_4 = new QHBoxLayout(CGeometryLodGeneratorTaskPanel);
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        GenerateLodChainButton = new QPushButton(CGeometryLodGeneratorTaskPanel);
        GenerateLodChainButton->setObjectName(QStringLiteral("GenerateLodChainButton"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(GenerateLodChainButton->sizePolicy().hasHeightForWidth());
        GenerateLodChainButton->setSizePolicy(sizePolicy);

        horizontalLayout_3->addWidget(GenerateLodChainButton);

        CanelGenerateLodChainButton = new QPushButton(CGeometryLodGeneratorTaskPanel);
        CanelGenerateLodChainButton->setObjectName(QStringLiteral("CanelGenerateLodChainButton"));
        CanelGenerateLodChainButton->setEnabled(true);
        sizePolicy.setHeightForWidth(CanelGenerateLodChainButton->sizePolicy().hasHeightForWidth());
        CanelGenerateLodChainButton->setSizePolicy(sizePolicy);

        horizontalLayout_3->addWidget(CanelGenerateLodChainButton);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        GenerateLodChainButtonProgressBar = new QProgressBar(CGeometryLodGeneratorTaskPanel);
        GenerateLodChainButtonProgressBar->setObjectName(QStringLiteral("GenerateLodChainButtonProgressBar"));
        GenerateLodChainButtonProgressBar->setValue(24);

        horizontalLayout->addWidget(GenerateLodChainButtonProgressBar);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        GenerateLodChainButtonLabel = new QLabel(CGeometryLodGeneratorTaskPanel);
        GenerateLodChainButtonLabel->setObjectName(QStringLiteral("GenerateLodChainButtonLabel"));
        QFont font;
        font.setPointSize(13);
        GenerateLodChainButtonLabel->setFont(font);
        GenerateLodChainButtonLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_2->addWidget(GenerateLodChainButtonLabel);


        verticalLayout->addLayout(horizontalLayout_2);


        horizontalLayout_4->addLayout(verticalLayout);


        retranslateUi(CGeometryLodGeneratorTaskPanel);

        QMetaObject::connectSlotsByName(CGeometryLodGeneratorTaskPanel);
    } // setupUi

    void retranslateUi(QWidget *CGeometryLodGeneratorTaskPanel)
    {
        CGeometryLodGeneratorTaskPanel->setWindowTitle(QApplication::translate("CGeometryLodGeneratorTaskPanel", "Form", 0));
        GenerateLodChainButton->setText(QApplication::translate("CGeometryLodGeneratorTaskPanel", "Generate LOD Chain", 0));
        CanelGenerateLodChainButton->setText(QApplication::translate("CGeometryLodGeneratorTaskPanel", "Canel Generation", 0));
        GenerateLodChainButtonLabel->setText(QApplication::translate("CGeometryLodGeneratorTaskPanel", "Waiting for task..", 0));
    } // retranslateUi

};

namespace Ui {
    class CGeometryLodGeneratorTaskPanel: public Ui_CGeometryLodGeneratorTaskPanel {};
} // namespace Ui

QT_END_NAMESPACE

