#pragma once

#include <QMainWindow>
#include "IEditor.h"

namespace Ui {
class CLodGeneratorTestAllDialog;
}

class CLodGeneratorTestAllDialog : public QMainWindow, public IEditorNotifyListener
{
    Q_OBJECT

public:
    explicit CLodGeneratorTestAllDialog(QWidget *parent = 0);
    ~CLodGeneratorTestAllDialog();

	void OnEditorNotifyEvent( EEditorNotifyEvent event ) override;

private:
    Ui::CLodGeneratorTestAllDialog *ui;
};

