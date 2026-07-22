# Seshat

Seshat is a lightweight, minimalist binary data format designed to support user-defined formats. It provides a simple indexed structure for efficient embedding of arbitrary data blocks, metadata, and binary assets while keeping access predictable and reasonably fast (O(log n)).

# Contents

This repository contains Seshat standard documents in ``formats`` folder, and a C99 single-header-library for seshat read/write inside ``include/seshat``.

# Versions

| Index | Markdown Document | Changes | Implementations |
| - | - | - | - |
| 0 | [Format Version 0](formats/V0.md) | First Version | [C99 API](include/seshat/seshat.h)

## License

This project is licensed under MIT License. See [LICENSE.md](LICENSE.md).

## Zstandard license

This project uses Zstandard (zstd).
Copyright (c) Meta Platforms, Inc. and contributors.
Licensed under the BSD 3-Clause License or GPLv2.
Zstandard: https://github.com/facebook/zstd
