/* API for a simple text editor *
 * by Eshan Bajwa (eshanbajwa@gmail.com) *
 * heavily based on 'kilo' by Salatore Sanfilippo (antirez@gmail.com) *
 * and the associated snaptoken tutorial by Paige Ruten (paige.ruten@gmail.com) */
#pragma once

typedef enum Mode{
  VIEW,
  INSERT,
  COMMAND,
  QUIT
} Mode;

typedef struct erow {
  int len; // length of the text, excluding null term
  char *text; // string content
} erow;

typedef struct commandrow {
  erow* cmd; // command string (and lenght)
  int mcol; // cursor position
} commandrow;

typedef struct editor{
  /* display info */
  int mr,mc; // cursor position
  int toprow; // row currently at the top of the display
  char status[80]; // status message
  Mode mode;

  /* content info */
  int numrows;
  erow *rowarray;
  bool dirty; // file modified?
  char *filename; // name of currently opened file

  commandrow* command; // current user command
} editor;

bool insertChar(erow *row, int pos, char c);
bool deleteChar(erow *row, int pos);

void newRow(editor *E, int rownum);
void deleteRow(editor *E, int rownum);

void splitRow(editor *E, int rownum, int pos);
void delCatRow(editor *E, int rownum);

editor *editorFromFile(char *filename);
void saveToFile(editor *e);