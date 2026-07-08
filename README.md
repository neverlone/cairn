# cairn

A tiny stack-based esoteric programming language, with an interpreter in one
file of portable C.

A **cairn** is a pile of stones left to mark a trail. Programs in this language
are built the same way — you stack values up and knock them back down. There
are no variables and no named functions: just a data **stack**, a small
addressable **field** of memory, and 21 single-character words.

```
$ echo '"Hello, World!\n"[@]' | ./cairn
Hello, World!
```

## Build & run

```sh
make            # builds ./cairn  (or: cc -O2 -o cairn cairn.c)
make test       # build and run the test suite
make demo       # run every example in examples/

./cairn path/to/program.cairn    # run a file
echo '2 3 + .' | ./cairn         # or pipe a program on stdin  ->  5
```

No dependencies beyond a C compiler and the standard library.

## The machine

- A **data stack** of signed 64-bit integers.
- A **field**: 256 integer cells, addressed `0`–`255`, all starting at `0`.
  This is your long-term storage, since there are no variables.
- Whitespace and any unrecognized character are **ignored**, so you can space
  and indent programs however you like.
- `( ... )` is a **comment** and may nest.

## Language reference

A number literal is a run of digits and pushes that value. Everything else is a
single character. For binary operators the second operand is on top: `a b -`
computes `a - b`.

| Word | Name | Effect |
|------|------|--------|
| `0-9` | push | push a number literal (`42` pushes 42) |
| `+ - * / %` | arithmetic | `a b +` → `a+b`; likewise `-`, `*`, `/`, `%` |
| `.` | print-num | pop and print as a decimal number |
| `@` | print-char | pop and print as an ASCII character |
| `:` | dup | duplicate the top value |
| `$` | drop | discard the top value |
| `\` | swap | swap the top two values |
| `o` | over | copy the second value to the top |
| `r` | rot | rotate the top three: `a b c` → `b c a` |
| `=` | eq | `a b =` → `1` if equal else `0` |
| `<` | lt | `a b <` → `1` if `a < b` else `0` |
| `>` | gt | `a b >` → `1` if `a > b` else `0` |
| `!` | not | `0` → `1`, anything else → `0` |
| `[` | while | if the top is `0`, jump past the matching `]` |
| `]` | loop | jump back to the matching `[` |
| `g` | get | `addr g` → push `field[addr]` |
| `p` | put | `val addr p` → store `val` into `field[addr]` |

### Loops

`[ ... ]` is a `while` loop. On `[`, the top of the stack is **peeked** (not
popped): if it is `0`, control skips past the matching `]`; otherwise the body
runs, and `]` jumps back to re-test. So the idiom is to keep the loop condition
on top of the stack and update it inside the body:

```
10 [ : . 10 @ 1 - ]     ( print 10, 9, ... 1, each on its own line )
```

### Strings

`"..."` is sugar. It pushes a `0` and then the characters **in reverse**, so a
`[@]` loop pops and prints them left-to-right and halts on the `0`:

```
"Hi\n"[@]        ( same as: 0 10 105 72 [@]  ->  Hi )
```

Supported escapes: `\n`, `\t`, `\\`, `\"`, `\0`.

## Examples

All of these live in [`examples/`](examples/) and are checked by `make test`.

**Fibonacci** ([`fib.cairn`](examples/fib.cairn)) — the field holds `a` and `b`
while the stack holds only the loop counter:

```
0 0 p  1 1 p  10 [ 0 g . 10 @  0 g 1 g +  1 g 0 p  1 p  1 - ]
```
```
0
1
1
2
3
5
8
13
21
34
```

**Triangle** ([`triangle.cairn`](examples/triangle.cairn)) — a nested loop:

```
*
**
***
****
*****
```

## How it works

The interpreter is three small passes over the program (see `cairn.c`):

1. **Tokenize.** The source text becomes a flat array of instructions. Number
   and string literals are expanded to `push` ops here, comments and unknown
   characters are dropped.
2. **Match brackets.** A single stack-based pass pairs every `[` with its `]`
   and records the jump targets, so loops are O(1) at run time (and unbalanced
   brackets are caught before anything executes).
3. **Execute.** A straightforward switch-per-opcode loop over the instruction
   array, with bounds checks on the stack and the field.

Errors (stack underflow, division by zero, unmatched brackets, out-of-range
field access) print a message to stderr and exit non-zero.

## Is it Turing-complete?

Effectively yes: with unbounded loops, arithmetic, and 256 addressable memory
cells you can build counters and conditionals and simulate a bounded-tape
machine. It's a toy — but a real one.

## License

MIT © neverlone. See [LICENSE](LICENSE).
