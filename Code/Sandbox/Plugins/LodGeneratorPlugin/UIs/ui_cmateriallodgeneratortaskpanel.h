/********************************************************************************
** Form generated from reading UI file 'cmateriallodgeneratortaskpanel.ui'
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
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CMaterialLODGeneratorTaskPanel
{
public:
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout;
    QPushButton *GenerateTextures_Button;

    void setupUi(QWidget *CMaterialLODGeneratorTaskPanel)
    {
        if (CMaterialLODGeneratorTaskPanel->objectName().isEmpty())
            CMaterialLODGeneratorTaskPanel->setObjectName(QStringLiteral("CMaterialLODGeneratorTaskPanel"));
        CMaterialLODGeneratorTaskPanel->resize(489, 43);
        verticalLayout_2 = new QVBoxLayout(CMaterialLODGeneratorTaskPanel);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        GenerateTextures_Button = new QPushButton(CMaterialLODGeneratorTaskPanel);
        GenerateTextures_Button->setObjectName(QStringLiteral("GenerateTextures_Button"));

        verticalLayout->addWidget(GenerateTextures_Button);


        verticalLayout_2->addLayout(verticalLayout);


        retranslateUi(CMaterialLODGeneratorTaskPanel);

        QMetaObject::connectSlotsByName(CMaterialLODGeneratorTaskPanel);
    } // setupUi

    void retranslateUi(QWidget *CMaterialLODGeneratorTaskPanel)
    {
        CMaterialLODGeneratorTaskPanel->setWindowTitle(QApplication::translate("CMaterialLODGeneratorTaskPanel", "Form", 0));
        GenerateTextures_Button->setText(QApplication::translate("CMaterialLODGeneratorTaskPanel", "Generate Textures", 0));
    } // retranslateUi

};

namespace Ui {
    class CMaterialLODGeneratorTaskPanel: public Ui_CMaterialLODGeneratorTaskPanel {};
} // namespace Ui

QT_END_NAMESPACE

