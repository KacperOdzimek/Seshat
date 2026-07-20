/*

*/

#ifndef SESHAT_H
#define SESHAT_H

#include <stdint.h>

// Basic

typedef enum sht_data_type {
    sht_data_type_archive   = 0,
    sht_data_type_int64     = 1,
    sht_data_type_float64   = 2,
    sht_data_type_utf8      = 3,
    sht_data_type_binary    = 4 
} sht_data_type;

// Archive

typedef struct sht_archive_create_info {
    uint64_t    file_bytes; // If non-zero reads archive from file, else creates a new blank one
    const void* file_data;  // If file_bytes non zero must be valid archive data
} sht_archive_create_info;

typedef struct sht_archive sht_archive;
sht_archive* sht_create_archive(const sht_archive_create_info* info);
void sht_free_archive(sht_archive*);

// Archive iteration

uint32_t sht_archive_query_index_entries_count(sht_archive*);           // Count of entries in index
uint32_t sht_archive_query_index_position(sht_archive*, const char*);   // Returns given entry position, on fail returns count of entries

// Archive queries

const char*     sht_archive_query_name      (sht_archive*, uint32_t);   // Returns given entry name
sht_data_type   sht_archive_query_data_type (sht_archive*, uint32_t);   // Returns data type of given entry
int             sht_archive_query_compressed(sht_archive*, uint32_t);   // Returns whether given entry data is compressed

// Archive value queries
// All of those operations return non-zero at success
// Only then value at *target is valid
// Data is automatically decompressed

int sht_archive_query_value_archive(sht_archive*, uint32_t, sht_archive** target);
int sht_archive_query_value_int64  (sht_archive*, uint32_t, int64_t*      target);    
int sht_archive_query_value_float64(sht_archive*, uint32_t, double*       target);

int sht_archive_query_value_binary_via_copy(sht_archive*, uint32_t, uint64_t* bytes, void* begin);  // Copies data - caller must free begin!
int sht_archive_query_value_utf_8_via_copy (sht_archive*, uint32_t, uint64_t* bytes, void* begin);  // Copies data - caller must free begin!

int sht_archive_query_value_binary_via_mmap(sht_archive*, uint32_t, uint64_t* bytes, const void* begin);    // Allows read-only view into file data
int sht_archive_query_value_utf_8_via_mmap(sht_archive*,  uint32_t, uint64_t* bytes, const void* begin);    // Allows read-only view into file data

// Archive modification



#endif // SESHAT_H

#ifdef SESHAT_IMPL



#endif // SESHAT_IMPL
