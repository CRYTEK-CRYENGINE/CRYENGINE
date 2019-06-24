/********************************************************************************
** Form generated from reading UI file 'clodgeneratoralldialog.ui'
**
** Created by: Qt User Interface Compiler version 5.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLODGENERATORALLDIALOG_H
#define UI_CLODGENERATORALLDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "../panel/LodGeneratorAllPanel.h"

QT_BEGIN_NAMESPACE

class Ui_CLodGeneratorAllDialog
{
public:
    CLodGeneratorAllPanel *centralWidget;
    QHBoxLayout *horizontalLayout;
    QWidget *widget;
    QVBoxLayout *verticalLayout;

    void setupUi(QMainWindow *CLodGeneratorAllDialog)
    {
        if (CLodGeneratorAllDialog->objectName().isEmpty())
            CLodGeneratorAllDialog->setObjectName(QStringLiteral("CLodGeneratorAllDialog"));
        CLodGeneratorAllDialog->resize(587, 146);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(CLodGeneratorAllDialog->sizePolicy().hasHeightForWidth());
        CLodGeneratorAllDialog->setSizePolicy(sizePolicy);
        CLodGeneratorAllDialog->setMinimumSize(QSize(587, 146));
        CLodGeneratorAllDialog->setMaximumSize(QSize(587, 146));
        centralWidget = new CLodGeneratorAllPanel(CLodGeneratorAllDialog);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        horizontalLayout = new QHBoxLayout(centralWidget);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        widget = new QWidget(centralWidget);
        widget->setObjectName(QStringLiteral("widget"));
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));

        horizontalLayout->addWidget(widget);

        CLodGeneratorAllDialog->setCentralWidget(centralWidget);

        retranslateUi(CLodGeneratorAllDialog);

        QMetaObject::connectSlotsByName(CLodGeneratorAllDialog);
    } // setupUi

    void retranslateUi(QMainWindow *CLodGeneratorAllDialog)
    {
        CLodGeneratorAllDialog->setWindowTitle(QApplication::translate("CLodGeneratorAllDialog", "CLodGeneratorAllDialog", 0));
    } // retranslateUi

};

namespace Ui {
    class CLodGeneratorAllDialog: public Ui_CLodGeneratorAllDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLODGENERATORALLDIALOG_H
