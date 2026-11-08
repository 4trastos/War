# Cómo el kernel ejecuta un binario ELF64

## 1. El file header (`Elf64_Ehdr`)

**Comando:**

```bash
readelf -h /tmp/test/mi_binario
```

**Salida (binario no infectado):**

```
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x1080
  Start of program headers:          64 (bytes into file)
  Start of section headers:          14016 (bytes into file)
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         14
  Size of section headers:           64 (bytes)
  Number of section headers:         31
  Section header string table index: 30
```

**Lo que hace el kernel:**

1. **Lee los primeros 64 bytes del fichero (el `Elf64_Ehdr`).**
2. **Verifica el magic `7f 45 4c 46` (`\x7fELF`).** **Si no coincide, devuelve `ENOEXEC`.**
3. **Comprueba `e_ident[EI_CLASS]`:** **`2` = ELF64.**
4. **Comprueba `e_ident[EI_DATA]`:** **`1` = little-endian.**
5. **Comprueba `e_machine`:** **`0x3e` = x86-64.**
6. **Lee `e_type`:** **`DYN` = PIE.**
7. **Lee `e_entry = 0x1080`:** **el offset del `_start`.**
8. **Lee `e_phoff = 64`:** **la tabla de `Phdr` empieza en el offset 64.**
9. **Lee `e_phentsize = 56`:** **cada `Phdr` mide 56 bytes.**
10. **Lee `e_phnum = 14`:** **hay 14 segmentos.**

**Interpretación:**

- **`e_entry = 0x1080`:** **en PIE, el kernel salta a `base + 0x1080`.** **`base` es la dirección aleatoria de carga (ASLR).**
- **`e_phoff = 64`:** **la tabla de `Phdr` está justo después del file header.**
- **`e_phnum = 14`:** **14 segmentos en total, pero solo 4 son `PT_LOAD` (los que el kernel mapea).**

---

## 2. La tabla de program headers (`Elf64_Phdr`)

**Comando:**

```bash
readelf -lW /tmp/test/mi_binario
```

**Salida (resumida):**

```
Elf file type is DYN (Position-Independent Executable file)
Entry point 0x1080
There are 14 program headers, starting at offset 64

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000000040 0x0000000000000040 0x000310 0x000310 R   0x8
  INTERP         0x000374 0x0000000000000374 0x0000000000000374 0x00001c 0x00001c R   0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x000640 0x000640 R   0x1000
  LOAD           0x001000 0x0000000000001000 0x0000000000001000 0x0001f5 0x0001f5 R E 0x1000
  LOAD           0x002000 0x0000000000002000 0x0000000000002000 0x000148 0x000148 R   0x1000
  LOAD           0x002db0 0x0000000000003db0 0x0000000000003db0 0x000260 0x000268 RW  0x1000
  DYNAMIC        0x002dc0 0x0000000000003dc0 0x0000000000003dc0 0x0001f0 0x0001f0 RW  0x8
  NOTE           0x000350 0x0000000000000350 0x0000000000000350 0x000024 0x000024 R   0x4
  NOTE           0x0020f8 0x00000000000020f8 0x00000000000020f8 0x000030 0x000030 R   0x8
  NOTE           0x002128 0x0000000000002128 0x0000000000002128 0x000020 0x000020 R   0x4
  GNU_PROPERTY   0x0020f8 0x00000000000020f8 0x00000000000020f8 0x000030 0x000030 R   0x8
  GNU_EH_FRAME   0x002014 0x0000000000002014 0x0000000000002014 0x000034 0x000034 R   0x4
  GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0x10
  GNU_RELRO      0x002db0 0x0000000000003db0 0x0000000000003db0 0x000250 0x000250 R   0x1
```

**Lo que hace el kernel:**

1. **Lee los 14 `Phdr` desde `e_phoff = 64`.**
2. **Para cada uno, comprueba `p_type`:**
   - **`PT_LOAD` (`1`):** **lo mapea en memoria.**
   - **`PT_INTERP` (`3`):** **lee la ruta del loader dinámico.**
   - **`PT_DYNAMIC` (`2`):** **lo procesa el loader (no el kernel).**
   - **`PT_NOTE` (`4`):** **lo ignora (solo el linker lo usa).**
   - **`PT_PHDR` (`6`):** **la propia tabla de `Phdr`.**
   - **`PT_GNU_*`:** **los ignora (o los procesa el loader).**

**Los 4 `PT_LOAD` que el kernel mapea:**

| `p_offset` | `p_vaddr` | `p_filesz` | `p_memsz` | `p_flags` | `p_align` | Sección |
|---|---|---|---|---|---|---|
| `0x000000` | `0x000000` | `0x000640` | `0x000640` | `R` | `0x1000` | `.interp`, `.note`, `.gnu.hash`, `.dynsym`, `.dynstr`, `.rela` |
| `0x001000` | `0x001000` | `0x0001f5` | `0x0001f5` | `R E` | `0x1000` | `.init`, `.plt`, `.text`, `.fini` |
| `0x002000` | `0x002000` | `0x000148` | `0x000148` | `R` | `0x1000` | `.rodata`, `.eh_frame` |
| `0x002db0` | `0x003db0` | `0x000260` | `0x000268` | `RW` | `0x1000` | `.init_array`, `.dynamic`, `.got`, `.data`, `.bss` |

**Interpretación:**

- **Cada `PT_LOAD` cubre un rango de páginas.** **El kernel los mapea en `base + p_vaddr`.**
- **`p_offset` y `p_vaddr` son congruentes módulo `p_align` (`0x1000`):** **`0x1000 % 0x1000 == 0x1000 % 0x1000 == 0`.**
- **`p_filesz <= p_memsz`:** **el `.bss` se rellena con ceros (`p_memsz - p_filesz = 0x268 - 0x260 = 8`).**

---

## 3. El `PT_LOAD` ejecutable (donde está el `_start`)

**Comando:**

```bash
readelf -lW /tmp/test/mi_binario | grep "LOAD.*R E"
```

**Salida:**

```
  LOAD           0x001000 0x0000000000001000 0x0000000000001000 0x0001f5 0x0001f5 R E 0x1000
```

**Interpretación:**

- **`p_offset = 0x1000`:** **el segmento empieza en el offset `0x1000` del fichero.**
- **`p_vaddr = 0x1000`:** **la dirección virtual es `0x1000` (offset PIE).**
- **`p_filesz = 0x1f5`:** **el segmento mide `0x1f5` bytes en el fichero.**
- **`p_memsz = 0x1f5`:** **en memoria también (no hay `.bss` en este segmento).**
- **`p_flags = R E`:** **read + execute.**
- **`p_align = 0x1000`:** **alineado a página.**

**El `_start` (`e_entry = 0x1080`) está dentro de este segmento (`0x1000`-`0x11F5`).** **El kernel mapea este segmento en `base + 0x1000`, y salta a `base + 0x1080`.**

---

## 4. El `PT_INTERP` (el loader dinámico)

**Comando:**

```bash
readelf -lW /tmp/test/mi_binario | grep INTERP
```

**Salida:**

```
  INTERP         0x000374 0x0000000000000374 0x0000000000000374 0x00001c 0x00001c R   0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
```

**Interpretación:**

- **`p_offset = 0x374`:** **la ruta del intérprete está en el offset `0x374` del fichero.**
- **`p_filesz = 0x1c`:** **la ruta mide `0x1c` bytes (`/lib64/ld-linux-x86-64.so.2`).**
- **`p_flags = R`:** **read-only.**

**Lo que hace el kernel:**

1. **Lee la ruta del intérprete desde el fichero (offset `p_offset`, longitud `p_filesz`).**
2. **Carga el intérprete (`/lib64/ld-linux-x86-64.so.2`) en memoria.**
3. **Le pasa el control al intérprete, con el binario original como argumento.**

**El intérprete (loader dinámico) hace:**

1. **Carga las librerías necesarias (`libc.so.6`).**
2. **Procesa la `.dynamic` (relocalizaciones, símbolos, etc.).**
3. **Inicializa el `.init_array` (constructores).**
4. **Salta al `_start` del binario original (`base + 0x1080`).**

---

## 5. La `.dynamic` (las relocalizaciones)

**Comando:**

```bash
readelf -d /tmp/test/mi_binario
```

**Salida:**

```
Dynamic section at offset 0x2dc0 contains 27 entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000000c (INIT)               0x1000
 0x000000000000000d (FINI)               0x11e8
 0x0000000000000019 (INIT_ARRAY)         0x3db0
 0x000000000000001b (INIT_ARRAYSZ)       8 (bytes)
 0x000000000000001a (FINI_ARRAY)         0x3db8
 0x000000000000001c (FINI_ARRAYSZ)       8 (bytes)
 0x000000006ffffef5 (GNU_HASH)           0x390
 0x0000000000000005 (STRTAB)             0x478
 0x0000000000000006 (SYMTAB)             0x3b8
 0x000000000000000a (STRSZ)              149 (bytes)
 0x000000000000000b (SYMENT)             24 (bytes)
 0x0000000000000015 (DEBUG)              0x0
 0x0000000000000003 (PLTGOT)             0x3fb0
 0x0000000000000002 (PLTRELSZ)           48 (bytes)
 0x0000000000000014 (PLTREL)             RELA
 0x0000000000000017 (JMPREL)             0x610
 0x0000000000000007 (RELA)               0x550
 0x0000000000000008 (RELASZ)             192 (bytes)
 0x0000000000000009 (RELAENT)            24 (bytes)
 0x000000000000001e (FLAGS)              BIND_NOW
 0x000000006ffffffb (FLAGS_1)            Flags: NOW PIE
 0x000000006ffffffe (VERNEED)            0x520
 0x000000006fffffff (VERNEEDNUM)         1
 0x000000006ffffff0 (VERSYM)             0x50e
 0x000000006ffffff9 (RELACOUNT)          3
 0x0000000000000000 (NULL)               0x0
```

**Interpretación:**

- **`NEEDED = libc.so.6`:** **el binario necesita `libc.so.6`.**
- **`INIT = 0x1000` / `FINI = 0x11e8`:** **funciones de inicialización/finalización.**
- **`INIT_ARRAY = 0x3db0` / `FINI_ARRAY = 0x3db8`:** **constructores/destructores.**
- **`STRTAB = 0x478` / `SYMTAB = 0x3b8`:** **tablas de strings y símbolos.**
- **`PLTGOT = 0x3fb0` / `JMPREL = 0x610` / `RELA = 0x550`:** **relocalizaciones.**

**Lo que hace el loader:**

1. **Carga `libc.so.6` en memoria.**
2. **Procesa la `.dynamic` para resolver los símbolos (`printf`, `exit`, etc.).**
3. **Aplica las relocalizaciones en `.got` / `.got.plt`.**
4. **Llama a los constructores de `.init_array`.**
5. **Salta al `_start` del binario.**

---

## 6. El `_start` (el código del punto de entrada)

**Comando:**

```bash
objdump -d --start-address=0x1080 --stop-address=0x10a6 /tmp/test/mi_binario
```

**Salida:**

```
0000000000001080 <_start>:
    1080:	f3 0f 1e fa          	endbr64
    1084:	31 ed                	xor    %ebp,%ebp
    1086:	49 89 d1             	mov    %rdx,%r9
    1089:	5e                   	pop    %rsi
    108a:	48 89 e2             	mov    %rsp,%rdx
    108d:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
    1091:	50                   	push   %rax
    1092:	54                   	push   %rsp
    1093:	45 31 c0             	xor    %r8d,%r8d
    1096:	31 c9                	xor    %ecx,%ecx
    1098:	48 8d 3d ca 00 00 00 	lea    0xca(%rip),%rdi        # 1169 <main>
    109f:	ff 15 33 2f 00 00    	call   *0x2f33(%rip)        # 3fd8 <__libc_start_main@GLIBC_2.34>
    10a5:	f4                   	hlt
```

**Interpretación paso a paso:**

| Instrucción | Bytes | Qué hace |
|---|---|---|
| `endbr64` | `f3 0f 1e fa` | **Instrucción de CET (Control-flow Enforcement Technology).** **Marca el inicio de una función válida.** |
| `xor %ebp,%ebp` | `31 ed` | **Pone `rbp` a 0.** |
| `mov %rdx,%r9` | `49 89 d1` | **Guarda `rtld_fini` (que el kernel puso en `rdx`) en `r9`.** |
| `pop %rsi` | `5e` | **Saca `argc` del stack.** |
| `mov %rsp,%rdx` | `48 89 e2` | **`argv` está en `rsp` (después del `pop`).** |
| `and $0xfffffffffffffff0,%rsp` | `48 83 e4 f0` | **Alinea `rsp` a 16 bytes.** |
| `push %rax` | `50` | **Pasa `stack_end` a `__libc_start_main`.** |
| `push %rsp` | `54` | **Pasa `stack_end` (el valor alineado).** |
| `xor %r8d,%r8d` | `45 31 c0` | **`fini = NULL`.** |
| `xor %ecx,%ecx` | `31 c9` | **`init = NULL`.** |
| `lea 0xca(%rip),%rdi` | `48 8d 3d ca 00 00 00` | **Carga la dirección de `main` (`0x1169`) en `rdi`.** |
| `call *0x2f33(%rip)` | `ff 15 33 2f 00 00` | **Llama a `__libc_start_main@GLIBC_2.34`.** |
| `hlt` | `f4` | **Si `__libc_start_main` retorna (no debería), `hlt` detiene la CPU.** |

**El `_start` no es el `main`.** **Es el punto de entrada que llama a `__libc_start_main`, que a su vez llama a `main`.**

**Flujo completo:**

1. **El kernel salta a `_start` (`base + 0x1080`).**
2. **`_start` llama a `__libc_start_main` (de `libc.so.6`).**
3. **`__libc_start_main` inicializa `libc`, llama a los constructores (`.init_array`), llama a `main` (`base + 0x1169`), y luego llama a `exit`.**
4. **`main` ejecuta `printf("Hola mundo!\n")`, y retorna.**
5. **`__libc_start_main` llama a `exit(0)`, y el proceso termina.**

---

## 7. El `PT_NOTE` (el segmento que secuestramos)

**Comando:**

```bash
readelf -lW /tmp/test/mi_binario | grep NOTE
```

**Salida:**

```
  NOTE           0x000350 0x0000000000000350 0x0000000000000350 0x000024 0x000024 R   0x4
  NOTE           0x0020f8 0x00000000000020f8 0x00000000000020f8 0x000030 0x000030 R   0x8
  NOTE           0x002128 0x0000000000002128 0x0000000000002128 0x000020 0x000020 R   0x4
```

**Interpretación:**

- **`NOTE 1`:** **`p_offset = 0x350`, `p_filesz = 0x24`, `p_align = 0x4`.** **Es el `.note.gnu.build-id`.**
- **`NOTE 2`:** **`p_offset = 0x20f8`, `p_filesz = 0x30`, `p_align = 0x8`.** **Es el `.note.gnu.property`.**
- **`NOTE 3`:** **`p_offset = 0x2128`, `p_filesz = 0x20`, `p_align = 0x4`.** **Es el `.note.ABI-tag`.**

**El kernel no mapea los `PT_NOTE`.** **Solo el linker los usa (para build-id, ABI, etc.).** **Por eso son el candidato ideal para secuestrar: convertirlos en `PT_LOAD` no rompe nada.**

---

## 8. El mapa de memoria en runtime

**Comando (con un binario que se quede colgado):**

```bash
# Añade un getchar() a mi_binario.c, o usa sleep:
/tmp/test/mi_binario &
sleep 1
pmap $!
kill $!
```

**Salida (ejemplo):**

```
555555554000-555555555000 r--p 00000000 08:01 12345678   /tmp/test/mi_binario
555555555000-555555556000 r-xp 00001000 08:01 12345678   /tmp/test/mi_binario
555555556000-555555557000 r--p 00002000 08:01 12345678   /tmp/test/mi_binario
555555557000-555555559000 rw-p 00002000 08:01 12345678   /tmp/test/mi_binario
7ffff7fc0000-7ffff7fc1000 r--p 00000000 08:01 87654321   /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7ffff7fc1000-7ffff7ff0000 r-xp 00001000 08:01 87654321   /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
...
```

**Interpretación:**

- **`0x555555554000`:** **base PIE (aleatoria por ASLR).**
- **`0x555555554000`-`0x555555555000`:** **`PT_LOAD` R (offset `0x0`, `p_flags = R`).**
- **`0x555555555000`-`0x555555556000`:** **`PT_LOAD` R E (offset `0x1000`, `p_flags = R E`).** **Aquí está el `_start` (`0x555555555080`).**
- **`0x555555556000`-`0x555555557000`:** **`PT_LOAD` R (offset `0x2000`, `p_flags = R`).**
- **`0x555555557000`-`0x555555559000`:** **`PT_LOAD` RW (offset `0x2db0`, `p_flags = RW`).** **Aquí está `.dynamic`, `.got`, `.data`, `.bss`.**
- **`0x7ffff7fc0000`-...:** **el loader dinámico (`ld-linux-x86-64.so.2`).**

---

## 9. Resumen del flujo del kernel

1. **El kernel lee el `Elf64_Ehdr` (64 bytes).** **Verifica el magic, la clase, el endianness, la máquina.**
2. **Lee la tabla de `Phdr` (14 entradas de 56 bytes).**
3. **Para cada `PT_LOAD`, lo mapea en `base + p_vaddr`, con permisos de `p_flags`.**
4. **Si hay `PT_INTERP`, carga el loader (`ld-linux-x86-64.so.2`) y le pasa el control.**
5. **El loader carga las librerías (`libc.so.6`), procesa la `.dynamic`, aplica las relocalizaciones.**
6. **El loader salta al `_start` del binario (`base + e_entry = base + 0x1080`).**
7. **El `_start` llama a `__libc_start_main`, que llama a `main`.**
8. **`main` ejecuta `printf("Hola mundo!\n")`, y termina.**
9. **`__libc_start_main` llama a `exit(0)`, y el proceso termina.**

**El `e_entry` es el offset del `_start`.** **En PIE, el kernel suma la base (ASLR).** **El `_start` está en el `PT_LOAD` ejecutable (`p_flags = R E`).** **El `_start` no es el `main`; es el punto de entrada que llama a `__libc_start_main`, que a su vez llama a `main`.**

# El mapa de memoria real (`/proc/maps` y `pmap`)

### 10.1. Cómo obtener el mapa de memoria en runtime

**Para ver el mapa de memoria real de un proceso, el binario tiene que quedarse vivo.** **Si el binario termina inmediatamente (como `mi_binario` con `printf` y `return 0`), no hay tiempo a hacer `pmap`.**

**Solución: añadir un `getchar()` a `mi_binario.c`, justo antes del `return 0`:**

```c
#include <stdio.h>

int main(void) {
    printf("Hola mundo!\n");
    getchar();          // ← espera a que pulses Enter
    return 0;
}
```

**Recompilar:**

```bash
gcc -o mi_binario mi_binario.c
```

**Ejecutar en background, y ver el mapa:**

```bash
/tmp/test/mi_binario &
sleep 1
pmap $(pgrep mi_binario)
cat /proc/$(pgrep mi_binario)/maps
kill $(pgrep mi_binario)
```

**El `getchar()` bloquea el proceso, y da tiempo a hacer `pmap`.** **El proceso se queda en `Stopped` (detenido por la terminal) si se ejecuta en background, pero eso no impide ver el mapa.**

### 10.2. El mapa del binario limpio

**Comando:**

```bash
cp mi_binario /tmp/test/mi_binario
/tmp/test/mi_binario &
sleep 1
pmap $(pgrep mi_binario)
kill $(pgrep mi_binario)
```

**Salida (ejemplo real):**

```
2873458:   /tmp/test/mi_binario
000061d09e65a000      4K r---- mi_binario
000061d09e65b000      4K r-x-- mi_binario
000061d09e65c000      4K r---- mi_binario
000061d09e65d000      4K r---- mi_binario
000061d09e65e000      4K rw--- mi_binario
000061d0ce0e0000    132K rw---   [ anon ]
000076da8f000000    160K r---- libc.so.6
000076da8f028000   1632K r-x-- libc.so.6
...
```

**Interpretación:**

| Dirección | Permisos | `PT_LOAD` | Sección |
|---|---|---|---|
| `0x61d09e65a000` | `r----` | LOAD R (offset `0x0`) | `.interp`, `.note`, `.gnu.hash`, `.dynsym`, `.dynstr`, `.rela` |
| `0x61d09e65b000` | `r-x--` | LOAD R E (offset `0x1000`) | `.init`, `.plt`, `.text`, `.fini` |
| `0x61d09e65c000` | `r----` | LOAD R (offset `0x2000`) | `.rodata`, `.eh_frame` |
| `0x61d09e65d000` | `r----` | LOAD R (offset `0x2000`, segunda página) | `.rodata`, `.eh_frame` |
| `0x61d09e65e000` | `rw---` | LOAD RW (offset `0x3000`) | `.dynamic`, `.got`, `.data`, `.bss` |

**Fíjate en que el `LOAD RW` aparece en `offset 0x3000`, no en `0x2db0`.** **El kernel alinea `p_offset` a página:** **`0x2db0 & ~0xFFF = 0x2000`.** **Pero el `maps` muestra `offset 0x3000`.** **Eso es porque el kernel mapea cada página con los permisos del último `LOAD` que la cubre.** **La página `0x2000`-`0x2FFF` es `r--p` (del `LOAD R` en `offset 0x2000`), y la página `0x3000`-`0x3FFF` es `rw-p` (del `LOAD RW` en `offset 0x2db0`).**

**La base es `0x61d09e65a000` (aleatoria por ASLR).** **El `_start` (`e_entry = 0x1080`) está en `0x61d09e65b080` (base + `0x1080`).** **Eso cae dentro del `LOAD R E` (`0x61d09e65b000`-`0x61d09e65c000`).** **Correcto.**

### 10.3. El mapa del binario infectado (la prueba definitiva)

**Comando:**

```bash
cp mi_binario /tmp/test/mi_binario
./War
/tmp/test/mi_binario &
sleep 1
pmap $(pgrep mi_binario)
kill $(pgrep mi_binario)
```

**Salida (ejemplo real):**

```
2873458:   /tmp/test/mi_binario
0000634ce6b72000      4K r---- mi_binario (deleted)
0000634ce6b73000      4K r-x-- mi_binario (deleted)
0000634ce6b74000      4K r---- mi_binario (deleted)
0000634ce6b75000      4K r---- mi_binario (deleted)
0000634ce6b76000      4K rw--- mi_binario (deleted)
0000634ce6b77000      4K r-x-- mi_binario (deleted)   ← ¡EL PAYLOAD!
0000634d0f5c7000    132K rw---   [ anon ]
000074baf1c00000    160K r---- libc.so.6
...
```

**La diferencia clave:** **hay una página nueva (`r-x--`) que no estaba en el binario limpio.** **Esa es el `PT_LOAD` que antes era `PT_NOTE`.** **El kernel la ha mapeado en `0x634ce6b77000` (base + `0x5000`), y el `e_entry` apunta ahí.** **El payload se ejecuta, modifica la firma, y salta al `_start` original.**

**El `(deleted)` en todas las líneas es porque el payload hizo `rename("/tmp/.hidden", "/tmp/test/mi_binario")`.** **Eso reemplaza el inodo, y el kernel marca todas las páginas del inodo viejo como `(deleted)`.** **El proceso sigue ejecutando el inodo viejo (en memoria), pero el disco tiene el inodo nuevo (con la firma modificada).** **En la siguiente ejecución, el kernel carga el inodo nuevo.**

### 10.4. Comparación limpio vs infectado

| Binario limpio | Binario infectado |
|---|---|
| `4K r---- mi_binario` | `4K r---- mi_binario (deleted)` |
| `4K r-x-- mi_binario` | `4K r-x-- mi_binario (deleted)` |
| `4K r---- mi_binario` | `4K r---- mi_binario (deleted)` |
| `4K r---- mi_binario` | `4K r---- mi_binario (deleted)` |
| `4K rw--- mi_binario` | `4K rw--- mi_binario (deleted)` |
| **(no hay)** | **`4K r-x-- mi_binario (deleted)` ← EL PAYLOAD** |

**La página nueva (`r-x--`) es el `PT_NOTE` convertido en `PT_LOAD`.** **El kernel la mapea, y el `e_entry` apunta ahí.** **El payload se ejecuta, modifica la firma (con `rename`), y salta al `_start` original.** **El `(deleted)` es el precio del `rename` en caliente.**

**Esa es la prueba definitiva de que el `PT_NOTE` + `rename` funciona.**

---

## 11. Flujo completo de la infección (resumen)

**Cuando `./War` infecta un binario:**

1. **`ft_infect` lee el ELF del binario.** **Busca un `PT_NOTE` (el de build-id, `p_offset = 0x350`).**
2. **Calcula `aligned_insertion` (final del archivo, alineado a `0x1000`) y `new_vaddr` (por encima de todos los `PT_LOAD`).**
3. **Convierte el `PT_NOTE` en `PT_LOAD`:**
   - `p_type = PT_LOAD`
   - `p_flags = PF_R | PF_X`
   - `p_offset = aligned_insertion`
   - `p_vaddr = new_vaddr`
   - `p_filesz = payload_len + sig_len`
   - `p_memsz = payload_len + sig_len`
   - `p_align = 0x1000`
4. **Redirige `e_entry` a `new_vaddr`.**
5. **Escribe el payload en `aligned_insertion`, y la firma a continuación.**
6. **Parchea el payload con `entry_delta` (para saltar al `_start` original) y `sig_offset` (para encontrar la firma).**

**Cuando el binario infectado se ejecuta:**

1. **El kernel carga el ELF, mapea los `PT_LOAD` (incluido el nuevo), y salta a `e_entry` (`new_vaddr`).**
2. **El payload se ejecuta:**
   - Calcula su base (`base_payload`) con `call`/`pop`/`sub`.
   - Hace `readlink("/proc/self/exe")` para obtener el path.
   - Hace `fork`. **El padre salta al `_start` original; el hijo sigue.**
   - **El hijo hace `execve("/proc/self/exe", ["WARCHILD", path, NULL], NULL)`.** **Eso crea un proceso nuevo, con un `mm` distinto, y el kernel no bloquea `open(O_RDWR)` ni `mmap RW`.**
   - **El proceso nuevo (el hijo re-ejecutado) detecta `argv[0] == "WARCHILD"`, y va a `.child_work`:**
     - `open(path, O_RDWR)`.
     - `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)`.
     - **Modifica el fingerprint en el mapeo.**
     - `msync(mmap, size, MS_SYNC)`.
     - `munmap`, `close`, `exit(0)`.
   - **El binario en disco tiene la firma modificada.**
3. **El padre (el proceso original) salta al `_start` original, y el binario funciona como si nada.**

**El `(deleted)` en `/proc/maps` es porque el payload hace `rename` en caliente (si usas `rename`), o no aparece si usas `execve` (si el `execve` funciona).** **En el caso de `execve`, el binario no se reemplaza, pero el `execve` en el hijo crea un proceso nuevo que modifica el binario en disco (con `mmap` RW), sin `rename`.** **Eso evita el `(deleted)` y el `Security violation` de coreutils.**

**Si el `execve` falla (por `ETXTBSY`), el hijo no modifica la firma.** **En ese caso, la única opción es el `rename` en caliente (que rompe coreutils).**

---

## 12. Limitaciones conocidas

### 12.1. `ETXTBSY` al abrir el ejecutable en curso

**El kernel bloquea `open(O_RDWR)` sobre el ejecutable en curso con `ETXTBSY`.** **Eso impide modificar la firma del binario en ejecución directamente.** **La solución es `fork` + `execve` con `argv` especial, para crear un proceso nuevo que no esté ejecutando el binario, y que pueda hacer `open(O_RDWR)` sin `ETXTBSY`.**

### 12.2. `Security violation` de coreutils

**Coreutils (como `ls`, `cat`) comprueban `/proc/self/exe` al arrancar.** **Si el path termina en `(deleted)`, abortan con `Security violation`.** **Eso ocurre si el payload hace `rename` en caliente.** **La solución es `execve` + `mmap RW` + `msync` (sin `rename`), que no marca el inodo como `(deleted)`.** **Pero el `execve` puede fallar con `ETXTBSY` si el binario sigue en ejecución.**

### 12.3. Solución perfecta no existe

**No hay forma de modificar el binario en ejecución sin `rename` (que rompe coreutils) o sin `ETXTBSY` (que impide la modificación).** **El kernel protege los ejecutables en curso por buenas razones (seguridad).** **La solución de `war` es `rename` en caliente, que rompe coreutils pero funciona para binarios que no son coreutils (`mi_binario`).** **La solución de `execve` + `mmap RW` evita el `rename`, pero puede fallar con `ETXTBSY`.**

**Para el subject, la solución aceptada es `rename` en caliente, y documentar que coreutils aborta (pero el payload ya hizo su trabajo).** **O usar `execve` + `mmap RW` y aceptar que a veces falla con `ETXTBSY`.**

---

## 13. Comandos de comprobación (resumen)

```bash
# 1. File header
readelf -h /tmp/test/mi_binario

# 2. Program headers (todos)
readelf -lW /tmp/test/mi_binario

# 3. Solo los PT_LOAD
readelf -lW /tmp/test/mi_binario | grep LOAD

# 4. Solo el PT_LOAD ejecutable
readelf -lW /tmp/test/mi_binario | grep "LOAD.*R E"

# 5. Solo el PT_INTERP
readelf -lW /tmp/test/mi_binario | grep INTERP

# 6. Dynamic section
readelf -d /tmp/test/mi_binario

# 7. Símbolo _start (dirección y tamaño)
readelf -s /tmp/test/mi_binario | grep _start

# 8. Desensamblar el _start (usando la dirección y tamaño del paso 7)
#    En mi_binario: _start en 0x10a0, tamaño 38 (0x26), termina en 0x10c6.
objdump -d --start-address=0x10a0 --stop-address=0x10c6 /tmp/test/mi_binario

# 9. Solo los PT_NOTE
readelf -lW /tmp/test/mi_binario | grep NOTE

# 10. Mapa de memoria en runtime (con getchar() en mi_binario.c)
/tmp/test/mi_binario &
sleep 1
pmap $(pgrep mi_binario)
cat /proc/$(pgrep mi_binario)/maps
kill $(pgrep mi_binario)

# 11. Verificar la infección
strings /tmp/test/mi_binario | tail -1
# War version 1.0 (c)oded Oct - 2026 by davgalle - <fingerprint>

# 12. Verificar que el binario sigue funcionando
/tmp/test/mi_binario
# Hola mundo!
```
----