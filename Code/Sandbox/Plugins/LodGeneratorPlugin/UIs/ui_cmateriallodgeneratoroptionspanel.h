/********************************************************************************
** Form generated from reading UI file 'cmateriallodgeneratoroptionspanel.ui'
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
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <Serialization/QPropertyTree/QPropertyTree.h>

QT_BEGIN_NAMESPACE

class Ui_CMaterialLODGeneratorOptionsPanel
{
public:
    QVBoxLayout *verticalLayout;
	QPropertyTree* m_propertyTree;

    void setupUi(QWidget *CMaterialLODGeneratorOptionsPanel)
    {
        if (CMaterialLODGeneratorOptionsPanel->objectName().isEmpty())
            CMaterialLODGeneratorOptionsPanel->setObjectName(QStringLiteral("CMaterialLODGeneratorOptionsPanel"));
        CMaterialLODGeneratorOptionsPanel->resize(501, 691);
        verticalLayout = new QVBoxLayout(CMaterialLODGeneratorOptionsPanel);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        m_propertyTree = new QPropertyTree(CMaterialLODGeneratorOptionsPanel);
        m_propertyTree->setObjectName(QStringLiteral("m_propertyTree"));

        verticalLayout->addWidget(m_propertyTree);


        retranslateUi(CMaterialLODGeneratorOptionsPanel);

        QMetaObject::connectSlotsByName(CMaterialLODGeneratorOptionsPanel);
    } // setupUi

    void retranslateUi(QWidget *CMaterialLODGeneratorOptionsPanel)
    {
        CMaterialLODGeneratorOptionsPanel->setWindowTitle(QApplication::translate("CMaterialLODGeneratorOptionsPanel", "Form", 0));
    } // retranslateUi

};

namespace Ui {
    class CMaterialLODGeneratorOptionsPanel: public Ui_CMaterialLODGeneratorOptionsPanel {};
} // namespace Ui

QT_END_NAMESPACE

