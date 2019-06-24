#pragma once

#include <QMainWindow>
#include "IEditor.h"

namespace Ui {
class CLodGeneratorAllDialog;
}

class CLodGeneratorAllDialog : public QMainWindow, public IEditorNotifyListener
{
    Q_OBJECT

public:
    explicit CLodGeneratorAllDialog(QWidget *parent = 0);
    ~CLodGeneratorAllDialog();

	void OnEditorNotifyEvent( EEditorNotifyEvent event ) override;

private:
    Ui::CLodGeneratorAllDialog *ui;
};

