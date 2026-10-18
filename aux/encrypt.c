#include "../incl/war.h"

void    ft_decrypt(char *signature)
{
    char            buffer[4096];
    int             fd;
    int             bytes_read;
    int             i = 0;
    unsigned int    value;

    fd = open(signature, O_RDONLY);
    if (fd == -1)
        return;
    
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';

    while (buffer[i] != '\0')
    {
        if (buffer[i] == '0' && buffer[i + 1] == 'x')
        {
            sscanf(&buffer[i], "0x%02x", &value);
            printf("%c", (char)value ^ KEY);
            i += 4;
            while (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == ';')
                i++;
        }
        else
            i++;
    }
    printf("\n");
    close(fd);

    return;
}

int main(int argc, char **argv)
{
    int     i = 0;
    char    *str = "War version 1.0 (c)oded Oct - 2026 by davgalle - [FINGERPRINT]";

    printf("Arguemntos: ( %d )\n", argc);

    if (argc == 2)
    {
        printf("Dentro\n");
        ft_decrypt(argv[1]);
        return(0);
    }

    while (str[i] != '\0')
    {
        printf("0x%02x", (unsigned char)str[i] ^ KEY);
        if (str[i] != '\0')
            printf(", ");
        i++;
    }
    printf("0x00;\n");
    return (0);
}