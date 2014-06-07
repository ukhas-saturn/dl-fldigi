AC_DEFUN([AC_FLDIGI_WEBP], [
    AC_CHECK_HEADER([webp/decode.h], [], [AC_MSG_WARN([could not find webp/decode.h])])
    AC_CHECK_LIB([webp], [WebPDecodeRGB], [], [AC_MSG_WARN([could not find libwebp])])
])
