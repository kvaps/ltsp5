#!/bin/sh
set -e
cd client/localapps/
./autogen.sh
cd -
cd client/nbd-proxy/
./autogen.sh
cd -
