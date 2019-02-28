// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <BoostPythonMacros.h>
#include <FilePathUtil.h>
#include <ICommandManager.h>

#include <fstream>
#include <map>

namespace
{

/*For each command, produces autocomplete data in this format:
def rename(arg1,arg2):
	"""
	Renames layer with specific name.

	:param arg1: Current layer name
	:param arg2: New layer name
	"""
	pass
*/
const char* PyGeneratePythonEditorAutocompleteFiles()
{
	const string outputFolder = PathUtil::GetUserSandboxFolder() + "python/autocomplete/sandbox/";

	QDir dir(outputFolder.c_str());
	dir.removeRecursively();
	dir.mkpath(outputFolder.c_str());

	//ModuleName <-> ofstream (ModuleName.py)
	std::map<string, std::ofstream> autocompleteFiles;

	std::vector<CCommand*> commands;
	GetIEditor()->GetICommandManager()->GetCommandList(commands);

	for (const CCommand* pCommand : commands)
	{
		if (!pCommand->IsAvailableInScripting())
		{
			continue;
		}

		const auto& moduleName = pCommand->GetModule();

		auto& fileIt = autocompleteFiles.find(moduleName);
		if (fileIt == autocompleteFiles.cend())
		{
			std::ofstream ofs{ outputFolder + moduleName + ".py" };
			fileIt = autocompleteFiles.emplace(std::make_pair<>(moduleName, std::move(ofs))).first;
		}

		auto& ofs = fileIt->second;
		ofs << "def " << pCommand->GetName() << '(';

		const auto& args = pCommand->GetParameters();

		if (!args.empty())
		{
			size_t commas = args.size() - 1;
			int argNum = 1;
			for (const auto& arg : args)
			{
				ofs << "arg" << argNum;
				if (commas != 0)
				{
					ofs << ',';
					--commas;
				}
				++argNum;
			}
		}

		ofs << "):\n";
		ofs << "\t\"\"\"\n";
		ofs << '\t' << pCommand->GetDescription() << '\n';

		if (!args.empty())
		{
			ofs << '\n';
			int argNum = 1;
			for (const auto& arg : args)
			{
				ofs << "\t:param arg" << argNum << ": " << arg.GetDescription() << '\n';
				++argNum;
			}
		}
		ofs << "\t\"\"\"\n";
		ofs << "\tpass\n\n";
	}

	return dir.absolutePath().toStdString().c_str();
}

} // unnamed namespace


REGISTER_PYTHON_COMMAND(PyGeneratePythonEditorAutocompleteFiles, pythoneditor, generate_pythoneditor_autocomplete_files,
                        CCommandDescription("Generate autocomplete data for PythonEditor. Returns output folder"));
