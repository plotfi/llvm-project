#include "clang/AST/Decl.h"
#include "clang/AST/Mangle.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Index/CodegenNameGenerator.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"

using namespace clang;
using namespace clang::index;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

using namespace driver;
using namespace options;
using namespace llvm::opt;

static cl::opt<std::string> Triple("triple", cl::desc("clang-ifso triple"),
                                   cl::init(""));

namespace clang {
namespace ast_matchers {
namespace {

AST_MATCHER(NamedDecl, isVisible) {
  return Node.getVisibility() == DefaultVisibility;
}

AST_POLYMORPHIC_MATCHER(hasDiscardableLinkage,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(FunctionDecl,
                                                        VarDecl)) {
  return false;
}

AST_MATCHER_P(ObjCImplementationDecl, hasInterface,
              ::clang::ast_matchers::internal::Matcher<ObjCInterfaceDecl>,
              InnerMatcher) {
  return true;
}

} // namespace
} // namespace ast_matchers
} // namespace clang

class FindVisible : public MatchFinder::MatchCallback {
  std::vector<std::string> VisibleNames;

public:
  void run(const MatchFinder::MatchResult &Result) override {
    const auto ND = Result.Nodes.getNodeAs<NamedDecl>("Definition");
    assert(ND && "expected matching NamedDecl named 'Definition'");
    ASTContext &Ctx = ND->getASTContext();
    index::CodegenNameGenerator CGNameGen(Ctx);
    std::string Name = CGNameGen.getName(ND);
    VisibleNames.push_back(Name);
  }

  const std::vector<std::string> &getVisibleNames() const {
    return VisibleNames;
  }

  bool ProducedSymbols = false;
};

void PrintYamlSymbols(const std::vector<std::string> &Names,
                      llvm::raw_ostream &OS);
void PrintYamlHeader(llvm::Triple &T, llvm::raw_ostream &OS);

int main(int argc, const char **argv) {
  cl::OptionCategory Category("clang-ifso options");
  CommonOptionsParser Opt(argc, argv, Category);

  unsigned MissingArgIndex = 0, MissingArgCount = 0;
  std::unique_ptr<opt::OptTable> Opts = createDriverOptTable();
  InputArgList Args = Opts->ParseArgs(llvm::makeArrayRef(argv + 1, argc),
                                      MissingArgIndex, MissingArgCount);

  std::string TripleStr =
      llvm::Triple::normalize(Args.getLastArgValue(OPT_triple));
  if (TripleStr == "unknown" || TripleStr == "")
    TripleStr = llvm::sys::getProcessTriple();
  llvm::Triple T(TripleStr);

  ClangTool Tool(Opt.getCompilations(), Opt.getSourcePathList());

  auto Adjuster = getInsertArgumentAdjuster("-Qunused-arguments");
  Tool.appendArgumentsAdjuster(Adjuster);

  auto VisibleDef =
      allOf(isDefinition(), isVisible(), unless(hasDiscardableLinkage()),
            unless(isStaticStorageClass()));
  auto Matcher = namedDecl(anyOf(
      functionDecl(VisibleDef), varDecl(VisibleDef, unless(hasLocalStorage())),
      objcImplementationDecl(hasInterface(isVisible()))));

  FindVisible Handler;
  MatchFinder Finder;
  Finder.addMatcher(id("Definition", Matcher), &Handler);

  if (Handler.ProducedSymbols)
    return EXIT_FAILURE;
  if (Tool.run(newFrontendActionFactory(&Finder).get()))
    return EXIT_FAILURE;

  PrintYamlHeader(T, llvm::errs());
  PrintYamlSymbols(Handler.getVisibleNames(), llvm::errs());
  return EXIT_SUCCESS;
}
