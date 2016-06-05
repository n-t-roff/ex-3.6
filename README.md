# ex-3.6 (from 4.0BSD release)
This is `vi` version 3.6 taken from 4.0BSD.
It had been released in 1980.
## Installation notes
Some configuration (e.g. installation paths) can be done in the
[`makefile`](https://github.com/n-t-roff/ex-3.6/blob/master/Makefile.in).
For compiling it on BSD and Linux autoconfiguration is required:
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
* When scrolling down less than a full screen height
  or using `^E`
  some lines may be displayed wrong aligned.
  `^L` fixes this.
  Scrolling down with `^F` does't have this problem.

**Attention**:
The original `vi` had not been 8-bit clean!
Moreover it does automatically change all 8-bit characters to 7-bit in the whole file even if no editing is done!
This will e.g. destroy all UTF-8 characters.
