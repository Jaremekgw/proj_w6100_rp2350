/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $ ls ~/project/pico2/pico-examples/flash/partition_info/
 * CMakeLists.txt  partition_info.c  pt.json  uf2_family_ids.c  uf2_family_ids.h
 * 
 */

#include <assert.h>
#include <stdio.h>
#include "stdlib.h"
#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "boot/uf2.h"
// #include "stdarg.h"
#include "utility.h"
#include "partition.h"

#define PART_LOC_FIRST(x) ( ((x) & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB )
#define PART_LOC_LAST(x)  ( ((x) & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS)  >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB )

#define PARTITION_LOCATION_AND_FLAGS_SIZE  2
#define PARTITION_ID_SIZE                  2
#define PARTITION_NAME_MAX                 127  // name length is indicated by 7 bits
#define PARTITION_TABLE_FIXED_INFO_SIZE    (4 + PARTITION_TABLE_MAX_PARTITIONS * (PARTITION_LOCATION_AND_FLAGS_SIZE + PARTITION_ID_SIZE))


#define PARTITION_EXTRA_FAMILY_ID_MAX      3

/* 
        // -- try to find read from flash solution -- //
        #define TEST1 PT_INFO_PARTITION_LOCATION_AND_FLAGS
        // in bootrom_constants.h:
        #define BOOT_TYPE_NORMAL     0
        #define BOOT_TYPE_BOOTSEL    2
        #define BOOT_TYPE_RAM_IMAGE  3
        #define BOOT_TYPE_FLASH_UPDATE 4

        #define CFLASH_SECLEVEL_BITS            0x00000300u
        #define CFLASH_SECLEVEL_LSB             _u(8)
        // Zero is not a valid security level:
        #define CFLASH_SECLEVEL_VALUE_SECURE    _u(1)
        #define CFLASH_SECLEVEL_VALUE_NONSECURE _u(2)
        #define CFLASH_SECLEVEL_VALUE_BOOTLOADER _u(3)

            // pico-examples/flash$ vim runtime_flash_permissions/runtime_flash_permissions.c +16


        // --
 */

 
/*
 * Stores partition table information and data read status
 */
typedef struct {
    uint32_t table[PARTITION_TABLE_FIXED_INFO_SIZE];
    uint32_t fields;
    bool has_partition_table;
    int partition_count;
    uint32_t unpartitioned_space_first_sector;
    uint32_t unpartitioned_space_last_sector;
    uint32_t flags_and_permissions;
    int current_partition;
    size_t pos;
    int status;
} pico_partition_table_t;

/*
 * Stores information on each partition
 */
typedef struct {
    uint32_t first_sector;
    uint32_t last_sector;
    uint32_t flags_and_permissions;
    bool has_id;
    uint64_t partition_id;
    bool has_name;
    char name[PARTITION_NAME_MAX + 1];
    uint32_t extra_family_id_count;
    uint32_t extra_family_ids[PARTITION_EXTRA_FAMILY_ID_MAX];
} pico_partition_t;


static int get_single_partition_info(int partition_id, partition_info_t *info);
static int get_corresponding_partition_id(uint current_partition_id);

int read_partition_table(pico_partition_table_t *pt);
bool read_next_partition(pico_partition_table_t *pt, pico_partition_t *p);


//=============================== Begin rom_get_other_image_addr

void show_current_partition(void) {
    boot_info_t boot_info;
    rom_get_boot_info(&boot_info);
    printf("Current partition index: %d\n", boot_info.partition);
}



// Return flash XIP address of the alternate partition for EFU update
/**
 * Get addresses for alternate partition in case of A/B partitioning
 * @param boot_info boot_info_t structure for current boot partition
 * @param part_info pointer to partition_info_t structure to fill with alternate partition info
 * @return 0 on success, -1 on failure
 */
int get_alt_part_addr(boot_info_t *boot_info, partition_info_t *part_info) {
    // get alternate partition ID
    int pt_id = get_corresponding_partition_id((uint)boot_info->partition);

    if (get_single_partition_info(pt_id, part_info) != 0)
        return -1;
    return 0;
}

//=============================== End rom_get_other_image_addr


// block this part
// static __attribute__((aligned(4))) uint8_t workarea[4 * 1024];
// #define WORKAREA_SIZE  3264
#define WORKAREA_SIZE  3264


//=============================== Begin UF2



/**
 * Required for partition info display
 */
typedef struct {
    size_t count;
    char **items;
} uf2_family_ids_t;

//----

#define UF2_FAMILY_ID_HEX_SIZE   (2 + 8 * 2 + 1)

/**
 * Required for partition info display
 */
static void _add(uf2_family_ids_t *ids, const char *str) {
    ids->items = realloc(ids->items, (ids->count + 1) * sizeof(char *));
    ids->items[ids->count] = strdup(str);
    if (ids->items[ids->count] == NULL) {
        perror("strdup");
        return;
    }
    ids->count++;
}

/**
 * Required for partition info display
 */
static void _add_default_families(uf2_family_ids_t *ids, uint32_t flags) {
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_ABSOLUTE_BITS)
        _add(ids, "absolute");
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_RP2040_BITS)
        _add(ids, "rp2040");
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_RP2350_ARM_S_BITS)
        _add(ids, "rp2350-arm-s");
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_RP2350_ARM_NS_BITS)
        _add(ids, "rp2350-arm-ns");
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_RP2350_RISCV_BITS)
        _add(ids, "rp2350-riscv");
    if (flags & PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_DATA_BITS)
        _add(ids, "data");
}

/**
 * Required for partition info display
 */
uf2_family_ids_t *uf2_family_ids_new(uint32_t flags) {
    uf2_family_ids_t *ids = malloc(sizeof(uf2_family_ids_t));
    ids->count = 0;
    ids->items = NULL;
    _add_default_families(ids, flags);
    return ids;
}

/**
 * Required for partition info display
 */
char *uf2_family_ids_join(const uf2_family_ids_t *ids, const char *sep) {
    size_t total_length = 0;
    size_t sep_length = strlen(sep);

    for (size_t i = 0; i < ids->count; i++) {
        total_length += strlen(ids->items[i]);
        if (i < ids->count - 1)
            total_length += sep_length;
    }

    char *result = calloc(1, total_length + 1);
    if (!result) {
        perror("calloc");
        return NULL;
    }

    result[0] = '\0';
    for (size_t i = 0; i < ids->count; i++) {
        strcat(result, ids->items[i]);
        if (i < ids->count - 1)
            strcat(result, sep);
    }

    return result;
}

void uf2_family_ids_free(uf2_family_ids_t *ids) {
    for (size_t i = 0; i < ids->count; i++) {
        free(ids->items[i]);
    }
    free(ids->items);
    free(ids);
}

void uf2_family_ids_add_extra_family_id(uf2_family_ids_t *ids, uint32_t family_id) {
    char hex_id[UF2_FAMILY_ID_HEX_SIZE];
    switch (family_id) {
        case CYW43_FIRMWARE_FAMILY_ID:
            _add(ids, "cyw43-firmware");
            break;
        default:
            sprintf(hex_id, "0x%08x", family_id);
            _add(ids, hex_id);
            break;
    }
}

//=============================== End UF2

/*
 * Read the partition table information.
 *
 * See the RP2350 datasheet 5.1.2, 5.4.8.16 for flags and structures that can be specified.
 */
int read_partition_table(pico_partition_table_t *pt) {
    // Reads fixed size fields
    uint32_t flags = PT_INFO_PT_INFO | PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID;
    int rc = rom_get_partition_table_info(pt->table, sizeof(pt->table), flags);
    if (rc < 0) {
        pt->partition_count = 0;
        pt->status = rc;
        return rc;
    }

    size_t pos = 0;
    pt->fields = pt->table[pos++];
    assert(pt->fields == flags);
    pt->partition_count = pt->table[pos] & 0x000000FF;
    pt->has_partition_table = pt->table[pos] & 0x00000100;
    pos++;
    uint32_t location = pt->table[pos++];
    pt->unpartitioned_space_first_sector = PART_LOC_FIRST(location);
    pt->unpartitioned_space_last_sector = PART_LOC_LAST(location);
    pt->flags_and_permissions = pt->table[pos++];
    pt->current_partition = 0;
    pt->pos = pos;
    pt->status = 0;

    return 0;
}

/*
 * Extract each partition information
 */
bool read_next_partition(pico_partition_table_t *pt, pico_partition_t *p) {
    if (pt->current_partition >= pt->partition_count) {
        return false;
    }

    size_t pos = pt->pos;
    uint32_t location = pt->table[pos++];
    p->first_sector = PART_LOC_FIRST(location);
    p->last_sector = PART_LOC_LAST(location);
    p->flags_and_permissions = pt->table[pos++];
    p->has_name = p->flags_and_permissions & PICOBIN_PARTITION_FLAGS_HAS_NAME_BITS;
    p->has_id = p->flags_and_permissions & PICOBIN_PARTITION_FLAGS_HAS_ID_BITS;

    if (p->has_id) {
        uint32_t id_low  = pt->table[pos++];
        uint32_t id_high = pt->table[pos++];
        p->partition_id = ((uint64_t)id_high << 32) | id_low;
    } else {
        p->partition_id = 0;
    }
    pt->pos = pos;

    p->extra_family_id_count = (p->flags_and_permissions & PICOBIN_PARTITION_FLAGS_ACCEPTS_NUM_EXTRA_FAMILIES_BITS)
                                   >> PICOBIN_PARTITION_FLAGS_ACCEPTS_NUM_EXTRA_FAMILIES_LSB;
    if (p->extra_family_id_count | p->has_name) {
        // Read variable length fields
        uint32_t extra_family_ids_and_name[PARTITION_EXTRA_FAMILY_ID_MAX + (((PARTITION_NAME_MAX + 1) / sizeof(uint32_t)) + 1)];
        uint32_t flags = PT_INFO_SINGLE_PARTITION | PT_INFO_PARTITION_FAMILY_IDS | PT_INFO_PARTITION_NAME;

        uint32_t value = ((uint32_t)pt->current_partition << 24) | (uint32_t)flags;
        int rc = rom_get_partition_table_info(extra_family_ids_and_name, sizeof(extra_family_ids_and_name), value);
                                            //   (pt->current_partition << 24 | flags));
        if (rc < 0) {
            pt->status = rc;
            return false;
        }
        size_t pos_ = 0;
        uint32_t __attribute__((unused)) fields = extra_family_ids_and_name[pos_++];
        assert(fields == flags);
        for (size_t i = 0; i < p->extra_family_id_count; i++, pos_++) {
            p->extra_family_ids[i] = extra_family_ids_and_name[pos_];
        }

        if (p->has_name) {
            uint8_t *name_buf = (uint8_t *)&extra_family_ids_and_name[pos_];
            uint8_t name_length = *name_buf++ & 0x7F;
            memcpy(p->name, name_buf, name_length);
            p->name[name_length] = '\0';
        }
    }
    if (!p->has_name)
         p->name[0] = '\0';

    pt->current_partition++;
    return true;
}




/**
 * Print partition information, 
 * based on pico-examples/flash/partition_info/partition_info.c
 * called from telnet.c
 * 
 * It is for calling from telent to see the current state of partition
 * Additional info will be added later
 */
int partition_info(char *msg, size_t msg_max_sz) {
    char *cursor = msg;
    size_t remaining = msg_max_sz;

    pico_partition_table_t pt;
    int rc;
    rc = read_partition_table(&pt);
    if (rc != 0) {
        // panic("rom_get_partition_table_info returned %d", pt.status);
        msg_printf(&cursor, &remaining,
                   "rom_get_partition_table_info returned %d\n",
                   pt.status);
        return -1;
    }
    if (!pt.has_partition_table) {
        // printf("there is no partition table\n");
        msg_printf(&cursor, &remaining, "there is no partition table\n");
        return 0;
    } else if (pt.partition_count == 0) {
        //printf("the partition table is empty\n");
        msg_printf(&cursor, &remaining, "the partition table is empty\n");
        return 0;
    }

    uf2_family_ids_t *family_ids = uf2_family_ids_new(pt.flags_and_permissions);
    char *str_family_ids = uf2_family_ids_join(family_ids, ", ");
    // printf("un-partitioned_space: S(%s%s) NSBOOT(%s%s) NS(%s%s) uf2 { %s }\n",
    msg_printf(&cursor, &remaining,
               "un-partitioned_space: S(%s%s) NSBOOT(%s%s) NS(%s%s) uf2 { %s }\n",
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_S_R_BITS ? "r" : ""),
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_S_W_BITS ? "w" : ""),
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NSBOOT_R_BITS ? "r" : ""),
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NSBOOT_W_BITS ? "w" : ""),
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NS_R_BITS ? "r" : ""),
               (pt.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NS_W_BITS ? "w" : ""),
               str_family_ids);
    free(str_family_ids);
    uf2_family_ids_free(family_ids);

    msg_printf(&cursor, &remaining, "partitions:\n");
    pico_partition_t p;
    while (read_next_partition(&pt, &p)) {
        msg_printf(&cursor, &remaining,
                   "%3d:    %08x->%08x S(%s%s) NSBOOT(%s%s) NS(%s%s)",
                   pt.current_partition - 1,
                   p.first_sector * FLASH_SECTOR_SIZE,
                   (p.last_sector + 1) * FLASH_SECTOR_SIZE,
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_S_R_BITS ? "r" : ""),
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_S_W_BITS ? "w" : ""),
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NSBOOT_R_BITS ? "r" : ""),
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NSBOOT_W_BITS ? "w" : ""),
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NS_R_BITS ? "r" : ""),
                   (p.flags_and_permissions & PICOBIN_PARTITION_PERMISSION_NS_W_BITS ? "w" : ""));

        if (p.has_id) {
            msg_printf(&cursor, &remaining, ", id=%016llx", p.partition_id);
        }
        if (p.has_name) {
            msg_printf(&cursor, &remaining, ", \"%s\"", p.name);
        }

        // print UF2 family ID
        family_ids = uf2_family_ids_new(p.flags_and_permissions);
        for (size_t i = 0; i < p.extra_family_id_count; i++) {
            uf2_family_ids_add_extra_family_id(family_ids, p.extra_family_ids[i]);
        }
        str_family_ids = uf2_family_ids_join(family_ids, ", ");
        // printf(", uf2 { %s }", str_family_ids);
        msg_printf(&cursor, &remaining, ", uf2 { %s }\n", str_family_ids);

        free(str_family_ids);
        uf2_family_ids_free(family_ids);

        printf("\n");
    }
    if (pt.status != 0) {
        // panic("rom_get_partition_table_info returned %d", pt.status);
        msg_printf(&cursor, &remaining,
                   "rom_get_partition_table_info returned %d\n", pt.status);
    }

    // return 0;
    return (int)(msg_max_sz - remaining);
}


static int get_corresponding_partition_id(uint current_partition_id) {
    int rc = rom_get_b_partition(current_partition_id); 
    if (rc < 0)     // BOOTROM_ERROR_NOT_FOUND
        return 0;   // This is (A) partition
    return rc;      // This is (B) partition
}

/*
 * Ensure partition table is loaded.
 */
static int ensure_partition_table_loaded(void) {
    #define PT_WORKAREA_SIZE   3264     // safe margin
    uint8_t pt_workarea[PT_WORKAREA_SIZE];
    return rom_load_partition_table(pt_workarea, PT_WORKAREA_SIZE, false);
}

/*
 * Get single partition information by ID.
 * 
 */
static int get_single_partition_info(int partition_id, partition_info_t *info) {
    uint16_t first_sector_number;
    uint16_t last_sector_number;
    uint32_t out[4];            // for [2] was problem. In example I saw [8]. Use [4] to be safe.
    uint32_t flags;
    int ret = -1;
    int rc;

    if (ensure_partition_table_loaded() < 0)
        return ret;

    flags = PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | ((uint32_t)partition_id << 24);
    rc = rom_get_partition_table_info(out, (int)(sizeof out / sizeof out[0]), flags);
    if (rc != 3)
        return ret;

    first_sector_number = (out[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
    last_sector_number = (out[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
    info->start_offset = first_sector_number * 0x1000;
    info->start_addr = info->start_offset + XIP_BASE;
    // info->end_offset = (uint32_t)(last_sector_number + 1) * 0x1000;
    info->end_offset = ((uint32_t)(last_sector_number + 1)) << 12;
    info->end_addr = info->end_offset + XIP_BASE;
    info->size = info->end_offset - info->start_offset;

    return 0;
}

/**
 * 
 */

 #ifdef BOOT_INFO_ON_USB
/**
 * Additional function to print to USB boot info
 * It was used to create EFU (Ethernet Firmware Updaete)
 * 
 * $ vim ../pico-examples/bootloaders/encrypted/enc_bootloader.c +122
 * 
 */
void read_boot_info(void) {
    int rc;
    uint8_t boot_partition;
    boot_info_t info;
    uint32_t data_start_addr = 0;
    uint32_t data_end_addr = 0;
    uint32_t data_max_size = 0;
    uint32_t my_workarea[WORKAREA_SIZE / 4];
    // static uint32_t my_workarea[WORKAREA_SIZE / 4];


    printf("Getting boot info:\n");

    rc = rom_get_boot_info(&info);
    printf("\tBoot Type %#x\n", info.boot_type);
    printf("\tPartition %#x\n", info.partition);
    printf("\tPart_idx  %#x\n", info.diagnostic_partition_index);
    printf("\tUpd_info  %#x\n", info.tbyb_and_update_info);
    printf("\tBoot diag %#x\n", info.boot_diagnostic);
    printf("\tReboot params [0]=%#x [1]=%#x\n", info.reboot_params[0], info.reboot_params[1]);
    if (info.boot_type == BOOT_TYPE_FLASH_UPDATE) {
        printf(" - Flash Update Base: 0x%08x\r\n", info.reboot_params[0]);
        //target = rom_get_other_image_addr(&info);
        partition_info_t part_info[1];
        if (get_alt_part_addr(&info, part_info) == 0)
            printf(" - Target address   : @0x%08x\r\n", part_info->start_addr);
    }
    else {
        printf(" - Not a flash update boot\r\n");
    }

    printf("This partition:\n");
    rc = rom_pick_ab_partition_during_update(my_workarea, sizeof(my_workarea), 0);
    switch(rc) {
        case BOOTROM_ERROR_LOCK_REQUIRED:
            printf("Error: Partition A/B failed %d - BOOTROM_ERROR_LOCK_REQUIRED\n", rc);
            return;
            break;
        case BOOTROM_ERROR_NOT_PERMITTED:
            printf("Error: Partition A/B failed %d - BOOTROM_ERROR_LOCK_REQUIRED\n", rc);
            return;
            break;
        case BOOTROM_ERROR_NOT_FOUND:
            printf("Error: Partition A/B failed %d - BOOTROM_ERROR_LOCK_REQUIRED\n", rc);
            return;
            break;
        default:
            if (rc < 0) {
                printf("Error: Partition A/B choice failed %d\n", rc);
                // reset_usb_boot(0, 0);
                return;
            } else {
                printf("\tPartition A/B picked: %d\n", rc);
            }
    }

    #define PT_TABLE_SIZE 8
    uint32_t pt_table[PT_TABLE_SIZE];
    
    boot_partition = rc;
    /**
     * This part is equivalent to get_single_partition_info():
     * 
     * partition_info_t partition[1];
     * if (get_single_partition_info(boot_partition, partition) == 0)
     *     printf("\tPartition Start: 0x%x, End: 0x%x, Size: %uk bytes\r\n", partition->start, partition->stop, partition->size/1024);
     * else
     *     printf("Error: No partition info found for partition %d\n", boot_partition);
     */
    rc = rom_get_partition_table_info(pt_table, PT_TABLE_SIZE, PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | (boot_partition << 24));
    if (rc != 3) {
        printf("Error: No boot partition - assuming bin at start of flash  ret=%d\n", rc);
        return;
    } else {
        // uint32_t val0 = pt_table[0];
        // uint32_t val1 = pt_table[1];
        uint16_t first_sector_number = (pt_table[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
        uint16_t last_sector_number = (pt_table[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
        data_start_addr = first_sector_number * 0x1000;
        data_end_addr = (last_sector_number + 1) * 0x1000;
        data_max_size = data_end_addr - data_start_addr;

        printf("\tPartition Start: 0x%x, End: 0x%x, Size: 0x%x\r\n", data_start_addr, data_end_addr, data_max_size);
        // printf("Values from rom_get_partition_table_info [0]:%#x [1]:%#x\r\n", val0, val1);
    }

    printf("--------\r\n");
    for (int p = 0; p < 6; p++) {
        partition_info_t partition[1];
        if (get_single_partition_info(p, partition) < 0) {
            printf("Partition %d: not found\n", p);
            continue;
        }
        printf("Partition %d: start_offset=0x%08X  size=%uk bytes\n",
               p, partition->start_offset, partition->size/1024 );
    }

    printf("== == == == == == == == == ==\r\n");
}
#endif // BOOT_INFO_ON_USB


// void util_flash_erase(uint32_t offs, uint32_t size) {
//     uint32_t save = save_and_disable_interrupts();

//     flash_range_erase(offs, size);

//     restore_interrupts(save);
// }

// void util_flash_read(uint32_t offs, void *data, uint32_t size) {
//     // flash_range_read(offs, data, size);
//     printf("Read function not implemented\r\n");
// }
// void util_flash_write(uint32_t offs, const void *data, uint32_t size) {
//     // void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count);
//     flash_range_program(offs, data, size);
// }



