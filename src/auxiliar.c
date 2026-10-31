#include "../incl/war.h"

void    *ft_update_payload(t_war *war, char *str)
{
    (void)war;
    (void)str;
    return (NULL);
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