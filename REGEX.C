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


PSZ  pszGLOBALERR;
PSZ  pszTOKENSTR;
UINT uTOKENIDX;


typedef struct _rx
   {
   UCHAR      cType;
   UINT       uCount;
   UINT       uIndex;
   struct _rx *left;
   struct _rx *right;
   struct _rx *next;
   struct _rx *back;
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


void PointTo (PRX prxChain, PRX prxTo)
   {
//   PRX prx;
//
//   /*--- add (last child to parent ptr)fixup to make continuous path ---*/
//   for (prx = prxChain; prx && prx->next; prx = prx->next)
//      ;
//   if (prx) 
//      prx->next = prxTo;
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
      prx = NewOp ('S');
      EatToken ('{', NULL);
      prx2 = ParseR1 ();
      EatToken ('}', "Invalid {} closure");
      prx->left = prx2;
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
   UCHAR cTok;

   prx1 = ParseR4 ();
   cTok = GetToken(0);
   if (cTok == '+' || cTok == '@')
      {
      prx2 = NewOp (GetToken (1));
      prx2->left  = prx1;
      PointTo (prx2->left, prx2);
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
      PointTo (prx2->left, prx2);
      PointTo (prx2->right, prx2);
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


void Fixup (PRX prxHead, PRX prxBack)
   {
   PRX prx;

   for (prx = prxHead; prx; prx = prx->next)
      {
      if (!prx->next)
         prx->back = prxBack;

      if (prx->cType == '|')
         prxBack = (prxBack->next ? prxBack->next : NULL);
         
      Fixup (prx->left,  prxBack);
      Fixup (prx->right, prxBack);
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
//void pn (PRX prx)
//   {
//   printf (" %c->left=%c, uCount=%d, uIndex=%d\n",
//           prx->cType, prx->left->cType, prx->uCount, prx->uIndex);
//   }
//

//BOOL EvalRegex (PRX prx, PSZ pszStr, UINT uIndex)
//   {
//   UINT i;
//   BOOL bRet;
//
//   if (!prx)
//      return TRUE;
//
//   switch (prx->cType)
//      {
//      case 'C':
//         if (pszStr[uIndex] != prx->cChar)
//            return FALSE;
//         return EvalRegex (prx->next, pszStr, uIndex+1);
//
//      case 'O':
//         if (!strchr (prx->pszStr, pszStr[uIndex]))
//            return FALSE;
//         return EvalRegex (prx->next, pszStr, uIndex+1);
//
//      case '<':
//         if (uIndex && pszStr[uIndex-1] != '\n')
//            return FALSE;
//         return EvalRegex (prx->next, pszStr, uIndex);
//
//      case '>':
//         if (pszStr[uIndex] && pszStr[uIndex] != '\n')
//            return FALSE;
//         return EvalRegex (prx->next, pszStr, uIndex);
//
//      case '*':
//         for (i=0; pszStr[uIndex+i]; i++)
//            if (EvalRegex (prx->next, pszStr, uIndex+i))
//               return TRUE;
//         return EvalRegex (prx->next, pszStr, uIndex+i);
//
//      case '?':
//         if (!pszStr[uIndex])
//            return FALSE;
//         return EvalRegex (prx->next, pszStr, uIndex+1);
//
//      case '+':
//         prx->uCount++;
//         if (prx->uCount == 1)
//            {
//            prx->uIndex = uIndex;
//            bRet = EvalRegex (prx->left, pszStr, uIndex);
//            prx->uCount--;
//            return bRet;
//            }
//
//         if (EvalRegex (prx->next, pszStr, uIndex))
//            return TRUE;
//
//         if (prx->uIndex == uIndex)
//            {
//            prx->uCount--;
//            return FALSE;
//            }
//         prx->uIndex = uIndex;
//
//         bRet = EvalRegex (prx->left, pszStr, uIndex);
//         prx->uCount--;
//         return bRet;
//
//      case '@':
//         prx->uCount++;
//         if (prx->uCount == 1)
//            prx->uIndex = uIndex;
//
//         if (EvalRegex (prx->next, pszStr, uIndex))
//            return TRUE;
//
//         if (prx->uCount>1 && prx->uIndex == uIndex) // already tried left branch
//            {
//            prx->uCount--;
//            return FALSE;
//            }
//         prx->uIndex = uIndex;
//
//         bRet = EvalRegex (prx->left, pszStr, uIndex);
//         prx->uCount--;
//         return bRet;
//
//      case '|':
//         if (EvalRegex (prx->left,  pszStr, uIndex))
//            return TRUE;
//         if (EvalRegex (prx->right, pszStr, uIndex))
//            return TRUE;
//         return FALSE;
//
//      default :
//         AddError ("Unknown OP");
//         return FALSE;
//      }
//   }

BOOL EvalRegex (PRX prx, PSZ pszStr, UINT uIndex)
   {
   UINT i;
   BOOL bRet;
   PRX  prxNext;

   if (!prx)
      return TRUE;

   prxNext = (prx->next ? prx->next : prx->back);

   switch (prx->cType)
      {
      case 'C':
         if (pszStr[uIndex] != prx->cChar)
            return FALSE;
         return EvalRegex (prxNext, pszStr, uIndex+1);

      case 'O':
         if (!strchr (prx->pszStr, pszStr[uIndex]))
            return FALSE;
         return EvalRegex (prxNext, pszStr, uIndex+1);

      case '<':
         if (uIndex && pszStr[uIndex-1] != '\n')
            return FALSE;
         return EvalRegex (prxNext, pszStr, uIndex);

      case '>':
         if (pszStr[uIndex] && pszStr[uIndex] != '\n')
            return FALSE;
         return EvalRegex (prxNext, pszStr, uIndex);

      case '*':
         for (i=0; pszStr[uIndex+i]; i++)
            if (EvalRegex (prxNext, pszStr, uIndex+i))
               return TRUE;
         return EvalRegex (prxNext, pszStr, uIndex+i);

      case '?':
         if (!pszStr[uIndex])
            return FALSE;
         return EvalRegex (prxNext, pszStr, uIndex+1);

//      case 'S': // scope
//         prx->uStart = uIndex;
//         bRet = EvalRegex (prxNext, pszStr, uIndex+1);
//         prx->uStart = uIndex;
//   
//


      case '+':
         prx->uCount++;
         if (prx->uCount == 1)
            {
            prx->uIndex = uIndex;
            bRet = EvalRegex (prx->left, pszStr, uIndex);
            prx->uCount--;
            return bRet;
            }

         if (EvalRegex (prxNext, pszStr, uIndex))
            return TRUE;

         if (prx->uIndex == uIndex)
            {
            prx->uCount--;
            return FALSE;
            }
         prx->uIndex = uIndex;

         bRet = EvalRegex (prx->left, pszStr, uIndex);
         prx->uCount--;
         return bRet;

      case '@':
         prx->uCount++;
         if (prx->uCount == 1)
            prx->uIndex = uIndex;

         if (EvalRegex (prxNext, pszStr, uIndex))
            return TRUE;

         if (prx->uCount>1 && prx->uIndex == uIndex) // already tried left branch
            {
            prx->uCount--;
            return FALSE;
            }
         prx->uIndex = uIndex;

         bRet = EvalRegex (prx->left, pszStr, uIndex);
         prx->uCount--;
         return bRet;

      case '|':
         if (EvalRegex (prx->left,  pszStr, uIndex))
            return TRUE;
         if (EvalRegex (prx->right, pszStr, uIndex))
            return TRUE;
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
   PRX prx;

   prx = ParseRegex (pszRegex);
   prx = AddTerminator (prx);


   printf ("-------------------------------------------------\n");
   DumpRegex (prx);
   printf ("-------------------------------------------------\n");


   if (pszGLOBALERR)
      return FALSE;

   if (!EvalRegex (prx, pszSrc, 0))
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
      if (EvalRegex (prx, pszSrc, i))
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

