/*
 *
 * regex.c
 * Saturday, 7/13/1996.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GnuType.h>
#include <GnuFile.h>
#include <GnuMisc.h>


PSZ pszGLOBALERR;
PSZ  pszTOKENSTR;
UINT uTOKENIDX;


typedef struct _rx
   {
   CHAR       cType;
   UINT       uCount;
   struct _rx *left;
   struct _rx *right;
   struct _rx *next;
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
CHAR GetToken (BOOL bEat)
   {
   CHAR cTok;

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



CHAR GetTokenVal (BOOL bEat)
   {
   CHAR cTok;

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
 *
 */
void EatToken (CHAR cTok, PSZ pszErr)
   {
   if (cTok != GetToken (1))
      AddError (pszErr);
   }



/*
 * builds a string of consecutive literals
 *
 */
PSZ GetTokenString1 (void)
   {
   CHAR szList[512];
   UINT uSize = 0;

   while (GetToken (0) == 'C' || GetToken (0) == '-')
      szList[uSize++] = GetTokenVal (1);
   szList[uSize] = '\0';
   return strdup (szList);
   }


/*
 * builds a string of consecutive literals
 * expands '-'s to be a range indicator
 */
PSZ GetTokenString2 (void)
   {
   CHAR cTok, cLast, cEnd, szList[512];
   UINT uSize = 0;

   if ((cTok = GetToken (0)) != 'C')
      return AddError ("Invalid Range");
   szList[uSize++] = cLast = GetTokenVal (1);

   while (TRUE)
      {
      cTok = GetToken (0);
      if (cTok == 'C')
         szList[uSize++] = cLast = GetTokenVal (1);
      else if (cTok == '-')
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


PRX NewOp (CHAR cType)
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
   CHAR cTok;
   BOOL bInvert = FALSE;
   PSZ  psz;

   EatToken ('[', NULL);
   cTok = GetToken (0);
   if (cTok == '~')
      {
      EatToken ('~', NULL);
      bInvert = TRUE;
      }
   psz = GetTokenString2 ();
   EatToken (']', "Invalid [] closure");
   return psz;
   }





/*
 * R4 = '<' | '>' | '?' | '*'   |
 *      '[' ['~'] CHARLIST1 ']' |
 *      '{' R1 '}'              |
 *      CHARLIST2
 *
 * CHARLIST1 = CHAR1 { [ '-' CHAR1 ] | CHAR1}
 * CHARLIST2 = CHAR2 { CHAR2 }
 * CHAR1     = any char except {}[]<>?*|-  or  '\' ANYCHAR
 * CHAR2     = any char except {}[]<>?*|   or  '\' ANYCHAR
 * ANYCHAR   = all characters
 */
PRX ParseR4 (void)
   {
   CHAR cTok;
   PRX  prx;

   cTok = GetToken(0);

   if (strchr ("<>?*", cTok))
      return NewOp (GetToken(1));
   if (cTok == 'C')
      {
      prx = NewOp ('A'); // 'and' string
      prx->pszStr = GetTokenString1 ();
      return prx;
      }
   if (cTok == '{')
      {
      EatToken ('{', NULL);
      prx = ParseR1 ();
      EatToken ('}', "Invalid {} closure");
      return prx;
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
   CHAR cTok;

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
   CHAR cTok;

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


void Fixup (PRX prxHead, PRX prxNext)
   {
   PRX prx, prx2;

   for (prx = prxHead; prx; prx = prx2)
      {
      prx2 = prx->next;
      if (!prx->next)
         prx->next = prxNext;
      if (prx->cType == '+' || prx->cType == '@')
         prxNext = prx;
      Fixup (prx->left, prx);
      Fixup (prx->right, NULL);
      }
   }

/*
 *
 */
PRX ParseRegex (PSZ pszRegex)
   {
   PRX prx;

   InitTokens (pszRegex);
   prx = ParseR1 ();
   Fixup (prx, NULL);
   return prx;
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

   for (i=0; i<uIndent; i++)
      printf (" ");
   if (!prxHead)
      {
      printf ("*Null*\n");
      return;
      }
   for (prx = prxHead; prx; prx = prx->next)
      {
      if (prx->cType == 'A' || prx->cType == 'O')
         printf ("Node: [%c] %s\n", prx->cType, prx->pszStr);
      else   
         printf ("Node: [%c]\n", prx->cType);

      if (!prx->left && !prx->right)
         return; // a leaf node

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
//
//BOOL EvalNode (PRX prx, PSZ pszString, UINT uIndex, PUINT puMatchLen)
//   {
//   UINT i, uLeftLen, uRightLen;
//
//   *puMatchLen = 0;
//
//   switch (prx->cType)
//      {
//      case 'A':
//         *puMatchLen = strlen (prx->pszStr);
//         return (!strncmp (prx->pszStr, pszString+uIndex, *puMatchLen));
//
//      case 'O':
//         *puMatchLen = 1;
//         return !!strchr (prx->pszStr, pszString[uIndex]);
//
//      case '<':
//         return (!uIndex || pszString[uIndex-1] == '\n');
//
//      case '>':
//         return (!pszString[uIndex] || pszString[uIndex] == '\n');
//
//      case '*':
//         for (i=0; pszString[uIndex+i]; i++)
//            if (EvalNode (prx->left, pszString, uIndex+i, puMatchLen))
//               {
//               *puMatchLen += i;
//               return TRUE;
//               }
//         return FALSE;
//
//      case '?':
//         *puMatchLen = 1;
//         return !!pszString[uIndex];
//
//      case '+':
//         if (!EvalNode (prx->left, pszString, uIndex, puMatchLen))
//            return FALSE;
//         /*--- FALL THROUGH ---*/
//
//      case '@':
//         while (TRUE)
//            {
//            if (!EvalNode (prx->left, pszString, uIndex+*puMatchLen, &i))
//               break;
//            *puMatchLen += i;
//            if (!i)
//               break;   // <@ for example could cause infinite loop
//            }
//         return TRUE;
//
//      case '&':
//         if (!EvalNode (prx->left,  pszString, uIndex, &uLeftLen))
//            return FALSE;
//         if (!EvalNode (prx->right, pszString, uIndex+uLeftLen, &uRightLen))
//            return FALSE;
//         *puMatchLen = uLeftLen + uRightLen;
//         return TRUE;
//
//      case '|':
//         if (EvalNode (prx->left,  pszString, uIndex, puMatchLen))
//            return TRUE;
//         if (EvalNode (prx->right, pszString, uIndex, puMatchLen))
//            return TRUE;
//         return TRUE;
//
//      case 0:
//         return TRUE;
//
//      default :
//         AddError ("Unknown OP");
//         return FALSE;
//      }
//   }
//



BOOL EvalNode (PRX prx, PSZ pszStr, UINT uIndex)
   {
   UINT i, uLen;

   if (!prx)
      return TRUE;

   switch (prx->cType)
      {
      case 'A':
         uLen = strlen (prx->pszStr);
         if (strncmp (prx->pszStr, pszStr+uIndex, uLen))
            return FALSE;
         return EvalNode (prx->next, pszStr, uIndex+uLen);

      case 'O':
         if (!strchr (prx->pszStr, pszStr[uIndex]))
            return FALSE;
         return EvalNode (prx->next, pszStr, uIndex+1);

      case '<':
         if (uIndex && pszStr[uIndex-1] != '\n')
            return FALSE;
         return EvalNode (prx->next, pszStr, uIndex);

      case '>':
         if (pszStr[uIndex] && pszStr[uIndex] != '\n')
            return FALSE;
         return EvalNode (prx->next, pszStr, uIndex);

      case '*':
         for (i=0; pszStr[uIndex+i]; i++)
            if (EvalNode (prx->next, pszStr, uIndex+i))
               return TRUE;
         return EvalNode (prx->next, pszStr, uIndex+i);

      case '?':
         if (!pszStr[uIndex])
            return FALSE;
         return EvalNode (prx->next, pszStr, uIndex+1);

      case '+':
         prx->uCount++;
         if (prx->uCount < 2)
            return EvalNode (prx->left, pszStr, uIndex);
         /*--- fall through ---*/

      case '@':
         if (EvalNode (prx->next, pszStr, uIndex))
            return TRUE;
         return EvalNode (prx->left, pszStr, uIndex);

      case '|':
         if (EvalNode (prx->left,  pszStr, uIndex))
            return EvalNode (prx->next,  pszStr, uIndex);
         if (EvalNode (prx->right, pszStr, uIndex))
            return EvalNode (prx->next,  pszStr, uIndex);;
         return FALSE;

      default :
         AddError ("Unknown OP");
         return FALSE;
      }
   }





BOOL EvalRegex (PRX prxHead, PSZ pszString)
   {
   PRX  prx;

   for (prx = prxHead; prx; prx = prx->next)
      if (!EvalNode (prx, pszString, 0))
         return FALSE;
   return TRUE;
   }


/***************************************************************************/
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

BOOL StrMatch (PSZ pszRegex, PSZ pszSrc)
   {
   PRX prx;

   prx = ParseRegex (pszRegex);
   prx = AddTerminator (prx);

   if (pszGLOBALERR)
      return FALSE;

   if (!EvalNode (prx, pszSrc, 0))
      return FALSE;

   return TRUE;
   }


BOOL StrFind (PSZ pszRegex, PSZ pszSrc, PUINT puStart, PUINT puEnd)
   {
   PRX  prx;
   UINT i, uLen, uMatchLen;

   prx = ParseRegex (pszRegex);

   if (pszGLOBALERR)
      return FALSE;

   uLen = strlen (pszSrc);
   for (i=0; i< uLen; i++)
      if (EvalNode (prx, pszSrc, i))
         {
         *puStart = i;
         *puEnd   = i+uMatchLen-1;
         return TRUE;
         }
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
//   FILE *fp;
//   CHAR szLine[256];
//   PRX  prx;
//   if (!(fp = fopen (argv[1], "rt")))
//      Error ("Unable to open input file %s", argv[1]);
//
//   FilReadLine (fp, szLine, ";", sizeof szLine);
//   fclose (fp);
//
//   printf ("Regex: %s\n", szLine);
//   prx = ParseRegex (szLine);
//   DumpRegex (prx);


int main (int argc, char *argv[])
   {
   BOOL bRet;

   printf ("REGEX: %s\n", argv[1]);
   printf ("  SRC: %s\n", argv[2]);

   bRet = StrMatch (argv[1], argv[2]);

   if (pszGLOBALERR)
      printf (pszGLOBALERR);
   printf (bRet ? "TRUE" : "FALSE");
   return 0;
   }

