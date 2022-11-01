#!/bin/bash

rm -f pgsiftorder.o pgsiftorder.so

make clean
make

echo "Compiling ..."

#ISERVER="/usr/include/pgsql/server"
ISERVER="/usr/pgsql-9.4/include/server"
#IINT="/usr/include/pgsql/internal"
IINT="/usr/pgsql-9.4/include/internal"

gcc -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -grecord-gcc-switches -m64 -mtune=generic -DLINUX_OOM_SCORE_ADJ=0 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -fpic -I. -I. -I$ISERVER -I$IINT -D_GNU_SOURCE -c -o pgsiftorder.o pgsiftorder.c
gcc -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -grecord-gcc-switches -m64 -mtune=generic -DLINUX_OOM_SCORE_ADJ=0 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -fpic -L/usr/lib64 -Wl,-z,relro   -Wl,--as-needed  -shared -o pgsiftorder.so pgsiftorder.o

dir="$(/usr/pgsql-9.4/bin/pg_config --pkglibdir)"
echo "Libdir is: $dir"
/usr/bin/mkdir -p '/usr/lib64/pgsql'
/usr/bin/install -c -m 755  pgsiftorder.so '/usr/lib64/pgsql/'
/usr/bin/install -c -m 755  pgsiftorder.so $dir
#su postgres -c "psql tidb -c \"DROP FUNCTION IF EXISTS model_sum_real(real[], real[]) CASCADE;\""
#su postgres -c "psql tidb -c \"CREATE OR REPLACE FUNCTION model_sum_real(real[], real[]) RETURNS real[] AS 'pgsiftorder.so', 'c_model_sum_real' LANGUAGE C IMMUTABLE STRICT;\""
rm -f /tmp/db-install.sql
cp install.sql /tmp/db-install.sql
su postgres -c "psql tidb -f /tmp/db-install.sql"
echo "Done"
exit 0