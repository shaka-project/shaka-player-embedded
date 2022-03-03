# ABI Guidelines

This document describes guidelines for maintaining ABI compatibility.  As a C++
project, we need to consider an app that loads a newer version of the library
while they compiled using an older version of the headers.  ABI
(application-binary interface) specifies how the executable will interact with
the library at a binary level.  By maintaining ABI compatibility, an app can
load newer versions of the library using a `.dll` or `.so` file and it will
still work.

We only maintain ABI compatibility within major release numbers.  v1.1 will be
compatible with v1.4, but not compatible with v2.1.  We make no promises about
partial compatibility between major versions.  It should be assumed that v2 is
completely incompatible with v1.

We also only maintain ABI compatibility with release versions and release
branches; main branch can break ABI at any time.  Any commit that will break
ABI should have a line in the commit message with `Abi-Breaking:` and a reason.


## Removing Features

When something is to be removed from the library, it can only be done on a major
version release.  Once a feature is decided to be removed, the next minor
release should mark the feature as deprecated so the compiler will warn the app
and a log will be printed.  We will make at least one minor release before
removing a feature.

For example, if we wanted to remove a feature, we would mark it deprecated in
v1.4.  Then when we release v2 we will remove the feature.  We will not remove
any features in v2 unless they were marked as deprecated in the previous minor
release.


## Guidelines for Developers

These guidelines are based on the guidelines by the KDE team:
https://community.kde.org/Policies/Binary_Compatibility_Issues_With_C%2B%2B

We only need to worry about ABI on the public types.  This means that we are
completely free to change anything about internal types.  This includes
implementations of public types.  For example, ImplementationHelper is a public
interface type, so we need to think about ABI for it; but the type that
implements that interface can be changed without breaking public ABI.


### What you can change

- Anything in a `.cc` file, `.js` file, or an internal-only `.h` file.
- Add new static data members.
- Add new non-virtual functions (must appear last).
- Add new virtual functions (must appear last) (may not work on Windows).
- Add a new constructor.
- Change a private Impl class definition.
- Add values to an `enum` or `enum class`.
  - You can't change existing values.
  - You cannot change the underlying type (e.g. from `uint8_t` to `uint16_t`).
- Rename private members.

### What you cannot change

- The signature of a public function (return type, arguments, `const`).
  - You cannot add an argument even if it has a default value.
- The accessibility of an existing member.
  - You cannot even make a private member public.
- Class hierarchy.
- Add new virtual methods if the app will inherit from it.
- Add new non-static data members.
- Change the order of members.
- Insert a member before an existing member.

### Adding new types

If you define a new public type, you should do the following:

- Include a virtual, non-inline destructor.
  - Can be non-virtual if type is `final` or the type shouldn't be destroyed by
    the app.
- Include a non-inline copy and move constructor/assignment operator.
  - If the type shouldn't be moved/copied, explicitly delete them.
- Include an Impl type and private member, even if it doesn't use it.
  - If the type is an interface, you don't need this.
- Mark type as `final` unless it is an interface.
- `enum` or `enum class` should have an explicit base type.
