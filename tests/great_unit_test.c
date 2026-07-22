#include "../include/seshat/seshat.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void test_builder_create_destroy(void) {
    sht_builder* builder = NULL;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(builder != NULL);

    sht_free_builder(builder);
}

static void test_builder_add_int64(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "value", 123456789) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    assert(buffer != NULL);
    assert(bytes > 0);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_negative_int64(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "negative", -987654321) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_float64(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_float64(builder, "pi", 3.1415926535) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_text_copy(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    const char text[] = "hello world";

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_text(builder, "text", sizeof(text) - 1, text, sht_access_make_copy) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_binary_copy(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    uint8_t data[] = {1, 2, 3, 4, 5};

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_binary(builder, "bin", sizeof(data), data, sht_access_make_copy) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_empty_text(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_text(builder, "empty", 0, "", sht_access_make_copy) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_compressed_text(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    const char* text = "compressed text payload";

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_text_compressed(builder, "text", strlen(text), text, 1) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_builder_add_compressed_binary(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    uint8_t data[1024];

    memset(data, 0xaa, sizeof(data));

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_binary_compressed(builder, "binary", sizeof(data), data, 1) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    free(buffer);
    sht_free_builder(builder);
}

static void test_duplicate_names(void) {
    sht_builder* builder = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);

    assert(sht_builder_add_int64(builder, "same", 1) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "same", 2) == sht_status_ok);

    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_err_duplicate_names);

    sht_free_builder(builder);
}

static void test_view_create_invalid_buffer(void) {
    sht_view* view = NULL;
    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = NULL,
        .bytes = 0
    };

    assert(sht_create_view(&view, &info) != sht_status_ok);
}

static void test_roundtrip_int64(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    int64_t value = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "number", 42) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_get_as_int64(view, 0, &value) == sht_status_ok);
    assert(value == 42);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_roundtrip_float64(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    double value = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_float64(builder, "number", 1.25) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_get_as_float64(view, 0, &value) == sht_status_ok);
    assert(value == 1.25);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_roundtrip_text_zero_copy(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    uint64_t text_bytes = 0;
    const uint8_t* ptr = NULL;

    const char text[] = "zero copy";

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_text(builder, "text", strlen(text), text, sht_access_make_copy) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_get_as_bytes(view, 0, &text_bytes, &ptr) == sht_status_ok);

    assert(text_bytes == strlen(text));
    assert(memcmp(ptr, text, text_bytes) == 0);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_view_query_entry_count(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    uint32_t count = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);

    assert(sht_builder_add_int64(builder, "a", 1) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "b", 2) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "c", 3) == sht_status_ok);

    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_query_entry_count(view, &count) == sht_status_ok);
    assert(count == 3);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_view_find_existing(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    uint32_t entry = 999;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "hello", 1) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_find(view, "hello", &entry) == sht_status_ok);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_view_find_missing(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;
    uint32_t entry = 0;

    assert(sht_create_builder(&builder) == sht_status_ok);
    assert(sht_builder_add_int64(builder, "hello", 1) == sht_status_ok);
    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);
    assert(sht_view_find(view, "world", &entry) == sht_status_err_not_found);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

static void test_grand_roundtrip(void) {
    sht_builder* builder = NULL;
    sht_view* view = NULL;
    void* buffer = NULL;
    size_t bytes = 0;

    uint32_t count = 0;
    uint32_t entry = 0;

    int64_t int_value = 0;
    double float_value = 0;
    uint64_t data_bytes = 0;
    const uint8_t* data_ptr = NULL;

    uint8_t binary[4096];
    memset(binary, 0x5a, sizeof(binary));

    assert(sht_create_builder(&builder) == sht_status_ok);

    /* Many integer entries */
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "int_%d", i);

        assert(sht_builder_add_int64(builder, name, i * 1000) == sht_status_ok);
    }

    /* Many float entries */
    for (int i = 0; i < 50; i++) {
        char name[32];
        snprintf(name, sizeof(name), "float_%d", i);

        assert(sht_builder_add_float64(builder, name, i * 0.5) == sht_status_ok);
    }

    /* Text entries */
    const char text1[] = "The quick brown fox jumps over the lazy dog";
    const char text2[] = "Seshat grand roundtrip test payload";

    assert(sht_builder_add_text(
        builder,
        "description",
        sizeof(text1) - 1,
        text1,
        sht_access_make_copy
    ) == sht_status_ok);

    assert(sht_builder_add_text_compressed(
        builder,
        "compressed_text",
        sizeof(text2) - 1,
        text2,
        1
    ) == sht_status_ok);

    /* Large binary payload */
    assert(sht_builder_add_binary(
        builder,
        "large_binary",
        sizeof(binary),
        binary,
        1
    ) == sht_status_ok);

    assert(sht_builder_serialize(builder, &buffer, &bytes) == sht_status_ok);
    assert(buffer != NULL);
    assert(bytes > 0);

    sht_view_create_info info = {
        .access = sht_access_read_unowned,
        .buffer = buffer,
        .bytes = bytes
    };

    assert(sht_create_view(&view, &info) == sht_status_ok);

    assert(sht_view_query_entry_count(view, &count) == sht_status_ok);
    assert(count == 153); /* 100 ints + 50 floats + 3 payloads */

    /* Verify integer entry */
    assert(sht_view_find(view, "int_42", &entry) == sht_status_ok);
    assert(sht_view_get_as_int64(view, entry, &int_value) == sht_status_ok);
    assert(int_value == 42000);

    /* Verify float entry */
    assert(sht_view_find(view, "float_10", &entry) == sht_status_ok);
    assert(sht_view_get_as_float64(view, entry, &float_value) == sht_status_ok);
    assert(float_value == 5.0);

    /* Verify binary payload */
    assert(sht_view_find(view, "large_binary", &entry) == sht_status_ok);
    assert(sht_view_get_as_bytes(view, entry, &data_bytes, &data_ptr) == sht_status_ok);

    assert(data_bytes == sizeof(binary));
    assert(memcmp(data_ptr, binary, sizeof(binary)) == 0);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

void main(void) {
    test_builder_create_destroy();
    test_builder_add_int64();
    test_builder_add_negative_int64();
    test_builder_add_float64();
    test_builder_add_text_copy();
    test_builder_add_binary_copy();
    test_builder_add_empty_text();
    test_builder_add_compressed_text();
    test_builder_add_compressed_binary();
    test_duplicate_names();

    test_view_create_invalid_buffer();

    test_roundtrip_int64();
    test_roundtrip_float64();
    test_roundtrip_text_zero_copy();
    test_view_query_entry_count();
    test_view_find_existing();
    test_view_find_missing();
    
    test_grand_roundtrip();

    printf("All good\n");
}

#define SESHAT_IMPL
#include "../include/seshat/seshat.h"
