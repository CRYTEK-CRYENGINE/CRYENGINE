/********************************************************************************
** Form generated from reading UI file 'singlegeneratepanel.ui'
**
** Created by: Qt User Interface Compiler version 5.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SINGLEGENERATEPANEL_H
#define UI_SINGLEGENERATEPANEL_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SingleGeneratePanel
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QPushButton *SelectObj;
    QPushButton *Prepare;
    QPushButton *AddLod;
    QLineEdit *LodIndex;
    QPushButton *GenerateMesh;
    QPushButton *GenerateTexture;
    QPushButton *SingleAutoGenerate;
    QProgressBar *GenerateProgress;

    void setupUi(QWidget *SingleGeneratePanel)
    {
        if (SingleGeneratePanel->objectName().isEmpty())
            SingleGeneratePanel->setObjectName(QStringLiteral("SingleGeneratePanel"));
        SingleGeneratePanel->resize(608, 70);
        verticalLayout = new QVBoxLayout(SingleGeneratePanel);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        SelectObj = new QPushButton(SingleGeneratePanel);
        SelectObj->setObjectName(QStringLiteral("SelectObj"));

        horizontalLayout->addWidget(SelectObj);

        Prepare = new QPushButton(SingleGeneratePanel);
        Prepare->setObjectName(QStringLiteral("Prepare"));

        horizontalLayout->addWidget(Prepare);

        AddLod = new QPushButton(SingleGeneratePanel);
        AddLod->setObjectName(QStringLiteral("AddLod"));

        horizontalLayout->addWidget(AddLod);

        LodIndex = new QLineEdit(SingleGeneratePanel);
        LodIndex->setObjectName(QStringLiteral("LodIndex"));

        horizontalLayout->addWidget(LodIndex);

        GenerateMesh = new QPushButton(SingleGeneratePanel);
        GenerateMesh->setObjectName(QStringLiteral("GenerateMesh"));

        horizontalLayout->addWidget(GenerateMesh);

        GenerateTexture = new QPushButton(SingleGeneratePanel);
        GenerateTexture->setObjectName(QStringLiteral("GenerateTexture"));

        horizontalLayout->addWidget(GenerateTexture);

        SingleAutoGenerate = new QPushButton(SingleGeneratePanel);
        SingleAutoGenerate->setObjectName(QStringLiteral("SingleAutoGenerate"));

        horizontalLayout->addWidget(SingleAutoGenerate);


        verticalLayout->addLayout(horizontalLayout);

        GenerateProgress = new QProgressBar(SingleGeneratePanel);
        GenerateProgress->setObjectName(QStringLiteral("GenerateProgress"));
        GenerateProgress->setValue(24);

        verticalLayout->addWidget(GenerateProgress);


        retranslateUi(SingleGeneratePanel);

        QMetaObject::connectSlotsByName(SingleGeneratePanel);
    } // setupUi

    void retranslateUi(QWidget *SingleGeneratePanel)
    {
        SingleGeneratePanel->setWindowTitle(QApplication::translate("SingleGeneratePanel", "Form", 0));
        SelectObj->setText(QApplication::translate("SingleGeneratePanel", "<", 0));
        Prepare->setText(QApplication::translate("SingleGeneratePanel", "Prepare", 0));
        AddLod->setText(QApplication::translate("SingleGeneratePanel", "AddLod", 0));
        GenerateMesh->setText(QApplication::translate("SingleGeneratePanel", "GenerateMesh", 0));
        GenerateTexture->setText(QApplication::translate("SingleGeneratePanel", "GenerateTexture", 0));
        SingleAutoGenerate->setText(QApplication::translate("SingleGeneratePanel", "SingleAutoGenerate", 0));
    } // retranslateUi

};

namespace Ui {
    class SingleGeneratePanel: public Ui_SingleGeneratePanel {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SINGLEGENERATEPANEL_H
