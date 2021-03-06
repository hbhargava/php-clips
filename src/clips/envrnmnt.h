   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*                ENVRNMNT HEADER FILE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for supporting multiple environments.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added code to CreateEnvironment to free        */
/*            already allocated data if one of the malloc    */
/*            calls fail.                                    */
/*                                                           */
/*            Modified AllocateEnvironmentData to print a    */
/*            message if it was unable to allocate memory.   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CreateRuntimeEnvironment function.       */
/*                                                           */
/*            Added support for context information when an  */
/*            environment is created (i.e a pointer from the */
/*            CLIPS environment to its parent environment).  */
/*                                                           */
/*      6.30: Added support for passing context information  */ 
/*            to user defined functions and callback         */
/*            functions.                                     */
/*                                                           */
/*            Support for hashing EXTERNAL_ADDRESS data      */
/*            type.                                          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
/*                                                           */
/*            Removed support for environment globals.       */
/*                                                           */
/*************************************************************/

#ifndef _H_envrnmnt

#pragma once

#define _H_envrnmnt

#include <stdbool.h>

#include "symbol.h"

#define USER_ENVIRONMENT_DATA 70
#define MAXIMUM_ENVIRONMENT_POSITIONS 100

struct environmentCleanupFunction
  {
   const char *name;
   void (*func)(void *);
   int priority;
   struct environmentCleanupFunction *next;
  };

typedef struct environmentData
  {   
   unsigned int initialized : 1;
   void *context;
   void *routerContext;
   void *functionContext;
   void *callbackContext;
   void **theData;
   void (**cleanupFunctions)(void *);
   struct environmentCleanupFunction *listOfCleanupEnvironmentFunctions;
   struct environmentData *next;
  } Environment;

typedef struct environmentData ENVIRONMENT_DATA;
typedef struct environmentData * ENVIRONMENT_DATA_PTR;

#define GetEnvironmentData(theEnv,position) (((struct environmentData *) theEnv)->theData[position])
#define SetEnvironmentData(theEnv,position,value) (((struct environmentData *) theEnv)->theData[position] = value)

   bool                           AllocateEnvironmentData(void *,unsigned int,unsigned long,void (*)(void *));
   void                          *CreateEnvironment(void);
   void                          *CreateRuntimeEnvironment(struct symbolHashNode **,struct floatHashNode **,
                                                                  struct integerHashNode **,struct bitMapHashNode **);
   bool                           DestroyEnvironment(void *);
   bool                           AddEnvironmentCleanupFunction(void *,const char *,void (*)(void *),int);
   void                          *GetEnvironmentContext(void *);
   void                          *SetEnvironmentContext(void *,void *);
   void                          *GetEnvironmentRouterContext(void *);
   void                          *SetEnvironmentRouterContext(void *,void *);
   void                          *GetEnvironmentFunctionContext(void *);
   void                          *SetEnvironmentFunctionContext(void *,void *);
   void                          *GetEnvironmentCallbackContext(void *);
   void                          *SetEnvironmentCallbackContext(void *,void *);

#endif /* _H_envrnmnt */

