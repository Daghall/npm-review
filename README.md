# NPM review

[ncurses](https://en.wikipedia.org/wiki/Ncurses)-based [TUI](https://en.wikipedia.org/wiki/Text-based_user_interface) for reviewing and upgrading/downgrading NPM packages, viewing package info, and dependencies, with vim-like key-bindings.

Heavily inspired by [tig](https://github.com/jonas/tig).

![Example](/example.gif)

## Key-bindings

The active window is the _alternate_ window, if it is displayed, _package_ window otherwise.

| Key   | Command |
| ----- | ------- |
|      `j`  | Move cursor down in the active window |
|      `k`  | Move cursor up in the active window |
| `J` or ⬇️  | Move cursor down in the _package_ window |
| `K` or ⬆️  | Move cursor up in the _package_ window |
|      `q`  | Close the active window (exit if in _package_ window) |
|      `Q`  | Exit |
|  `Enter`  | Show versions, or `npm install` selected version |
|      `D`  | `npm uninstall` selected package |
|      `i`  | `npm info` of the selected package |
|      `I`  | Show dependencies for selected package |
|      `l`  | Show more of dependencies (expand tree) |
|      `h`  | Show less of dependencies (contract tree, or close) |
|      `g`  | Move to the first item in the active window |
|      `G`  | Move to the last item in the active window |
|      `/`  | Start regex filtering in the _package_ window |
| `ctrl-e`  | Scroll the _alternative_ window up |
| `ctrl-y`  | Scroll the _alternative_ window down |


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

## Prerequisites

The data being fetched is processed with external programs, that needs to be installed in order for the program to work propely:

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

- Error handling is minimal
- Resizing terminal breaks rendering

## Development

There are two flags that can be helpful when developing:

  - `-d` – Activates debug-mode that writes log messages to `./log`
  - `-f` – Fakes the HTTP request data, making offline development easier
