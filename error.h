void error_exit(int signo) {
    printf("Program exit in the running, with error:%d\n", signo);
    exit(signo);
}

void errexit(int signo, std::string errinfo) {
    std::cout << errinfo;
    raise(signo);
}
