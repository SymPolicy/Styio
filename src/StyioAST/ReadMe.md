__Check List:__ Add a new ast class: 

[x] Declare in ASTDecl.hpp
[x] Define in AST.hpp

[x] Add a new type to `NodeHintType` enum in `StyioToken/Token.hpp`
[x] Add a new name to `reprNodeType` method in `StyioToken/Token.cpp`

[x] Add it to `StyioToString/ToStringVisitor.hpp::ToStringVisitor`
[x] Implement `toString` method in `StyioToString/ToString.cpp`

[x] Add it to `StyioAnalyzer/ASTAnalyzerAST.hpp::AnalyzerVisitor`
[x] Implement `typeInfer` method in `StyioAnalyzer/TypeInfer.cpp`
[x] Implement `toStyioIR` method in `StyioAnalyzer/ToStyioIR.cpp`