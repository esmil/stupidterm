A Stupid Terminal
=================

About
-----

This is a simple wrapper around the VTE terminal emulator
widget for GTK+. Its main feature is to easily start new
terminal windows from the command line. Eg.

```sh
$ st -- ssh -p 2222 myserver.com &
```

This will start a new terminal window running ssh, and
will automatically close when you exit the session.

A nifty trick is to add the following snippet to your
.bashrc so you don't have to add -- and & after every
command.

```sh
vt() { /usr/bin/st -- "$@" & }
```

No support for tabs or other bells and whistles are
implemnted. Use your window manager for that.


Installing
----------

Make sure you have VTE with API version 2.91 installed
along with headers, pkg-config, make and a C compiler.

```sh
$ git clone 'https://github.com/esmil/stupidterm.git'
$ cd stupidterm
$ make release
$ sudo make prefix=/usr install
```

The git repo also includes a PKGBUILD for Archlinux and
a spec-file for RPM based distros to build packages
directly from git.

Packaging status:

[![Packaging status](https://repology.org/badge/vertical-allrepos/stupidterm.svg)](https://repology.org/metapackage/stupidterm/versions)

Configuring
-----------

On startup stupidterm will look for ```stupidterm.ini``` in your user configuration dir.
Usually ```$HOME/.config/stupidterm.ini```.

Copy the included example there and edit it to your hearts content.


License
-------

stupidterm is based on the vte example terminal and still licensed under LGPL.
