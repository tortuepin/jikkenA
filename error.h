#ifndef __INCLUDED__
#define __INCLUDED__



typedef enum ErrorMessageNo{
    ERR_MSG_CREATE = 0,
    ERR_MSG_UNLINK = 1,
    ERR_MSG_OPEN = 2,
    ERR_MSG_CLOSE = 3,
    ERR_MSG_READ = 4,
    ERR_MSG_WRITE = 5,
    ERR_MSG_LSEEK = 6,
    ERR_MSG_ACCESS = 7,
    ERR_MSG_STAT = 8,
    ERR_MSG_MALLOC = 9,
}ErrorMessageNo;


extern void printErrorMessage(ErrorMessageNo messageNo, const char *funcName, int lineNum);

#endif
