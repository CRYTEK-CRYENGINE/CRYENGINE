/********************************************************************************
** Form generated from reading UI file 'clodgeneratorfilepanel.ui'
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
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CLodGeneratorAllPanel
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *SelectMeshPathEdit;
    QPushButton *SelectMeshButton;
    QPushButton *SelectObjButton;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *AutoGenerateButton;
    QHBoxLayout *horizontalLayout_4;
    QComboBox *GenerateConifgCombo;
    QHBoxLayout *horizontalLayout_5;
    QPushButton *GenerateOptionButton;
    QProgressBar *AutoGenerateProgress;

    void setupUi(QWidget *CLodGeneratorFilePanel)
    {
        if (CLodGeneratorFilePanel->objectName().isEmpty())
            CLodGeneratorFilePanel->setObjectName(QStringLiteral("CLodGeneratorFilePanel"));
        CLodGeneratorFilePanel->resize(587, 111);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(CLodGeneratorFilePanel->sizePolicy().hasHeightForWidth());
        CLodGeneratorFilePanel->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(CLodGeneratorFilePanel);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        label = new QLabel(CLodGeneratorFilePanel);
        label->setObjectName(QStringLiteral("label"));
        QFont font;
        font.setPointSize(13);
        label->setFont(font);

        horizontalLayout->addWidget(label);

        SelectMeshPathEdit = new QLineEdit(CLodGeneratorFilePanel);
        SelectMeshPathEdit->setObjectName(QStringLiteral("SelectMeshPathEdit"));
        SelectMeshPathEdit->setFont(font);

        horizontalLayout->addWidget(SelectMeshPathEdit);

        SelectMeshButton = new QPushButton(CLodGeneratorFilePanel);
        SelectMeshButton->setObjectName(QStringLiteral("SelectMeshButton"));
        SelectMeshButton->setLayoutDirection(Qt::LeftToRight);

        horizontalLayout->addWidget(SelectMeshButton);

        SelectObjButton = new QPushButton(CLodGeneratorFilePanel);
        SelectObjButton->setObjectName(QStringLiteral("SelectObjButton"));

        horizontalLayout->addWidget(SelectObjButton);


        verticalLayout->addLayout(horizontalLayout);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        AutoGenerateButton = new QPushButton(CLodGeneratorFilePanel);
        AutoGenerateButton->setObjectName(QStringLiteral("AutoGenerateButton"));

        horizontalLayout_3->addWidget(AutoGenerateButton);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        GenerateConifgCombo = new QComboBox(CLodGeneratorFilePanel);
        GenerateConifgCombo->setObjectName(QStringLiteral("GenerateConifgCombo"));

        horizontalLayout_4->addWidget(GenerateConifgCombo);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QStringLiteral("horizontalLayout_5"));
        GenerateOptionButton = new QPushButton(CLodGeneratorFilePanel);
        GenerateOptionButton->setObjectName(QStringLiteral("GenerateOptionButton"));

        horizontalLayout_5->addWidget(GenerateOptionButton);


        horizontalLayout_4->addLayout(horizontalLayout_5);


        horizontalLayout_3->addLayout(horizontalLayout_4);


        verticalLayout_2->addLayout(horizontalLayout_3);

        AutoGenerateProgress = new QProgressBar(CLodGeneratorFilePanel);
        AutoGenerateProgress->setObjectName(QStringLiteral("AutoGenerateProgress"));
        AutoGenerateProgress->setValue(24);
        AutoGenerateProgress->setTextVisible(true);

        verticalLayout_2->addWidget(AutoGenerateProgress);


        verticalLayout->addLayout(verticalLayout_2);


        retranslateUi(CLodGeneratorFilePanel);

        QMetaObject::connectSlotsByName(CLodGeneratorFilePanel);
    } // setupUi

    void retranslateUi(QWidget *CLodGeneratorFilePanel)
    {
        CLodGeneratorFilePanel->setWindowTitle(QApplication::translate("CLodGeneratorFilePanel", "Form", 0));
        label->setText(QApplication::translate("CLodGeneratorFilePanel", "Source CGF:", 0));
        SelectMeshButton->setText(QApplication::translate("CLodGeneratorFilePanel", "...", 0));
        SelectObjButton->setText(QApplication::translate("CLodGeneratorFilePanel", "<", 0));
        AutoGenerateButton->setText(QApplication::translate("CLodGeneratorFilePanel", "AutoGenerate", 0));
        GenerateOptionButton->setText(QApplication::translate("CLodGeneratorFilePanel", "Option", 0));
    } // retranslateUi

};

namespace Ui {
    class CLodGeneratorAllPanel: public Ui_CLodGeneratorAllPanel {};
} // namespace Ui

QT_END_NAMESPACE

