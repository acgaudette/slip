writing [graphics] math in C is terrible. don't argue--you know it's true!

what if someone did a dumb thing and made a gloriously terse DSL
optimized for the use case of rapidly writing shader-like math
that you could embed in C code like inline assembly?

slip takes a C file with inline slip as input,
and produces C without inline slip to stdout.

example:
```
	$ dot up + [1 2 3] [4 5 6]
```

which slip turns into:
```
	v3_dot(V3_UP, v3_add((v3) { 1, 2, 3 }, (v3) { 4, 5, 6 }));
```

I have been working on slip for approximately 3 hours.
it compiles [itself](https://github.com/acgaudette/slip/blob/37ba673bceebae72605db6c0e17d34e297429e2c/main.c#L16) !
maybe I will never look at this again. who knows!

dependencies:
- [acg](https://github.com/acgaudette/acg) (for panic)

currently this outputs math in the style of [alg](https://github.com/acgaudette/alg).
in theory it can support any math library as a backend; see [./key.binds](key.binds).
