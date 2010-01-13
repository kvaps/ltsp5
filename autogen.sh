#!/bin/sh
set -e
cd localapps/
./autogen.sh
cd -
cd nbd-proxy/
./autogen.sh
cd -
