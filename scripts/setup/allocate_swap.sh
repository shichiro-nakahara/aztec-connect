#!/bin/bash

echo "Allocating 512GB swap at /swapfile_512..."

dd if=/dev/zero of=/swapfile_512 bs=1024 count=536870912

chmod 0600 /swapfile_512

mkswap /swapfile_512

swapon /swapfile_512