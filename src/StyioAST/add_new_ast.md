__Check List:__ Add a new ast class: 

[x] Declare in ASTDecl.hpp
[x] Define in AST.hpp

[x] Add a new type to `StyioASTType` enum in `StyioToken/Token.hpp`
[x] Add a new name to `reprASTType` method in `StyioToken/Token.cpp`

[x] Add it to `StyioToString/ToStringVisitor.hpp::ToStringVisitor`
[x] Add a new `toString` method to `StyioToString/ToStringVisitor.hpp::StyioRepr`

[x] Implement `toString` method in `StyioToString/ToString.cpp`

[x] Add it to `StyioAnalyzer/ASTAnalyzerAST.hpp::AnalyzerVisitor`
[x] Add a new `typeInfer` method to `StyioAnalyzer/ASTAnalyzerAST.hpp::StyioAnalyzer`
[x] Add a new `toStyioIR` method to `StyioAnalyzer/ASTAnalyzerAST.hpp::StyioAnalyzer`

[X] Implement `typeInfer` method in `StyioAnalyzer/TypeInfer.cpp`
[] Implement `toStyioIR` method in `StyioAnalyzer/ToStyioIR.cpp`