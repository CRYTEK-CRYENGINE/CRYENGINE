#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "IPlugin.h"
#include "GameExporter.h"
#include "Util/BoostPythonHelpers.h"
//#include "DisplaySettings.h"
#include <CrySerialization/Enum.h>
#include "QtViewPane.h"
#include "panel/LodGeneratorDialog.h"
#include "panel/LodGeneratorAllDialog.h"

#include <QMessageBox>
#include "QtViewPane.h"

class LodGenerator : public IPlugin
{
public:
	//! Returns the version number of the plugin
	virtual int32       GetPluginVersion() override { return 1; }
	//! Returns the human readable name of the plugin
	virtual const char* GetPluginName() override { return "CryLodGenerator"; }
	//! Returns the human readable description of the plugin
	virtual const char* GetPluginDescription()  override { return "CryLodGenerator"; }
//	void Init()
//	{
//		CRegistrationContext rc;
//		rc.pCommandManager = GetIEditor()->GetCommandManager();
//		rc.pClassFactory = (CClassFactory*)GetIEditor()->GetClassFactory();
////		DesignerEditor::RegisterTool(rc);
//
//		RegisterQtViewPane<CLodGeneratorDialog>( 
//			GetIEditor(),
//			"LodGeneratorView",
//			"LodGeneratorView");
//
//		RegisterQtViewPane<CLodGeneratorAllDialog>( 
//			GetIEditor(),
//			"LodGeneratorAllView",
//			"LodGeneratorAllView");
//	}

};
REGISTER_PLUGIN(LodGenerator);

