

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for gen/chrome/credential_provider/gaiacp/gaia_credential_provider.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=ARM64 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_ARM64)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4152 )  /* function/data pointer conversion in expression */

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 475
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif /* __RPCPROXY_H_VERSION__ */


#include "gaia_credential_provider_i.h"

#define TYPE_FORMAT_STRING_SIZE   93                                
#define PROC_FORMAT_STRING_SIZE   383                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   1            

typedef struct _gaia_credential_provider_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } gaia_credential_provider_MIDL_TYPE_FORMAT_STRING;

typedef struct _gaia_credential_provider_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } gaia_credential_provider_MIDL_PROC_FORMAT_STRING;

typedef struct _gaia_credential_provider_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } gaia_credential_provider_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};


extern const gaia_credential_provider_MIDL_TYPE_FORMAT_STRING gaia_credential_provider__MIDL_TypeFormatString;
extern const gaia_credential_provider_MIDL_PROC_FORMAT_STRING gaia_credential_provider__MIDL_ProcFormatString;
extern const gaia_credential_provider_MIDL_EXPR_FORMAT_STRING gaia_credential_provider__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGaiaCredentialProvider_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGaiaCredentialProvider_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGaiaCredentialProviderForTesting_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGaiaCredentialProviderForTesting_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGaiaCredential_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGaiaCredential_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IReauthCredential_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IReauthCredential_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const gaia_credential_provider_MIDL_PROC_FORMAT_STRING gaia_credential_provider__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure OnUserAuthenticated */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 16 */	0x12,		/* 18 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x1 ),	/* 1 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x6 ),	/* 6 */
/* 26 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 30 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 32 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter credential */

/* 34 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 36 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 38 */	NdrFcShort( 0x2 ),	/* Type Offset=2 */

	/* Parameter username */

/* 40 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 42 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 44 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter password */

/* 46 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 48 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 50 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter sid */

/* 52 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 54 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 56 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter fire_credentials_changed */

/* 58 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 60 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 62 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 64 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 66 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 68 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Terminate */


	/* Procedure HasInternetConnection */

/* 70 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 72 */	NdrFcLong( 0x0 ),	/* 0 */
/* 76 */	NdrFcShort( 0x4 ),	/* 4 */
/* 78 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 80 */	NdrFcShort( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x8 ),	/* 8 */
/* 84 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 86 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x0 ),	/* 0 */
/* 92 */	NdrFcShort( 0x0 ),	/* 0 */
/* 94 */	NdrFcShort( 0x1 ),	/* 1 */
/* 96 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 98 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 100 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 102 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure SetHasInternetConnection */

/* 104 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 106 */	NdrFcLong( 0x0 ),	/* 0 */
/* 110 */	NdrFcShort( 0x3 ),	/* 3 */
/* 112 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 114 */	NdrFcShort( 0x6 ),	/* 6 */
/* 116 */	NdrFcShort( 0x8 ),	/* 8 */
/* 118 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 120 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 122 */	NdrFcShort( 0x0 ),	/* 0 */
/* 124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x0 ),	/* 0 */
/* 128 */	NdrFcShort( 0x2 ),	/* 2 */
/* 130 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 132 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hic */

/* 134 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 136 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 138 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 142 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Initialize */

/* 146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 152 */	NdrFcShort( 0x3 ),	/* 3 */
/* 154 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0x8 ),	/* 8 */
/* 160 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 162 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 170 */	NdrFcShort( 0x2 ),	/* 2 */
/* 172 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 174 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter provider */

/* 176 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 178 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 180 */	NdrFcShort( 0x38 ),	/* Type Offset=56 */

	/* Return value */

/* 182 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 184 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 186 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnUserAuthenticated */

/* 188 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 190 */	NdrFcLong( 0x0 ),	/* 0 */
/* 194 */	NdrFcShort( 0x5 ),	/* 5 */
/* 196 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 200 */	NdrFcShort( 0x8 ),	/* 8 */
/* 202 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 204 */	0xe,		/* 14 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 206 */	NdrFcShort( 0x1 ),	/* 1 */
/* 208 */	NdrFcShort( 0x1 ),	/* 1 */
/* 210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 212 */	NdrFcShort( 0x3 ),	/* 3 */
/* 214 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 216 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter authentication_info */

/* 218 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 220 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 222 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter status_text */

/* 224 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 226 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 228 */	NdrFcShort( 0x52 ),	/* Type Offset=82 */

	/* Return value */

/* 230 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 232 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 234 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure ReportError */

/* 236 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 238 */	NdrFcLong( 0x0 ),	/* 0 */
/* 242 */	NdrFcShort( 0x6 ),	/* 6 */
/* 244 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 246 */	NdrFcShort( 0x10 ),	/* 16 */
/* 248 */	NdrFcShort( 0x8 ),	/* 8 */
/* 250 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 252 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 254 */	NdrFcShort( 0x0 ),	/* 0 */
/* 256 */	NdrFcShort( 0x1 ),	/* 1 */
/* 258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 260 */	NdrFcShort( 0x4 ),	/* 4 */
/* 262 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 264 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 266 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter status */

/* 268 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 270 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 272 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter substatus */

/* 274 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 276 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 278 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter status_text */

/* 280 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 282 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 284 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 286 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 288 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 290 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure SetEmailForReauth */

/* 292 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 294 */	NdrFcLong( 0x0 ),	/* 0 */
/* 298 */	NdrFcShort( 0x3 ),	/* 3 */
/* 300 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 304 */	NdrFcShort( 0x8 ),	/* 8 */
/* 306 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 308 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 312 */	NdrFcShort( 0x1 ),	/* 1 */
/* 314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 316 */	NdrFcShort( 0x2 ),	/* 2 */
/* 318 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 320 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter email */

/* 322 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 324 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 326 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 328 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 330 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 332 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure SetOSUserInfo */

/* 334 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 336 */	NdrFcLong( 0x0 ),	/* 0 */
/* 340 */	NdrFcShort( 0x4 ),	/* 4 */
/* 342 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 346 */	NdrFcShort( 0x8 ),	/* 8 */
/* 348 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 350 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x1 ),	/* 1 */
/* 356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 358 */	NdrFcShort( 0x3 ),	/* 3 */
/* 360 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 362 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter sid */

/* 364 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 366 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 368 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter username */

/* 370 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 372 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 374 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 376 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 378 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 380 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const gaia_credential_provider_MIDL_TYPE_FORMAT_STRING gaia_credential_provider__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/*  4 */	NdrFcLong( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x0 ),	/* 0 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 14 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 16 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 18 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 20 */	
			0x12, 0x0,	/* FC_UP */
/* 22 */	NdrFcShort( 0xe ),	/* Offset= 14 (36) */
/* 24 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 26 */	NdrFcShort( 0x2 ),	/* 2 */
/* 28 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 30 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 32 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 34 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 36 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 38 */	NdrFcShort( 0x8 ),	/* 8 */
/* 40 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (24) */
/* 42 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 44 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 46 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 48 */	NdrFcShort( 0x0 ),	/* 0 */
/* 50 */	NdrFcShort( 0x8 ),	/* 8 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0xffde ),	/* Offset= -34 (20) */
/* 56 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 58 */	NdrFcLong( 0xcec9ef6c ),	/* -825626772 */
/* 62 */	NdrFcShort( 0xb2e6 ),	/* -19738 */
/* 64 */	NdrFcShort( 0x4bb6 ),	/* 19382 */
/* 66 */	0x8f,		/* 143 */
			0x1e,		/* 30 */
/* 68 */	0x17,		/* 23 */
			0x47,		/* 71 */
/* 70 */	0xba,		/* 186 */
			0x4f,		/* 79 */
/* 72 */	0x71,		/* 113 */
			0x38,		/* 56 */
/* 74 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 76 */	NdrFcShort( 0x6 ),	/* Offset= 6 (82) */
/* 78 */	
			0x13, 0x0,	/* FC_OP */
/* 80 */	NdrFcShort( 0xffd4 ),	/* Offset= -44 (36) */
/* 82 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 84 */	NdrFcShort( 0x0 ),	/* 0 */
/* 86 */	NdrFcShort( 0x8 ),	/* 8 */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (78) */

			0x0
        }
    };

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ] = 
        {
            
            {
            BSTR_UserSize
            ,BSTR_UserMarshal
            ,BSTR_UserUnmarshal
            ,BSTR_UserFree
            }

        };



/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IGaiaCredentialProvider, ver. 0.0,
   GUID={0xCEC9EF6C,0xB2E6,0x4BB6,{0x8F,0x1E,0x17,0x47,0xBA,0x4F,0x71,0x38}} */

#pragma code_seg(".orpc")
static const unsigned short IGaiaCredentialProvider_FormatStringOffsetTable[] =
    {
    0,
    70
    };

static const MIDL_STUBLESS_PROXY_INFO IGaiaCredentialProvider_ProxyInfo =
    {
    &Object_StubDesc,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredentialProvider_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGaiaCredentialProvider_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredentialProvider_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IGaiaCredentialProviderProxyVtbl = 
{
    &IGaiaCredentialProvider_ProxyInfo,
    &IID_IGaiaCredentialProvider,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGaiaCredentialProvider::OnUserAuthenticated */ ,
    (void *) (INT_PTR) -1 /* IGaiaCredentialProvider::HasInternetConnection */
};

const CInterfaceStubVtbl _IGaiaCredentialProviderStubVtbl =
{
    &IID_IGaiaCredentialProvider,
    &IGaiaCredentialProvider_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_gaia_credential_provider_0000_0001, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IGaiaCredentialProviderForTesting, ver. 0.0,
   GUID={0x224CE2FB,0x2977,0x4585,{0xBD,0x46,0x1B,0xAE,0x8D,0x79,0x64,0xDE}} */

#pragma code_seg(".orpc")
static const unsigned short IGaiaCredentialProviderForTesting_FormatStringOffsetTable[] =
    {
    104
    };

static const MIDL_STUBLESS_PROXY_INFO IGaiaCredentialProviderForTesting_ProxyInfo =
    {
    &Object_StubDesc,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredentialProviderForTesting_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGaiaCredentialProviderForTesting_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredentialProviderForTesting_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IGaiaCredentialProviderForTestingProxyVtbl = 
{
    &IGaiaCredentialProviderForTesting_ProxyInfo,
    &IID_IGaiaCredentialProviderForTesting,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGaiaCredentialProviderForTesting::SetHasInternetConnection */
};

const CInterfaceStubVtbl _IGaiaCredentialProviderForTestingStubVtbl =
{
    &IID_IGaiaCredentialProviderForTesting,
    &IGaiaCredentialProviderForTesting_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IGaiaCredential, ver. 0.0,
   GUID={0xE5BF88DF,0x9966,0x465B,{0xB2,0x33,0xC1,0xCA,0xC7,0x51,0x0A,0x59}} */

#pragma code_seg(".orpc")
static const unsigned short IGaiaCredential_FormatStringOffsetTable[] =
    {
    146,
    70,
    188,
    236
    };

static const MIDL_STUBLESS_PROXY_INFO IGaiaCredential_ProxyInfo =
    {
    &Object_StubDesc,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredential_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGaiaCredential_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IGaiaCredential_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IGaiaCredentialProxyVtbl = 
{
    &IGaiaCredential_ProxyInfo,
    &IID_IGaiaCredential,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGaiaCredential::Initialize */ ,
    (void *) (INT_PTR) -1 /* IGaiaCredential::Terminate */ ,
    (void *) (INT_PTR) -1 /* IGaiaCredential::OnUserAuthenticated */ ,
    (void *) (INT_PTR) -1 /* IGaiaCredential::ReportError */
};

const CInterfaceStubVtbl _IGaiaCredentialStubVtbl =
{
    &IID_IGaiaCredential,
    &IGaiaCredential_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IReauthCredential, ver. 0.0,
   GUID={0xCC75BCEA,0xA636,0x4798,{0xBF,0x8E,0x0F,0xF6,0x4D,0x74,0x34,0x51}} */

#pragma code_seg(".orpc")
static const unsigned short IReauthCredential_FormatStringOffsetTable[] =
    {
    292,
    334
    };

static const MIDL_STUBLESS_PROXY_INFO IReauthCredential_ProxyInfo =
    {
    &Object_StubDesc,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IReauthCredential_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IReauthCredential_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    gaia_credential_provider__MIDL_ProcFormatString.Format,
    &IReauthCredential_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IReauthCredentialProxyVtbl = 
{
    &IReauthCredential_ProxyInfo,
    &IID_IReauthCredential,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IReauthCredential::SetEmailForReauth */ ,
    (void *) (INT_PTR) -1 /* IReauthCredential::SetOSUserInfo */
};

const CInterfaceStubVtbl _IReauthCredentialStubVtbl =
{
    &IID_IReauthCredential,
    &IReauthCredential_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};

static const MIDL_STUB_DESC Object_StubDesc = 
    {
    0,
    NdrOleAllocate,
    NdrOleFree,
    0,
    0,
    0,
    0,
    0,
    gaia_credential_provider__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x801026e, /* MIDL Version 8.1.622 */
    0,
    UserMarshalRoutines,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

const CInterfaceProxyVtbl * const _gaia_credential_provider_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IGaiaCredentialProviderProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGaiaCredentialProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IReauthCredentialProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGaiaCredentialProviderForTestingProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _gaia_credential_provider_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IGaiaCredentialProviderStubVtbl,
    ( CInterfaceStubVtbl *) &_IGaiaCredentialStubVtbl,
    ( CInterfaceStubVtbl *) &_IReauthCredentialStubVtbl,
    ( CInterfaceStubVtbl *) &_IGaiaCredentialProviderForTestingStubVtbl,
    0
};

PCInterfaceName const _gaia_credential_provider_InterfaceNamesList[] = 
{
    "IGaiaCredentialProvider",
    "IGaiaCredential",
    "IReauthCredential",
    "IGaiaCredentialProviderForTesting",
    0
};


#define _gaia_credential_provider_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _gaia_credential_provider, pIID, n)

int __stdcall _gaia_credential_provider_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _gaia_credential_provider, 4, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _gaia_credential_provider, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _gaia_credential_provider, 4, *pIndex )
    
}

const ExtendedProxyFileInfo gaia_credential_provider_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _gaia_credential_provider_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _gaia_credential_provider_StubVtblList,
    (const PCInterfaceName * ) & _gaia_credential_provider_InterfaceNamesList,
    0, /* no delegation */
    & _gaia_credential_provider_IID_Lookup, 
    4,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_ARM64)*/

