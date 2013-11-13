CL = cl
CXX = g++

PINTOOL = ../pintool
LINK = C:/ProgFiles86/Microsoft\ Visual\ Studio\ 10.0/VC/Bin/link.exe

ifeq ($(OS),Windows_NT)
	PYTHON = C:/Python27
	OBJECTS86 = pin-x86.obj py-x86.obj
	PINTOOLS = pypin-x86.dll
else
	PYTHON = /usr/include/python2.7
	OBJECTS86 = pin-x86.o py-x86.o
	OBJECTS64 = pin-x64.o py-x64.o
	PINTOOLS = pypin-x86.so pypin-x64.so
endif

default: $(OBJECTS86) $(OBJECTS64) $(PINTOOLS)

py-x86.obj: py.cpp
	$(CL) /c /MT /EHs- /EHa- /wd4530 /Gy /O2 /I$(PYTHON)/include $^ /Fo$@

pin-x86.obj: pin.cpp
	$(CL) /c /MT /EHs- /EHa- /wd4530 /DTARGET_WINDOWS \
		/DBIGARRAY_MULTIPLIER=1 /DUSING_XED /D_CRT_SECURE_NO_DEPRECATE  \
		/D_SECURE_SCL=0 /nologo /Gy /O2 /DTARGET_IA32 /DHOST_IA32 \
		/I$(PINTOOL)\source\include /I$(PINTOOL)\source\include\gen \
		/I$(PINTOOL)\source\tools\InstLib \
		/I$(PINTOOL)\extras\xed2-ia32\include \
		/I$(PINTOOL)\extras\components\include $^ /Fo$@

pypin-x86.dll: $(OBJECTS86)
	$(LINK) /DLL /EXPORT:main /NODEFAULTLIB /NOLOGO /INCREMENTAL:NO /OPT:REF \
		/MACHINE:x86 /ENTRY:Ptrace_DllMainCRTStartup@12 /BASE:0x55000000 \
		/LIBPATH:$(PINTOOL)\ia32\lib /LIBPATH:$(PINTOOL)\ia32\lib-ext \
		/LIBPATH:$(PINTOOL)\extras\xed2-ia32\lib /OUT:$@ $^ pin.lib \
		/LIBPATH:$(PYTHON)\Libs python27.lib \
		libxed.lib libcpmt.lib libcmt.lib pinvm.lib kernel32.lib ntdll-32.lib

test:
	make -C tests test

clean:
	rm '*.dll' '*.exp' '*.lib' '*.obj'
