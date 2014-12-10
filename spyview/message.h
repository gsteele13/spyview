#ifndef __message_h__
#define __message_h__

void error(const char *str, ...) __attribute__ ((format (printf,1,2), noreturn)); 
void warn(const char *str, ...) __attribute__ ((format (printf,1,2)));  
void info(const char *str, ...) __attribute__ ((format (printf,1,2)));  

#endif
