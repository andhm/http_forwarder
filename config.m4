dnl $Id$
dnl config.m4 for extension http_forwarder

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(http_forwarder, for http_forwarder support,
dnl Make sure that the comment is aligned:
dnl [  --with-http_forwarder             Include http_forwarder support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(http_forwarder, whether to enable http_forwarder support,
Make sure that the comment is aligned:
[  --enable-http_forwarder           Enable http_forwarder support])

if test "$PHP_HTTP_FORWARDER" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-http_forwarder -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/http_forwarder.h"  # you most likely want to change this
  dnl if test -r $PHP_HTTP_FORWARDER/$SEARCH_FOR; then # path given as parameter
  dnl   HTTP_FORWARDER_DIR=$PHP_HTTP_FORWARDER
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for http_forwarder files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       HTTP_FORWARDER_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$HTTP_FORWARDER_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the http_forwarder distribution])
  dnl fi

  dnl # --with-http_forwarder -> add include path
  dnl PHP_ADD_INCLUDE($HTTP_FORWARDER_DIR/include)

  dnl # --with-http_forwarder -> check for lib and symbol presence
  dnl LIBNAME=http_forwarder # you may want to change this
  dnl LIBSYMBOL=http_forwarder # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $HTTP_FORWARDER_DIR/$PHP_LIBDIR, HTTP_FORWARDER_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_HTTP_FORWARDERLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong http_forwarder lib version or lib not found])
  dnl ],[
  dnl   -L$HTTP_FORWARDER_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(HTTP_FORWARDER_SHARED_LIBADD)

  PHP_NEW_EXTENSION(http_forwarder, http_forwarder.c, $ext_shared)
fi
