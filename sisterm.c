/* -------------------------
 Release Date  2019-02-04
 Update  Date  2019-02-06
------------------------- */
// ToDo
// -b light, dark
// Various syntax

#define PROGRAM      "sisterm"
#define VERSION      "1.1.4"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <stdio.h>
#include <time.h>
#include <regex.h>

#include "sisterm.h"
#include "syntax.h"
#include "palette.h"


#ifdef __linux__
#define CLOCK CLOCK_REALTIME_COARSE
#else
#define CLOCK CLOCK_REALTIME
#endif

#define MAX_LENGTH 256

unsigned char s[MAX_LENGTH];
unsigned char *io = s;

regex_t reg_prompt;
regex_t reg_vendors;
regex_t reg_ipv4_net;
regex_t reg_ipv4_sub;
regex_t reg_ipv4_wild;
regex_t reg_ipv6;
regex_t reg_var;
regex_t reg_string;
regex_t reg_action;
regex_t reg_protocol;
regex_t reg_keyword;
regex_t reg_cond;
regex_t reg_interface;
regex_t reg_command;
//regex_t reg_emphasis;
//regex_t reg_comment;


int main(int argc, char **argv)
{
  const char *sPort = NULL;     // SerialPort
  const char *B     = NULL;     // BaudRate
  const char *R     = NULL;     // File path to load
  const char *W     = NULL;     // File path to save
  speed_t baudRate  = B9600;    // Default BaudRate
  int  existsflag   = 0;        // Whether to log file
  int  logflag      = 0;        // Whether to take a log
  int  excflag      = 0;        // Exclamation mark flag
  int  bsflag       = 0;        // BackSpace Flag
  int  prflag       = 0;        // Prompt Flag
  int  rflag        = 0;        // Read file Flag
  int  ts           = 0;        // Whether to timestamp
  unsigned char     comm[32];   // For comment
  unsigned char     date[32];   // Buffer to set timestamp
  struct timespec   now;
  struct tm         tm;
  FILE              *log;
  char mode[3]      = "w+";


  for (int i=1; i<argc; i++)
  {
    if(*argv[i]=='-' && *(argv[i]+2)=='\0')
    {
      switch(*++argv[i])
      {
        case 'l':
        // /path/to/SerialPort
          if(NULL==argv[i+1])
          {
            nothingArgs(argv[0], *argv[i]);
            return EXIT_FAILURE;
          }
          sPort = argv[++i];
          break;

        case 's':
        // BaudRate speed
          if(NULL==argv[i+1])
          {
            nothingArgs(argv[0], *argv[i]);
            return EXIT_FAILURE;
          }
          B = argv[++i];
          break;

        case 'r':
        // /path/to/config.txt
          if(NULL==argv[i+1])
          {
            nothingArgs(argv[0], *argv[i]);
            return EXIT_FAILURE;
          }
          R = argv[++i];
          break;

        case 'w':
        // /path/to/log.txt
          if(NULL==argv[i+1])
          {
            nothingArgs(argv[0], *argv[i]);
            return EXIT_FAILURE;
          }
          W = argv[++i];
          break;

/* ------------------------------------------------
        case 'b':
        // /path/to/log.txt
          if(NULL==argv[i+1])
          {
            nothingArgs(argv[0], *argv[i]);
            return EXIT_FAILURE;
          }
          if( !strcasecmp(argv[++i], "light") )
            bg = LIGHT;
          else if( !strcasecmp(argv[i], "dark") )
            bg = DARK;
          else
            bg = NONE;
          break;
------------------------------------------------ */

        case 't':
        // Add timestamp to log
          ts = 1;
          break;

        case 'a':
        // Append log
          strcpy(mode, "a+");
          break;

        case 'h':
        // Show help
          usage(argv[0]);
          return EXIT_SUCCESS;

        case 'v':
        // Show version
          version();
          return EXIT_SUCCESS;

        default :
          printf("%s: unrecognized option `-%s`\n", argv[0], argv[i]);
          printf("Use %s -h for help\n", argv[0]);
          return EXIT_FAILURE;
      }
    }
    else
    {
      if( !strcmp(argv[i], "--help") ){
        usage(argv[0]); return EXIT_SUCCESS;
      }
      printf("%s: %s: System not found\n", argv[0], argv[i]);
      return EXIT_FAILURE;
    }
  }


  if( R != NULL ) rflag = 1;


  if( sPort == NULL && !rflag )
  {
    printf("%s: must specify Serial Port\n", argv[0]);
    return EXIT_FAILURE;
  }


  if( B != NULL && !rflag )
  {
    if     (!strcmp(B, "0"))      baudRate = B0;      // hang up
    else if(!strcmp(B, "50"))     baudRate = B50;
    else if(!strcmp(B, "75"))     baudRate = B75;
    else if(!strcmp(B, "110"))    baudRate = B110;
    else if(!strcmp(B, "134"))    baudRate = B134;
    else if(!strcmp(B, "150"))    baudRate = B150;
    else if(!strcmp(B, "200"))    baudRate = B200;
    else if(!strcmp(B, "300"))    baudRate = B300;
    else if(!strcmp(B, "600"))    baudRate = B600;
    else if(!strcmp(B, "1200"))   baudRate = B1200;
    else if(!strcmp(B, "1800"))   baudRate = B1800;
    else if(!strcmp(B, "2400"))   baudRate = B2400;
    else if(!strcmp(B, "4800"))   baudRate = B4800;
    else if(!strcmp(B, "9600"))   baudRate = B9600;   // Default
    else if(!strcmp(B, "19200"))  baudRate = B19200;
    else if(!strcmp(B, "38400"))  baudRate = B38400;
    else if(!strcmp(B, "57600"))  baudRate = B57600;
    else if(!strcmp(B, "115200")) baudRate = B115200;
    else if(!strcmp(B, "230400")) baudRate = B230400;
    else
    {
      printf("(%s) Invalid BaudRate...\n", B);
      return EXIT_FAILURE;
    }
  }


  if( W != NULL && !rflag )
  {
    if(!access(W, F_OK))
      existsflag = 1;

    if( existsflag && (access( W, (F_OK | R_OK) ) < 0) )
    {
      printf("Access to the log file is denied\n");
      return EXIT_FAILURE;
    }

    if( existsflag && !strcmp(mode, "w+") )
    {
      printf("\a%s already exists!\n", W);
      printf("Do you want to overwrite?[confirm]");
      unsigned char con = getchar();
      if( !(con=='\n' || con=='y' || con=='Y') )
        return EXIT_SUCCESS;
    }

    log = fopen(W, mode);

    if(log == NULL)
    {
      if(access(W, F_OK))
      {
        printf("Failed to create file\n");
        printf("Try to check the permissions\n");
        return EXIT_FAILURE;
      }
      else if( access( W, (F_OK | R_OK) ) < 0 )
      {
        printf("Access to the log file is denied\n");
        return EXIT_FAILURE;
      }

      printf("%s: open (%s): Failure\n", argv[0], W);
      return EXIT_FAILURE;
    }

    logflag = 1;
  }
  else
    ts = 0;

  struct termios tio;
  struct termios stdio;
  struct termios old_stdio;
  int fd;

  unsigned char c             = '0';
  const unsigned char endcode = '~';
  tcgetattr(STDOUT_FILENO, &old_stdio);

  memset(&stdio, 0, sizeof(stdio));
  stdio.c_iflag     = 0;
  stdio.c_oflag     = 0;
  stdio.c_cflag     = 0;
  stdio.c_lflag     = 0;
  stdio.c_cc[VMIN]  = 1;
  stdio.c_cc[VTIME] = 0;
  tcsetattr(STDOUT_FILENO, TCSANOW,&stdio);
  tcsetattr(STDOUT_FILENO, TCSAFLUSH,&stdio);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

  memset(&tio, 0, sizeof(tio));
  tio.c_iflag       = 0;
  tio.c_oflag       = 0;
  tio.c_cflag       = CS8 | CREAD | CLOCAL;
  tio.c_lflag       = 0;
  tio.c_cc[VMIN]    = 1;
  tio.c_cc[VTIME]   = 5;

  fd = open(sPort, O_RDWR | O_NONBLOCK);
  if( fd < 0 && !rflag )
  {
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
    if(access( sPort, F_OK ) < 0)
      printf("%s: open (%s): No such file or directory\n", argv[0], sPort);
    else if(access( sPort, (R_OK | W_OK) ) < 0)
      printf("%s: open (%s): Permission denied\n", argv[0], sPort);
    // en route
    //else if(fcntl(fd, F_GETFL) == (F_RDLCK | F_WRLCK) )
    //  printf("%s: Line in use\n", sPort);
    else
      printf("%s: open (%s): Failure\n", argv[0], sPort);
    close(fd);
    return EXIT_FAILURE;
  }

  if( cfsetspeed(&tio, baudRate) != 0 ) return EXIT_FAILURE;

  if( regcompAll() != 0 ) return EXIT_FAILURE;

  if( rflag )
  {
    int  i;
    FILE *fr;
    fr = fopen(R, "r");
    if(fr == NULL)
    {
      tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
      if(access( R, F_OK ) < 0)
        printf("%s: open (%s): No such file or directory\n", argv[0], R);
      else if(access( R, (R_OK) ) < 0)
        printf("%s: open (%s): Permission denied\n", argv[0], R);
      else
        printf("%s: open (%s): Failure\n", argv[0], R);
      return EXIT_FAILURE;
    }

    tcsetattr(fd, TCSANOW, &tio);

    while((i=fgetc(fr)) != EOF)
    {
      c = (char)i;
      write(STDOUT_FILENO, &c, 1);

      if( 0x0a==c )
      {
        prflag  = 1;
        excflag = 0;
        write(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
      }

      coloring(c);

      if( excflag )
        memset( io = s, '\0', MAX_LENGTH );

      if( prflag )
      {
        if( regexec(&reg_prompt, &c, 0, 0, 0) == 0)
        {
          memset( io = s, '\0', MAX_LENGTH );
          prflag = 0;
        }
      }

      if( 0x21==c )
      {
        excflag = 1;
        write(STDOUT_FILENO, comm, sprintf(comm, "\b%s%c", COLOR_COMMENT, c));
      }

      if(read(STDIN_FILENO, &c, 1) > 0)
      {
        if(c == endcode) break;
      }
    }

    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
    fclose(fr);
    printf("%s\n", RESET);

    return EXIT_SUCCESS;
  }

  printf("Connected.\n");

  tcsetattr(fd, TCSANOW, &tio);

  while (1)
  {
    // if new data is available on the serial port, print it out
    // ToDo Parallel processing
    if(read(fd, &c, 1) > 0)
    {
      write(STDOUT_FILENO, &c, 1);
      if(logflag) fwrite(&c, 1, 1, log);

      if( 0x08==c && 0==bsflag )
      {
        bsflag = 3;
        if( s[1] == '\0' )
        {
          excflag = 0;
          write(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
        }
      }

      if( 0x0a==c )
      {
        prflag  = 1;
        excflag = 0;
        write(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
        if( ts )
        {
          clock_gettime(CLOCK, &now);
          localtime_r(&now.tv_sec, &tm);
          sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
              tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
              tm.tm_hour, tm.tm_min, tm.tm_sec,
              (int) now.tv_nsec / 1000000
              );
          fwrite(date, strlen(date), 1, log);
        }
      }

      if     ( 0 == bsflag )   coloring(c);
      else if( 3 == bsflag-- ) coloring(c);

      if( excflag )
        memset( io = s, '\0', MAX_LENGTH );

      if( prflag ) {
        if( regexec(&reg_prompt, &c, 0, 0, 0) == 0 )
        {
          memset( io = s, '\0', MAX_LENGTH );
          prflag = 0;
        }
      }

      if( 0x21==c )
      {
        excflag = 1;
        write(STDOUT_FILENO, comm, sprintf(comm, "\b%s%c", COLOR_COMMENT, c));
      }

    }

    // if new data is available on the console, send it to the serial port
    if(read(STDIN_FILENO, &c, 1) > 0)
    {
      if(c == endcode) break;
      write(fd, &c, 1);
    }
  }

  close(fd);
  if(logflag)
  {
    c = '\n';
    fwrite(&c, 1, 1, log);
    fclose(log);
  }
  tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);

  printf("%s\nDisconnected.\n", RESET);

  return EXIT_SUCCESS;
}

int regcompAll()
{
  int regmiss = 0 ;
  if(regcomp(&reg_prompt   , "#|>"    , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_vendors  , VENDORS  , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_ipv4_net , IPV4_NET , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_ipv4_sub , IPV4_SUB , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_ipv4_wild, IPV4_WILD, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_ipv6     , IPV6     , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_var      , VAR      , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_string   , STRING   , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_action   , ACTION   , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_protocol , PROTOCOL , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_keyword  , KEYWORD  , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_cond     , COND     , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_interface, INTERFACE, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regcomp(&reg_command  , COMMAND  , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  //if(regcomp(&reg_emphasis , EMPHASIS , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  //if(regcomp(&reg_comment  , COMMENT  , REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) regmiss=1;
  if(regmiss) return EXIT_FAILURE;
  return 0;
}


int syntaxCheck(unsigned char *str)
{
  if( regexec(&reg_vendors  , str, 0, 0, 0) == 0 ) return HL_VENDORS;
  if( regexec(&reg_ipv4_net , str, 0, 0, 0) == 0 ) return HL_IPV4_NET;
  if( regexec(&reg_ipv4_sub , str, 0, 0, 0) == 0 ) return HL_IPV4_SUB;
  if( regexec(&reg_ipv4_wild, str, 0, 0, 0) == 0 ) return HL_IPV4_WILD;
  if( regexec(&reg_ipv6     , str, 0, 0, 0) == 0 ) return HL_IPV6;
  if( regexec(&reg_string   , str, 0, 0, 0) == 0 ) return HL_STRING;
  if( regexec(&reg_var      , str, 0, 0, 0) == 0 ) return HL_VAR;
  if( regexec(&reg_action   , str, 0, 0, 0) == 0 ) return HL_ACTION;
  if( regexec(&reg_protocol , str, 0, 0, 0) == 0 ) return HL_PROTOCOL;
  if( regexec(&reg_keyword  , str, 0, 0, 0) == 0 ) return HL_KEYWORD;
  if( regexec(&reg_cond     , str, 0, 0, 0) == 0 ) return HL_COND;
  if( regexec(&reg_interface, str, 0, 0, 0) == 0 ) return HL_INTERFACE;
  if( regexec(&reg_command  , str, 0, 0, 0) == 0 ) return HL_COMMAND;
  //if( regexec(&reg_emphasis , str, 0, 0, 0) == 0 ) return HL_EMPHASIS;
  //if( regexec(&reg_comment  , str, 0, 0, 0) == 0 ) return HL_COMMENT;
  return -1;
}


void repaint(unsigned char *color)
{
  io = s;
  size_t i = 0;
  unsigned char str[MAX_LENGTH];
  unsigned char tmp[MAX_LENGTH];
  while(*io)
  {
    tmp[i++] = *io++;
    write(STDOUT_FILENO, str, sprintf(str, "\b \b"));
  }
  if(tmp[i]!='\0') tmp[i]='\0';
  write(STDOUT_FILENO, str, sprintf(str, "%s%s%s", color, tmp, RESET));
}


void coloring(unsigned char c)
{
  if( (c!=0x08 && c<0x21) )
  {
    memset( io = s, '\0', sizeof(s) );
    return;
  }

  if( '\b'==c )
  {
    if( s[0]!='\0' )
    {
      *io--;
      s[strlen(s)-1] = '\0';
    }
  }
  else if( strlen(s) < MAX_LENGTH - 1 )
  {
    *io++ = c;
  }
  else
  {
    memset( io = s, '\0', sizeof(s) );
    return;
  }

  int checked = syntaxCheck(s);
  if(checked >= 0)
  {
    switch(checked)
    {
      case HL_VENDORS:
        repaint(COLOR_VENDORS);
        break;
      case HL_ACTION:
        repaint(COLOR_ACTION);
        break;
      case HL_KEYWORD:
        repaint(COLOR_KEYWORD);
        if(!strcasecmp(s, "route")) return;
        if(!strcasecmp(s, "system")) return;
        if(!strcasecmp(s, "host")) return;
        break;
      case HL_COND:
        repaint(COLOR_COND);
        break;
      case HL_PROTOCOL:
        repaint(COLOR_PROTOCOL);
        if(!strcasecmp(s, "dial")) return;
        break;
      case HL_VAR:
        repaint(COLOR_VAR);
        if(!strcasecmp(s, "enable")) return;
        break;
      case HL_STRING:
        repaint(COLOR_STRING);
        break;
      //case HL_EMPHASIS:
      //  repaint(COLOR_EMPHASIS);
      //  break;
      case HL_INTERFACE:
        repaint(COLOR_INTERFACE);
        break;
      //case HL_COMMENT:
      //  repaint(COLOR_COMMENT);
      //  break;
      case HL_IPV4_NET:
        repaint(COLOR_IPV4_NET);
        if(*(io-1)>0x29 || *(io-1)<0x3a) return;
        break;
      case HL_IPV4_SUB:
        repaint(COLOR_IPV4_SUB);
        if(*(io-1)>0x29 || *(io-1)<0x3a) return;
        break;
      case HL_IPV4_WILD:
        repaint(COLOR_IPV4_WILD);
        if(*(io-1)>0x29 || *(io-1)<0x3a) return;
        break;
      case HL_IPV6:
        repaint(COLOR_IPV6);
        if( (*(io-1)>0x29 || *(io-1)<0x3b)
         || (*(io-1)>0x40 || *(io-1)<0x47)
         || (*(io-1)>0x60 || *(io-1)<0x67)
         ) return;
        break;
      default: break;
    }
    memset( io = s, '\0', sizeof(s) );
  }

}

void nothingArgs(char *argv0, char op)
{
  printf("%s: option `-%c` requires an argument\n", argv0, op);
}

void version()
{
  printf("%s %s\n", PROGRAM, VERSION);
}

void usage(char *v)
{
  printf("Usage: %s [-l SERIAL_PORT] [-s BAUDRATE] [-r /path/to/file]\n"
         "            [-w /path/to/LOG] [-t] [-a] [-h] [-v]\n\n", v);

  printf("Command line interface for Serial Console by Network device.\n");
  printf("------------------------------------------------------------\n");
  printf("https://github.com/yorimoi/sisterm\n\n");

  printf("Options:\n");
  printf("  -h,--help   Show this help message and exit\n");
  printf("  -v          Show %s version and exit\n", PROGRAM);
  printf("  -l port     Use named device   (e.g. /dev/ttyS0)\n");
  printf("  -s speed    Use given speed    (default 9600)\n");
  printf("  -r path     Output config file (e.g. /tmp/config.txt)\n");
  printf("  -w path     Saved log          (e.g. /tmp/sist.log)\n");
  printf("  -t          Add timestamp to log\n");
  printf("  -a          Append to log      (default overwrite)\n\n");

  printf("Commands:\n");
  printf("  ~           Terminate the conversation\n");
}

