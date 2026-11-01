#include "../incl/war.h"

void    ft_update_payload(t_war *war, char *str)
{
    off_t   offset;
    int     fd;
    char    *ptr;

    offset = (unsigned char *)war->signd - war->elf;
    ptr = strchr((char *)war->signd, 'P');
    if (!ptr)
        return;
    ptr++;
    offset += (ptr - (char * )war->signd);
    
    fd = open(war->binary, O_RDWR);
    if (fd == -1)
        return;
    if (lseek(fd, offset, SEEK_SET) == -1)
    {
        close(fd);
        return;
    }
    write(fd, str, strlen(str));
    close(fd);
    return;
}

int ft_couter(char *str)
{
    int i = 0;
    int len = 0;

    while (str[i] != '\0')
    {
        if (str[i] == 'P')
        {
            i++;
            while (str[i] != '\0')
            {
                len++;
                i++;
            }
            return (len);
        }
        i++;
    }
    return (len);
}