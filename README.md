=Introduction=
An attempt at reproducing the numbers in the CHD paper [2] using Jenkin's hash [1].

=Pre-reqs=
* gnu-make
* g++ >= v4.8

=Building and Running=
```bash
make chd
```
Builds a 'chd' executable. Use '-h' option to this exe to know about its usage.

=Info=
This project uses Jenkin's hash [1] along with the CHD algo [2] in order to construct
perfect hashes for a given set of vocabulary. Currently, the vocabulary set is
limited to plain-old ascii strings. The hash function is parameterized by the
initial values used before the hashing loop starts, with the value of 0 being
used by the hash function used to create buckets. Currently, the code just prints
out this initial value for each bucket. Later, I'll write a nice output format
to it!

=References=
[1]. Jenkin's hash by Bob Jenkins: http://burtleburtle.net/bob/hash/doobs.html
[2]. CHD paper: http://cmph.sourceforge.net/papers/esa09.pdf
