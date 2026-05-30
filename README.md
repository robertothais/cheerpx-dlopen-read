# CheerpX `dlopen` file-read coherence

After a shared library is `dlopen`'d, reading that library's file from disk returns the relocated in-memory image instead of the on-disk bytes. The bytes that differ are the relocation sites the dynamic loader patched into the code at load time. Reading the same file _before_ `dlopen` returns the correct on-disk contents.

This surfaced in a library that performs a load-time integrity self-check. This is a common pattern in software that cares about tamper-resistance: once loaded, it reopens its own file on disk, hashes the image, and verifies a signature over it. Under CheerpX the read returns the relocated image rather than the signed on-disk bytes, so the hash never matches and the check fails. Because such a check is meant to fail closed, the program then stops rather than run on what it treats as a tampered image, so it does not run under CheerpX at all.

## Observed

`main` reads `/usr/local/lib/librepro.so` in full, `dlopen`s it, reads it again, and diffs the two buffers:

- before `dlopen`: matches the on-disk file
- after `dlopen`: differs at the library's text-relocation sites

`librepro.so` is a minimal non-PIC shared object (`DT_TEXTREL`) with relocations that land in `.text`. For example, `read_global()` compiles to `mov eax, ds:<addr>; ret`; on disk the address operand is the unrelocated placeholder, and after `dlopen` the read comes back with it relocated to the library's load base (which varies per run):

```
on-disk  : a1 00 00 00 00 c3   mov eax, ds:0x00000000 ; ret
read back: a1 04 30 dd af c3   mov eax, ds:0xafdd3004 ; ret
```

A position-independent (`-fPIC`) build, whose relocations land in the GOT rather than `.text`, reads back unchanged.

As a secondary observation, `main` also forks a child that never `dlopen`s the library and has it read the file. On Linux the child reads the on-disk bytes, unaffected by the parent's mapping; under CheerpX it reads the same relocated bytes, so the effect is not confined to the process that loaded the library.

## Specified behavior

`dlopen` maps the library `MAP_PRIVATE` (copy-on-write). Applying text relocations writes into the code pages, but on a private mapping those writes do not reach the underlying file, and are not visible to other processes reading it. `read(2)` of the file returns the on-disk contents whether or not the library is currently mapped or relocated.

## Requirements

- Docker
- pnpm

## Usage

```sh
pnpm install
pnpm build
pnpm test
```

`pnpm build` compiles `librepro.so` and `main` in an i386 Debian image and exports an ext2. `pnpm test` boots CheerpX (1.3.3 by default), runs `main`, and compares the two reads; it prints `REPRODUCED` and exits non-zero when they differ.

For comparison, the same image runs natively: `docker run --rm --platform linux/386 cheerpx-dlopen-read` prints `OK` on Linux.
