#include "../include/seshat/seshat.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    assert(sht_view_find(view, 5, "hello", &entry) == sht_status_ok);

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
    assert(sht_view_find(view, 5, "world", &entry) == sht_status_err_not_found);

    sht_free_view(view);
    free(buffer);
    sht_free_builder(builder);
}

#include <stdio.h>

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

    printf("All good\n");
}

#define SESHAT_IMPL
#include "../include/seshat/seshat.h"
