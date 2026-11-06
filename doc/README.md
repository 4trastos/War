# Inyectar malware polimórfico en binario de 64 bits

## 1. Leer los file header.

```bash
readelf -h /bin/ls
```

- La terminal nos laza esta info:
```bash
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x6aa0
  Start of program headers:          64 (bytes into file)
  Start of section headers:          136232 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         13
  Size of section headers:           64 (bytes)
  Number of section headers:         31
  Section header string table index: 30
```

La información que realmente nos interesa para nuestro objetivo:
```bash
Entry point address:               0x6aa0
Start of program headers:          64 (bytes into file)
Size of program headers:           56 (bytes)
Number of section headers:         31
```
- **Entry point address:** `e_entry` es la direccción virtual donde el kernel comienza a ejecutar el binario. Es una dirección dentro un LOAD, concretamente dentro del segmento ejecutable, el `_start` del binario.
- **Start of program headers:** `e_phoff` El offset en el archivo donde empiza la tabla de *Program Headres*.
- **Size of program headers:** `e_phentsize` El tamaño de la tabla de *Program Hearders*
- **Number of section headers:** `e_phnum` Número de entradas en la tabla. Cada entrada es un `Phdr` (describe un segmento).


## 2. Buscar el LOAD ejecutable.

```bash
readelf -lW /bin/ls | grep LOAD
```

Buscamos un segmento `E` que contenga las instrucciones que ejeucta el binario. El `_start` (`e_entry`) está dentro de ese segmento. Los demás segmentos LOAD contienen datos, no codigo.

```bash
  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x003458 0x003458 R   0x1000
  LOAD           0x004000 0x0000000000004000 0x0000000000004000 0x013091 0x013091 R E 0x1000   <=
  LOAD           0x018000 0x0000000000018000 0x0000000000018000 0x007458 0x007458 R   0x1000
  LOAD           0x01ffd0 0x0000000000020fd0 0x0000000000020fd0 0x0012a8 0x002570 RW  0x1000
```

## 3. Estudiar el LOAD.

La información que contiene el segmento LOAD que nos interesa:
```bash
LOAD           0x004000 0x0000000000004000 0x0000000000004000 0x013091 0x013091 R E 0x1000
```

- `Type`        `LOAD`          Segmento que el kernel caarga en memoria
- `Offset`      `0x004000`      Dirección donde empieza en el archivo
- `VirtAddr`    `0x004000`      Dirección donde se carga en memoria (memoria virtual)
- `PhysAddr`    `0x004000`      No se usa en Linux (dirección física)
- `FileSiz`     `0x013091`      Cuantos bytes ocupa en el archivo
- `MemSiz`      `0x013091`      Cuantos bytes ocupa en memoria
- `Flg`         `R E`           Permisos: Read + Execute
- `Align`       `0x1000`        Alineación a página (0x1000 = 4096)

## 4. Calcular el final del segmnento

Necesitamos calcular donde termina el segmento ejecutable del binario, porque ahí será donde escribiremos nuestro payload. Dicho de otra manera, donde comienza nuestro payload.

Para calcular tenemos que sumar el `Offset` y `FileSiz`:
```bash
readelf -lW /bin/ls | awk '$1=="LOAD" && $7=="R" && $8=="E" {print $2, $5}'
0x004000 0x013091

printf '0x%x\n' $((0x004000 + 0x013091))
0x17091
```
```bash
Offset + FileSiz = 0x004000 + 0x013091 = 0x17091
```

## 5. Dónde cae el payload en memoria
