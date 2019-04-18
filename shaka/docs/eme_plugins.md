# Custom EME Plugins

This document describes how to create and register custom EME plugins.  Defining
a custom EME implementation allows the app to handle decrypting the media
content without handling media-specific features.  As of now, the EME
implementations cannot handle decoding; they can only decrypt the bytes.  This
means you can't implement secure decrypt-decode or secure rendering.


## Overview

First, you need to create the custom implementation types.  You need to define
derived types for [Implementation](@ref shaka::eme::Implementation) and
[ImplementationFactory](@ref shaka::eme::ImplementationFactory).  There will be
a single, global instance of the factory that is used to create instance of the
Implementation.  Each playback instance will get its own Implementation instance
and there can be multiple alive at once if there are multiple Player instances.

### Registering your plugin

Once you have written the type, you need to compile it into some library.  You
have two options:

First, you can compile your code into your app.  You can do so any way you like.
Then, before you create a Player instance, you need to create a global instance
of your ImplementationFactory and register it with the
[ImplementationRegistry](@ref shaka::eme::ImplementationRegistry). This factory
instance must live for the duration of the app.

Second, you can compile your code into a separate library.  Then you use the
plugin definition below to register with our build system.  When you build Shaka
Player Embedded, our build system will include your library and include the
plugin into the Embedded library.  The implementation(s) will be automatically
registered during library startup.


## Plugin Definition

If you want, you can create a JSON definition of the plugin and have it built
into the library.  This means the plugin will be auto-registered and you just
have to use the library like normal.  This also allows you to keep the library
and the plugin together and avoid versioning problems.

To define a plugin, you write a JSON file that describes your plugin.  It should
contain one or more libraries that will be built into the library.  Then you
indicate the headers where the implementations are found and we will generate
code to register those types.

To register your JSON file, pass the `--eme-impl` flag to `./configure`.  You
can pass this multiple times for multiple implementations:

```sh
./configure --eme-impl my-eme-def.json
```

### JSON definition


<table>
  <tr><th>Field Name</th><th>Field Type</th><th>Description</th></tr>
  <tr>
    <td>`libraries`</td><td>array of strings</td>
    <td>(optional) An array of paths to libraries to link against.</td>
  </tr>
  <tr>
    <td>`implementations`</td><td>array</td>
    <td>An array of implementation definitions for the implementations this
        plugin defines.</td>
  </tr>
</table>


<table>
  <caption>Implementation definition</caption>
  <tr><th>Field Name</th><th>Field Type</th><th>Description</th></tr>
  <tr>
    <td>`factory_type`</td><td>string</td>
    <td>A (possibly qualified) type name to the ImplementationFactory type.  See
        below for requirements.</td>
  </tr>
  <tr>
    <td>`header`</td><td>string</td>
    <td>The path to the header that defines this type.</td>
  </tr>
  <tr>
    <td>`key_system`</td><td>string</td>
    <td>The key system name that this implements. (e.g. `org.w3.clearkey`)</td>
  </tr>
  <tr>
    <td>`sources`</td><td>array of strings</td>
    <td>(optional) An array of paths to source files to compile.  These will be
        compiled into the library and can use internals.</td>
  </tr>
  <tr>
    <td>`include_dirs`</td><td>array of strings</td>
    <td>(optional) An array of header search paths to include when building.
        Useful when giving `sources`.</td>
  </tr>
</table>

#### Example

```json
{
  "libraries": [
    "build/cdm_adapter.a"
  ],
  "implementations": [
    {
      "factory_type": "my_ns::MyImplementationFactory",
      "header": "include/my_implementation_factory.h",
      "key_system": "com.example.cdm"
    }
  ]
}
```

### Dynamic libraries

It is suggested to use a static library to make building easier.  But you can
use a dynamic library or a framework instead.  For iOS builds, the library
or framework will be copied into the compiled framework; for non-iOS builds,
you are responsible for copying the dynamic library to the install directory
so the library loader can find it at runtime.

#### Factory requirements

Each implementation factory defined by the plugin must publicly implement the
ImplementationFactory type and must have an empty constructor.  The factory will
be allocated using `new`.  For static libraries, the type doesn't need to be
exported.

#### Paths

File paths in the JSON file can be absolute; if they are relative, they are
relative to the JSON file.  File paths can include format specifiers to give
different paths based on what is currently being built.  The format specifier is
in the format `%(name)s` where name is one of the below names.  For example,
`foo/%(config)s/bar` may give `foo/Debug/bar`.

| Name | Description | Possible values |
| ---- | ----------- | --------------- |
| config | The current configuration name | `Debug` or `Release` |
| arch | The target CPU architecture | `x86`, `x64`, `arm`, or `arm64` |
| os | The target OS | `linux`, `mac`, `win`, `ios`, or `android` |

#### Building plugin from source

It is possible to build your plugin from source instead of from a static
library.  If you pass the `sources` field, those files will be compiled into
the library.  This allows access to the library internals.  For example, this
can be used so your plugin can use the
[FileSystem](@ref shaka::util::FileSystem) type to avoid duplicating code.  Note
that internals are not ABI or API stable, so they may change at any time.
