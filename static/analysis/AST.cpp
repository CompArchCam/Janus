#include "AST.h"
#include "Expression.h"
#include "Function.h"
#include "IO.h"
#include "Function.h"
#include "janus_arch.h"
#include <iostream>
#include <stack>
#include <signal.h>
using namespace janus;
using namespace std;

void buildASTGraph(Function *function)
{
	printf("							buildASTGraph --- START --- \n");
    auto &exprs = function->exprs;

    //check if SSA graph is present
    if (!function) return;

    if (!function->allStates.size()) {
        cerr<<"function "<<function->name<<" SSA not present in AST construction"<<endl;
        return;
    }
    printf("							buildASTGraph --- function->allStates.size() = %lu --- \n", function->allStates.size());
    //step 1: for each variable state (SSA node), reserve space for an expression
    for (auto vs: function->allStates) {
        //skip expression that has already been constructed
        if (vs->expr) continue;
        Expr *expr = new Expr(vs);
        vs->expr = expr;
        exprs.insert(expr);
    }

    printf("							buildASTGraph --- exprs.size() = %lu --- \n", exprs.size());

    printf("							buildASTGraph --- probe 1 --- \n");
    //step 2: create additional expressions for SSA node corner cases
    //copy existing nodes
    set<Expr *> exprRef = exprs;
    for (auto expr: exprRef) {
    	//printf("			buildASTGraph --- probe 2 --- \n");
        //initially all exprs are variable state
        VarState *vs = expr->v;
        //printf("			buildASTGraph --- vs->id = %d \n", vs->id);
        //printf("			buildASTGraph --- probe 3_0 --- \n");

        //if (vs->isPHI) {
        	//printf("			buildASTGraph --- probe 3_1 --- \n");
        //}
        //printf("			buildASTGraph --- probe 3_2 --- \n");
        //if the vs is a phi node 
        if (vs->isPHI) {
        	//printf("			buildASTGraph --- probe 4 --- \n");
            //change the expression to phi expressions
            vs->expr->kind = Expr::PHI;
            if (vs->pred.size() < 2) {
                vs->expr->kind = Expr::NONE;
                continue;
            }
            vs->expr->p.e1 = NULL;
            for (auto phi : vs->pred) {
                if (vs->expr->p.e1)
                    vs->expr->p.e2 = phi->expr;
                else vs->expr->p.e1 = phi->expr;
            }
            //printf("			buildASTGraph --- probe 5 --- \n");
        } else if (vs->type == JVAR_MEMORY) {
        	//printf("			buildASTGraph --- probe 6 --- \n");
            //change the expression to memory expressions
            vs->expr->kind = Expr::MEM;
            Variable mem = (Variable)*vs;
            Expr *addrExpr = new Expr();
            addrExpr->kind = Expr::BINARY;
            addrExpr->b.op = Expr::ADD;
            VarState *indexvs = NULL;
            VarState *basevs  = NULL;
            /* Link base and index to SSA */
            for (auto vi: vs->pred) {
                if ((Variable)*vi == Variable((uint32_t)mem.base)) {
                    basevs = vi;
                }
                if ((Variable)*vi == Variable((uint32_t)mem.index)) {
                    indexvs = vi;
                }
            }
            if (basevs && indexvs) {
                //base + index * scale + disp
                //A = index * scale
                Expr *indexExpr = newExpr(exprs);
                indexExpr->kind = Expr::BINARY;
                indexExpr->b.op = Expr::MUL;
                indexExpr->b.e1 = indexvs->expr;
                indexExpr->b.e2 = newExpr((int64_t)mem.scale, exprs);
                //B = A + disp
                Expr *indexExpr2 = newExpr(exprs);
                indexExpr2->kind = Expr::BINARY;
                indexExpr2->b.op = Expr::ADD;
                indexExpr2->b.e1 = indexExpr;
                indexExpr2->b.e2 = newExpr((int64_t)mem.value, exprs);
                //C = base + B
                addrExpr->kind = Expr::BINARY;
                addrExpr->b.op = Expr::ADD;
                addrExpr->b.e1 = basevs->expr;
                addrExpr->b.e2 = indexExpr2;
                //printf("			buildASTGraph --- probe 7 --- \n");
            } else if (basevs && !indexvs) {
            	//printf("			buildASTGraph --- probe 8 --- \n");
                //base + disp
                addrExpr->kind = Expr::BINARY;
                addrExpr->b.op = Expr::ADD;
                addrExpr->b.e1 = basevs->expr;
                addrExpr->b.e2 = newExpr((int64_t)mem.value, exprs);
                //printf("			buildASTGraph --- probe 9 --- \n");
            } else if (!basevs && indexvs) {
            	//printf("			buildASTGraph --- probe 10 --- \n");
                //A = index * scale
                Expr *indexExpr = newExpr(exprs);
                indexExpr->kind = Expr::BINARY;
                indexExpr->b.op = Expr::MUL;
                indexExpr->b.e1 = indexvs->expr;
                indexExpr->b.e2 = newExpr((int64_t)mem.scale, exprs);
                //A + disp
                addrExpr->kind = Expr::BINARY;
                addrExpr->b.op = Expr::ADD;
                addrExpr->b.e1 = indexExpr;
                addrExpr->b.e2 = newExpr((int64_t)mem.value, exprs);
                //printf("			buildASTGraph --- probe 11 --- \n");
            } else {
            	//printf("			buildASTGraph --- probe 11_1 --- \n");
                //something is wrong
                vs->expr->kind = Expr::NONE;
            }
            vs->expr->m = addrExpr;
        } else if (vs->type == JVAR_CONSTANT) {
        	//printf("			buildASTGraph --- probe 12 --- \n");
            vs->expr->kind = Expr::INTEGER;
            vs->expr->i = vs->value;
            //printf("			buildASTGraph --- probe 13 --- \n");
        } else if (vs->type == JVAR_SHIFTEDCONST || vs->type == JVAR_SHIFTEDREG) {
        	//printf("			buildASTGraph --- probe 14 --- \n");
            vs->expr = newExpr(exprs); 
            vs->expr->kind = Expr::BINARY;
            //Expr *shiftExpr = newExpr(exprs);
            //shiftExpr->kind = Expr::BINARY;

            VarState *regvs;
            VarState *shiftvs;
            for (auto vi: vs->pred) {
                if ((Variable)*vi == Variable((uint32_t)vs->value)) {
                    regvs = vi;
                }
            }
            if(vs->type == JVAR_SHIFTEDREG) {
                vs->expr->b.e1 = regvs->expr;
            }
            else if(vs->type == JVAR_SHIFTEDCONST) {
                vs->expr->b.e1 = newExpr((int64_t)vs->value, exprs);
            }
            switch(vs->shift_type)
            {
                case JSHFT_LSL:
                    vs->expr->b.op = Expr::SHL;
                break;
                case JSHFT_LSR:
                    vs->expr->b.op = Expr::LSR;
                break;
                case JSHFT_ASR:
                    vs->expr->b.op = Expr::ASR;
                break;
            }
            //printf("			buildASTGraph --- probe 4 --- \n");
            vs->expr->b.e2 = newExpr((int64_t)vs->shift_value, exprs);
            //printf("			buildASTGraph --- probe 15 --- \n");
        }
    }
    printf("							buildASTGraph --- probe 16 --- \n");
    //now link all the expressions based on SSA graph
    LOOPLOG("Build Janus Expressions:"<<endl);
    //LOOPLOG("The following instructions are not handled:"<<endl);
    for (auto &instr: function->instrs) {
        //link output with inputs in terms of expressions
        for (auto &output: instr.outputs) {
            buildExpr(*output->expr, exprs, &instr);
        }
    }
    printf("							buildASTGraph --- DONE --- \n");
}


static void printExprOp(Expr *expr, void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    switch (expr->kind) {
        case Expr::INTEGER:
            os << "0x"<<hex<<expr->i;
        break;
        case Expr::VAR:
            os << expr->vs;
        break;
        case Expr::MEM:
            os << "mem";
        break;
        case Expr::BINARY:
            switch (expr->b.op) {
                case Expr::ADD:
                    os << "+";
                break;
                case Expr::SUB:
                    os << "-";
                break;
                case Expr::MUL:
                    os << "x";
                break;
                default:
                    os << "udf";
                break;
            }
        break;
        case Expr::PHI:
            os << "phi";
        break;
        case Expr::UNARY:
            switch (expr->b.op) {
                case Expr::NEG:
                    os << "-";
                break;
                default:
                    os << "udf";
                break;
            }
        break;
        default:
        break;
    }
}

void Function::printASTDot(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    os<<dec;
    os << "digraph "<<name<<"AST {"<<endl;
    os << "label=\""<<name<<"_"<<dec<<fid+1<<"_AST\";"<<endl;

    for (auto expr:exprs) {
        os << "\tE"<<hex<<(uint64_t)(expr)<<" ";
        //append attributes
        os <<"[";
        //os <<"label=<"<< bb.dot_print()<<">";
        os <<"label=\"";
        printExprOp(expr, outputStream);
        os <<"\"";
        os<<",shape=plaintext"<<"];";

        os <<endl;

        //print edges
        switch (expr->kind) {
            case Expr::MEM:
                os << "\tE"<<hex<<(uint64_t)(expr->m)<<" -> E"<<(uint64_t)(expr)<<endl;
            break;
            case Expr::BINARY:
                os << "\tE"<<hex<<(uint64_t)(expr->b.e1)<<" -> E"<<(uint64_t)(expr)<<endl;
                os << "\tE"<<hex<<(uint64_t)(expr->b.e2)<<" -> E"<<(uint64_t)(expr)<<endl;
            break;
            case Expr::PHI:
                os << "\tE"<<hex<<(uint64_t)(expr->p.e1)<<" -> E"<<(uint64_t)(expr)<<endl;
                os << "\tE"<<hex<<(uint64_t)(expr->p.e2)<<" -> E"<<(uint64_t)(expr)<<endl;
            break;
            case Expr::UNARY:
                os << "\tE"<<hex<<(uint64_t)(expr->u.e)<<" -> E"<<(uint64_t)(expr)<<endl;
            break;
            default:
            break;
        }
    }
    os << "} "<<endl;
}
