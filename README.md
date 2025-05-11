# `pcap` files parser


### Directory structure

* Main project directory contains `main.cpp` and project files.
* Directory `cmake` contains cmake files:
  * `CompilationOptions.cmake` to set compilation options (only Linux)
  * `Findlibpcap.cmake` to find `libpcap` library in the operating system
* Directory `lib` contains actual implementation of pcap parser
  * Most files have corresponding `.cpp`
  * Most headers are short
    * except `functional.hpp` - see note on functional programming below
* Directory `tests` contains unit tests (with Catch2)

### Dependencies

This project relies on two external dependencies
* libpcap
* Catch2

Both are installed in the provided `Dockerfile`.

This project requires C++23, and I tested it with:
* gcc-13
* gcc-14
* gcc-15
* clang-19
* clang-20

At least one of these compilers had been in use long enough to be a viable option for most
deployments, I hope.

### Design

Almost everything is implemented in `lib`. The `int main()` function only pulls 
dependencies from `lib`, and executes these in a functional sequence, guarded by
`try ... catch` (the only such block in this project).

The parts of the project are clearly visible as stages of processing inside `int main()`.

I used dynamic polymorphism to implement `Inputs` and `PcapInputs` (with `MockInputs`
in unit tests). This aside I used simple aggregate types as much as possible. In
functional programming style, most types are not "hidden state machines" as often
things turn to be in OOP programs, hence they benefit little from strong
encapsulation/information hiding.

One common pattern used in this project is `make` member of a class (often a niebloid,
although a static function would sometimes do). The role of a `make` is to enforce all the
invariants of the type being created - which removes the problem of exceptions throw
from a constructor. A `make` function simply returns a monad, which can be in
error state if an object cannot be created. Sometimes we expect that an object can be
always created e.g. `stats` type does not have any invariant, hence its `make` 
does not return a monad - just a `stats` object.

### What is a "functional sequence" ?

Imagine you have this block of code:

```
int main(int argc, const char** argv)
{
  try
  {
    if (argc != 2) {
      throw exception("received ", argc - 1, " parameters but expected 1");
    }

    auto const files = find_inputs(argv[1]);
    if (!files) {
      throw exception(files.error().what());
    }

    auto const sorted_files = sort_channels(*files);
    if (!sorted_files) {
      throw exception(sorted_files.error());
    }

    auto pcap_inputs = PcapInputs::make(*sorted_files);
    if (!pcap_inputs) {
      throw exception(pcap_inputs.error());
    }

    auto const result = stats::make(*pcap_inputs);
    if (!result) {
      throw exception(result.error());
    }

    std::cout << *result << std::endl;
  }
  catch(exception const& e)
  { 
    std::cerr << e.what() << std::endl;
    return 13;
  }

  return 0;
}

```

There is lots of repetition for handling of errors here. Handling errors is a critical
part of a program, but in this style it is easily missed. In the excerpt above,
all the return types are `std::expected` which makes it more obvious where error
handling is needed (i.e. one needs to dereference the return value, and most
programmers think before putting an `*` before a variable "could this crash ?") but
the repetition is obvious. Obviously a programmer could also throw exception from every
single function called above, rather than just below the function call. This would
shorten this section, but exceptions do not compose well. If used for program flow control
this way, they would introduce many additional (and invisible) execution paths.
Exceptions are best used only for handling of exceptional conditions e.g. out of memory.
Even when used for the flow control in top level `int main()`, it does not look good.

Another option is a nest (literally) of `if` statements, which is also not very readable.

One way to mitigate is to rely on monadic operations in `std::expected` [added
to C++23](https://en.cppreference.com/w/cpp/utility/expected) and indeed, a functional
sequence is built on top of these.

A functional sequence is a single expression, built from two types of blocks:
* _monads_ (in this program only `std::expected`) and
* _functors_ (a tiny wrapper for a function to be passed to a monadic operation)
* optionally terminated with a `discard` _functor_ which yields `void`
  * used to shut down warning about "ignored return value"

These blocks are connected by `operator|`, overloaded on a monad on the left side, and a
functor on the right. Each evaluation of `operator|` yields a monad, which can be passed
to the next functor. Hence, a monad which in the code excerpt above is a stack
variable, in a functional sequence becomes an rvalue passed from functor to
functor. Rvalues work well with move-only data and provide compiler with useful
guarantees e.g. no aliasing, enabling more aggressive optimizations.

Implementation of `operator|` wraps the actual monadic operation - ideally it should be a
single expression, but in this project for reasons of Clang compatibility (see note
on functional programming below) I had to implement polyfills in `lib/functional.hpp`.

The key property of monadic operations (both as standardized in C++23, and also as
functors in this project) is that they only invoke a user function if certain properties
of a monad are met:
* `and_then` and `transform` only invoke user function if monad is set to a value.
  * `and_then` takes a function which takes a value as a parameter and returns a monad (i.e. `std::expected` in this project)
  * `transform` takes a function which returns any value - which is then stored in a monad (and returned) by the operation itself.
* `or_else` only invoke user function if monad is set to an error.
  * `or_else` takes a function which takes an error as a parameter and returns a monad (i.e. `std::expected` in this project)

If a the condition is not met, then monadic operation will short-circuit, i.e.
will return the monad unchanged, without the user function being invoked. Either way,
a monad return value from the `operator|` feeds into the next functor in the sequence,
which can either process the value type (passing error unchanged), or the error
(passing value unchanged); or it can `discard` the monad altogether.

This means that a functional sequence only needs one location to handle all errors,
e.g.:

```
    return (
      find_inputs(argv[1])
        | and_then(sort_channels)
        | and_then(PcapInputs::make)
        | transform(stats::make)
        | transform([](stats const &result) -> int {
            std::cout << result << std::endl;
            return 0;
          })
        | or_else([](error const &err) -> std::expected<int, error> {
            std::cerr << err << std::endl;
            return err.code();
          })
    ).value();
```

This single expression does everything. Since the last functor is `or_else`, we
know that any error has been replaced with the value of its code, which is
next used as a return value from the expression. In the happy path, this value
is an explicit `return 0` in the last `transform`.

It is both more readable and robust than e.g. a nest of `if` statements or a
collection of stack variables with various stages of data processing
(as seen in the top excerpt).

### Note on functional programming

Functional programming is not very common in C++ programs, the primary reason being
that C++ compilers have been in the past bad at avoiding unnecessary copies of data.

In functional programming style, making immutable variables is the primary way of progressing
a computation - the result of each step of computation is stored in a monad, which
is then used as an input for the next step. Luckily, since the past 10 or so years,
compiler vendors have learned to avoid making copies and even completely avoid creating
variables in runtime (hello, constant evaluation). Also, functional programming style
does compose with other programming styles (as demonstrated `lib/stats.cpp`) so there
is no need to use it all the time.

The facilities to enable this are all in a single file `lib/functional.hpp` , the
size of which could have been significantly smaller if `libc++` did have
`std::move_only_function` and/or if `clang` worked flawlessly with the monadic
functions implemented in `libstdc++`. I had to implement polyfills, rather than
rely on a single expression for each functor. Not that I like `clang` so much
(I like all conforming compilers), however I do use `clangd` and `clang-tidy` and
I really want them to understand what I'm doing. Hence, support for this compiler
is important for me.

In this project I define and use functors `and_then`, `transform` and `or_else`.
They are documented inside `lib/functional.hpp` and the program contains three examples
of their use:
* in `int main()`, top level processing from command line argument to `std::cout << result`
* in `packet::parse_t::operator()`, parsing of a data packet to extract its `properties` i.e. `sequence` and a `timestamp`
* small fragment in `stats::make_t::operator()(Inputs &&inputs, error_callback_t log)`,
passing data packet to parsing and then updating the `last` state with extracted `properties`


### Niebloids everywhere !

Let's not exaggerate, there are only few of them. For good reason. In imperative
programming style, we typically only name a function when we want to invoke it, which
may (or may not) trigger on overload resolution and may (or may not) supply default
function arguments. If we want to pass a function to a functor, we can capture
its address, e.g.

```
void foo(int);
enum error;

expected<int, error>(42) | transform(&foo);
```

In principle this works, but with limitations:

* No way to capture a default parameter: if `foo` had a second (defaulted) parameter, it 
does not belong to the type of `&foo` hence the `transform` functor would have no way
of knowing to use that default value of the second parameter.
* No way to capture an overload set. [We are working to fix it](https://wg21.link/p2825), but only for C++26 
* Pretty difficult to do this for a template function: it needs to be fully instantiated,
meaning all template parameters have to be provided. This can be unwieldy.

There is also an additional, often hypothetical but sometimes real, problem:

* Why risk using overload set, when we know exactly what function we want ?

A niebloid, being an object rather than a function, avoids all of these. At a cost
of additional typing/reading - but it's easy to get used to the pattern.

Example:
```
constexpr inline struct parse_t final {
  [[nodiscard]] auto operator()(data_t const &) const -> std::expected<properties, error>;
} parse;
```

Here I am creating an inline object named `parse`, of type `parse_t`, which implements
`operator()`. If I pass the `parse` object around, I am no longer using a function
pointer but an object. Since the object is `inline constexpr`, the compiler will know
to:
* Use only single definition of the object across all the translation units it appears in
* Instantiate it during the constant evaluation context (i.e. during compilation rather than runtime)

This means the object is, in many ways, cheaper to pass around than a function pointer. Simply
speaking, the compiler knows not only its type but also its value - during the compilation.
