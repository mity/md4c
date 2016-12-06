#
# Makefile --
#
#	`NMAKE` file for `md4c` on Win32/MSVC
#
# Use:
#
#     nmake <target>
#     nmake CONF=Release <target>
#     nmake CONF=Release_MT <target>
#     nmake CONF=Debug <target>
#
# Special <targets>: `all` (the default), `clean`, `distclean`.

### Environment and Macros #############################################
#
# We use replacements for the default CC and MAKE values that
# result in a less noisy build process.

MAKE	= $(MAKE) /NOLOGO RECURSIVE=1
CC	= $(CC) /nologo $(DEFINES) 
AS	= $(AS) /nologo
AR	= LIB /NOLOGO

# Some useful shorthands.
RM	= -DEL /Q >NUL: 2>&1
LS	= -DIR /B/L 2>NUL:
LN	= LINK /nologo
MKDIR   = -MKDIR >NUL: 2>&1

### Project definitions ################################################

PLATFORM      = Win32
PROJECT       = md4c

CFLAGS_Release_MT  = /O2 /MT /W3 /DNDEBUG /I..\md4c 
ARFLAGS_Release_MT =
LIBS_Release_MT    =

CFLAGS_Release  = /O2 /MD /W3 /DNDEBUG /I..\md4c 
ARFLAGS_Release =
LIBS_Release    =

CFLAGS_Debug	= /ZI /MDd /W3 /I..\md4c 
ARFLAGS_Debug	=
LIBS_Debug	=

### File groups

PRODUCTS = $(TARGETS) $(OTHER) $(JUNK)	# All generated files.
OTHER = $(OUTDIR)\*.pdb 		# Not a target, not disposable.
JUNK = $(INTDIR)\*.obj                  # All "disposable" files.
CONF_FILE = build_env.bat		# File to store configuration.

### Configuration setup ################################################

# CONF=release:		Build "Release" project config (the default).
# CONF=release_mt:	Build "Release_MT" project config.
# CONF=debug:		Build "Debug" project config.
# CONF=env:		Use environment settings.


!IFDEF RECURSIVE
!UNDEF CONF
!ELSE # !RECURSIVE

!IFNDEF CONF
# No `CONF` given, use default.
CONF = release
# !MESSAGE Using default configuration: "$(CONF)".
!ENDIF # CONF

!IFNDEF CONF
# No `CONF` given, use default
CONF = Release
!ENDIF # CONF

!IF "$(CONF)" == "release" || "$(CONF)" == "Release"

CFLAGS = $(CFLAGS_Release)
ARFLAGS = $(ARFLAGS_Release)
OUTDIR = .\Release
OOO1 = $(MOD1:{=.\Release\md4c\)
TTT1 = $(OUT1:{=.\Release\)

!ELSEIF "$(CONF)" == "release_mt" || "$(CONF)" == "Release_MT"

CFLAGS = $(CFLAGS_Release_MT)
ARFLAGS = $(ARFLAGS_Release)
OUTDIR = .\Release_MT
OOO1 = $(MOD1:{=.\Release_MT\md4c\)
TTT1 = $(OUT1:{=.\Release_MT\)

!ELSEIF "$(CONF)" == "debug" || "$(CONF)" == "Debug"

CFLAGS = $(CFLAGS_Debug)
ARFLAGS = $(ARFLAGS_Debug)
OUTDIR = .\Debug
OOO1 = $(MOD1:{=.\Debug\md4c\)
TTT1 = $(OUT1:{=.\Debug\)

!ELSEIF "$(CONF)" == "env" || "$(CONF)" == "Env"
!MESSAGE Using environment for configuration.
!ELSE
!ERROR Unknown configuration name: "$(CONF)".
!ENDIF

# Configuration independent : Intermediate files, source location.

INTDIR = $(OUTDIR)\$(PROJECT)

!MESSAGE OUTDIR="$(OUTDIR)"
!MESSAGE INTDIR="$(INTDIR)"

SAVCONF = savcfg.bat $(JUNK)
!ENDIF # RECURSIVE

### Project Targets ####################################################

TARGETS = $(TGT1)


# Each target (TGT) is a `.exe`, `.dll`, or `.lib` built from source
# files (SRC), which in turn are compiled to object files (OBJ).
# Each target may also depend on other files (DEP), like headers.

OUT1 = {md2html}

MOD1 = {md2html} {cmdline} {entity}
OBJ1 = $(OOO1:}=.obj) $(INTDIR)\md4c.obj

DEP1 =

SSS1 = $(MOD1:{=..\md2html\)
SRC1 = $(SSS1:}=.c) ..\md4c\md4c.c

TGT1 = $(TTT1:}=.exe)

### Public Pseudotargets ###############################################

# `all`:	builds all targets, in the configuration given by the
#		environment, and this becomes the new current
#		configuration.

{..\md4c}.c{$(INTDIR)}.obj::
	$(CC) $(CFLAGS) "-Fd$(INTDIR)" "-Fo$(INTDIR)/" -c $<

{..\md2html}.c{$(INTDIR)}.obj::
	$(CC) $(CFLAGS) "-Fd$(INTDIR)" "-Fo$(INTDIR)/" -c $<

!MESSAGE TARGETS="$(TARGETS)"

all: savconf $(TARGETS) "$(OUTDIR)" "$(INTDIR)"

# `distclean`:	deletes all product files and the current configuration.

distclean:
	@ECHO Deleting products:
	@ECHO $(JUNK) $(OTHER) $(TARGETS) $(TGT1) $(CONF_FILE) = (
	@$(LS) $(JUNK) $(OTHER) $(TARGETS) $(TGT1)
	@$(RM) $(JUNK)
	@$(RM) $(OTHER)
	@$(RM) $(TARGETS)
	@$(RM) $(TGT1)
	@$(RM) $(CONF_FILE)
	@ECHO )

# `clean`:	deletes all "disposable" product files, but not the
#		targets and the configuration.

clean:
	@ECHO Deleting intermediate files: (
	@$(LS) $(JUNK)
	@$(RM) $(JUNK)
	@ECHO )

### Project build descriptions #########################################

$(TGT1): savconf $(OBJ1) $(DEP1) $(OUTDIR) $(INTDIR)
	$(CC) $(CFLAGS) "/Fe$(TGT1)" $(OBJ1)

$(OUTDIR):
	@$(MKDIR) "$(OUTDIR)"

$(INTDIR): $(OUTDIR)
	@$(MKDIR) "$(INTDIR)"

### Private pseudotargets ##############################################

$(TARGETS): savconf

savconf:
	@ECHO Using configuration "$(CONF)".
	@SET CFLAGS=$(CFLAGS)
	@SET ARFLAGS=$(ARFLAGS)
	@SET LIBS=$(LIBS)
	@SET CONF=$(CONF)
	@SET PROJECT=$(PROJECT)
	@SET OUTDIR=$(OUTDIR)
	-@$(SAVCONF)

### EOF ################################################################
# vim:noet:ts=8:sw=8:tw=0

