
Debian
====================
This directory contains files used to package infinitumd/infinitum-qt
for Debian-based Linux systems. If you compile infinitumd/infinitum-qt yourself, there are some useful files here.

## infinitum: URI support ##


infinitum-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install infinitum-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your infinitum-qt binary to `/usr/bin`
and the `../../share/pixmaps/infinitum128.png` to `/usr/share/pixmaps`

infinitum-qt.protocol (KDE)

