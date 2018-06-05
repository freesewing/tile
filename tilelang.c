/*
#  tilelang - language extention for the tile.c freesewing program
#
# --------------------------------------------------------------
#  Tile is a fork of 'poster' by Jos T.J. van Eijndhoven
#  <J.T.J.v.Eijndhoven@ele.tue.nl>
#
#  Forked by Joost De Cock for freesewing.org
#  Language extention by Wouter van Wageningen
#
#  Copyright (C) 1999 Jos T.J. van Eijndhoven
#  Copyright (C) 2017 Joost De Cock
# --------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tilelang.h"

char *langPrompts[ LANG_PROMPTS_MAX ] ;
char *langTranslates[ LANG_PROMPTS_MAX ] ;
char *langBuffer = NULL;
char *langEmpty = "";
char fileName[15];
char *cPointer;

void ResetLangPrompts( void )
{
  for( int i = 0 ; i < LANG_PROMPTS_MAX ; i ++ )
    langPrompts[i] = langEmpty;
}

int SkipTo( char **cPointer, char c )
{
  while( *cPointer[0] && *cPointer[0] != c )
    *cPointer[0] ++;

  if( ! *cPointer[0] )
  {
    //Error in language file
    fprintf( stderr, "Error in language file %s\n", fileName );
    ResetLangPrompts();
    return( 0 );
  }
  return( 1 );
}

int LangRead( char *language )
{
  int fileSize, readSize;

  if( strlen( language ) != 2 )
    return(1);

  sprintf( fileName, "tile.%s.yml", language );

  FILE *fileHandler = fopen(fileName, "r");

  if( fileHandler )
  {
    // Seek the last byte of the file
    fseek( fileHandler, 0, SEEK_END );
    // Offset from the first to the last byte, or in other words, filesize
    fileSize = ftell( fileHandler );
    // go back to the start of the file
    rewind( fileHandler );

    // Allocate a string that can hold it all
    langBuffer = (char*) malloc( sizeof(char) * (fileSize + 1) );

    if( ! langBuffer )
    {
       // No memory, set the buffer to NULL
       fprintf( stderr, "Error allocating memory\n" );
       langBuffer = NULL;
    }

    // Read it all in one operation
    readSize = fread( langBuffer, sizeof(char), fileSize, fileHandler);

    if( fileSize != readSize)
    {
       // Something went wrong, throw away the memory and set
       // the buffer to NULL
       fprintf( stderr, "Error reading language file %s\n", fileName );
       free( langBuffer );
       langBuffer = NULL;
    }

    // Always remember to close the file.
    fclose( fileHandler );

    cPointer = langBuffer;

    for( int i = 0 ; i < LANG_PROMPTS_MAX ; i ++ )
    {
      if( ! SkipTo( &cPointer, '"') ) return(1);
      langPrompts[i] = ++ cPointer;

      if( ! SkipTo( &cPointer, '"') ) return(1);
      *cPointer ++ = '\0';

      if( ! SkipTo( &cPointer, ':') ) return(1);

      if( ! SkipTo( &cPointer, '"') ) return(1);
      langTranslates[i] = ++ cPointer;

      if( ! SkipTo( &cPointer, '"') ) return(1);
      *cPointer ++ = '\0';
    }
  }
  else
  {
    return( 1 );
  }
  return( 0 ) ;
}

void LangClose( void )
{
  if( langBuffer )
    free( langBuffer );
  langBuffer = NULL;
}

char *LangPrompt( char *defPrompt )
{
  if( langBuffer )
  {
    for( int i; i < LANG_PROMPTS_MAX ; i ++ )
    {
      if( strcmp( langPrompts[ i ], defPrompt ) == 0 )
        return( langTranslates[ i ] );
    }
  }
  return( defPrompt );
}
