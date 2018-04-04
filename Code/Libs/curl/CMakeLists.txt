project (curl C)

#---

set (SOURCE_ROOT
	"../../SDKs/curl/lib/amigaos.c"
	"../../SDKs/curl/lib/asyn-ares.c"
	"../../SDKs/curl/lib/asyn-thread.c"
	"../../SDKs/curl/lib/base64.c"
	"../../SDKs/curl/lib/conncache.c"
	"../../SDKs/curl/lib/connect.c"
	"../../SDKs/curl/lib/content_encoding.c"
	"../../SDKs/curl/lib/cookie.c"
	"../../SDKs/curl/lib/curl_addrinfo.c"
	"../../SDKs/curl/lib/curl_des.c"
	"../../SDKs/curl/lib/curl_endian.c"
	"../../SDKs/curl/lib/curl_fnmatch.c"
	"../../SDKs/curl/lib/curl_gethostname.c"
	"../../SDKs/curl/lib/curl_gssapi.c"
	"../../SDKs/curl/lib/curl_memrchr.c"
	"../../SDKs/curl/lib/curl_multibyte.c"
	"../../SDKs/curl/lib/curl_ntlm_core.c"
	"../../SDKs/curl/lib/curl_ntlm_wb.c"
	"../../SDKs/curl/lib/curl_rtmp.c"
	"../../SDKs/curl/lib/curl_sasl.c"
	"../../SDKs/curl/lib/curl_sspi.c"
	"../../SDKs/curl/lib/curl_threads.c"
	"../../SDKs/curl/lib/dict.c"
	"../../SDKs/curl/lib/dotdot.c"
	"../../SDKs/curl/lib/easy.c"
	"../../SDKs/curl/lib/escape.c"
	"../../SDKs/curl/lib/file.c"
	"../../SDKs/curl/lib/fileinfo.c"
	"../../SDKs/curl/lib/formdata.c"
	"../../SDKs/curl/lib/ftp.c"
	"../../SDKs/curl/lib/ftplistparser.c"
	"../../SDKs/curl/lib/getenv.c"
	"../../SDKs/curl/lib/getinfo.c"
	"../../SDKs/curl/lib/gopher.c"
	"../../SDKs/curl/lib/hash.c"
	"../../SDKs/curl/lib/hmac.c"
	"../../SDKs/curl/lib/hostasyn.c"
	"../../SDKs/curl/lib/hostcheck.c"
	"../../SDKs/curl/lib/hostip.c"
	"../../SDKs/curl/lib/hostip4.c"
	"../../SDKs/curl/lib/hostip6.c"
	"../../SDKs/curl/lib/hostsyn.c"
	"../../SDKs/curl/lib/http.c"
	"../../SDKs/curl/lib/http2.c"
	"../../SDKs/curl/lib/http_chunks.c"
	"../../SDKs/curl/lib/http_digest.c"
	"../../SDKs/curl/lib/http_negotiate.c"
	"../../SDKs/curl/lib/http_ntlm.c"
	"../../SDKs/curl/lib/http_proxy.c"
	"../../SDKs/curl/lib/idn_win32.c"
	"../../SDKs/curl/lib/if2ip.c"
	"../../SDKs/curl/lib/imap.c"
	"../../SDKs/curl/lib/inet_ntop.c"
	"../../SDKs/curl/lib/inet_pton.c"
	"../../SDKs/curl/lib/krb5.c"
	"../../SDKs/curl/lib/ldap.c"
	"../../SDKs/curl/lib/llist.c"
	"../../SDKs/curl/lib/md4.c"
	"../../SDKs/curl/lib/md5.c"
	"../../SDKs/curl/lib/memdebug.c"
	"../../SDKs/curl/lib/mprintf.c"
	"../../SDKs/curl/lib/multi.c"
	"../../SDKs/curl/lib/netrc.c"
	"../../SDKs/curl/lib/non-ascii.c"
	"../../SDKs/curl/lib/nonblock.c"
	"../../SDKs/curl/lib/nwlib.c"
	"../../SDKs/curl/lib/nwos.c"
	"../../SDKs/curl/lib/openldap.c"
	"../../SDKs/curl/lib/parsedate.c"
	"../../SDKs/curl/lib/pingpong.c"
	"../../SDKs/curl/lib/pipeline.c"
	"../../SDKs/curl/lib/pop3.c"
	"../../SDKs/curl/lib/progress.c"
	"../../SDKs/curl/lib/rawstr.c"
	"../../SDKs/curl/lib/rtsp.c"
	"../../SDKs/curl/lib/security.c"
	"../../SDKs/curl/lib/select.c"
	"../../SDKs/curl/lib/sendf.c"
	"../../SDKs/curl/lib/share.c"
	"../../SDKs/curl/lib/slist.c"
	"../../SDKs/curl/lib/smb.c"
	"../../SDKs/curl/lib/smtp.c"
	"../../SDKs/curl/lib/socks.c"
	"../../SDKs/curl/lib/socks_gssapi.c"
	"../../SDKs/curl/lib/socks_sspi.c"
	"../../SDKs/curl/lib/speedcheck.c"
	"../../SDKs/curl/lib/splay.c"
	"../../SDKs/curl/lib/ssh.c"
	"../../SDKs/curl/lib/strdup.c"
	"../../SDKs/curl/lib/strequal.c"
	"../../SDKs/curl/lib/strerror.c"
	"../../SDKs/curl/lib/strtok.c"
	"../../SDKs/curl/lib/strtoofft.c"
	"../../SDKs/curl/lib/system_win32.c"
	"../../SDKs/curl/lib/telnet.c"
	"../../SDKs/curl/lib/tftp.c"
	"../../SDKs/curl/lib/timeval.c"
	"../../SDKs/curl/lib/transfer.c"
	"../../SDKs/curl/lib/url.c"
	"../../SDKs/curl/lib/version.c"
	"../../SDKs/curl/lib/warnless.c"
	"../../SDKs/curl/lib/wildcard.c"
	"../../SDKs/curl/lib/x509asn1.c"
)
source_group ("lib" FILES ${SOURCE_ROOT})

set (SOURCE_VAUTH
	"../../SDKs/curl/lib/vauth/cleartext.c"
	"../../SDKs/curl/lib/vauth/cram.c"
	"../../SDKs/curl/lib/vauth/digest.c"
	"../../SDKs/curl/lib/vauth/digest_sspi.c"
	"../../SDKs/curl/lib/vauth/krb5_gssapi.c"
	"../../SDKs/curl/lib/vauth/krb5_sspi.c"
	"../../SDKs/curl/lib/vauth/ntlm.c"
	"../../SDKs/curl/lib/vauth/ntlm_sspi.c"
	"../../SDKs/curl/lib/vauth/oauth2.c"
	"../../SDKs/curl/lib/vauth/spnego_gssapi.c"
	"../../SDKs/curl/lib/vauth/spnego_sspi.c"
	"../../SDKs/curl/lib/vauth/vauth.c"
)
source_group ("lib\\vauth" FILES ${SOURCE_VAUTH})

set (SOURCE_VTLS
	"../../SDKs/curl/lib/vtls/axtls.c"
	"../../SDKs/curl/lib/vtls/cyassl.c"
	"../../SDKs/curl/lib/vtls/darwinssl.c"
	"../../SDKs/curl/lib/vtls/gskit.c"
	"../../SDKs/curl/lib/vtls/gtls.c"
	"../../SDKs/curl/lib/vtls/mbedtls.c"
	"../../SDKs/curl/lib/vtls/nss.c"
	"../../SDKs/curl/lib/vtls/openssl.c"
	"../../SDKs/curl/lib/vtls/polarssl.c"
	"../../SDKs/curl/lib/vtls/polarssl_threadlock.c"
	"../../SDKs/curl/lib/vtls/schannel.c"
	"../../SDKs/curl/lib/vtls/vtls.c"
)
source_group ("lib\\vtls" FILES ${SOURCE_VTLS})

#---

if (COMMAND CryEngineStaticModule)
	set(${PROJECT_NAME}_SOURCES ${SOURCE_ROOT} ${SOURCE_VAUTH} ${SOURCE_VTLS})
	CryEngineStaticModule(${PROJECT_NAME} SOLUTION_FOLDER "Libs")
else()
	add_library (${PROJECT_NAME} STATIC ${SOURCE_ROOT} ${SOURCE_VAUTH} ${SOURCE_VTLS})
	set_solution_folder("Libs" ${PROJECT_NAME})
endif()

target_include_directories (${PROJECT_NAME} 
	PUBLIC ${SDK_DIR}/curl/include
	PRIVATE ${SDK_DIR}/curl/lib
)

target_compile_definitions(${PROJECT_NAME} 
	PUBLIC	CURL_STATICLIB

	PRIVATE
	BUILDING_LIBCURL
		
	# disable unused features
	#CURL_DISABLE_CRYPTO_AUTH
	CURL_DISABLE_PROXY
	CURL_DISABLE_COOKIES
				
	# disable all unused protocols
	#CURL_DISABLE_HTTP
	CURL_DISABLE_DICT
	CURL_DISABLE_FILE
	CURL_DISABLE_FTP
	CURL_DISABLE_GOPHER
	CURL_DISABLE_IMAP
	CURL_DISABLE_TFTP
	CURL_DISABLE_LDAP
	CURL_DISABLE_POP3
	CURL_DISABLE_RTSP
	CURL_DISABLE_SMB
	CURL_DISABLE_SMTP
	CURL_DISABLE_TELNET
)

#--- available modules

target_compile_definitions(${PROJECT_NAME} PRIVATE
	HAVE_ZLIB_H
	HAVE_LIBZ
)
target_link_libraries (${PROJECT_NAME} PRIVATE zlib)

#--- other

if (WIN32)
	#Use native SSL support on Windows
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		USE_WINDOWS_SSPI
		USE_SCHANNEL
	)
	target_link_libraries (${PROJECT_NAME} INTERFACE Ws2_32 Crypt32)
endif()

#--- configure ---
#--- Should use include() to divide configure settings to a seperate file

if (NOT WIN32)
# Generate custom configuration file

#--- HEADER FILES
include (CheckIncludeFiles)

check_include_files ("arpa/inet.h" HAVE_ARPA_INET_H)
check_include_files ("assert.h" HAVE_ASSERT_H)
check_include_files ("crypto.h" HAVE_CRYPTO_H)
check_include_files ("errno.h" HAVE_ERRNO_H)
check_include_files ("err.h" HAVE_ERR_H)
check_include_files ("fcntl.h" HAVE_FCNTL_H)
check_include_files ("getopt.h" HAVE_GETOPT_H)
check_include_files ("inttypes.h" HAVE_INTTYPES_H)
check_include_files ("io.h" HAVE_IO_H)
check_include_files ("limits.h" HAVE_LIMITS_H)
check_include_files ("locale.h" HAVE_LOCALE_H)
check_include_files ("netdb.h" HAVE_NETDB_H)
check_include_files ("netinet/in.h" HAVE_NETINET_IN_H)
check_include_files ("process.h" HAVE_PROCESS_H)
check_include_files ("pthread.h" HAVE_PTHREAD_H)
check_include_files ("signal.h" HAVE_SIGNAL_H)
check_include_files ("sgtty.h" HAVE_SGTTY_H)
check_include_files ("ssl.h" HAVE_SSL_H)
check_include_files ("stdbool.h" HAVE_STDBOOL_H)
check_include_files ("stdlib.h" HAVE_STDLIB_H)
check_include_files ("sys/param.h" HAVE_SYS_PARAM_H)
check_include_files ("sys/select.h" HAVE_SYS_SELECT_H)
check_include_files ("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_files ("sys/sockio.h" HAVE_SYS_SOCKIO_H)
check_include_files ("sys/stat.h" HAVE_SYS_STAT_H)
check_include_files ("sys/time.h" HAVE_SYS_TIME_H)
check_include_files ("sys/types.h" HAVE_SYS_TYPES_H)
check_include_files ("sys/utime.h" HAVE_SYS_UTIME_H)
check_include_files ("termio.h" HAVE_TERMIO_H)
check_include_files ("termios.h" HAVE_TERMIOS_H)
check_include_files ("time.h" HAVE_TIME_H)
check_include_files ("unistd.h" HAVE_UNISTD_H)
check_include_files ("windows.h" HAVE_WINDOWS_H)
check_include_files ("winsock.h" HAVE_WINSOCK_H)
check_include_files ("winsock2.h" HAVE_WINSOCK2_H)
check_include_files ("ws2tcpip.h" HAVE_WS2TCPIP_H)

#--- FUNCTIONS
set (CURL_INCLUDES
	"string.h"
	"unistd.h"
	"netdb.h"
	"sys/time.h"
	"arpa/inet.h"
	"stdio.h"
	"sys/select.h"
	"locale.h"
	"sys/socket.h"
	"time.h"
	"stdlib.h"
	"termios.h"
	"utime.h"
)

include (CheckSymbolExists)
include (CheckFunctionExists)

check_symbol_exists(closesocket   "${CURL_INCLUDES}" HAVE_CLOSESOCKET)
#define HAVE_DOPRNT 1
check_symbol_exists(ftruncate      "${CURL_INCLUDES}" HAVE_FTRUNCATE)
check_symbol_exists(gethostbyaddr "${CURL_INCLUDES}" HAVE_GETHOSTBYADDR)
check_function_exists(gethostname HAVE_GETHOSTNAME)
#define HAVE_GETPASS 1
#define HAVE_GETSERVBYNAME 1
check_symbol_exists(getprotobyname "${CURL_INCLUDES}" HAVE_GETPROTOBYNAME)
check_symbol_exists(gettimeofday  "${CURL_INCLUDES}" HAVE_GETTIMEOFDAY)
check_symbol_exists(inet_addr     "${CURL_INCLUDES}" HAVE_INET_ADDR)
#define HAVE_IOCTLSOCKET 1
#define HAVE_IOCTLSOCKET_FIONBIO 1
check_symbol_exists(perror        "${CURL_INCLUDES}" HAVE_PERROR)
check_symbol_exists(RAND_screen   "${CURL_INCLUDES}" HAVE_RAND_SCREEN)
check_symbol_exists(RAND_status   "${CURL_INCLUDES}" HAVE_RAND_STATUS)
check_symbol_exists(CRYPTO_cleanup_all_ex_data "${CURL_INCLUDES}"
    HAVE_CRYPTO_CLEANUP_ALL_EX_DATA)
check_symbol_exists(select        "${CURL_INCLUDES}" HAVE_SELECT)
check_symbol_exists(setlocale      "${CURL_INCLUDES}" HAVE_SETLOCALE)
#define HAVE_SETMODE 1
check_symbol_exists(setvbuf       "${CURL_INCLUDES}" HAVE_SETVBUF)
check_symbol_exists(socket        "${CURL_INCLUDES}" HAVE_SOCKET)
check_symbol_exists(strcasecmp    "${CURL_INCLUDES}" HAVE_STRCASECMP)
check_symbol_exists(strdup        "${CURL_INCLUDES}" HAVE_STRDUP)
check_symbol_exists(strftime      "${CURL_INCLUDES}" HAVE_STRFTIME)
check_symbol_exists(stricmp       "${CURL_INCLUDES}" HAVE_STRICMP)
#define HAVE_STRNCASECMP 1
#define HAVE_STRNICMP 1
check_symbol_exists(strstr        "${CURL_INCLUDES}" HAVE_STRSTR)
check_symbol_exists(strtoll        "${CURL_INCLUDES}" HAVE_STRTOLL)
check_symbol_exists(tcgetattr     "${CURL_INCLUDES}" HAVE_TCGETATTR)
check_symbol_exists(tcsetattr     "${CURL_INCLUDES}" HAVE_TCSETATTR)
check_symbol_exists(utime         "${CURL_INCLUDES}" HAVE_UTIME)

#define GETNAMEINFO_QUAL_ARG1 const
#define GETNAMEINFO_TYPE_ARG1 struct sockaddr *
#define GETNAMEINFO_TYPE_ARG2 socklen_t
#define GETNAMEINFO_TYPE_ARG46 DWORD
#define GETNAMEINFO_TYPE_ARG7 int

check_symbol_exists(recv "${CURL_INCLUDES}" HAVE_RECV)
#define RECV_TYPE_ARG1 SOCKET
#define RECV_TYPE_ARG2 char *
#define RECV_TYPE_ARG3 int
#define RECV_TYPE_ARG4 int
#define RECV_TYPE_RETV int

check_symbol_exists(recvfrom "${CURL_INCLUDES}" HAVE_RECVFROM)
#define RECVFROM_TYPE_ARG1 SOCKET
#define RECVFROM_TYPE_ARG2 char
#define RECVFROM_TYPE_ARG3 int
#define RECVFROM_TYPE_ARG4 int
#define RECVFROM_TYPE_ARG5 struct sockaddr
#define RECVFROM_TYPE_ARG6 int
#define RECVFROM_TYPE_RETV int

check_symbol_exists(send "${CURL_INCLUDES}" HAVE_SEND)
#define SEND_TYPE_ARG1 SOCKET
#define SEND_QUAL_ARG2 const
#define SEND_TYPE_ARG2 char *
#define SEND_TYPE_ARG3 int
#define SEND_TYPE_ARG4 int
#define SEND_TYPE_RETV int

#--- TYPE SIZES
include (CheckTypeSize)

check_type_size("int"  SIZEOF_INT)
check_type_size("long double"  SIZEOF_LONG_DOUBLE)
check_type_size("long long"  SIZEOF_LONG_LONG)
check_type_size("short"  SIZEOF_SHORT)
check_type_size("size_t" SIZEOF_SIZE_T)

#---- COMPILER SPECIFIC

if (HAVE_SIZEOF_LONG_LONG)
  set(HAVE_LONGLONG 1)
  set(HAVE_LL 1)
endif (HAVE_SIZEOF_LONG_LONG)

#--- STRUCT RELATED

set (CMAKE_EXTRA_INCLUDE_FILES ${CURL_INCLUDES})
check_type_size("struct timeval" STRUCT_TIMEVAL)
set (CMAKE_EXTRA_INCLUDE_FILES)

#--- ADDITIONAL DEFINITIONS

set (OS "\"${CMAKE_SYSTEM_NAME}\"")

#--- MORE

if(HAVE_PTHREAD_H)
	set(USE_THREADS_POSIX 1)
endif()

#---

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/curl_config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/curl_config.h")
target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H)
target_include_directories (${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

endif (NOT WIN32)
