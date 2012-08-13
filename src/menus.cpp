#include "cube.h"

void refreshservers(void);

namespace menu
{
  struct mitem { char *text; char *action; };
  struct gmenu
  {
    const char *name;
    vector<mitem> items;
    int mwidth;
    int menusel;
  };

  static vector<gmenu> menus;
  static int vmenu = -1;
  static ivector menustack;
  static string empty = "";

  void set(int menu)
  {
    if((vmenu = menu)>=1) resetmovement(player1);
    if(vmenu==1) menus[1].menusel = 0;
  }

  void show(char *name)
  {
    loopv(menus) if(i>1 && strcmp(menus[i].name, name)==0) {
      set(i);
      return;
    }
  }

  static int compare(mitem *a, mitem *b)
  {
    const int x = atoi(a->text);
    const int y = atoi(b->text);
    if(x>y) return -1;
    if(x<y) return 1;
    return 0;
  }

  void sort(int start, int num)
  {
    qsort(&menus[0].items[start], num, sizeof(mitem), (int (__cdecl *)(const void *,const void *))compare);
  }

  bool render(void)
  {
    if(vmenu<0) { menustack.setsize(0); return false; }
    if(vmenu==1) refreshservers();
    gmenu &m = menus[vmenu];
    sprintf_sd(title)(vmenu>1 ? "[ %s menu ]" : "%s", m.name);
    int mdisp = m.items.length();
    int w = 0;
    loopi(mdisp)
    {
      int x = text_width(m.items[i].text);
      if(x>w) w = x;
    }

    int tw = text_width(title);
    if(tw>w) w = tw;
    int step = FONTH/4*5;
    int h = (mdisp+2)*step;
    int y = (VIRTH-h)/2;
    int x = (VIRTW-w)/2;
    blendbox(x-FONTH/2*3, y-FONTH, x+w+FONTH/2*3, y+h+FONTH, true);
    draw_text(title, x, y,2);
    y += FONTH*2;

    if(vmenu) {
      int bh = y+m.menusel*step;
      blendbox(x-FONTH, bh-10, x+w+FONTH, bh+FONTH+10, false);
    }
    loopj(mdisp) {
      draw_text(m.items[j].text, x, y, 2);
      y += step;
    }
    return true;
  }

  void newm(const char *name)
  {
    gmenu &menu = menus.add();
    menu.name = newstring(name);
    menu.menusel = 0;
  }

  void manual(int m, int n, char *text)
  {
    if(!n) menus[m].items.setsize(0);
    mitem &mitem = menus[m].items.add();
    mitem.text = text;
    mitem.action = empty;
  }

  void item(char *text, char *action)
  {
    gmenu &menu = menus.last();
    mitem &mi = menu.items.add();
    mi.text = newstring(text);
    mi.action = action[0] ? newstring(action) : mi.text;
  }

  bool key(int code, bool isdown)
  {
    if(vmenu<=0) return false;
    int menusel = menus[vmenu].menusel;
    if(isdown) {
      if(code==SDLK_ESCAPE) {
        set(-1);
        if(!menustack.empty()) set(menustack.pop());
        return true;
      }
      else if(code==SDLK_UP || code==-4) menusel--;
      else if(code==SDLK_DOWN || code==-5) menusel++;
      int n = menus[vmenu].items.length();
      if(menusel<0) menusel = n-1;
      else if(menusel>=n) menusel = 0;
      menus[vmenu].menusel = menusel;
    } else {
      if(code==SDLK_RETURN || code==-2) {
        char *action = menus[vmenu].items[menusel].action;
        if(vmenu==1) connects(getservername(menusel));
        menustack.add(vmenu);
        set(-1);
        execute(action, true);
      }
    }
    return true;
  }

  COMMANDN(menuitem, item, ARG_2STR);
  COMMANDN(showmenu, show, ARG_1STR);
  COMMANDN(newmenu, newm, ARG_1STR);
}

