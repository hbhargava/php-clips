   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_METHODS compilation flag.   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when DEBUGGING_FUNCTIONS   */
/*            is set to 0 and PROFILING_FUNCTIONS is set to  */
/*            1.                                             */
/*                                                           */
/*            Fixed typing issue when OBJECT_SYSTEM          */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*************************************************************/

#ifndef _H_genrcfun

#pragma once

#define _H_genrcfun

typedef struct defgenericModule DEFGENERIC_MODULE;
typedef struct restriction RESTRICTION;
typedef struct method DEFMETHOD;
typedef struct defgeneric DEFGENERIC;

#include <stdio.h>

#include "conscomp.h"
#include "constrct.h"
#include "expressn.h"
#include "evaluatn.h"
#include "moduldef.h"
#include "symbol.h"

#if OBJECT_SYSTEM
#include "object.h"
#endif

struct defgenericModule
  {
   struct defmoduleItemHeader header;
  };

struct restriction
  {
   void **types;
   EXPRESSION *query;
   short tcnt;
  };

struct method
  {
   short index;
   unsigned busy;
   short restrictionCount;
   short minRestrictions;
   short maxRestrictions;
   short localVarCount;
   unsigned system : 1;
   unsigned trace : 1;
   RESTRICTION *restrictions;
   EXPRESSION *actions;
   char *ppForm;
   struct userData *usrData;
  };

struct defgeneric
  {
   struct constructHeader header;
   unsigned busy;
   bool trace;
   DEFMETHOD *methods;
   short mcnt;
   short new_index;
  };

#define DEFGENERIC_DATA 27

struct defgenericData
  { 
   struct construct *DefgenericConstruct;
   int DefgenericModuleIndex;
   ENTITY_RECORD GenericEntityRecord;
#if DEBUGGING_FUNCTIONS
   unsigned WatchGenerics;
   unsigned WatchMethods;
#endif
   DEFGENERIC *CurrentGeneric;
   DEFMETHOD *CurrentMethod;
   DATA_OBJECT *GenericCurrentArgument;
#if (! RUN_TIME) && (! BLOAD_ONLY)
   unsigned OldGenericBusySave;
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefgenericCodeItem;
#endif
#if (! BLOAD_ONLY) && (! RUN_TIME)
   struct token GenericInputToken;
#endif
  };

#define DefgenericData(theEnv) ((struct defgenericData *) GetEnvironmentData(theEnv,DEFGENERIC_DATA))
#define SaveBusyCount(gfunc)    (DefgenericData(theEnv)->OldGenericBusySave = gfunc->busy)
#define RestoreBusyCount(gfunc) (gfunc->busy = DefgenericData(theEnv)->OldGenericBusySave)

#if ! RUN_TIME
   bool                           ClearDefgenericsReady(void *);
   void                          *AllocateDefgenericModule(void *);
   void                           FreeDefgenericModule(void *,void *);
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

   bool                           ClearDefmethods(void *);
   bool                           RemoveAllExplicitMethods(void *,DEFGENERIC *);
   void                           RemoveDefgeneric(void *,void *);
   bool                           ClearDefgenerics(void *);
   void                           MethodAlterError(void *,DEFGENERIC *);
   void                           DeleteMethodInfo(void *,DEFGENERIC *,DEFMETHOD *);
   void                           DestroyMethodInfo(void *,DEFGENERIC *,DEFMETHOD *);
   bool                           MethodsExecuting(DEFGENERIC *);
#endif
#if ! OBJECT_SYSTEM
   bool                           SubsumeType(int,int);
#endif

   long                           FindMethodByIndex(DEFGENERIC *,long);
#if DEBUGGING_FUNCTIONS || PROFILING_FUNCTIONS
   void                           PrintMethod(void *,char *,size_t,DEFMETHOD *);
#endif
#if DEBUGGING_FUNCTIONS
   void                           PreviewGeneric(UDFContext *,CLIPSValue *);
#endif
   DEFGENERIC                    *CheckGenericExists(void *,const char *,const char *);
   long                           CheckMethodExists(void *,const char *,DEFGENERIC *,long);

#if ! OBJECT_SYSTEM
   const char                    *TypeName(void *,int);
#endif

   void                           PrintGenericName(void *,const char *,DEFGENERIC *);

#endif /* _H_genrcfun */

