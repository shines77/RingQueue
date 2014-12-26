
#include "get_char.h"

#include <stdio.h>

#if defined(__MINGW32__)
#include <conio.h>
#elif defined(__linux__) || defined(__GNUC__)
#include <termios.h>
#endif  /* __MINGW32__ */

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <conio.h>
#endif  /* _MSC_VER */

#if defined(__MINGW32__)

/* Read 1 character without echo */
int jimi_getch(void)
{
    return _getch();
}

/* Read 1 character with echo */
int jimi_getche(void)
{
    int ch = _getch();
    if (ch != EOF)
        printf("%c", (char)ch);
    else
        printf("EOF: (%d)", ch);
    return ch;
}

#elif defined(__linux__) || defined(__GNUC__)

/// <comment>
///
/// What is equivalent to getch() & getche() in Linux?
///
/// From: http://stackoverflow.com/questions/7469139/what-is-equivalent-to-getch-getche-in-linux
///
/// </comment>

static struct termios s_term_old, s_term_new;

/* Initialize new terminal i/o settings */
void init_terminal_os(int echo)
{
    tcgetattr(0, &s_term_old);                  /* grab old terminal i/o settings */
    s_term_new = s_term_old;                    /* make new settings same as old settings */
    s_term_new.c_lflag &= ~ICANON;              /* disable buffered i/o */
    s_term_new.c_lflag &= echo ? ECHO : ~ECHO;  /* set echo mode */
    tcsetattr(0, TCSANOW, &s_term_new);         /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void reset_terminal_os(void)
{
    tcsetattr(0, TCSANOW, &s_term_old);
}

/* Read 1 character - echo defines echo mode */
int jimi_getch_term(int echo)
{
    int ch;
    init_terminal_os(echo);
    ch = getchar();
    reset_terminal_os();
    return ch;
}

/* Read 1 character without echo */
int jimi_getch(void)
{
    return jimi_getch_term(0);
}

/* Read 1 character with echo */
int jimi_getche(void)
{
    return jimi_getch_term(1);
}

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

/* Read 1 character without echo */
int jimi_getch(void)
{
    return _getch();
}

/* Read 1 character with echo */
int jimi_getche(void)
{
    int ch = _getch();
    if (ch != EOF)
        printf("%c", (char)ch);
    else
        printf("EOF: (%d)", ch);
    return ch;
}

#else  /* other unknown os */

/* Read 1 character without echo */
int jimi_getch(void)
{
    return (int)-1;
}

/* Read 1 character with echo */
int jimi_getche(void)
{
    return (int)-1;
}

#endif  /* __linux__ */
