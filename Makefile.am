ACLOCAL_AMFLAGS=-I m4
plugindir=${rofi_PLUGIN_INSTALL_DIR}/

plugin_LTLIBRARIES = files.la

files_la_SOURCES=\
		 src/files.c

files_la_CFLAGS= @glib_CFLAGS@ @rofi_CFLAGS@ @gtk_CFLAGS@
files_la_LIBADD= @glib_LIBS@ @rofi_LIBS@ @gtk_LIBS@
files_la_LDFLAGS= -module -avoid-version
