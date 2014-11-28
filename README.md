# Space 2 Super

This little hack for X turns the spacebar key into another Super key when used in combination.
When used alone, it behaves like the ordinary space bar on the key release event.
Especially useful with XMonad, Awesome WM and the like.

## Prerequisites:
* Install X11 and XTEST development packages. On Debian GNU/Linux derivatives:

```bash
sudo apt-get install libx11-dev libxtst-dev
```
or, equivalently,
```bash
make deps
```
* If the program complains about a missing "XRecord" module, enable it by adding 'Load "Record"'
to the Module section of /etc/X11/xorg.conf (this step is unnecessary on most systems), e.g.:

```
    Section "Module"
            Load  "Record"
    EndSection
```

## Installation:
```bash
make deps && make && cp space2super s2sctl $PREFIX/bin
```

## Usage:
* Load Space2Super with "s2sctl start"
* Unload Space2Super with "s2sctl stop"
