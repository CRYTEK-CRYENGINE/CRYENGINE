// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_WINDOWS

	#include "ConsoleHelpGen.h"
	#include "System.h"

// remove bad characters, toupper, not very fast
string CConsoleHelpGen::FixAnchorName(const char* szName)
{
	string ret;

	const char* p = szName;

	while (*p)
	{
		if ((*p >= 'a' && *p <= 'z')
		    || (*p >= 'A' && *p <= 'Z')
		    || (*p >= '0' && *p <= '9'))
		{
			if (*p >= 'a' && *p <= 'z')
				ret += *p - 'a' + 'A';
			else
				ret += *p;
		}

		++p;
	}

	return ret;
}

string CConsoleHelpGen::GetCleanPrefix(const char* p)
{
	string sRet;

	while (*p != '_' && *p != 0)
		sRet += *p++;

	return sRet;
}

string CConsoleHelpGen::SplitPrefixString_Part1(const char* p)
{
	string sRet;

	while (*p != 10 && *p != 13 && *p != 0)
		sRet += *p++;

	return sRet;
}

const char* CConsoleHelpGen::SplitPrefixString_Part2(const char* p)
{
	while (*p != 10 && *p != 13 && *p != 0)
		p++;

	while (*p == 10 || *p == 13)
		p++;

	return p;
}

void CConsoleHelpGen::StartPage(FILE* f, const char* szPageName, const char* szPageDescription) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<HTML><HEAD><TITLE>%s</TITLE><META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">", szPageName);
		fprintf(f, "<META NAME=\"DESCRIPTION\" CONTENT=\"%s\">", szPageDescription);
		fprintf(f, "<META NAME=\"author\" content=\"Crytek\">");
		fprintf(f, "<META NAME=\"copyright\" CONTENT=\"Crytek\">");
		fprintf(f, "<META NAME=\"KEYWORDS\" CONTENT=\"CryEngine,Crytek\">");
		fprintf(f, "<META NAME=\"distribution\" CONTENT=\"Crytek\">");
		fprintf(f, "<META NAME=\"revisit-after\" CONTENT=\"10 days\">");
		fprintf(f, "<META NAME=\"robots\" CONTENT=\"INDEX, NOFOLLOW\">");
		fprintf(f, "</HEAD><BODY bgcolor=#ffffff leftmargin=0 topmargin=0 alink=#0000ff link=#0000ff vlink=#0000ff text=#000000>");
	}
}

void CConsoleHelpGen::EndPage(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<P></P></BODY></HTML>");
	}
}

void CConsoleHelpGen::KeyValue(FILE* f, const char* szKey, const char* szValue) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<b>%s:</b> %s<br>\n", szKey, szValue);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		//		fprintf(f,"*%s:* %s\n",szKey,szValue);
		fprintf(f, "| *%s:* | %s |\n", szKey, szValue);
	}
	else assert(0);
}

void CConsoleHelpGen::LogVersion(FILE* f) const
{
	char s[1024];

	{
		GetModuleFileName(NULL, s, sizeof(s));

		char fdir[_MAX_PATH];
		char fdrive[_MAX_PATH];
		char file[_MAX_PATH];
		char fext[_MAX_PATH];
		_splitpath(s, fdrive, fdir, file, fext);

		KeyValue(f, "Executable", (string(file) + fext).c_str());
	}

	{
		time_t ltime;

		time(&ltime);
		tm* today = localtime(&ltime);

		strftime(s, 128, "%c", today);
		KeyValue(f, "Date(MM/DD/YY) Time", s);
	}

	{
		const SFileVersion& ver = gEnv->pSystem->GetFileVersion();
		ver.ToString(s);

		KeyValue(f, "FileVersion", s);
	}

	{
		const SFileVersion& ver = gEnv->pSystem->GetProductVersion();
		ver.ToString(s);

		KeyValue(f, "ProductVersion", s);
	}

	if (m_eWorkMode == eWM_HTML)
		fprintf(f, "<br>\n");
}

void CConsoleHelpGen::StartH1(FILE* f, const char* szName) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<h1>%s</h1>\n", szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "h1. %s\n", szName);
	}
	else assert(0);
}

void CConsoleHelpGen::EndH1(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<br>\n");
	}
}

void CConsoleHelpGen::StartH3(FILE* f, const char* szName) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<h3>%s</h3><ul>\n", szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "\nh3. %s\n", szName);
	}
	else assert(0);
}

void CConsoleHelpGen::EndH3(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "</ul>\n");
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
	}
	else assert(0);
}

void CConsoleHelpGen::StartCVar(FILE* f, const char* szName) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<h3>%s</h3><ul>\n", szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		//		fprintf(f,"\nh3. %s\n",szName);
		fprintf(f, "\n<div class=\"panel\" style=\"border-style: none;border-width: 1px;\"><div class=\"panelContent\"><p><b>%s</b><br/>\n", szName);
	}
	else assert(0);
}

void CConsoleHelpGen::EndCVar(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "</ul>\n");
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "</div></div>\n\n");
	}
	else assert(0);
}
void CConsoleHelpGen::SingleLinePrefix(FILE* f, const char* szPrefix, const char* szPrefixDesc, const char* szLink) const
{
	// group within the list of all groups (no elements)

	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<li><a href=\"%s\">%s_ %s</a></li>\n", szLink, szPrefix, szPrefixDesc);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		string sPrefix;

		if (*szPrefix)
			sPrefix = string(szPrefix) + "_";

		//		fprintf(f,"| %s | [%s|%s] |\n",sPrefix.c_str(),szPrefixDesc,szLink);		// e.g. "" "CL_" "CC_" "I_" "T_"
		fprintf(f, "{section:border=false}\n"
		           "{column:width=50px}{align:right}%s{align}{column}\n"
		           "{column:width=10px}{column}\n"
		           "{column}{align:left}[%s|%s]{align}{column}\n"
		           "{section}\n",
		        sPrefix.c_str(), szPrefixDesc, szLink);   // e.g. "" "CL_" "CC_" "I_" "T_"
	}
	else assert(0);
}

void CConsoleHelpGen::StartPrefix(FILE* f, const char* szPrefix, const char* szPrefixDesc, const char* szLink) const
{
	// group before all the group elements

	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<li><a href=\"%s\">%s_ %s</li></a><ul>\n", szLink, szPrefix, szPrefixDesc);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		if (*szPrefix)
			fprintf(f, "* [%s %s|%s]\n", szPrefix, szPrefixDesc, szLink);
		else
			fprintf(f, "* [%s|%s]\n", szPrefixDesc, szLink);
	}
	else assert(0);
}

void CConsoleHelpGen::EndPrefix(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "</ul>\n");
	}
}

void CConsoleHelpGen::SingleLineEntry_InGlobal(FILE* f, const char* szName, const char* szLink) const
{
	// element within a group

	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<li><a href=\"%s\">%s</a></li>\n", szLink, szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "** [%s|%s]\n", szName, szLink);
	}
	else assert(0);
}

void CConsoleHelpGen::SingleLineEntry_InGroup(FILE* f, const char* szName, const char* szLink) const
{
	// element within a group

	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<li><a href=\"%s\">%s</a></li>\n", szLink, szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "* [%s|%s]\n", szName, szLink);
	}
	else assert(0);
}

// szName without #
void CConsoleHelpGen::Anchor(FILE* f, const char* szName) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<a name=\"%s\"></a>\n", szName);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		fprintf(f, "{anchor:%s}", szName);
	}
	else assert(0);
}

void CConsoleHelpGen::InsertConsoleVars(std::set<const char*, string_nocase_lt>& setCmdAndVars, const char* szLocalPrefix) const
{
	CXConsole::ConsoleVariablesMap::const_iterator itrVar, itrVarEnd = m_rParent.m_mapVariables.end();

	for (itrVar = m_rParent.m_mapVariables.begin(); itrVar != itrVarEnd; ++itrVar)
	{
		ICVar* var = itrVar->second;

		if (strnicmp(var->GetName(), szLocalPrefix, strlen(szLocalPrefix)) == 0)
			setCmdAndVars.insert(var->GetName());
	}
}

void CConsoleHelpGen::InsertConsoleCommands(std::set<const char*, string_nocase_lt>& setCmdAndVars, const char* szLocalPrefix) const
{
	CXConsole::ConsoleCommandsMap::const_iterator itrCmd, itrCmdEnd = m_rParent.m_mapCommands.end();

	for (itrCmd = m_rParent.m_mapCommands.begin(); itrCmd != itrCmdEnd; ++itrCmd)
	{
		const CConsoleCommand& cmd = itrCmd->second;

		if (strnicmp(cmd.m_sName, szLocalPrefix, strlen(szLocalPrefix)) == 0)
			setCmdAndVars.insert(cmd.m_sName.c_str());
	}
}

void CConsoleHelpGen::InsertConsoleVars(std::set<const char*, string_nocase_lt>& setCmdAndVars, std::map<string, const char*> mapPrefix) const
{
	CXConsole::ConsoleVariablesMap::const_iterator itrVar, itrVarEnd = m_rParent.m_mapVariables.end();

	for (itrVar = m_rParent.m_mapVariables.begin(); itrVar != itrVarEnd; ++itrVar)
	{
		ICVar* var = itrVar->second;
		bool bInsert = true;

		{
			std::map<string, const char*>::const_iterator it2, end = mapPrefix.end();

			for (it2 = mapPrefix.begin(); it2 != end; ++it2)
				if (it2->first != "___" && strnicmp(var->GetName(), it2->first, it2->first.size()) == 0)
				{ bInsert = false; break; }
		}

		if (bInsert)
			setCmdAndVars.insert(var->GetName());
	}
}

void CConsoleHelpGen::InsertConsoleCommands(std::set<const char*, string_nocase_lt>& setCmdAndVars, std::map<string, const char*> mapPrefix) const
{
	CXConsole::ConsoleCommandsMap::const_iterator itrCmd, itrCmdEnd = m_rParent.m_mapCommands.end();

	for (itrCmd = m_rParent.m_mapCommands.begin(); itrCmd != itrCmdEnd; ++itrCmd)
	{
		const CConsoleCommand& cmd = itrCmd->second;
		bool bInsert = true;

		{
			std::map<string, const char*>::const_iterator it2, end = mapPrefix.end();

			for (it2 = mapPrefix.begin(); it2 != end; ++it2)
				if (it2->first != "___" && strnicmp(cmd.m_sName, it2->first, it2->first.size()) == 0)
				{ bInsert = false; break; }
		}

		if (bInsert)
			setCmdAndVars.insert(cmd.m_sName.c_str());
	}
}

void CConsoleHelpGen::CreateSingleEntryFile(const char* szName) const
{
	assert(m_eWorkMode == eWM_Confluence);  // only needed for confluence

	FILE* f3 = fopen((string(GetFolderName()) + GetFileExtension() + "/" + FixAnchorName(szName)).c_str(), "w");

	if (!f3)
	{
		assert(0);
		return;     // error
	}

	//	StartPage(f3,szName,"");		// HTML style

	IncludeSingleEntry(f3, szName);

	//	EndPage(f3);		// HTML style

	fclose(f3);
}

const CConsoleCommand* CConsoleHelpGen::FindConsoleCommand(const char* szName) const
{
	CXConsole::ConsoleCommandsMap::const_iterator itrCmd, itrCmdEnd = m_rParent.m_mapCommands.end();

	for (itrCmd = m_rParent.m_mapCommands.begin(); itrCmd != itrCmdEnd; ++itrCmd)
	{
		const CConsoleCommand& cmd = itrCmd->second;

		if (strcmp(cmd.m_sName.c_str(), szName) == 0)
			return &cmd;
	}

	return 0;
}

void CConsoleHelpGen::IncludeSingleEntry(FILE* f, const char* szName) const
{
	StartCVar(f, szName);

	uint32 dwFlags = 0;
	const char* szHelp = "";

	// slow but good for simpler code
	if (ICVar* pVar = gEnv->pConsole->GetCVar(szName))
	{
		dwFlags = pVar->GetFlags();
		szHelp = pVar->GetHelp();
	}
	else if (const CConsoleCommand* pCommand = FindConsoleCommand(szName))
	{
		dwFlags = pCommand->m_nFlags;
		szHelp = pCommand->m_sHelp.c_str();
	}
	else assert(0);   // internal error

	const char* szFlags = CXConsole::GetFlagsString(dwFlags);

	if (*szFlags)
	{
		//		fprintf(f3,"%%GRAY%% %s %%ENDCOLOR%%<br>\n",szFlags);		// twiki style
		if (m_eWorkMode == eWM_HTML)
			fprintf(f, "%s<br>\n", szFlags);      // simple HTML style
		else if (m_eWorkMode == eWM_Confluence)
			fprintf(f, "<font color=\"#808080\">%s</font></p>\n", szFlags);
		else assert(0);
	}

	if (*szHelp == 0)
	{
		//		fprintf(f,"%%TODO%%\n");	// wiki style, in our wiki %TODO% is defined as <img src=\"%%ICONURL{todo}%%\" width=\"37\" height=\"16\" alt=\"TODO\" border=\"0\" />
		if (m_eWorkMode == eWM_HTML)
			fprintf(f, "<blockquote></b>*TODO*</b></blockquote>\n"); // HTML style
		else if (m_eWorkMode == eWM_Confluence)
			fprintf(f, "{warning}TODO{warning}\n");      // Confluence style
		else assert(0);
	}
	else
	{
		if (m_eWorkMode == eWM_HTML)
			fprintf(f, "<blockquote><pre><verbatim><tt>\n%s\n</tt></verbatim></pre></blockquote>\n", szHelp);    // HTML style, <tt> to get fixed with font (layout in code is often making the assumption the forn is fixed width)
		else if (m_eWorkMode == eWM_Confluence)
		{
			fprintf(f, "<pre>\n");

			string sHelp = szHelp;

			// currently not required as the {noformat} is used
			//			sHelp.replace("[","\\[");		sHelp.replace("]","\\]");
			//			sHelp.replace("{","\\{");		sHelp.replace("}","\\}");
			//			sHelp.replace("(","\\(");		sHelp.replace(")","\\)");

			fprintf(f, "%s\n", sHelp.c_str());   // Confluence style

			fprintf(f, "</pre>");
		}
		else assert(0);
	}

	EndCVar(f);
}

void CConsoleHelpGen::Work()
{
	//	string sEngineFolder = string("%USER%/")+GetFolderName();
	//	gEnv->pCryPak->RemoveDir(sEngineFolder.c_str(),true);			// todo: check if that works
	//	CryCreateDirectory(sEngineFolder.c_str());

	m_eWorkMode = eWM_HTML;
	CreateMainPages();

	m_eWorkMode = eWM_Confluence;
	CreateMainPages();
	CreateFileForEachEntry();

	m_eWorkMode = eWM_None;
}

void CConsoleHelpGen::CreateFileForEachEntry()
{
	assert(m_eWorkMode == eWM_Confluence);  // only needed for confluence

	// generate a single file for each console command
	{
		CXConsole::ConsoleCommandsMap::const_iterator itrCmd, itrCmdEnd = m_rParent.m_mapCommands.end();

		for (itrCmd = m_rParent.m_mapCommands.begin(); itrCmd != itrCmdEnd; ++itrCmd)
		{
			const CConsoleCommand& cmd = itrCmd->second;

			CreateSingleEntryFile(cmd.m_sName.c_str());
		}
	}

	// generate a single file for each console variable
	{
		CXConsole::ConsoleVariablesMap::iterator itrVar, itrVarEnd = m_rParent.m_mapVariables.end();

		for (itrVar = m_rParent.m_mapVariables.begin(); itrVar != itrVarEnd; ++itrVar)
		{
			ICVar* var = itrVar->second;

			CreateSingleEntryFile(var->GetName());
		}
	}
}

void CConsoleHelpGen::CreateMainPages()
{
	CryCreateDirectory(GetFolderName());

	std::map<string, const char*> mapPrefix;

	// order here doesn't matter, after the name some help can be added (after the first return)
	mapPrefix["AI_"] = "Artificial Intelligence";
	mapPrefix["NET_"] = "Network";
	mapPrefix["ED_"] = "Editor";
	mapPrefix["ES_"] = "Entity System";
	mapPrefix["CON_"] = "Console";
	mapPrefix["AG_"] = "Animation Graph"       "\n"  "High level animation logic, describes animation selection and flow, matches animation state to current game logical state.";
	mapPrefix["AC_"] = "Animated Character"      "\n"  "Better name would be 'Character Movement'.\nBridges game controlled movement and animation controlled movement.";
	mapPrefix["CA_"] = "Character Animation"     "\n"  "Motion synthesize and playback, parameterization through blending and inversed kinematics.";
	mapPrefix["E_"] = "3DEngine";
	mapPrefix["I_"] = "Input";
	mapPrefix["FG_"] = "Flow Graph"          "\n"  "hyper graph: game logic";
	mapPrefix["P_"] = "Physics";
	mapPrefix["R_"] = "Renderer";
	mapPrefix["S_"] = "Sound";
	mapPrefix["G_"] = "Game"              "\n"  "game specific, not part of CryEngine";
	mapPrefix["SYS_"] = "System";
	mapPrefix["V_"] = "Vehicle";
	mapPrefix["FT_"] = "Feature Test";
	mapPrefix["DEMO_"] = "Time Demo";
	mapPrefix["FT_"] = "Feature Test";
	mapPrefix["GFX_"] = "Scaleform GFx";
	mapPrefix["GL_"] = "Game Lobby";
	mapPrefix["HUD_"] = "Heads Up Display";
	mapPrefix["KC_"] = "Kill Cam";
	mapPrefix["MN_"] = "Mannequin";
	mapPrefix["PL_"] = "Player";
	mapPrefix["PP_"] = "Player Progression";
	mapPrefix["AIM_"] = "Aiming";
	mapPrefix["CAPTURE_"] = "Capture";
	mapPrefix["DS_"] = "Dialog Scripts";
	mapPrefix["GT_"] = "Game Token";
	mapPrefix["LOG_"] = "Logging";
	mapPrefix["MOV_"] = "Movie Sequences";
	mapPrefix["OSM_"] = "Overload Scene Manager";
	mapPrefix["PROFILE_"] = "Profiling";
	mapPrefix["STAP_"] = "Screen-space Torso Aim Pose";
	mapPrefix["LUA_"] = "Lua"             "\n"  "scripting system";
	mapPrefix["SV_"] = "Server";
	mapPrefix["MFX_"] = "Material Effects";
	mapPrefix["M_"] = "Multi threading";
	mapPrefix["CC_"] = "Character Customization";
	mapPrefix["CL_"] = "Client";
	mapPrefix["Q_"] = "Quality"           "\n"  "usually shader quality";
	mapPrefix["T_"] = "Timer";
	mapPrefix["___"] = "Remaining";           // key defined to get it sorted in the end

	FILE* f1 = fopen((string(GetFolderName()) + "/index" + GetFileExtension()).c_str(), "w");
	if (!f1)
		return;

	StartPage(f1, "CryEngine ConsoleHTMLHelp", "main page");

	StartH1(f1, "Console Commands and Variables");

	LogVersion(f1);

	if (m_eWorkMode == eWM_HTML)
		fprintf(f1, "This list was exported from the engine by using the <b>DumpCommandsVars</b> console command.<br>\n\n");
	else if (m_eWorkMode == eWM_Confluence)
		fprintf(f1, "This list was exported from the engine by using the *DumpCommandsVars* console command.\n\n");
	else assert(0);

	// show all registered Prefix with one line
	{
		std::map<string, const char*>::const_iterator it, end = mapPrefix.end();

		StartH3(f1, "Registered Prefixes");

		for (it = mapPrefix.begin(); it != end; ++it)
		{
			const char* szLocalPrefix = it->first.c_str();    // can be 0 for remaining ones
			string sCleanPrefix = GetCleanPrefix(szLocalPrefix);
			string sPrefixName = SplitPrefixString_Part1(it->second);

			SingleLinePrefix(f1, sCleanPrefix.c_str(), sPrefixName.c_str(), (string("CONSOLEPREFIX") + FixAnchorName(sCleanPrefix.c_str()) + GetFileExtension()).c_str());
			//			fprintf(f1,"   * [[CONSOLEPREFIX%s][%s_ %s]]\n",FixAnchorName(sCleanPrefix.c_str()).c_str(),sCleanPrefix.c_str(),sPrefixName.c_str());
		}

		EndH3(f1);  // Registered prefixes
	}

	{
		std::map<string, const char*>::const_iterator it, it2, end = mapPrefix.end();

		StartH3(f1, "Console Commands and Variables Sorted by Prefix");

		for (it = mapPrefix.begin(); it != end; ++it)
		{
			const char* szLocalPrefix = it->first.c_str();    // can be 0 for remaining ones
			string sCleanPrefix = GetCleanPrefix(szLocalPrefix);
			string sPrefixName = SplitPrefixString_Part1(it->second);

			std::set<const char*, string_nocase_lt> setCmdAndVars;    // to get console variables and commands sorted together

			if (strcmp("___", szLocalPrefix) != 0)
			{
				// insert all starting with the prefix
				InsertConsoleVars(setCmdAndVars, szLocalPrefix);
				InsertConsoleCommands(setCmdAndVars, szLocalPrefix);
			}
			else
			{
				// insert all not starting with any of the prefix
				InsertConsoleVars(setCmdAndVars, mapPrefix);
				InsertConsoleCommands(setCmdAndVars, mapPrefix);
			}

			// -------------------------------

			string sSubName = string("CONSOLEPREFIX") + sCleanPrefix;

			StartPrefix(f1, sCleanPrefix.c_str(), sPrefixName.c_str(), (sSubName + GetFileExtension()).c_str());

			string sFileOut = string(GetFolderName()) + "/" + sSubName + GetFileExtension();
			FILE* f2 = fopen(sFileOut.c_str(), "w");
			if (!f2)
			{
				fclose(f1);
				return;
			}

			// headline
			{
				string sHeadline;

				if (sCleanPrefix.empty())
					sHeadline = "Console Commands and Variables Without Special Prefix";
				else
					sHeadline = string("Console Commands and Variables with Prefix ") + sCleanPrefix + "_";

				StartH1(f2, sHeadline.c_str());
			}

			Explanation(f2, SplitPrefixString_Part2(it->second));

			KeyValue(f2, "Possible Flags", CXConsole::GetFlagsString(0xffffffff));
			//			fprintf(f2,"<b>Possible Flags:</b><br>\n");
			//			fprintf(f2,"     <blockquote>%s</blockquote>",CXConsole::GetFlagsString(0xffffffff));

			// log console variables and commands
			{
				StartH3(f2, "Alphabetically Sorted");

				std::set<const char*, string_nocase_lt>::const_iterator itI, endI = setCmdAndVars.end();

				for (itI = setCmdAndVars.begin(); itI != endI; ++itI)
				{
					SingleLineEntry_InGlobal(f1, *itI, (sSubName + GetFileExtension() + "#Anchor" + FixAnchorName(*itI)).c_str());
					SingleLineEntry_InGroup(f2, *itI, (string("#Anchor") + FixAnchorName(*itI)).c_str());
				}

				EndH3(f2);
			}

			{
				StartH3(f2, "Console Variables and Commands");

				bool bFirst = true;
				{
					std::set<const char*, string_nocase_lt>::const_iterator itI, endI = setCmdAndVars.end();

					for (itI = setCmdAndVars.begin(); itI != endI; ++itI)
					{
						if (!bFirst)
							Separator(f2);
						bFirst = false;

						Anchor(f2, (string("Anchor") + FixAnchorName(*itI)).c_str());    // anchor

						IncludeSingleEntry(f2, *itI);
					}
				}
				EndH3(f2);
			}

			EndH1(f2);  // Console Commands and Variables ...

			fclose(f2);
			f2 = 0;

			EndPrefix(f1);
		}

		EndH3(f1);  // Console commands and variables sorted by prefix
	}

	EndH1(f1);
	EndPage(f1);

	fclose(f1);

	m_rParent.ConsoleLogInputResponse("successfully wrote directory %s", GetFolderName());
}

void CConsoleHelpGen::Separator(FILE* f) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<hr>\n");
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		//		fprintf(f,"----\n");
	}
	else assert(0);
}

void CConsoleHelpGen::Explanation(FILE* f, const char* szText) const
{
	if (m_eWorkMode == eWM_HTML)
	{
		fprintf(f, "<blockquote>%s</blockquote><br>\n<br>\n", szText);
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		//		fprintf(f,"{quote}%s{quote}\n\n",szText);
		fprintf(f, "%s\n\n", szText);
	}
	else assert(0);
}

const char* CConsoleHelpGen::GetFileExtension() const
{
	if (m_eWorkMode == eWM_HTML)
	{
		return ".html";
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
	}
	else assert(0);

	return "";
}

const char* CConsoleHelpGen::GetFolderName() const
{
	if (m_eWorkMode == eWM_HTML)
	{
		return "ConsoleHTMLHelp";
	}
	else if (m_eWorkMode == eWM_Confluence)
	{
		return "ConsoleHTMLHelp/CRYAUTOGEN";
	}
	else assert(0);

	return 0;
}

#endif  // CRY_PLATFORM_WINDOWS
