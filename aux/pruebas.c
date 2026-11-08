#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: intenta abrir el ejecutable en O_RDWR
        int fd = open("/tmp/test/mi_binario", O_RDWR);
        if (fd < 0) perror("open hijo");
        else { printf("open hijo OK\n"); close(fd); }
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    return 0;
}