#include "../incl/war.h"

int main()
{
    char *str = "Hola mundo!";
    for (int i = 0; i < strlen(str); i++)
    {
        write(1,&str[i], 1);
    }
    write(1, "\n", 1);
    getchar();
    return (0);
}