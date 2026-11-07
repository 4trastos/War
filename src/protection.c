#include "../incl/war.h"

// void    ft_antiprocess(t_war *war)
// {
//     char            buffer[1024];
//     char            path[1024];
//     int             fd;
//     DIR             *proc;
//     struct dirent   *entry;
//     char            *p;

//     proc = opendir("/proc");
//     if (!proc)
//         return;

//     while ((entry = readdir(proc)) != NULL && war->antivirus != 1)
//     {
//         // Ignorar entradas que no sean PIDs numéricos
//         if (entry->d_name[0] < '1' || entry->d_name[0] > '9')
//             continue;

//         snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
//         fd = open(path, O_RDONLY);
//         if (fd == -1)
//             continue;

//         memset(buffer, 0, sizeof(buffer));
//         ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
//         close(fd);

//         if (bytes_read <= 0)
//             continue;

//         // Como cmdline usa '\0' entre argumentos, nos quedamos solo con el primer argumento (el binario)
//         // Y por si viene con ruta (ej: /tmp/test/test o ./test), extraemos solo el nombre base
//         p = buffer;
//         while (*p)
//             p++;
//         // Si hay argumentos adicionales, el primer argumento termina en el primer '\0'.
//         // Buscamos el último '/' para aislar el nombre del ejecutable (basename)
//         char *base_name = strrchr(buffer, '/');
//         if (base_name)
//             base_name++; // Saltar la barra
//         else
//             base_name = buffer; // Si no tiene barra, es el buffer entero

//         // Comparamos el nombre base del proceso con el que buscamos (ej. "test")
//         if (strcmp("test", base_name) == 0)
//         {
//             war->antivirus = 1;
//             break;
//         }
//     }
//     closedir(proc);
// }

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
