// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//-----------------------------------------------------------------------------
//	ClientCodes
//-----------------------------------------------------------------------------

#include "Vicon_ClientCodes.h"

#ifndef DISABLE_VICON

const std::vector<string> ClientCodes::MarkerTokens = MakeMarkerTokens();
const std::vector<string> ClientCodes::BodyTokens = MakeBodyTokens();

	#include <WinSock2.h>

	#define NRESULT  uint32

	#define NET_OK   0x00000000
	#define NET_FAIL 0x80000000

	#define NET_FAILED(a)                          (((a) & NET_FAIL) ? 1 : 0)
	#define NET_SUCCEDED(a)                        (((a) & NET_FAIL) ? 0 : 1)

	#define MAKE_NRESULT(severity, facility, code) (severity | facility | code)
	#define NET_FACILITY_SOCKET 0x01000000

//! regular BSD/UNIX error (errno)
//@{
	#define NET_EINTR           MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEINTR)
	#define NET_EBADF           MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEBADF)
	#define NET_EACCES          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEACCES)
	#define NET_EFAULT          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEFAULT)
	#define NET_EINVAL          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEINVAL)
	#define NET_EMFILE          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEMFILE)
	#define NET_WSAEINTR        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEINTR)
	#define NET_WSAEBADF        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEBADF)
	#define NET_WSAEACCES       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEACCES)
	#define NET_WSAEFAULT       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEFAULT)
	#define NET_WSAEINVAL       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEINVAL)
	#define NET_WSAEMFILE       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEMFILE)
	#define NET_EWOULDBLOCK     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEWOULDBLOCK)
	#define NET_EINPROGRESS     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEINPROGRESS)
	#define NET_EALREADY        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEALREADY)
	#define NET_ENOTSOCK        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENOTSOCK)
	#define NET_EDESTADDRREQ    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEDESTADDRREQ)
	#define NET_EMSGSIZE        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEMSGSIZE)
	#define NET_EPROTOTYPE      MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEPROTOTYPE)
	#define NET_ENOPROTOOPT     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENOPROTOOPT)
	#define NET_EPROTONOSUPPORT MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEPROTONOSUPPORT)
	#define NET_ESOCKTNOSUPPORT MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAESOCKTNOSUPPORT)
	#define NET_EOPNOTSUPP      MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEOPNOTSUPP)
	#define NET_EPFNOSUPPORT    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEPFNOSUPPORT)
	#define NET_EAFNOSUPPORT    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEAFNOSUPPORT)
	#define NET_EADDRINUSE      MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEADDRINUSE)
	#define NET_EADDRNOTAVAIL   MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEADDRNOTAVAIL)
	#define NET_ENETDOWN        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENETDOWN)
	#define NET_ENETUNREACH     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENETUNREACH)
	#define NET_ENETRESET       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENETRESET)
	#define NET_ECONNABORTED    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAECONNABORTED)
	#define NET_ECONNRESET      MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAECONNRESET)
	#define NET_ENOBUFS         MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENOBUFS)
	#define NET_EISCONN         MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEISCONN)
	#define NET_ENOTCONN        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENOTCONN)
	#define NET_ESHUTDOWN       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAESHUTDOWN)
	#define NET_ETOOMANYREFS    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAETOOMANYREFS)
	#define NET_ETIMEDOUT       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAETIMEDOUT)
	#define NET_ECONNREFUSED    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAECONNREFUSED)
	#define NET_ELOOP           MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAELOOP)
	#define NET_ENAMETOOLONG    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENAMETOOLONG)
	#define NET_EHOSTDOWN       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEHOSTDOWN)
	#define NET_EHOSTUNREACH    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEHOSTUNREACH)
	#define NET_ENOTEMPTY       MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAENOTEMPTY)
	#define NET_EPROCLIM        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEPROCLIM)
	#define NET_EUSERS          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEUSERS)
	#define NET_EDQUOT          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEDQUOT)
	#define NET_ESTALE          MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAESTALE)
	#define NET_EREMOTE         MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEREMOTE)
//@}

//! regular BSD/UNIX netdb error (h_errno)
// the error code is stored with a bias NET_H_ERRNO_BIAS to avoid a conflict
// with the errno codes.
//@{
	#define NET_H_ERRNO_BIAS   (1024)
	#define NET_HOST_NOT_FOUND MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAHOST_NOT_FOUND)
	#define NET_TRY_AGAIN      MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSATRY_AGAIN)
	#define NET_NO_RECOVERY    MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSANO_RECOVERY)
	#define NET_NO_DATA        MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSANO_DATA)
	#define NET_NO_ADDRESS     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSANO_ADDRESS)
//@}

	#if CRY_PLATFORM_WINAPI
//! extended winsock errors
//@{
		#define NET_SYSNOTREADY     MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSASYSNOTREADY)
		#define NET_VERNOTSUPPORTED MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAVERNOTSUPPORTED)
		#define NET_NOTINITIALISED  MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSANOTINITIALISED)
		#define NET_EDISCON         MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, WSAEDISCON)
//@}
	#endif
// CryNet specific errors and messages
	#define NET_FACILITY_CRYNETWORK 0x02000000

	#define NET_NOIMPL              MAKE_NRESULT(NET_FAIL, NET_FACILITY_CRYNETWORK, 0x01)
	#define NET_SOCKET_NOT_CREATED  MAKE_NRESULT(NET_FAIL, NET_FACILITY_CRYNETWORK, 0x02)

struct tNetError tNetErrors[] =
{
	{ NET_OK,                 "No Error"                                                   },
	{ NET_FAIL,               "Generic Error"                                              },
	// SOCKET
	{ NET_EINTR,              "WSAEINTR - interrupted function call"                       },
	{ NET_EBADF,              "WSAEBADF - Bad file number"                                 },
	{ NET_EACCES,             "WSAEACCES - error in accessing socket"                      },
	{ NET_EFAULT,             "WSAEFAULT - bad address"                                    },
	{ NET_EINVAL,             "WSAEINVAL - invalid argument"                               },
	{ NET_EMFILE,             "WSAEMFILE - too many open files"                            },
	{ NET_EWOULDBLOCK,        "WSAEWOULDBLOCK - resource temporarily unavailable"          },
	{ NET_EINPROGRESS,        "WSAEINPROGRESS - operation now in progress"                 },
	{ NET_EALREADY,           "WSAEALREADY - operation already in progress"                },
	{ NET_ENOTSOCK,           "WSAENOTSOCK - socket operation on non-socket"               },
	{ NET_EDESTADDRREQ,       "WSAEDESTADDRREQ - destination address required"             },
	{ NET_EMSGSIZE,           "WSAEMSGSIZE - message to long"                              },
	{ NET_EPROTOTYPE,         "WSAEPROTOTYPE - protocol wrong type for socket"             },
	{ NET_ENOPROTOOPT,        "WSAENOPROTOOPT - bad protocol option"                       },
	{ NET_EPROTONOSUPPORT,    "WSAEPROTONOSUPPORT - protocol not supported"                },
	{ NET_ESOCKTNOSUPPORT,    "WSAESOCKTNOSUPPORT - socket type not supported"             },
	{ NET_EOPNOTSUPP,         "WSAEOPNOTSUPP - operation not supported"                    },
	{ NET_EPFNOSUPPORT,       "WSAEPFNOSUPPORT - protocol family not supported"            },
	{ NET_EAFNOSUPPORT,       "WSAEAFNOSUPPORT - address family not supported by protocol" },
	{ NET_EADDRINUSE,         "WSAEADDRINUSE - address is in use"                          },
	{ NET_EADDRNOTAVAIL,      "WSAEADDRNOTAVAIL - address is not valid in context"         },
	{ NET_ENETDOWN,           "WSAENETDOWN - network is down"                              },
	{ NET_ENETUNREACH,        "WSAENETUNREACH - network is unreachable"                    },
	{ NET_ENETRESET,          "WSAENETRESET - network dropped connection on reset"         },
	{ NET_ECONNABORTED,       "WSACONNABORTED - software caused connection aborted"        },
	{ NET_ECONNRESET,         "WSAECONNRESET - connection reset by peer"                   },
	{ NET_ENOBUFS,            "WSAENOBUFS - no buffer space available"                     },
	{ NET_EISCONN,            "WSAEISCONN - socket is already connected"                   },
	{ NET_ENOTCONN,           "WSAENOTCONN - socket is not connected"                      },
	{ NET_ESHUTDOWN,          "WSAESHUTDOWN - cannot send after socket shutdown"           },
	{ NET_ETOOMANYREFS,       "WSAETOOMANYREFS - Too many references: cannot splice"       },
	{ NET_ETIMEDOUT,          "WSAETIMEDOUT - connection timed out"                        },
	{ NET_ECONNREFUSED,       "WSAECONNREFUSED - connection refused"                       },
	{ NET_ELOOP,              "WSAELOOP - Too many levels of symbolic links"               },
	{ NET_ENAMETOOLONG,       "WSAENAMETOOLONG - File name too long"                       },
	{ NET_EHOSTDOWN,          "WSAEHOSTDOWN - host is down"                                },
	{ NET_EHOSTUNREACH,       "WSAEHOSTUNREACH - no route to host"                         },
	{ NET_ENOTEMPTY,          "WSAENOTEMPTY - Cannot remove a directory that is not empty" },
	{ NET_EUSERS,             "WSAEUSERS - Ran out of quota"                               },
	{ NET_EDQUOT,             "WSAEDQUOT - Ran out of disk quota"                          },
	{ NET_ESTALE,             "WSAESTALE - File handle reference is no longer available"   },
	{ NET_EREMOTE,            "WSAEREMOTE - Item is not available locally"                 },

	// extended winsock errors(not BSD compliant)
	#if CRY_PLATFORM_WINAPI
	{ NET_EPROCLIM,           "WSAEPROCLIM - too many processes"                           },
	{ NET_SYSNOTREADY,        "WSASYSNOTREADY - network subsystem is unavailable"          },
	{ NET_VERNOTSUPPORTED,    "WSAVERNOTSUPPORTED - winsock.dll verison out of range"      },
	{ NET_NOTINITIALISED,     "WSANOTINITIALISED - WSAStartup not yet performed"           },
	{ NET_NO_DATA,            "WSANO_DATA - valid name, no data record of requested type"  },
	{ NET_EDISCON,            "WSAEDISCON - graceful shutdown in progress"                 },
	#endif

	// extended winsock errors (corresponding to BSD h_errno)
	{ NET_HOST_NOT_FOUND,     "WSAHOST_NOT_FOUND - host not found"                         },
	{ NET_TRY_AGAIN,          "WSATRY_AGAIN - non-authoritative host not found"            },
	{ NET_NO_RECOVERY,        "WSANO_RECOVERY - non-recoverable error"                     },
	{ NET_NO_DATA,            "WSANO_DATA - valid name, no data record of requested type"  },
	{ NET_NO_ADDRESS,         "WSANO_ADDRESS - no address, look for MX record"             },

	// XNetwork specific
	{ NET_NOIMPL,             "XNetwork - Function not implemented"                        },
	{ NET_SOCKET_NOT_CREATED, "XNetwork - socket not yet created"                          },
	{ 0,                      0                                                            } // sentinel
};

//////////////////////////////////////////////////////////////////////////
const char* CViconClient::GetErrorDescription(uint32 nError)
{
	uint32 nRes = MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, nError);
	int n = 0;
	while (tNetErrors[n].sErrorDescription != '\0')
	{
		if (tNetErrors[n].nrErrorCode == nRes)
			return(tNetErrors[n].sErrorDescription);
		n++;
	}
	return (NULL);
}

//////////////////////////////////////////////////////////////////////////
void CViconClient::PrintErrorDescription(uint32 nError)
{
	/*
	   uint32 numJoints=sizeof(DefSkel)/sizeof(SDefaultJoint);

	   const char *szRes=GetErrorDescription(nError);
	   if (szRes)
	   gEnv->pLog->LogError(szRes);
	 */
}

#endif

