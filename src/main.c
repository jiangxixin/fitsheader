/*
 * fitsheader -- FITS file header viewer using cfitsio
 *
 * Usage:
 *   fitsheader <file.fits> [keyword ...]
 *     Without keywords: prints all header cards.
 *     With keywords:    prints only matching ones (case-insensitive).
 *
 *   fitsheader -x <file.fits>
 *     Hexdump mode: prints each 80-byte card in hex + ASCII.
 *
 * Compile with CMake (see CMakeLists.txt).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fitsio.h>

#define FITS_CARD  80
#define EXIT_OK    0
#define EXIT_ARG   1
#define EXIT_FITS  2

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-x] <file.fits> [keyword ...]\n", prog);
    fprintf(stderr, "  -x   hexdump each 80-byte header card\n");
    fprintf(stderr, "  Without keywords prints all header entries.\n");
}

/* Print all header cards as formatted text */
static int print_all_headers(fitsfile *fptr, int *status) {
    char card[FITS_CARD + 1];
    int nkeys, i;

    fits_get_hdrspace(fptr, &nkeys, NULL, status);
    if (*status != 0) {
        fprintf(stderr, "cfitsio error getting hdrspace: %d\n", *status);
        return *status;
    }

    /* cfitsio: card 0 is always empty (special sentinel for the mandatory
     * SIMPLE card of the primary HDU). Cards start at i=1, END is at
     * i=nkeys+1. So we iterate 1 .. nkeys+1 inclusive. */
    for (i = 1; i <= nkeys + 1; i++) {
        if (fits_read_record(fptr, i, card, status) != 0)
            break;
        card[FITS_CARD] = '\0';
        /* skip blank cards */
        if (card[0] == '\0') continue;
        printf("%s\n", card);
    }
    return *status;
}

/* Print only the header cards whose 8-char keyword matches one of
 * the argv keywords (case-insensitive). */
static int print_matching_keywords(fitsfile *fptr, int argc, char *argv[],
                                   int *status) {
    char card[FITS_CARD + 1];
    int nkeys, i, j;

    fits_get_hdrspace(fptr, &nkeys, NULL, status);

    /* cfitsio: cards start at i=1, END is at i=nkeys+1 */
    for (i = 1; i <= nkeys + 1; i++) {
        if (fits_read_record(fptr, i, card, status) != 0)
            break;
        card[FITS_CARD] = '\0';
        if (card[0] == '\0') continue;

        /* Extract keyword (first 8 chars) */
        char kw[9] = {0};
        for (j = 0; j < 8; j++)
            kw[j] = (card[j] == ' ') ? '\0' : card[j];

        for (j = 0; j < argc; j++) {
            if (strcasecmp(kw, argv[j]) == 0) {
                printf("%s\n", card);
                break;
            }
        }
    }
    return *status;
}

/* Hexdump each 2880-byte FITS block from the header section. */
static int hexdump_header(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        return 1;
    }

    unsigned char block[2880];
    size_t n;
    int block_idx = 0;
    int all_zero = 0;

    /* Read until we hit END or two consecutive all-zero blocks
     * (avoids reading the whole file on corrupt headers). */
    while ((n = fread(block, 1, sizeof(block), fp)) > 0) {
        int is_zero = 1;
        for (size_t k = 0; k < n; k++) {
            if (block[k] != 0) { is_zero = 0; break; }
        }

        if (is_zero && all_zero)
            break;  /* Two empty blocks in a row → done */
        all_zero = is_zero;

        /* Check for END keyword in this block */
        int has_end = 0;
        for (int c = 0; c + 8 <= (int)n; c += FITS_CARD) {
            if (strncmp((char *)block + c, "END     ", 8) == 0) {
                has_end = 1;
                break;
            }
        }
        if (has_end) {
            /* Print this block then stop */
            for (size_t k = 0; k < n; k++) {
                int col = k % FITS_CARD;
                if (col == 0)
                    printf("  block %2d card %2d offset %4d: ",
                           block_idx, (int)(k / FITS_CARD), (int)k);
                printf("%02x ", block[k]);
                if (col == 7 || col == 9) printf(" ");
                if (col == FITS_CARD - 1) {
                    printf(" |");
                    for (int p = k - 79; p <= (int)k; p++)
                        putchar((block[p] >= 32 && block[p] < 127) ?
                                block[p] : '.');
                    printf("|\n");
                }
            }
            break;
        }

        /* Print full block */
        for (size_t k = 0; k < n; k++) {
            int col = k % FITS_CARD;
            if (col == 0)
                printf("  block %2d card %2d offset %4d: ",
                       block_idx, (int)(k / FITS_CARD), (int)k);
            printf("%02x ", block[k]);
            if (col == 7 || col == 9) printf(" ");
            if (col == FITS_CARD - 1) {
                printf(" |");
                for (int p = k - 79; p <= (int)k; p++)
                    putchar((block[p] >= 32 && block[p] < 127) ?
                            block[p] : '.');
                printf("|\n");
            }
        }

        block_idx++;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    int status;
    fitsfile *fptr = NULL;

    /* ---- Parse arguments ---- */
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_ARG;
    }

    int hexdump_mode = 0;
    int file_arg = 1;

    if (strcmp(argv[1], "-x") == 0) {
        hexdump_mode = 1;
        file_arg = 2;
        if (argc < 3) {
            print_usage(argv[0]);
            return EXIT_ARG;
        }
    }

    const char *filename = argv[file_arg];
    int want_all = (argc == file_arg + 1);

    /* ---- Hexdump mode: read raw file, no cfitsio needed ---- */
    if (hexdump_mode) {
        return hexdump_header(filename);
    }

    /* ---- Normal mode: use cfitsio ---- */
    status = 0;
    if (fits_open_file(&fptr, filename, READONLY, &status) != 0) {
        fprintf(stderr, "Cannot open FITS file '%s': %d\n", filename, status);
        return EXIT_FITS;
    }

    if (want_all) {
        status = print_all_headers(fptr, &status);
    } else {
        status = print_matching_keywords(fptr, argc - file_arg - 1,
                                          &argv[file_arg + 1], &status);
    }

    fits_close_file(fptr, &status);

    if (status != 0) {
        fprintf(stderr, "cfitsio error (code %d)\n", status);
        return EXIT_FITS;
    }

    return EXIT_OK;
}
