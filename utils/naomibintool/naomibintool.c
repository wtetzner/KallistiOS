/* KallistiOS ##version##

   naomibintool.c
   Copyright (C) 2020 Lawrence Sebald

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

#define NAOMI_REGION_JAPAN          0
#define NAOMI_REGION_USA            1
#define NAOMI_REGION_EXPORT         2
#define NAOMI_REGION_KOREA          3
#define NAOMI_REGION_AUSTRALIA      4

typedef struct naomi_segment {
    uint32_t rom_offset;
    uint32_t ram_offset;
    uint32_t size;
} naomi_segment_t;

/* NAOMI ROM header structure. Thanks to DragonMinded's documentation for the
   data here (https://github.com/DragonMinded/netboot/blob/trunk/docs/naomi.md)
 */
typedef struct naomi_hdr {
    char signature[16];
    char developer[32];
    char region_title[8][32];
    uint16_t mfg_year;
    uint8_t mfg_month;
    uint8_t mfg_day;
    char serial_number[4];
    uint16_t eightmb_mode;
    uint16_t g1_init;
    uint32_t g1_rrc;
    uint32_t g1_rwc;
    uint32_t g1_frc;
    uint32_t g1_fwc;
    uint32_t g1_crc;
    uint32_t g1_cwc;
    uint32_t g1_gdrc;
    uint32_t g1_gdwc;
    uint8_t m2m4_checksum[132];
    struct {
        uint8_t apply;
        uint8_t system_settings;
        uint8_t coin_chute;
        uint8_t coin_setting;
        uint8_t coin1_rate;
        uint8_t coin2_rate;
        uint8_t credit_rate;
        uint8_t bonus_rate;
        uint8_t seqtext_offset[8];
    } eeprom[8];
    char sequence_text[8][32];
    naomi_segment_t segment[8];
    naomi_segment_t test_segment[8];
    uint32_t entry;
    uint32_t test_entry;
    uint8_t supported_regions;
    uint8_t supported_players;
    uint8_t supported_display_freq;
    uint8_t supported_display_dir;
    uint8_t check_eeprom;
    uint8_t service_type;
    uint8_t m1_checksums[138];
    uint8_t padding[71];
    uint8_t encrypted;
} naomi_hdr_t;

static void print_header(const naomi_hdr_t *hdr) {
    int i;

    printf("Platform Signature: %.16s\n", hdr->signature);
    printf("Developer: %.32s\n", hdr->developer);

    for(i = 0; i < 8; ++i) {
        printf("Region title %d: %.32s\n", i + 1, hdr->region_title[i]);
    }
}

static void print_segments(const naomi_hdr_t *hdr) {
    int i;

    for(i = 0; i < 8; ++i) {
        if(hdr->segment[i].rom_offset == 0xFFFFFFFF)
            break;

        printf("Segment %d\n"
               "ROM Offset: %08" PRIx32 "\n"
               "RAM Offset: %08" PRIx32 "\n"
               "Size: %" PRIu32 "\n", i + 1, hdr->segment[i].rom_offset,
               hdr->segment[i].ram_offset, hdr->segment[i].size);
    }

    for(i = 0; i < 8; ++i) {
        if(hdr->test_segment[i].rom_offset == 0xFFFFFFFF)
            break;

        printf("Test Segment %d\n"
               "ROM Offset: %08" PRIx32 "\n"
               "RAM Offset: %08" PRIx32 "\n"
               "Size: %" PRIu32 "\n", i + 1, hdr->test_segment[i].rom_offset,
               hdr->test_segment[i].ram_offset, hdr->test_segment[i].size);
    }
}

static void print_entries(const naomi_hdr_t *hdr) {
    printf("Entry point: %08" PRIx32 "\n"
           "Test Entry point: %08" PRIx32 "\n", hdr->entry, hdr->test_entry);
}

void usage(const char *progname) {
    printf("Usage: %s oper filename [args]\n\n", progname);
    printf("Where oper is one of the following:\n"
           "  read  -- Reads the header binary and prints out information.\n");
}

int read_header(int argc, char *argv[]) {
    FILE *fp;
    naomi_hdr_t hdr;

    if(argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if(!(fp = fopen(argv[2], "rb"))) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    if(fread(&hdr, sizeof(naomi_hdr_t), 1, fp) != 1) {
        perror("Error reading file");
        fclose(fp);
        return EXIT_FAILURE;
    }

    fclose(fp);

    if(memcmp(hdr.signature, "NAOMI           ", 16) &&
       memcmp(hdr.signature, "Naomi2          ", 16)) {
        fprintf(stderr, "File does not appear to be a NAOMI/NAOMI2 ROM.\n");
        return EXIT_FAILURE;
    }

    print_header(&hdr);
    print_segments(&hdr);
    print_entries(&hdr);

    return 0;
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!strcmp(argv[1], "read")) {
        return read_header(argc, argv);
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
