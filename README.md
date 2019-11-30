# Space2Super

This little hack for X turns the spacebar key into another Super key when used in combination.
When used alone, it behaves like the ordinary space bar on the key release event.
Especially useful with e.g. tiling window managers.

So it is like [Space2Ctrl](https://github.com/r0adrunner/Space2Ctrl) from which it was forked
but for the Space-to-Super case.
Please also take note of [xcape](https://github.com/alols/xcape) which, when paired with `xmodmap`,
can emulate both Space2Ctrl and Space2Super.


## Prerequisites:
* Install the XTEST development package. On Debian GNU/Linux derivatives:
```bash
sudo apt-get install libxtst-dev
```
or, equivalently:
```bash
make deps
```
* If the program complains about a missing "XRecord" module, enable it by adding 'Load "Record"'
to the Module section of ``/etc/X11/xorg.conf`` (this step is unnecessary on most systems), e.g.:
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
* Load Space2Super with "s2sctl start".
* Unload Space2Super with "s2sctl stop".
