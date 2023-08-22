# WatchTeX

WatchTeX is a simple program that watches every `.tex` files in a specified directory, and compiles them when they are modified.

## Usage

```bash
watchtex [directory]
```

If no directory is specified, the current directory is used.

To stop the program, press `Ctrl+C`.

To work, the program needs [rubber](https://gitlab.com/latex-rubber/rubber) installed and available in the `PATH` environment variable.

## Compilation

To compile the program, you need to have:

- a C++20 compiler installed (default: `g++`)
- `make` installed
- `librt` installed

If you want to use another compiler, you can specify it with the `CC` environment variable.

To compile the program for release, run:

```bash
make release
```

Otherwise, to compile the program for debug, run:

```bash
make debug
```

## Limitations

At the moment, the program only compiles on Linux x86-64 machines.

## License

This program is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
