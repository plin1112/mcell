/*  Character string handling functions */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "strfunc.h"


/*************************************************************************
my_strdup:
  In: a string (\0 terminated array of characters)
  Out: a copy of the string, newly malloced, or NULL if out of memory       
  Note: the calling function is responsible for freeing the memory.
        This works just like strdup.  Why not use strdup?
*************************************************************************/
char *my_strdup(char *s)
{ 
  char *temp = NULL;
  
  if ((temp=(char *)malloc(strlen(s)+1))!=NULL) {
    strcpy(temp,s);
  }

  return(temp);
} 
  

/*************************************************************************
my_strcat:
  In: two strings
  Out: the two strings concatenated, in a newly malloced block of memory,
       or NULL if there isn't enough memory.
  Note: the calling function is responsible for freeing the memory.
*************************************************************************/
char *my_strcat(char *s1, char *s2)
{ 
  char *temp = NULL;
  size_t len1,len2;

  len1 = (s1==NULL) ? 0 : strlen(s1);
  len2 = (s2==NULL) ? 0 : strlen(s2);
  if ((temp=(char *)malloc(len1+len2+1))!=NULL) {
    if (len1) strcpy(temp,s1);
    if (len2) strcpy(temp+len1,s2);
    temp[len1+len2] = '\0';
  }
   
  return(temp);
}

/*************************************************************************
my_strclump:
  In: an array of strings, NULL-terminated
  Out: all of the strings concatenated, in a newly malloced block of
       memory, or NULL if there isn't enough memory.
  Note: the calling function is responsible for freeing the memory.
*************************************************************************/
char *my_strclump(char **slist)
{
  int i,j,n,len;
  char **sp = NULL;
  char *s = NULL;
  char *temp = NULL;
  
  for (sp=slist,n=0 ; *sp!=NULL ; sp++,n++);
  
  for (i=0,len=0;i<n;i++) len += strlen(slist[i]);
  
  temp = (char*) malloc(len+1);
  if (temp==NULL) return NULL;
  
  j=0;
  for (sp=slist;*sp!=NULL;sp++)
  {
    for (s=*sp ; *s!=0 ; s++)
    {
      temp[j++] = *s;
      if (j==len) { temp[j]=0; return temp; }
    }
  }
  
  temp[j]=0;
  return temp;
}

/*************************************************************************
strip_quotes:
  In: a string that must be at least two characters long
  Out: a copy of the string, newly malloced, missing the first and
       last characters (assumed to be quotes); NULL is returned if
       malloc fails.
  Note: this function does NOT do any error checking!
*************************************************************************/
char *strip_quotes(char *s)
{ 
  char *temp = NULL;
  int len = strlen(s);
  
  if ((temp=(char *)malloc(len-1))!=NULL) {
    strncpy(temp,s+1,len-2);
    temp[len-2]='\0';
  }

  return(temp);
} 

#ifndef DEBUG
/* no_printf looks like printf but does nothing when DEBUG is off */
/* With DEBUG on, no_printf is #defined to printf (in mcell_structs.h) */
void no_printf(const char *format, ...)
{
}
#endif

