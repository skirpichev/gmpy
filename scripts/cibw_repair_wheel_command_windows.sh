#!/bin/bash

set -e -x

wheel=$WHEELNAME
dest_dir=$WHEELHOUSE

# XXX: this dumps *.dll and *.lib to the site-packages/
pip install -U git+https://github.com/skirpichev/delvewheel.git@fix-lib-sdir
delvewheel repair ${wheel} -w ${dest_dir} --add-path .local/bin --no-mangle-all -v

cp .local/bin/{gmp,mpfr,mpc}.lib ${dest_dir}
(cd ${dest_dir}; wheel unpack --dest . gmpy2-*.whl; mv *.lib gmpy2-*/*/platlib/; wheel pack gmpy2-*[0-9]; rm -rf gmpy2-*[0-9])
