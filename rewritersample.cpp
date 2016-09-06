
#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stack>

#include "clang/AST/Attr.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace clang;

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
  MyASTVisitor(Rewriter &R) : TheRewriter(R) {} //MyASTVisitor Constructor, assigning &R to private variable named TheRewriter

  bool VisitStmt(Stmt *s) {

    if(!is_cl_kernel)
      return true;

    if (isa<IfStmt>(s)){
      
        IfStmt *IfStatement = cast<IfStmt>(s); //cast s which is Stmt* type to IfStmt* type
        
        Stmt *Then = IfStatement->getThen();
        Stmt *Else = IfStatement->getElse();
        if(else_checking.empty() || (else_checking.top() != IfStatement))
        {
          TheRewriter.InsertText(IfStatement->getIfLoc(), "//fuck1234();\n",true,true);//before enter
          //TheRewriter.InsertText(Then->getSourceRange().getEnd(), "\nIf_end();\n",true, true);
        }
        else
        {
          if(!else_checking.empty() && (else_checking.top() == IfStatement))
            else_checking.pop();
    
          //TheRewriter.InsertText(Then->getSourceRange().getEnd(), "\nIf_end();\n",true, true);
        }
        
        
        if (Else)
        {
         if(isa<IfStmt>(Else))
          {            else_checking.push(Else);
            return true;
          }
        
          TheRewriter.InsertText(Else->getLocEnd().getLocWithOffset(1), "\n//123if_end();\n",true, true);
        }
        else
        {
         TheRewriter.InsertText(Then->getLocEnd().getLocWithOffset(1), "\n//abcif_end();\n",true, true); 
        }
      
    }
    else if(isa<ForStmt>(s)){
      for_loop_trace = true;
      ForStmt *ForStatement = cast<ForStmt>(s); 
      TheRewriter.InsertText(ForStatement->getSourceRange().getBegin(), "\n//for_start();\n",true,true);//before enter
      TheRewriter.InsertTextAfter(ForStatement->getLocEnd().getLocWithOffset(1), "\n//for_end();\n");//after enter
    }
    else if(isa<WhileStmt>(s)){
      while_loop_trace=true;
      WhileStmt *WhileStatement = cast<WhileStmt>(s);
      TheRewriter.InsertText(WhileStatement->getSourceRange().getBegin(), "\n//while_start();\n",true,true);//before enter
      TheRewriter.InsertTextAfter(WhileStatement->getLocEnd().getLocWithOffset(1), "\n//while_end();\n");//after enter
       
    }
    else if(isa<CompoundStmt>(s))
    {
      CompoundStmt *Compoundstatement = cast<CompoundStmt>(s);
      if(for_loop_trace)
      {
        //TheRewriter.InsertText(Compoundstatement->getLocStart().getLocWithOffset(1), "\ncas_simt_raise();\n",true,true);
        for_loop_trace = false;
      }
      else if(while_loop_trace)
      {
        //TheRewriter.InsertText(Compoundstatement->getLocStart().getLocWithOffset(1), "\ncas_simt_raise();\n",true,true);
        while_loop_trace = false;
      }
    }
   

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.

    if (!f->hasAttr<OpenCLKernelAttr>())
        is_cl_kernel=false;
    else
        is_cl_kernel=true;

    if (f->hasBody()&&is_cl_kernel)
    {
          Stmt *FuncBody = f->getBody(); //Returns true if the function has a body (definition).

          // Type name as string
          /*QualType QT = f->getReturnType();
          std::string TypeStr = QT.getAsString();
		*/
          //Function name
          DeclarationName DeclName = f->getNameInfo().getName();
          std::string FuncName = DeclName.getAsString();
          std::string PrintFunc = "\n//kernel-name is : "+FuncName;
          //llvm::errs()<<PrintFunc<<"\n";
          
          TheRewriter.InsertText(FuncBody->getLocStart().getLocWithOffset(1),PrintFunc,true, true);

          
          TheRewriter.InsertText(FuncBody->getLocStart().getLocWithOffset(1),"\n//kernel_begin();\n",true, true);
          TheRewriter.InsertText(FuncBody->getLocEnd(),"//kernel_end();\n",true, true);

         
    } 

    return true;
  }

private:
  Rewriter &TheRewriter;
  int priority=0;
  bool is_cl_kernel;
  bool for_loop_trace = false;
  bool while_loop_trace = false;

  //Stmt* else_checking = NULL;
  stack<Stmt*> else_checking;
  Stmt* for_end=NULL;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : Visitor(R) {}

  // Override the method that gets called for each parsed top-level
  // declaration.igetLocEnd()
  virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b)
      // Traverse the declaration using our AST visitor.
      Visitor.TraverseDecl(*b);
    return true;
  }

private:
  MyASTVisitor Visitor;
};

int main(int argc, char *argv[]) {
  if (argc != 3) {
    llvm::errs() << "Usage: rewritersample <input_filename> <output_filename>\n";
    return 1;
  }

  ofstream myfile;
  myfile.open (argv[2]);

  // CompilerInstance will hold the instance of the Clang compiler for us,
  // managing the various objects needed to run the compiler.
  CompilerInstance TheCompInst;
  TheCompInst.createDiagnostics();

  LangOptions &lo = TheCompInst.getLangOpts();

  lo.OpenCL = 1;
  lo.OpenCLVersion=12;
  lo.Bool=1;


  // Initialize target info with the default triple for our platform.
  auto TO = std::make_shared<TargetOptions>();
  TO->Triple = llvm::sys::getDefaultTargetTriple();
  TargetInfo *TI =
      TargetInfo::CreateTargetInfo(TheCompInst.getDiagnostics(), TO);
  TheCompInst.setTarget(TI);

  TheCompInst.createFileManager();
  FileManager &FileMgr = TheCompInst.getFileManager();
  TheCompInst.createSourceManager(FileMgr);
  SourceManager &SourceMgr = TheCompInst.getSourceManager();
  TheCompInst.createPreprocessor(TU_Module);
  TheCompInst.createASTContext();

  // A Rewriter helps us manage the code rewriting task.
  Rewriter TheRewriter;
  TheRewriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());

  // Set the main file handled by the source manager to the input file.
  const FileEntry *FileIn = FileMgr.getFile(argv[1]);
  SourceMgr.setMainFileID(
      SourceMgr.createFileID(FileIn, SourceLocation(), SrcMgr::C_User));
  TheCompInst.getDiagnosticClient().BeginSourceFile(
      TheCompInst.getLangOpts(), &TheCompInst.getPreprocessor());

  // Create an AST consumer instance which is going to get called by
  // ParseAST.
  MyASTConsumer TheConsumer(TheRewriter);

  // Parse the file to AST, registering our consumer as the AST consumer.
  ParseAST(TheCompInst.getPreprocessor(), &TheConsumer,
           TheCompInst.getASTContext());

  // At this point the rewriter's buffer should be full with the rewritten
  // file contents.
  const RewriteBuffer *RewriteBuf =
      TheRewriter.getRewriteBufferFor(SourceMgr.getMainFileID());
  //llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
  myfile << std::string(RewriteBuf->begin(), RewriteBuf->end());
  myfile.close();

  return 0;
}
