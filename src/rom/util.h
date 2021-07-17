#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

int  isdigit(int c);
void lputc (char c);
void lputs (const char *s);
void lprintf(const char *format, ...);

void emu_stop(void);

#endif
