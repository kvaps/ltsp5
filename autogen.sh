#!/bin/sh
set -e
cd localapps/
./autogen.sh
cd -
cd client/nbd-proxy/
./autogen.sh
cd -
