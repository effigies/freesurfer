## 
## Makefile.am 
##

AM_CFLAGS=-I$(top_srcdir)/include

AM_CXXFLAGS=-I$(top_srcdir)/include \
             $(BOOST_CFLAGS)

AM_LDFLAGS=

bin_PROGRAMS = mri_ca_register

mri_ca_register_SOURCES=mri_ca_register.c
mri_ca_register_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH)) 
mri_ca_register_LDFLAGS=$(OS_LDFLAGS)

foodir=$(prefix)/docs/xml
foo_DATA=mri_ca_register.help.xml mri_ca_register_cuda.help.xml
foo2dir=$(prefix)/docs/html
foo2_DATA=mri_ca_register.help.xml.html mri_ca_register_cuda.help.xml.html

## ----
## CUDA
## ----

# BUILDCUDA is defined if configure.in finds CUDA
if BUILDCUDA

# rules for building cuda files
.cu.o:
	$(NVCC) -o $@ -c $< $(NVCCFLAGS) $(CUDA_CFLAGS) $(AM_CFLAGS) $(MNI_CFLAGS)

bin_PROGRAMS += mri_ca_register_cuda

mri_ca_register_cuda_SOURCES = mri_ca_register.c \
	mri_ca_register_cuda.help.xml.h
mri_ca_register_cuda_CFLAGS = $(CUDA_CFLAGS) $(AM_CFLAGS) -DFS_CUDA
mri_ca_register_cuda_CXXFLAGS = $(CUDA_CFLAGS) $(AM_CFLAGS) -DFS_CUDA
mri_ca_register_cuda_LDADD = $(addprefix $(top_builddir)/, $(LIBS_CUDA_MGH)) $(CUDA_LIBS)
mri_ca_register_cuda_LDFLAGS = $(OS_LDFLAGS) 
mri_ca_register_cuda_LINK = $(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=link $(CCLD) $(mri_ca_register_cuda_CFLAGS) \
        $(CFLAGS) $(mri_ca_register_cuda_LDFLAGS) $(LDFLAGS) -o $@

endif
# endif BUILDCUDA

# trick to get test data into the build directory
foo_SOURCES=
foo:
	if ! test -f "$(top_builddir)/mri_ca_register/testdata.tar.gz"; then \
		cp -v $(top_srcdir)/mri_ca_register/testdata.tar.gz \
					$(top_builddir)/mri_ca_register/; fi;

check_PROGRAMS=foo

TESTS=test_mri_ca_register $(top_builddir)/scripts/help_xml_validate

clean-local:
	rm -f $(BUILT_SOURCES)

EXTRA_DIST=$(foo_DATA) $(BUILT_SOURCES)

# Our release target. Include files to be excluded here. They will be
# found and removed after 'make install' is run during the 'make
# release' target.
EXCLUDE_FILES=""
include $(top_srcdir)/Makefile.extra

BUILT_SOURCES=mri_ca_register.help.xml.h mri_ca_register_cuda.help.xml.h \
	mri_ca_register.help.xml.html mri_ca_register_cuda.help.xml.html
