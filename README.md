# MPA
Multi-Precision-Arithmetic

# Usage
1) Add the single header file "mpa_integer.h" to your include path.
2) Compile your project as usual.

# Tests
To run the tests, you can compile the file "mpa_integer_test.cpp" and run it.

Example command line executed from the repo root (just omit the sanitizers if they are not supported on your system):
  ```
     $ export CXX=clang++
     $ export CXX_FLAGS="-O3 -Wall -Werror -Wextra -pedantic"
     $ $CXX test/mpa_integer_test.cpp $CXX_FLAGS -I. -fsanitize=undefined -fsanitize=address -o mpa_integer_test
     $ ./mpa_integer_test 
     running tests ...
     wordtype uint16_t OK
     wordtype uint32_t OK
     wordtype uint64_t OK
     all tests passed
  ```

# Examples
The examples section contains a basic RSA key generator and parser.

You can compile it from the repo root using for instance the following command:
  ```
     $ $CXX examples/rsa.cpp $CXX_FLAGS -I. -o rsa
  ```
In order to generate a key with for example bitlength 4096 (and optionally verify it with openssl), you can run the following command:
  ```
     $ time ./rsa generate 4096 && openssl rsa -in example.rsa -check -noout
     generating rsa key
     bitlength: 4096
     iterations : 245
     iterations : 390

     writing private key to: example.rsa
     wrote 3239 bytes in total

     writing public key to: example.rsa.pub
     wrote 742 bytes in total


     real	0m0.607s
     user	0m1.106s
     sys	0m0.002s
     RSA key ok
  ```
If you provide a filepath as third argument, the key will be written to the provided filepath; if no filepath is provided, the key will be written to the file "example.rsa" in the current working directory.

In order to parse a key, you can call the tool as follows:
  ```
     $ ./rsa parse example.rsa.pub 
     <<<RSA PUBLIC KEY DETAIL START>>>

     encryption exponent:
     0x10001
     modulus:
     0xb864665a287ac1e743da12070907b84ba84940f359898bfbe04098a883401b8195328d31aeb27c9edb1a94f15cfa8967451b2f1ca102eeb61f21bab3c58d310121c58e4e1dbec3ac2281b8028eeb8dfe3ff21d9e026e4bd66a164d3394543d5c6af6e8a94e6af946bbdb5c60a5429e5aa8373900cb388452a178052fa3a76762bf33dc05c054f90076457e48864bcbcce166512014c54636040500f0b6fe9703da3ab0dd06b6728bc1ecb758a2f9f36d4a45f3017558bd4968bf089ab2fad4b5973c611c44fd1c754e7c44ac815c2919bcc22c3592b16cd01dc6b9ebe0eb475ddb749724ba2e216bb9c971891bda08d1800c447574160a61f80c7f8eb5235485e14e6a5ea1679a10a126934533757703e9623fa87b8804efda05c9f26eae3f4810f574bc7b21bf53cae84d20ed2a01b7abe74fe05d2d05336cd85c1aefe0aca4b27c1663ef9c4725a7549d56fac397941a7eed64e01b1161633876b17caa01584781066131baff5afbd0ade7808e3ee5d1c192c6e91e91e0e660075c707b058c7d1db27ee88a69929f46f725b06f5086d3c011704c1109843ecddda515a3a2f2bc04bf8a337642722de41ed8d5e2b5ab1e3332ff5bc8d228809688fe4e177323c8b201edafca58d620df4c0f1a82410b5fbfcf29bf92f4362de0d867afd72a464b631a530e0e0f13e4c6ebc1c0d3f57ef67459dc08bbb3ccdf953f94ef8e39f5
     <<<RSA PUBLIC KEY DETAIL END>>>
  ```

