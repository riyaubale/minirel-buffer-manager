# Minirel Buffer Manager – Database Systems Project

A C++ implementation of a database buffer manager for the Minirel database system, featuring page management, buffer replacement, and disk I/O coordination using the Clock replacement algorithm.

---

## Project Overview

This project focuses on implementing the core buffer management layer of a DBMS. The buffer manager is responsible for efficiently managing memory-resident database pages and coordinating transfers between disk and main memory.

Key responsibilities include:
- Page caching
- Buffer replacement
- Dirty page handling
- Pin/unpin tracking
- Hash-based page lookup
- Disk page flushing

---

## Features

- Implemented a full database buffer manager in C++
- Designed page frame tracking using descriptor tables
- Implemented Clock replacement algorithm for buffer eviction
- Managed dirty page flushing to disk
- Supported page pinning and unpinning
- Built hash table mapping between disk pages and memory frames
- Integrated with Minirel I/O layer for disk operations
- Handled page allocation and file flushing operations

---

## Core Components

### Buffer Pool
- Fixed-size array of memory frames
- Stores database pages loaded from disk

### Buffer Descriptor Table (`BufDesc`)
Tracks metadata for each frame:
- Page number
- File pointer
- Pin count
- Dirty bit
- Reference bit
- Valid bit

### Buffer Hash Table (`BufHashTbl`)
Provides fast lookup for:
- `(File, PageNo) → FrameNo`

Implemented using:
- Chained bucket hashing

### Buffer Manager (`BufMgr`)
Central controller responsible for:
- Page reads
- Page replacement
- Page allocation
- Page flushing
- Clock algorithm management

---

## Clock Replacement Algorithm

Implemented an efficient approximation of LRU using the Clock algorithm:
- Frames arranged in a circular structure
- Uses reference bits to track recent access
- Evicts unpinned pages with cleared reference bits
- Writes dirty pages back to disk before replacement

---

## Key Methods Implemented

### `allocBuf()`
- Finds a free frame using the Clock algorithm
- Flushes dirty pages if necessary
- Removes old hash table entries

### `readPage()`
- Reads page from disk if not resident
- Returns existing frame if already cached
- Updates pin count and reference bits

### `unPinPage()`
- Decrements pin count
- Marks pages dirty when modified

### `allocPage()`
- Allocates new disk pages
- Assigns buffer frames
- Updates hash table mappings

### `flushFile()`
- Flushes all dirty pages of a file to disk
- Clears associated frames
- Removes hash entries

---

## Technologies Used

- C++
- Object-Oriented Programming
- Hash Tables
- Disk I/O Management
- Database Systems Concepts

---

## Project Structure

```text
.
├── buf.h             # Buffer manager class definitions
├── buf.C             # Buffer manager implementation
├── bufHash.C         # Hash table implementation
├── db.h              # DB and File class definitions
├── db.C              # DB and File implementations
├── page.h            # Page class definition
├── page.C            # Page implementation
├── error.h           # Error codes and definitions
├── error.C           # Error handling implementation
├── testbuf.C         # Test driver
└── makefile          # Build configuration
