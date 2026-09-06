Smew programming language
-------------------------

I'm making a programming language (⸝⸝╸-╺⸝⸝)

Current syntax view is @ example.sm

The language will be imperative, expression-oriented, no exceptions, maybe with effect system
and context propagation (i pulled the last term straight out of my head).

The goal is for simple-looking syntax to have simple and predictable semantics, and for expensive
operations to be clearly visible in the source code. Also sane defaults (I'm looking at you, C++)

Types will be nominal, + algebraic data types at least for sum-like results (to replace exceptions).

No classes and inheritance also, will go with traits.

Also no runtime reflection, but yes AST/syntax-based hygienic macros.

Roadmap:
- [ ] Lexer <-- I'm here
- [ ] Parser
- [ ] HIR lowering (a.k.a. desugaring)
- [ ] Scope and name resolution
- [ ] Semantic analysis (i.e. type checking)
- [ ] MIR lowering
- [ ] Simple (register-based?) VM
- [ ] Bytecode generation
