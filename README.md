# War — Project UNIX | 42

> Tercer proyecto de la rama de virología de 42. Construcción de un virus polimórfico sobre ELF 64 bits.

---

## Índice

- [Descripción](#descripción)
- [Contexto](#contexto)
- [Requisitos mandatory](#requisitos-mandatory)
- [Funcionamiento](#funcionamiento)
- [Firma polimórfica](#firma-polimórfica)
- [Protecciones implementadas](#protecciones-implementadas)
- [Uso](#uso)
- [Bonus](#bonus)
- [Restricciones técnicas](#restricciones-técnicas)
- [Evaluación](#evaluación)

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
