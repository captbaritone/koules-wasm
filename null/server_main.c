/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  null/server_main.c  entry point for the dedicated       *
*                      server                              *
***********************************************************/
/* sdl/init.c's main() opens a window before it does anything else, and
 * takes the server branch only after SDL is up. A dedicated server
 * wants the opposite: parse the server options, start listening, and
 * never touch a display at all.
 *
 * The options here are the server half of sdl/init.c's getopt string,
 * with the same meanings. The display options (-s, -l, -x, -M, -d) are
 * accepted and ignored so that an existing command line does not fail.
 */

#include "../koules.h"
#include "../net.h"		/* initport, DEFAULTINITPORT */
#include "../server.h"
#include "../mygetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main (int argc, char *argv[])
{
  char            c;

  /* server_loop() takes it from here and never returns. */
  server = 1;

  while ((c = getopt (argc, argv, "KWD:P:L:SC:slExMmdh")) != EOF)
    {
      switch (c)
	{
	case 'S':
	  break;		/* already implied */
	case 'K':
	  servergameplan = DEATHMATCH;
	  break;
	case 'W':
	  /* Width mode: room for 320x200 clients. */
	  GAMEHEIGHT = 360;
	  break;
	case 'E':
	  GAMEWIDTH = 900;
	  GAMEHEIGHT = 600;
	  MAPWIDTH = 900;
	  MAPHEIGHT = 600;
	  DIV = 2;
	  break;
	case 'D':
	  {
	    int             p;
	    if (sscanf (optarg, "%i", &p) != 1 || p < 0 || p > 4)
	      {
		printf ("-D : invalid difficulty\n");
		exit (2);
	      }
	    difficulty = p;
	  }
	  break;
	case 'P':
	  {
	    int             p;
	    if (sscanf (optarg, "%i", &p) != 1 || p < 0)
	      {
		printf ("-P : invalid port number\n");
		exit (2);
	      }
	    initport = p;
	  }
	  break;
	case 'L':
	  {
	    int             p;
	    if (sscanf (optarg, "%i", &p) != 1 || p < 1 || p > 100)
	      {
		printf ("-L : invalid level number\n");
		exit (2);
	      }
	    serverstartlevel = p - 1;
	  }
	  break;
	case 'C':
	  printf ("This build is a dedicated server; -C is for clients.\n");
	  exit (2);
	  /* Display and sound options: meaningless here, but harmless. */
	case 's':
	case 'l':
	case 'x':
	case 'M':
	case 'm':
	case 'd':
	  break;
	default:
	  printf ("USAGE: koules dedicated server\n"
		  " -P<port> select port. Default is:%i\n"
		  " -L<level> select starting level\n"
		  " -D<number> difficulty 0 (nightmare) to 4 (very easy)\n"
		  " -K deathmatch mode\n"
		  " -W width mode, for 320x200 clients\n", DEFAULTINITPORT);
	  exit (2);
	}
    }

  srand (time (NULL));

  init_server ();
  server_loop ();		/* does not return */
  return 0;
}
