#!/bin/bash

set -e -x

wheel=$WHEELNAME
dest_dir=$WHEELHOUSE

pip install -U --force-reinstall git+https://github.com/skirpichev/delvewheel.git@fix-lib-sdir
delvewheel repair ${wheel} -w ${dest_dir} --add-path .local/bin --no-mangle-all -v

cp .local/bin/{gmp,mpfr,mpc}.lib ${dest_dir}
(cd ${dest_dir}; wheel unpack --dest . gmpy2-*.whl; mv *.lib gmpy2-*/gmpy2.libs; wheel pack gmpy2-*[0-9]; rm -rf gmpy2-*[0-9])
