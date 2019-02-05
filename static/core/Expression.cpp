#include "Expression.h"
#include "Function.h"
#include "Iterator.h"
#include "Variable.h"
#include "IO.h"
#include <iostream>
#include <set>
#include <stack>
#include <sstream>

using namespace janus;
using namespace std;

/* Construct an expression from a SSA variable state node */
Expr::Expr(VarState *vs)
:kind(VAR),vs(vs),v(vs)
{
    //link the current expression to vs
    //GASSERT(vs->expr == NULL, "vs expression already constructed");
    expandedLoopForm = NULL;
    expandedFuncForm = NULL;
    iteratorTo = NULL;
}

Expr *newExpr(set<janus::Expr*> &exprs)
{
    Expr *expr = new Expr();
    exprs.insert(expr);
    return expr;
}

Expr *newExpr(int64_t value, set<janus::Expr*> &exprs)
{
    Expr *expr = new Expr(value);
    exprs.insert(expr);
    return expr;
}

Expr *newExpr(VarState *vs, set<janus::Expr*> &exprs)
{
    Expr *expr = new Expr(vs);
    exprs.insert(expr);
    return expr;
}

bool janus::operator==(const Expr& expr1, const Expr& expr2)
{
    if (expr1.kind == Expr::INTEGER && expr2.kind == Expr::INTEGER) {
        return (uint64_t)expr1.i == (uint64_t)expr2.i;
    }
    if (expr1.kind != expr2.kind) return false;
    if (expr1.kind==Expr::VAR) {
        if ((uint64_t)expr1.v == (uint64_t)expr2.v) return true;
        else return false;
    }
    if (expr1.kind==Expr::MEM) {
        if ((uint64_t)expr1.m == (uint64_t)expr2.m) return true;
        else return false;
    }
    if (expr1.kind==Expr::EXPANDED) {
        if ((uint64_t)expr1.ee == (uint64_t)expr2.ee) return true;
        else return false;
    }
    if (expr1.kind==Expr::UNARY) {
        if (expr1.u.op != expr2.u.op) return false;
        if ((uint64_t)expr1.u.e != (uint64_t)expr2.u.e) return false;
        else return true;
    }
    if (expr1.kind==Expr::BINARY) {
        if (expr1.b.op != expr2.b.op) return false;
        if ((uint64_t)expr1.b.e1 != (uint64_t)expr2.b.e1) return false;
        if ((uint64_t)expr1.b.e2 != (uint64_t)expr2.b.e2) return false;
        return true;
    }
    if (expr1.kind==Expr::PHI) {
        if ((uint64_t)expr1.p.e1 != (uint64_t)expr2.p.e1) return false;
        if ((uint64_t)expr1.p.e2 != (uint64_t)expr2.p.e2) return false;
        return true;
    }
    return false;
}

bool janus::operator<(const Expr& expr1, const Expr& expr2)
{
    if (expr1.kind == Expr::INTEGER && expr2.kind == Expr::INTEGER) {
        return (uint64_t)expr1.i < (uint64_t)expr2.i;
    }
    else if (expr1.kind < expr2.kind) return true;
    else if (expr1.kind==expr2.kind) {
        if (expr1.kind==Expr::VAR) {
            if ((uint64_t)expr1.v < (uint64_t)expr2.v) return true;
            else return false;
        }
        if (expr1.kind==Expr::MEM) {
            if ((uint64_t)expr1.m < (uint64_t)expr2.m) return true;
            else return false;
        }
        if (expr1.kind==Expr::EXPANDED) {
            if ((uint64_t)expr1.ee < (uint64_t)expr2.ee) return true;
            else return false;
        }
        return ((uint64_t)expr1.vs < (uint64_t)expr2.vs);
        if (expr1.kind==Expr::UNARY) {
            if (expr1.u.op < expr2.u.op) return true;
            else if (expr1.u.op == expr2.u.op) {
                return ((uint64_t)expr1.u.e < (uint64_t)expr2.u.e);
            } else return false;
        }
        if (expr1.kind==Expr::BINARY) {
            if (expr1.b.op < expr2.b.op) return true;
            else if (expr1.u.op == expr2.u.op) {
                if ((uint64_t)expr1.b.e1 >= (uint64_t)expr2.b.e1) return false;
                if ((uint64_t)expr1.b.e2 >= (uint64_t)expr2.b.e2) return false;
                return true;
            } else return false;
        }
        if (expr1.kind==Expr::PHI) {
            if ((uint64_t)expr1.b.e1 >= (uint64_t)expr2.b.e1) return false;
            if ((uint64_t)expr1.b.e2 >= (uint64_t)expr2.b.e2) return false;
            return true;
        }
    } else return false;
}

void buildExpr(Expr &expr, set<Expr*> &exprs, Instruction *instr)
{

    if (!instr) return;
    if (instr->inputs.size() == 0) return;

    if (instr->opcode == Instruction::Add) {
        expr.kind = Expr::BINARY;
        expr.b.op = Expr::ADD;
        expr.b.e1 = instr->inputs[0]->expr;
        if (instr->inputs[1])
            expr.b.e2 = instr->inputs[1]->expr;
        else //duplicate the operands for "add x0, x0"
            expr.b.e2 = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Sub) {
        expr.kind = Expr::BINARY;
        expr.b.op = Expr::SUB;
        expr.b.e1 = instr->inputs[0]->expr;
        if (instr->inputs[1])
            expr.b.e2 = instr->inputs[1]->expr;
        else //duplicate the operands for "sub x0, x0"
            expr.b.e2 = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Mov) {
        if (instr->inputs.size()<1) {
            cerr<<"Error in parsing instruction "<<*instr;
            return;
        }
        expr.kind = Expr::UNARY;
        expr.u.op = Expr::MOV;
        expr.u.e = instr->inputs[0]->expr;
    }  else if (instr->opcode == Instruction::Neg) {
        if (instr->inputs.size()<1) {
            cerr<<"Error in parsing instruction "<<*instr;
            return;
        }
        expr.kind = Expr::UNARY;
        expr.u.op = Expr::NEG;
        expr.u.e = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::GetPointer) {
        VarState *vs = NULL;
        for (auto vi: instr->inputs) {
            if (vi->type == JVAR_POLYNOMIAL) {
                vs = vi;
            }
        }
        if (!vs) {
            cerr<<"Error in parsing instruction "<<*instr;
            return;
        }
        Variable mem = (Variable)*vs;

        if (mem.index == 0 && mem.base != 0) {
            //if index register is zero, simply creates an add expression
            //base + disp
            int64_t disp = mem.value;
            expr.kind = Expr::BINARY;
            expr.b.op = Expr::ADD;
            expr.b.e2 = newExpr(disp, exprs);
            for (auto vi: vs->pred) {
                if ((Variable)*vi == Variable((uint32_t)mem.base)) {
                    expr.b.e1 = vi->expr;
                }
            }
        } else if (mem.index != 0){
            VarState *indexvs = NULL;
            VarState *basevs  = NULL;
            for (auto vi: vs->pred) {
                if ((Variable)*vi == Variable((uint32_t)mem.base)) {
                    basevs = vi;
                }
                if ((Variable)*vi == Variable((uint32_t)mem.index)) {
                    indexvs = vi;
                }
            }
            if (!indexvs) {
                cerr<<"Error in parsing instruction "<<*instr<<endl;
                return;
            }
            //first create index and scale expression (index * scale)
            Expr *indexExpr = newExpr(exprs);
            indexExpr->kind = Expr::BINARY;
            indexExpr->b.op = Expr::MUL;
            indexExpr->b.e1 = indexvs->expr;
            indexExpr->b.e2 = newExpr((int64_t)mem.scale, exprs);
            //second create (index * scale + disp) expression
            if (basevs) {
                Expr *indexExpr2 = newExpr(exprs);
                indexExpr2->kind = Expr::BINARY;
                indexExpr2->b.op = Expr::ADD;
                indexExpr2->b.e1 = indexExpr;
                indexExpr2->b.e2 = newExpr((int64_t)mem.value, exprs);

                //third create (base+ (index * scale + disp))
                expr.kind = Expr::BINARY;
                expr.b.op = Expr::ADD;
                expr.b.e1 = basevs->expr;
                expr.b.e2 = indexExpr2;
            } else {
                expr.kind = Expr::BINARY;
                expr.b.op = Expr::ADD;
                expr.b.e1 = indexExpr;
                expr.b.e2 = newExpr((int64_t)mem.value, exprs);
            }
        } else {
            expr.kind = Expr::INTEGER;
            expr.i = mem.value;
        }
    } else if (instr->opcode == Instruction::Compare) {

    } else if (instr->opcode == Instruction::Mul) {
        expr.kind = Expr::BINARY;
        expr.b.op = Expr::MUL;
        expr.b.e1 = instr->inputs[0]->expr;
        if (instr->inputs.size()>1)
            expr.b.e2 = instr->inputs[1]->expr;
        else //duplicate the operands for "mul x0, x0"
            expr.b.e2 = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Div) {
        expr.kind = Expr::BINARY;
        expr.b.op = Expr::DIV;
        expr.b.e1 = instr->inputs[0]->expr;
        if (instr->inputs.size()>1)
            expr.b.e2 = instr->inputs[1]->expr;
        else //duplicate the operands for "div x0, x0"
            expr.b.e2 = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Load) {
        expr.kind = Expr::UNARY;
        expr.u.op = Expr::MOV;
        expr.u.e = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Store) {
        expr.kind = Expr::UNARY;
        expr.u.op = Expr::MOV;
        expr.u.e = instr->inputs[0]->expr;
    } else if (instr->opcode == Instruction::Shl) {

    } else if (instr->opcode == Instruction::LShr ||
               instr->opcode == Instruction::AShr) {

    } else if (instr->opcode == Instruction::Call ||
               instr->opcode == Instruction::Return) {

    } /* else if (instr->isMOV_partial()) {
        //partial move or type conversion
        if (instr->inputs.size()==2) {
            expr.kind = Expr::UNARY;
            expr.u.op = Expr::MOV;
            expr.u.e = instr->inputs[1]->expr;
        } else if (instr->inputs.size()==1) {
            expr.kind = Expr::UNARY;
            expr.u.op = Expr::MOV;
            expr.u.e = instr->inputs[0]->expr;
        } else {
            cerr<<"Error in parsing instruction ";
            instr->print(&cerr);
            return;
        }
    } */
    else {
        LOOPLOG("\t\tNot handled: "<<*instr<<endl);
    }
}

void Expr::add(Expr &expr) {
    if (kind == INTEGER) {
        if (expr.kind == INTEGER)
            i += expr.i;
        else {
            //switch to expanded exprs
            int64_t temp = i;
            Expr e1 = Expr(1);
            kind = EXPANDED;
            ee = new ExpandedExpr(ExpandedExpr::SUM);
            if (temp)
                ee->addTerm(e1,temp);
            else if (expr.kind == EXPANDED)
                ee->merge(expr.ee);
            else
                ee->addTerm(expr);
        }
    } else if (kind == EXPANDED) {
        if (expr.kind != EXPANDED)
            ee->addTerm(expr);
        else
            ee->merge(expr.ee);
    } else {
        //switch to expanded exprs
        Expr copy = *this;
        kind = EXPANDED;
        ee = new ExpandedExpr(ExpandedExpr::SUM);
        ee->addTerm(copy);
        ee->addTerm(expr);
    }
}

void Expr::add(ExpandedExpr *ex)
{
    if (kind == EXPANDED)
        ee->merge(ex);
    else {
        ExpandedExpr *e = new ExpandedExpr(ExpandedExpr::SUM);
        e->addTerm(this);
        e->merge(ex);
        ee = e;
        kind = EXPANDED;
    }
}

void Expr::subtract(Expr &expr) {
    if (kind == INTEGER) {
        if (expr.kind == INTEGER)
            i -= expr.i;
        else {
            //switch to expanded exprs
            int64_t temp = i;
            Expr e1 = Expr(1);
            kind = EXPANDED;
            Expr tempExpr = expr;
            tempExpr.negate();
            ee = new ExpandedExpr(ExpandedExpr::SUM);
            ee->addTerm(e1,temp);
            ee->addTerm(tempExpr);
        }
    } else if (kind == EXPANDED) {
        if (expr.kind != EXPANDED)
            ee->subtract(expr);
        else
            ee->subtract(expr.ee);
    } else {
        //switch to expanded exprs
        Expr copy = *this;
        Expr tempExpr = expr;
        tempExpr.negate();
        kind = EXPANDED;
        ee = new ExpandedExpr(ExpandedExpr::SUM);
        ee->addTerm(copy);
        ee->addTerm(tempExpr);
    }
}

void Expr::multiply(Expr &expr) {
    if (kind == INTEGER) {
        if (expr.kind == INTEGER)
            i *= expr.i;
        else {
            //switch to expanded exprs
            int64_t temp = i;
            Expr e1 = Expr(1);
            kind = EXPANDED;
            ee = new ExpandedExpr(ExpandedExpr::MUL);
            ee->addTerm(e1,temp);
            ee->addTerm(expr);
        }
    } else if (kind == EXPANDED) {
        ee->multiply(expr);
    } else {
        //switch to expanded exprs
        Expr copy = *this;
        kind = EXPANDED;
        ee = new ExpandedExpr(ExpandedExpr::MUL);
        ee->addTerm(copy);
        ee->addTerm(expr);
    }
}

void Expr::phi(Expr &expr) {
    if (kind == EXPANDED) {
        if (ee->kind == ExpandedExpr::PHI)
            ee->addTerm(expr);
        else {
            //change ee to PHI node
            ASSERT_NOT_IMPLEMENTED("inserting an phi expression into other type of expression");
        }
    } else {
        //Perform a copy of itself
        Expr copy = *this;
        //change itself to a phi expression
        kind = EXPANDED;
        ee = new ExpandedExpr(ExpandedExpr::SUM);
        ee->addTerm(copy);
        ee->addTerm(expr);
    }
}

void Expr::negate() {
    //current we only allow integers
    if (kind == INTEGER) {
        i = -i;
    } else {
        //switch to expanded exprs
        Expr copy = *this;
        kind = EXPANDED;
        ee = new ExpandedExpr(ExpandedExpr::SUM);
        ee->addTerm(copy, -1);
    }
}

//Creates and returns a new ExpandedExpr representing a left shift by an immediate as a multiply
ExpandedExpr Expr::shl(Expr &expr) {
    ExpandedExpr eexpr;
    if (expr.kind == INTEGER){
        //create a new expanded expr (converting shift to multiply)
        Expr copy;
        if(kind == VAR)
            copy = *this;
        else
            copy = Expr(i);
        int64_t multiplier = 1 << expr.i;
        Expr e1 = Expr(1);
        kind = EXPANDED;
        eexpr = ExpandedExpr(ExpandedExpr::MUL);
        eexpr.addTerm(e1, multiplier);
        eexpr.addTerm(copy);
    }
    else {
        cerr << "Can't evaluate shift by a register value";
    }
    return eexpr;
}

bool Expr::isLeaf() {
    if (kind == INTEGER ||
        kind == NONE ||
        kind == MEM ||
        kind == ADDREC) return true;

    if (kind == VAR && v->lastModified == NULL) return true;
    if (kind == VAR && v->lastModified->opcode == Instruction::Call) return true;
    return false;
}

bool Expr::isRegister() {
    if (kind == VAR && vs->type == JVAR_REGISTER) return true;
    return false;
}

bool Expr::isOne() {
    if (kind == INTEGER && i==1) return true;
    return false;
}

bool Expr::hasIterator(Loop *loop) {
    if (kind == EXPANDED) {
        return ee->hasIterator(loop);
    } else return false;
}

void Expr::simplify() {
    if (kind == EXPANDED) {
        ExpandedExpr *exe = ee;
        if (exe->exprs.size()==1) {
            int64_t num = 0;
            for (auto term: ee->exprs) {
                if (term.first.kind == INTEGER && term.second.kind == INTEGER)
                    num += (term.first.i * term.second.i);
                else return;
            }
            kind = INTEGER;
            i = num;
        } else ee->simplify();
    }
}

void Expr::expand() {
    if (kind == EXPANDED) return;
    Expr copy = *this;
    kind = EXPANDED;
    ee = new ExpandedExpr(ExpandedExpr::SUM);
    ee->addTerm(copy);
}

Expr ExpandedExpr::get(Expr node)
{
    return exprs[node];
}

Expr ExpandedExpr::get(VarState *vs)
{
    if (vs->expr)
        return exprs[*vs->expr];
    else
        return exprs[Expr(vs)];
}

Expr ExpandedExpr::get(int i)
{
    return exprs[Expr(i)];
}

bool ExpandedExpr::isEmpty()
{
    return exprs.size() == 0;
}

void ExpandedExpr::remove(Expr node)
{
    exprs.erase(node);
}

bool ExpandedExpr::hasTerm(VarState *vs)
{
    for (auto expr : exprs) {
        if (expr.first.vs == vs) return true;
    }
    return false;
}

bool ExpandedExpr::hasTerm(Expr node)
{
    return (exprs.find(node) != exprs.end());
}

void ExpandedExpr::addTerm(int value)
{
    addTerm(Expr(1), value);
}

void ExpandedExpr::addTerm(Expr node)
{
    //node should not be expanded type
    Expr e = Expr(1);
    addTerm(node, e);
}

void ExpandedExpr::addTerm(Expr *node)
{
    addTerm(*node);
}

void ExpandedExpr::addTerm(Expr node, int coeff)
{
    Expr e = Expr(coeff);
    addTerm(node, e);
}

void ExpandedExpr::addTerm(Expr *node, int coeff)
{
    addTerm(*node,coeff);
}

void ExpandedExpr::addTerm(Expr *node, Expr &coeff)
{
    addTerm(*node,coeff);
}

//main add term
void ExpandedExpr::addTerm(Expr node, Expr &coeff)
{
    if (kind == SUM) {
        if (node.kind==Expr::INTEGER && coeff.kind==Expr::INTEGER) {
            Expr nee = Expr((int64_t)node.i * coeff.i);
            Expr one = Expr(1);
            if (hasTerm(one)) {
                exprs[one].add(nee);
            } else exprs[one] = nee;
        } else if (hasTerm(node)) {
            exprs[node].add(coeff);
        } else exprs[node] = coeff;
    } else if (kind == PHI) {
        if (hasTerm(node)) {
            exprs[node].phi(coeff);
        } else exprs[node] = coeff;
    } else if (kind == MUL) {
        if (node.kind==Expr::INTEGER && coeff.kind==Expr::INTEGER) {
            Expr nee = Expr((int64_t)node.i * coeff.i);
            Expr one = Expr(1);
            if (hasTerm(one)) {
                exprs[one].multiply(nee);
            } else exprs[one] = nee;
        } else if (hasTerm(node)) {
            exprs[node].multiply(coeff);
        } else exprs[node] = coeff;
    } else {
        cout <<"Add term case not implemented"<<endl;
    }
}

void ExpandedExpr::merge(ExpandedExpr &expr)
{
    merge(&expr);
}

void ExpandedExpr::merge(ExpandedExpr *expr)
{
    if (!expr) return;
    if ((kind == SUM && expr->kind == SUM) ||
        (kind == PHI && expr->kind == PHI) ||
        (kind == MUL && expr->kind == MUL)) {
        for (auto e: expr->exprs) {
            addTerm(e.first, e.second);
        }
    } else if (kind == SUM && expr->kind == PHI) {
        if (exprs.size()==0) {
            //simply copy the phi expression
            *this = *expr;
            return;
        }
        /* A = a + b
         * B = (c,d)
         * A + B = (a+b+c,a+b+d)
         */
        //copy the current expressions
        ExpandedExpr copy = *this;
        //change the type from SUM to PHI
        *this = *expr;
        expand();
        //perform summation of each option of this phi expression
        for (auto &e: exprs) {
            Expr node = e.first;
            node.ee->merge(copy);
        }
    } else if (kind == PHI && expr->kind == SUM) {
        /* A = (a,b)
         * B = c + d
         * A + B = (a+c+d,b+c+d)
         */
        if (exprs.size()==0) {
            //create an expanded node
            ExpandedExpr *ee = new ExpandedExpr(ExpandedExpr::SUM);
            *ee = *expr;
            Expr node(ee);
            addTerm(node);
            return;
        }
        //expand the current phi
        expand();
        //perform summation of each option of this phi expression
        for (auto &e: exprs) {
            Expr node = e.first;
            node.ee->merge(expr);
        }
    } else if (kind == SUM && expr->kind == MUL) {
        for (auto &e: expr->exprs) {
            multiply(e.first);
            multiply(e.second);
        }
    } else {
        cerr<<"ExpandedExpr::merge error merge different types of expanded expression!"<<endl;
        return;
    }

}

void ExpandedExpr::negate()
{
    for (auto &e: exprs) {
        e.second.negate();
    }
}

void ExpandedExpr::subtract(Expr expr)
{
    if (expr.kind == Expr::EXPANDED)
        subtract(*expr.ee);
    else addTerm(expr,-1);
}

void ExpandedExpr::subtract(ExpandedExpr &expr)
{
    if (kind == SUM && expr.kind == SUM) {
        for (auto e: expr.exprs) {
            //convert A * 1 to 1 * A
            if (e.first.kind == Expr::INTEGER && e.second.kind == Expr::INTEGER) {
                addTerm(Expr(1), -(e.first.i * e.second.i));
                continue;
            }
            if (exprs.find(e.first) == exprs.end()) {
                Expr negExpr = e.second;
                negExpr.negate();
                addTerm(e.first, negExpr);
            } else {
                exprs[e.first].subtract(e.second);
            }
        }
    }
    else if (kind == PHI && expr.kind == SUM) {
        //expand all its term to expanded form
        expand();
        //phi - sum
        for (auto e: exprs) {
            //for each of its element, perform sum
            e.first.ee->subtract(expr);
        }
    }
    else if (kind == SUM && expr.kind == PHI) {
        //sum - phi
        ExpandedExpr copy = *this;
        //change the type from SUM to PHI
        *this = expr;
        expand();
        //perform summation of each option of this phi expression
        for (auto &e: exprs) {
            Expr node = e.first;
            node.ee->negate();
            node.ee->merge(copy);
        }
    } else {
        cerr<<"ExpandedExpr::subtract error subtract different types of expanded expression!"<<endl;
        return;
    }
    //after subtraction, simplify the term
    simplify(); 
}

void ExpandedExpr::multiply(Expr expr)
{
    if (kind == SUM) {
        for (auto &e: exprs) {
            e.second.multiply(expr);
        }
    } else if (kind == MUL) {
        addTerm(expr);
    } else if (kind == PHI) {
        for (auto &e: exprs) {
            e.second.multiply(expr);
        }
    } else {
        cerr<<"ExpandedExpr::multiply error cases not handled "<<*this<<endl;
        return;
    }
}

pair<Expr::ExprKind,int64_t>
ExpandedExpr::evaluate(Loop *loop)
{
    if (exprs.size() == 1) {
        auto term = exprs.begin();
        Expr first = (*term).first;
        Expr second = (*term).second;
        if (first.kind == Expr::INTEGER &&
            second.kind == Expr::INTEGER) {
            return make_pair(Expr::INTEGER, first.i * second.i);
        }
        else if (first.kind == Expr::VAR &&
            second.kind == Expr::INTEGER &&
            second.i == 1) {
            return make_pair(Expr::VAR, (int64_t)first.v);
        } else if (first.vs && loop->isConstant(first.vs) && second.kind == Expr::INTEGER && second.i == 1) {
            return make_pair(Expr::VAR, (int64_t)loop->getAbsoluteConstant(first.vs));
        } else if (first.kind == Expr::PHI &&
            second.kind == Expr::INTEGER &&
            second.i == 1) {
            return make_pair(Expr::VAR, (int64_t)first.vs);
        }
        else return make_pair(Expr::NONE, -1);
    } else return make_pair(Expr::NONE, -1);
}

void
ExpandedExpr::simplify()
{
    //simplify rule 1: convert immediate A * B to 1 * (A*B)
    for (auto e=exprs.begin(); e!=exprs.end();) {
        Expr node = (*e).first;
        Expr coeff = (*e).second;
        if (node.kind == Expr::INTEGER &&
            coeff.kind == Expr::INTEGER &&
            node.i != 1) {
            exprs.erase(e++);
            addTerm(Expr(1), node.i * coeff.i);
        }
        else ++e;
    }

    //simplify rule 2: remove terms with zero coefficient
    for (auto e=exprs.begin(); e!=exprs.end();) {
        Expr node = (*e).first;
        Expr coeff = (*e).second;
        //remove the term if the coeff is zero
        if (coeff.i == 0)
            exprs.erase(e++);
        else ++e;
    }

    //simplify rule 3: SUM(PHI) to PHI(SUM)
}

//Expand all its terms into the **ExpandedExpr** type
void
ExpandedExpr::expand()
{
    for (auto e=exprs.begin(); e!=exprs.end();) {
        Expr node = (*e).first;
        if (node.kind == Expr::INTEGER) {
            //change integer to expanded
            exprs.erase(e++);
            ExpandedExpr *ee = new ExpandedExpr(ExpandedExpr::SUM);
            node.kind = Expr::EXPANDED;
            node.ee = ee;
            ee->addTerm(node);
        } else e++;
    }
}

ExpandedExpr *expandExpr(Expr *expr, Loop *loop)
{
    if (!expr) return NULL;
    //if already constructed
    if (expr->expandedLoopForm) return expr->expandedLoopForm;

    //if not, allocate a new expanded expression, defaulting sum
    expr->expandedLoopForm = new ExpandedExpr(ExpandedExpr::SUM);

    //call internal loop expression constructor
    if (!buildLoopExpr(expr, expr->expandedLoopForm, loop)) {
        delete expr->expandedLoopForm;
        expr->expandedLoopForm = NULL;
    }
    return expr->expandedLoopForm;
}

ExpandedExpr *expandExpr(Expr *expr, Function *func, Loop *loop)
{
    if (!expr) return NULL;
    //if already constructed
    if (expr->expandedFuncForm) return expr->expandedFuncForm;

    //if not, allocate a new expanded expression, defaulting sum
    expr->expandedFuncForm = new ExpandedExpr(ExpandedExpr::SUM);

    //call internal loop expression constructor
    if (!buildFuncExpr(expr, expr->expandedFuncForm, func, loop, false, NULL)) {
        delete expr->expandedFuncForm;
        expr->expandedFuncForm = NULL;
    }
    return expr->expandedFuncForm;
}

ostream& janus::operator<<(ostream& out, const Expr& expr)
{
    switch (expr.kind) {
        case Expr::INTEGER:
            if (expr.i < 0)
                out<<"-0x"<<hex<<-expr.i;
            else
                out<<"0x"<<hex<<expr.i;
        break;
        case Expr::VAR:
            out<<expr.v;
        break;
        case Expr::MEM:
            if (expr.m) {
                if (expr.m->expandedLoopForm)
                    out <<"["<<*expr.m->expandedLoopForm<<"]";
                else
                    out <<expr.vs;
            }
            else
                out<<"[NULL]";
        break;
        case Expr::EXPANDED:
            out<<*expr.ee;
        break;
        case Expr::ADDREC:
            out<<"{"<<*expr.rec.base<<",+,"<<*expr.rec.stride<<"}@L"<<dec<<expr.rec.iter->loop->id;
        break;
        default:
            if (expr.vs) {
                out<<expr.vs;
            } else {
                out<<"UDF";
            }
        break;
    }
    if (expr.iteratorTo && expr.kind != Expr::ADDREC) {
        out<<"@L"<<dec<<expr.iteratorTo->loop->id;
        return out;
    }
    return out;
}

ostream& janus::operator<<(ostream& out, const ExpandedExpr& expr)
{
    char op;
    bool firstTerm = true;

    if (expr.kind == ExpandedExpr::SUM) op = '+';
    else if (expr.kind == ExpandedExpr::PHI) op = ',';
    else if (expr.kind == ExpandedExpr::MUL) op = '*';
    else op = '?';

    if (expr.kind == ExpandedExpr::PHI) out <<"(";
    if (expr.exprs.size()==0) {
        out<<"0";
        return out;
    }
    for (auto e: expr.exprs) {
        //print operations
        if (!firstTerm) {
            if (expr.kind == ExpandedExpr::PHI)
                out <<op;
            else
                out <<" "<<op<<" ";
        } else firstTerm = false;
        //print the term
        if (e.first.kind == Expr::INTEGER && e.first.i == 1) {
            out <<e.second;
            continue;
        }
        if (e.first.kind == Expr::EXPANDED) {
            out<<"Error";
            return out;
        }
        out << e.first;
        //print the coefficient
        Expr &coeff = e.second;
        //don't print constant 1
        if (!(coeff.isOne()))
            out <<"*"<<coeff;
    }
    if (expr.kind == ExpandedExpr::PHI) out <<")";
    return out;
}

ExpandedExpr janus::operator-(const ExpandedExpr &expr1, const ExpandedExpr &expr2)
{
    ExpandedExpr result = expr1;
    ExpandedExpr sub = expr2;
    result.subtract(sub);
    return result;
}

ExpandedExpr janus::operator&(const ExpandedExpr &expr1, const ExpandedExpr &expr2)
{
    ExpandedExpr result(ExpandedExpr::SUM);

    if (expr1.kind != ExpandedExpr::SUM || expr2.kind != ExpandedExpr::SUM) return result;
    for (auto e: expr1.exprs) {
        Expr term = e.first;
        if (expr2.exprs.find(term) != expr2.exprs.end()) {
            result.addTerm(e.first, e.second);
        }
    }
    return result;
}

void
ExpandedExpr::extendToFuncScope(Function *func, Loop *loop)
{
    map<Expr, Expr> refs = exprs;
    exprs.clear();

    for (auto e: refs) {
        Expr term = e.first;

        if (!term.isLeaf()) {
            ExpandedExpr terms(ExpandedExpr::SUM);
            buildFuncExpr(&term, &terms, func, loop, false, NULL);
            terms.multiply(e.second);
            merge(terms);
        } else exprs[e.first] = e.second;
    }
    simplify();
}

void
ExpandedExpr::extendToLoopScope(Loop *loop)
{
    map<Expr, Expr> refs = exprs;
    exprs.clear();

    for (auto e: refs) {
        Expr term = e.first;
        if (!term.isLeaf()) {
            ExpandedExpr terms(ExpandedExpr::SUM);
            buildLoopExpr(&term, &terms, loop);
            terms.multiply(e.second);
            merge(terms);
        } else if (term.kind == Expr::MEM) {
            //expand memory address
            term.expandedLoopForm = expandExpr(term.m, loop);
            exprs[term] = e.second;
        }
        else exprs[e.first] = e.second;
    }
    simplify();
}

void
ExpandedExpr::extendToMemoryScope(Function *func, Loop *loop)
{
    map<Expr, Expr> refs = exprs;
    exprs.clear();

    for (auto e: refs) {
        Expr term = e.first;
        if (term.isRegister() || !term.isLeaf()) {
            //register expression should be expanded
            ExpandedExpr terms(ExpandedExpr::SUM);
            buildFuncExpr(&term, &terms, func, loop, true, NULL);
            terms.multiply(e.second);
            merge(terms);
        } else exprs[e.first] = e.second;
    }
}

bool
ExpandedExpr::extendToVariableScope(Function *func, Loop *loop, Variable *var)
{
    map<Expr, Expr> refs = exprs;
    exprs.clear();

    for (auto e: refs) {
        Expr term = e.first;
        //we only expand non-leaf terms
        if (!term.isLeaf()) {
            ExpandedExpr terms(ExpandedExpr::SUM);
            buildFuncExpr(&term, &terms, func, loop, true, var);
            terms.multiply(e.second);
            merge(terms);
        } else exprs[e.first] = e.second;
    }
}


void
ExpandedExpr::removeImmediate()
{
    for (auto e=exprs.begin(); e!=exprs.end();) {
        Expr node = (*e).first;
        Expr coeff = (*e).second;
        //remove the term if the coeff is zero
        if (node.kind == Expr::INTEGER && coeff.kind == Expr::INTEGER)
            exprs.erase(e++);
        else ++e;
    }
}

bool
ExpandedExpr::isConstant()
{
    bool result = true;
    for (auto e: exprs) {
        Expr term = e.first;
        if (term.kind == Expr::MEM) {
            if (term.expandedLoopForm)
                result &= term.expandedLoopForm->isConstant();
            else
                return false;
        } else if (term.iteratorTo)
            return false;
    }
    return result;
}

Iterator *
ExpandedExpr::hasIterator(Loop *loop)
{
    for (auto e: exprs) {
        Expr term = e.first;
        if (term.iteratorTo && term.iteratorTo->loop == loop)
            return term.iteratorTo;
        if (term.kind == Expr::ADDREC && term.rec.iter->loop == loop)
            return term.rec.iter;
    }
    return NULL;
}

bool
ExpandedExpr::isArrayBase(Loop *loop)
{
    if (!exprs.size()) return false;
    for (auto e: exprs) {
        Expr term = e.first;
        if (term.kind == Expr::INTEGER) {
            if (exprs.size() > 1) continue;
            else return false;
        }
        if (term.iteratorTo && term.iteratorTo->loop == loop)
            return false;
    }
    return true;
}

ExpandedExpr
ExpandedExpr::removeIterator(Loop *loop)
{
    ExpandedExpr result(ExpandedExpr::SUM);
    for (auto e: exprs) {
        Expr term = e.first;
        if (!(term.iteratorTo && term.iteratorTo->loop == loop))
            result.addTerm(e.first, e.second);
    }
    return result;
}

bool buildLoopExpr(Expr *expr, ExpandedExpr *expanded, Loop *loop)
{
    if (expr == NULL) return false;
    /* Step 1: Checks if the expression code is beyond the scope */
    if (expr->vs && loop->isConstant(expr->vs)) {
        expanded->addTerm(*expr);
        return true;
    }

    /* Step 2: Checks if the expression node reaches to the definition of loop iterators */
    if (expr->vs && loop->iterators.find(expr->vs) != loop->iterators.end()) {
        //simply add this loop iterator
        expanded->addTerm(*expr);
        return true;
    }

    /* The following code creates a new expandedExpr class */
    switch (expr->kind) {
        case Expr::INTEGER:
        case Expr::VAR:
        case Expr::MEM:
            expanded->addTerm(expr);
        break;
        case Expr::UNARY:
            if (expr->u.op == Expr::MOV) {
                if (!buildLoopExpr(expr->u.e, expanded, loop)) return false;
            } else if (expr->u.op == Expr::NEG) {
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                if (!buildLoopExpr(expr->u.e, &negExpr, loop)) return false;
                negExpr.negate();
                expanded->merge(negExpr);
            } else {
                //LOOPLOG("\t\t\tUnrecognised unary expressions" <<endl);
            }
        break;
        case Expr::BINARY:
            if (expr->b.op == Expr::ADD) {
                if (!buildLoopExpr(expr->b.e1, expanded, loop)) return false;
                if (!buildLoopExpr(expr->b.e2, expanded, loop)) return false;
            } else if (expr->b.op == Expr::SUB) {
                if (!buildLoopExpr(expr->b.e1, expanded, loop)) return false;
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                if (!buildLoopExpr(expr->b.e2, &negExpr, loop)) return false;
                negExpr.negate();
                expanded->merge(negExpr);
            } else if (expr->b.op == Expr::SHL) {
                ExpandedExpr shiftExpr = expr->b.e1->shl(*expr->b.e2);
                expanded->merge(shiftExpr);
            } else if (expr->b.op == Expr::MUL) {
                if (expr->b.e2->kind == Expr::INTEGER) {
                    ExpandedExpr *ee1 = new ExpandedExpr(ExpandedExpr::SUM);
                    if (!buildLoopExpr(expr->b.e1, ee1, loop)) return false;
                    ee1->multiply(Expr(expr->b.e2->i));
                    expanded->merge(ee1);
                    delete ee1;
                } else {
                    //LOOPLOG("\t\t\tUnrecognised binary expressions" <<endl);
                }
            } else {
                //LOOPLOG("\t\t\tUnrecognised binary expressions" <<endl);
            }
        break;
        case Expr::PHI:
            //if it is a phi expression, firstly check if this phi belongs to any loop's iterator
            if (expr->iteratorTo) {
                //simply add this term
                expanded->addTerm(expr);
            } else {
                ExpandedExpr *ee1 = expandExpr(expr->p.e1, loop);
                ExpandedExpr *ee2 = expandExpr(expr->p.e2, loop);
                expr->expandedFuncForm = new ExpandedExpr(ExpandedExpr::PHI);
                expr->expandedFuncForm->addTerm(Expr(ee1));
                expr->expandedFuncForm->addTerm(Expr(ee2));
                expanded->addTerm(expr);
            }
        break;
        default:
        break;
    }
    return true;
}

/* Traverse upwards along the AST graph
 * If the starting point is inside a loop, please also supply the belonging loop pointer */
bool buildFuncExpr(Expr *expr, ExpandedExpr *expanded,
                   Function *func, Loop *loop,
                   bool stopAtMemory, Variable *stopAtVariable)
{
    if (expr == NULL) return false;

    /* Step 1: if it is leaf node, simply add into the expanded and return */
    if (stopAtMemory) {
        if (expr->vs && expr->vs->type != JVAR_REGISTER) {
            expanded->addTerm(expr);
            return true;
        }
    } else {
        if (expr->isLeaf()) {
            expanded->addTerm(expr);
            return true;
        }
    }
    
    /* Stop at particular variable */
    if (stopAtVariable) {
        if (expr->vs && *(Variable *)expr->vs == *stopAtVariable) {
            expanded->addTerm(expr);
            return true;
        }
    }

    switch (expr->kind) {
        case Expr::INTEGER:
        case Expr::MEM:
            expanded->addTerm(expr);
        break;
        case Expr::VAR:
            //if it is an expression created outside AST construction, the real expection is wrapped
            if (expr->vs->expr && expr->vs->expr->kind != Expr::VAR) {
                if (!buildFuncExpr(expr->vs->expr, expanded, func, loop, stopAtMemory, stopAtVariable)) return false;
            }
            else
                expanded->addTerm(expr);
        break;
        case Expr::UNARY:
            if (expr->u.op == Expr::MOV) {
                if (!buildFuncExpr(expr->u.e, expanded, func, loop, stopAtMemory, stopAtVariable)) return false;
            } else if (expr->u.op == Expr::NEG) {
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                if (!buildFuncExpr(expr->u.e, &negExpr, func, loop, stopAtMemory, stopAtVariable)) return false;
                negExpr.negate();
                expanded->merge(negExpr);
            } else {
                //LOOPLOG("\t\t\tUnrecognised unary expressions" <<endl);
            }
        break;
        case Expr::BINARY:
            if (expr->b.op == Expr::ADD) {
                if (!buildFuncExpr(expr->b.e1, expanded, func, loop, stopAtMemory, stopAtVariable)) return false;
                if (!buildFuncExpr(expr->b.e2, expanded, func, loop, stopAtMemory, stopAtVariable)) return false;
            } else if (expr->b.op == Expr::SUB) {
                if (!buildFuncExpr(expr->b.e1, expanded, func, loop, stopAtMemory, stopAtVariable)) return false;
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                if (!buildFuncExpr(expr->b.e2, &negExpr, func, loop, stopAtMemory, stopAtVariable)) return false;
                negExpr.negate();
                expanded->merge(negExpr);
            } else if (expr->b.op == Expr::SHL) {
                ExpandedExpr shiftExpr = expr->b.e1->shl(*expr->b.e2);
                expanded->merge(shiftExpr);
            } else {
                //LOOPLOG("\t\t\tUnrecognised binary expressions" <<endl);
            }
        break;
        case Expr::PHI: {
            //for this phi node, we first need to check if this phi belongs to
            //an iterator of the parent loops
            if (expr->iteratorTo) {
                Iterator *sit = expr->iteratorTo;
                Loop *suspect = sit->loop;

                //if the suspect loop iterator is the descendants of the current loop
                //Use the final value of the iterator
                if (loop->descendants.find(suspect) != loop->descendants.end()) {
                    if (!sit->main) {
                        LOOPLOG("\t\t\tFound phi loop iterator but not a main iterator "<<dec<<suspect->id<<endl);
                        return false;
                    }
                    //add its final values to the cyclic expression
                    if (sit->finalKind == Iterator::INTEGER) {
                        expanded->addTerm(Expr(sit->finalImm));
                    }
                    else if (sit->finalKind == Iterator::EXPANDED_EXPR) {
                        bool cycle = false;
                        for (auto term: sit->finalExprs->exprs) {
                            Expr t = term.first;
                            if (t.kind == Expr::INTEGER) {
                                expanded->addTerm(t,term.second);
                            } else
                            //try to find cycles from the final expressions
                            buildFuncExpr(&t, expanded, func, loop, stopAtMemory, stopAtVariable);
                        }
                    }
                } else {
                    //if it is the ancestor
                    //simply add the term for range propagation
                    expanded->addTerm(expr);
                }
            } else {
                //expand the expression for both paths
                ExpandedExpr *ee1 = expandExpr(expr->p.e1, func, loop);
                ExpandedExpr *ee2 = expandExpr(expr->p.e2, func, loop);
                if (!ee1 || !ee2) return false;
                if (ee1->size() == 0 || ee2->size() == 0) return false;
                //if two expressions are exactly the same, simply add one
                if (*ee1 == *ee2) {
                    expanded->merge(ee1);
                } else {
                    expr->expandedFuncForm = new ExpandedExpr(ExpandedExpr::PHI);
                    expr->expandedFuncForm->addTerm(Expr(ee1));
                    expr->expandedFuncForm->addTerm(Expr(ee2));
                    expanded->addTerm(expr);
                }
            }
        }
        break;
        default:
            //LOOPLOG("\t\t\tUnrecognised expr expressions" <<endl);
        break;
    }
    return true;
}

ExpandedExpr *ExpandedExpr::getArrayBase(Loop *loop)
{
    ExpandedExpr *result = new ExpandedExpr(SUM);

    for (auto e: exprs) {
        Expr term = e.first;
        //skip all integer part of the expression
        if (term.kind == Expr::INTEGER && e.second.kind == Expr::INTEGER) {
            continue;
        }
        else if (term.iteratorTo) {
            Loop *currentLoop = term.iteratorTo->loop;
            //if the iterator belongs to the current loop
            if (currentLoop == loop) {
                //place the init expr to array base expression
                if (term.iteratorTo->initKind == Iterator::EXPR)
                    result->addTerm(term.iteratorTo->initExpr);
                else if (term.iteratorTo->initKind == Iterator::SINGLE_VAR)
                    result->addTerm(term.iteratorTo->initVar->expr);
                else if (term.iteratorTo->initKind == Iterator::EXPANDED_EXPR)
                    result->merge(term.iteratorTo->initExprs);
                else if (term.iteratorTo->initKind == Iterator::INTEGER)
                    continue;
            } else if (term.iteratorTo->initExprs) {
                ExpandedExpr *temp = term.iteratorTo->initExprs->getArrayBase(loop);
                result->merge(temp);
                delete temp;
            } else if (term.iteratorTo->initKind == Iterator::EXPR) {
                //expand the expressions and get the array base
                term.iteratorTo->initExprs = expandExpr(&term.iteratorTo->initExpr, loop->parent, loop);
                if (!term.iteratorTo->initExprs) {
                    result->addTerm(term.iteratorTo->initExpr);
                } else {
                    ExpandedExpr *temp = term.iteratorTo->initExprs->getArrayBase(loop);
                    result->merge(temp);
                    delete temp;
                }
            } else if (term.iteratorTo->initKind == Iterator::SINGLE_VAR) {
                term.iteratorTo->initExprs = expandExpr(term.iteratorTo->initVar->expr, loop->parent, loop);
                //expand the expressions and get the array base
                if (!term.iteratorTo->initExprs) {
                    result->addTerm(term.iteratorTo->initVar);
                } else {
                    ExpandedExpr *temp = term.iteratorTo->initExprs->getArrayBase(loop);
                    result->merge(temp);
                    delete temp;
                }
            }
        } else result->addTerm(e.first, e.second);
    }
    return result;
}

bool janus::operator<(const ExpandedExpr& expr1, const ExpandedExpr& expr2)
{
    auto exprs1 = expr1.exprs;
    auto exprs2 = expr2.exprs;
    //get a special key for the expression
    int64_t key1 = 0;
    int64_t key2 = 0;
    for (auto e: exprs1) {
        key1 += (e.first.i * e.second.i);
    }
    for (auto e: exprs2) {
        key2 += (e.first.i* e.second.i);
    }

    return (key1 < key2);
}

bool janus::operator==(const ExpandedExpr& expr1, const ExpandedExpr& expr2)
{
    if (expr1.kind != expr2.kind) return false;
    auto exprs1 = expr1.exprs;
    auto exprs2 = expr2.exprs;
    //get a special key for the expression
    int64_t key1 = 0;
    int64_t key2 = 0;
    for (auto e: exprs1) {
        key1 += (e.first.i * e.second.i);
    }
    for (auto e: exprs2) {
        key2 += (e.first.i * e.second.i);
    }

    return (key1 == key2);
}
