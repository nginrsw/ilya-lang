# Ilya

Ilya Ilyukhin Programming Language is a personal fork of Lua, modified
slightly to make it easier for myself to learn, write, and personalize.

For now, only two changes have been made:

- The reserved word `local` has been replaced with `lock`.
- The reserved word `function` has been replaced with `fn`.

Everything else remains the same as in the original Ilya language.

---

## Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/nginrsw/ilya-lang.git
   cd ilya
   ```

2. Run `make` in your terminal to build the program:
   ```bash
   make
   ```

3. After the build process is complete, install the program by creating a
   symbolic link:
   ```bash
   sudo ln -s /path/to/program/ilya-lang/ilya /usr/local/bin/ilya
   ```

   Replace `/path/to/program` with the actual path to the compiled program.

Now, you can run the `ilya` command from anywhere in your terminal!

Try making a file named `file.ilya`, type `io.write("Hello Ilya\n")` inside it, and run it with `ilya file.ilya` in your terminal.
