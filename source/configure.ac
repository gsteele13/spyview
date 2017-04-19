# Process this file with autoconf to produce a configure script.
AC_INIT([spyview], [0.1], [Gary Steele (gsteele@mit.edu)])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(subdir-objects)
AC_CONFIG_SRCDIR([spyview/ImageWindow.C])
AC_CONFIG_HEADER([config.h])
USER_DIRS

#
# Work out our target
#
if test $target_os = mingw32msvc; then
  windows_target=yes
  AC_DEFINE(WIN32,[1],[True if compiling for win32])
#  AC_DEFINE(USER_DATA_ABSOLUTE,dataRelative,[Directory to use for user-owned data, like config files])
  AC_SUBST(WIN32_EXTRA,['spyview_console${EXEEXT} spybrowse_console${EXEEXT}'])
  AC_SUBST(WIN32_CONSOLE_FLAGS, ['-mwindows -mconsole'])
else
#  AC_DEFINE(USER_DATA_ABSOLUTE,["~/.xform3d/"])
  windows_target=no
  AC_SUBST(WIN32_EXTRA,[])
  AC_SUBST(WIN32_CONSOLE_FLAGS, [])
fi
AM_CONDITIONAL(WINDOWS,test $windows_target = yes)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

# Check for C++11 language standard
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])

# Check for fluid
AC_PATH_PROG(FLUID,fluid,none)
AC_ARG_VAR(FLUID,[path to executable to compile .fl files])
if test $FLUID = none; then
  AC_MSG_ERROR([Cannot find fluid])
fi

AC_PATH_PROG(GROFF,groff,none)
AC_PATH_PROG(PSTOPDF,ps2pdf,none)
AC_ARG_VAR(GROFF,[path to groff])
AC_ARG_VAR(PSTOPDF,[path to ps2pdf])
AM_CONDITIONAL(MAKEPDF,test  $GROFF != none  -a  $PSTOPDF != none)

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lfltk':
if test $windows_target = no; then
if test $target_vendor != apple; then
 AC_CHECK_LIB([GL], [main], ,AC_MSG_ERROR([libGL not found]))
 AC_CHECK_LIB([GLU], [main], ,AC_MSG_ERROR([libGLU not found]))
fi
#
# Windows checks and libraries
#
else
##  LIBS="$LIBS -lgdi32 -lopengl32_local -lglu32_local -lole32 -lwsock32 -luuid"
  AC_CHECK_LIB(gdi32,main,[],AC_MSG_ERROR([cannot find libgdi32]))
  AC_CHECK_LIB(opengl32,main,[],AC_MSG_ERROR([cannot find libopengl32]))
  AC_CHECK_LIB(glu32,main,[],AC_MSG_ERROR([cannot find libglu32]))
  AC_CHECK_LIB(ole32,main,[],AC_MSG_ERROR([cannot find libole32]))
  AC_CHECK_LIB(wsock32,main,[],AC_MSG_ERROR([cannot find libwsock32]))
  AC_CHECK_LIB(uuid,main,[],AC_MSG_ERROR([cannot find libuuid]))
fi

AC_LANG_PUSH([C++])
AC_CHECK_LIB([z], [main], , AC_MSG_ERROR([libz not found]))
AC_CHECK_LIB([png], [main], , AC_MSG_ERROR([libpng not found]))
AC_CHECK_LIB([fltk], [main], , AC_MSG_ERROR([libfltk not found]))
AC_CHECK_LIB([fltk_jpeg], [main], , AC_MSG_WARN([libjpeg not found: assuming you are using system version]))
AC_CHECK_LIB([fltk_images], [main], , AC_MSG_ERROR([libfltk_images not found]))
AC_CHECK_LIB([fltk_gl], [main], ,AC_MSG_ERROR([libfltk_gl not found]))
AC_CHECK_LIB([boost_serialization],[main],,AC_CHECK_LIB([boost_serialization-mt],[main],,AC_MSG_ERROR([libboost-serialization-dev library not found.])))
AC_CHECK_LIB([boost_regex],[main],,AC_CHECK_LIB([boost_regex-mt],[main],,AC_MSG_ERROR([libboost-regex-dev library not found.])))
AC_LANG_POP

#AC_CHECK_LIB([gnuplot_i], [gnuplot_init], , AC_MSG_ERROR([libgnuplot_i not found]))
#AC_CHECK_LIB([pgm], [main], , AC_MSG_ERROR([libpgm not found]))
#AC_CHECK_LIB([ppm], [main], , AC_MSG_ERROR([libppm not found]))
AC_CHECK_LIB([netpbm], [main], , AC_MSG_ERROR([libnetpbm not found]))
#AC_CHECK_LIB([pthread], [main], , AC_MSG_ERROR([libpthread not found]))
AC_CHECK_LIB([m], [round], , AC_MSG_ERROR([libm math library not found]))

# Check for Xwindows include paths
AC_PATH_XTRA

# Set the default compiler flags. Add xlib stuff.
# Also define GNU_SOURCE so we can use functions like round()
extracflags="-Wall -D_GNU_SOURCE -fpermissive"
CFLAGS="$CFLAGS$X_CFLAGS $extracflags"
CXXFLAGS="$CXXFLAGS$X_CFLAGS $extracflags"
if test $windows_target = no; then
LIBS="$LIBS $X_LIBS -lXext -lX11"
LDFLAGS="$X_LIBS $LDFLAGS"
else
LIBS="$LIBS"
fi


# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Directory info
if test $windows_target = yes; then
  AC_DEFINE(_STF,["%d"],[Format string for a size_t])
  AC_DEFINE(DIRECTORY_SEPARATOR,'\\',[Directory separator on target system])
else
  AC_DEFINE(DIRECTORY_SEPARATOR,'/')
  AC_DEFINE(_STF,["%zd"],[Format string for a size_t])
fi
AC_DEFINE_UNQUOTED(SPYVIEW_PREFIX,["$prefix"],[Directory we're ultimately installing to])
CPPFLAGS="$CPPFLAGS -DSPYVIEW_DATADIR='\"$datadir\"'"

# Checks for library functions.
AC_CHECK_FUNCS([pow strrchr strstr symlink])

AC_CONFIG_FILES([Makefile spyview/Makefile debian/Makefile])
AC_OUTPUT
