#include "../incl/war.h"

int ft_couter(char *str, int *count)
{
    int i = 0;
    int len = 0;

    while (str[i] != '\0')
    {
        if (str[i] >= '0' && str[i] <= '9')
        {
            *count = i;
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

void    ft_replace_marker(unsigned char *payload, unsigned int payload_len, unsigned char *marker, uint64_t value)
{
    unsigned int    i;

    i = 0;
    while (i < payload_len - 4)
    {
        if (memcmp(&payload[i], marker, 4) == 0)
        {
            memcpy(&payload[i], &value, sizeof(uint64_t));
            return;
        }
        i++;
    }
}

