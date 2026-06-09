# tnfstestdos

Comprehensive DOS filesystem test for [tnfsdrv](https://github.com/Fbeen/tnfsdos) — but works on any DOS drive.

## What does it do?

`TNFSTEST.EXE` tests all standard DOS filesystem functions via INT 21h. It creates a subdirectory `TNFSTEST` on the specified drive, runs all tests there, and cleans up completely afterwards.

The program is written as a pure DOS application with no knowledge of TNFS or any network protocol. This means it can be used to test both a local hard disk and a TNFS virtual drive mapped by tnfsdrv.

### Tested functions

| Section | DOS function | INT 2Fh AL | Description |
|---------|-------------|------------|-------------|
| 1 | AH=3Ch / AH=40h | 17h / 09h | Create file and write |
| 2 | AH=3Dh / AH=3Fh / AH=42h | 16h / 08h / 21h | Open, read, seek (SET / CUR / END) |
| 3 | AH=42h SEEK_END + AH=40h | 21h / 09h | Append (write at end of file) |
| 4 | AH=43h GET/SET | 0Fh / 0Eh | File attributes (including read-only) |
| 5 | AH=36h | 0Ch | Disk free space |
| 6 | AH=39h / AH=3Bh / AH=47h | 02h / 05h / 03h | Mkdir, chdir, getcwd, rmdir |
| 7 | AH=4Eh / AH=4Fh | 1Bh / 1Ch | FindFirst / FindNext (wildcards) |
| 8 | AH=41h | 13h | Delete file |
| 9 | AH=56h | 07h | Rename file |
| 10 | AH=57h GET/SET | — | File date and time |
| 11 | AH=40h CX=0 | 09h | Truncate (cut file at current position) |
| 12 | — | — | Multiple files open simultaneously |
| 13 | — | — | Large I/O (4096 bytes in one call) |
| 14 | — | — | Binary data (all 256 byte values, including 0x00, 0x0D, 0x0A, 0x1A) |
| 15 | — | — | Nested directories (two levels deep) |
| 16 | — | — | Expected error conditions (non-existent file, duplicate dir, invalid handle) |

Each test reports `PASS` or `FAIL`. Optional functions that may not be implemented (e.g. truncate on TNFS) report `OK` or `NOTE` — a `NOTE` does not count as a failure.

## Building

Requires [OpenWatcom](https://github.com/open-watcom/open-watcom-v2) (16-bit DOS, small model).

```
make
```

The executable is written to `build/tnfstest.exe`.

### Compiler flags

```
wcc -bt=dos -ms -3 -s -wx
```

| Flag | Meaning |
|------|---------|
| `-bt=dos` | DOS target platform |
| `-ms` | Small memory model |
| `-3` | 80386 instruction set |
| `-s` | No stack overflow check |
| `-wx` | All warnings enabled |

## Usage

```
TNFSTEST [drive]
```

| Command | Test location |
|---------|---------------|
| `TNFSTEST C:` | `C:\TNFSTEST\` |
| `TNFSTEST D:` | `D:\TNFSTEST\` |
| `TNFSTEST N:` | `N:\TNFSTEST\` |
| `TNFSTEST` | `.\TNFSTEST\` (current drive) |

The program:
1. Saves the current working directory
2. Creates `[drive]:\TNFSTEST\` (reuses it if it already exists)
3. Runs all 16 test sections with a 0.5 second pause between each
4. Cleans up all created files and directories
5. Restores the working directory to its original location

## Sample output

```
TNFSTEST - Uitgebreide DOS filesystem test
==========================================
Basismap : D:\TNFSTEST
Opstart  : D:\

[1. CREATE + WRITE  (AL=17h, AL=09h, AL=06h)]
  PASS  create TST1.DAT  (AL=17h)
  PASS  write 256 bytes  (AL=09h)
  PASS  close na write  (AL=06h)
...
==========================================
RESULTAAT: 88 geslaagd, 0 mislukt, 0 nota
==========================================
```

On a local DOS hard disk all tests are expected to pass (0 failures). On a TNFS virtual drive via tnfsdrv, optional functions may report `NOTE` if they are not yet implemented in the driver.

## Relation to tnfsdrv

This test program was developed as part of the [tnfsdrv](https://github.com/Fbeen/tnfsdos) project — a DOS TSR that emulates a network disk as a DOS drive using the TNFS protocol. By running `TNFSTEST` first on a local drive (0 failures) and then on the TNFS drive, you can immediately see which INT 2Fh subfunctions are missing or behaving incorrectly in the driver.
