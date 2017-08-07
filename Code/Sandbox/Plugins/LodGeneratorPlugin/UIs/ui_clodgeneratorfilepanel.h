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
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CLodGeneratorFilePanel
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *SelectMeshPathEdit;
    QPushButton *SelectMeshButton;
    QPushButton *SelectObjButton;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QSpacerItem *horizontalSpacer;
    QLineEdit *SelectMaterialPathEdit;
    QPushButton *SelectMaterialButton;
	QPushButton *ChangeMaterialButton;

    void setupUi(QWidget *CLodGeneratorFilePanel)
    {
        if (CLodGeneratorFilePanel->objectName().isEmpty())
            CLodGeneratorFilePanel->setObjectName(QStringLiteral("CLodGeneratorFilePanel"));
        CLodGeneratorFilePanel->resize(587, 123);
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

        horizontalLayout->addWidget(SelectMeshButton);

        SelectObjButton = new QPushButton(CLodGeneratorFilePanel);
        SelectObjButton->setObjectName(QStringLiteral("SelectObjButton"));

        horizontalLayout->addWidget(SelectObjButton);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        label_2 = new QLabel(CLodGeneratorFilePanel);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setFont(font);

        horizontalLayout_2->addWidget(label_2);

        horizontalSpacer = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        SelectMaterialPathEdit = new QLineEdit(CLodGeneratorFilePanel);
        SelectMaterialPathEdit->setObjectName(QStringLiteral("SelectMaterialPathEdit"));
        SelectMaterialPathEdit->setFont(font);

        horizontalLayout_2->addWidget(SelectMaterialPathEdit);

		ChangeMaterialButton = new QPushButton(CLodGeneratorFilePanel);
		ChangeMaterialButton->setObjectName(QStringLiteral("ChangeMaterialButton"));

		horizontalLayout_2->addWidget(ChangeMaterialButton);

        SelectMaterialButton = new QPushButton(CLodGeneratorFilePanel);
        SelectMaterialButton->setObjectName(QStringLiteral("SelectMaterialButton"));

        horizontalLayout_2->addWidget(SelectMaterialButton);


        verticalLayout->addLayout(horizontalLayout_2);


        retranslateUi(CLodGeneratorFilePanel);

        QMetaObject::connectSlotsByName(CLodGeneratorFilePanel);
    } // setupUi

    void retranslateUi(QWidget *CLodGeneratorFilePanel)
    {
        CLodGeneratorFilePanel->setWindowTitle(QApplication::translate("CLodGeneratorFilePanel", "Form", 0));
        label->setText(QApplication::translate("CLodGeneratorFilePanel", "Source CGF:", 0));
        SelectMeshButton->setText(QApplication::translate("CLodGeneratorFilePanel", "...", 0));
        SelectObjButton->setText(QApplication::translate("CLodGeneratorFilePanel", "<", 0));
        label_2->setText(QApplication::translate("CLodGeneratorFilePanel", "Material:", 0));
        SelectMaterialButton->setText(QApplication::translate("CLodGeneratorFilePanel", "O", 0));
		ChangeMaterialButton->setText(QApplication::translate("CLodGeneratorFilePanel", "...", 0));
    } // retranslateUi

};

namespace Ui {
    class CLodGeneratorFilePanel: public Ui_CLodGeneratorFilePanel {};
} // namespace Ui

QT_END_NAMESPACE

