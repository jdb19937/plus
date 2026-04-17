# plus

**A file pager built from scratch in a single C source file. No ncurses. No termcap. No external dependencies of any kind. Just raw terminal escape sequences, the way it should have been done all along.**

## Why plus Exists

`more` is a relic — it scrolls forward and that's about it. `less` is powerful, but it's also 30,000 lines of code with a build system that pulls in autoconf, and it still can't decide whether it's a pager or a text editor. Every other pager either depends on ncurses (which depends on a terminfo database, which depends on your system being configured correctly, which it isn't) or reimplements half of it anyway.

plus does what a pager needs to do: display text, scroll in both directions, search, and get out of the way. It does this in a single file, with direct ANSI escape sequences, in about 500 lines. It enters the alternate screen buffer so your scrollback stays clean. It handles terminal resizes. It highlights search matches. It reads from pipes. And when the file fits on one screen, it just prints it and exits, because that's obviously the right thing to do.

## Getting Started

```bash
face
```

That's it. One source file in, one binary out.

## Usage

```bash
plus file.c                   # page a file
cat enormous.log | plus       # read from a pipe
plus --auxilium               # show keybindings
```

When stdout isn't a terminal, plus passes text straight through — pipe it wherever you like.

## Keybindings

The controls are exactly what you'd expect if you've ever used `less` or `vim`:

| Key | Action |
|---|---|
| `j` / `↓` / Enter | One line down |
| `k` / `↑` | One line up |
| Space / `f` / PgDn | One page down |
| `b` / PgUp | One page up |
| `d` | Half page down |
| `u` | Half page up |
| `g` / Home | Jump to beginning |
| `G` / End | Jump to end |
| `/` | Search (case-insensitive) |
| `n` | Next match |
| `N` | Previous match |
| `q` | Quit |

## Search

Type `/` followed by your query. Matches are highlighted in place as you scroll through the file. `n` and `N` cycle forward and backward through results, wrapping around at the boundaries. Search is always case-insensitive, because case-sensitive search in a pager is a mistake that `less` made thirty years ago and never corrected.

## Terminal Handling

plus manages the terminal properly. It enters raw mode for keystroke-by-keystroke input, switches to the alternate screen buffer so your shell history remains untouched when you quit, hides the cursor during rendering to eliminate flicker, and restores everything — terminal state, screen buffer, cursor visibility — on exit, on Ctrl-C, and on SIGTERM. If your terminal supports it, plus handles it correctly. If the terminal resizes while you're reading, plus adapts immediately.

When reading from a pipe, plus opens `/dev/tty` directly for keyboard input, so `cat file | plus` works exactly the way you'd expect.

## License

Free. Public domain. Use however you like.
