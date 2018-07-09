// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <EditorFramework/Editor.h>

// Disable warnings (treated as errors) thrown by shiboken headers, causing compilation to fail.
#pragma warning (push)
#pragma warning (disable : 4522 4800 4244 4005)
#include <sbkpython.h>
#include <shiboken.h>
#pragma warning (pop)
#include <pyside.h>

#include <pyside2_qtwidgets_python.h>
#include <typeresolver.h>

PyTypeObject** SbkPySide2_QtWidgetsTypes;
SbkConverter** SbkPySide2_QtWidgetsTypeConverters;

struct PythonWidget
{
	PyObject* pShibokenWrapper;
	QWidget*  pQtWidget;

	operator bool() const { return pShibokenWrapper && pQtWidget; }
};

QWidget* CreateErrorWidget()
{
	QLabel* result = new QLabel();
	result->setAlignment(Qt::AlignCenter);
	result->setText("Failed to create widget. Consult the error log for details.");
	return result;
}

PythonWidget InstantiateWidgetFromPython(PyObject* pWidgetType)
{
	PyObject* arglist;
	PyObject* pyResult;

	// Call evaluate on the python type (which will create an instance)
	arglist = Py_BuildValue("()");
	pyResult = PyObject_CallObject(pWidgetType, arglist);
	Py_DECREF(arglist);

	if (!pyResult)
	{
		if (PyErr_Occurred())
		{
			PyErr_PrintEx(0);
		}

		return PythonWidget{ nullptr, CreateErrorWidget() };
	}

	// Check to make sure we've gotten a QWidget
	PythonToCppFunc pythonToCpp = Shiboken::Conversions::isPythonToCppPointerConvertible((SbkObjectType*)SbkPySide2_QtWidgetsTypes[SBK_QWIDGET_IDX], pyResult);
	if (!pythonToCpp)
	{
		Shiboken::warning(PyExc_RuntimeWarning, 2, "Invalid return value in function %s, expected %s, got %s.", "IWidgetFactory.ConstructWidget", Shiboken::SbkType<QWidget>()->tp_name, pyResult->ob_type->tp_name);
		Py_DECREF(pyResult);
		return PythonWidget{ nullptr, CreateErrorWidget() };
	}

	// Convert python QWidget to a QWidget*
	QWidget* cppResult;
	pythonToCpp(pyResult, &cppResult);

	return PythonWidget { pyResult, cppResult };
};

class PythonViewPaneWidget : public IPane
{
	friend class PythonViewPaneClass;

public:
	PythonViewPaneWidget(PyObject* pWidgetType, const char* name) : IPane()
		, name(name)
	{
		pythonWidget = InstantiateWidgetFromPython(pWidgetType);
	}

	~PythonViewPaneWidget()
	{
		// Decrease reference to shiboken wrapper so it will be cleaned up.
		if (pythonWidget)
		{
			Py_DECREF(pythonWidget.pShibokenWrapper);
		}
	}

	virtual QWidget*    GetWidget() override          { return pythonWidget.pQtWidget; }
	virtual const char* GetPaneTitle() const override { return name.c_str(); }

private:
	std::string  name;
	PythonWidget pythonWidget;
};

class PythonViewPaneClass : public IViewPaneClass
{
private:
	PyObject*   pWidgetType;
	std::string name;
	std::string category;
	bool        needsMenuItem;
	std::string menuPath;
	bool        unique;

public:
	PythonViewPaneClass(PyObject* pWidgetType, const char* name, const char* category, bool needsMenuItem, const char* menuPath, bool unique)
		: IViewPaneClass()
		, pWidgetType(pWidgetType)
		, name(name)
		, category(category)
		, needsMenuItem(needsMenuItem)
		, menuPath(menuPath)
		, unique(unique)
	{
		Py_INCREF(pWidgetType);
	}

	~PythonViewPaneClass()
	{
		Py_DECREF(pWidgetType);
	}

	virtual const char*    ClassName() override       { return name.c_str(); }
	virtual const char*    Category()  override       { return category.c_str(); }
	virtual bool           NeedsMenuItem()            { return needsMenuItem; }
	virtual const char*    GetMenuPath()              { return menuPath.c_str(); }
	virtual CRuntimeClass* GetRuntimeClass() override { return 0; }
	virtual const char*    GetPaneTitle() override    { return name.c_str(); }
	virtual bool           SinglePane() override      { return unique; }
	virtual IPane*         CreatePane() const override
	{
		return new PythonViewPaneWidget(pWidgetType, name.c_str());
	}
};

static PyObject* RegisterWindow(PyObject* dummy, PyObject* args)
{
	PyObject* pythonType;
	const char* name;
	const char* category;
	bool needsMenu;
	const char* menuPath;
	bool unique;

	if (PyArg_ParseTuple(args, "Ossbsb:register_window", &pythonType, &name, &category, &needsMenu, &menuPath, &unique))
	{
		if (!PyType_Check(pythonType))
		{
			PyErr_SetString(PyExc_TypeError, "Parameter must be a type");
			return NULL;
		}

		IEditor* editor = GetIEditor();
		if (!editor)
		{
			PyErr_SetString(PyExc_RuntimeError, "Plugin not loaded");
			return NULL;
		}

		editor->GetClassFactory()->RegisterClass(new PythonViewPaneClass(pythonType, name, category, needsMenu, menuPath, unique));

		Py_INCREF(Py_None);
		return Py_None;
	}
	return NULL;
}

static PyMethodDef BridgeMethods[] =
{
	{ "register_window", RegisterWindow, METH_VARARGS, "" },
	{ NULL,              NULL,           0,            NULL} // Sentinel
};

PyMODINIT_FUNC initSandboxPythonBridge(void)
{
	// Import QtWidgets PySide module so we can convert python objects to QWidget.
	{
		Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide2.QtWidgets"));
		if (requiredModule.isNull())
			return SBK_MODULE_INIT_ERROR;
		SbkPySide2_QtWidgetsTypes = Shiboken::Module::getTypes(requiredModule);
		SbkPySide2_QtWidgetsTypeConverters = Shiboken::Module::getTypeConverters(requiredModule);
	}

	// Initialize python module and register methods
	PyObject* module;
	module = Py_InitModule("SandboxPythonBridge", BridgeMethods);
}
