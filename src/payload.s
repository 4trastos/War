section .text
global _ft_payload

_ft_payload:
    jmp     code_start
    
data_start:
new_vaddr:      dq 0xDEADBEEF
e_entry_orig:   dq 0xDEADBEEF

code_start:
    call    next
next:
    pop     rbx
    sub     rbx, 0x17       ; 0x17 = next - _ft_payload

    mov     r12, [rbx + 2]
    mov     r15, [rbx + 10]

    mov     rax, rbx
    sub     rax, r12
    add     rax, r15
    jmp     rax