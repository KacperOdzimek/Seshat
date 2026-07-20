# Seshat

File format, somewhat like JSON, but binary. Good for application assets archives. Supports compression.

## General Format

## Fundamental

This apply to entire file:
```
Endian:  Little
Float64: IEEE 754 binary64
```

## Header

Valid seshat file begins with header:

```
Position:       Begin of file
Align:          8 bytes
```
```
uint64 version      // Future-proof as for today only 0
uint64 magic        // Magic value, ascii encoded text "SESHAT!?"
uint32 index        // Index bytes
uint32 names        // Names bytes
uint64 content      // Content bytes
```

## Index

### Indices Array

Index is array of entries, after the header.

```
Position:       sizeof(header) // Already aligned 8 bytes
Align:          8 bytes
Entry align:    4 bytes
```
```
uint32 name offset  // Bytes offset from names array begin
uint64 data offset  // Data offset from content begin
uint16 data type    // Data type enumerator
uint16 data flags   // Data storage flags (bitflag)
```


### Names Array

Names array is directly after indices array. 

```
Position:       sizeof(header) + header:index_bytes
Align:          4 bytes
Entry align:    2 bytes
```
```
uint8 length
uint8 ascii[length]
```

## Contents

Contents are chunks of data linked to index fields.
Each chunk is guaranteed to keep 8 bytes align.
Chunks are guaranteed not to overlap.

```
Position:       sizeof(header) + header:index_bytes + header:names_bytes
Align:          8 bytes
Entry align:    8 bytes
```

## Data Types

Data type informs interpreter, what data is linked to entry.
Following data types exists:

### 0: Archive
A nested seshat file:

```
uint64 bytes    // Archive length
(Nested file)
```

### 1: Int64

```
int64 value     // Single int64 value
```

### 2: Float64
```
float64 value    // Single float64 value
```

### 3: UTF-8 Text
```
uint64 bytes   // Text length in bytes
(Text)
```

### 4: Binary
```
uint64 bytes    // Binary sequence bytes
(Binary data)
```

## Data Flags

Data flags are extra info about stored data.
Following flags exists (numered by bit from LSB):

### 0: Compressed

Data is compressed with Seshat-Compression algorithm.

