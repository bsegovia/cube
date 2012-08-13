#include "cube.h"
#include <ctype.h>

namespace console
{
  struct cline { char *cref; int outtime; };
  static vector<cline> conlines;
  static const int ndraw = 5;
  static const unsigned int WORDWRAP = 80;
  static int conskip = 0;
  static bool saycommandon = false;
  static string commandbuf;
  static cvector vhistory;
  static int histpos = 0;

  static void setconskip(int n)
  {
    conskip += n;
    if (conskip<0) conskip = 0;
  }

  static void line(const char *sf, bool highlight)
  {
    cline cl;
    cl.cref = conlines.length()>100 ? conlines.pop().cref : newstringbuf("");   // constrain the buffer size
    cl.outtime = lastmillis; // for how long to keep line on screen
    conlines.insert(0,cl);
    if (highlight) { // show line in a different colour, for chat etc.
      cl.cref[0] = '\f';
      cl.cref[1] = 0;
      strcat_s(cl.cref, sf);
    } else
      strcpy_s(cl.cref, sf);
    puts(cl.cref);
#ifndef WIN32
    fflush(stdout);
#endif
  }

  void out(const char *s, ...)
  {
    sprintf_sdv(sf, s);
    s = sf;
    int n = 0;
    while (strlen(s)>WORDWRAP) { // cut strings to fit on screen
      string t;
      strn0cpy(t, s, WORDWRAP+1);
      line(t, n++!=0);
      s += WORDWRAP;
    }
    line(s, n!=0);
  }

  void render(void)
  {
    int nd = 0;
    char *refs[ndraw];
    loopv(conlines)
      if (conskip ? i>=conskip-1 || i>=conlines.length()-ndraw :
          lastmillis-conlines[i].outtime<20000) {
        refs[nd++] = conlines[i].cref;
        if (nd==ndraw) break;
      }
    loopj(nd) draw_text(refs[j], FONTH/3, (FONTH/4*5)*(nd-j-1)+FONTH/3, 2);
  }

  // keymap is defined externally in keymap.cfg
  struct keym { int code; char *name; char *action; } keyms[256];
  int numkm = 0;

  static void keymap(char *code, char *key, char *action)
  {
    keyms[numkm].code = atoi(code);
    keyms[numkm].name = newstring(key);
    keyms[numkm++].action = newstringbuf(action);
  }

  static void bindkey(char *key, char *action)
  {
    for(char *x = key; *x; x++) *x = toupper(*x);
    loopi(numkm) if (strcmp(keyms[i].name, key)==0) {
      strcpy_s(keyms[i].action, action);
      return;
    }
    out("unknown key \"%s\"", key);
  }

  /*! Turns input to the command line on or off */
  static void saycommand(const char *init)
  {
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
    if (!editmode) keyrepeat(saycommandon);
    if (!init) init = "";
    strcpy_s(commandbuf, init);
  }

  static void mapmsg(char *s) { strn0cpy(hdr.maptitle, s, 128); }

#ifndef WIN32
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>
#endif

  static void paste()
  {
#ifdef WIN32
    if (!IsClipboardFormatAvailable(CF_TEXT)) return;
    if (!OpenClipboard(NULL)) return;
    char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
    strcat_s(commandbuf, cb);
    GlobalUnlock(cb);
    CloseClipboard();
#else
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    wminfo.subsystem = SDL_SYSWM_X11;
    if (!SDL_GetWMInfo(&wminfo)) return;
    int cbsize;
    char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
    if (!cb || !cbsize) return;
    int commandlen = strlen(commandbuf);
    for(char *cbline = cb, *cbend;
        commandlen + 1 < _MAXDEFSTR &&
        cbline < &cb[cbsize]; cbline = cbend + 1)
    {
      cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
      if (!cbend) cbend = &cb[cbsize];
      if (commandlen + cbend - cbline + 1 > _MAXDEFSTR) cbend = cbline + _MAXDEFSTR - commandlen - 1;
      memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
      commandlen += cbend - cbline;
      commandbuf[commandlen] = '\n';
      if (commandlen + 1 < _MAXDEFSTR && cbend < &cb[cbsize]) ++commandlen;
      commandbuf[commandlen] = '\0';
    }
    XFree(cb);
#endif
  }

  static void history(int n)
  {
    static bool rec = false;
    if (!rec && n>=0 && n<vhistory.length()) {
      rec = true;
      execute(vhistory[vhistory.length()-n-1]);
      rec = false;
    }
  }

  void keypress(int code, bool isdown, int cooked)
  {
    if (saycommandon) {                               // keystrokes go to commandline
      if (isdown) {
        switch(code) {
          case SDLK_RETURN:
            break;
          case SDLK_BACKSPACE:
          case SDLK_LEFT:
          {
            for(int i = 0; commandbuf[i]; i++) if (!commandbuf[i+1]) commandbuf[i] = 0;
            resetcomplete();
            break;
          }
          case SDLK_UP:
            if (histpos) strcpy_s(commandbuf, vhistory[--histpos]);
            break;
          case SDLK_DOWN:
            if (histpos<vhistory.length()) strcpy_s(commandbuf, vhistory[histpos++]);
            break;
          case SDLK_TAB:
            complete(commandbuf);
            break;
          case SDLK_v:
            if (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) { paste(); return; }
          default:
            resetcomplete();
            if (cooked) { char add[] = { cooked, 0 }; strcat_s(commandbuf, add); }
        }
      } else {
        if (code==SDLK_RETURN) {
          if (commandbuf[0]) {
            if (vhistory.empty() || strcmp(vhistory.last(), commandbuf))
              vhistory.add(newstring(commandbuf));  // cap this?
            histpos = vhistory.length();
            if (commandbuf[0]=='/')
              execute(commandbuf, true);
            else
              toserver(commandbuf);
          }
          saycommand(NULL);
        }
        else if (code==SDLK_ESCAPE)
          saycommand(NULL);
      }
    } else if (!menu::key(code, isdown)) { // keystrokes go to menu
      loopi(numkm) if (keyms[i].code==code) { // keystrokes go to game, lookup in keymap and execute
        string temp;
        strcpy_s(temp, keyms[i].action);
        execute(temp, isdown);
        return;
      }
    }
  }

  char *getcurcommand() { return saycommandon ? commandbuf : NULL; }

  void writebinds(FILE *f)
  {
    loopi(numkm)
      if (*keyms[i].action)
        fprintf(f, "bind \"%s\" [%s]\n", keyms[i].name, keyms[i].action);
  }

  COMMANDN(conskip, setconskip, ARG_1INT);
  COMMAND(keymap, ARG_3STR);
  COMMANDN(bind, bindkey, ARG_2STR);
  COMMAND(saycommand, ARG_VARI);
  COMMAND(mapmsg, ARG_1STR);
  COMMAND(history, ARG_1INT);
}

