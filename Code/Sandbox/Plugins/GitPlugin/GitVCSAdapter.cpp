// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "GitVCSAdapter.h"
#include "GitOutputParser.h"
#include "GitLocalStatusMerger.h"
#include "GitExecutor.h"
#include "RevisionPersistance.h"
#include "GitDiffProvider.h"
#include "GitSyncronizer.h"
#include "TextFileEditor.h"
#include "ConflictResolutionStrategyProvider.h"
#include <vector>
#include <unordered_map>
#include "EditorFramework/Preferences.h"
#include "VersionControl/VersionControlCache.h"

namespace Private_GitVCSAdapter
{

const string WORKING_TREE_TAG = "working_tree";

struct SGitGeneralPreferences : public SPreferencePage
{
	SGitGeneralPreferences()
		: SPreferencePage("git", "Version Control/Git")
	{}

	bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("git", "Git");

		string oldRepository = repository;

		ar(repository, "repository", "Repository");
		ar.closeBlock();

		return true;
	}

	// TODO: this settings is not supposed to be exposed in settings. Use "git remote get-url origin" to detect it
	// in settings there should be path to git, path to work space, remote name, branch name?
	string repository{ "" };
};

SGitGeneralPreferences g_preferences;

// TODO: extract
string toString(const std::vector<string>& strings, const string& separator = " ")
{
	if (strings.empty())
		return "";

	string ret;
	for (const auto& str : strings)
	{
		ret += str + separator;
	}
	return ret.substr(0, ret.size() - separator.size());
}

static std::vector<string> ToNamesList(std::vector<CVersionControlFileStatus*> remoteNonDeleteChanges)
{
	std::vector<string> filePaths;
	filePaths.reserve(remoteNonDeleteChanges.size());
	for (auto fileStatus : remoteNonDeleteChanges)
	{
		filePaths.push_back(fileStatus->GetFileName());
	}
	return filePaths;
}

}

CGitVCSAdapter::CGitVCSAdapter(const string& rootPath)
	: m_executor(new CGitExecutor(rootPath))
	, m_persistance(new CRevisionPersistance(rootPath))
	, m_diffProvider(new CGitDiffProvider(m_executor.get(), m_persistance.get()))
	, m_localStatusMerger(new CGitLocalStatusMerger())
	, m_syncronizer(new CGitSyncronizer(m_executor.get()))
	, m_ignoreEditor(new CTextFileEditor(rootPath + "/.gitignore"))
{}

SVersionControlError CGitVCSAdapter::UpdateStatus()
{
	m_executor->UpdateRemote();
	std::vector<CVersionControlFileStatus> fileStatuses;
	m_diffProvider->GetDiff(fileStatuses);

	std::vector<CVersionControlFileStatus> statusResults;
	GetStatus(statusResults);

	// TODO: we can also check for all non-conflicting files (or maybe just added locally)
	if (fileStatuses.size() == 0)
	{
		m_syncronizer->SyncWithRemote(statusResults);
		fileStatuses = statusResults;
	}
	else
	{
		m_localStatusMerger->MergeIntoDiff(statusResults, fileStatuses);
	}

	UpdateConflictFlag(fileStatuses);
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::UpdateStatus(const std::vector<string>& filePaths, const std::vector<string>& folders)
{
	// TODO: optimize
	UpdateStatus();
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::UpdateConflictFlag(std::vector<CVersionControlFileStatus>& fileStatuses)
{
	for (auto& fileStatus : fileStatuses)

	{
		auto isConflicting = !fileStatus.IsUntouchedLocally() && !fileStatus.IsUntouchedRemotely();
		if (isConflicting)
			fileStatus.AddState(CVersionControlFileStatus::eState_Conflicting);
		else
			fileStatus.RemoveState(CVersionControlFileStatus::eState_Conflicting);
	}
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::GetLatest(const std::vector<string>& files, const std::vector<string>& folders, bool force)
{
	UpdateStatus();	

	std::vector<CVersionControlFileStatus*> remoteNonDeleteChanges;
	std::vector<string> remoteDeleted;
	std::vector<string> conflicts;
	/*for (auto& fileStatus : GetCache()->GetData())
	{
		if (fileStatus.GetConflicting())
		{
			conflicts.push_back(fileStatus.GetFileName());
		}
		else if (fileStatus.GetRemoteState() != EState::Unmodified)
		{
			if (fileStatus.GetRemoteState() == EState::Deleted)
			{
				remoteDeleted.push_back(fileStatus.GetFileName());
			}
			else
			{
				remoteNonDeleteChanges.push_back(&fileStatus);
			}
		}
	}*/

	if (!remoteDeleted.empty()) 
	{
		m_executor->RemoveFiles(remoteDeleted);
		SaveFilesRevisions(remoteDeleted);
	}

	if (!remoteNonDeleteChanges.empty())
	{
		std::vector<string> remoteNonDeleteChangesNames = Private_GitVCSAdapter::ToNamesList(remoteNonDeleteChanges);
		m_executor->Checkout(remoteNonDeleteChangesNames);
		if (!conflicts.empty())
		{
			m_executor->AddFiles(remoteNonDeleteChangesNames);
			string files;
			for (const auto& str : remoteNonDeleteChangesNames)
			{
				files += str + " ";
			}
			m_executor->Commit("checkout: " + files);
		}
		SaveFilesRevisions(remoteNonDeleteChangesNames);
		std::for_each(remoteNonDeleteChanges.begin(), remoteNonDeleteChanges.end(), [](CVersionControlFileStatus* fileStatus)
		{
			fileStatus->ClearStates();
		});
	}

	if (conflicts.empty())
	{
		m_executor->ResetMixed(m_executor->GetRemoteName() + "/master");
		return EVersionControlError::None;
	}

	return EVersionControlError::Unknown;
}

/*CVersionControlError CGitVCSAdapter::GetLatestFiles(const std::vector<string>& filePaths)
{
	using namespace Private_GitVCSAdapter;
	m_executor->UpdateRemote();
	
	std::vector<string> remoteRevisions;
	for (const auto& filePath : filePaths)
	{
		const auto& revision = GetFileRemoteRevision(filePath);
		m_persistance->Save(filePath, revision);
	}

	m_executor->Checkout(filePaths);
	m_executor->Reset();
	m_executor->AddFiles(filePaths);
	m_executor->Commit("checkout: " + toString(filePaths));
	return 0;
}*/

/*CVersionControlError CGitVCSAdapter::PublishFiles(std::vector<CVersionControlFileStatus>& filePaths)
{
	//if (!CanPush())
		// TODO: return error code
		return 1;

	using EState = CVersionControlFileStatus::EState;
	GetChanges(fileStatuses);
	for (const auto& fileStatus : fileStatuses)
	{
		if (fileStatus.GetConflicting())
			return false;
	}

	m_executor->ResetMixed(m_executor->GetRemoteName() + "/master");
	m_executor->AddAll();

	std::vector<CVersionControlFileStatus> statusResults;
	GetStatus(statusResults);
	string commitMessage;
	for (const auto& fileStatus : statusResults)
	{
		if (fileStatus.GetLocalState() == EState::Added)
		{
			commitMessage += "added: ";
		}
		else if (fileStatus.GetLocalState() == EState::Deleted)
		{
			commitMessage += "deleted: ";
		}
		else if (fileStatus.GetLocalState() == EState::Modified)
		{
			commitMessage += "modified: ";
		}
		commitMessage += fileStatus.GetFileName() + ", ";
	}

	m_executor->Commit(commitMessage.substr(0, commitMessage.length() - 2));
	CheckUpstreamAndPush();
	SaveFilesRevisions(statusResults, CGitOutputParser().parseRevision(m_executor->GetLatestRemoteRevision()));
	
	return true;
}*/

SVersionControlError CGitVCSAdapter::SubmitFiles(const std::vector<string>& filePaths, const string& message)
{
	using namespace Private_GitVCSAdapter;
	if (!CanPush())
		// TODO: return error code
		return EVersionControlError::Unknown;
	m_executor->AddFiles(filePaths);
	m_executor->Commit("save before push: " + toString(filePaths));
	m_executor->SetTag(WORKING_TREE_TAG);
	m_executor->ResetMixed(m_executor->GetRemoteName() + "/master");
	m_executor->AddFiles(filePaths);
	m_executor->Commit(message != "" ? message : "files: " + toString(filePaths));
	// TODO: test set upstream
	CheckUpstreamAndPush();
	m_executor->ResetMixed(WORKING_TREE_TAG);
	m_executor->RemoteTag(WORKING_TREE_TAG);
	SaveFilesRevisions(filePaths, CGitOutputParser().parseRevision(m_executor->GetLatestRemoteRevision()));
	return EVersionControlError::None;
}

void CGitVCSAdapter::ResolveWithStrategy(EConflictResolutionStrategy strategy, const std::vector<string>& filesNames)
{
	using namespace Private_GitVCSAdapter;
	if (strategy == EConflictResolutionStrategy::Add)
	{
		// TODO: is there any use case?
		m_executor->AddFiles(filesNames);
	}
	else if (strategy == EConflictResolutionStrategy::Delete)
	{
		m_executor->RemoveFiles(filesNames);
		m_executor->Commit("removed files :" + toString(filesNames, ","));
		SaveFilesRevisions(filesNames);
	}
	else if (strategy == EConflictResolutionStrategy::KeepOurs)
	{
		SaveFilesRevisions(filesNames);
	}
	else if (strategy == EConflictResolutionStrategy::TakeTheir)
	{
		m_executor->Checkout(filesNames);
		m_executor->AddFiles(filesNames);
		m_executor->Commit("resolved by taking their :" + toString(filesNames, ","));
		SaveFilesRevisions(filesNames);
	}
}

SVersionControlError CGitVCSAdapter::ResolveConflicts(const std::vector<SVersionControlFileConflictStatus>& conflictStatuses)
{
	using EConflictResolutionStrategy = IVersionControlAdapter::EConflictResolutionStrategy;

	std::vector<std::vector<string>> resolutions = std::vector<std::vector<string>>(static_cast<int>(EConflictResolutionStrategy::Count));
	//CConflictResolutionStrategyProvider strategyProvider;
	/*for (const auto& conflictStatus : conflictStatuses)
	{
		const auto& fileStatus = fileStatuses.find(conflictStatus.first);
		EConflictResolutionStrategy strategy = strategyProvider.Provide(*fileStatus->second, conflictStatus.second);
		resolutions[static_cast<int>(strategy)].push_back(conflictStatus.first);
	}*/

	int strategy = static_cast<int>(EConflictResolutionStrategy::Add);
	if (resolutions[strategy].size() > 0)
		ResolveWithStrategy(EConflictResolutionStrategy::Add, resolutions[strategy]);

	strategy = static_cast<int>(EConflictResolutionStrategy::Delete);
	if (resolutions[strategy].size() > 0)
		ResolveWithStrategy(EConflictResolutionStrategy::Delete, resolutions[strategy]);

	strategy = static_cast<int>(EConflictResolutionStrategy::KeepOurs);
	if (resolutions[strategy].size() > 0)
		ResolveWithStrategy(EConflictResolutionStrategy::KeepOurs, resolutions[strategy]);

	strategy = static_cast<int>(EConflictResolutionStrategy::TakeTheir);
	if (resolutions[strategy].size() > 0)
		ResolveWithStrategy(EConflictResolutionStrategy::TakeTheir, resolutions[strategy]);

	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::AddFiles(const std::vector<string>& filePaths)
{
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::EditFiles(const std::vector<string>& filePaths)
{
	//const string& revision = GetFileLocalRevision(filePath);
	//m_persistance->Save(filePath, revision);
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::DeleteFiles(const std::vector<string>& filePaths)
{
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::Revert(const std::vector<string>& files, const std::vector<string>& folders)
{
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::RevertUnchanged(const std::vector<string>& files, const std::vector<string>& folders)
{
	return EVersionControlError::None;
}

SVersionControlError CGitVCSAdapter::CheckSettings()
{
	return EVersionControlError::None;
}

SPreferencePage* CGitVCSAdapter::GetPreferences()
{
	using namespace Private_GitVCSAdapter;
	return &g_preferences;
}

void CGitVCSAdapter::GetStatus(std::vector<CVersionControlFileStatus>& fileStatuses)
{
	std::vector<string> conflicts;
	CGitOutputParser().parseStatus(m_executor->GetStatus(), fileStatuses, conflicts);
}

string CGitVCSAdapter::GetFileRemoteRevision(const string& filePath)
{
	return CGitOutputParser().parseRevision(m_executor->GetFileRemoteRevision(filePath));
}

string CGitVCSAdapter::GetFileLocalRevision(const string& filePath)
{
	return CGitOutputParser().parseRevision(m_executor->GetFileLocalRevision(filePath));
}

void CGitVCSAdapter::SaveFilesRevisions(const std::vector<string>& filesNames)
{
	for (const auto& fileName : filesNames)
	{
		string revision = GetFileRemoteRevision(fileName);
		m_persistance->Save(fileName, revision);
	}
}

void CGitVCSAdapter::SaveFilesRevisions(const std::vector<string>& filesNames, const string& revision)
{
	for (const auto& fileName : filesNames)
	{
		m_persistance->Save(fileName, revision);
	}
}

void CGitVCSAdapter::SaveFilesRevisions(const std::vector<CVersionControlFileStatus>& fileStatuses, const string& revision)
{
	for (const auto& fileStatus : fileStatuses)
	{
		m_persistance->Save(fileStatus.GetFileName(), revision);
	}
}

void CGitVCSAdapter::GetAllLocalRevisions(std::unordered_map<string, string, stl::hash_strcmp<string>>& fileRevisions)
{
	std::vector<string> fileNames;
	CGitOutputParser().parseFileNames(m_executor->ListLocalFiles(), fileNames);
	for (const auto& fileName : fileNames)
	{
		fileRevisions[fileName] = GetFileLocalRevision(fileName);
	}
}

bool CGitVCSAdapter::CanPush()
{
	bool bCanPush = !CGitOutputParser().HasErrors(m_executor->GetRemoteUrl());
	return bCanPush &= !CGitOutputParser().HasErrors(m_executor->UpdateRemote());
}

void CGitVCSAdapter::CheckUpstreamAndPush()
{
	bool bSetUpstream = CGitOutputParser().HasErrors(m_executor->GetUpstreamBranch());
	m_executor->Push(bSetUpstream);
}

/*EVersionControlError CGitVCSAdapter::IsInitialized(bool& result)
{
	const auto& gitOutput = m_executor->GetBranch();
	result = !CGitOutputParser().HasEVersionControlErrors(gitOutput) && gitOutput != "";
	return 0;
}

EVersionControlError CGitVCSAdapter::Initialize()
{
	m_executor->Init();
	if (!m_ignoreEditor->DoesFileExist())
	{
		m_ignoreEditor->CreateFile();
	}
	m_ignoreEditor->AppendLine(m_persistance->GetFilePath());
	m_executor->Reset();
	m_executor->AddFiles({ m_ignoreEditor->GetFilePath() });
	m_executor->Commit("initial commit");
	return 0;
}

EVersionControlError CGitVCSAdapter::IgnoreFiles(const std::vector<string>& patterns)
{
	for (const auto& pattern : patterns)
	{
		m_ignoreEditor->AppendLine(pattern);
	}
	m_executor->Reset();
	m_executor->AddFiles({ m_ignoreEditor->GetFilePath() });
	m_executor->Commit("updated gitignore");
	return 0;
}

EVersionControlError CGitVCSAdapter::SetRepoLocation(const string& repoPath)
{
	string oldUrl = m_executor->GetRemoteUrl();
	bool bHadOldUrl = !CGitOutputParser().HasEVersionControlErrors(oldUrl);
	if (bHadOldUrl)
	{
		m_executor->SetRemoteUrl(repoPath);
	}
	else
	{
		m_executor->AddRemote("origin", repoPath);
		m_executor->SetUpstream();
	}
	bool bIsBadRemote = CGitOutputParser().HasEVersionControlErrors(m_executor->UpdateRemote());
	if (bIsBadRemote)
	{
		if (bHadOldUrl)
		{
			m_executor->SetRemoteUrl(oldUrl);
		}
		else
		{
			m_executor->RemoveRemote("origin");
		}
		return 1;
	}
	return 0;
}

EVersionControlError CGitVCSAdapter::GetLastHistoryStep(string& result)
{
	result = m_executor->GetLatestRemoteRevision();
	return 0;
}

EVersionControlError CGitVCSAdapter::UpdateSettings(const void*, bool& result)
{
	result = false;
	return 1;
}*/
