# BLOA LANGUAGE

BLOA is a practical general-purpose scripting language that is especially
suited to web development and general automation. Fast, flexible, and
pragmatic, BLOA empowers both small scripts and large applications.

BLOA is distributed under the [Modified BSD License](LICENSE)
(SPDX-License-Identifier: `BSD-3-Clause`).

## Documentation

BLOA documentation is maintained in the `docs/` directory.

BLOA source files support both `<?bloa` and `<?php` opening tags. The shorthand
`<?bloa=` can also be used as an alias for `<?=`.

## Installation

### Prebuilt packages and binaries

Prebuilt packages and binaries can be used to get up and running fast with
BLOA.

For Windows, BLOA binaries can be obtained from the project distribution
channels. After extracting the archive the `*.exe` files are ready to use.

For other systems, see the installation documentation in `docs/`.

### Building BLOA source code

*For Windows, see the BLOA build documentation in `docs/`.*

BLOA is built to support thread-safe execution by default. For high-performance
BLOA builds, a ZTS-capable compiler toolchain is recommended. The runtime GC is
also tuned for higher thresholds to reduce cycle collection overhead while
keeping throughput high.

BLOA now includes a built-in `parallel_map()` helper for concurrent execution
of callback-based array transformations on ZTS builds. Its worker scheduler uses
lock-free atomic indexing to reduce thread contention and improve throughput.

For a minimal BLOA build from Git, you will need autoconf, bison, and re2c. For
a default build, you will additionally need libxml2 and libsqlite3.

On Ubuntu, you can install these using:

```shell
sudo apt install -y pkg-config build-essential autoconf bison re2c libxml2-dev libsqlite3-dev ccache
```

If available, use `ccache` for faster repeated rebuilds.

On Fedora, you can install these using:

```shell
sudo dnf install re2c bison autoconf make ccache libxml2-devel sqlite-devel
```

On MacOS, you can install these using `brew`:

```shell
brew install autoconf bison re2c libiconv libxml2 sqlite
```

or with `MacPorts`:

```shell
sudo port install autoconf bison re2c libiconv libxml2 sqlite3
```

Generate configure:

```shell
./buildconf
```

Configure your build. `--enable-debug` is recommended for development, see
`./configure --help` for a full list of options.

```shell
# For development
./configure --enable-debug
# For production
./configure
```

Build BLOA. To speed up the build, specify the maximum number of jobs using the
`-j` argument:

By default, BLOA is configured to build with thread safety enabled. Use
`./configure --enable-debug --enable-zts` for a debug build, or simply
`./configure` if your environment supports ZTS by default.

```shell
make -j4
```

The number of jobs should usually match the number of available cores, which
can be determined using `nproc`.

## Testing BLOA source code

BLOA ships with an extensive test suite, the command `make test` is used after
successful compilation of the sources to run this test suite.

It is possible to run tests using multiple cores by setting `-jN` in
`TEST_PHP_ARGS` or `TESTS`:

```shell
make TEST_PHP_ARGS=-j4 test
```

Shall run `make test` with a maximum of 4 concurrent jobs: Generally the maximum
number of jobs should not exceed the number of cores available.

Use the `TEST_PHP_ARGS` or `TESTS` variable to test only specific directories:

```shell
make TESTS=tests/lang/ test
```

Project testing and quality assurance guidance is available in the repository
and in the docs.

## Installing BLOA built from source

After a successful build (and test), BLOA may be installed with:

```shell
make install
```

Depending on your permissions and prefix, `make install` may need superuser
permissions.

## BLOA extensions

Extensions provide additional functionality on top of BLOA. BLOA consists of many
essential bundled extensions. Additional extensions can be found in the extension
community repositories.

## Contributing

The BLOA source code is located in the Git repository at
[github.com/bloa-lang/bloa-src](https://github.com/bloa-lang/bloa-src). Contributions
are most welcome by forking the repository and sending a pull request.

Discussions are done on GitHub for most topics.

New features require an RFC and must be accepted by the developers. See
[Request for comments - RFC](https://github.com/bloa-lang/bloa-src/issues) for
more information on the process.

Bug fixes don't require an RFC. If the bug has a GitHub issue, reference it in
the commit message using `GH-NNNNNN`.

    Fix GH-7815: php_uname doesn't recognise latest Windows versions
    Fix #55371: get_magic_quotes_gpc() throws deprecation warning

See [Git workflow](https://github.com/bloa-lang/bloa-src/pulls) for details on
how pull requests are merged.

### Guidelines for contributors

See further documents in the repository for more information on how to
contribute:

- [Contributing to BLOA](/CONTRIBUTING.md)
- [BLOA coding standards](/CODING_STANDARDS.md)
- [Internal documentation](https://github.com/bloa-lang/bloa-src/)
- [Mailing list rules](/docs/mailinglist-rules.md)
- [BLOA release process](/docs/release-process.md)

## Credits

For the list of people who've put work into BLOA, please see the project
credits information.
