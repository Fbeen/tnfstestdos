/*
 * TNFSTEST.C  -  Uitgebreide DOS filesystem test voor tnfsdrv.
 *
 * Test alle standaard DOS file/dir operaties via de C runtime.
 * Werkt op elke DOS drive: lokaal (C:, D: ...) of TNFS (via tnfsdrv TSR).
 *
 * Gebruik:  TNFSTEST [drive]
 *   TNFSTEST T:    - test op T:\TNFSTEST\  (verwachte notatie)
 *   TNFSTEST T     - idem
 *   TNFSTEST C:    - test op C:\TNFSTEST\
 *   TNFSTEST       - test in .\TNFSTEST\
 *
 * Tests worden ALTIJD in de submap TNFSTEST op de opgegeven drive gedaan.
 * Aan het einde wordt alles opgeruimd.
 *
 * ---------------------------------------------------------------
 * Geimplementeerde INT 2Fh subfuncties (AL-waarden) in tnfsdrv:
 *   01h RMDIR       02h MKDIR       03h SETCURDIR   05h CHDIR
 *   06h CLOSE       08h READ        09h WRITE       0Ch DISKSPACE
 *   0Eh SETATTR     0Fh GETATTR     13h DELETE      16h OPEN
 *   17h CREATE      21h SEEKEND     1Bh FINDFIRST   1Ch FINDNEXT
 *
 * NIET geimplementeerd (getest als FAIL of NOTE):
 *   07h RENAME      (INT 21h AH=56h)      -> test_rename()  [FAIL op TNFS]
 *   ??h FILE DATUM  (INT 21h AH=57h)      -> test_datetime() [NOTE]
 *   09h TRUNCATE    (WRITE CX=0)          -> test_truncate() [NOTE]
 * ---------------------------------------------------------------
 *
 * Compileren (OpenWatcom):
 *   wcc -bt=dos -ms -3 -s tnfstest.c
 *   wlink system dos name build\tnfstest.exe file build\tnfstest.obj
 */

#include <dos.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Tellers en output                                                    */
/* ------------------------------------------------------------------ */

static int g_pass  = 0;
static int g_fail  = 0;
static int g_note  = 0;

static void chk(int cond, const char *name)
{
    if (cond) {
        printf("  PASS  %s\n", name);
        g_pass++;
    } else {
        printf("  FAIL  %s\n", name);
        g_fail++;
    }
}

/* Optionele test: telt niet mee als fout, maar wordt gemeld */
static void noted(int cond, const char *name)
{
    if (cond) {
        printf("  OK    %s\n", name);
        g_pass++;
    } else {
        printf("  NOTE  %s  (waarschijnlijk niet geimplementeerd in tnfsdrv)\n", name);
        g_note++;
    }
}

#define SECTION(s)  printf("\n[%s]\n", s)

/* ------------------------------------------------------------------ */
/* Basismap, pad-helper en oorspronkelijke cwd                         */
/* ------------------------------------------------------------------ */

static char g_base[84];       /* bv. "T:\TNFSTEST" */
static char g_path[128];
static char g_path2[128];
static char g_orig_cwd[128];  /* cwd bij opstart, wordt hersteld bij exit */

static void mkp(char *out, const char *sub)
{
    strcpy(out, g_base);
    strcat(out, "\\");
    strcat(out, sub);
}

/* ------------------------------------------------------------------ */
/* Data-patroon helpers                                                 */
/* ------------------------------------------------------------------ */

static void fill_pat(char *buf, unsigned len, unsigned char seed)
{
    unsigned i;
    for (i = 0; i < len; i++)
        buf[i] = (char)((unsigned char)(i + seed));
}

static int check_pat(const char *buf, unsigned len, unsigned char seed)
{
    unsigned i;
    for (i = 0; i < len; i++)
        if ((unsigned char)buf[i] != (unsigned char)(i + seed))
            return 0;
    return 1;
}

/* Statische I/O-buffers buiten de stack */
static char g_wbuf[4096];
static char g_rbuf[4096];

/* ================================================================== */
/* SECTIE 1: Create + Write  (INT 2Fh AL=17h CREATE, AL=09h WRITE)    */
/* ================================================================== */

static void test_create_write(void)
{
    int fd;
    int n;

    SECTION("1. CREATE + WRITE  (AL=17h, AL=09h, AL=06h)");

    mkp(g_path, "TST1.DAT");

    fd = open(g_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
    chk(fd >= 0, "create TST1.DAT  (AL=17h)");
    if (fd < 0) return;

    fill_pat(g_wbuf, 256, 0x41);
    n = write(fd, g_wbuf, 256);
    chk(n == 256, "write 256 bytes  (AL=09h)");

    chk(close(fd) == 0, "close na write  (AL=06h)");
}

/* ================================================================== */
/* SECTIE 2: Open + Read + Seek  (AL=16h, AL=08h, AL=21h)             */
/* ================================================================== */

static void test_open_read_seek(void)
{
    int fd;
    int n;
    long pos;

    SECTION("2. OPEN + READ + SEEK  (AL=16h, AL=08h, AL=21h)");

    mkp(g_path, "TST1.DAT");

    fd = open(g_path, O_RDONLY | O_BINARY);
    chk(fd >= 0, "open bestaand bestand  (AL=16h)");
    if (fd < 0) return;

    /* AL=08h: lees en verifieer */
    memset(g_rbuf, 0, 256);
    n = read(fd, g_rbuf, 256);
    chk(n == 256, "read 256 bytes  (AL=08h)");
    chk(check_pat(g_rbuf, 256, 0x41), "gelezen data = geschreven data");

    /* SEEK_SET */
    pos = lseek(fd, 0L, SEEK_SET);
    chk(pos == 0L, "lseek SEEK_SET 0 -> positie 0");

    /* SEEK_CUR */
    read(fd, g_rbuf, 10);
    pos = lseek(fd, 5L, SEEK_CUR);
    chk(pos == 15L, "lseek SEEK_CUR +5 -> positie 15");

    /* AL=21h: SEEKEND */
    pos = lseek(fd, 0L, SEEK_END);
    chk(pos == 256L, "lseek SEEK_END -> bestandsgrootte = 256  (AL=21h)");

    /* lezen na EOF */
    n = read(fd, g_rbuf, 10);
    chk(n == 0, "read na EOF geeft 0 bytes");

    /* seek + lees 1 byte op offset 50 */
    lseek(fd, 50L, SEEK_SET);
    n = read(fd, g_rbuf, 1);
    chk(n == 1 && (unsigned char)g_rbuf[0] == (unsigned char)(50 + 0x41),
        "seek offset 50: correcte byte gelezen");

    close(fd);
}

/* ================================================================== */
/* SECTIE 3: Append  (SEEKEND + WRITE)                                 */
/* ================================================================== */

static void test_append(void)
{
    int fd;
    int n;
    long pos;

    SECTION("3. APPEND  (open rdwr + SEEK_END + WRITE)");

    mkp(g_path, "TST1.DAT");
    fd = open(g_path, O_RDWR | O_BINARY);
    chk(fd >= 0, "open rdwr voor append");
    if (fd < 0) return;

    pos = lseek(fd, 0L, SEEK_END);
    chk(pos == 256L, "seek naar einde (pos=256) voor append");

    fill_pat(g_wbuf, 128, 0xBB);
    n = write(fd, g_wbuf, 128);
    chk(n == 128, "append: schrijf 128 bytes na einde");

    pos = lseek(fd, 0L, SEEK_END);
    chk(pos == 384L, "bestandsgrootte na append = 384");

    lseek(fd, 0L, SEEK_SET);
    n = read(fd, g_rbuf, 256);
    chk(n == 256 && check_pat(g_rbuf, 256, 0x41),
        "origineel deel (byte 0-255) ongewijzigd na append");

    n = read(fd, g_rbuf, 128);
    chk(n == 128 && check_pat(g_rbuf, 128, 0xBB),
        "ge-appended deel (byte 256-383) correct");

    close(fd);
}

/* ================================================================== */
/* SECTIE 4: Bestandsattributen  (AL=0Fh GETATTR, AL=0Eh SETATTR)    */
/* ================================================================== */

static void test_attributes(void)
{
    unsigned attr;
    int rc;
    int fd;

    SECTION("4. BESTANDSATTRIBUTEN  (AL=0Fh GETATTR, AL=0Eh SETATTR)");

    mkp(g_path, "TST1.DAT");

    /* GETATTR */
    rc = _dos_getfileattr(g_path, &attr);
    chk(rc == 0, "getfileattr TST1.DAT  (AL=0Fh)");
    chk(rc == 0 && !(attr & _A_SUBDIR), "bestand heeft geen directory-bit");
    if (rc == 0) printf("         attr = 0x%02X\n", attr);

    /* SETATTR: zet read-only */
    rc = _dos_setfileattr(g_path, _A_RDONLY);
    chk(rc == 0, "setfileattr: zet read-only  (AL=0Eh)");

    _dos_getfileattr(g_path, &attr);
    chk(attr & _A_RDONLY, "read-only bit correct gezet");

    fd = open(g_path, O_WRONLY | O_BINARY);
    if (fd >= 0) {
        chk(0, "open read-only bestand voor schrijven geeft fout");
        close(fd);
    } else {
        chk(1, "open read-only bestand voor schrijven geeft fout");
    }

    /* verwijder read-only */
    rc = _dos_setfileattr(g_path, _A_ARCH);
    chk(rc == 0, "setfileattr: verwijder read-only (terug naar normaal)");

    fd = open(g_path, O_WRONLY | O_BINARY);
    chk(fd >= 0, "open na verwijderen read-only lukt nu");
    if (fd >= 0) close(fd);
}

/* ================================================================== */
/* SECTIE 5: Schijfruimte  (AL=0Ch DISKSPACE)                         */
/*                                                                      */
/* LET OP: tnfsdrv retourneert momenteel hardgecodeerde dummy-waarden  */
/* (spc=8, bps=512, total=4096, avail=4096 => 16 MB totaal/vrij).     */
/* Op een lokale drive komen hier echte waarden uit.                    */
/* De TNFS-protocol functies tnfs_size() en tnfs_free() bestaan al,   */
/* maar worden nog niet aangeroepen vanuit do_diskspace().             */
/* ================================================================== */

/* Hardgecodeerde dummy-waarden die tnfsdrv teruggeeft (AL=0Ch handler) */
#define TNFSDRV_DUMMY_SPC    8u
#define TNFSDRV_DUMMY_BPS  512u
#define TNFSDRV_DUMMY_CLU 4096u

static void test_diskspace(void)
{
    struct _diskfree_t df;
    unsigned drive;
    unsigned long total_kb;
    unsigned long avail_kb;
    int is_dummy;

    SECTION("5. SCHIJFRUIMTE  (INT 21h AH=36h -> AL=0Ch DISKSPACE)");

    drive = (g_base[1] == ':')
          ? (unsigned)((g_base[0] & ~0x20u) - 'A') + 1u
          : 0u;

    chk(_dos_getdiskfree(drive, &df) == 0,
        "getdiskfree geeft success  (AL=0Ch)");

    if (df.bytes_per_sector == 0 || df.sectors_per_cluster == 0) {
        chk(0, "bytes_per_sector en sectors_per_cluster > 0");
        return;
    }

    chk(1, "bytes_per_sector en sectors_per_cluster > 0");

    total_kb = (unsigned long)df.total_clusters
             * (unsigned long)df.sectors_per_cluster
             * (unsigned long)df.bytes_per_sector
             / 1024UL;
    avail_kb = (unsigned long)df.avail_clusters
             * (unsigned long)df.sectors_per_cluster
             * (unsigned long)df.bytes_per_sector
             / 1024UL;

    printf("         totaal : %lu KB  (%u clusters)\n", total_kb, df.total_clusters);
    printf("         vrij   : %lu KB  (%u clusters)\n", avail_kb, df.avail_clusters);
    printf("         cluster: %u sectoren * %u bytes = %u bytes\n",
           df.sectors_per_cluster, df.bytes_per_sector,
           (unsigned)(df.sectors_per_cluster * df.bytes_per_sector));

    /* Detecteer hardgecodeerde dummy-waarden van tnfsdrv */
    is_dummy = (df.sectors_per_cluster == TNFSDRV_DUMMY_SPC &&
                df.bytes_per_sector    == TNFSDRV_DUMMY_BPS &&
                df.total_clusters      == TNFSDRV_DUMMY_CLU &&
                df.avail_clusters      == TNFSDRV_DUMMY_CLU);

    noted(!is_dummy,
          "schijfruimte: echte server-waarden (niet de 16 MB dummy uit tnfsdrv)");
    if (is_dummy)
        printf("         HINT: do_diskspace() retourneert hardgecodeerde 16 MB;\n"
               "         implementeer tnfs_free()/tnfs_size() in redirector.c.\n");

    /* beschikbare ruimte moet <= totaal */
    chk(df.avail_clusters <= df.total_clusters,
        "vrije clusters <= totale clusters");
}

/* ================================================================== */
/* SECTIE 6: mkdir / chdir / getcwd / rmdir  (AL=01-03,05)            */
/* ================================================================== */

static void test_directories(void)
{
    char cwd[128];
    unsigned attr;
    int rc;
    int fd;

    SECTION("6. MKDIR / CHDIR / GETCWD / RMDIR  (AL=02h,03h,05h,01h)");

    mkp(g_path, "SUBDIR");

    rc = _mkdir(g_path);
    chk(rc == 0, "mkdir SUBDIR  (AL=02h)");

    _dos_getfileattr(g_path, &attr);
    chk(attr & _A_SUBDIR, "SUBDIR heeft directory-attribuut");

    rc = _chdir(g_path);
    chk(rc == 0, "chdir naar SUBDIR  (AL=05h)");

    if (_getcwd(cwd, sizeof(cwd)) != NULL) {
        chk(strstr(cwd, "SUBDIR") != NULL || strstr(cwd, "subdir") != NULL,
            "getcwd toont SUBDIR als huidige map  (AL=03h/getcwd)");
        printf("         cwd = %s\n", cwd);
    } else {
        chk(0, "getcwd geeft huidige map terug");
    }

    fd = open("INSIDE.TXT", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
    chk(fd >= 0, "bestand aanmaken in SUBDIR");
    if (fd >= 0) { write(fd, "inside\r\n", 8); close(fd); }

    rc = _chdir(g_base);
    chk(rc == 0, "chdir terug naar basismap");

    /* niet-lege map verwijderen moet mislukken */
    rc = _rmdir(g_path);
    chk(rc != 0, "rmdir niet-lege SUBDIR geeft fout  (AL=01h)");

    mkp(g_path2, "SUBDIR\\INSIDE.TXT");
    chk(remove(g_path2) == 0, "verwijder bestand INSIDE.TXT in SUBDIR");

    mkp(g_path, "SUBDIR");
    rc = _rmdir(g_path);
    chk(rc == 0, "rmdir lege SUBDIR slaagt  (AL=01h)");
}

/* ================================================================== */
/* SECTIE 7: FindFirst / FindNext  (AL=1Bh, AL=1Ch)                   */
/* ================================================================== */

static void test_find(void)
{
    struct find_t f;
    int rc;
    int fd;
    int count;
    int found_dir;

    SECTION("7. FINDFIRST / FINDNEXT  (AL=1Bh, AL=1Ch)");

    mkp(g_path, "FILE_A.TXT");
    fd = open(g_path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    if (fd >= 0) { write(fd, "A", 1); close(fd); }

    mkp(g_path, "FILE_B.TXT");
    fd = open(g_path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    if (fd >= 0) { write(fd, "B", 1); close(fd); }

    mkp(g_path, "FILE_C.DAT");
    fd = open(g_path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    if (fd >= 0) { write(fd, "C", 1); close(fd); }

    /* *.TXT: precies 2 verwacht */
    mkp(g_path, "*.TXT");
    rc = _dos_findfirst(g_path, _A_NORMAL|_A_RDONLY|_A_ARCH, &f);
    chk(rc == 0, "findfirst *.TXT  (AL=1Bh)");
    if (rc == 0) {
        count = 1;
        printf("         gevonden: %-13s  %lu bytes\n", f.name, f.size);
        while (_dos_findnext(&f) == 0) {
            count++;
            printf("         gevonden: %-13s  %lu bytes\n", f.name, f.size);
        }
        chk(count == 2, "findnext: precies 2 .TXT bestanden  (AL=1Ch)");
    }

    /* *.* : minstens 4 bestanden */
    mkp(g_path, "*.*");
    rc = _dos_findfirst(g_path, _A_NORMAL|_A_RDONLY|_A_ARCH, &f);
    chk(rc == 0, "findfirst *.*");
    if (rc == 0) {
        count = 1;
        while (_dos_findnext(&f) == 0) count++;
        chk(count >= 4, "findnext: minstens 4 bestanden");
        printf("         totaal: %d bestanden\n", count);
    }

    /* *.* inclusief submappen */
    mkp(g_path, "FINDDIR");
    _mkdir(g_path);
    mkp(g_path, "*.*");
    rc = _dos_findfirst(g_path, _A_NORMAL|_A_ARCH|_A_SUBDIR, &f);
    if (rc == 0) {
        count = 0; found_dir = 0;
        do {
            count++;
            if (f.attrib & _A_SUBDIR) found_dir++;
        } while (_dos_findnext(&f) == 0);
        chk(found_dir >= 1, "findfirst met _A_SUBDIR: minstens 1 map gevonden");
        printf("         %d items totaal, %d directories\n", count, found_dir);
    }
    mkp(g_path, "FINDDIR");
    _rmdir(g_path);

    /* geen match: moet fout geven */
    mkp(g_path, "NIETS.XYZ");
    rc = _dos_findfirst(g_path, _A_NORMAL|_A_ARCH, &f);
    chk(rc != 0, "findfirst niet-bestaand patroon geeft fout");

    /* opruimen */
    mkp(g_path, "FILE_A.TXT"); remove(g_path);
    mkp(g_path, "FILE_B.TXT"); remove(g_path);
    mkp(g_path, "FILE_C.DAT"); remove(g_path);
}

/* ================================================================== */
/* SECTIE 8: Delete  (AL=13h)                                          */
/* ================================================================== */

static void test_delete(void)
{
    unsigned attr;
    int fd;
    int rc;

    SECTION("8. DELETE  (AL=13h)");

    /* maak extra bestand aan en verwijder het */
    mkp(g_path, "DEL_ME.TMP");
    fd = open(g_path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    if (fd >= 0) { write(fd, "weggooi", 7); close(fd); }

    rc = remove(g_path);
    chk(rc == 0, "delete DEL_ME.TMP  (AL=13h)");
    rc = _dos_getfileattr(g_path, &attr);
    chk(rc != 0, "DEL_ME.TMP niet meer aanwezig");

    /* verwijder ook het hoofdbestand uit eerdere tests */
    mkp(g_path, "TST1.DAT");
    rc = remove(g_path);
    chk(rc == 0, "delete TST1.DAT");
    rc = _dos_getfileattr(g_path, &attr);
    chk(rc != 0, "TST1.DAT niet meer aanwezig");
}

/* ================================================================== */
/* SECTIE 9: Rename  (INT 21h AH=56h -> INT 2Fh AL=07h)               */
/* Niet geimplementeerd in tnfsdrv: zal FAIL op TNFS, PASS op lokaal. */
/* ================================================================== */

static void test_rename(void)
{
    int fd;
    int rc;
    unsigned attr;

    SECTION("9. RENAME  (INT 21h AH=56h -> INT 2Fh AL=07h)");
    printf("         (Verwacht FAIL op TNFS drive; PASS op lokale drive)\n");

    mkp(g_path,  "REN_SRC.TXT");
    mkp(g_path2, "REN_DST.TXT");

    fd = open(g_path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    chk(fd >= 0, "aanmaken bronbestand REN_SRC.TXT");
    if (fd < 0) return;
    write(fd, "rename test\r\n", 13);
    close(fd);

    rc = rename(g_path, g_path2);
    chk(rc == 0, "rename REN_SRC.TXT -> REN_DST.TXT");

    if (rc == 0) {
        chk(_dos_getfileattr(g_path, &attr) != 0,
            "REN_SRC.TXT niet meer aanwezig na rename");
        chk(_dos_getfileattr(g_path2, &attr) == 0,
            "REN_DST.TXT bestaat na rename");
        remove(g_path2);
    } else {
        remove(g_path);  /* opruimen ook als mislukt */
    }
}

/* ================================================================== */
/* SECTIE 10: Bestandsdatum en -tijd  (INT 21h AH=57h)                */
/* Waarschijnlijk NIET geimplementeerd in tnfsdrv — telt als NOTE.    */
/* ================================================================== */

static void test_datetime(void)
{
    int fd;
    int rc;
    unsigned orig_date;
    unsigned orig_time;
    unsigned new_date;
    unsigned new_time;
    unsigned rd;
    unsigned rt;

    SECTION("10. BESTANDSDATUM EN -TIJD  (INT 21h AH=57h)");
    printf("         (Verwacht NOTE op TNFS drive; PASS op lokale drive)\n");

    /* DOS datum: bits 15-9 = jaar-1980, 8-5 = maand, 4-0 = dag */
    /* DOS tijd:  bits 15-11 = uur, 10-5 = minuut, 4-0 = seconde/2 */
    /* 2025-06-15 = jaar=45, maand=6, dag=15 -> (45<<9)|(6<<5)|15 = 0x5ACF */
    /* 13:30:00   = uur=13, minuut=30, sec=0 -> (13<<11)|(30<<5)|0 = 0x6BC0  */
    new_date = (unsigned)(((2025 - 1980) << 9) | (6 << 5) | 15);
    new_time = (unsigned)((13 << 11) | (30 << 5) | 0);

    mkp(g_path, "DATETIME.TXT");
    fd = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    if (fd < 0) {
        noted(0, "bestand aanmaken voor datum/tijd test");
        return;
    }
    write(fd, "datum test\r\n", 12);

    /* lees huidige datum/tijd */
    rc = _dos_getftime(fd, &orig_date, &orig_time);
    noted(rc == 0, "getftime: lees bestandsdatum/tijd  (INT 21h AH=57h AL=00)");

    if (rc == 0)
        printf("         huidige datum=0x%04X tijd=0x%04X\n",
               orig_date, orig_time);

    /* schrijf nieuwe datum/tijd */
    rc = _dos_setftime(fd, new_date, new_time);
    noted(rc == 0, "setftime: schrijf bestandsdatum/tijd  (INT 21h AH=57h AL=01)");

    if (rc == 0) {
        /* lees terug en verifieer */
        rc = _dos_getftime(fd, &rd, &rt);
        noted(rc == 0 && rd == new_date && rt == new_time,
              "getftime na setftime: datum/tijd klopt terug");
        if (rc == 0)
            printf("         teruggel. datum=0x%04X tijd=0x%04X "
                   "(verwacht 0x%04X 0x%04X)\n",
                   rd, rt, new_date, new_time);
    }

    close(fd);
    remove(g_path);
}

/* ================================================================== */
/* SECTIE 11: Truncate  (WRITE CX=0 = bestand afkappen)               */
/* Kan mislukken op TNFS als de server 0-byte-schrijven negeert.      */
/* ================================================================== */

static void test_truncate(void)
{
    int fd;
    int n;
    long sz;

    SECTION("11. TRUNCATE  (open + seek + write 0 bytes)");
    printf("         (Verwacht NOTE op TNFS drive; PASS op lokale drive)\n");

    mkp(g_path, "TRUNC.DAT");

    /* maak bestand met 200 bytes */
    fd = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    chk(fd >= 0, "aanmaken TRUNC.DAT");
    if (fd < 0) return;
    fill_pat(g_wbuf, 200, 0x77);
    n = write(fd, g_wbuf, 200);
    chk(n == 200, "schrijf 200 bytes");
    close(fd);

    /* heropenen, seek naar offset 100, truncate via write(0) */
    fd = open(g_path, O_RDWR | O_BINARY);
    chk(fd >= 0, "heropenen voor truncate");
    if (fd < 0) { remove(g_path); return; }

    lseek(fd, 100L, SEEK_SET);
    n = write(fd, g_wbuf, 0);  /* INT 21h AH=40h CX=0 = truncate at current pos */
    noted(n == 0, "write(0) op offset 100 slaagt");

    sz = lseek(fd, 0L, SEEK_END);
    noted(sz == 100L, "bestandsgrootte na truncate op 100 = 100 bytes");
    if (sz != 100L) printf("         werkelijke grootte = %ld\n", sz);

    close(fd);
    remove(g_path);
}

/* ================================================================== */
/* SECTIE 12: Meerdere bestanden tegelijk open                         */
/* ================================================================== */

static void test_multiopen(void)
{
    int fd0;
    int fd1;
    int fd2;
    int n;

    SECTION("12. MEERDERE BESTANDEN TEGELIJK OPEN");

    mkp(g_path, "M0.DAT");
    fd0 = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    mkp(g_path, "M1.DAT");
    fd1 = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    mkp(g_path, "M2.DAT");
    fd2 = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);

    chk(fd0 >= 0 && fd1 >= 0 && fd2 >= 0,
        "3 bestanden gelijktijdig openen");

    if (fd0 >= 0 && fd1 >= 0 && fd2 >= 0) {
        fill_pat(g_wbuf, 64, 0x00); write(fd0, g_wbuf, 64);
        fill_pat(g_wbuf, 64, 0x55); write(fd1, g_wbuf, 64);
        fill_pat(g_wbuf, 64, 0xAA); write(fd2, g_wbuf, 64);
        chk(1, "schrijven naar 3 gelijktijdig geopende bestanden");

        lseek(fd0, 0L, SEEK_SET);
        n = read(fd0, g_rbuf, 64);
        chk(n == 64 && check_pat(g_rbuf, 64, 0x00), "bestand 0: patroon 0x00 correct");

        lseek(fd1, 0L, SEEK_SET);
        n = read(fd1, g_rbuf, 64);
        chk(n == 64 && check_pat(g_rbuf, 64, 0x55), "bestand 1: patroon 0x55 correct");

        lseek(fd2, 0L, SEEK_SET);
        n = read(fd2, g_rbuf, 64);
        chk(n == 64 && check_pat(g_rbuf, 64, 0xAA), "bestand 2: patroon 0xAA correct");
    }

    if (fd0 >= 0) close(fd0);
    if (fd1 >= 0) close(fd1);
    if (fd2 >= 0) close(fd2);

    mkp(g_path, "M0.DAT"); remove(g_path);
    mkp(g_path, "M1.DAT"); remove(g_path);
    mkp(g_path, "M2.DAT"); remove(g_path);
}

/* ================================================================== */
/* SECTIE 13: Grote I/O  (4096 bytes in een keer)                      */
/* ================================================================== */

static void test_large_io(void)
{
    int fd;
    int n;
    long sz;

    SECTION("13. GROTE I/O  (4096 bytes)");

    fill_pat(g_wbuf, 4096, 0x33);

    mkp(g_path, "LARGE.DAT");
    fd = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    chk(fd >= 0, "aanmaken LARGE.DAT");
    if (fd < 0) return;

    n = write(fd, g_wbuf, 4096);
    chk(n == 4096, "schrijf 4096 bytes in een keer");

    sz = lseek(fd, 0L, SEEK_END);
    chk(sz == 4096L, "bestandsgrootte = 4096");

    lseek(fd, 0L, SEEK_SET);
    memset(g_rbuf, 0, sizeof(g_rbuf));
    n = read(fd, g_rbuf, 4096);
    chk(n == 4096, "lees 4096 bytes terug");
    chk(n == 4096 && check_pat(g_rbuf, 4096, 0x33),
        "alle 4096 bytes correct");

    /* deelstuk na seek naar midden */
    lseek(fd, 1024L, SEEK_SET);
    n = read(fd, g_rbuf, 256);
    chk(n == 256 && check_pat(g_rbuf, 256, (unsigned char)(1024 + 0x33)),
        "deelstuk lezen na seek naar offset 1024");

    close(fd);
    remove(g_path);
}

/* ================================================================== */
/* SECTIE 14: Binaire data  (alle 256 byte-waarden)                   */
/* ================================================================== */

static void test_binary(void)
{
    int fd;
    int n;
    int ok;
    unsigned i;

    SECTION("14. BINAIRE DATA  (alle 256 byte-waarden)");

    for (i = 0; i < 256; i++)
        g_wbuf[i] = (char)(unsigned char)i;

    mkp(g_path, "BIN.DAT");
    fd = open(g_path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    chk(fd >= 0, "aanmaken BIN.DAT");
    if (fd < 0) return;

    n = write(fd, g_wbuf, 256);
    chk(n == 256, "schrijf bytes 0x00..0xFF");

    lseek(fd, 0L, SEEK_SET);
    memset(g_rbuf, 0xCC, 256);
    n = read(fd, g_rbuf, 256);
    chk(n == 256, "lees 256 bytes terug");

    ok = 1;
    for (i = 0; i < 256; i++)
        if ((unsigned char)g_rbuf[i] != (unsigned char)i) { ok = 0; break; }
    chk(ok,
        "alle 256 byte-waarden correct (ook 0x00, 0x1A EOF-marker, 0x0D, 0x0A)");

    close(fd);
    remove(g_path);
}

/* ================================================================== */
/* SECTIE 15: Nested directories  (twee niveaus diep)                  */
/* ================================================================== */

static void test_nested_dirs(void)
{
    char inner[128];
    char fpath[128];
    char cwd[128];
    struct find_t f;
    int rc;
    int fd;
    int count;

    SECTION("15. NESTED DIRECTORIES");

    mkp(g_path, "OUTER");
    chk(_mkdir(g_path) == 0, "mkdir OUTER");

    strcpy(inner, g_path);
    strcat(inner, "\\INNER");
    chk(_mkdir(inner) == 0, "mkdir OUTER\\INNER");

    strcpy(fpath, inner);
    strcat(fpath, "\\DEEP.TXT");
    fd = open(fpath, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IREAD|S_IWRITE);
    chk(fd >= 0, "bestand aanmaken in OUTER\\INNER");
    if (fd >= 0) { write(fd, "diepste niveau\r\n", 16); close(fd); }

    rc = _chdir(inner);
    chk(rc == 0, "chdir naar OUTER\\INNER");

    if (_getcwd(cwd, sizeof(cwd)) != NULL) {
        chk(strstr(cwd,"INNER") != NULL || strstr(cwd,"inner") != NULL,
            "getcwd toont INNER als huidige map");
    }

    rc = _dos_findfirst("*.*", _A_NORMAL|_A_ARCH, &f);
    chk(rc == 0, "findfirst *.* in OUTER\\INNER");
    if (rc == 0) {
        count = 1;
        while (_dos_findnext(&f) == 0) count++;
        chk(count >= 1, "minstens 1 bestand gevonden in OUTER\\INNER");
    }

    _chdir(g_base);

    remove(fpath);
    rc = _rmdir(inner);
    chk(rc == 0, "rmdir OUTER\\INNER");
    rc = _rmdir(g_path);
    chk(rc == 0, "rmdir OUTER");
}

/* ================================================================== */
/* SECTIE 16: Foutgevallen  (verwachte DOS-fouten)                    */
/* ================================================================== */

static void test_errors(void)
{
    int fd;
    int rc;
    int n;
    unsigned attr;

    SECTION("16. FOUTGEVALLEN");

    /* open niet-bestaand */
    mkp(g_path, "BESTAAT_NIET.DAT");
    fd = open(g_path, O_RDONLY | O_BINARY);
    chk(fd < 0, "open niet-bestaand geeft fout");
    if (fd >= 0) close(fd);

    /* delete niet-bestaand */
    rc = remove(g_path);
    chk(rc != 0, "delete niet-bestaand geeft fout");

    /* mkdir al-bestaand */
    mkp(g_path, "DUPL_DIR");
    _mkdir(g_path);
    rc = _mkdir(g_path);
    chk(rc != 0, "mkdir al-bestaande map geeft fout");
    _rmdir(g_path);

    /* rmdir niet-bestaand */
    mkp(g_path, "GEEN_MAP");
    rc = _rmdir(g_path);
    chk(rc != 0, "rmdir niet-bestaande map geeft fout");

    /* getfileattr niet-bestaand */
    mkp(g_path, "GHOST.TXT");
    rc = _dos_getfileattr(g_path, &attr);
    chk(rc != 0, "getfileattr niet-bestaand geeft fout");

    /* lezen met ongeldige handle */
    n = read(-1, g_rbuf, 10);
    chk(n < 0, "read met ongeldige handle geeft fout");
}

/* ================================================================== */
/* Opruimen: verwijder alle testbestanden en mappen                    */
/* ================================================================== */

static void cleanup(void)
{
    static const char *files[] = {
        "TST1.DAT", "DEL_ME.TMP",
        "FILE_A.TXT", "FILE_B.TXT", "FILE_C.DAT",
        "REN_SRC.TXT", "REN_DST.TXT",
        "M0.DAT", "M1.DAT", "M2.DAT",
        "LARGE.DAT", "BIN.DAT",
        "DATETIME.TXT", "TRUNC.DAT",
        "INSIDE.TXT",
        NULL
    };
    int i;

    for (i = 0; files[i]; i++) {
        mkp(g_path, files[i]);
        _dos_setfileattr(g_path, _A_ARCH);  /* read-only? verwijder het eerst */
        remove(g_path);
    }

    /* submappen van buiten naar binnen verwijderen */
    mkp(g_path, "SUBDIR\\INSIDE.TXT");     remove(g_path);
    mkp(g_path, "OUTER\\INNER\\DEEP.TXT"); remove(g_path);
    mkp(g_path, "OUTER\\INNER");           _rmdir(g_path);
    mkp(g_path, "OUTER");                  _rmdir(g_path);
    mkp(g_path, "SUBDIR");                 _rmdir(g_path);
    mkp(g_path, "FINDDIR");                _rmdir(g_path);
    mkp(g_path, "DUPL_DIR");               _rmdir(g_path);
}

/* ================================================================== */
/* Main                                                                 */
/* ================================================================== */

int main(int argc, char *argv[])
{
    printf("TNFSTEST - Uitgebreide DOS filesystem test\n");
    printf("==========================================\n");

    /* Bepaal basismap: altijd X:\TNFSTEST of .\TNFSTEST */
    if (argc >= 2 && argv[1][0] != '\0') {
        char drv = argv[1][0];
        if (drv >= 'a' && drv <= 'z') drv = (char)(drv - 32);
        /* accepteer "N", "N:" en "N:\" */
        g_base[0] = drv;
        strcpy(g_base + 1, ":\\TNFSTEST");
    } else {
        strcpy(g_base, "TNFSTEST");
    }

    /* Sla huidige werkmap op zodat we die aan het einde kunnen herstellen */
    if (_getcwd(g_orig_cwd, sizeof(g_orig_cwd)) == NULL)
        g_orig_cwd[0] = '\0';

    printf("Basismap : %s\n", g_base);
    printf("Opstart  : %s\n", g_orig_cwd[0] ? g_orig_cwd : "(onbekend)");
    printf("==========================================\n");

    /* Maak basismap aan.
     * DOS geeft bij een al-bestaande map foutcode 5 (Access Denied),
     * niet foutcode 183 (Already Exists) — dus errno is EACCES, niet
     * EEXIST.  Controleer daarna via getfileattr of de map er gewoon is. */
    if (_mkdir(g_base) != 0) {
        unsigned attr;
        if (_dos_getfileattr(g_base, &attr) == 0 && (attr & _A_SUBDIR)) {
            printf("  INFO  basismap bestaat al, wordt hergebruikt\n");
        } else {
            printf("FOUT: kan basismap niet aanmaken: %s (errno=%d)\n",
                   g_base, errno);
            return 1;
        }
    }

    /* ---- Tests uitvoeren ----------------------------------------- */
    test_create_write();   delay(500);
    test_open_read_seek(); delay(500);
    test_append();         delay(500);
    test_attributes();     delay(500);
    test_diskspace();      delay(500);
    test_directories();    delay(500);
    test_find();           delay(500);
    test_delete();         delay(500);
    test_rename();         delay(500);
    test_datetime();       delay(500);
    test_truncate();       delay(500);
    test_multiopen();      delay(500);
    test_large_io();       delay(500);
    test_binary();         delay(500);
    test_nested_dirs();    delay(500);
    test_errors();

    /* ---- Opruimen ------------------------------------------------- */
    printf("\n[OPRUIMEN]\n");

    /* Eerst terug naar de originele werkmap, zodat we:
     *  a) niet IN g_base staan als we die proberen te verwijderen,
     *  b) de cwd hersteld is voordat we terugkeren naar de shell. */
    if (g_orig_cwd[0] != '\0') {
        if (_chdir(g_orig_cwd) == 0)
            printf("  OK    cwd hersteld naar %s\n", g_orig_cwd);
        else
            printf("  WARN  cwd herstellen naar %s mislukt\n", g_orig_cwd);
    }

    cleanup();

    if (_rmdir(g_base) == 0)
        printf("  OK    basismap %s verwijderd\n", g_base);
    else
        printf("  WARN  basismap %s kon niet worden verwijderd\n", g_base);

    /* ---- Samenvatting --------------------------------------------- */
    printf("\n==========================================\n");
    printf("RESULTAAT: %d geslaagd, %d mislukt, %d nota\n",
           g_pass, g_fail, g_note);
    printf("==========================================\n");

    if (g_fail > 0)
        printf("Zie FAIL-regels hierboven — waarschijnlijk niet geimpl. in tnfsdrv.\n");
    if (g_note > 0)
        printf("Zie NOTE-regels hierboven — optionele/ontbrekende functies.\n");

    return (g_fail == 0) ? 0 : 1;
}
