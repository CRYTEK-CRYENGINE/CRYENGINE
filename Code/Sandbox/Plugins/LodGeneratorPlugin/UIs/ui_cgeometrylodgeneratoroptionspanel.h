/********************************************************************************
** Form generated from reading UI file 'cgeometrylodgeneratoroptionspanel.ui'
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
#include <Qtwidgets/QBoxLayout>
#include <Serialization/QPropertyTree/QPropertyTree.h>

QT_BEGIN_NAMESPACE

class Ui_CGeometryLodGeneratorOptionsPanel
{
public:
    QVBoxLayout *verticalLayout;
	QPropertyTree* m_propertyTree;
    void setupUi(QWidget *CGeometryLodGeneratorOptionsPanel)
    {
        if (CGeometryLodGeneratorOptionsPanel->objectName().isEmpty())
            CGeometryLodGeneratorOptionsPanel->setObjectName(QStringLiteral("CGeometryLodGeneratorOptionsPanel"));
        CGeometryLodGeneratorOptionsPanel->resize(501, 531);

        verticalLayout = new QVBoxLayout(CGeometryLodGeneratorOptionsPanel);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        m_propertyTree = new QPropertyTree(CGeometryLodGeneratorOptionsPanel);
        m_propertyTree->setObjectName(QStringLiteral("m_propertyTree"));

        verticalLayout->addWidget(m_propertyTree);


        retranslateUi(CGeometryLodGeneratorOptionsPanel);

        QMetaObject::connectSlotsByName(CGeometryLodGeneratorOptionsPanel);
    } // setupUi

    void retranslateUi(QWidget *CGeometryLodGeneratorOptionsPanel)
    {
        CGeometryLodGeneratorOptionsPanel->setWindowTitle(QApplication::translate("CGeometryLodGeneratorOptionsPanel", "Form", 0));
    } // retranslateUi

};

namespace Ui {
    class CGeometryLodGeneratorOptionsPanel: public Ui_CGeometryLodGeneratorOptionsPanel {};
} // namespace Ui

QT_END_NAMESPACE

