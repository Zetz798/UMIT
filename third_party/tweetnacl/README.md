# TweetNaCl 20140427

Unmodified C source and header, downloaded from the authors:

- https://tweetnacl.cr.yp.to/20140427/tweetnacl.c
- https://tweetnacl.cr.yp.to/20140427/tweetnacl.h

Public domain: https://tweetnacl.cr.yp.to/
Authors: Daniel J. Bernstein, Bernard van Gastel, Wesley Janssen,
Tanja Lange, Peter Schwabe, Sjaak Smetsers.

SHA-256:

```text
tweetnacl.c 02e65bc3013ff2168983365e55906bc783c4c7e0a60d8100f17bb303a17175c4
tweetnacl.h 43f29ad721d9927b747b0100ab4160c119e7bb180c7c98a66e4bf79d31244287
```

The application provides `randombytes` with `QRandomGenerator::system()`.
Used for Phantom X25519/XSalsa20-Poly1305 connection responses, not for storing
or generating the user's wallet signing key. Native buffers use NaCl padding;
the Phantom wire format omits the 16-byte leading zero block.
