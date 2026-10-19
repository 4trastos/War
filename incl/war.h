#ifndef WAR_H
# define WAR_H

# include <string.h>
# include <unistd.h>
# include <stdlib.h>
# include <stdio.h>
# include <stdint.h>
# include <elf.h>
# include <dirent.h>
# include <ctype.h>
# include <errno.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/ptrace.h>

# define KEY 0x42

typedef struct s_war
{
    int             fd;
    int             file_size;
    int             antivirus;
    char            *strtab;
    char            *decrypted;
    const char      *binary;
    unsigned char   *elf;
    void            *signd;
    Elf64_Ehdr      *header;
    Elf64_Shdr      *sectab;
    struct stat     file_info;
    DIR             *dir;
    pid_t           pid;
}   t_war;

/*  FUNCTIONS   */

//int     main(void);
void    ft_antiprocess(t_war *war);

#endif