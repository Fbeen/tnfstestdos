# tnfstestdos

Uitgebreide DOS filesystem test voor [tnfsdrv](https://github.com/Fbeen/tnfsdos) — maar werkt op elke DOS drive.

## Wat doet het?

`TNFSTEST.EXE` test alle standaard DOS bestandssysteem-functies via INT 21h. Het programma maakt een submap `TNFSTEST` aan op de opgegeven drive, voert daar alle tests uit en ruimt daarna alles weer op.

De tests zijn geschreven als puur DOS-programma: het weet niets van TNFS of andere netwerkprotocollen. Daardoor kun je het zowel op een lokale harde schijf als op een TNFS virtual drive draaien.

### Geteste functies

| Sectie | DOS functie | INT 2Fh AL | Omschrijving |
|--------|-------------|------------|--------------|
| 1 | AH=3Ch / AH=40h | 17h / 09h | Bestand aanmaken en schrijven |
| 2 | AH=3Dh / AH=3Fh / AH=42h | 16h / 08h / 21h | Openen, lezen, seekken |
| 3 | AH=42h SEEK_END + AH=40h | 21h / 09h | Append (toevoegen aan einde) |
| 4 | AH=43h GET/SET | 0Fh / 0Eh | Bestandsattributen (ook read-only) |
| 5 | AH=36h | 0Ch | Vrije schijfruimte opvragen |
| 6 | AH=39h / AH=3Bh / AH=47h | 02h / 05h / 03h | Mkdir, chdir, getcwd, rmdir |
| 7 | AH=4Eh / AH=4Fh | 1Bh / 1Ch | FindFirst / FindNext (wildcards) |
| 8 | AH=41h | 13h | Bestand verwijderen |
| 9 | AH=56h | 07h | Bestand hernoemen |
| 10 | AH=57h GET/SET | — | Bestandsdatum en -tijd |
| 11 | AH=40h CX=0 | 09h | Truncate (afkappen op huidige positie) |
| 12 | — | — | Meerdere bestanden tegelijk open |
| 13 | — | — | Grote I/O (4096 bytes in één keer) |
| 14 | — | — | Binaire data (alle 256 byte-waarden, incl. 0x00, 0x0D, 0x0A, 0x1A) |
| 15 | — | — | Geneste mappen (twee niveaus diep) |
| 16 | — | — | Verwachte foutcodes (niet-bestaand, al-bestaand, ongeldige handle) |

Elke test geeft `PASS` of `FAIL`. Optionele functies die mogelijk niet geïmplementeerd zijn (zoals truncate op TNFS) geven `OK` of `NOTE` — een `NOTE` telt niet mee als fout.

## Compileren

Vereist [OpenWatcom](https://github.com/open-watcom/open-watcom-v2) (16-bit DOS, small model).

```
make
```

De executable verschijnt in `build/tnfstest.exe`.

### Compiler flags

```
wcc -bt=dos -ms -3 -s -wx
```

| Flag | Betekenis |
|------|-----------|
| `-bt=dos` | DOS doelplatform |
| `-ms` | Small memory model |
| `-3` | 80386 instructieset |
| `-s` | Geen stack-overflow check |
| `-wx` | Alle waarschuwingen aan |

## Gebruik

```
TNFSTEST [drive]
```

| Commando | Testlocatie |
|----------|-------------|
| `TNFSTEST C:` | `C:\TNFSTEST\` |
| `TNFSTEST D:` | `D:\TNFSTEST\` |
| `TNFSTEST N:` | `N:\TNFSTEST\` |
| `TNFSTEST` | `.\TNFSTEST\` (huidige drive) |

Het programma:
1. Slaat de huidige werkmap op
2. Maakt `[drive]:\TNFSTEST\` aan (hergebruikt als die al bestaat)
3. Voert alle 16 testsecties uit, met een pauze van 0,5 seconde ertussen
4. Ruimt alle aangemaakte bestanden en mappen op
5. Herstelt de werkmap naar de oorspronkelijke locatie

## Uitvoer

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

Op een lokale DOS harde schijf worden alle tests verwacht te slagen (0 mislukt). Op een TNFS virtual drive via tnfsdrv kunnen sommige optionele functies een `NOTE` geven als ze nog niet geïmplementeerd zijn in de driver.

## Relatie met tnfsdrv

Dit testprogramma is ontwikkeld als onderdeel van het [tnfsdrv](https://github.com/Fbeen/tnfsdos) project — een DOS TSR die via het TNFS-protocol een netwerkschijf emuleert als DOS drive. Door `TNFSTEST` eerst op een lokale schijf te draaien (0 fouten) en daarna op de TNFS drive, zie je direct welke INT 2Fh subfuncties nog ontbreken of afwijken in de driver.
