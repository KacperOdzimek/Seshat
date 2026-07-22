/*
----------------------------------------------------------------
Contents:
This file provides a builder and zero-copy view API for the Seshat V0 format.

----------------------------------------------------------------
Code info:
- sht prefix
- SESHAT_IMPL macro to build
- libzstd >= 1.3.0 dependant

----------------------------------------------------------------
Usage:
- sht_view:
    - parse an existing memory buffer without copying
    - payload data remains referenced directly from the source buffer
    - only decompression allocates new memory
- sht_builder:
    - build documents entry-by-entry
    - automatically handles sorting, alignment, padding, serialization, and zstd compression

----------------------------------------------------------------
Thread safety:
- sht_view is safe for concurrent read-only access after creation
- sht_builder is NOT thread-safe while being modified
*/

#ifndef SESHAT_H
#define SESHAT_H

#include <stddef.h>
#include <stdint.h>

// ===========================
// Data Types

typedef enum sht_type {
    sht_type_int64              = 0,
    sht_type_float64            = 1,
    sht_type_text               = 2,
    sht_type_binary             = 3,
    sht_type_text_compressed    = 4,
    sht_type_binary_compressed  = 5
} sht_type;

// ===========================
// Status Codes

typedef enum sht_status {
    sht_status_ok = 0,
    sht_status_err_bad_file,
    sht_status_err_bad_entry,
    sht_status_err_missing_out,
    sht_status_err_allocation_failure,
    sht_status_err_duplicate_names,
    sht_status_err_not_found,
    sht_status_err_type_mismatch,
    sht_status_err_buffer_too_small,
} sht_status;

// ===========================
// Memory access

typedef enum sht_access {
    sht_access_claim_ownership,
    sht_access_read_unowned,
    sht_access_make_copy
} sht_access;

// ===========================
// Viewer

typedef struct sht_view_create_info {
    sht_access  access;
    const void* buffer;
    size_t      bytes;
} sht_view_create_info;

typedef struct sht_view sht_view;
sht_status sht_create_view(sht_view** target, const sht_view_create_info* info);
void sht_free_view(sht_view*);

// Queries entries count
sht_status sht_view_query_entry_count(
    sht_view*, uint32_t* count
);

// Queries entry type at index
sht_status sht_view_query_entry_type(
    sht_view*, uint32_t entry, sht_type* out_type
);

// Binary search entry by name
// Name may be not null-terminated
sht_status sht_view_find(
    sht_view*, uint8_t name_len, const char* name, uint32_t* out
);

// ===========================
// Viewer Gets
// Pick version based on entry type

sht_status sht_view_get_as_int64(
    sht_view*, uint32_t entry, int64_t* out
);

sht_status sht_view_get_as_float64(
    sht_view*, uint32_t entry, double* out
);

// Zero-copy: hands back a pointer into the original buffer
sht_status sht_view_get_as_bytes(
    sht_view*, uint32_t entry, uint64_t* out_bytes, const uint8_t* *out_ptr
);

// Size the output buffer for seshat_entry_decompress
sht_status sht_view_get_as_uncompressed_size(
    sht_view*, uint32_t entry, uint64_t* out_bytes
);

// Decompresses a TEXT_compressed / BINARY_compressed entry into a caller-supplied buffer.
sht_status sht_view_get_as_decompress(
    sht_view*, uint32_t entry, void* out_buf, uint64_t out_buf_size
);

// ===========================
// Builder

typedef struct sht_builder sht_builder;
sht_status sht_create_builder(sht_builder** target);
void sht_free_builder(sht_builder*);

// ===========================
// Builder add operations
// Names in following function calls are copied right-away
// Names longer than 255 characters will be capped to this length

sht_status sht_builder_add_int64(
    sht_builder*, const char* name, int64_t value
);

sht_status sht_builder_add_float64(
    sht_builder*, const char* name, double value
);

sht_status sht_builder_add_text(
    sht_builder*, const char* name, uint64_t bytes, const char* text, sht_access access
);

sht_status sht_builder_add_binary(
    sht_builder*, const char* name, uint64_t bytes, const void* data, sht_access access
);

// Level = zstd compression level; pass <= 0 for sane default
sht_status sht_builder_add_text_compressed(
    sht_builder*, const char* name, uint64_t bytes, const char* text, int level
);

// Level = zstd compression level; pass <= 0 for sane default
sht_status sht_builder_add_binary_compressed(
    sht_builder*, const char* name, uint64_t bytes, const void* data, int level
);

// ===========================
// Builder Serialization

// Serializes to a a new buffer; caller must free(*out_buffer) if status was sht_status_ok
sht_status sht_builder_serialize(
    sht_builder*, void** out_buffer, size_t* out_bytes
);

#endif /* SESHAT_H */

#ifdef SESHAT_IMPL

// ===========================
// Depedency

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zstd.h>

// ===========================
// Constant

#define HEADER_BYTES 32u
#define MAGIC_VALUE  (uint8_t[8]){'S', 'E', 'S', 'H', 'A', 'T', '!', '?'}

// ===========================
// Helpers

// Align up 8 bytes
static inline uint64_t align8(uint64_t x) {
    return (x + 7ull) & ~7ull;
}

// Little endian reads

static inline uint32_t read_le32(const void* p) {
    const uint8_t* b = (const uint8_t*)p;
    return  (uint32_t)b[0]         |
            ((uint32_t)b[1] << 8)  |
            ((uint32_t)b[2] << 16) |
            ((uint32_t)b[3] << 24);
}
 
static inline uint64_t read_le64(const void* p) {
    const uint8_t* b = (const uint8_t*)p;
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | (uint64_t)b[i];
    return v;
}

static inline int64_t read_le64_signed(const void* p) {
    const uint8_t* b = (const uint8_t*)p; uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | (uint64_t)b[i];
    return (int64_t)v;
}

static inline double read_le_double(const void* p) {
    const uint8_t* b = (const uint8_t*)p; uint64_t bits = 0;
    for (int i = 7; i >= 0; i--) bits = (bits << 8) | (uint64_t)b[i];
    double value; memcpy(&value, &bits, sizeof(value));
    return value;
}

// Little endian writes

static inline void write_le32(uint8_t* dst, uint64_t* pos, uint32_t value) {
    for (int i = 0; i < 4; ++i) dst[*pos + i] = (uint8_t)(value >> (8 * i));
    *pos += 4;
}

static inline void write_le64(uint8_t* dst, uint64_t* pos, uint64_t value) {
    for (int i = 0; i < 8; ++i) dst[*pos + i] = (uint8_t)(value >> (8 * i));
    *pos += 8;
}

static inline void write_le64_signed(uint8_t* result, uint64_t* position, int64_t value) {
    uint64_t bits; memcpy(&bits, &value, sizeof(bits));
    write_le64(result, position, bits);
}

static inline void write_double(uint8_t* result, uint64_t* position, double value) {
    uint64_t bits; memcpy(&bits, &value, sizeof(bits));
    write_le64(result, position, bits);
}

static inline void write_bytes(uint8_t* result, uint64_t* position, const void* src, uint64_t size) {
    memcpy(result + *position, src, size); *position += size;
}

// Compare Ops

// Lexicographics name comparison
static int name_comp(uint8_t a_length, const uint8_t* a, uint8_t b_length, const uint8_t* b) {
    uint8_t min_length = a_length < b_length ? a_length : b_length;

    int cmp = memcmp(a, b, min_length);
    if (cmp != 0) return cmp;

    if (a_length < b_length) return -1;
    if (a_length > b_length) return 1;
    return 0;
}

// ===========================
// Viewer

struct sht_view {
    sht_access  access;

    const void* buffer;
    size_t      bytes;

    uint32_t    index_count;    // Number of index entries
    uint64_t    index_begin;    // Always 32 (sizeof header)

    uint64_t    names_begin;    // Byte offset of names array from buffer begin
    uint32_t    names_bytes;    // Names array size in bytes

    uint64_t    content_begin;  // Byte offset of content section from buffer begin
    uint64_t    content_bytes;  // Content section size in bytes
};
 
typedef struct view_entry {
    uint32_t    name_offset;
    uint32_t    type;
    uint64_t    data_bytes;
    uint64_t    data_offset;
} view_entry;

static inline view_entry view_read_index(const sht_view* viewer, uint32_t entry) {
    const uint8_t* p = (const uint8_t*)viewer->buffer + viewer->index_begin + (uint64_t)entry * 24;
    view_entry out = {
        .name_offset = read_le32(p + 0),
        .type        = read_le32(p + 4),
        .data_bytes  = read_le64(p + 8),
        .data_offset = read_le64(p + 16)
    };
    return out;
}
 
// Gets entry's at index name
// Returns 0 on out-of-bounds (corrupt file), 1 on success.
static inline int view_get_entry_name(const sht_view* viewer, uint32_t entry, const uint8_t** out_ptr, uint8_t* out_bytes) {
    view_entry idx = view_read_index(viewer, entry);
    if ((uint64_t)idx.name_offset >= viewer->names_bytes) return 0;
 
    const uint8_t* name_ptr = (const uint8_t*)viewer->buffer + viewer->names_begin + idx.name_offset;
    uint8_t length = name_ptr[0];
    if ((uint64_t)idx.name_offset + 1 + length > viewer->names_bytes) return 0;
 
    *out_bytes = length;
    *out_ptr = name_ptr + 1;
    return 1;
}
 
// Checks [data_offset, data_offset + data_bytes) is in view bounds
static inline int view_data_in_bounds(const sht_view* viewer, uint64_t data_offset, uint64_t data_bytes) {
    if (data_offset > viewer->content_bytes) return 0;
    uint64_t end = data_offset + data_bytes;
    if (end < data_offset) return 0; // overflow
    if (end > viewer->content_bytes) return 0;
    return 1;
}
 
sht_status sht_create_view(sht_view** target, const sht_view_create_info* info) {
    if (!target || !info || !info->buffer) return sht_status_err_bad_file;
    if (info->bytes < HEADER_BYTES) return sht_status_err_bad_file;
 
    const uint8_t* buf = (const uint8_t*)info->buffer;
 
    // Magic
    if (memcmp(buf, MAGIC_VALUE, 8) != 0) return sht_status_err_bad_file;
 
    // Version - only version 0 is understood
    uint64_t version = read_le64(buf + 8);
    if (version != 0) return sht_status_err_bad_file;
 
    uint32_t index_bytes   = read_le32(buf + 16);
    uint32_t names_bytes   = read_le32(buf + 20);
    uint64_t content_bytes = read_le64(buf + 24);
 
    // Index entries are fixed 24 bytes each
    if (index_bytes % 24 != 0) return sht_status_err_bad_file;
 
    uint64_t names_begin = (uint64_t)HEADER_BYTES + index_bytes;
    if (names_begin < index_bytes) return sht_status_err_bad_file;     // overflow guard
 
    uint64_t content_begin = align8(names_begin + names_bytes);
    if (content_begin < names_begin) return sht_status_err_bad_file;  // overflow guard
 
    uint64_t total_size = content_begin + content_bytes;
    if (total_size < content_begin) return sht_status_err_bad_file;    // overflow guard
    if (total_size > (uint64_t)info->bytes) return sht_status_err_bad_file;
 
    *target = malloc(sizeof(sht_view));
    if (!(*target)) return sht_status_err_allocation_failure;
 
    sht_view* view = *target;
    *view = (sht_view){
        .access         = info->access,
        .bytes          = info->bytes,
        .index_count    = index_bytes / 24,
        .index_begin    = HEADER_BYTES,
        .names_begin    = names_begin,
        .names_bytes    = names_bytes,
        .content_begin  = content_begin,
        .content_bytes  = content_bytes
    };
 
    if (info->access == sht_access_make_copy) {
        void* copy = malloc(info->bytes);
        if (!copy) {
            free(view); *target = NULL;
            return sht_status_err_allocation_failure;
        }
        memcpy(copy, info->buffer, info->bytes);
        view->buffer = copy;
    } 
    else {
        view->buffer = info->buffer;
    }
 
    return sht_status_ok;
}
 
void sht_free_view(sht_view* viewer) {
    if (!viewer) return;
    if (viewer->access != sht_access_read_unowned) {
        free((void*)viewer->buffer);
    }
    free(viewer);
}
 
// Queries entries count
sht_status sht_view_query_entry_count(sht_view* viewer, uint32_t* count) {
    if (!count) return sht_status_err_missing_out;
    *count = viewer->index_count;
    return sht_status_ok;
}
 
// Queries entry type at index
sht_status sht_view_query_entry_type(sht_view* viewer, uint32_t entry, sht_type* out_type) {
    if (!out_type)                      return sht_status_err_missing_out;
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;
 
    view_entry idx = view_read_index(viewer, entry);
    *out_type = (sht_type)idx.type;
    return sht_status_ok;
}
 
// Binary search entry by name (index is guaranteed sorted lexicographically per spec)
sht_status sht_view_find(sht_view* viewer, uint8_t name_len, const char* name, uint32_t* out) {
    if (!name) return sht_status_err_not_found;
    if (!out)  return sht_status_err_missing_out;

    uint32_t lo = 0, hi = viewer->index_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t* mid_name; uint8_t mid_len;

        if (!view_get_entry_name(viewer, mid, &mid_name, &mid_len)) return sht_status_err_bad_file;
        int cmp = name_comp(mid_len, mid_name, name_len, (const uint8_t*)name);

        if (cmp == 0) {
            *out = mid;  return sht_status_ok;
        }

        if (cmp < 0) lo = mid + 1;
        else         hi = mid;
    }
 
    return sht_status_err_not_found;
}
 
sht_status sht_view_get_as_int64(sht_view* viewer, uint32_t entry, int64_t* out) {
    if (!out)                           return sht_status_err_missing_out;      // Ensure out
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;        // Ensure entry
 
    view_entry idx = view_read_index(viewer, entry);
    if (idx.type != sht_type_int64)     return sht_status_err_type_mismatch;        // Ensure type
    if (idx.data_bytes != 8)            return sht_status_err_bad_file;             // Ensure size
    if (!view_data_in_bounds(viewer, idx.data_offset, idx.data_bytes)) return sht_status_err_bad_file;  // Ensure bounds
 
    const uint8_t* p = (const uint8_t*)viewer->buffer + viewer->content_begin + idx.data_offset;
    *out = read_le64_signed(p); return sht_status_ok;
}
 
sht_status sht_view_get_as_float64(sht_view* viewer, uint32_t entry, double* out) {
    if (!out)                           return sht_status_err_missing_out;      // Ensure out
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;        // Ensure entry
 
    view_entry idx = view_read_index(viewer, entry);
    if (idx.type != sht_type_float64)   return sht_status_err_type_mismatch;    // Ensure type
    if (idx.data_bytes != 8)            return sht_status_err_bad_file;         // Ensure size
    if (!view_data_in_bounds(viewer, idx.data_offset, idx.data_bytes)) return sht_status_err_bad_file;  // Ensure bounds
 
    const uint8_t* p = (const uint8_t*)viewer->buffer + viewer->content_begin + idx.data_offset;
    *out = read_le_double(p); return sht_status_ok;
}
 
// Zero-copy: hands back a pointer straight into the source buffer, no allocation
sht_status sht_view_get_as_bytes(sht_view* viewer, uint32_t entry, uint64_t* out_bytes, const uint8_t** out_ptr) {
    if (!out_bytes || !out_ptr)         return sht_status_err_missing_out;      // Ensure out
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;        // Ensure entry
 
    view_entry idx = view_read_index(viewer, entry);
    if (idx.type != sht_type_text && idx.type != sht_type_binary)       return sht_status_err_type_mismatch;    // Ensure type
    if (!view_data_in_bounds(viewer, idx.data_offset, idx.data_bytes))  return sht_status_err_bad_file;         // Ensure bounds
 
    *out_ptr = (const uint8_t*)viewer->buffer + viewer->content_begin + idx.data_offset;
    *out_bytes = idx.data_bytes; return sht_status_ok;
}
 
// Reads the uncompressed size prefix without decompressing anything
sht_status sht_view_get_as_uncompressed_size(sht_view* viewer, uint32_t entry, uint64_t* out_bytes) {
    if (!out_bytes)                     return sht_status_err_missing_out;  // Ensure out
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;    // Ensure entry
 
    view_entry idx = view_read_index(viewer, entry);
    if (idx.type != sht_type_text_compressed && idx.type != sht_type_binary_compressed) return sht_status_err_type_mismatch;    // Ensure type
    if (idx.data_bytes < 8)                                                             return sht_status_err_bad_file;         // Ensure size
    if (!view_data_in_bounds(viewer, idx.data_offset, idx.data_bytes))                  return sht_status_err_bad_file;         // Ensure bounds
 
    const uint8_t* p = (const uint8_t*)viewer->buffer + viewer->content_begin + idx.data_offset;
    *out_bytes = read_le64(p); return sht_status_ok;
}
 
// Decompresses fresh into the caller-supplied buffer every call - result is never cached
sht_status sht_view_get_as_decompress(sht_view* viewer, uint32_t entry, void* out_buf, uint64_t out_buf_size) {
    if (!out_buf)                       return sht_status_err_missing_out;  // Ensure out
    if (entry >= viewer->index_count)   return sht_status_err_bad_entry;    // Ensure entry
 
    view_entry idx = view_read_index(viewer, entry);
    if (idx.type != sht_type_text_compressed && idx.type != sht_type_binary_compressed) return sht_status_err_type_mismatch;    // Ensure type
    if (idx.data_bytes < 8)                                                             return sht_status_err_bad_file;         // Ensure size
    if (!view_data_in_bounds(viewer, idx.data_offset, idx.data_bytes))                  return sht_status_err_bad_file;         // Ensure bounds
 
    const uint8_t* p = (const uint8_t*)viewer->buffer + viewer->content_begin + idx.data_offset;
    uint64_t uncompressed_size = read_le64(p); const uint8_t* compressed_data  = p + 8; uint64_t compressed_bytes = idx.data_bytes - 8;
 
    if (out_buf_size < uncompressed_size) return sht_status_err_buffer_too_small;   // Ensure out buffer size
    size_t written = ZSTD_decompress(out_buf, (size_t)out_buf_size, compressed_data, (size_t)compressed_bytes);
    if (ZSTD_isError(written) || (uint64_t)written != uncompressed_size) return sht_status_err_bad_file;    // Check decompression succeeded
 
    return sht_status_ok;
}

// ===========================
// Builder

typedef struct builder_entry {
    uint8_t             name_length;
    uint8_t             name[255];
    sht_type            type;
    union {
        int64_t         int64;
        double          float64;
        struct {
            int         dofree;
            uint64_t    uncompressed;
            uint64_t    length;
            const void* begin;
        } data_block;
    } value;
} builder_entry;

struct sht_builder {
    // Unsorted entries dynamic array
    uint32_t        entries_count;
    uint32_t        entries_capacity;
    builder_entry*  entries;
};

sht_status sht_create_builder(sht_builder** target) {
    *target = calloc(1, sizeof(sht_builder));
    if (!(*target)) return sht_status_err_allocation_failure;
    return sht_status_ok;
}

static inline int no_free_type(sht_type type) {
    if (type == sht_type_int64 || type == sht_type_float64) return 1;
    return 0;
}

void sht_free_builder(sht_builder* builder) {
    if (!builder) return;
    for (uint32_t i = 0; i < builder->entries_count; i++) {
        builder_entry* entry = &builder->entries[i];
        if (no_free_type(entry->type))       continue;
        if (!entry->value.data_block.dofree) continue;
        free((void*)entry->value.data_block.begin);
    }
    free(builder->entries);
    free(builder);
}

static inline builder_entry* add_entry(sht_builder* builder, const char* name) {
    if (builder->entries_count + 1 >= builder->entries_capacity) {
        uint32_t new_cap = builder->entries_capacity * 2;
        if (new_cap == 0) new_cap = 16;
        builder_entry* new_entries = realloc(builder->entries, new_cap * sizeof(builder_entry));
        if (!new_entries) return NULL; // Failure
        builder->entries            = new_entries;
        builder->entries_capacity   = new_cap;
    }

    builder_entry* entry = &builder->entries[builder->entries_count++];
    *entry = (builder_entry){0};    // Initialize entry
    for (int i = 0; i < 255; i++) {
        if (name[i] == '\0') break;
        entry->name[i] = name[i];
        entry->name_length++;
    }

    return entry;
}

static inline sht_status link_data_block(builder_entry* entry, uint64_t bytes, const void* data, sht_access access) {
    switch (access) {
    case sht_access_claim_ownership: {
        entry->value.data_block.dofree  = 1;
        entry->value.data_block.length  = bytes;
        entry->value.data_block.begin   = data;
    } break;
    case sht_access_read_unowned: {
        entry->value.data_block.dofree  = 0;
        entry->value.data_block.length  = bytes;
        entry->value.data_block.begin   = data;
    } break;
    case sht_access_make_copy: {
        entry->value.data_block.dofree  = 1;
        entry->value.data_block.length  = bytes;
        entry->value.data_block.begin   = malloc(bytes);
        if (!entry->value.data_block.begin) return sht_status_err_allocation_failure;
        memcpy((void*)entry->value.data_block.begin, data, bytes);
    } break;
    }

    return sht_status_ok;
}

sht_status sht_builder_add_int64(
    sht_builder* builder, const char* name, int64_t value
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;
    
    entry->type = sht_type_int64;
    entry->value.int64 = value;

    return sht_status_ok;
}

sht_status sht_builder_add_float64(
    sht_builder* builder, const char* name, double value
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;
    
    entry->type = sht_type_float64;
    entry->value.float64 = value;

    return sht_status_ok;
}

sht_status sht_builder_add_text(
    sht_builder* builder, const char* name, uint64_t bytes, const char* text, sht_access access
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;

    entry->type = sht_type_text;
    sht_status status = link_data_block(entry, bytes, text, access);

    if (status != sht_status_ok) builder->entries_count--; // remove malformed
    return status;
}

sht_status sht_builder_add_binary(
    sht_builder* builder, const char* name, uint64_t bytes, const void* data, sht_access access
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;

    entry->type = sht_type_binary;
    sht_status status = link_data_block(entry, bytes, data, access);

    if (status != sht_status_ok) builder->entries_count--; // remove malformed
    return status;
}

sht_status sht_builder_add_text_compressed(
    sht_builder* builder, const char* name, uint64_t bytes, const char* text, int level
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;

    if (level <= 0) level = ZSTD_CLEVEL_DEFAULT;
    size_t bound = ZSTD_compressBound((size_t)bytes);

    void* compressed = malloc(bound);
    if (!compressed) {
        builder->entries_count--;
        return sht_status_err_allocation_failure;
    }

    size_t compressed_size = ZSTD_compress(compressed, bound, text, (size_t)bytes, level);
    if (ZSTD_isError(compressed_size)) {
        free(compressed);
        builder->entries_count--;
        return sht_status_err_allocation_failure;
    }

    // Shrink allocation
    void* tmp = realloc(compressed, compressed_size);
    if (tmp) compressed = tmp;

    entry->type = sht_type_text_compressed;
    entry->value.data_block.dofree          = 1;
    entry->value.data_block.uncompressed    = bytes;
    entry->value.data_block.begin           = compressed;
    entry->value.data_block.length          = compressed_size;

    return sht_status_ok;
}

sht_status sht_builder_add_binary_compressed(
    sht_builder*    builder,
    const char*     name,
    uint64_t        bytes,
    const void*     data,
    int             level
) {
    builder_entry* entry = add_entry(builder, name);
    if (!entry) return sht_status_err_allocation_failure;

    if (level <= 0) level = ZSTD_CLEVEL_DEFAULT;
    size_t bound = ZSTD_compressBound((size_t)bytes);

    void* compressed = malloc(bound);
    if (!compressed) {
        builder->entries_count--;
        return sht_status_err_allocation_failure;
    }

    size_t compressed_size = ZSTD_compress(compressed, bound, data, (size_t)bytes, level);
    if (ZSTD_isError(compressed_size)) {
        free(compressed);
        builder->entries_count--;
        return sht_status_err_allocation_failure;
    }

    // Shrink allocation
    void* tmp = realloc(compressed, compressed_size);
    if (tmp) compressed = tmp;

    entry->type = sht_type_binary_compressed;
    entry->value.data_block.dofree          = 1;
    entry->value.data_block.uncompressed    = bytes;
    entry->value.data_block.begin           = compressed;
    entry->value.data_block.length          = compressed_size;

    return sht_status_ok;
}

// Lexicographics name comparison for qsort of builder entry objects
static int name_comp_for_builder_entries(const void* a, const void* b) {
    const builder_entry* entry_a = (const builder_entry *)a;
    const builder_entry* entry_b = (const builder_entry *)b;
    return name_comp(entry_a->name_length, entry_a->name, entry_b->name_length, entry_b->name);
}

// Get entry content size
static inline uint64_t entry_data_bytes(builder_entry* entry) {
    switch (entry->type) {
    case sht_type_int64:            case sht_type_float64:              return 8;
    case sht_type_text:             case sht_type_binary:               return entry->value.data_block.length;
    case sht_type_text_compressed:  case sht_type_binary_compressed:    return 8 + entry->value.data_block.length;
    } return 0; // unreachable
}

sht_status sht_builder_serialize(sht_builder* builder, void** out_buffer, size_t* out_bytes) {
    // Sort by name
    qsort(builder->entries, builder->entries_count, sizeof(builder_entry), name_comp_for_builder_entries);

    // Ensure no names are equal, calculate names size, calculate content bytes
    uint64_t name_bytes = 0; uint64_t content_bytes = 0;

    for (uint32_t i = 0; i < builder->entries_count; ++i) {
        builder_entry* curr = &builder->entries[i];

        name_bytes    += 1 + curr->name_length;
        content_bytes =  align8(content_bytes);   // Spec: each content starts at 8-byte alignment
        content_bytes += entry_data_bytes(curr);

        if (i == 0) continue;

        builder_entry* prev = &builder->entries[i - 1];
        if (prev->name_length == curr->name_length &&
            memcmp(prev->name, curr->name, prev->name_length) == 0) {
            return sht_status_err_duplicate_names;
        }
    }

    // Calculate required file size
    uint64_t file_size = align8(32u + builder->entries_count * 24 + name_bytes) + content_bytes;

    uint8_t* result = malloc(file_size);
    if (!result) return sht_status_err_allocation_failure;

    uint64_t position = 0;

    // Write header
    write_bytes(result, &position, MAGIC_VALUE, 8);
    write_le64(result,  &position, 0); // Version
    write_le32(result,  &position, builder->entries_count * 24);
    write_le32(result,  &position, (uint32_t)name_bytes);
    write_le64(result,  &position, content_bytes);

    // Write index
    uint32_t name_offset = 0;
    uint64_t data_offset = 0;

    for (uint32_t i = 0; i < builder->entries_count; ++i) {
        builder_entry* entry = &builder->entries[i];
        data_offset = align8(data_offset); // Spec: each content starts at 8-byte alignment

        write_le32(result, &position, name_offset);
        write_le32(result, &position, entry->type);
        write_le64(result, &position, entry_data_bytes(entry));
        write_le64(result, &position, data_offset);

        name_offset += entry->name_length + 1;
        data_offset += entry_data_bytes(entry);
    }

    // Write names
    for (uint32_t i = 0; i < builder->entries_count; ++i) {
        builder_entry* entry = &builder->entries[i];
        write_bytes(result, &position, &entry->name_length, 1);
        write_bytes(result, &position, entry->name, entry->name_length);
    }

    // Write content
    for (uint32_t i = 0; i < builder->entries_count; ++i) {
        position = align8(position); // Spec: each content starts at 8-byte alignment
        builder_entry* entry = &builder->entries[i];

        switch (entry->type) {
        case sht_type_int64: {
            write_le64_signed(result, &position, entry->value.int64);
        } break;
        case sht_type_float64: {
            write_double(result, &position, entry->value.float64);
        } break;
        case sht_type_text_compressed: case sht_type_binary_compressed: {
            write_le64 (result, &position, entry->value.data_block.uncompressed);
            write_bytes(result, &position, entry->value.data_block.begin, entry->value.data_block.length);
        } break;
        case sht_type_text: case sht_type_binary: {
            write_bytes(result, &position, entry->value.data_block.begin, entry->value.data_block.length);
        } break;
        }
    }

    *out_buffer = result;
    *out_bytes = position;
    return sht_status_ok;
}

#endif // SESHAT_IMPL
