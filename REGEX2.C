/*
 *
 * regex2.c
 * Saturday, 7/13/1996.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <GnuType.h>
#include <GnuFile.h>
#include <GnuMisc.h>


PSZ  pszGLOBALERR;
PSZ  pszTOKENSTR;
UINT uTOKENIDX;


typedef struct _rx
   {
   UCHAR      cType;
   struct _rx *left;
   struct _rx *right;
   struct _rx *next;
   UCHAR      cChar;
   PSZ        pszStr;
   } RX;
typedef RX *PRX;



PVOID AddError (PSZ psz)
   {
   if (!pszGLOBALERR)
      pszGLOBALERR = psz;
   return NULL;
   }


/***************************************************************************/
/*                                                                         */
/* Tokenizer fns                                                           */
/*                                                                         */
/***************************************************************************/


void InitTokens (PSZ pszRegex)
   {
   if (!pszRegex || !*pszRegex)
      AddError ("No Pattern Specified");
   uTOKENIDX   = 0;
   pszTOKENSTR  = pszRegex;
   pszGLOBALERR = NULL;
   }

/*
 * returns one of:
 *   <   >
 *   {   }
 *   [   ]
 *   +   -
 *   @   *
 *   ?   ~
 *   |   C  (any other char)
 *   0 end of stream
 */
UCHAR GetToken (BOOL bEat)
   {
   UCHAR cTok;

   if (!(cTok = pszTOKENSTR [uTOKENIDX]))
      return 0;
   if (!strchr ("<>{}[]+@*?~|-", cTok))
      cTok = 'C';
   if (bEat)
      uTOKENIDX++;
   if (cTok == '\\')
      {
      if (!(cTok = pszTOKENSTR [uTOKENIDX]))
         return 0;
      if (bEat)
         uTOKENIDX++;
      }
   return cTok;
   }


/*
 * if current token is a 'C' this fn
 * will return the actual char
 */
UCHAR GetTokenVal (BOOL bEat)
   {
   UCHAR cTok;

   if (!(cTok = pszTOKENSTR [uTOKENIDX]))
      return 0;
   if (bEat)
      uTOKENIDX++;
   if (cTok == '\\')
      {
      if (!(cTok = pszTOKENSTR [uTOKENIDX]))
         return 0;
      if (bEat)
         uTOKENIDX++;
      }
   return cTok;
   }


/*
 *
 */
void EatToken (UCHAR cTok, PSZ pszErr)
   {
   if (cTok != GetToken (1))
      AddError (pszErr);
   }


/*
 * builds a string of consecutive literals
 * expands '-'s to be a range indicator
 */
PSZ GetTokenString (BOOL bRanges)
   {
   UCHAR cTok, cLast, cEnd, szList[512];
   UINT uSize = 0;

   cTok = GetToken (0);
   if (bRanges && cTok != 'C')
      return AddError ("Invalid Range");
   szList[uSize++] = cLast = GetTokenVal (1);

   while (TRUE)
      {
      cTok = GetToken (0);
      if (cTok == 'C')
         szList[uSize++] = cLast = GetTokenVal (1);
      else if (!bRanges && cTok == '-')
         szList[uSize++] = cLast = GetTokenVal (1);
      else if (bRanges && cTok == '-')
         {
         EatToken ('-', NULL);
         if ((cTok = GetToken (0)) != 'C')
            return AddError ("Invalid Range");
         if ((cEnd = GetTokenVal (1)) < cLast)
            return AddError ("Invalid Range");
         for (cTok=cLast+1; cTok<=cEnd; cTok++)
            szList[uSize++] = cTok;
         cLast = cEnd;
         }
      else
         break;
      }
   szList[uSize] = '\0';
   return strdup (szList);
   }


/***************************************************************************/
/*                                                                         */
/* Parser fns                                                              */
/*                                                                         */
/***************************************************************************/

PRX ParseR1 (void);


PRX NewOp (UCHAR cType)
   {
   PRX prx;

   prx = calloc (1, sizeof (RX));
   prx->cType = cType;
   return prx;
   }


PRX AddTerminator (PRX prxOrg)
   {
   PRX prx, prx2;

   for (prx=prxOrg; prx && prx->next; prx = prx->next)
      ;
   prx2 = NewOp ('>');
   if (prx)
      prx->next = prx2;
   else
      prx = prx2;
   return prxOrg;
   }


PSZ ReadRange (void)
   {
   UCHAR cTok;
   BOOL bInvert = FALSE;
   PSZ  psz;

   EatToken ('[', NULL);
   cTok = GetToken (0);
   if (cTok == '~')
      {
      EatToken ('~', NULL);
      bInvert = TRUE;
      }
   psz = GetTokenString (TRUE);
   EatToken (']', "Invalid [] closure");
   return psz;
   }




/*
 * R4 = '<' | '>' | '?' | '*'   |
 *      '[' ['~'] UCHARLIST1 ']' |
 *      '{' R1 '}'              |
 *      UCHAR2
 *
 * UCHARLIST1 = UCHAR1 { [ '-' UCHAR1 ] | UCHAR1}
 * UCHARLIST2 = UCHAR2 { UCHAR2 }
 * UCHAR1     = any char except {}[]<>?*|-  or  '\' ANYUCHAR
 * UCHAR2     = any char except {}[]<>?*|   or  '\' ANYUCHAR
 * ANYUCHAR   = all characters
 */
PRX ParseR4 (void)
   {
   UCHAR cTok;
   PRX  prx, prx2;

   cTok = GetToken(0);

   if (strchr ("<>?*", cTok))
      return NewOp (GetToken(1));
   if (cTok == 'C')
      {
      prx = NewOp ('C');
      prx->cChar = GetTokenVal (TRUE);
      return prx;
      }
   if (cTok == '{')
      {
//      prx = NewOp ('S');
      EatToken ('{', NULL);
      prx2 = ParseR1 ();
      EatToken ('}', "Invalid {} closure");
//      prx->left = prx2;
//      return prx;
      return prx2;
      }
   if (cTok == '[')
      {
      prx = NewOp ('O'); // 'or' string
      prx->pszStr = ReadRange ();
      return prx;
      }
   return AddError ("Invalid Closure");
   }


/*
 * R3 = R4 ['+' | '@']
 */
PRX ParseR3 (void)
   {
   PRX  prx1, prx2;
   UCHAR cTok;

   prx1 = ParseR4 ();
   cTok = GetToken(0);
   if (cTok == '+' || cTok == '@')
      {
      prx2 = NewOp (GetToken (1));
      prx2->left  = prx1;
      return prx2;
      }
   return prx1;
   }


/*
 * R2 = R3 [ '|' R2]
 */
PRX ParseR2 (void)
   {
   PRX prx1, prx2;

   prx1 = ParseR3 ();
   if (GetToken(0) == '|')
      {
      prx2 = NewOp (GetToken (1));
      prx2->left  = prx1;
      prx2->right = ParseR2 ();
      return prx2;
      }
   return prx1;
   }


/*
 * R1 = R2 {R2 ...}
 */
PRX ParseR1 (void)
   {
   PRX prx1, prx2, prxHead;
   UCHAR cTok;

   prxHead = prx1 = ParseR2 ();

   while (TRUE)
      {
      cTok = GetToken(0);
      if (!cTok || cTok == '}')
         break;
      prx2 = ParseR2 ();
      prx1->next = prx2;
      prx1 = prx2;
      }
   return prxHead;
   }


/*
 *
 */
PRX ParseRegex (PSZ pszRegex)
   {
   InitTokens (pszRegex);
   return ParseR1 ();
   }

/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

BOOL EvalRegex (PRX prx, PSZ pszStr, UINT uIndex, PUINT puNewIdx)
   {
   UINT i;

   if (kbhit ())
      Error ("\nUser break");

   if (!prx)
      {
      *puNewIdx = uIndex;
      return TRUE;
      }

   switch (prx->cType)
      {
      case 'C':
         if (pszStr[uIndex] != prx->cChar)
            return FALSE;
         return EvalRegex (prx->next, pszStr, uIndex+1, puNewIdx);

      case 'O':
         if (!strchr (prx->pszStr, pszStr[uIndex]))
            return FALSE;
         return EvalRegex (prx->next, pszStr, uIndex+1, puNewIdx);

      case '<':
         if (uIndex && pszStr[uIndex-1] != '\n')
            return FALSE;
         return EvalRegex (prx->next, pszStr, uIndex, puNewIdx);

      case '>':
         if (pszStr[uIndex] && pszStr[uIndex] != '\n')
            return FALSE;
         return EvalRegex (prx->next, pszStr, uIndex, puNewIdx);

      case '*':
         for (i=0; pszStr[uIndex+i]; i++)
            if (EvalRegex (prx->next, pszStr, uIndex+i, puNewIdx))
               return TRUE;
         return EvalRegex (prx->next, pszStr, uIndex+i, puNewIdx);

      case '?':
         if (!pszStr[uIndex])
            return FALSE;
         return EvalRegex (prx->next, pszStr, uIndex+1, puNewIdx);

//      case 'S': // scope
//         prx->uStart = uIndex;
//         bRet = EvalRegex (prx->next, pszStr, uIndex+1);
//         prx->uStart = uIndex;
//   
//

      case '+':
         if (!EvalRegex (prx->left, pszStr, uIndex, puNewIdx))
            return FALSE;
         uIndex = *puNewIdx;

         while (TRUE)
            {
            if (EvalRegex (prx->next, pszStr, uIndex, puNewIdx))
               return TRUE;
            if (!EvalRegex (prx->left, pszStr, uIndex, puNewIdx))
               return FALSE;
            uIndex = *puNewIdx;
            }

      case '@':
         while (TRUE)
            {
            if (EvalRegex (prx->next, pszStr, uIndex, puNewIdx))
               return TRUE;
            if (!EvalRegex (prx->left, pszStr, uIndex, puNewIdx))
               return FALSE;
            uIndex = *puNewIdx;
            }

      case '|':
         if (EvalRegex (prx->left,  pszStr, uIndex, puNewIdx))
            return EvalRegex (prx->next, pszStr, uIndex, puNewIdx);
         if (EvalRegex (prx->right, pszStr, uIndex, puNewIdx))
            return EvalRegex (prx->next, pszStr, uIndex, puNewIdx);
         return FALSE;

      default :
         AddError ("Unknown OP");
         return FALSE;
      }
   }


/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

void _dump (PRX prxHead, UINT uIndent)
   {
   UINT i;
   PRX  prx;

   for (prx = prxHead; prx; prx = prx->next)
      {
      for (i=0; i<uIndent; i++)
         printf (" ");

      if (prx->cType == 'O')
         printf ("Node: [%c] %s\n", prx->cType, prx->pszStr);
      else if (prx->cType == 'C')
         printf ("Node: [%c] %c\n", prx->cType, prx->cChar);
      else   
         printf ("Node: [%c]\n", prx->cType);

      _dump (prx->left,  uIndent + 3);
      _dump (prx->right, uIndent + 3);
      }
   }


void DumpRegex (PRX prx)
   {
   if (pszGLOBALERR)
      Error (pszGLOBALERR);

   _dump (prx, 0);
   }



/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

BOOL StrMatch (PSZ pszRegex, PSZ pszSrc)
   {
   PRX  prx;
   UINT uIndex;

   prx = ParseRegex (pszRegex);
   prx = AddTerminator (prx);


   printf ("-------------------------------------------------\n");
   DumpRegex (prx);
   printf ("-------------------------------------------------\n");


   if (pszGLOBALERR)
      return FALSE;

   if (!EvalRegex (prx, pszSrc, 0, &uIndex))
      return FALSE;

   return TRUE;
   }


BOOL StrFind (PSZ pszRegex, PSZ pszSrc, PUINT puStart, PUINT puEnd)
   {
//   PRX  prx;
//   UINT i, uLen, uMatchLen;
//
//   prx = ParseRegex (pszRegex);
//
//
//   if (pszGLOBALERR)
//      return FALSE;
//
//   uLen = strlen (pszSrc);
//   for (i=0; i< uLen; i++)
//      if (EvalRegex (prx, pszSrc, i, &uIndex))
//         {
//         *puStart = i;
//         *puEnd   = i+uMatchLen-1;
//         return TRUE;
//         }
   return FALSE;
   }


PSZ StrReplace (PSZ pszRegex, PSZ pszSrc, PSZ pszDest)
   {
   return NULL;
   }


/***************************************************************************/
/*                                                                         */
/* fns                                                                     */
/*                                                                         */
/***************************************************************************/

int main (int argc, char *argv[])
   {
   BOOL bRet;

   printf ("REGEX: %s\n", argv[1]);
   if (!(argv[1]))
      return printf ("Usage: REGEX \"expression\"  \"string\"");
   printf ("  SRC: %s\n", argv[2]);

   bRet = StrMatch (argv[1], argv[2]);

   if (pszGLOBALERR)
      printf (pszGLOBALERR);
   printf (bRet ? "TRUE" : "FALSE");
   return 0;
   }


