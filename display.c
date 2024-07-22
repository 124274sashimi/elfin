#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"

#define szstr(str) str,sizeof(str)
#define ANSI_BUFSIZE 32

extern struct editorInterface* I;

int min(int a, int b) { return((a < b) ? (a) : (b)); }
int max(int a, int b) { return((a > b) ? (a) : (b)); }

/* ======= POINT UTILS ======= */
bool pointEqual(point p1, point p2){
  return (p1.r == p2.r && p1.c == p2.c);
}

bool pointGreater(point p1, point p2){
  if (p1.r > p2.r) { return true; }
  else if (p1.r < p2.r) { return false; }
  else { return (p1.c > p2.c); }
}

bool pointLess(point p1, point p2){
  if (p1.r < p2.r) { return true; }
  else if (p1.r > p2.r) { return false; }
  else {return (p1.c < p2.c); }
}

point maxPoint(point p1, point p2){ 
  if (pointGreater(p1, p2)) { return p1; }
  else { return p2; }
}

point minPoint(point p1, point p2){
  if (pointLess(p1, p2)) { return p1; }
  else { return p2; } 
}

/* ======= ESC SEQUENCE UTILS ======= */
void abAppend(struct abuf* ab, char *s, int len){
  char* new = realloc(ab->buf, ab->size + len);
  if (new == NULL) return; // :(
  memcpy(new+ab->size, s, len);
  ab->buf = new;
  ab->size += len;
}

// append the MOVE esc sequence to a string
void move(struct abuf* ab, int r, int c){
  char buf[ANSI_BUFSIZE];
  snprintf(buf, ANSI_BUFSIZE, "\x1b[%d;%dH", r, c);
  abAppend(ab, buf, strnlen(buf, ANSI_BUFSIZE));
}

void abFree(struct abuf* ab){
  free(ab->buf);
  free(ab);
}

/* ======= COMMAND STUFF ======= */
point search(point start, char* needle){
  start.c ++;
  // search from start
  for(int i = start.r; i < I->E->numrows; i++){
    struct erow* curr_row = I->E->rowarray[i];
    int start_c = i == start.r ? start.c : 0;
    char* loc = strnstr(curr_row->text + start_c, needle, curr_row->len - start_c);
    if (loc){
      point ret = {i, loc - curr_row->text};
      return ret;
    }
  }
  // search from beginning
  for(int i = 0; i < I->E->numrows; i++){
    struct erow* curr_row = I->E->rowarray[i];
    char* loc = strnstr(curr_row->text, needle, curr_row->len);
    if (loc){
      point ret = {i, loc - curr_row->text};
      return ret;
    }
  }
  start.c --; 
  return start;
}

/* ======= DISPLAY UTILS ======= */
point getBoundedCursor(){
  point out = I->cursor;
  out.c = min(out.c, I->E->rowarray[out.r]->len);
  return out;
}

point getLargestDisplayedPoint(){ // TODO technically doesn't work because of wide chars
  int maxr = I->ws.ws_row;
  int maxc = I->ws.ws_col - I->coloff - 1;
  struct editor* E = I->E;

  int visual_r = 1;

  int r;
  int len;
  for(r = I->toprow; r < E->numrows && visual_r < maxr; r++){
    len = E->rowarray[r]->len;
    visual_r += len/maxc + (len % maxc != 0) + (len == 0);
  }
  int extra = max(visual_r - maxr, 0);
  if (extra != 0){
    len -= len % maxc - (extra - 1)*maxc;
  }
  point out = {r-1, len};
  return out;
}

void adjustToprow(){
  if(I->cursor.r < I->toprow){
    I->toprow = I->cursor.r;
    return;
  }

  while(pointGreater(getBoundedCursor(), getLargestDisplayedPoint())) I->toprow++;
}

/* ======= DISPLAY ======= */
void clearScreen(){
  write(STDIN_FILENO, szstr("\x1b[2J"));
}

void printEditorContents(){
	int maxr = I->ws.ws_row; // height
	int maxc = I->ws.ws_col - I->coloff - 1; // width

  struct editor* E = I->E;
  int visual_r = 1;

  point save_cursor;
  save_cursor.r = -1; // default not found
  
  struct abuf ab = {NULL, 0}; // buffer for the WHOLE screen
  abAppend(&ab,szstr("\x1b[?25l")); // hide cursor
  // ITER OVER THE ROWS
  for(int r = I->toprow; visual_r < maxr && r < E->numrows; r++){
    struct erow* curr_row = E->rowarray[r];
    int visual_c = -1; // same as displayed col when modded by maxc and adjusted by coloff

    /* LINENUM DISPLAY */
    move(&ab, visual_r, 0);
    char linenum[I->coloff];
    sprintf(linenum, "%*d ", I->coloff-1, r+1);
    abAppend(&ab, szstr("\x1b[38;5;" LINENUM_FG "m")); // set linenum color
    abAppend(&ab, linenum, I->coloff);
    abAppend(&ab, szstr("\x1b[m")); // reset color

    if (curr_row->len == 0 && I->cursor.r == r){ // cursor on empty line
      save_cursor.r = visual_r;
      save_cursor.c = I->coloff + 1;
    }
    // ITER OVER EACH CHAR (0-indexed)
    for(int c = 0; c < curr_row->len && visual_r < maxr; c++){
      char* to_add = curr_row->text + c;
      int cwidth = 1;
      if (*to_add == '\t') { // TODO abstract
        cwidth = 2;
      }
      visual_c += cwidth;
      
      /* SUBLINE HANDLING */
      if(c != 0 && visual_c % maxc < cwidth){ // new subline?
        visual_c += cwidth-1 - (visual_c % maxc); // (wide only) how many 'slots' skipped?
        if (++visual_r >= maxr) break;
        abAppend(&ab,szstr("\x1b[0K")); // erase to end of line (needed for resize)
        move(&ab, visual_r, I->coloff + 1); // move to upcoming subline
        abAppend(&ab,szstr("\x1b[1K")); // erase to start of line
      }

      /* CURSOR FINDING LOGIC */
      if(save_cursor.r == -1 && I->cursor.r == r){
        if (c == I->cursor.c){ 
          save_cursor.r = visual_r;
          save_cursor.c = (visual_c-cwidth+1) % maxc + I->coloff + 1;
        } else if (c == curr_row->len - 1){
          save_cursor.r = visual_r;
          save_cursor.c = (visual_c+1) % maxc + I->coloff + 1;
        }
      }

      /* WRITING CHARACTER */
      // TODO abstract character substitution
      if (*to_add == '\t') { // replace tabs with double spaces
        abAppend(&ab, "  ", 2);
      } else {
        abAppend(&ab, to_add, 1);
      }
    }
    // prepare to start a new row
    abAppend(&ab,szstr("\x1b[0K")); // erase to end of line
    visual_r++;
  }
  // draw blank lines past the last row
  for(int r = visual_r; r < maxr; r++){
    move(&ab, r, 0);
    abAppend(&ab,szstr("\x1b[0K")); // erase to end of line
    abAppend(&ab, szstr("~"));
  }
  // move the cursor to display position
  if (I->mode == COMMAND){
    move(&ab, maxr, I->cmd.mcol+1);
  } else{
    move(&ab, save_cursor.r, save_cursor.c);
  }
  abAppend(&ab,szstr("\x1b[?25h")); // show cursor
  
  write(STDIN_FILENO, ab.buf, ab.size);
  free(ab.buf);
}

void statusPrintMode(){ // TODO rename this lol
  if(I->mode == COMMAND){
    abAppend(&I->status, I->cmd.msg.text, I->cmd.msg.len);
    return;
  }
  switch(I->mode){ // MODE
    case VIEW:
      abAppend(&I->status, szstr("VIEW — "));
      break;
    case INSERT:  
      abAppend(&I->status, szstr("INSERT — "));
      break;
    default:
      break;
  }
  // filename
  abAppend(&I->status, I->filename, strlen(I->filename));
  // number of lines, number of bytes
  char* buf;
  int len;
  asprintf(&buf, " %dL", I->E->numrows);
  len = strlen(buf);
  abAppend(&I->status, buf, len);
  abAppend(&I->status, szstr("\x1b[0K")); // erase to end of line
  // cursor coordinates
  asprintf(&buf, "%d, %d", I->cursor.r+1, I->cursor.c+1); // asprintf my beloved
  len = strlen(buf);
  move(&I->status, I->ws.ws_row, I->ws.ws_col - len);
  abAppend(&I->status, buf, len);
  free(buf);
}

void printEditorStatus(){
  struct abuf ab = {NULL, 0};
  abAppend(&ab,szstr("\x1b[?25l")); // hide cursor
  move(&ab, I->ws.ws_row, 0);
  abAppend(&ab, I->status.buf, I->status.size); // write the status
  abAppend(&ab, szstr("\x1b[0K")); // erase to end of line
  write(STDIN_FILENO, ab.buf, ab.size);
  free(ab.buf);
}

void resize(int _ __attribute__((unused))){
  ioctl(1, TIOCGWINSZ, &I->ws);
  point max = {I->ws.ws_row, I->ws.ws_col};
  point min = {0, 0};
  I->cursor = maxPoint(minPoint(I->cursor, max), min);
  I->status.size = 0;
  statusPrintMode();
  adjustToprow();
  printEditorContents();
  printEditorStatus();
}