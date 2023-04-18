# NPM review

[ncurses](https://en.wikipedia.org/wiki/Ncurses)-based [TUI](https://en.wikipedia.org/wiki/Text-based_user_interface) for reviewing and upgrading/downgrading NPM packages, with vim-like key-bindings.

Heavily inspired by [tig](https://github.com/jonas/tig).

## Key-bindings

The active window is the _version_ window, if it is displayed, _package_ window otherwise.

| Key   | Command |
| ----- | ------- |
|     j | Move cursor down in the active window |
|     k | Move cursor up in the active window |
|     J | Move cursor down in the _package_ window |
|     K | Move cursor up in the _package_ window |
|     q | Close the active window (exit if in _package_ window) |
|     Q | Exit |
| Enter | Open _version_ window, or `npm install` selected version |
|     g | Move to the first item in the active window |
|     G | Move to the last item in the active window |
|     / | Start regex filtering in the _package_ window |

## Prerequisites

  - [jq](https://github.com/stedolan/jq)
  - [ncurses](https://en.wikipedia.org/wiki/Ncurses)

## Installation

Only developed/tested on macOS.

### Install dependencies
These may be installed with [homebrew](https://github.com/Homebrew/brew):
```
brew install ncurses jq
```

### Install `npm-review`

Git clone this repo and run

```
make install
```

to install the binary under `/usr/local/bin`.


Now nacigate to a directory containing a `package.json` and run

```
npm-review
```

## Caveats

- Error handling is minimal
- Resizing terminal breaks rendering

## Development

There are to flags that can be helpful when developing:

  - `-d` – Activates debug-mode that writes log messages to `./log`
  - `-f` – Fakes the version window data, making offline development easier
