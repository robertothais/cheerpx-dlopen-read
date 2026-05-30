# syntax=docker/dockerfile:1
# check=skip=FromPlatformFlagConstDisallowed

FROM --platform=linux/386 debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*

COPY repro.c main.c /tmp/

# librepro.so: built non-PIC (-fno-pic, -z notext) so it carries text
# relocations the loader patches directly into .text, like the library this
# was found with. main: reads the .so before and after dlopen and diffs them,
# and forks a never-loading child to check the read cross-process.
RUN gcc -shared -fno-pic -Wl,-z,notext -O2 -o /usr/local/lib/librepro.so /tmp/repro.c \
    && gcc -O2 -o /usr/local/bin/main /tmp/main.c -ldl \
    && rm /tmp/repro.c /tmp/main.c \
    && apt-get purge -y gcc libc6-dev && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/* /var/cache/* /tmp/*

CMD ["/usr/local/bin/main"]
