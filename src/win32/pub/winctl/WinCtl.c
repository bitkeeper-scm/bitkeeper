/*
    WinCtl - control windows under Windows 9x/Windows NT
    Copyright (C) 1999  David J. Biesack (biesack@mindspring.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


David J. Biesack
biesack@mindspring.com
David.Biesack@acm.org
David.Biesack@sas.com

3325 Bentwillow Dr.
Fuquay Varina NC 27526

Version  Date       What
1.1      1999Dec30  Francis Litterio (franl@world.std.com) provided -focus switch
                    and properly quoted VERSION
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VERSION "1.1"

void usage(char *program)
{
  printf("Usage: %s [-focus | -id windowid | -title title | -name name | -class class] [command...]\n", program);
  printf("WinCtl (version %s) performs a window control operation on a top level window.\n\
Each command (alii in parentheses) is one of:\n\
    show (unhide)\n\
    restore (deiconify)\n\
    hide\n\
    maximize\n\
    minimize (iconify)\n\
    normal\n\
    top (raise)\n\
    topmost\n\
    notopmost\n\
    bottom (lower)\n\
    pos x-pos y-pos (move x-pos y-pos)\n\
    size width heigh\n\
\n\
command defaults to \"restore\". Commands are case insensitive.\n\
\n\
Specify a window by either a numeric decimal window id (-id windowid)\n\
or its case insensitive title/name (-title title or -name name)\n\
or its case insensitive class (-class class)\n\
or -focus to specify the window that currently has focus.\n\
When a class is specified, commands apply only to the first\n\
window of that class.\n\
Switches may be abbreviated.\n\
\n\
For example:\n\
        WinCtl -class Emacs restore raise pos 0 0 size 1000 700\n\
\n\
Copyright 1999 David J. Biesack\n\
biesack@mindspring.com\n\
http://biesack.home.mindspring.com\n\
\n\
", VERSION);
  exit(1);
}

int main(int argc, char **argv)
{
  int i;
  HWND windowId = NULL;
  LPSTR command = NULL;
  for (i=1; i<argc; i++)
    {
      LPSTR title = NULL;

      if (strnicmp(argv[i], "-focus", 6) == 0)
        {
          windowId = GetForegroundWindow();
        }
      else if (strnicmp(argv[i],"-id",2)==0 && i+1 < argc)
        {
          windowId = (HWND) atol(argv[++i]);
        }
      else if (((strnicmp(argv[i],"-title",2)==0) || (strnicmp(argv[i],"-name",2)==0)) && i+1 < argc)
        {
          title = argv[++i];
          windowId = FindWindow(NULL, title);
          if (windowId == NULL)
            {
              printf("WinCtl: can't find window \"%s\"\n", title);
              exit(1);
            }
        }
      else if ((strnicmp(argv[i],"-class",2)==0) && i+1 < argc)
        {
          title = argv[++i];
          windowId = FindWindow(title, NULL);
          if (windowId == NULL)
            {
              printf("WinCtl: can't find window class \"%s\"\n", title);
              exit(1);
            }
        }
      else if (argv[i][0] == '-')
        usage(argv[0]);
      else
        {
          int op = -1, flags = 0, xpos = 0, ypos = 0, height = 0, width = 0;
          HWND order = NULL;
          command = argv[i];
          
          if (windowId == NULL)
            {
              printf("WinCtl: no window specified.\n");
              usage(argv[0]);
            }

          /* parse the command */
          order = NULL;
          op = -1;
          flags = 0;
          if ( (stricmp(command,"pos")==0 || stricmp(command,"move")==0) && i+2 < argc)
            {
              xpos = atoi(argv[++i]);
              ypos = atoi(argv[++i]);
              flags = SWP_NOZORDER | SWP_NOSIZE;
            }
          else if (strnicmp(command,"size",4)==0 && i+2 < argc)
            {
              width = atoi(argv[++i]);
              height = atoi(argv[++i]);
              flags = SWP_NOZORDER | SWP_NOMOVE;
            }
          else if (stricmp(command, "raise")==0 || stricmp(command, "top")==0)
            {
              order = HWND_TOP;
              flags = SWP_NOMOVE | SWP_NOSIZE;
            }
          else if (stricmp(command, "topmost")==0)
            {
              order = HWND_TOPMOST;
              flags = SWP_NOMOVE | SWP_NOSIZE;
            }
          else if (stricmp(command, "notopmost")==0)
            {
              order = HWND_NOTOPMOST;
              flags = SWP_NOMOVE | SWP_NOSIZE;
            }
          else if (stricmp(command, "lower")==0 || stricmp(command, "bottom")==0)
            {
              order = HWND_BOTTOM;
              flags = SWP_NOMOVE | SWP_NOSIZE;
            }
          else if (stricmp(command,"hide")==0)          op = SW_HIDE;
          else if (strnicmp(command,"max",3)==0)        op = SW_MAXIMIZE;
          else if (strnicmp(command,"min",3)==0)        op = SW_MINIMIZE;
          else if (strnicmp(command,"iconify",1)==0)    op = SW_MINIMIZE;
          else if (strnicmp(command,"deiconify",1)==0)  op = SW_RESTORE;
          else if (stricmp(command,"restore")==0)       op = SW_RESTORE;
          else if (stricmp(command,"show")==0)          op = SW_SHOW;
          else if (stricmp(command,"unhide")==0)        op = SW_SHOW;
          else if (stricmp(command,"normal")==0)        op = SW_SHOWNORMAL;
          else usage(argv[0]);

          if (op != -1)
            ShowWindowAsync( windowId, op );
          if (order != NULL || flags != 0)
            SetWindowPos(windowId, order, xpos, ypos, width, height, flags);
        }
    }

  /* handle defaults. */

  if (windowId == NULL)
    {
      printf("WinCtl: no window specified.\n");
      usage(argv[0]);
    }

  if (command == NULL) ShowWindowAsync( windowId, SW_RESTORE );
    
  return 0;
}

/*
 * Local Variables:
 * compile-command: "nmake -f WinCtl.mak CFG=\"WinCtl - Win32 Release\""
 * End:
 */
