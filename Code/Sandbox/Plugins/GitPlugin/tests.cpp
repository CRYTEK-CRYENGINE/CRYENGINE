// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
//#include "VersionControl/VersionControl.h"
#include "GitOutputParser.h"
#include "ConflictResolutionStrategyProvider.h"
#include "GitLocalStatusMerger.h"
#include "GitVCSAdapter.h"
#include "GitDiffProvider.h"
#include "GitSyncronizer.h"
#include "ITextFileEditor.h"
#include <iostream>
#include <cstdarg>
//#include <CrySystem/CryUnitTest.h>

/*namespace 
{

std::vector<string> toStringList(std::initializer_list<string> list)
{
	return list;
}

using EState = CVersionControlFileStatus::EState;

CRY_UNIT_TEST_SUITE(Parser_Status)
{

	CRY_UNIT_TEST_FIXTURE(StatusTest)
	{
	public:
		void Done() override
		{
			m_fileStatuses.clear();
			conflicts.clear();
		}

	protected:
		void parseGitOutput(string gitOutput, bool swapAttributes = false)
		{
			if (gitOutput.size() > 0)
				gitOutput.append("\n");
			gitOutput = "#\n" + gitOutput;
			CGitOutputParser().parseStatus(gitOutput, m_fileStatuses, conflicts, swapAttributes);
		}

		void AssertSize(int size)
		{
			CRY_UNIT_TEST_CHECK_EQUAL(m_fileStatuses.size(), size);
		}

		void AssertElement(int index, const string& fileName, EState localState, EState remoteState = EState::Unmodified, 
			bool bIsConflicting = false)
		{
			const auto& fileStatus = m_fileStatuses[index];
			CRY_UNIT_TEST_CHECK_EQUAL(fileStatus.GetFileName(), fileName);
			CRY_UNIT_TEST_ASSERT(fileStatus.GetLocalState() == localState);
			CRY_UNIT_TEST_ASSERT(fileStatus.GetRemoteState() == remoteState);
			CRY_UNIT_TEST_CHECK_EQUAL(fileStatus.GetConflicting(), bIsConflicting);
		}

	private:
		std::vector<CVersionControlFileStatus> m_fileStatuses;
		std::vector<string> conflicts;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(EmptyOutputGivesEmptyList, StatusTest)
	{
		parseGitOutput("");

		AssertSize(0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(FileNameIsParsed, StatusTest)
	{
		parseGitOutput(" M file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(Output_MIsModified, StatusTest)
	{
		parseGitOutput(" M file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputM_IsModified, StatusTest)
	{
		parseGitOutput("M  file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputA_IsAdded, StatusTest)
	{
		parseGitOutput("A  file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Added, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputQuestionMakrsIsAdded, StatusTest)
	{
		parseGitOutput("?? file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Added, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(Output_DIsDeleted, StatusTest)
	{
		parseGitOutput(" D file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputD_IsDeleted, StatusTest)
	{
		parseGitOutput("D  file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputDDIsDeletedAndDeprecated, StatusTest)
	{
		parseGitOutput("DD file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Deleted, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputAUIsAddedAndOutdated, StatusTest)
	{
		parseGitOutput("AU file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Added, EState::Modified, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputUDIsModifiedAndDeprecated, StatusTest)
	{
		parseGitOutput("UD file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified, EState::Deleted, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputUAIsModifiedAndNew, StatusTest)
	{
		parseGitOutput("UA file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified, EState::Added, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputDUIsDeletedAndOutdated, StatusTest)
	{
		parseGitOutput("DU file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Modified, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputAAIsAddedAndNew, StatusTest)
	{
		parseGitOutput("AA file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Added, EState::Added, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(OutputUUIsModifiedAndOutdated, StatusTest)
	{
		parseGitOutput("UU file.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Modified, EState::Modified, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(SwapAttributes, StatusTest)
	{
		parseGitOutput("UD file.txt", true);

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Modified, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(ParsingOfTwoLines, StatusTest)
	{
		parseGitOutput("UD file1.txt\nA  file2.txt");

		AssertSize(2);
		AssertElement(0, "file1.txt", EState::Modified, EState::Deleted, true);
		AssertElement(1, "file2.txt", EState::Added, EState::Unmodified, false);
	}
}

CRY_UNIT_TEST_SUITE(Parser_Diff)
{
	CRY_UNIT_TEST_FIXTURE(ParseDiffTest)
	{
	public:
		void Done() override
		{
			m_fileStatuses.clear();
		}

	protected:
		void parseGitOutput(string gitOutput, bool bRemote = true)
		{
			if (gitOutput.size() > 0)
				gitOutput.append("\n");
			CGitOutputParser().parseDiff(gitOutput, m_fileStatuses, bRemote);
		}

		void AssertSize(int size)
		{
			CRY_UNIT_TEST_CHECK_EQUAL(m_fileStatuses.size(), size);
		}

		void AssertElement(int index, const string& fileName, EState localState, EState remoteState = EState::Unmodified, bool bIsConflicting = false)
		{
			const auto& fileStatus = m_fileStatuses[index];
			CRY_UNIT_TEST_ASSERT(fileStatus.GetFileName() == fileName);
			CRY_UNIT_TEST_ASSERT(fileStatus.GetLocalState() == localState);
			CRY_UNIT_TEST_ASSERT(fileStatus.GetRemoteState() == remoteState);
			CRY_UNIT_TEST_CHECK_EQUAL(fileStatus.GetConflicting(), bIsConflicting);
		}

	private:
		std::vector<CVersionControlFileStatus> m_fileStatuses;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(EmptyOutputGivesEmptyList, ParseDiffTest)
	{
		parseGitOutput("");

		AssertSize(0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(FileNameIsParsed, ParseDiffTest)
	{
		parseGitOutput("M\tfile.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Unmodified, EState::Modified);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(OutputMIsModified, ParseDiffTest)
	{
		parseGitOutput("M\tfile.txt");
		AssertSize(1);
		AssertElement(0, "file.txt", EState::Unmodified, EState::Modified);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(OutputAIsAdded, ParseDiffTest)
	{
		parseGitOutput("A\tfile.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Unmodified, EState::Added);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(OutputDIsDeleted, ParseDiffTest)
	{
		parseGitOutput("D\tfile.txt");

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Unmodified, EState::Deleted);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LocalChanges, ParseDiffTest)
	{
		parseGitOutput("D\tfile.txt", false);

		AssertSize(1);
		AssertElement(0, "file.txt", EState::Deleted, EState::Unmodified);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(ParsingOfTwoLines, ParseDiffTest)
	{
		parseGitOutput("D\tfile1.txt\nA\tfile2.txt");

		AssertSize(2);
		AssertElement(0, "file1.txt", EState::Unmodified, EState::Deleted);
		AssertElement(1, "file2.txt", EState::Unmodified, EState::Added);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(ParsingRenaming, ParseDiffTest)
	{
		parseGitOutput("R050\td\tg");

		AssertSize(2);
		AssertElement(0, "d", EState::Unmodified, EState::Deleted);
		AssertElement(1, "g", EState::Unmodified, EState::Added);
	}
};

CRY_UNIT_TEST_SUITE(Parser_Revision)
{
	CRY_UNIT_TEST(LineEndIsTrimmed)
	{
		string revision = CGitOutputParser().parseRevision("67092baa9594947a56370e2a70f4fa5e6c2d0c99\n");
		CRY_UNIT_TEST_CHECK_EQUAL(revision, "67092baa9594947a56370e2a70f4fa5e6c2d0c99");
	}
}

CRY_UNIT_TEST_SUITE(Parser_FileNames)
{
	CRY_UNIT_TEST(MultipleLines)
	{
		std::vector<string> fileNames;
		CGitOutputParser().parseFileNames("file1\nfile2\nfile3\n", fileNames);
		CRY_UNIT_TEST_CHECK_EQUAL(fileNames[0], "file1");
		CRY_UNIT_TEST_CHECK_EQUAL(fileNames[1], "file2");
		CRY_UNIT_TEST_CHECK_EQUAL(fileNames[2], "file3");
	}
}

CRY_UNIT_TEST_SUITE(Parser_Rebase)
{
	using ERebaseResult = CGitOutputParser::ERebaseResult;

	CRY_UNIT_TEST(ContinueGivesNoChanges)
	{
		ERebaseResult result = CGitOutputParser().parseRebaseContinue(string(
			"Applying: M c\n") +
			"No changes - did you forget to use 'git add' ?\n" +
			"If there is nothing left to stage, chances are that something else\n" +
			"already introduced the same changes; you might want to skip this patch.\n\n" +
			"When you have resolved this problem, run \"git rebase --continue\".\n" +
			"If you prefer to skip this patch, run \"git rebase --skip\" instead.\n" +
			"To check out the original branch and stop rebasing, run \"git rebase --abort\".\n");
		CRY_UNIT_TEST_ASSERT(result == ERebaseResult::NoChanges);
	}

	CRY_UNIT_TEST(ContinueGivesNoOutput)
	{
		ERebaseResult result = CGitOutputParser().parseRebaseContinue("Applying: M b\n");
		CRY_UNIT_TEST_ASSERT(result == ERebaseResult::Done);
		result = CGitOutputParser().parseRebaseContinue("");
		CRY_UNIT_TEST_ASSERT(result == ERebaseResult::Done);
	}

	CRY_UNIT_TEST(ContinueGivesNeedsMerge)
	{
		ERebaseResult result = CGitOutputParser().parseRebaseContinue(string(
			"c: needs merge\n") +
			"file4.txt : needs merge\n" +
			"You must edit all merge conflicts and then\n" +
			"mark them as resolved using git add\n");
		CRY_UNIT_TEST_ASSERT(result == ERebaseResult::Conflicts);
	}

	CRY_UNIT_TEST(ContinueGivesNoRebase)
	{
		ERebaseResult result = CGitOutputParser().parseRebaseContinue("No rebase in progress ?\n");
		CRY_UNIT_TEST_ASSERT(result == ERebaseResult::NoRebase);
	}
}

CRY_UNIT_TEST_SUITE(ConflictResolution_StrategyProvider)
{
	using EConflictResolutionStrategy = IVersionControlAdapter::EConflictResolutionStrategy;

	CRY_UNIT_TEST_FIXTURE(ConflictResolutionStrategyProviderTest)
	{
	protected:
		EConflictResolutionStrategy Provide(EState localState, EState remoteState, EConflictResolution resolution, bool bConflicting = true)
		{
			CVersionControlFileStatus fileStatus = CVersionControlFileStatus("file.txt");
			fileStatus.SetLocalState(localState);
			fileStatus.SetRemoteState(remoteState);
			fileStatus.SetConflicting(bConflicting);

			return strategyProvider.Provide(fileStatus, resolution);
		}

		CConflictResolutionStrategyProvider strategyProvider;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfFileIsNotInConflict_NoStrategyProvided, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Unmodified, EConflictResolution::Ours, false) == EConflictResolutionStrategy::None);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoResolutionIsChosen_NoStrategyProvided, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Modified, EConflictResolution::None) == EConflictResolutionStrategy::None);
	}

	// Signature: L<LocalState>R<RemoteState><Resolution>_<Strategy>

	CRY_UNIT_TEST_WITH_FIXTURE(LDeletedRDeletedOurs_ToDelete, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Deleted, EState::Deleted, EConflictResolution::Ours) == EConflictResolutionStrategy::Delete);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LDeletedRDeletedTheir_ToDelete, ConflictResolutionStrategyProviderTest) 
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Deleted, EState::Deleted, EConflictResolution::Their) == EConflictResolutionStrategy::Delete);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LDeletedRModifiedOurs_ToDelete, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Deleted, EState::Modified, EConflictResolution::Ours) == EConflictResolutionStrategy::Delete);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LDeletedRModifiedTheir_ToAdd, ConflictResolutionStrategyProviderTest)
	{
		//CRY_UNIT_TEST_ASSERT(Provide(EState::Deleted, EState::Modified, EConflictResolution::Their) == EConflictResolutionStrategy::Add);
		CRY_UNIT_TEST_ASSERT(Provide(EState::Deleted, EState::Modified, EConflictResolution::Their) == EConflictResolutionStrategy::TakeTheir);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LAddedRAddedOurs_ToKeepOurs, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Added, EState::Added, EConflictResolution::Ours) == EConflictResolutionStrategy::KeepOurs);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LAddedRAddedTheir_ToTakeTheir, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Added, EState::Added, EConflictResolution::Their) == EConflictResolutionStrategy::TakeTheir);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LModifiedRDeletedOurs_ToAdd, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Deleted, EConflictResolution::Ours) == EConflictResolutionStrategy::KeepOurs);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LModifiedRDeletedTheir_ToDelete, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Deleted, EConflictResolution::Their) == EConflictResolutionStrategy::Delete);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LModifiedRModifiedOurs_ToKeepOurs, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Modified, EConflictResolution::Ours) == EConflictResolutionStrategy::KeepOurs);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(LModifiedRModifiedTheir_ToTakeTheir, ConflictResolutionStrategyProviderTest)
	{
		CRY_UNIT_TEST_ASSERT(Provide(EState::Modified, EState::Modified, EConflictResolution::Their) == EConflictResolutionStrategy::TakeTheir);
	}

};

CRY_UNIT_TEST_SUITE(Status_LocalMerger)
{
	CRY_UNIT_TEST_FIXTURE(DiffMergeTest)
	{
	public:
		void Done() override
		{
			m_statusParseResult.clear();
			m_diffParseResult.clear();
		}

	protected:
		void Merge()
		{
			CGitLocalStatusMerger().MergeIntoDiff(m_statusParseResult, m_diffParseResult);
		}

		void AddToStatus(EState localState, EState remoteState = EState::Unmodified, bool bIsConflicting = false, const string& fileName = "file.txt")
		{
			CVersionControlFileStatus fileStatus = CVersionControlFileStatus(fileName);
			fileStatus.SetLocalState(localState);
			fileStatus.SetRemoteState(remoteState);
			fileStatus.SetConflicting(bIsConflicting);
			m_statusParseResult.push_back(fileStatus);
		}

		void AddToDiff(EState localState, EState remoteState = EState::Unmodified, bool bIsConflicting = false, const string& fileName = "file.txt")
		{
			CVersionControlFileStatus fileStatus = CVersionControlFileStatus(fileName);
			fileStatus.SetLocalState(localState);
			fileStatus.SetRemoteState(remoteState);
			fileStatus.SetConflicting(bIsConflicting);
			m_diffParseResult.push_back(fileStatus);
		}

		void AssertSize(int size)
		{
			CRY_UNIT_TEST_CHECK_EQUAL(m_diffParseResult.size(), size);
		}

		void AssertElement(int index, EState localState, EState remoteState = EState::Unmodified, bool bIsConflicting = false)
		{
			const auto& fileStatus = m_diffParseResult[index];
			CRY_UNIT_TEST_ASSERT(fileStatus.GetLocalState() == localState);
			CRY_UNIT_TEST_ASSERT(fileStatus.GetRemoteState() == remoteState);
			CRY_UNIT_TEST_CHECK_EQUAL(fileStatus.GetConflicting(), bIsConflicting);
		}

	private:
		std::vector<CVersionControlFileStatus> m_statusParseResult;
		std::vector<CVersionControlFileStatus> m_diffParseResult;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfBothEmpty_ResultEmpty, DiffMergeTest)
	{
		Merge();
		AssertSize(0);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfStatusEmpty_DiffUnchanged, DiffMergeTest)
	{
		AddToDiff(EState::Modified, EState::Deleted);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Modified, EState::Deleted);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfDiffEmpty_StatusPopulatesIt, DiffMergeTest)
	{
		AddToStatus(EState::Modified, EState::Deleted);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Modified, EState::Deleted);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(MultipleRecords, DiffMergeTest)
	{
		AddToDiff(EState::Modified, EState::Deleted);
		AddToDiff(EState::Deleted, EState::Unmodified);
		Merge();

		AssertSize(2);
		AssertElement(0, EState::Modified, EState::Deleted);
		AssertElement(1, EState::Deleted, EState::Unmodified);
	};

	// Signature: LocalD<Diff>S<Status>_<Result>

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDModifiedSModified_Modified, DiffMergeTest)
	{
		AddToDiff(EState::Modified);
		AddToStatus(EState::Modified);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Modified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDModifiedSDeleted_Deleted, DiffMergeTest)
	{
		AddToDiff(EState::Modified);
		AddToStatus(EState::Deleted);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Deleted);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDAddedSModified_Added, DiffMergeTest)
	{
		AddToDiff(EState::Added);
		AddToStatus(EState::Modified);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Added);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDAddedSDeleted_Unmodified, DiffMergeTest)
	{
		AddToDiff(EState::Added);
		AddToStatus(EState::Deleted);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDDeletedSAdded_Modified, DiffMergeTest)
	{
		AddToDiff(EState::Deleted);
		AddToStatus(EState::Added);
		Merge();

		AssertSize(1);
		AssertElement(0, EState::Modified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(WhenConflicting_StatusHasPriority, DiffMergeTest)
	{
		AddToDiff(EState::Added, EState::Unmodified, false, "file1");
		AddToStatus(EState::Modified, EState::Modified, true, "file1");

		AddToDiff(EState::Added, EState::Unmodified, false, "file2");
		AddToStatus(EState::Deleted, EState::Modified, true, "file2");

		AddToDiff(EState::Deleted, EState::Unmodified, false, "file3");
		AddToStatus(EState::Added, EState::Modified, true, "file3");

		Merge();

		AssertSize(3);
		AssertElement(0, EState::Modified, EState::Modified, true);
		AssertElement(1, EState::Deleted, EState::Modified, true);
		AssertElement(2, EState::Added, EState::Modified, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(WhenDiffLocallyUnmodified_ReturnLocalStatus, DiffMergeTest)
	{
		AddToDiff(EState::Unmodified, EState::Modified, false, "file1");
		AddToStatus(EState::Modified, EState::Unmodified, false, "file1");

		AddToDiff(EState::Unmodified, EState::Modified, false, "file2");
		AddToStatus(EState::Deleted, EState::Unmodified, false, "file2");

		AddToDiff(EState::Unmodified, EState::Deleted, false, "file3");
		AddToStatus(EState::Modified, EState::Unmodified, false, "file3");

		Merge();

		AssertSize(3);
		AssertElement(0, EState::Modified, EState::Modified, false);
		AssertElement(1, EState::Deleted, EState::Modified, false);
		AssertElement(2, EState::Modified, EState::Deleted, false);
	};
}

namespace GitAdapter 
{

class DummyGitExecutor : public IGitExecutor
{
public:
	string Reset() override { return ""; }

	string AddFiles(const std::vector<string>& files) override { return ""; }

	string AddAll() override { return ""; }

	string UpdateRemote() override { return ""; }

	string Commit(const string& message) override { return ""; }

	string Checkout(const std::vector<string>& files) override { return ""; }

	string Push(bool bSetUpstream = false) override { return ""; }

	string SetTag(const string& tag) override { return ""; }

	string RemoteTag(const string& tag) override { return ""; }

	string RemoveFiles(const std::vector<string>& filesNames) override { return ""; }

	string ResetMixed(const string& branch) override { return ""; }

	string GetLatestLocalRevision() override { return ""; }

	string GetLatestRemoteRevision() override { return ""; }

	string DiffWithRemote(const string& branch) override { return ""; }

	string DiffWithHead(const string& branch) override { return ""; }

	string GetStatus() override { return ""; }

	string GetFileRemoteRevision(const string& filePath) override { return ""; }

	string GetFileLocalRevision(const string& filePath) override { return ""; }

	string ListLocalFiles() override { return ""; }

	string GetMergeBase() override { return ""; }

	string GetBranch() override { return ""; }

	string GetRemoteName() override { return ""; }

	string GetRemoteUrl() override { return ""; }

	string SetRemoteUrl(const string& repoPath) override { return ""; }

	string AddRemote(const string& name, const string& url) override { return ""; }

	string RemoveRemote(const string& name) override { return ""; }

	string SetUpstream() override { return ""; }

	string GetUpstreamBranch() override { return ""; }

	string Init() override { return ""; }
};

class StubGitExecutor : public DummyGitExecutor
{
public:
	string DiffWithRemote(const string& branch) override { return m_stubDiffRemote; }

	string DiffWithHead(const string& branch) override { return m_stubDiffHead; }

	string GetFileRemoteRevision(const string& filePath) override { return m_stubGetFileRemoteRevision; }

	string GetStatus() override { return m_stubGetStatus; }

	string GetRemoteUrl() override { return m_stubGetRemoteUrl; }

	string UpdateRemote() override { return m_stubUpdateRemote; }

	string GetUpstreamBranch() override { return m_stubGetUpstreamBranch; }

	string GetBranch() override { return m_stubGetBranch; }

	void StubDiffWithRemote(string stub) { m_stubDiffRemote = std::move(stub); }

	void StubDiffWithHead(string stub) { m_stubDiffHead = std::move(stub); }

	void StubGetFileRemoteRevision(string stub) { m_stubGetFileRemoteRevision = std::move(stub); }

	void StubGetStatus(string stub) { m_stubGetStatus = std::move(stub); }

	void StubGetRemoteUrl(string stub) { m_stubGetRemoteUrl = std::move(stub); }

	void StubUpdateRemote(string stub) { m_stubUpdateRemote = std::move(stub); }

	void StubGetUpstreamBranch(string stub) { m_stubGetUpstreamBranch = std::move(stub); }

	void StubGetBranch(string stub) { m_stubGetBranch = std::move(stub); }

private:
	string m_stubDiffRemote;
	string m_stubDiffHead;
	string m_stubGetFileRemoteRevision;
	string m_stubGetStatus;
	string m_stubGetRemoteUrl;
	string m_stubUpdateRemote;
	string m_stubGetUpstreamBranch;
	string m_stubGetBranch;
};

class SpyGitExecutor : public StubGitExecutor
{
public:
	string UpdateRemote() override
	{
		++m_numCallsUpdateRemote;
		return StubGitExecutor::UpdateRemote();
	}

	string AddAll() override
	{
		++m_numCallsAddAll;
		return StubGitExecutor::AddAll();
	}

	string Checkout(const std::vector<string>& filesNames) override
	{
		++m_numCallsCheckout;
		m_paramsCheckout = filesNames;
		return StubGitExecutor::Checkout(filesNames);
	}

	string AddFiles(const std::vector<string>& filesNames) override
	{
		++m_numCallsAddFiles;
		m_paramsAddFiles = filesNames;
		return StubGitExecutor::AddFiles(filesNames);
	}

	string Commit(const string& message) override
	{
		++m_numCallsCommit;
		return StubGitExecutor::Commit(message);
	}

	string ResetMixed(const string& branch) override
	{
		++m_numCallsResetMixed;
		return StubGitExecutor::ResetMixed(branch);
	}

	string RemoveFiles(const std::vector<string>& filesNames) override
	{
		++m_numCallsRemoveFiles;
		m_paramsRemoveFiles = filesNames;
		return StubGitExecutor::RemoveFiles(filesNames);
	}

	string Push(bool bSetUpstream = false) override
	{
		++m_numCallsPush;
		m_paramPush = bSetUpstream;
		return StubGitExecutor::Push(bSetUpstream);
	}

	string GetRemoteUrl() override
	{
		++m_numCallsGetRemoteUrl;
		return StubGitExecutor::GetRemoteUrl();
	}

	string SetRemoteUrl(const string& repoPath) override
	{
		++m_numCallsSetRemoteUrl;
		return StubGitExecutor::SetRemoteUrl(repoPath);
	}

	string AddRemote(const string& name, const string& url) override
	{
		++m_numCallsAddRemote;
		return StubGitExecutor::AddRemote(name, url);
	}

	string RemoveRemote(const string& name) override
	{
		++m_numCallsRemoveRemote;
		return StubGitExecutor::RemoveRemote(name);
	}

	string SetUpstream() override
	{
		++m_numCallSetUpstream;
		return StubGitExecutor::SetUpstream();
	}

	int GetNumCallsUpdateRemote() const { return m_numCallsUpdateRemote; }
	int GetNumCallsAddAll() const { return m_numCallsAddAll; }
	int GetNumCallsCheckout() const { return m_numCallsCheckout; }
	int GetNumCallsAddFiles() const { return m_numCallsAddFiles; }
	int GetNumCallsCommit() const { return m_numCallsCommit; }
	int GetNumCallsResetMixed() const { return m_numCallsResetMixed; }
	int GetNumCallsRemoveFiles() const { return m_numCallsRemoveFiles; }
	int GetNumCallsPush() const { return m_numCallsPush; }
	int GetNumCallsGetRemoteUrl() const { return m_numCallsGetRemoteUrl; }
	int GetNumCallsSetRemoteUrl() const { return m_numCallsSetRemoteUrl; }
	int GetNumCallsAddRemote() const { return m_numCallsAddRemote; }
	int GetNumCallsRemoveRemote() const { return m_numCallsRemoveRemote; }
	int GetNumCallsSetUpstream() const { return m_numCallSetUpstream; }

	const std::vector<string>& GetParamsToCheckout() const { return m_paramsCheckout; }
	const std::vector<string>& GetParamsToAddFiles() const { return m_paramsAddFiles; }
	const std::vector<string>& GetParamsToRemoveFiles() const { return m_paramsRemoveFiles; }
	bool GetParamToPush() const { return m_paramPush; }

private:
	int m_numCallsUpdateRemote{ 0 };
	int m_numCallsAddAll{ 0 };
	int m_numCallsCheckout{ 0 };
	int m_numCallsAddFiles{ 0 };
	int m_numCallsCommit{ 0 };
	int m_numCallsResetMixed{ 0 };
	int m_numCallsRemoveFiles{ 0 };
	int m_numCallsPush{ 0 };
	int m_numCallsGetRemoteUrl{ 0 };
	int m_numCallsSetRemoteUrl{ 0 };
	int m_numCallsAddRemote{ 0 };
	int m_numCallsRemoveRemote{ 0 };
	int m_numCallSetUpstream{ 0 };

	std::vector<string> m_paramsCheckout;
	std::vector<string> m_paramsAddFiles;
	std::vector<string> m_paramsRemoveFiles;
	bool m_paramPush{ false };
};

class MockGitExecutor : public SpyGitExecutor
{
public:
	enum class EFunc
	{
		ResetMixed,
		RemoveFiles
	};

	string ResetMixed(const string& branch) override
	{
		m_callSequence.push_back(EFunc::ResetMixed);
		return SpyGitExecutor::ResetMixed(branch);
	}

	string RemoveFiles(const std::vector<string>& filesNames) override
	{
		m_callSequence.push_back(EFunc::RemoveFiles);
		return SpyGitExecutor::RemoveFiles(filesNames);
	}

	void AssertCallSequence(int n_args, ...)
	{
		std::vector<EFunc> expected;
		va_list ap;
		va_start(ap, n_args);
		for (int i = 1; i <= n_args; i++) 
		{
			expected.push_back(va_arg(ap, EFunc));
		}
		va_end(ap);
		CRY_UNIT_TEST_ASSERT(m_callSequence == expected);
	}

private:
	std::vector<EFunc> m_callSequence;
};

class DummyGitLocalStatusMerger : public IGitLocalStatusMerger
{
public:
	void MergeIntoDiff(const std::vector<CVersionControlFileStatus>& statusParseResult, std::vector<CVersionControlFileStatus>& diffParseResult) override {}
};

class StubGitLocalStatusMerger : public DummyGitLocalStatusMerger 
{
public:
	void MergeIntoDiff(const std::vector<CVersionControlFileStatus>& statusParseResult, std::vector<CVersionControlFileStatus>& diffParseResult) override
	{
		if (m_bMergeInfoDifFStubbed)
			diffParseResult = m_stubMergeIntoDiff;
	}

	void StubMergeIntoDiff(const std::vector<CVersionControlFileStatus>& stub)
	{
		m_bMergeInfoDifFStubbed = true;
		m_stubMergeIntoDiff = stub;
	}

private:
	std::vector<CVersionControlFileStatus> m_stubMergeIntoDiff;

	bool m_bMergeInfoDifFStubbed = false;
};

class SpyGitLocalStatusMerger : public StubGitLocalStatusMerger
{
public:
	void MergeIntoDiff(const std::vector<CVersionControlFileStatus>& statusParseResult, std::vector<CVersionControlFileStatus>& diffParseResult) override
	{
		++m_numCallsMergeIntoDiff;
		StubGitLocalStatusMerger::MergeIntoDiff(statusParseResult, diffParseResult);
	}

	int GetNumCallsMergeIntoDiff() { return m_numCallsMergeIntoDiff; }

private:
	int m_numCallsMergeIntoDiff{ 0 };
};

class DummyRevisionPersistance : public IRevisionPersistance
{
public:
	void Save(const string& filePath, const string& revision) override {}

	string Load(const string& filePath) override { return ""; }

	string GetFilePath() const override { return ""; }
};

class StubRevisionPersistance : public DummyRevisionPersistance
{
public:
	string Load(const string& filePath) override 
	{ 
		return m_revisions[filePath];
	}

	void StubFileRevision(const string& filePath, const string& revision)
	{
		m_revisions[filePath] = revision;
	}

private:
	std::unordered_map<string, string, stl::hash_strcmp<string>> m_revisions;
};

class SpyRevisionPersistance : public StubRevisionPersistance
{
public:
	void Save(const string& filePath, const string& revision) override
	{
		m_paramSave = std::make_pair(filePath, revision);
		StubRevisionPersistance::Save(filePath, revision);
	}

	std::pair<string, string> GetParamToSave() const { return m_paramSave; }

private:
	std::pair<string, string> m_paramSave;
};

class StubGitDiffProvider : public IGitDiffProvider
{
public:
	void GetDiff(std::vector<CVersionControlFileStatus>& diffResult) override 
	{
		diffResult = m_diffResult;
	}

	void StubGetDiff(std::vector<CVersionControlFileStatus>& stub)
	{
		m_diffResult = stub;
	}

private:
	std::vector<CVersionControlFileStatus> m_diffResult;
};

class SpyGitDiffprovider : public StubGitDiffProvider
{
public:
	void GetDiff(std::vector<CVersionControlFileStatus>& diffResult) override
	{
		++m_numCallsGetDiff;
		StubGitDiffProvider::GetDiff(diffResult);
	}

	int GetNumCallsGetDiff() const { return m_numCallsGetDiff; }

private:
	int m_numCallsGetDiff{ 0 };
};
		
class SpyGitSyncronizer : public IGitSyncronizer
{
public:
	void SyncWithRemote(const std::vector<CVersionControlFileStatus>& currentStatus) override
	{
		++m_numCallsSyncWithRemote;
	}

	int GetNumCallsSyncWithRemote() const { return m_numCallsSyncWithRemote; }

private:
	int m_numCallsSyncWithRemote{ 0 };
};

class DummyGitIgnoreHandler : public ITextFileEditor
{
public:
	bool DoesFileExist() override { return false; }

	void CreateFile() override {}

	void AppendLine(const string& line) override {} 

	string GetFilePath() const { return ""; }
};

class StubGitIgnoreHandler : public DummyGitIgnoreHandler
{
public:
	bool DoesFileExist() override { return m_stubDoesFileExist; }

	void StubDoesFileExist(bool stub) { m_stubDoesFileExist = stub; }

private:
	bool m_stubDoesFileExist{ false };
};

class SpyGitIgnoreHandler : public StubGitIgnoreHandler
{
public:
	void AppendLine(const string& line) override
	{
		++m_numCallsAppendLine;
		m_paramAppendLine = line;
		StubGitIgnoreHandler::AppendLine(line);
	}

	void CreateFile() override
	{
		++m_numCallsCreateFile;
		StubGitIgnoreHandler::CreateFile();
	}

	int GetNumCallsAppendLine() const { return m_numCallsAppendLine; }

	int GetNumCallsCreateFile() const { return m_numCallsCreateFile; }

	string GetParamToApendLine() const { return m_paramAppendLine; }

private:
	int m_numCallsAppendLine{ 0 };
	int m_numCallsCreateFile{ 0 };
	string m_paramAppendLine{ "" };
};

CRY_UNIT_TEST_FIXTURE(GitAdapterBaseTest)
{
public:
	void Done() override
	{
		m_fileStatuses.clear();
	}

protected:
	CVersionControlFileStatus CreateFileStatus(const string& filePath, EState localState, EState remoteState, bool bConflicting = false)
	{
		CVersionControlFileStatus fs = CVersionControlFileStatus(filePath);
		fs.SetLocalState(localState);
		fs.SetRemoteState(remoteState);
		fs.SetConflicting(bConflicting);
		return fs;
	}

	void AddFileStatus(const string& filePath, EState localState, EState remoteState, bool bConflicting = false)
	{
		m_fileStatuses.push_back(CreateFileStatus(filePath, localState, remoteState, bConflicting));
	}

	void AssertElementConflict(const string& filePath, bool bConflicting)
	{
		const auto it = std::find_if(m_fileStatuses.cbegin(), m_fileStatuses.cend(), [&filePath](const CVersionControlFileStatus& fs)
		{
			return filePath == fs.GetFileName();
		});
		if (it != m_fileStatuses.cend())
		{
			CRY_UNIT_TEST_CHECK_EQUAL(it->GetConflicting(), bConflicting);
		}
		else
		{
			CRY_UNIT_TEST_ASSERT(false);
		}
	}

	void AssertElement(const string& filePath, EState localState, EState remoteState)
	{
		const auto it = std::find_if(m_fileStatuses.cbegin(), m_fileStatuses.cend(), [&filePath](const CVersionControlFileStatus& fs)
		{
			return filePath == fs.GetFileName();
		});
		if (it != m_fileStatuses.cend())
		{
			CRY_UNIT_TEST_ASSERT(it->GetLocalState() == localState);
			CRY_UNIT_TEST_ASSERT(it->GetRemoteState() == remoteState);
		}
		else
		{
			CRY_UNIT_TEST_ASSERT(false);
		}
	}

	std::vector<CVersionControlFileStatus> m_fileStatuses;
};

class GitAdapterTest : public GitAdapterBaseTest
{
public:
	void Init() override
	{
		GitAdapterBaseTest::Init();
		m_executor = new MockGitExecutor();
		m_persistance = new SpyRevisionPersistance();
		m_diffProvider = new SpyGitDiffprovider();
		m_localStatusMerger = new SpyGitLocalStatusMerger();
		m_syncronizer = new SpyGitSyncronizer();
		m_ignoreEditor = new SpyGitIgnoreHandler();
		m_adapter = new CGitVCSAdapter(
			std::unique_ptr<IGitExecutor>(m_executor),
			std::unique_ptr<IRevisionPersistance>(m_persistance),
			std::unique_ptr<IGitDiffProvider>(m_diffProvider),
			std::unique_ptr<IGitLocalStatusMerger>(m_localStatusMerger),
			std::unique_ptr<IGitSyncronizer>(m_syncronizer),
			std::unique_ptr<ITextFileEditor>(m_ignoreEditor));
	}

	void Done() override
	{
		delete m_adapter;
		GitAdapterBaseTest::Done();
	}

protected:
	SpyRevisionPersistance* m_persistance;
	MockGitExecutor* m_executor;
	SpyGitDiffprovider* m_diffProvider;
	SpyGitLocalStatusMerger* m_localStatusMerger;
	SpyGitSyncronizer* m_syncronizer;
	CGitVCSAdapter* m_adapter;
	SpyGitIgnoreHandler* m_ignoreEditor;
};

CRY_UNIT_TEST_SUITE(GitAdapter_DiffProvider)
{
	class DiffProviderTest : public GitAdapterTest
	{
	public:
		void Init() override
		{
			m_executor = new StubGitExecutor();
			m_persistance = new StubRevisionPersistance();
			m_executor->StubGetFileRemoteRevision("dummy_reviison");
		}

		void Done() override
		{
			m_diffList.clear();
			delete m_executor;
			delete m_persistance;
		}

	protected:
		void GenerateDiff()
		{
			CGitDiffProvider(m_executor, m_persistance).GetDiff(m_diffList);
		}

		void AssertSize(int size)
		{
			CRY_UNIT_TEST_CHECK_EQUAL(m_diffList.size(), size);
		}

		void AssertElement(const string& filePath, EState localState, EState remoteState)
		{
			const auto it = std::find_if(m_diffList.cbegin(), m_diffList.cend(), [&filePath](const CVersionControlFileStatus& fs)
			{
				return filePath == fs.GetFileName();
			});
			if (it != m_diffList.cend())
			{
				CRY_UNIT_TEST_ASSERT(it->GetLocalState() == localState);
				CRY_UNIT_TEST_ASSERT(it->GetRemoteState() == remoteState);
			}
			else
			{
				CRY_UNIT_TEST_ASSERT(false);
			}
		}

		StubRevisionPersistance* m_persistance;
		StubGitExecutor* m_executor;

	private:
		std::vector<CVersionControlFileStatus> m_diffList;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(NoDiff_ResultEmpty, DiffProviderTest)
	{
		GenerateDiff();

		AssertSize(0);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalModified, DiffProviderTest)
	{
		m_executor->StubDiffWithHead("M\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Modified, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDeleted, DiffProviderTest)
	{
		m_executor->StubDiffWithHead("D\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Deleted, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalAdded, DiffProviderTest)
	{
		m_executor->StubDiffWithHead("A\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Added, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(RemoteModified, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Unmodified, EState::Modified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(RemoteDeleted, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("D\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Unmodified, EState::Deleted);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(RemoteAdded, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("A\ta\n");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Unmodified, EState::Added);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalAddedRemoteAdded, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("A\ta\n");
		m_executor->StubDiffWithHead("A\tb\n");

		GenerateDiff();

		AssertSize(2);
		AssertElement("a", EState::Unmodified, EState::Added);
		AssertElement("b", EState::Added, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalModifiedRemoteDeleted, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("D\ta\n");
		m_executor->StubDiffWithHead("M\tb\n");

		GenerateDiff();

		AssertSize(2);
		AssertElement("a", EState::Unmodified, EState::Deleted);
		AssertElement("b", EState::Modified, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(LocalDeletedRemoteModified, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\n");
		m_executor->StubDiffWithHead("D\tb\n");

		GenerateDiff();

		AssertSize(2);
		AssertElement("a", EState::Unmodified, EState::Modified);
		AssertElement("b", EState::Deleted, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(FileWithSameRevision_Discarded, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\n");
		m_executor->StubDiffWithHead("M\ta\n");
		m_executor->StubGetFileRemoteRevision("1111111\n");
		m_persistance->StubFileRevision("a", "1111111");

		GenerateDiff();

		AssertSize(0);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(WhenDifferentRevision_LocalDiscarded, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\n");
		m_executor->StubDiffWithHead("D\ta\n");
		m_executor->StubGetFileRemoteRevision("1111111\n");
		m_persistance->StubFileRevision("a", "2222222");

		GenerateDiff();

		AssertSize(1);
		AssertElement("a", EState::Unmodified, EState::Modified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(MultipleOverlappingFiles, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\nD\tb\n");
		m_executor->StubDiffWithHead("M\ta\nA\tc\n");
		m_executor->StubGetFileRemoteRevision("1111111\n");
		GenerateDiff();

		AssertSize(3);
		AssertElement("a", EState::Unmodified, EState::Modified);
		AssertElement("b", EState::Unmodified, EState::Deleted);
		AssertElement("c", EState::Added, EState::Unmodified);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(RemoteFilesCheckedAgainstSavedRevision, DiffProviderTest)
	{
		m_executor->StubDiffWithRemote("M\ta\nM\tb\n");
		m_executor->StubDiffWithHead("");
		m_executor->StubGetFileRemoteRevision("1111111\n");
		m_persistance->StubFileRevision("a", "1111111");
		GenerateDiff();

		AssertSize(1);
		AssertElement("b", EState::Unmodified, EState::Modified);
	};
}

CRY_UNIT_TEST_SUITE(GitAdapter_UpdateStatus)
{
	class GetChangesTest : public GitAdapterTest
	{
	protected:
		void CallUpdateStatus()
		{
			m_adapter->UpdateStatus();
		}
	};

	CRY_UNIT_TEST_WITH_FIXTURE(RemoteUpdatedOnce, GetChangesTest)
	{
		CallUpdateStatus();

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsUpdateRemote(), 1);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfThereIsDifference_LocalMergerIsCalled, GetChangesTest)
	{
		std::vector<CVersionControlFileStatus> diff = { CreateFileStatus("a", EState::Modified, EState::Unmodified) };
		m_diffProvider->StubGetDiff(diff);

		CallUpdateStatus();

		CRY_UNIT_TEST_CHECK_EQUAL(m_localStatusMerger->GetNumCallsMergeIntoDiff(), 1);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoDifference_LocalMergerIsNotCalled, GetChangesTest)
	{
		CallUpdateStatus();

		CRY_UNIT_TEST_CHECK_EQUAL(m_localStatusMerger->GetNumCallsMergeIntoDiff(), 0);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfChangesLocallyAndRemotely_MarkedAsConflict, GetChangesTest)
	{
		std::vector<CVersionControlFileStatus> diff = { 
			CreateFileStatus("a", EState::Modified, EState::Modified),
			CreateFileStatus("b", EState::Added, EState::Added),
			CreateFileStatus("c", EState::Modified, EState::Deleted),
			CreateFileStatus("d", EState::Deleted, EState::Modified),
			CreateFileStatus("e", EState::Modified, EState::Unmodified),
			CreateFileStatus("f", EState::Unmodified, EState::Modified) };
		m_diffProvider->StubGetDiff(diff);

		CallUpdateStatus();

		AssertElementConflict("a", true);
		AssertElementConflict("b", true);
		AssertElementConflict("c", true);
		AssertElementConflict("d", true);
		AssertElementConflict("e", false);
		AssertElementConflict("f", false);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoDiffWithRemote_SynconizerIsCalled, GetChangesTest)
	{
		m_executor->StubGetStatus(" M a\n");

		CallUpdateStatus();

		CRY_UNIT_TEST_CHECK_EQUAL(m_syncronizer->GetNumCallsSyncWithRemote(), 1);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoDiffWithRemote_LocalStatusIsTaken, GetChangesTest)
	{
		m_executor->StubGetStatus("## master\n M a\n D b\n");

		CallUpdateStatus();

		AssertElement("a", EState::Modified, EState::Unmodified);
		AssertElement("b", EState::Deleted, EState::Unmodified);
	};
}

CRY_UNIT_TEST_SUITE(GitAdapter_Syncronizer)
{
	CRY_UNIT_TEST_FIXTURE(SyncronizerTest)
	{
	public:
		void Init() override
		{
			m_executor = new SpyGitExecutor();
			m_syncronizer = new CGitSyncronizer(m_executor);
		}

		void Done() override
		{
			delete m_syncronizer;
			delete m_executor;
		}

	protected:
		void Sync() 
		{
			m_syncronizer->SyncWithRemote(m_fileStatuses);
		}

		void AddFileStatus(const string& filePath, EState localState, EState remoteState)
		{
			CVersionControlFileStatus fs = CVersionControlFileStatus(filePath);
			fs.SetLocalState(localState);
			fs.SetRemoteState(remoteState);
			m_fileStatuses.push_back(fs);
		}

		SpyGitExecutor* m_executor;
		IGitSyncronizer* m_syncronizer;
		std::vector<CVersionControlFileStatus> m_fileStatuses;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(ResetIsCalled, SyncronizerTest)
	{
		Sync();

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsResetMixed(), 1);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfAfterResetNewLocalChanges_CommitThem, SyncronizerTest)
	{
		AddFileStatus("b", EState::Modified, EState::Unmodified);
		m_executor->StubGetStatus("## master\n M a\n M b\n");

		Sync();

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddFiles(), 1);
		CRY_UNIT_TEST_ASSERT(m_executor->GetParamsToAddFiles() == toStringList({ "a" }));
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
	};
}

CRY_UNIT_TEST_SUITE(GitAdapter_DownloadLatest)
{
	CRY_UNIT_TEST_WITH_FIXTURE(IfNoConflicts_CheckoutNonDeletedResetMixed, GitAdapterTest)
	{
		std::vector<CVersionControlFileStatus> stub = {
			CreateFileStatus("a", EState::Modified, EState::Unmodified),
			CreateFileStatus("b", EState::Added, EState::Unmodified),
			CreateFileStatus("c", EState::Deleted, EState::Unmodified),
			CreateFileStatus("d", EState::Unmodified, EState::Modified),
			CreateFileStatus("e", EState::Unmodified, EState::Added),
			CreateFileStatus("f", EState::Unmodified, EState::Deleted) };
		m_diffProvider->StubGetDiff(stub);

		auto error = m_adapter->DownloadLatest({}, {}, false);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCheckout(), 1);
		CRY_UNIT_TEST_ASSERT(m_executor->GetParamsToCheckout() == toStringList({ "d", "e" }));
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsResetMixed(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_persistance->GetParamToSave().first, "e");
		CRY_UNIT_TEST_CHECK_EQUAL(error, IVersionControlAdapter::Error::None);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoNonDeletedRemoteChagnes_NoCheckout, GitAdapterTest)
	{
		std::vector<CVersionControlFileStatus> stub = {
			CreateFileStatus("a", EState::Modified, EState::Unmodified),
			CreateFileStatus("b", EState::Added, EState::Unmodified),
			CreateFileStatus("c", EState::Deleted, EState::Unmodified),
			CreateFileStatus("f", EState::Unmodified, EState::Deleted) };
		m_diffProvider->StubGetDiff(stub);

		m_adapter->DownloadLatest({}, {}, false);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCheckout(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddFiles(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfConflicts_CheckoutNonConflictingAndCommit, GitAdapterTest)
	{
		std::vector<CVersionControlFileStatus> stub = {
			CreateFileStatus("a", EState::Modified, EState::Unmodified),
			CreateFileStatus("b", EState::Added, EState::Unmodified),
			CreateFileStatus("c", EState::Deleted, EState::Modified),
			CreateFileStatus("d", EState::Modified, EState::Modified),
			CreateFileStatus("e", EState::Unmodified, EState::Added),
			CreateFileStatus("f", EState::Unmodified, EState::Deleted) };
		m_diffProvider->StubGetDiff(stub);

		auto error = m_adapter->DownloadLatest({}, {}, false);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);

		CRY_UNIT_TEST_ASSERT(m_executor->GetParamsToCheckout() == toStringList({ "e" }));
		CRY_UNIT_TEST_ASSERT(m_executor->GetParamsToAddFiles() == toStringList({ "e" }));
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsResetMixed(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_persistance->GetParamToSave().first, "e");
		CRY_UNIT_TEST_CHECK_EQUAL(error, IVersionControlAdapter::Error::None);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoConflictsAndDeletedRemotely_NoCheckoutAndDeletedLocally, GitAdapterTest)
	{
		using EFunc = MockGitExecutor::EFunc;

		std::vector<CVersionControlFileStatus> stub = { CreateFileStatus("a", EState::Unmodified, EState::Deleted) };
		m_diffProvider->StubGetDiff(stub);

		std::vector<CVersionControlFileStatus> changes;
		m_adapter->DownloadLatest({}, {}, false);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCheckout(), 0);
		CRY_UNIT_TEST_ASSERT(m_executor->GetParamsToRemoveFiles() == toStringList({ "a" }));
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsResetMixed(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_persistance->GetParamToSave().first, "a");
		m_executor->AssertCallSequence(2, EFunc::RemoveFiles, EFunc::ResetMixed);
	};
};

CRY_UNIT_TEST_SUITE(GitAdapter_Publish)
{
	CRY_UNIT_TEST_WITH_FIXTURE(IfConflicts_NoPublish, GitAdapterTest)
	{
		std::vector<CVersionControlFileStatus> stub = {
			CreateFileStatus("a", EState::Modified, EState::Modified),
			CreateFileStatus("b", EState::Added, EState::Unmodified),
			CreateFileStatus("c", EState::Deleted, EState::Unmodified) };
		m_diffProvider->StubGetDiff(stub);

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 0);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, false);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoConflicts_Publish, GitAdapterTest)
	{
		std::vector<CVersionControlFileStatus> stub = {
			CreateFileStatus("a", EState::Modified, EState::Unmodified),
			CreateFileStatus("b", EState::Added, EState::Unmodified),
			CreateFileStatus("c", EState::Deleted, EState::Unmodified) };
		m_diffProvider->StubGetDiff(stub);

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_diffProvider->GetNumCallsGetDiff(), 1);
		
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsResetMixed(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddAll(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 1);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, true);
	};

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoRemote_NoPublish, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("fatal: bad url");

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 0);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, false);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfInvalidRemote_NoPublish, GitAdapterTest)
	{
		m_executor->StubUpdateRemote("fatal: bad remote");

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 0);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, false);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoRemote_NoPublishFile, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("fatal: bad url");

		//bool bResult = m_adapter->Publish(std::vector<string>({ "file" }));

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 0);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, false);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfInvalidRemote_NoPublishFile, GitAdapterTest)
	{
		m_executor->StubUpdateRemote("fatal: bad remote");

		//bool bResult = m_adapter->Publish(std::vector<string>({ "file" }));

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 0);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, false);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(InNoUpstream_PublishSetsIt, GitAdapterTest)
	{
		m_executor->StubGetUpstreamBranch("fatal: no upstream");

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetParamToPush(), true);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, true);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(InNoUpstream_PublishFileSetsIt, GitAdapterTest)
	{
		m_executor->StubGetUpstreamBranch("fatal: no upstream");

		//bool bResult = m_adapter->Publish(std::vector<string>({ "file" }));

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetParamToPush(), true);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, true);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(InUpstreamExists_PublishDoesntSetIt, GitAdapterTest)
	{
		m_executor->StubGetUpstreamBranch("upstream");

		//std::vector<CVersionControlFileStatus> changes;
		//bool bResult = m_adapter->Publish(changes);

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetParamToPush(), false);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, true);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(InUpstreamExists_PublishFileDoesntSetIt, GitAdapterTest)
	{
		m_executor->StubGetUpstreamBranch("upstream");

		//bool bResult = m_adapter->PublishFiles(std::vector<string>({ "file" }), "");

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsPush(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetParamToPush(), false);
		//CRY_UNIT_TEST_CHECK_EQUAL(bResult, true);
	}
};

CRY_UNIT_TEST_SUITE(GitAdapter_IgnoreFiles)
{
	CRY_UNIT_TEST_WITH_FIXTURE(GitHandlerIsCalledAndCommit, GitAdapterTest)
	{
		m_adapter->IgnoreFiles({ "a", "b" });

		CRY_UNIT_TEST_CHECK_EQUAL(m_ignoreEditor->GetNumCallsAppendLine(), 2);
		// TODO: introduce matchers like OneOf({"a", "b"}) so that test doesn't depend on implementation
		CRY_UNIT_TEST_CHECK_EQUAL(m_ignoreEditor->GetParamToApendLine(), "b");
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddFiles(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
	}
}

CRY_UNIT_TEST_SUITE(GitAdapter_Initialize)
{
	CRY_UNIT_TEST_WITH_FIXTURE(IfNoFile_CreateAndCommit, GitAdapterTest)
	{
		m_ignoreEditor->StubDoesFileExist(false);

		m_adapter->Initialize();

		CRY_UNIT_TEST_CHECK_EQUAL(m_ignoreEditor->GetNumCallsCreateFile(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddFiles(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfFileExists_Commit, GitAdapterTest)
	{
		m_ignoreEditor->StubDoesFileExist(true);

		m_adapter->Initialize();

		CRY_UNIT_TEST_CHECK_EQUAL(m_ignoreEditor->GetNumCallsCreateFile(), 0);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddFiles(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsCommit(), 1);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfNoBranch_Uninitialized, GitAdapterTest)
	{
		m_executor->StubGetBranch("");

		bool bIsInitialized;
		m_adapter->IsInitialized(bIsInitialized);

		CRY_UNIT_TEST_CHECK_EQUAL(bIsInitialized, false);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfBranchExist_Initialized, GitAdapterTest)
	{
		m_executor->StubGetBranch("master");

		bool bIsInitialized;
		m_adapter->IsInitialized(bIsInitialized);

		CRY_UNIT_TEST_CHECK_EQUAL(bIsInitialized, true);
	}

}

CRY_UNIT_TEST_SUITE(GitAdapter_SetRepoLocation)
{
	CRY_UNIT_TEST_WITH_FIXTURE(IfNoRemote_AddNewSetUpstream, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("fatal: no remote");

		m_adapter->SetRepoLocation("new_url");

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddRemote(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsSetUpstream(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsSetRemoteUrl(), 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfRemoteExists_Replace, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("old_url");

		m_adapter->SetRepoLocation("new_url");

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsSetRemoteUrl(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddRemote(), 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfNewRemoteInvalidAndHadOldOne_SetBack, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("old_url");
		m_executor->StubUpdateRemote("fatal: bad remote");

		m_adapter->SetRepoLocation("new_url");

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsSetRemoteUrl(), 2);
		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsAddRemote(), 0);
	}

	CRY_UNIT_TEST_WITH_FIXTURE(IfNewRemoteInvalidAndNoOldRemote_Remove, GitAdapterTest)
	{
		m_executor->StubGetRemoteUrl("fatal: no remote");
		m_executor->StubUpdateRemote("fatal: bad remote");

		m_adapter->SetRepoLocation("new_url");

		CRY_UNIT_TEST_CHECK_EQUAL(m_executor->GetNumCallsRemoveRemote(), 1);
	}
}

CRY_UNIT_TEST_SUITE(VersionControl)
{
	class DummyGitVCSAdapter : public IVersionControlAdapter
	{
	public:
		 void GetChanges(std::vector<CVersionControlFileStatus>& fileStatuses) override {}

		 bool DownloadLatest(std::vector<CVersionControlFileStatus>& fileStatuses) override { return false; }

		 void DownloadLatest(const std::vector<string>& filePaths) override {}

		 bool Publish(std::vector<CVersionControlFileStatus>& fileStatuses) override { return false; }

		 bool Publish(const std::vector<string>& filePaths, string message = "") override { return false; }

		 void ResolveConflicts(const StatusesMap& filesStatuse, const std::vector<std::pair<string, EConflictResolution>>& conflictStatuses) {}

		 void StartEdit(const string& filePath) override {}

		 bool IsInitialized() override { return false; }

		 void Initialize() override {}

		 void IgnoreFiles(const std::vector<string>& patterns) override {}

		 bool SetRepoLocation(const string& repoPath) override { return false; }

		 string GetLastHistoryStep() override { return ""; }
	};

	class StubGitVCSAdapter : public DummyGitVCSAdapter
	{
	public:
		void GetChanges(std::vector<CVersionControlFileStatus>& fileStatuses) override
		{
			fileStatuses = m_stubGetChanges;
		}

		void StubGetChanges(std::vector<CVersionControlFileStatus> stub) { m_stubGetChanges = std::move(stub); }

	private:
		std::vector<CVersionControlFileStatus> m_stubGetChanges;
	};

	class SpyGitVCSAdapter : public StubGitVCSAdapter
	{
	public:
		bool Publish(const std::vector<string>& filePaths, string message = "") override 
		{
			m_paramPublish = filePaths;
			return StubGitVCSAdapter::Publish(filePaths, message);
		}

		std::vector<string> GetParamToPublish() const { return m_paramPublish; }

	private:
		std::vector<string> m_paramPublish;
	};

	class VersionControlTest : public GitAdapterBaseTest
	{
	public:
		void Init() override
		{
			GitAdapterBaseTest::Init();
			m_adapter = new SpyGitVCSAdapter();
			m_vcs = new CVersionControl(std::unique_ptr<IVersionControlAdapter>(m_adapter));
		}

		void Done() override
		{
			delete m_adapter;
			GitAdapterBaseTest::Done();
		}

	protected:
		SpyGitVCSAdapter* m_adapter;
		CVersionControl* m_vcs;
	};

	CRY_UNIT_TEST_WITH_FIXTURE(PublishFilesPublishesOnlyModifiedFiles, VersionControlTest)
	{
		AddFileStatus("a", EState::Unmodified, EState::Unmodified);
		AddFileStatus("b", EState::Added, EState::Unmodified);
		AddFileStatus("c", EState::Modified, EState::Unmodified);
		AddFileStatus("d", EState::Unmodified, EState::Modified);
		AddFileStatus("e", EState::Deleted, EState::Unmodified);

		m_adapter->StubGetChanges(m_fileStatuses);
		
		auto files = std::vector<string>({ "a", "b", "c", "d", "e" });
		m_vcs->Publish(files);

		auto paramToPublish = m_adapter->GetParamToPublish();
		CRY_UNIT_TEST_CHECK_EQUAL(paramToPublish.size(), 3);
		// TODO: use matchers
		CRY_UNIT_TEST_CHECK_EQUAL(paramToPublish[0], "b");
		CRY_UNIT_TEST_CHECK_EQUAL(paramToPublish[1], "c");
		CRY_UNIT_TEST_CHECK_EQUAL(paramToPublish[2], "e");
	}
}

}

}*/
