   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*              DEFFUNCTION HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added missing initializer for ENTITY_RECORD.   */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_dffnxfun

#pragma once

#define _H_dffnxfun

typedef struct deffunctionStruct DEFFUNCTION;
typedef struct deffunctionModule DEFFUNCTION_MODULE;

#include "conscomp.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "expressn.h"
#include "moduldef.h"
#include "symbol.h"

struct deffunctionModule
  {
   struct defmoduleItemHeader header;
  };

struct deffunctionStruct
  {
   struct constructHeader header;
   unsigned busy,
            executing;
   bool trace;
   EXPRESSION *code;
   int minNumberOfParameters,
       maxNumberOfParameters,
       numberOfLocalVars;
  };
  
#define DEFFUNCTION_DATA 23

struct deffunctionData
  { 
   struct construct *DeffunctionConstruct;
   int DeffunctionModuleIndex;
   ENTITY_RECORD DeffunctionEntityRecord;
#if DEBUGGING_FUNCTIONS
   unsigned WatchDeffunctions;
#endif
   struct CodeGeneratorItem *DeffunctionCodeItem;
   DEFFUNCTION *ExecutingDeffunction;
#if (! BLOAD_ONLY) && (! RUN_TIME)
   struct token DFInputToken;
#endif
  };

#define DeffunctionData(theEnv) ((struct deffunctionData *) GetEnvironmentData(theEnv,DEFFUNCTION_DATA))

   bool                           CheckDeffunctionCall(void *,void *,int);
   void                           DeffunctionGetBind(DATA_OBJECT *);
   void                           DFRtnUnknown(DATA_OBJECT *);
   void                           DFWildargs(DATA_OBJECT *);
   const char                    *EnvDeffunctionModule(void *,void *);
   void                          *EnvFindDeffunction(void *,const char *);
   void                          *EnvFindDeffunctionInModule(void *,const char *);
   void                           EnvGetDeffunctionList(void *,DATA_OBJECT *,struct defmodule *);
   const char                    *EnvGetDeffunctionName(void *,void *);
   SYMBOL_HN                     *EnvGetDeffunctionNamePointer(void *,void *);
   const char                    *EnvGetDeffunctionPPForm(void *,void *);
   void                          *EnvGetNextDeffunction(void *,void *);
   bool                           EnvIsDeffunctionDeletable(void *,void *);
   void                           EnvSetDeffunctionPPForm(void *,void *,const char *);
   bool                           EnvUndeffunction(void *,void *);
   void                           GetDeffunctionListFunction(UDFContext *,CLIPSValue *);
   void                           GetDeffunctionModuleCommand(UDFContext *,CLIPSValue *);
   DEFFUNCTION                   *LookupDeffunctionByMdlOrScope(void *,const char *);
   DEFFUNCTION                   *LookupDeffunctionInScope(void *,const char *);
#if (! BLOAD_ONLY) && (! RUN_TIME)
   void                           RemoveDeffunction(void *,void *);
#endif
   void                           SetupDeffunctions(void *);
   void                           UndeffunctionCommand(UDFContext *,CLIPSValue *);
#if DEBUGGING_FUNCTIONS
   bool                           EnvGetDeffunctionWatch(void *,void *);
   void                           EnvListDeffunctions(void *,const char *,struct defmodule *);
   void                           EnvSetDeffunctionWatch(void *,bool,void *);
   void                           ListDeffunctionsCommand(UDFContext *,CLIPSValue *);
   void                           PPDeffunctionCommand(UDFContext *,CLIPSValue *);
#endif

#endif /* _H_dffnxfun */






