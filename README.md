# NPM review

[ncurses](https://en.wikipedia.org/wiki/Ncurses)-based [TUI](https://en.wikipedia.org/wiki/Text-based_user_interface) for reviewing and upgrading/downgrading NPM packages, viewing package info, and dependencies, with vim-like key-bindings.

Heavily inspired by [tig](https://github.com/jonas/tig).

![Example](/example.gif)

## Flags

  - `-V` – Initialize with [version check](#version-check-v)
  - See [development](#development) for more

## Key-bindings

The active window is the _alternate_ window, if it is displayed, _package_ window otherwise.

| Key(s)  | Action |
| ------- | ------- |
|      `j`  | Move cursor down in the active window |
|      `k`  | Move cursor up in the active window |
| `J` or ⬇️  | Move cursor down in the _package_ window |
| `K` or ⬆️  | Move cursor up in the _package_ window |
|      `q`  | Close the active window (exit if in _package_ window) |
|      `Q`  | Exit |
|  `Enter`  | Show versions, or `npm install` selected version |
|      `D`  | `npm uninstall` selected package |
|      `V`  | Check version of all packages against latest |
|      `i`  | `npm info` of the selected package |
|      `I`  | Show dependencies for selected package |
|      `l`  | Show more of dependencies (expand tree) |
|      `h`  | Show less of dependencies (contract tree, or close) |
|     `gg`  | Move to the first item in the active window |
|     `gj`  | Move to the next **lower** major _version_ (or down) |
|     `gk`  | Move to the next **higher** major _version_ (or up) |
|     `gc`  | Move to the currently installed _version_ (or no-op) |
|      `G`  | Move to the last item in the active window |
|     `zt`  | Move the selected row in the active window to the top |
|     `zz`  | Move the selected row in the active window to the middle |
|     `zb`  | Move the selected row in the active window to the bottom |
|      `/`  | Start regex filtering in the _package_ window |
|      `?`  | Start regex filtering in the _package_ window |
| `ctrl-e`  | Scroll the active window up one row |
| `ctrl-y`  | Scroll the active window down one row |
| `ctrl-d`  | Scroll the active window up half a screen |
| `ctrl-u`  | Scroll the active window down half a screen |


## Searching

To start regex filtering, hit `/` and start typing.

Pressing `Enter` exits search mode, keeping the filtering.

Press `Esc` or `ctrl-c` (while in search mode) to clear pattern and exit search mode, showing everything.


### Initialize with search

A search pattern can be give on the command line, prefixed with a `/`, to start with a filtered list. For example:

```
$ npm-review /e.*p
```


## Alternate window

The active window is the package window, unless one of the alternate views is open. The alternate view can be scrolled with `j` and `k`, respectively.

There are threw alternate views:

### Versions (`Enter`)

Lists versions, highlighting the currently installed.

Installation of another version can be done by selecting it (`j`/`k` and hitting `Enter`.)

### Info (`i`)

Shows basic information about the package.

### Dependencies (`I`/`l`)

Shows a tree view of a package's dependencies. By default only the direct dependencies are shown. To show the full tree, hit `l`. Show less with `h`.

### Version check (`V`)

Checks the installed versions of all packages against the _npm registry_.  
Displays versions behind on the current major, and the newest major, if any.

If `ENTER` is pressed on a row, the package's version list is displayed.

Caveat: the version check list must be re-initialized of the network, when exiting the version view.

Example output:
```
bootstrap    4.6.0   (DEV)                2 new – new major: 5
eslint       5.16.0  (DEV)                    ✓ – new major: 8
jquery       3.6.0   (DEV)                6 new
popper.js    1.16.1  (DEV)                    ✓ – new major: 2
redom        3.29.0  (DEV)                1 new
webpack-cli  5.1.1   (DEV)                3 new
```

## Prerequisites

The data being fetched is processed with external programs, that needs to be installed in order for the program to work properly:

  - [jq](https://github.com/stedolan/jq)
  - [gawk](https://www.gnu.org/software/gawk/)
  - [ncurses](https://en.wikipedia.org/wiki/Ncurses) (for building)

## Installation

Only developed/tested on macOS.

### Install dependencies

These may be installed with [homebrew](https://github.com/Homebrew/brew):
```
brew install ncurses jq gawk
```

### Install `npm-review`

Git clone this repo and run:

```
make install
```

to install the binary under `/usr/local/bin`.


Now navigate to a directory containing a `package.json` and run

```
npm-review
```

## Caveats

- Error handling is **minimal**

## Development

There are two flags that can be helpful when developing:

  - `-d` – Activates debug-mode that writes log messages to `./log`
  - `-f` – Fakes the HTTP request data, making offline development easier
