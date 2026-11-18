# Reference build environment = the 42 cluster (Fedora 44) = CI.
# The Makefile builds/runs this image with --platform linux/amd64
# (Docker Desktop emulates it on Apple Silicon).
FROM fedora:44

RUN dnf install -y \
        make gcc binutils nasm \
        grub2-tools grub2-tools-extra grub2-pc-modules xorriso xz gawk \
        qemu-system-x86-core \
    && dnf clean all

WORKDIR /kfs
