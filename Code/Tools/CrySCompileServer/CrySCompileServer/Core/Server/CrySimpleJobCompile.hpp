// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOBCOMPILE__
#define __CRYSIMPLEJOBCOMPILE__

#include "CrySimpleJobCache.hpp"
#include "../Error.hpp"


class CCrySimpleJobCompile	:	public	CCrySimpleJobCache
{
	static volatile long 			m_GlobalCompileTasks;
	static volatile long 			m_GlobalCompileTasksMax;
	static volatile long 			m_RemoteServerID;
	static volatile long long m_GlobalCompileTime;

	EProtocolVersion					m_Version;
	std::vector<uint8_t>*			m_pRVec;

	virtual size_t						SizeOf(std::vector<uint8_t>& rVec) = 0;

	bool											Compile(const TiXmlElement* pElement,std::vector<uint8_t>& rVec);
public:
														CCrySimpleJobCompile(uint32_t requestIP, EProtocolVersion Version, std::vector<uint8_t>* pRVec);
	virtual										~CCrySimpleJobCompile();

	virtual bool							Execute(const TiXmlElement* pElement);		

	static volatile long 			GlobalCompileTasks(){return m_GlobalCompileTasks;}
	static volatile long 			GlobalCompileTasksMax(){return m_GlobalCompileTasksMax;}
};

class CCompilerError : public ICryError
{
  public:
    CCompilerError(std::string entry, std::string errortext, std::string ccs, std::string IP,
                    std::string requestLine, std::string program, std::string project,
                    std::string platform, std::string tags, std::string profile);

    virtual ~CCompilerError() {}
    
    virtual void AddDuplicate(ICryError *err);

    virtual void SetUniqueID(int uniqueID) { m_uniqueID = uniqueID; }
    
    virtual bool Compare(const ICryError *err) const;
    virtual bool CanMerge(const ICryError *err) const;

    virtual bool HasFile() const { return true; }
    
    virtual void AddCCs(std::set<std::string> &ccs) const;

    virtual std::string GetErrorName() const;
    virtual std::string GetErrorDetails(EOutputFormatType outputType) const;
    virtual std::string GetFilename() const { return m_entry + ".txt"; }
    virtual std::string GetFileContents() const { return m_program; }
    
    std::vector<std::string> m_requests;
  private:
    void Init();
    std::string GetErrorLines() const;
    std::string GetContext(int linenum, int context=2, std::string prefix = ">") const;

    std::vector< std::pair<int,std::string> > m_errors;

    tdEntryVec m_CCs;

    std::string m_entry, m_errortext, m_hasherrors, m_IP,
                m_program, m_project, m_shader,
                m_platform, m_tags, m_profile;
    int m_uniqueID;

    friend CCompilerError;
};

#endif
