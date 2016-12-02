#!/bin/sh

olddir=`pwd`
srcdir=`dirname $0`

cd "$srcdir"

PKGCONFIG=`which pkg-config`
if test -z "$PKGCONFIG"; then
        echo "*** pkg-config not found, please install it ***"
        exit 1
fi

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
else
        autoreconf --force --install --verbose || exit $?
fi

cd "$olddir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
