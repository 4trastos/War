#include "../incl/war.h"

int ft_strlen(char *str)
{
    int i = 0;
    while (str[i] != '\0')
        i++;
    return (i);
}

int ft_couter(char *str, int offset)
{
    int len = 0;
    
    while (str[offset] != '\0')
    {
        len++;
        offset++;
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

