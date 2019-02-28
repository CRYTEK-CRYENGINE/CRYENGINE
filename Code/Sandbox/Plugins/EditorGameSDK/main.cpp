// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "IPlugin.h"
#include "EquipPack/EquipPackDialog.h"
#include "EquipPack/EquipPackLib.h"

//DECLARE_PYTHON_MODULE(gamesdk);

class EditorGameSDK : public IPlugin
{
public:

	EditorGameSDK()
	{
		GetIEditor()->RegisterDeprecatedPropertyEditor(ePropertyEquip, 
			std::function<bool(const string&, string&)>([](const string& oldValue, string& newValueOut)->bool
		{
			CEquipPackDialog dlg;
			dlg.SetCurrEquipPack(oldValue.GetString());
			if (dlg.DoModal() == IDOK)
			{
				newValueOut = dlg.GetCurrEquipPack().GetString();
				return true;
			}
			return false;
		}));

		CEquipPackLib::GetRootEquipPack().LoadLibs(true);
	}

	int32       GetPluginVersion() override { return 1; }
	const char* GetPluginName() override    { return "EditorGameSDK"; }
	const char* GetPluginDescription() override { return "Game SDK specific editor extensions"; }
};

REGISTER_PLUGIN(EditorGameSDK);
