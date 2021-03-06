   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*                DEFGLOBAL HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
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
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_globldef

#pragma once

#define _H_globldef

#include "conscomp.h"
#include "constrct.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "expressn.h"
#include "moduldef.h"
#include "symbol.h"

#define DEFGLOBAL_DATA 1

struct defglobalData
  { 
   struct construct *DefglobalConstruct;
   int DefglobalModuleIndex;  
   bool ChangeToGlobals;
#if DEBUGGING_FUNCTIONS
   unsigned WatchGlobals;
#endif
   bool ResetGlobals;
   struct entityRecord GlobalInfo;
   struct entityRecord DefglobalPtrRecord;
   long LastModuleIndex;
   struct defmodule *TheDefmodule;
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefglobalCodeItem;
#endif
  };

struct defglobal
  {
   struct constructHeader header;
   unsigned int watch   : 1;
   unsigned int inScope : 1;
   long busyCount;
   DATA_OBJECT current;
   struct expr *initial;
  };

struct defglobalModule
  {
   struct defmoduleItemHeader header;
  };

#define DefglobalData(theEnv) ((struct defglobalData *) GetEnvironmentData(theEnv,DEFGLOBAL_DATA))

   void                           InitializeDefglobals(void *);
   void                          *EnvFindDefglobal(void *,const char *);
   void                          *EnvFindDefglobalInModule(void *,const char *);
   void                          *EnvGetNextDefglobal(void *,void *);
   void                           CreateInitialFactDefglobal(void);
   bool                           EnvIsDefglobalDeletable(void *,void *);
   struct defglobalModule        *GetDefglobalModuleItem(void *,struct defmodule *);
   void                           QSetDefglobalValue(void *,struct defglobal *,DATA_OBJECT_PTR,bool);
   struct defglobal              *QFindDefglobal(void *,struct symbolHashNode *);
   void                           EnvGetDefglobalValueForm(void *,char *,size_t,void *);
   bool                           EnvGetGlobalsChanged(void *);
   void                           EnvSetGlobalsChanged(void *,bool);
   bool                           EnvGetDefglobalValue(void *,const char *,DATA_OBJECT_PTR);
   bool                           EnvSetDefglobalValue(void *,const char *,DATA_OBJECT_PTR);
   void                           UpdateDefglobalScope(void *);
   void                          *GetNextDefglobalInScope(void *,void *);
   bool                           QGetDefglobalValue(void *,void *,DATA_OBJECT_PTR);
   const char                    *EnvDefglobalModule(void *,void *);
   const char                    *EnvGetDefglobalName(void *,void *);
   const char                    *EnvGetDefglobalPPForm(void *,void *);

#endif /* _H_globldef */


