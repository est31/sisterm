
#define COMMAND_NAME   "sist"
#define PROGRAM_NAME   "sisterm"
#define VERSION        "1.4.0-rc2"
#define UPDATE_DATE    "20190603"

#define CONFIG_FILE    "sist.conf"
#define MAX_PARAM_LEN  2048

#include "sisterm.h"
#include "palette.h"

//Debug
#include <stdarg.h>


#ifdef __linux__
#define              CLOCK CLOCK_REALTIME_COARSE
#else
#define              CLOCK CLOCK_REALTIME
#endif

#define MAX_LENGTH   256
#define REG_FLAGS    REG_EXTENDED | REG_NOSUB | REG_ICASE

typedef int bool;
#define true  1
#define false 0

char s[MAX_LENGTH];
char *io = s;
bool bsflag = false;


regex_t reg_prompt;
regex_t reg_slash;
// IPv4Net ...

// param
typedef struct {
    char *name;
    char *color;
    regex_t regex;
} Param;

Param *params;  // Dynamic array
int params_len;

//For debug
void DebugLog(const char *_format, ... ) {
    int len;
    va_list argList;
    va_start(argList, _format);
    char str[MAX_LENGTH];
    len = vsprintf(str, _format, argList);
    va_end(argList);
    transmission(STDOUT_FILENO, str, len);
}

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
    const char *sPort = NULL;             // SerialPort
    const char *B     = NULL;             // BaudRate
    const char *R     = NULL;             // File path to load
    const char *W     = NULL;             // File path to save
    const char *C     = NULL;             // File path to config
    speed_t baudRate  = B9600;            // Default BaudRate
    bool existsflag   = false;            // Whether to log file
    bool excflag      = false;            // Exclamation mark flag for comment
    int  comlen       = 0;                // Comment length
    bool escflag      = false;            // '^'
    bool spflag       = false;            // '['
    bool tilflag      = false;            // Del key -> BS key
    bool arrflag      = false;            // Arrow keys flag
    bool logflag      = false;            // Whether to take a log
    bool tcpflag      = false;            // TCP
    bool prflag       = false;            // Prompt Flag
    bool wflag        = false;            // Write file Flag
    bool rflag        = false;            // Read file Flag
    bool another_conf = false;            // another config file
    bool cflag        = true;             // Color Flag
    bool ts           = false;            // Whether to timestamp
    char* logbuf      = (char*)malloc(MAX_LENGTH);
    char* lb          = logbuf;           // Log buffer pointer
    int  lblen        = MAX_LENGTH - 2;
    char              comm[32];           // For comment
    char              date[81];           // Buffer to set timestamp
    struct timespec   now;
    struct tm         tm;
    FILE *lf          = NULL;
    char mode[3]      = "w+";             // Log file open mode
    char dstaddr[16];


    {
	    static struct option longopts[] = {
	    	{"port",      required_argument, 0, 'l'},
	    	{"speed",     required_argument, 0, 's'},
	    	{"read",      required_argument, 0, 'r'},
	    	{"write",     required_argument, 0, 'w'},
	    	{"config",    required_argument, 0, 'c'},
	    	{"help",      no_argument,       0, 'h'},
	    	{"version",   no_argument,       0, 'v'},
	    	{"timestamp", no_argument,       0, 't'},
	    	{"append",    no_argument,       0, 'a'},
	    	{"no-color",  no_argument,       0, 'n'},
	    	{0,0,0,0}
	    };
        int opt, idx;
        while((opt = getopt_long(
                argc,
                argv,
                "l:s:r:w:c:tanp:hv",
                longopts,
                &idx)
        ) != -1) {
            switch(opt) {
                case 'l':
                  // /path/to/SerialPort
                    sPort = optarg;
                    break;

                case 's':
                  // BaudRate speed
                    B = optarg;
                    break;

                case 'r':
                  // /path/to/config.txt
                    R = optarg;
                    rflag = true;
                    break;

                case 'w':
                  // /path/to/log.txt
                    W = optarg;
                    wflag = true;
                    break;

                case 'c':
                  // /path/to/config
                    C = optarg;
                    another_conf = true;
                    break;

                case 't':
                  // Add timestamp to log
                    ts = true;
                    break;

                case 'a':
                  // Append log
                    strcpy(mode, "a+");
                    break;

                case 'n':
                  // Without color
                    cflag = false;
                    break;

/* ---------------------------------------------------------------- */
                case 'p':
                  // Telnet test
                    tcpflag = true;
                    if(strlen(optarg) > 15) {
                        error("(%s) Invalid IP Address\n", optarg);
                        return EXIT_FAILURE;
                    }
                    strcpy(dstaddr, optarg);
                    break;
/* --------------------------------------------------------------- */

                case 'h':
                  // Show help
                    usage(argv);
                    return EXIT_SUCCESS;

                case 'v':
                  // Show version
                    version();
                    return EXIT_SUCCESS;

                default :
                    error("Use %s -h for help\n", argv[0]);
                    return EXIT_FAILURE;
              }
         }
    }

    {
        FILE *cfp;  // Config File Pointer
        char *path;

        if(another_conf) {
            path = (char*)malloc(strlen(C)+1);
            strcpy(path, C);
        } else {
            path = (char*)malloc(strlen(getenv("HOME")) + strlen(CONFIG_FILE) + 1);
            strcat(path, getenv("HOME"));
            strcat(path, "/" CONFIG_FILE);
        }

        regcompAll();

        cfp = fopen(path, "r");

        if(cfp == NULL) {
            cflag = false;
            error("%s: File open error", path);
            fprintf(stderr, "Press ENTER to continue of without color mode");
            (void)getchar();
        }
        else {
            params = (Param*)malloc(sizeof(Param));
            char *str = (char*)malloc(MAX_PARAM_LEN);
            int line = 0;

            while(fgets(str, MAX_PARAM_LEN, cfp) != NULL) {
                ++line;
                char top = '\0';
                sscanf(str, " %c", &top);
                // ignore comment and blank line
                if(strchr(" #\n\0", top))
                    continue;

                char *key = (char*)malloc(5+1),
                     *name = (char*)malloc(64),
                     *param = (char*)malloc(MAX_PARAM_LEN);

                sscanf(str, " %63[^ .] . %5[^ =] = %2047[^\n]", name, key, param);
                //printf("[name:%s, key:%s, param:%s]\n", name, key, param);

                bool suffer = false;
                for(int i=0; i<params_len; ++i)
                    if(!strcmp(params[i].name, name)) {
                        suffer = true;
                        break;
                    }
                if(!suffer) {
                    ++params_len;
                    params = (Param*)realloc(params, params_len * sizeof(Param));  // 要修正
                    params[params_len-1].name = (char*)malloc(strlen(name)+1);
                    strcpy(params[params_len-1].name, name);
                }

                // DOS file format
                if(param[strlen(param)-1] == 0x0D)
                    param[strlen(param)-1] = '\0';

                if(!strncmp(key, "color", 5)) {
                    bool color_flug = false;
                    for(int i=0; i<8; ++i)
                        if(!strncmp(param, ansi_colors[i].key, strlen(ansi_colors[i].key))) {
                            color_flug = true;
                            params[params_len-1].color = (char*)malloc(strlen(ansi_colors[i].val)+1);
                            strcpy(params[params_len-1].color, ansi_colors[i].val);
                            break;
                        }
                    if(!color_flug) {
                        if(strlen(param) > 6) {
                            // ""のチェック => error exit
                            char *p;
                            char *param_buf = (char*)malloc(strlen(param)+1);
                            strcpy(param_buf, param);
                            p = param_buf;
                            int i = 0;
                            while(*p) {
                                if(isspace(*p)) {
                                    ++p;
                                    continue;
                                }
                                param_buf[i] = *p;
                                ++i;
                                ++p;
                            }
                            param_buf[i] = '\0';

                            replace(param_buf, "\\033", "\033");
                            replace(param_buf, "\\e",   "\x1B");
                            replace(param_buf, "\\x1b", "\x1b");
                            replace(param_buf, "\\x1B", "\x1B");

                            strcpy(param, param_buf);
                            free(param_buf);

                            params[params_len-1].color = (char*)malloc(strlen(param)+1);
                            strcpy(params[params_len-1].color, param);
                        }
                        else if(strlen(param) == 3) {
                            char format[11+1];  // \033[38;5;XXXm
                            snprintf(format, sizeof(format), "\033[38;5;%3sm", param);
                            params[params_len-1].color = (char*)malloc(strlen(format)+1);
                            strcpy(params[params_len-1].color, format);
                        }
                        else if(strlen(param) == 6) {
                            char hexs[3][2+1] = {
                                { param[0], param[1], '\0' },
                                { param[2], param[3], '\0' },
                                { param[4], param[5], '\0' }};
                            char format[19+1];  // \033[38;2;XXX;XXX;XXXm
                            snprintf(format, sizeof(format), "\033[38;2;%03ld;%03ld;%03ldm",
                            strtol(hexs[0], NULL, 16),
                            strtol(hexs[1], NULL, 16),
                            strtol(hexs[2], NULL, 16));
                            params[params_len-1].color = (char*)malloc(strlen(format)+1);
                            strcpy(params[params_len-1].color, format);
                        } else {
                            error("%serror:%s Invalid color: '%s'", ansi_colors[AC_RED].val, RESET, param);
                            error("  ---> %s:%d", path, line);
                            error("%d | %s", line, str);
                            return EXIT_FAILURE;
                        }
                    }
                }
                else if(!strncmp(key, "regex", 5)) {
                    // ""のチェック => error exit
                    int rc;
                    if((rc = regcomp(&params[params_len-1].regex, param, REG_FLAGS))) {
                        char msg[100];
                        regerror(rc, &params[params_len-1].regex, msg, 100);
                        error("%serror:%s regcomp() failred: %s", ansi_colors[AC_RED].val, RESET, msg);
                        error("  ---> %s:%d", path, line);
                        error("%d | %s", line, str);
                        return EXIT_FAILURE;
                    }
                }
                else {
                    error("%serror:%s Neither color nor regex: '%s'", ansi_colors[AC_RED].val, RESET, key);
                    error("  ---> %s:%d", path, line);
                    error("%d | %s", line, str);
                    return EXIT_FAILURE;
                }

                free(key);
                free(name);
                free(param);
            }
            fclose(cfp);
            free(str);
        }
        free(path);
        //return 0;  // Debug
    }


    if( sPort == NULL && !rflag && !tcpflag ) {
        error("%s: must specify Serial Port\n", argv[0]);
        return EXIT_FAILURE;
    }


    if( B != NULL && !rflag && !tcpflag ) {
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
        else {
          error("(%s) Invalid BaudRate...\n", B);
          return EXIT_FAILURE;
        }
    }


    if( wflag && !rflag ) {
        if(!access(W, F_OK))
            existsflag = true;

        if( existsflag && (access( W, (F_OK | R_OK) ) < 0) ) {
            error("Access to the log file is denied\n");
            return EXIT_FAILURE;
        }

        if( existsflag && !strcmp(mode, "w+") ) {
            printf("\a%s already exists!\n", W);
            printf("Do you want to overwrite?[confirm]");
            char con = getchar();
            if( !(con=='\n' || con=='y' || con=='Y') )
                return EXIT_SUCCESS;
        }

        lf = fopen(W, mode);

        if(lf == NULL) {
            if(access(W, F_OK)) {
              error("Failed to create file\n");
              error("Try to check the permissions\n");
              return EXIT_FAILURE;
            }
            else if( access( W, (F_OK | R_OK) ) < 0 ) {
              error("Access to the log file is denied\n");
              return EXIT_FAILURE;
            }

            error("%s: open (%s): Failure\n", argv[0], W);
            return EXIT_FAILURE;
        }

        logflag = 1;
    }

    struct termios tio;
    struct termios stdio;
    struct termios old_stdio;
    int fd;

    char c             = '0';
    const char endcode = '~';
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
    if( fd < 0 && !rflag && !tcpflag ) {
        tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
        if(access( sPort, F_OK ) < 0)
            error("%s: open (%s): No such file or directory\n", argv[0], sPort);
        else if(access( sPort, (R_OK | W_OK) ) < 0)
            error("%s: open (%s): Permission denied\n", argv[0], sPort);
        // unstable
        //else if(fcntl(fd, F_GETFL) == -1)
        //  printf("%s: %s: Line in use\n", argv[0], sPort);
        else
            error("%s: open (%s): Failure\n", argv[0], sPort);
        close(fd);
        return EXIT_FAILURE;
    }

    if( cfsetispeed(&tio, baudRate) != 0 ) return EXIT_FAILURE;
    if( cfsetospeed(&tio, baudRate) != 0 ) return EXIT_FAILURE;

    if( rflag && !tcpflag ) {
        FILE *fr;
        fr = fopen(R, "r");
        if(fr == NULL) {
            tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
            if(access( R, F_OK ) < 0)
                error("%s: open (%s): No such file or directory\n", argv[0], R);
            else if(access( R, (R_OK) ) < 0)
                error("%s: open (%s): Permission denied\n", argv[0], R);
            else
                error("%s: open (%s): Failure\n", argv[0], R);
            return EXIT_FAILURE;
        }

        //if( setvbuf(stdout, NULL, _IOLBF, 2048) != 0 )
        //{
        //  /* If failure without buffering */
        //}

        tcsetattr(fd, TCSANOW, &tio);

        int i;
        while((i=fgetc(fr)) != EOF) {
            c = (char)i;
            if( 0x07==c || 0x08==c || 0x0a==c || 0x0d==c || (0x1f<c && 0x7f>c) )
                transmission(STDOUT_FILENO, &c, 1);

            if( 0x0a==c ) {
                prflag  = true;
                excflag = false;
                transmission(STDOUT_FILENO, comm, sprintf(comm, "%c%s", 0x0d, RESET));
            }

            if( cflag )
                coloring(c);

            if( cflag && prflag ) {
                if( regexec(&reg_prompt, &c, 0, 0, 0) == 0) {
                    memset( io = s, '\0', MAX_LENGTH );
                    prflag = false;
                }
            }

            if( 0x21==c && cflag) {
                excflag = true;
                transmission(STDOUT_FILENO, comm, sprintf(comm, "\b%s%c", COLOR_COMMENT, c));
            }

            if( excflag )
                memset( io = s, '\0', MAX_LENGTH );

            if(read(STDIN_FILENO, &c, 1) > 0) {
                if(c == endcode) break;
            }
        }

        tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
        fclose(fr);
        printf("\n%s", RESET);

        return EXIT_SUCCESS;
    }

    /* ----------------------------------------------------------------------- */
    if( tcpflag ) {
        // for Telnet
        tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);

        struct sockaddr_in sa;
        int port = 23;

        if( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
            perror("socket error");
            return EXIT_FAILURE;
        }

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = inet_addr(dstaddr);

        if( sa.sin_addr.s_addr == 0xffffffff ) {
            perror("invalid IP Address");
            return EXIT_FAILURE;
        }

        if( connect(fd, (struct sockaddr *)&sa, sizeof(sa)) > 0) {
            perror("connect eror");
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Connected.\n");

        tcsetattr(fd, TCSANOW, &tio);

        ///*
        pid_t pid;
        pid_t p_pid = getpid();
        pid = fork();

        if( 0 > pid ) {
            perror("fork() failure");
            return EXIT_FAILURE;
        } //*/

        //for(;;)
        {
            if( 0 == pid )
            for(;;) {
                //if( recv(fd, &c, 1, MSG_DONTWAIT) > 0 )
                if( recv(fd, &c, 1, 0) > 0 ) {
                    if( 0x07==c || 0x08==c || 0x0a==c || 0x0d==c || (0x1f<c && 0x7f>c) )
                        transmission(STDOUT_FILENO, &c, 1);

                    if( logflag ) {
                        // Unstable
                        if( (int)strlen(logbuf) > lblen ) {
                            lb = logbuf = (char*)realloc(logbuf, sizeof(char) * (lblen += MAX_LENGTH));
                            //free(logbuf);
                        }

                        if( 0x08==c ) {
                            //if( lb != logbuf ) // Sent 0x07 from device
                            lb--;
                        }
                        else if( 0x1f<c && 0x7f>c ) {
                            *lb++ = c;
                        }
                        else if( /*0x0d==c ||*/ 0x0a==c ) {
                            logbuf[strlen(logbuf)] = '\n';
                            //if( 0x0a==c )
                            {
                                if( ts ) {
                                    clock_gettime(CLOCK, &now);
                                    localtime_r(&now.tv_sec, &tm);
                                    sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
                                        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                                        tm.tm_hour, tm.tm_min, tm.tm_sec,
                                        (int) now.tv_nsec / 1000000);
                                    fwrite(date, 1, strlen(date), lf);
                                }

                                fwrite(logbuf, 1, strlen(logbuf), lf);

                                // ToDo
                                if( lblen > MAX_LENGTH - 2 ) {
                                    lblen = MAX_LENGTH - 2;
                                    free(logbuf);
                                    //lb = logbuf = (char *)realloc(
                                    //    logbuf, sizeof(char) * (MAX_LENGTH));
                                    lb = logbuf = (char*)malloc(MAX_LENGTH);
                                }
                                memset( lb = logbuf, '\0', MAX_LENGTH );
                            }
                        }
                    }

                    if( 0x0a==c ) {
                        prflag  = true;
                        excflag = false;
                        transmission(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
                    }

                    if( 0x21==c && cflag && !excflag ) {
                        comlen = 0;
                        excflag = true;
                        transmission(STDOUT_FILENO, comm, sprintf(comm, "\b%s%c", COLOR_COMMENT, c));
                    }

                    if( excflag && 0x07!=c )
                        comlen++;

                    if( 0x08==c ) {
                        if( excflag )
                            comlen-=2;
                        if( excflag && 0>=comlen ) {
                            transmission(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
                            io++;
                            excflag = false;
                        }
                    }

                    if( !excflag && cflag )
                        coloring(c);

                    if( cflag && prflag ) {
                        if( regexec(&reg_prompt, &c, 0, 0, 0) == 0 ) {
                            memset( io = s, '\0', MAX_LENGTH );
                            prflag = false;
                        }
                    }
                    //DebugLog("[%s]", s);
                }
                else if( recv(fd, &c, 1, 0) == 0) {
                    kill(p_pid, SIGINT);
                    break;  // hang up
                }

                if( kbhit() ) {
                    //DebugLog("\b");
                }
            }

            if( 0 != pid )
            for(;;) {
                if( kbhit() )
        //  if(read(STDIN_FILENO, &c, 1) > 0)
                {
                    c = getchar();
                    if( 0x1b==c )                          escflag = true;  // ^
                    else if( escflag && 0x5b==c )          spflag  = true;  // ^[
                    else if( spflag  && 0x33==c )          tilflag = true;  // ^[3
                    else if( spflag  && 0x40<c && 0x45>c ) arrflag = true;  // ^[[ABCD]
                    else if( tilflag && 0x7e==c ) {                         // ^[3~
                        c = 0x7f;
                        escflag = spflag = tilflag = false;
                    }
                    else {
                        escflag = spflag = false;
                    }

                    if( endcode == c ) {
                        //kill(p_pid, SIGINT);
                        kill(pid, SIGINT);
                        break;  // hang up
                    }

                    if( 0x00 == c )
                        c = 0x7f;  // BS on Vimterminal

                    //DebugLog("[0x%02x]", c);
                    if( !escflag )
                        send(fd, &c, 1, 0);
                    if( arrflag ) {
                        char* arrow = (char*)malloc(4);
                        sprintf(arrow, "%c%c%c", 0x1b, 0x5b, c);
                        send(fd, arrow, 3, 0);
                        free(arrow);
                        arrflag = false;
                    }
                }

                // 100 microsecond
                usleep(100);
            }
          //*/
        }

        if(logflag) {
            if( ts ) {
                clock_gettime(CLOCK, &now);
                localtime_r(&now.tv_sec, &tm);
                sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
                    tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                    (int) now.tv_nsec / 1000000);
                fwrite(date, 1, strlen(date), lf);
            }
            char loglast[strlen(logbuf)+1];
            sprintf(loglast, "%s%c", logbuf, 0x0a);
            fwrite(loglast, 1, strlen(loglast), lf);
            fclose(lf);
        }

        tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
        printf("%s\nDisconnected.\n", RESET);
        close(fd);
        return EXIT_SUCCESS;
    }
    /* ----------------------------------------------------------------------- */

    printf("Connected.\n");

    tcsetattr(fd, TCSANOW, &tio);

    for(;;) {
        // if new data is available on the serial port, print it out
        // ToDo Parallel processing
        if(read(fd, &c, 1) > 0) {
            if( 0x07==c || 0x08==c || 0x0a==c || 0x0d==c || (0x1f<c && 0x7f>c) )
                transmission(STDOUT_FILENO, &c, 1);
          //DebugLog("[%02x]", c);

            if( logflag ) {
                // Unstable
                if( (int)strlen(logbuf) > lblen ) {
                    lb = logbuf = (char*)realloc(logbuf, sizeof(char) * (lblen += MAX_LENGTH));
                    if(lb == NULL) {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                }

                if( 0x08==c ) {
                    //if( lb != logbuf ) // Sent 0x07 from device
                    lb--;
                }
                else if( 0x1f<c && 0x7f>c ) {
                  *lb++ = c;
                }
                else if( /*0x0d==c ||*/ 0x0a==c ) {
                    logbuf[strlen(logbuf)] = '\n';
                    //if( 0x0a==c )
                    {
                        if( ts ) {
                            clock_gettime(CLOCK, &now);
                            localtime_r(&now.tv_sec, &tm);
                            sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
                                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                                tm.tm_hour, tm.tm_min, tm.tm_sec,
                                (int) now.tv_nsec / 1000000);
                            fwrite(date, 1, strlen(date), lf);
                        }

                        fwrite(logbuf, 1, strlen(logbuf), lf);

                        // ToDo
                        if( lblen > MAX_LENGTH - 2 ) {
                            lblen = MAX_LENGTH - 2;
                            free(logbuf);
                            //lb = logbuf = (char *)realloc(
                            //    logbuf, sizeof(char) * (MAX_LENGTH));
                            lb = logbuf = (char*)malloc(MAX_LENGTH);
                        }
                        memset( lb = logbuf, '\0', MAX_LENGTH );
                    }
                }
            }

            if( 0x0a==c ) {
                prflag  = true;
                excflag = false;
                transmission(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
            }

            if( 0x21==c && cflag && !excflag ) {
                comlen = 0;
                excflag = true;
                transmission(STDOUT_FILENO, comm, sprintf(comm, "\b%s%c", COLOR_COMMENT, c));
            }

            if( excflag && 0x07!=c )
                comlen++;

            if( 0x07==c )
                bsflag = false;

            if( 0x08==c ) {
                if( excflag )
                    comlen-=2;
                if( excflag && 0>=comlen ) {
                    transmission(STDOUT_FILENO, comm, sprintf(comm, "%s", RESET));
                    //memset( io = s, '\0', MAX_LENGTH );
                    io++;
                    excflag = false;
                }
            }

            if( !excflag && cflag )
                coloring(c);

            if( cflag && prflag ) {
                if( regexec(&reg_prompt, &c, 0, 0, 0) == 0 ) {
                    memset( io = s, '\0', MAX_LENGTH );
                    prflag = false;
                }
            }
            //DebugLog("[%s]", s);
        }

        // if new data is available on the console, send it to the serial port
        if(read(STDIN_FILENO, &c, 1) > 0) {
            if( 0x1b==c )                 escflag = true; // ^
            else if( 0x5b==c && escflag ) spflag  = true; // ^[
            else if( 0x33==c && spflag )  tilflag = true; // ^[3
            else if( 0x7e==c && tilflag ) {               // ^[3~
                c = 0x7f;
                escflag = spflag = tilflag = false;
            }
            else {
                escflag = spflag = false;
            }

            if( endcode == c )                   break; // hang up
            if( 0x00 == c )                  c = 0x7f;  // BS on Vimterminal

            if( 0x7f == c )             bsflag = 3;     // BS on Vimterminal

            //if( '$' == c ) DebugLog("[lblen:%d]", lblen);
            //DebugLog("[0x%02x]", c);
            transmission(fd, &c, 1);
        }

        // 100 microsecond
        usleep(100);
    }

    close(fd);

    if(logflag) {
        if( ts ) {
            clock_gettime(CLOCK, &now);
            localtime_r(&now.tv_sec, &tm);
            sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                (int) now.tv_nsec / 1000000);
            fwrite(date, 1, strlen(date), lf);
        }
        char loglast[strlen(logbuf)+1];
        sprintf(loglast, "%s%c", logbuf, 0x0a);
        fwrite(loglast, 1, strlen(loglast), lf);
        fclose(lf);
    }

    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
    printf("%s\nDisconnected.\n", RESET);

    return EXIT_SUCCESS;
}


void transmission(int _fd, const void* _buf, size_t _len) {
    if( -1 == write(_fd, _buf, _len) ) {
        perror("write() error");
        //exit(EXIT_FAILURE);
    }
}


int kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void regcompAll() {
    //|| regcomp(&reg_slash    , "/$" , REG_FLAGS )) {
    if(regcomp(&reg_prompt   , "#|>", REG_FLAGS )) {
        error("regcomp: error");
        exit(EXIT_FAILURE);
    }
}

void replace(char *str, const char *before, const char *after) {
    char *p;
    while((p = strstr(str, before)) != NULL) {
        *p = '\0';
        p += (int)strlen(before);
        strcat(str, after);
        strcat(str, p);
    }
}

int syntaxCheck(char *str) {
    //if( regexec(&reg_slash, str, 0, 0, 0) == 0 )
    //    return HL_SLASH;
    int hi_num;
    for(hi_num = 0; hi_num < params_len; ++hi_num)
        if( regexec(&params[hi_num].regex, str, 0, 0, 0) == 0 )
            return hi_num;
    return -1;
}

void repaint(const char *color) {
    io = s;
    int  i  = 0;
    char bs = 0x08;
    char tmp[MAX_LENGTH];
    char str[MAX_LENGTH + 32];
    while(*io) {
        tmp[i++] = *io++;
        transmission(STDOUT_FILENO, &bs, 1);
    }
    if(tmp[i]!='\0') {
        tmp[i]='\0';
    }
    transmission(STDOUT_FILENO, str, sprintf(str, "%s%s%s", color, tmp, RESET));
}

void coloring(char c) {
    if( (/*0x08!=c &&*/ 0x21>c && !bsflag) ) {
        memset( io = s, '\0', MAX_LENGTH );
        return;
    }

    //en route
    if( bsflag ) {
        if( 2==bsflag-- /*&& '\0'!=s[0]*/)
            *io++ = '\0';
            //*io++ = 0x20;
        else
            io--;

        if( bsflag )
            return;
    }
    /*else if( 0x08==c ) {
        io--;
        if( '\0'==s )
            memset( io = s, '\0', MAX_LENGTH );
        return;
    } //*/
    else if( strlen(s) < MAX_LENGTH - 1 ) {
        *io++ = c;
    }
    else {
        memset( io = s, '\0', MAX_LENGTH );
        return;
    }

    int checked = syntaxCheck(s);
    if(checked >= 0)
        repaint(params[checked].color);

    //        case HL_SLASH:
    //            sprintf(s, "/");
    //            repaint(COLOR_SLASH);
    //            memset( io = s, '\0', sizeof(s) );
    //            break;

}

void setSignal(int p_signame) {
    if( signal(p_signame, sigcatch) == SIG_ERR ) {
        perror("SIG_ERR");
        exit(EXIT_FAILURE);
    }
}

void sigcatch() {
    exit(EXIT_SUCCESS);
}

void version() {
    printf("%s (%s) %s %s\n", COMMAND_NAME, PROGRAM_NAME, VERSION, UPDATE_DATE);
}

void usage(char *argv[]) {
    printf("Usage: %s [-l SERIAL_PORT] [-s BAUDRATE] [-r /path/to/file]\n"
           "            [-w /path/to/LOG] [-c /path/to/config] [-t] [-a] [-n] [-h] [-v]\n\n", argv[0]);

    printf("Command line interface for Serial Console by Network device.\n");
    printf("------------------------------------------------------------\n");
    printf("https://github.com/yorimoi/sisterm\n\n");

    printf("Options:\n");
    printf("  -h,--help     Show this help message and exit\n");
    printf("  -v,--version  Show %s version and exit\n", PROGRAM_NAME);
    printf("  -l port       Use named device   (e.g. /dev/ttyS0)\n");
    printf("  -s speed      Use given speed    (default 9600)\n");
    printf("  -r path       Output log file    (e.g. /tmp/config.txt)\n");
    printf("  -w path       Saved log          (e.g. /tmp/sist.log)\n");
    printf("  -t            Add timestamp to log\n");
    printf("  -a            Append to log      (default overwrite)\n");
    printf("  -n            Without color\n\n");
    printf("  -c path       Specification of config file (e.g. /tmp/for_cisco.conf)\n");
    printf("  -p IPAddress  Telnet !!!Beta version!!! Many bugs!\n\n");
    // rear printf("  -p portnumber [IPAddress]  \n");

    printf("Commands:\n");
    printf("  ~           Terminate the conversation\n");
}

