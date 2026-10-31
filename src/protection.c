#include "../incl/war.h"

void    ft_antiprocess(t_war *war)
{
    char            buffer[1024];
    char            path[1024];
    int             fd;
    DIR             *proc;
    struct dirent   *entry;

    proc = opendir("/proc");
    if (!proc)
        return;

    while ((entry = readdir(proc)) != NULL && war->antivirus != 1)
    {
        if (entry->d_name[0] < '1' || entry->d_name[0] > '9')
            continue;
        snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        fd = open(path, O_RDONLY);
        if (fd == -1)
            continue;
        memset(buffer, 0, sizeof(buffer));
        read(fd, buffer, sizeof(buffer) -1);
        close(fd);
        if (strcmp("./test", buffer) == 0)
            war->antivirus = 1;
    }
    closedir(proc);
}
