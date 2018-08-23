/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Switch to gets() if $TERM is something we can't support.
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - Completion?
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * Changes by Ketmar // Vampire Avalon:
 *    [*] reformatted
 *    [+] ignore unknown control codes
 *    [+] understands DEL, HOME, END in my mrxvt
 *    [+] Alt+w, Alt+BS: delete previous s-expression
 *    [+] ^w: delete previous word (shitty for now)
 *    [+] autocomplete hook
 *    [+] save/load history
 *    [+] LISP shit
 *    [+] yanking
 *    [+] many more shit
 */
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linenoise.h"


////////////////////////////////////////////////////////////////////////////////
static char *idioticTerms[] = {"dumb", "cons25", NULL};


////////////////////////////////////////////////////////////////////////////////
static struct termios origTIOS; /* in order to restore at exit */
static int rawmode = 0; /* for atexit() function to check if restore is needed*/
static int atexitInited = 0; /* register atexit just 1 time */
static int historyMaxLen = 256;
static int historyLen = 0;
linenoiseACHookFn linenoiseACHook = NULL;
int linenoiseOptHilightBrackets = 1;
const char *linenoiseBrcHiStr = "\x1b[7m";
char **history = NULL;


////////////////////////////////////////////////////////////////////////////////
static char yankBuf[LINENOISE_MAX_LINE_LEN] = {0};
typedef struct {
  int levelP;
  int levelB;
} BrcInfo;
static BrcInfo brcBuf[LINENOISE_MAX_LINE_LEN];


////////////////////////////////////////////////////////////////////////////////
static void linenoiseAtExit (void);
static int linenoiseHistoryAddInternal (const char *line, int noProcessing);


////////////////////////////////////////////////////////////////////////////////
static int isUnsupportedTerm (void) {
  const char *term = getenv("TERM");
  //
  if (term == NULL) return 0;
  for (int f = 0; idioticTerms[f]; ++f) if (!strcasecmp(term, idioticTerms[f])) return 1;
  return 0;
}


static int enableRawMode (int fd) {
  struct termios raw;
  //
  if (!isatty(STDIN_FILENO)) goto fatal;
  if (!atexitInited) {
    atexit(linenoiseAtExit);
    atexitInited = 1;
  }
  if (tcgetattr(fd, &origTIOS) == -1) goto fatal;
  raw = origTIOS; /* modify the original mode */
  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes - disable post processing */
  raw.c_oflag &= ~(OPOST);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - choing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer;
   * we want read to return every single byte, without timeout */
  raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */
  /* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;
  rawmode = 1;
  return 0;
fatal:
  errno = ENOTTY;
  return -1;
}


static void disableRawMode (int fd) {
  /* don't even check the return value as it's too late */
  if (rawmode && tcsetattr(fd, TCSAFLUSH, &origTIOS) != -1) rawmode = 0;
}


////////////////////////////////////////////////////////////////////////////////
/* at exit we'll try to fix the terminal to the initial conditions */
static void linenoiseAtExit (void) {
  disableRawMode(STDIN_FILENO);
  linenoiseClearHistory();
}


static int getColumns (void) {
  struct winsize ws;
  //
  if (ioctl(1, TIOCGWINSZ, &ws) == -1) return 80;
  return ws.ws_col;
}


////////////////////////////////////////////////////////////////////////////////
static void buildBrcBuf (const char *buf) {
  int levelP = 0, levelB = 0, inStr = 0, ignore = 0;
  //
  for (int f = 0; buf[f]; ++f) {
    brcBuf[f].levelP = 0;
    brcBuf[f].levelB = 0;
    if (ignore) { --ignore; continue; }
    if (inStr) {
      if (inStr == 1 && buf[f] == '\\' && buf[f+1] == '"') ++ignore;
      else if (buf[f] == '"') {
        if (inStr == 1) inStr = 0;
        else if (buf[f+1] != '"') inStr = 0;
        else ++ignore;
      }
    } else {
      switch (buf[f]) {
        case '"': inStr = 1; break;
        case '(': brcBuf[f].levelP = ++levelP; break;
        case ')': brcBuf[f].levelP = levelP--; break;
        case '[': brcBuf[f].levelB = ++levelB; break;
        case ']': brcBuf[f].levelB = levelB--; break;
        case ';': for (; buf[f]; ++f) brcBuf[f].levelP = brcBuf[f].levelB = 0; return;
        case '#':
          switch (buf[f+1]) {
            case ';': ++ignore; break;
            case '\\': ignore += 2; break;
            case '"': inStr = 2; ++ignore; break;
          }
          break;
      }
    }
  }
}


static int refreshLine (int fd, const char *prompt, const char *buf, size_t len, size_t pos, size_t cols) {
  char seq[64];
  size_t plen = strlen(prompt);
  size_t hipos = len;
  //
  if (linenoiseOptHilightBrackets && pos < len && strchr("()[]", buf[pos])) {
    buildBrcBuf(buf);
    if (brcBuf[pos].levelP || brcBuf[pos].levelB) {
      int h = (int)pos;
      //
      switch (buf[pos]) {
        case '(':
          for (++h; h < len; ++h) if (brcBuf[h].levelP == brcBuf[pos].levelP) break;
          break;
        case '[':
          for (++h; h < len; ++h) if (brcBuf[h].levelB == brcBuf[pos].levelB) break;
          break;
        case ')':
          for (--h; h >= 0; --h) if (brcBuf[h].levelP == brcBuf[pos].levelP) break;
          break;
        case ']':
          for (--h; h >= 0; --h) if (brcBuf[h].levelB == brcBuf[pos].levelB) break;
          break;
      }
      hipos = h<0?len:(size_t)h;
    }
  }
  //
  while (plen+pos >= cols) {
    ++buf;
    --len;
    --pos;
    if (hipos > 0) --hipos; else hipos = len;
  }
  while (plen+len > cols) --len;
  /* cursor to left edge */
  snprintf(seq, 64, linenoiseOptHilightBrackets?"\x1b[0m\x1b[0G":"\x1b[0G");
  if (write(fd, seq, strlen(seq)) == -1) return -1;
  /* write the prompt and the current buffer content */
  if (write(fd, prompt, strlen(prompt)) == -1) return -1;
  if (hipos < len) {
    /* rewrite this to output string in three parts */
    while (*buf) {
      if (!hipos) {
        hipos = len+1;
        if (write(fd, linenoiseBrcHiStr, strlen(linenoiseBrcHiStr)) == -1) return -1;
        if (write(fd, buf, 1) == -1) return -1;
        if (write(fd, "\x1b[0m", 4) == -1) return -1;
      } else {
        --hipos;
        if (write(fd, buf, 1) == -1) return -1;
      }
      ++buf;
    }
  } else if (write(fd, buf, len) == -1) return -1;
  /* erase to right */
  snprintf(seq, 64, "\x1b[0K");
  if (write(fd, seq, strlen(seq)) == -1) return -1;
  /* move cursor to original position */
  snprintf(seq, 64, "\x1b[0G\x1b[%dC", (int)(pos+plen));
  if (write(fd, seq, strlen(seq)) == -1) return -1;
  return 0;
}


static void autoCompletion (char *buf, size_t *posp, size_t *lenp, size_t cols) {
  if (linenoiseACHook) {
    //HACK!
    int ipos = (int)(*posp), ilen = (int)(*lenp);
    //
    linenoiseACHook(buf, &ipos, ilen, cols);
    ilen = strlen(buf);
    if (ipos < 0) posp = 0; else if (ipos > ilen) ipos = ilen;
    *posp = (size_t)ipos;
    *lenp = (size_t)ilen;
  }
}


/* move to the start of just finished sexpr */
static void leftSExpr (const char *buf, size_t *posp, size_t *lenp) {
  size_t pos = *posp;
  int level = 1;
  //
  if (pos <= 0) return;
  if (buf[pos-1] == ')') --level;
  while (pos > 0) {
    --pos;
    switch (buf[pos]) {
      case ')': ++level; break;
      case '(': if (--level < 1) goto done; break;
    }
  }
done:
  *posp = pos;
}


static void leftWord (const char *buf, size_t *posp, size_t *lenp) {
  size_t pos = *posp;
  //
  if (pos <= 0) return;
  if ((unsigned char)(buf[pos-1]) <= ' ') {
    /* spaces */
    while (pos > 0) if ((unsigned char)(buf[pos-1]) <= ' ') --pos; else break;
  }
  if (pos > 0) {
    if (isalnum(buf[pos-1])) {
      /* delete word */
      while (pos > 0) if (isalnum(buf[pos-1])) --pos; else break;
    } else {
      --pos;
    }
  }
  *posp = pos;
}


static void rightSExpr (const char *buf, size_t *posp, size_t *lenp) {
  size_t pos = *posp;
  size_t len = *lenp;
  int level = 1;
  //
  if (pos >= len) return;
  if (buf[pos+1] == '(') --level;
  while (pos < len) {
    ++pos;
    switch (buf[pos]) {
      case '\0': goto done;
      case '(': ++level; break;
      case ')':
        if (--level < 1) {
          if (pos < len) ++pos;
          goto done;
        }
        break;
    }
  }
done:
  *posp = pos;
}


static void rightWord (const char *buf, size_t *posp, size_t *lenp) {
  size_t pos = *posp;
  size_t len = *lenp;
  //
  if (pos >= len) return;
  if (isalnum(buf[pos])) {
    /* skip word */
    while (pos < len) if (isalnum(buf[pos])) ++pos; else break;
  } else {
    ++pos;
  }
  if (pos < len && (unsigned char)(buf[pos]) <= ' ') {
    /* spaces; go to the beginning of the word */
    while (pos < len) if ((unsigned char)(buf[pos]) <= ' ') ++pos; else break;
  }
  *posp = pos;
}


typedef void (*MoveFn) (const char *buf, size_t *posp, size_t *lenp);

static void yankIt (MoveFn fn, char *buf, size_t *posp, size_t *lenp) {
  size_t opos = *posp, len = *lenp, pos;
  //
  fn(buf, posp, lenp);
  pos = *posp;
  if (pos > opos) {
    size_t tmp = pos;
    pos = opos;
    opos = tmp;
  }
  if (pos < opos) {
    memcpy(yankBuf, buf+pos, opos-pos);
    yankBuf[opos-pos] = '\0';
    memmove(buf+pos, buf+opos, len-opos+1);
    len = strlen(buf); /* i'm soooo lazy */
  }
  if (pos > len) pos = len;
  *posp = pos;
  *lenp = len;
}


static void insertText (const char *text, char *buf, size_t *posp, size_t *lenp) {
  size_t pos = *posp, len = *lenp;
  //
  if (text == NULL) return;
  for (int f = 0; text[f] && len < LINENOISE_MAX_LINE_LEN-1; ++f, ++len, ++pos) {
    if (pos < len) memmove(buf+pos+1, buf+pos, len-pos);
    buf[pos] = text[f];
  }
  buf[len] = '\0';
  *posp = pos;
  *lenp = len;
}


/* action:
 *  1: upcase
 *  2: downcase
 *  3: capitalize */
static void doWordWork (char *buf, size_t *posp, size_t *lenp, int action) {
  size_t pos = *posp;
  //
  if (!buf[pos]) return;
  if (!isalnum(buf[pos])) return; /* not in the word */
  /* find word start */
  while (pos > 0) if (isalnum(buf[pos-1])) --pos; else break;
  /* process word */
  while (buf[pos] && isalnum(buf[pos])) {
    buf[pos] = (action != 2) ? toupper(buf[pos]) : tolower(buf[pos]);
    if (action == 3) action = 2;
    ++pos;
  }
}


static int linenoisePrompt (int fd, char *buf, const char *prompt) {
  size_t plen = strlen(prompt), pos = 0, len = 0, cols = getColumns();
  size_t buflen = LINENOISE_MAX_LINE_LEN-1; /* make sure there is always space for the nulterm */
  int historyIndex = 0;
  //
  buf[0] = '\0';
  /* the latest history entry is always our current buffer, that initially is just an empty string */
  linenoiseHistoryAdd("");
  if (write(fd, prompt, plen) == -1) return -1;
  for (;;) {
    char c, seq[2];
    int nread;
    //
    if (refreshLine(fd, prompt, buf, len, pos, cols)) return len;
    nread = read(fd, &c, 1);
    if (nread <= 0) return len;
    switch (c) {
      case 13: /* enter */
        pos = len;
        refreshLine(fd, prompt, buf, len, pos, cols);
        free(history[--historyLen]);
        return len;
      case 4: /* ctrl-d */
        pos = len;
        refreshLine(fd, prompt, buf, len, pos, cols);
        free(history[--historyLen]);
        return (len == 0) ? -1 : (int)len;
      case 3: /* ctrl-c */
        pos = len;
        refreshLine(fd, prompt, buf, len, pos, cols);
        free(history[--historyLen]);
        errno = EAGAIN;
        return -1;
      case 127: /* backspace */
      case 8: /* ctrl-h */
        if (pos > 0 && len > 0) {
          memmove(buf+pos-1, buf+pos, len-pos);
          --pos;
          buf[--len] = '\0';
        }
        break;
      case 20: /* ctrl-t */
        if (pos > 0 && pos < len) {
          int aux = buf[pos-1];
          buf[pos-1] = buf[pos];
          buf[pos] = aux;
          if (pos != len-1) ++pos;
        }
        break;
      case 2: /* ctrl-b */
        if (pos > 0) --pos;
        break;
      case 6: /* ctrl-f */
        if (pos != len) ++pos;
        break;
      case 16: /* ctrl-p */
        c = 'A';
        goto up_down_arrow;
      case 14: /* ctrl-n */
        c = 'B';
        goto up_down_arrow;
      case 27: /* escape sequence */
        if (read(fd, &c, 1) == -1) break;
        switch (c) {
          case '[': /* special char */
            if (read(fd, &c, 1) == -1) break;
            switch (c) {
              case 'A': case 'B': /* up and down arrow: history */
up_down_arrow:  if (historyLen > 1) {
                  /* Update the current history entry before to
                   * overwrite it with tne next one. */
                  free(history[historyLen-1-historyIndex]);
                  history[historyLen-1-historyIndex] = strdup(buf);
                  /* Show the new entry */
                  historyIndex += (c == 65) ? 1 : -1;
                  if (historyIndex < 0) {
                    historyIndex = 0;
                    break;
                  } else if (historyIndex >= historyLen) {
                    historyIndex = historyLen-1;
                    break;
                  }
                  strncpy(buf, history[historyLen-1-historyIndex], buflen);
                  buf[buflen] = '\0';
                  len = pos = strlen(buf);
                }
                break;
              case 'C': /* right arrow */
                if (pos != len) ++pos;
                break;
              case 'D': /* left arrow */
                if (pos > 0) --pos;
                break;
              case '1': /* Fx, mods+arrows, etc */
              case '2': /* here we wants Ctrl+Ins */
                seq[0] = c;
                if (read(fd, &c, 1) == -1) break;
                if (c == ';') {
                  /* with mods */
                  if (read(fd, &c, 1) == -1) break;
                  if (isdigit(c)) {
                    int modT = 0; /* bit 0: alt; bit 1: ctrl; bit 2: shift */
                    switch (c) {
                      /*case '2': modT = 0x04; break;*/
                      case '3': modT = 0x01; break;
                      /*case '4': modT = 0x05; break;*/
                      case '5': modT = 0x02; break;
                      default: goto skip_special;
                    }
                    if (read(fd, &c, 1) == -1) break;
                    switch (seq[0]) {
                      case '1': /* arrows */
                        switch (c) {
                          case 'C': /* right */
                            ((modT == 1) ? rightSExpr : rightWord)(buf, &pos, &len);
                            break;
                          case 'D': /* left */
                            ((modT == 1) ? leftSExpr : leftWord)(buf, &pos, &len);
                            break;
                        }
                        break;
                      case '2': /* ins */
                        switch (c) {
                          case '~':
                            insertText(yankBuf, buf, &pos, &len);
                            break;
                        }
                        break;
                    }
                  }
                  if (c == ';') goto skip_special;
                } else if (isdigit(c) || c == ';') goto skip_special;
                break;
              case '3': /* DEL */
                do { if (read(fd, &c, 1) == -1) break; } while (isdigit(c) || c == ';');
                if (pos < len) {
                  memmove(buf+pos, buf+pos+1, len-pos);
                  buf[--len] = '\0';
                }
                break;
              case '7': /* HOME */
                if (read(fd, &c, 1) == -1) break;
                if (c == '~') pos = 0;
                else if (isdigit(c) || c == ';') goto skip_special;
                break;
              case '8': /* END */
                if (read(fd, &c, 1) == -1) break;
                if (c == '~') pos = len;
                else if (isdigit(c) || c == ';') goto skip_special;
                break;
              case '0': case '4': case '5': case '6': case '9':
                /* skip special */
skip_special:   do { if (read(fd, &c, 1) == -1) break; } while (isdigit(c) || c == ';');
                break;
            }
            break;
          case '\x7f': /* alt+BS */
            /* delete previous sexpr */
            yankIt(leftSExpr, buf, &pos, &len);
            break;
          case 'u': case 'U':
            doWordWork(buf, &pos, &len, 1);
            break;
          case 'l': case 'L':
            doWordWork(buf, &pos, &len, 2);
            break;
          case 'c': case 'C':
            doWordWork(buf, &pos, &len, 3);
            break;
          case 'd': case 'D':
            /* delete next word */
            yankIt(rightWord, buf, &pos, &len);
            break;
          case 's': case 'S':
            /* delete next sexpr */
            yankIt(rightSExpr, buf, &pos, &len);
            break;
          case 'n': case 'N':
            /* delete the whole line. */
            strcpy(yankBuf, buf);
            buf[0] = '\0';
            pos = len = 0;
            break;
        }
        break;
      case '\t':
        autoCompletion(buf, &pos, &len, cols);
        break;
      case 11: /* Ctrl+k, delete from current to end of line. */
        strcpy(yankBuf, buf+pos);
        buf[pos] = '\0';
        len = pos;
        break;
      case '\x17': /* Ctrl+w */
        /* delete previous word */
        yankIt(leftWord, buf, &pos, &len);
        break;
      case '\x13': /* Ctrl+s */
        /* delete previous sexpr */
        yankIt(leftSExpr, buf, &pos, &len);
        break;
      case 25: /* Ctrl+y, yank */
        insertText(yankBuf, buf, &pos, &len);
        break;
      case 1: /* Ctrl+a, go to the start of the line */
        pos = 0;
        break;
      case 5: /* Ctrl+e, go to the end of the line */
        pos = len;
        break;
      default:
        if ((unsigned char)c >= 32) {
          seq[0] = c;
          seq[1] = '\0';
          insertText(seq, buf, &pos, &len);
        }
        break;
    }
  }
  free(history[--historyLen]);
  return len;
}


static int linenoiseRaw (char *buf, const char *prompt) {
  int count;
  //
  /* if (buflen == 0) { errno = EINVAL; return -1; } */
  if (enableRawMode(STDIN_FILENO) == -1) return -1;
  count = linenoisePrompt(STDIN_FILENO, buf, prompt);
  disableRawMode(STDIN_FILENO);
  fprintf(stdout, "\n"); fflush(stdout);
  return count;
}


char *linenoise (const char *prompt) {
  static char buf[LINENOISE_MAX_LINE_LEN];
  //
  if (isUnsupportedTerm() && !isatty(STDIN_FILENO)) {
    int count;
    //
    fprintf(stdout, "%s", prompt); fflush(stdout);
    if (fgets(buf, LINENOISE_MAX_LINE_LEN, stdin) == NULL) return NULL;
    count = strlen(buf)-1;
    while (count >= 0 && (buf[count] == '\n' || buf[count] == '\r')) buf[count--] = '\0';
  } else {
    if (linenoiseRaw(buf, prompt) < 0) return NULL;
  }
  return buf;
}


/* Using a circular buffer is smarter, but a bit more complex to handle. */
static int linenoiseHistoryAddInternal (const char *line, int noProcessing) {
  char *linecopy;
  //
  if (historyMaxLen <= 0) return -1;
  if (history == NULL) {
    history = malloc(sizeof(char *)*historyMaxLen);
    if (history == NULL) return -1;
    memset(history, 0, (sizeof(char *)*historyMaxLen));
  }
  if (!noProcessing) {
    // find duplicate, move it if found
    for (int f = 0; f < historyLen-1; ++f) {
      if (strcmp(line, history[f]) == 0) {
        // got it!
        char *tmp = history[f];
        //
        for (++f; f < historyLen; ++f) history[f-1] = history[f];
        history[historyLen-1] = tmp;
        return 0;
      }
    }
  }
  linecopy = strdup(line);
  if (linecopy == NULL) return -1;
  if (historyLen == historyMaxLen) {
    memmove(history, history+1, sizeof(char *)*(historyMaxLen-1));
    --historyLen;
  }
  history[historyLen++] = linecopy;
  return 0;
}


int linenoiseHistoryAdd (const char *line) {
  if (line != NULL) return linenoiseHistoryAddInternal(line, 0);
  return -1;
}


int linenoiseHistorySetMaxLen (int len) {
  char **new;
  //
  if (len < 1) return -1;
  if (history) {
    int tocopy = historyLen;
    //
    new = malloc(sizeof(char *)*len);
    if (new == NULL) return -1;
    if (len < tocopy) tocopy = len;
    memcpy(new, history+(historyMaxLen-tocopy), sizeof(char *)*tocopy);
    free(history);
    history = new;
  }
  historyMaxLen = len;
  if (historyLen > historyMaxLen) historyLen = historyMaxLen;
  return 0;
}


void linenoiseClearHistory (void) {
  if (history != NULL) {
    for (int f = historyLen-1; f >= 0; --f) if (history[f] != NULL) free(history[f]);
    historyLen = 0;
    free(history);
    history = NULL;
  }
}


int linenoiseHistorySave (FILE *fl) {
  if (!history) return 0;
  for (int f = 0; f < historyLen; ++f) {
    if (history[f] != NULL) {
      if (fprintf(fl, "%s\n", history[f]) < 0) return -1;
    }
  }
  return 0;
}


int linenoiseHistoryLoad (FILE *fl) {
  char *line = malloc((LINENOISE_MAX_LINE_LEN+16)*sizeof(char));
  //
  if (!line) return -1;
  linenoiseClearHistory();
  while (fgets(line, LINENOISE_MAX_LINE_LEN+1, fl)) {
    while (line[0] && (strchr("\n\r", line[strlen(line)-1]) != NULL)) line[strlen(line)-1] = '\0';
    linenoiseHistoryAdd(line);
  }
  free(line);
  return 0;
}


int linenoiseHistorySaveFile (const char *fname) {
  int res;
  FILE *fl = fopen(fname, "w");
  //
  if (!fl) return -1;
  res = linenoiseHistorySave(fl);
  if (fclose(fl) < 0) res = -1;
  return res;
}


int linenoiseHistoryLoadFile (const char *fname) {
  int res;
  FILE *fl = fopen(fname, "r");
  //
  if (!fl) return -1;
  res = linenoiseHistoryLoad(fl);
  if (fclose(fl) < 0) res = -1;
  return res;
}
