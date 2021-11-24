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

~~I have been working on slip for approximately 3 hours.~~
it compiles [itself](https://github.com/acgaudette/slip/blob/master/main.c#L16) !
maybe I will never look at this again. who knows!

currently this outputs math in the style of [alg](https://github.com/acgaudette/alg).
in theory it can support any math library as a backend; see [`./slip.binds`](slip.binds).

# a short language reference #

slip intentionally ignores the structure of the host language
in order to maintain flexibility and simplicity.
although it was intended for use in C, it should 'compile' just about anywhere.
in particular, there's no reason why it couldn't be inlined in glsl!

anywhere the escape delimiter (default: `$`) is found
(except inside string literals)
slip considers the remainder of the line for compilation.
no terminating punctuator is necessary;
slip will e.g. insert semicolons for you automatically.

```
	const float four = $ + 2 2
```

you can always override this behavior
by inserting a terminating punctuator (`:`, `,`, `;`) yourself.
when a terminating punctuator is found, slip will reparse,
allowing you to include multiple statements in one line.

```
	struct cond = {
		test ? $ 1 : 2, 4
	};
```

slip uses prefix / polish notation, like lisp:

```
	$ * 1 + 2 3
```
```
=>	(1 * (2 + 3));
```

slip features type inference,
and will select the appropriate version of a function
based on the types of its arguments:

```
	$ + time dot [1 2 3] dir
	    ^                ^
	    scalar           3-vector
```
```
=>	(time + v3_dot((v3) { 1 2 3 }, dir));
```

ultimately, all expressions must resolve to a type.
because many vector functions are generic,
slip cannot always infer the type of your expression.
therefore, it is possible to annotate the type of a free variable manually:

```
	$ + a' b
	  ^
	  scalar addition
```

the `'` suffix serves as an annotation here.
you can use this for arbitrary vectors by including a number
(the `1` suffix for scalars is optional and can be elided):

```
	$ mix a'3 b t
	  ^
	  lerp between two 3-vectors
```

typically (as in above) you only need to annotate one of the arguments --
slip can figure out the rest.

all recognized slip types are vectors (scalars are glossed as 1-vectors).
all vectors, even 1-vectors, can be specified using the vector tokens
(default: `[` and `]`).

```
	$ [ 8 .16 d' ]
```
```
=>	(v3) { 8, .16, d };
```

however, an EOL or punctuator is a recognized terminating vector token;
and, because all expressions are actually just vectors,
you can elide vector tokens for top-level statements.
therefore, the following line is equivalent to the one directly above:

```
	$ 8 .16 d'
```

this makes writing inline vectors in code extremely lexically efficient!

slip's bindings include support for polymorphic 'constants':

```
	$ + one * up [ 2 4 ]
	    ^     ^
	    |     inferred as [ 0 1 ]
	    inferred as [ 1 1 ]
```

as well as 'macros': really just direct text expansions for convenience,
e.g.

```
	$ dt
=>	_time.dt.real;
```

slip's identifiers support most 'special characters', including operators:

```
	$ *q rot axis-angle up theta/t
```
