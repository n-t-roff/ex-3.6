# ex-3.6 (from 4.0BSD release)
This is `vi` version 3.6 taken from 4.0BSD.
It had been released in November 1980.
## Installation notes
The software is downloaded with
```sh
git clone https://github.com/n-t-roff/ex-3.6.git
```
and can be kept up-to-date with
```sh
git pull
```
Some configuration (e.g. installation paths) can be done in the
[`makefile`](https://github.com/n-t-roff/ex-3.6/blob/master/Makefile.in).
For compiling on BSD, Linux and Solaris autoconfiguration is required:
```sh
$ ./configure
```
The software is build with
```sh
$ make
```
and installed with
```
$ su
# make install
# exit
```
All generated files are removed with
```sh
$ make distclean
```
## Usage notes
* PAGE-UP, PAGE-DOWN keys may work on most terminals by putting
  `map  ^[[5~ ^B` and `map  ^[[6~ ^F` into `~/.exrc`.
* The documents
  [viin.pdf](http://n-t-roff.github.io/ex/3.6/viin.pdf)
  and
  [exrm.pdf](http://n-t-roff.github.io/ex/3.6/exrm.pdf)
  describe vi version 3.6 in detail.
