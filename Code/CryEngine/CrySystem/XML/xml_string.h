// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef XML_STRING
#define XML_STRING

typedef string xml_string;
/*
   #include <malloc.h>
   #include <conio.h>

   class xml_string {
   public:
   xml_string()
   {
    m_sBuf=NULL;
   }
   xml_string(const char *s)
   {
    m_sBuf=NULL;
    set(s);
   }
   xml_string(const wchar_t *s)
   {
    m_sBuf=NULL;
    set(s);
   }
   ~xml_string()
   {
    if(m_sBuf)
      free(m_sBuf);
    m_sBuf=NULL;
   }
   void set(const char *s)
   {
    if(m_sBuf)free(m_sBuf);
    size_t len=strlen(s);
    m_sBuf=(unsigned char*)malloc(len+2);
    m_sBuf[0]=0;
    strcpy((char *)&m_sBuf[1],s);
   }
   void set(const wchar_t *s)
   {
    if(m_sBuf)free(m_sBuf);
    size_t len=wcslen(s);
    m_sBuf=(unsigned char*)malloc((len+2)*sizeof(wchar_t));
    m_sBuf[0]=1;
    wcscpy(((wchar_t *)&m_sBuf[1]),s);
   }
   const char *c_str_s()
   {
    if(!m_sBuf) return NULL;
    if(m_sBuf[0]!=0)
    {
      unsigned char *t=m_sBuf;
      m_sBuf=wide2single((const wchar_t *)&t[1]);
      free(t);
      return ((const char *)&m_sBuf[1]);

    }
    return ((const char *)&m_sBuf[1]);
   }
   const wchar_t *c_str_w()
   {
    if(!m_sBuf) return NULL;
    if(m_sBuf[0]==0)
    {
      unsigned char *t=m_sBuf;
      m_sBuf=single2wide((const char *)&t[1]);
      free(t);
      return ((const wchar_t *)&m_sBuf[1]);
    }
    return ((const wchar_t *)&m_sBuf[1]);
   }
   xml_string& operator =(xml_string& s)
   {
    set(s.c_str_w());
    return *this;
   }
   xml_string& operator =(const char *s)
   {
    set(s);
    return *this;
   }
   xml_string& operator =(const wchar_t *s)
   {
    set(s);
    return *this;
   }

   private:
   unsigned char *wide2single(const wchar_t *s)
   {
    size_t len=wcslen(s);
    unsigned char *ns=allocsingle(len);
    char *t=((char *)&ns[1]);
    for(size_t i=0;i<len;i++)
    {
      t[i]=(char)s[i];
    }
    return ns;
   }
   unsigned char *single2wide(const char *s)
   {
    size_t len=strlen(s);
    unsigned char *w=allocwide(len);
    wchar_t *t=(wchar_t *)&w[1];
    for(size_t i=0;i<len;i++)
    {
      t[i]=(unsigned char)s[i];
    }
    return w;
   }
   unsigned char *allocsingle(size_t len)
   {
    unsigned char* buf=(unsigned char*)malloc(len+2);
    memset(buf,0,len+2);
    buf[0]=0;
    return buf;
   }
   unsigned char *allocwide(size_t len)
   {
    unsigned char *buf=(unsigned char*)malloc((len+2)*sizeof(wchar_t));
    memset(buf,0,(len+2)*sizeof(wchar_t));
    buf[0]=1;
    return buf;
   }
   //the first byte tell if is a wide char or a single char
   //first byte != 0 widechar
   //first byte == 0 singlechar
   unsigned char *m_sBuf;
   };

   /*int _tmain(int argc, _TCHAR* argv[])
   {
   xml_string a("sono small"),w(L"sono wide");
   for(int i=0;i<10000;i++){
    printf("%s\n",w.c_str_s());
    wprintf(L"%s\n",w.c_str_w());
    printf("%s\n",a.c_str_s());
    wprintf(L"%s\n",a.c_str_w());
   }
   _getch();
   return 0;
   }*/

#endif //XML_STRING
