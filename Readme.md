# Seshat

Seshat is a lightweight, minimalist binary data format designed to support user-defined formats. It provides a simple indexed structure for efficient embedding of arbitrary data blocks, metadata, and binary assets while keeping access predictable and reasonably fast (O(log n)).

# Contents

This repository contains Seshat standard documents in ``formats`` folder, and a C99 single-header-library for seshat read/write inside ``include/seshat``.

# Versions

| Index | Markdown Document | Changes | Implementations |
| - | - | - | - |
| 0 | [Format Version 0](formats/V0.md) | First Version | [C99 API](include/seshat/seshat.h)
