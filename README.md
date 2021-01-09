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
* Load Space2Super with `s2sctl start`.
* Unload Space2Super with `s2sctl stop`.
* **IMPORTANT**: Whenever your key code mappings are changed (e.g. by `setxkbmap`),
    re-apply the Space2Super-specific changes with `s2sctl remap`
    (otherwise your Space key will produce no effect, even while just typing).
* You can check whether Space2Super is running by executing `s2sctl running`,
    which exits with a zero (success) code if Space2Super is active
    (e.g. use `if s2sctl running` in scripts).
* `s2sctl` also accepts `--quiet` as a second argument which suppresses non-critical messages.
* In `$XDG_CONFIG_HOME/space2super/config`, you can configure Space2Super typing timeout,
    i.e. the amount of time that should pass between a single Space press and the consequent release
    for it to count as typing a space character, by adding a line `timeout_millisec NUMBER`
    (`$XDG_CONFIG_HOME` is `~/.config` by default if unset).
