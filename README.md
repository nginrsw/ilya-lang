## Ilya Programming Language

<img src="img/ilyastars.png" alt="Ilya" width="150"/>

Ilya is a personal fork of Lua, This modification has been made to simplify
learning, writing, and personalizing the language for my own use.

For now, two main changes have been implemented:

- The reserved word `local` has been replaced with `lock`.
- The reserved word `function` has been replaced with `fn`.

Other than these changes, everything remains identical to the original Lua
language.

---

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/nginrsw/ilya-lang.git
   cd ilya
   ```

2. Build the program by running `make` in your terminal:
   ```bash
   make
   ```

   Or, if you want to test whether everything is working correctly, you can run
   the `all` file by executing:
   ```bash
   ./all
   ```

3. After building, create a symbolic link to install the program:
   ```bash
   sudo ln -s /path/to/program/ilya-lang/ilya /usr/local/bin/ilya
   ```

   Replace `/path/to/program` with the actual path to where the compiled program
   resides.

Once this is done, you can run the `ilya` command from anywhere in your
terminal!

For example, create a file named `file.ilya`, type `io.write("Hello Ilya\n")`
inside it, and run it with the following command:

```bash
ilya file.ilya
```

---

## Transpiling Between Lua and Ilya

If you wish to transpile code from Lua to Ilya or from Ilya to Lua, I’ve
prepared executable files located in the `transpiler` folder.

You can easily run these using the following commands:

- **From Lua to Ilya**:
  ```bash
  lua2ilya <filename>.lua
  ```

- **From Ilya to Lua**:
  ```bash
  ilya2lua <filename>.ilya
  ```

These commands will automatically convert your code between the two languages,
making it easier to switch between them.
