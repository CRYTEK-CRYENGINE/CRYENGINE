// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   The DialogLine Database holds all DialogLines and is able to pick the correct one, given a lineID.
   A SDialogLineSet contains several SDialogLines which are picked by some criteria (random, sequential...)

   /************************************************************************/

#pragma once

#include <CryString/HashedString.h>
#include <CrySystem/XML/IXml.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

namespace CryDRS
{
class CDialogLine final : public DRS::IDialogLine
{
public:
	CDialogLine();

	//////////////////////////////////////////////////////////
	// IDialogLine implementation
	virtual const string& GetText() const override                                     { return m_text; }
	virtual const string& GetStartAudioTrigger() const override                        { return m_audioStartTrigger; }
	virtual const string& GetEndAudioTrigger() const override                          { return m_audioStopTrigger; }
	virtual const string& GetLipsyncAnimation() const override                         { return m_lipsyncAnimation; }
	virtual const string& GetStandaloneFile() const override                           { return m_standaloneFile; }
	virtual void          SetText(const string& text) override                         { m_text = text; }
	virtual void          SetStartAudioTrigger(const string& trigger) override         { m_audioStartTrigger = trigger; }
	virtual void          SetEndAudioTrigger(const string& trigger) override           { m_audioStopTrigger = trigger; }
	virtual void          SetLipsyncAnimation(const string& lipsyncAnimation) override { m_lipsyncAnimation = lipsyncAnimation; }
	virtual void          SetStandaloneFile(const string& value) override              { m_standaloneFile = value; }
	virtual void          Serialize(Serialization::IArchive& ar) override;
	//////////////////////////////////////////////////////////

private:
	string m_text;              //todo: optimize into an (compressed) pool + also use wstring
	string m_audioStartTrigger; //todo: optimize by storing directly the TAudioControlID, problems occur, when ID can not be obtained because the bank is not loaded, and when saving the data to file
	string m_audioStopTrigger;
	string m_lipsyncAnimation;
	string m_standaloneFile;
};

class CDialogLineSet final : public DRS::IDialogLineSet
{
public:
	CDialogLineSet();
	const CDialogLine* PickLine();

	//////////////////////////////////////////////////////////
	// IDialogLineSet implementation
	virtual void              SetLineId(const CHashedString& lineId) override { m_lineId = lineId; }
	virtual void              SetPriority(int priority) override              { m_priority = priority; }
	virtual void              SetFlags(uint32 flags) override                 { m_flags = flags; }
	virtual CHashedString     GetLineId() const override                      { return m_lineId; }
	virtual int               GetPriority() const override                    { return m_priority; }
	virtual uint32            GetFlags() const override                       { return m_flags; }
	virtual uint32            GetLineCount() const override                   { return m_lines.size(); }
	virtual DRS::IDialogLine* GetLineByIndex(uint32 index) override;
	virtual DRS::IDialogLine* InsertLine(uint32 index) override;
	virtual void              RemoveLine(uint32 index) override;
	virtual void              Serialize(Serialization::IArchive& ar) override;
	//////////////////////////////////////////////////////////

private:
	CHashedString            m_lineId;
	int                      m_priority;
	uint32                   m_flags; //eDialogLineSetFlags
	int                      m_lastPickedLine;
	std::vector<CDialogLine> m_lines;
};

class CDialogLineDatabase final : public DRS::IDialogLineDatabase
{

public:
	CDialogLineDatabase();
	virtual ~CDialogLineDatabase();
	bool InitFromFiles(const char* szFilePath);

	//will return nullptr if there is no line with this ID
	const CDialogLine* GetLineByID(CHashedString lineID, int* pOutPriority = nullptr);

	//////////////////////////////////////////////////////////
	// IDialogLineDatabase implementation
	virtual bool                             Save(const char* szFilePath) override;
	virtual uint32                           GetLineSetCount() override;
	virtual DRS::IDialogLineSet*             GetLineSetByIndex(uint32 index) override;
	virtual const DRS::IDialogLineSet* const GetLineSetById(const CHashedString& lineID) const override;
	virtual DRS::IDialogLineSet*             InsertLineSet(uint32 index) override;
	virtual void                             RemoveLineSet(uint32 index) override;
	virtual void                             Serialize(Serialization::IArchive& ar) override;
	virtual void                             SerializeLinesHistory(Serialization::IArchive& ar) override;
	//////////////////////////////////////////////////////////

private:
	CHashedString GenerateUniqueId(const string& root);

	typedef std::vector<CDialogLineSet> DialogLineSetList;
	DialogLineSetList  m_lineSets;

	int                m_drsDialogBinaryFileFormatCVar;
	static const char* s_szFilename;
};
}
