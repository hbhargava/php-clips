   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*         DEFMODULE BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the defmodule       */
/*   construct such as clear, reset, save, ppdefmodule       */
/*   list-defmodules, and get-defmodule-list.                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "bload.h"
#include "constrct.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "modulbin.h"
#include "modulcmp.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"

#include "modulbsc.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    ClearDefmodules(void *);
#if DEFMODULE_CONSTRUCT
   static void                    SaveDefmodules(void *,void *,const char *);
#endif

/*****************************************************************/
/* DefmoduleBasicCommands: Initializes basic defmodule commands. */
/*****************************************************************/
void DefmoduleBasicCommands(
  void *theEnv)
  {
   EnvAddClearFunction(theEnv,"defmodule",ClearDefmodules,2000);

#if DEFMODULE_CONSTRUCT
   AddSaveFunction(theEnv,"defmodule",SaveDefmodules,1100);

#if ! RUN_TIME
   EnvAddUDF(theEnv,"get-defmodule-list","m", EnvGetDefmoduleListFunction,"EnvGetDefmoduleListFunction",0,0,NULL,NULL);

#if DEBUGGING_FUNCTIONS
   EnvAddUDF(theEnv,"list-defmodules","v", ListDefmodulesCommand,"ListDefmodulesCommand",0,0,NULL,NULL);
   EnvAddUDF(theEnv,"ppdefmodule","v",PPDefmoduleCommand,"PPDefmoduleCommand",1,1,"y",NULL);
#endif
#endif
#endif

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   DefmoduleBinarySetup(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   DefmoduleCompilerSetup(theEnv);
#endif
  }

/*********************************************************/
/* ClearDefmodules: Defmodule clear routine for use with */
/*   the clear command. Creates the MAIN module.         */
/*********************************************************/
static void ClearDefmodules(
  void *theEnv)
  {
#if (BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY) && (! RUN_TIME)
   if (Bloaded(theEnv) == true) return;
#endif
#if (! RUN_TIME)
   RemoveAllDefmodules(theEnv);

   CreateMainModule(theEnv);
   DefmoduleData(theEnv)->MainModuleRedefinable = true;
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

#if DEFMODULE_CONSTRUCT

/******************************************/
/* SaveDefmodules: Defmodule save routine */
/*   for use with the save command.       */
/******************************************/
static void SaveDefmodules(
  void *theEnv,
  void *theModule,
  const char *logicalName)
  {
   const char *ppform;

   ppform = EnvGetDefmodulePPForm(theEnv,theModule);
   if (ppform != NULL)
     {
      PrintInChunks(theEnv,logicalName,ppform);
      EnvPrintRouter(theEnv,logicalName,"\n");
     }
  }
  
/***************************************************/
/* EnvGetDefmoduleListFunction: H/L access routine */
/*   for the get-defmodule-list function.          */
/***************************************************/
void EnvGetDefmoduleListFunction(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   EnvGetDefmoduleList(UDFContextEnvironment(context),returnValue);
  }

/*************************************************/
/* EnvGetDefmoduleList: H/L and C access routine */
/*   for the get-defmodule-list function.        */
/*************************************************/
void EnvGetDefmoduleList(
  void *theEnv,
  CLIPSValue *returnValue)
  {
   void *theConstruct;
   unsigned long count = 0;
   struct multifield *theList;

   /*====================================*/
   /* Determine the number of constructs */
   /* of the specified type.             */
   /*====================================*/

   for (theConstruct = EnvGetNextDefmodule(theEnv,NULL);
        theConstruct != NULL;
        theConstruct = EnvGetNextDefmodule(theEnv,theConstruct))
     { count++; }

   /*===========================*/
   /* Create a multifield large */
   /* enough to store the list. */
   /*===========================*/

   SetpType(returnValue,MULTIFIELD);
   SetpDOBegin(returnValue,1);
   SetpDOEnd(returnValue,(long) count);
   theList = (struct multifield *) EnvCreateMultifield(theEnv,count);
   SetpValue(returnValue,(void *) theList);

   /*====================================*/
   /* Store the names in the multifield. */
   /*====================================*/

   for (theConstruct = EnvGetNextDefmodule(theEnv,NULL), count = 1;
        theConstruct != NULL;
        theConstruct = EnvGetNextDefmodule(theEnv,theConstruct), count++)
     {
      if (EvaluationData(theEnv)->HaltExecution == true)
        {
         EnvSetMultifieldErrorValue(theEnv,returnValue);
         return;
        }
      SetMFType(theList,count,SYMBOL);
      SetMFValue(theList,count,EnvAddSymbol(theEnv,EnvGetDefmoduleName(theEnv,theConstruct)));
     }
  }

#if DEBUGGING_FUNCTIONS

/********************************************/
/* PPDefmoduleCommand: H/L access routine   */
/*   for the ppdefmodule command.           */
/********************************************/
void PPDefmoduleCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   const char *defmoduleName;
   void *theEnv = UDFContextEnvironment(context);

   defmoduleName = GetConstructName(context,"ppdefmodule","defmodule name");
   if (defmoduleName == NULL) return;

   PPDefmodule(theEnv,defmoduleName,WDISPLAY);

   return;
  }

/*************************************/
/* PPDefmodule: C access routine for */
/*   the ppdefmodule command.        */
/*************************************/
bool PPDefmodule(
  void *theEnv,
  const char *defmoduleName,
  const char *logicalName)
  {
   void *defmodulePtr;

   defmodulePtr = EnvFindDefmodule(theEnv,defmoduleName);
   if (defmodulePtr == NULL)
     {
      CantFindItemErrorMessage(theEnv,"defmodule",defmoduleName);
      return(false);
     }

   if (EnvGetDefmodulePPForm(theEnv,defmodulePtr) == NULL) return(true);
   PrintInChunks(theEnv,logicalName,EnvGetDefmodulePPForm(theEnv,defmodulePtr));
   return(true);
  }

/***********************************************/
/* ListDefmodulesCommand: H/L access routine   */
/*   for the list-defmodules command.          */
/***********************************************/
void ListDefmodulesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   EnvListDefmodules(UDFContextEnvironment(context),WDISPLAY);
  }

/***************************************/
/* EnvListDefmodules: C access routine */
/*   for the list-defmodules command.  */
/***************************************/
void EnvListDefmodules(
  void *theEnv,
  const char *logicalName)
  {
   void *theModule;
   int count = 0;

   for (theModule = EnvGetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
    {
     EnvPrintRouter(theEnv,logicalName,EnvGetDefmoduleName(theEnv,theModule));
     EnvPrintRouter(theEnv,logicalName,"\n");
     count++;
    }

   PrintTally(theEnv,logicalName,count,"defmodule","defmodules");
  }

#endif /* DEBUGGING_FUNCTIONS */

#endif /* DEFMODULE_CONSTRUCT */


