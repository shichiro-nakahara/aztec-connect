#!/bin/bash

dd if=/dev/zero of=/swapfile_256 bs=1024 count=268435456

chmod 0600 /swapfile_256

mkswap /swapfile_256

swapon /swapfile_256