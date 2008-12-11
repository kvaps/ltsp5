#!/bin/sh
set -e
aclocal
autoconf
automake -a -c
