/* API for a simple text editor *
 * personal project by Eshan Bajwa (eshanbajwa@gmail.com) *
 * heavily based on 'kilo' and the associated snaptoken tutorial by Salatore Sanfilippo (antirez@gmail.com) */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "editor.h"

/* insert a character into a line at the specified position, if valid */
/* returns 1 if successful, else 0 */
bool insertChar(erow *row, int pos, char c){
  if(pos < 0 || pos > row->len) return false; // invalid location

  row->text = realloc(row->text, row->len+1);
  memmove(row->text + pos + 1, row->text + pos, row->len - pos);
  row->text[pos] = c;
  row->len ++;
  return true;
}

/* delete a  character from a line at the specified position, if valid */
/* returns true if successful, else false */
bool deleteChar(erow *row, int pos){
  if(pos < 0 || pos >= row->len) return false; // invalid location

  memmove(row->text+pos, row->text + pos + 1, row->len - pos);
  row->len --;
  return true;
}

/* insert a new row into the editor at rownum */
void newRow(editor *E, int rownum){
  if (rownum >= E->numrows){
    /* trying to insert past the length of editor, 
     * realloc and insert empty rows until rownum */
    E->rowarray = realloc(E->rowarray, (rownum + 1)*sizeof(erow));
    for (int i = E->numrows; i <= rownum; i++){
      E->rowarray[i].len = 0;
      E->rowarray[i].text = strdup("");
    }
    E->numrows = rownum + 1;
  }
  else{
    /* insert one new row in the middle of the editor's rows */
    E->rowarray = realloc(E->rowarray, (E->numrows +1)*sizeof(erow));
    int rowshift = E->numrows - rownum;
    memmove(E->rowarray + rownum + 1, E->rowarray + rownum,sizeof(erow)*rowshift);
    E->rowarray[rownum].len = 1;
    E->rowarray[rownum].text = strdup("");
    E->numrows ++;
  }

  E->dirty = true;
}

/* deletes the specified row from the editor, shifting rows below it up by one */
void deleteRow(editor *E, int rownum){
  //if (rownum < 0 || rownum >= E->numrows) return; // invalid row

  int num_moved = E->numrows - rownum + 1;
  erow *deleted_row = E->rowarray+rownum;

  free(deleted_row->text);
  memmove(deleted_row, deleted_row + 1, sizeof(erow)*num_moved);
  E->numrows --;
}

/* truncates a row and inserts the truncated portion into a new row below */
void splitRow(editor *E, int rownum, int pos){
  if (rownum < 0 || rownum > E->numrows) return; // invalid row
  if (pos < 0 || pos > E->rowarray[rownum].len) return; // invalid pos

  newRow(E, rownum + 1);
  erow *currrow = E->rowarray + rownum;
  erow *newrow = E->rowarray + rownum + 1;
  
  newrow->len = currrow->len - pos;
  newrow->text = strdup(currrow->text + pos);
  currrow->len = pos+1;
  currrow->text[pos] = '\0';
  currrow->text = realloc(currrow->text, currrow->len);
}

/* concatenates a row with the row above it, deleting the original row */
void delCatRow(editor *E, int rownum){
  if (rownum <= 0 || rownum >= E->numrows) return; // invalid row

  erow *top_row = E->rowarray + rownum - 1;
  erow *bot_row = E->rowarray + rownum;

  top_row->text = realloc(top_row->text, top_row->len + bot_row->len);
  top_row->len = top_row->len + bot_row->len - 1;
  strcat(top_row->text, bot_row->text);
  deleteRow(E, rownum);
}


editor* editorFromFile(char* filename){
  int c;
  editor* E = calloc(sizeof(editor), 1);
  E->mode = VIEW;
  E->filename = strdup(filename);

  E->command = calloc(sizeof(commandrow), 1);
  E->command->cmd = calloc(sizeof(erow), 1);
  E->command->cmd->text = strdup("");
  insertChar(E->command->cmd, 0, '\0');

  FILE* fp = fopen(filename, "r");
  
  newRow(E, E->numrows);
  while((c = fgetc(fp)) != EOF){
    erow* curr_row = E->rowarray + E->numrows - 1;
    if (c == '\n'){
      insertChar(curr_row, curr_row->len, '\0');
      newRow(E, E->numrows);
    }
    else{
      insertChar(curr_row, curr_row->len, c);
    }
  }
  if(E->numrows > 1 && E->rowarray[E->numrows-1].len == 0){
    deleteRow(E, E->numrows-1);
  }
  fclose(fp);
  return E;
}

void saveToFile(editor* e){
  FILE* fp = fopen(e->filename, "w");
  if (!fp){
    return;
  }

  for(int i = 0; i < e->numrows; i++){
    fprintf(fp, "%s\n", e->rowarray[i].text);
  }
}
