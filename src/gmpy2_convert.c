/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * gmpy_convert.c                                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Python interface to the GMP or MPIR, MPFR, and MPC multiple precision   *
 * libraries.                                                              *
 *                                                                         *
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,               *
 *           2008, 2009 Alex Martelli                                      *
 *                                                                         *
 * Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014,                     *
 *           2015, 2016, 2017, 2018, 2019, 2020 Case Van Horsen            *
 *                                                                         *
 * This file is part of GMPY2.                                             *
 *                                                                         *
 * GMPY2 is free software: you can redistribute it and/or modify it under  *
 * the terms of the GNU Lesser General Public License as published by the  *
 * Free Software Foundation, either version 3 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * GMPY2 is distributed in the hope that it will be useful, but WITHOUT    *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or   *
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public    *
 * License for more details.                                               *
 *                                                                         *
 * You should have received a copy of the GNU Lesser General Public        *
 * License along with GMPY2; if not, see <http://www.gnu.org/licenses/>    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* This file contains all the conversion functions for gmpy2.
 *
 * Overview
 * --------
 * gmpy2 tries to optimize the performance and accuracy of conversions from
 * other numeric types. gmpy2 uses a LBYL (Look Before You Leap) approach and
 * identifies the numeric type before conversion before conversion to a gmpy2
 * type. The basic operations (+, -, *, /) are optimized to directly work with
 * some basic types such as C longs or doubles.
 */

/* ======================================================================== *
 * Miscellaneous helper functions.                                          *
 * ======================================================================== */

/* Since macros are used in gmpy2's codebase, these functions are skipped
 * until they are needed for the C API in the future.
 */

#if 0
static int GMPy_isInteger(PyObject *obj)
{
    return IS_INTEGER(obj) ? 1 : 0;
}

static int GMPy_isFraction(PyObject *obj)
{
    return (!strcmp(Py_TYPE(obj)->tp_name, "Fraction")) ? 1 : 0;
}

static int GMPy_isRational(PyObject *obj)
{
    return IS_RATIONAL(obj) ? 1 : 0;
}

static int GMPy_isReal(PyObject *obj)
{
    return IS_REAL(obj) ? 1 : 0;
}

static int GMPy_isComplex(PyObject *obj)
{
    return IS_COMPLEX(obj) ? 1 : 0;
}
#endif

/* GMPy_ObjectType(PyObject *obj) returns an integer that identifies the
 * object's type. The return values are:
 *
 *    0:       unknown type
 *    1:       MPZ or XMPZ type
 *    2:       Python Integer (for Python 2.x) or Long type
 *    3:       Has __mpz__ method
 *    4 - 15:  Future use for other integer types
 *    16:      MPQ type
 *    17:      Python Fraction type
 *    18:      Has __mpq__ method
 *    19 - 31: Future use for other integer types
 *    32:      MPFR type
 *    33:      PyFloat type
 *    34:      Has __mpfr__ method
 *    35 - 47: Future use for other real floating point types
 *    48:      MPC type
 *    49:      PyComplex type
 *    50:      Has __mpc__ method
 *    51 - 63: Future use for other complex float point types
 *
 * Exceptions are never raised.
 */

static int GMPy_ObjectType(PyObject *obj)
{
    /* Tests are sorted by order by (best guess of) most common argument type.
     * Complete tests are done last.
     */

    /* We only check for exact Python types first. */

    if MPZ_Check(obj) {
        return OBJ_TYPE_MPZ;
    }

    if PyIntOrLong_CheckExact(obj) {
        return OBJ_TYPE_PyInteger;
    }

    if MPFR_Check(obj) {
        return OBJ_TYPE_MPFR;
    }

    if PyFloat_CheckExact(obj) {
        return OBJ_TYPE_PyFloat;
    }

    if MPC_Check(obj) {
        return OBJ_TYPE_MPC;
    }

    if PyComplex_CheckExact(obj) {
        return OBJ_TYPE_PyComplex;
    }

    if MPQ_Check(obj) {
        return OBJ_TYPE_MPQ;
    }

    if IS_FRACTION(obj) {
        return OBJ_TYPE_PyFraction;
    }

    if XMPZ_Check(obj) {
        return OBJ_TYPE_MPZ;
    }

    /* Now we do the non-Exact type check for native types. */

    if PyIntOrLong_Check(obj) {
        return OBJ_TYPE_PyInteger;
    }

    if PyFloat_Check(obj) {
        return OBJ_TYPE_PyFloat;
    }

    if PyComplex_Check(obj) {
        return OBJ_TYPE_PyComplex;
    }

    /* Now we look for the presence of __mpz__, __mpq__, __mpfr__, and __mpc__.
     * Since a type may define more than one of the special methods, we perform
     * the checks in reverse order.
     */

    if HAS_MPC_CONVERSION(obj) {
        return OBJ_TYPE_HAS_MPC;
    }

    if HAS_MPFR_CONVERSION(obj) {
        return OBJ_TYPE_HAS_MPFR;
    }

    if HAS_MPQ_CONVERSION(obj) {
        return OBJ_TYPE_HAS_MPQ;
    }

    if HAS_MPZ_CONVERSION(obj) {
        return OBJ_TYPE_HAS_MPZ;
    }

    return 0;
}


/* mpz_set_PyStr converts a Python "string" into a mpz_t structure. It accepts
 * a sequence of bytes (i.e. str in Python 2, bytes in Python 3) or a Unicode
 * string (i.e. unicode in Python 3, str in Python 3). Returns -1 on error,
 * 1 if successful.
 */

static int
mpz_set_PyStr(mpz_ptr z, PyObject *s, int base)
{
    char *cp;
    Py_ssize_t len, i;
    PyObject *ascii_str = NULL;

    if (PyBytes_Check(s)) {
        len = PyBytes_Size(s);
        cp = PyBytes_AsString(s);
    }
    else if (PyUnicode_Check(s)) {
        ascii_str = PyUnicode_AsASCIIString(s);
        if (!ascii_str) {
            VALUE_ERROR("string contains non-ASCII characters");
            return -1;
        }
        len = PyBytes_Size(ascii_str);
        cp = PyBytes_AsString(ascii_str);
    }
    else {
        TYPE_ERROR("object is not string or Unicode");
        return -1;
    }

    /* Don't allow NULL characters */
    for (i = 0; i < len; i++) {
        if (cp[i] == '\0') {
            VALUE_ERROR("string contains NULL characters");
            Py_XDECREF(ascii_str);
            return -1;
        }
    }

    /* Check for leading base indicators. */
    if (base == 0) {
        if (len > 2 && cp[0] == '0') {
            if (cp[1] == 'b')      { base = 2;  cp += 2; }
            else if (cp[1] == 'o') { base = 8;  cp += 2; }
            else if (cp[1] == 'x') { base = 16; cp += 2; }
            else                   { base = 10; }
        }
        else {
            base = 10;
        }
    }
    else if (cp[0] == '0') {
        /* If the specified base matches the leading base indicators, then
         * we need to skip the base indicators.
         */
        if (cp[1] =='b' && base == 2)       { cp += 2; }
        else if (cp[1] =='o' && base == 8)  { cp += 2; }
        else if (cp[1] =='x' && base == 16) { cp += 2; }
    }

    /* delegate rest to GMP's _set_str function */
    if (-1 == mpz_set_str(z, cp, base)) {
        VALUE_ERROR("invalid digits");
        Py_XDECREF(ascii_str);
        return -1;
    }
    Py_XDECREF(ascii_str);
    return 1;
}

/* Format an mpz into any base (2 to 62). Bits in the option parameter
 * control various behaviors:
 *   bit 0: if set, output is wrapped with mpz(...) or xmpz(...)
 *   bit 1: if set, a '+' is included for positive numbers
 *   bit 2: if set, a ' ' is included for positive nubmers
 *   bit 3: if set, a '0b', '0o', or '0x' is included for binary, octal, hex
 *   bit 4: if set, no prefix is included for binary, octal, hex
 *
 * Note: if neither bit 3 or 4 is set, prefixes that match the platform
 * default are included.
 *
 * If base < 0, capital letters are used.
 *
 * If which = 0, then mpz formatting is used (if bit 0 set). Otherwise xmpz
 * formatting is used (if bit 0 is set).
 */

static char* _ztag = "mpz(";
static char* _xztag = "xmpz(";

static PyObject *
mpz_ascii(mpz_t z, int base, int option, int which)
{
    PyObject *result;
    char *buffer, *p;
    int negative = 0;
    size_t size;

    if (
        !(
          ((base >= -36) && (base <= -2)) ||
          ((base >= 2) && (base <= 62))
         )
       ) {
        VALUE_ERROR("base must be in the interval 2 ... 62");
        return NULL;
    }

    /* Allocate extra space for:
     *
     * minus sign and trailing NULL byte (2)
     * 'xmpz()' tag                      (6)
     * '0x' prefix                       (2)
     *                                  -----
     *                                   10
     *
     * And add one more to be sure...
     */

    size = mpz_sizeinbase(z, (base < 0 ? -base : base)) + 11;
    TEMP_ALLOC(buffer, size);

    if (mpz_sgn(z) < 0) {
        negative = 1;
        mpz_neg(z, z);
    }

    p = buffer;
    if (option & 1) {
        if (which)
            strcpy(p, _xztag);
        else
            strcpy(p, _ztag);
        p += strlen(p);
    }

    if (negative) {
        *(p++) = '-';
    }
    else {
        if (option & 2)
            *(p++) = '+';
        else if (option & 4)
            *(p++) = ' ';
    }

    if (option & 8) {
        if (base == 2)        { *(p++) = '0'; *(p++) = 'b'; }
        else if (base == 8)   { *(p++) = '0'; *(p++) = 'o'; }
        else if (base == 16)  { *(p++) = '0'; *(p++) = 'x'; }
        else if (base == -16) { *(p++) = '0'; *(p++) = 'X'; }
    }
    else if (!(option & 24)) {
    #ifdef PY2
        if (base == 8)        { *(p++) = '0'; }
    #else
        if (base == 2)        { *(p++) = '0'; *(p++) = 'b'; }
        else if (base == 8)   { *(p++) = '0'; *(p++) = 'o'; }
    #endif
        else if (base == 16)  { *(p++) = '0'; *(p++) = 'x'; }
        else if (base == -16) { *(p++) = '0'; *(p++) = 'X'; }
    }

    /* Call GMP. */
    mpz_get_str(p, base, z);
    p = buffer + strlen(buffer);

    if (option & 1)
        *(p++) = ')';
    *(p++) = '\00';

    result = Py_BuildValue("s", buffer);
    if (negative == 1) {
        mpz_neg(z, z);
    }
    TEMP_FREE(buffer, size);
    return result;
}
