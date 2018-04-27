// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DXPSERROR__
#define __DXPSERROR__

#include <string>
#include <set>
#include "STLHelper.hpp"

// printf wrapper to format things nicely
void logmessage(const char *text, ...);

class ICryError
{
  public:
    enum EErrorType
    {
      SIMPLE_ERROR = 0,
      COMPILE_ERROR,
    };
    
    enum EOutputFormatType
    {
      OUTPUT_EMAIL = 0,
      OUTPUT_TTY,
      OUTPUT_HASH,
    };

    ICryError(EErrorType t);
    virtual ~ICryError() {};

    EErrorType GetType() const { return m_eType; }

    tdHash Hash() const { return CSTLHelper::Hash(GetErrorName()+GetErrorDetails(OUTPUT_HASH)); };

    virtual bool Compare(const ICryError *err) const { if(GetType() != err->GetType()) return GetType() < err->GetType(); return Hash() < err->Hash(); };
    virtual bool CanMerge(const ICryError *err) const { return true; }

    virtual void AddDuplicate(ICryError *err) { m_numDupes++; }
    uint32_t NumDuplicates() const { return m_numDupes; }
    
    virtual void SetUniqueID(int uniqueID) {}

    virtual bool HasFile() const { return false; };

    virtual void AddCCs(std::set<std::string> &ccs) const {}

    virtual std::string GetErrorName() const = 0;
    virtual std::string GetErrorDetails(EOutputFormatType outputType) const = 0;
    virtual std::string GetFilename() const { return "NoFile"; }
    virtual std::string GetFileContents() const { return ""; }
  private:
    EErrorType m_eType;
    uint32_t m_numDupes;
};

class CSimpleError : public ICryError
{
  public:
    CSimpleError( const std::string &in_text) : ICryError(SIMPLE_ERROR), m_text(in_text) {}
    virtual ~CSimpleError() {}

    virtual std::string GetErrorName() const { return m_text; };
    virtual std::string GetErrorDetails(EOutputFormatType outputType) const { return m_text; };
  private:
    std::string m_text;
};

#define CrySimple_ERROR(X) throw new CSimpleError(X)

#define CrySimple_SECURE_START try{
#define CrySimple_SECURE_END }catch(const ICryError *err){printf(err->GetErrorName().c_str());delete err;}

#endif
