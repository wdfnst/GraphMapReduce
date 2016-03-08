/*************************************************************************/
/*! 此文件主要用来定义与信号操作相关的函数和变常量
*/
/**************************************************************************/
#define SIGERR                  SIGTERM

/* 判断是否在控制台打印调试信息 */
#define INFO   true 
#define DEBUG  true 

void error_exit(int signo) {
    printf("Program exit in the running, with error:%d\n", signo);
    exit(signo);
}

void errexit(int signo, std::string errinfo) {
    std::cout << errinfo;
    raise(signo);
}
