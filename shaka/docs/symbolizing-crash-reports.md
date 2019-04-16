Symbolizing Crash Reports
=========================

This doc describes how to manually symbolize a crash report on a release binary.
This should be read both by partners who send crash reports and for us to
actually symbolize it.  This process can only be done if we have the debug info
for the binary.  This only happens for official builds that we release.  If you
build from source, you can do this yourself with the debug info in the build
directory.

Release binaries are optimized and don't have debug info.  This means there is
no way to get a useful stack trace or to get useful info from local variables.
Going from a memory address to a symbol and line number is symbolizing.  This
allows us to see the real stack trace.


What is needed
--------------

First, we obviously need the stack trace.  The best option is a whole crash
dump (e.g. a "core" file on linux).  If you provide that, you're done. But we
can work with a copy of the stack trace text and some extra info.

It is important that the entries show the memory address by itself or something
like `foo.so+0x1234`.  We need the exact bytes.  Note that
`ExportedMethod+0x123` is not useful, it needs to be absolute or relative to the
dynamic library.

Next we'll need which exact library you used; this includes the version string,
OS, and architecture.  We'll also need the build ID of the library.  Run one of
the following commands to get it:

```sh
# On Mac (or iOS with dynamic library):
dwarfdump -u <library.so>
# On iOS:
dwarfdump -u <library.framework/library>
# On Linux:
readelf -n <library.so> | grep "Build ID"
```

Windows doesn't have an exact build ID.  Instead just give the file version.
This can be retrieved by right-clicking on the file and selecting "Details".
Or in a PowerShell window, run:

```sh
(Get-Command ./library.dll).FileVersionInfo.FileVersion
```

### Load address

Lastly, we'll need the load address of the library in your executable.  This is
the address in the binary's memory where our library is loaded.  Since this is
a shared library, it can appear anywhere in memory.  We need to map the address
in the stack trace to relative to our library so we know what the code is.

It is IMPORTANT to get the load address for the same invocation the stack trace
is from.  Dynamic libraries can appear at different locations, so you need to
use the same run to get the load address and the stack trace.

If you have a debugger open in Xcode, select
`Debug > Debug Workflow > Shared Libraries...`. Then give us the address for our
shared library.

If you have a debugger open in Visual Studio, open the "Modules" tab (if not
open, select `Debug > Windows > Modules`).  Then give us the "Address" for our
library.

If you have a crash log from an iOS device, there is a section for
`Binary Images:` that lists the shared libraries.  We need the one that matches
the build ID you got before.  To get logs for an attached device, open Xcode
and select `Window > Devices and Simulators`, select your device and click the
`View Device Logs` button.

If you have a crash dump for Mac or iOS and are using `lldb`, enter `image list`
and give us the address for our library.  It will probably be near the top.

If you have a core dump for Linux, open it in `gdb` and enter `info share`
and give us the address for our library.  It will probably be near the top.

If you have a core/minidump for Windows, open it in `cdb` or `windbg` and enter
`lm` and give us the start address for our library.


Symbolizing the stack trace
---------------------------

First you need the debug info for the build.  For Mac and iOS this is a `.dSYM`
file.  On Linux, we just strip the shared library, so it is just a `.so` file
that has been renamed to `.so.debug`.  Run the commands above to get the build
ID and verify it matches what the report gave us.

For Mac and iOS, run the following command.  This will print the function name,
file, and line number for each address given.  If the address can't be found it
will just print the address itself.

```sh
atos -o <library.dSYM> -l <LOAD_ADDRESS> <ADDR1> <ADDR2> ...
```

For Linux, subtract the load address from the address and enter it (as a hex
number) into the following command.  If it doesn't work, try subtracting one
from the number.

```sh
addr2line -Cf -j .text -e <library.so.debug> <OFFSET1> <OFFSET2> ...
```

For Windows, you'll need the [Windows debugger tools][1] (not Visual Studio).
This usually comes with the Windows SDK or the Windows Driver Kit.  You then
need to run `dbh.exe`, an interactive symbol viewer/searcher:

[1]: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/

```
$ dbh.exe -v library.pdb

> base <LOAD_ADDRESS>
> addr <ADDR1>
   name : my_symbol
   addr :   <ADDR1>
   size : 0
  flags : 400000
   type : 0
modbase :   7ff123456789
  value :        0
    reg : 0
  scope : SymTagNull (0)
    tag : SymTagPublicSymbol (a)
  index : 12ab

> laddr <ADDR1>
  file : C:\users\build\src\foo.cpp
  line : 123
  addr : <ADDR1>
   key : 000000000000000
```

Note that `laddr` might not work for third-party static libraries if the line
info isn't available; but the `addr` command should always work.


Symbolizing crash dumps
-----------------------

If you have a crash dump from an unknown program, you can still get debug info
from it.  This also can be done to add symbols to a debugger run if there was
problems loading the symbols.

### Linux

First, load the core dump (`gdb -c core`).  Then find our library in the loaded
shared library list (`info share`) and remember the start address.  Last, load
the symbols for our library (`add-symbol-file library.so.debug 0x12345`).  You
should now have symbols for all of our parts and can move around our code like
normal.

### Mac

For OSX core dumps, load the core dump (`lldb -c core`).  Then add our dSYM
file (`add-dsym library.dylib.dSYM`).

### Windows

For Windows core/mini dumps, you can add the path to the folder containing the
.pdb on the command-line.  For example: `cdb -z core.dmp -y symbol_folder` or
`windbg -z core.dmp -y symbol_folder`.  If you have an existing session and
don't have symbols, the run `.sympath+ symbol_folder` and `.reload`.  Be sure
to give the folder name and that the .pdb file hasn't been renamed.
