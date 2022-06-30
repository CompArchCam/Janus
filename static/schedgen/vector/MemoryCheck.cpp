#include "MemoryCheck.h"

int maxUnrollFactor = 8;

static bool checkMemoryDependencies(Loop *loop, uint64_t stride)
{
    for (auto dep : loop->memoryDependences) {
        for (auto mem : dep.second) {
            ExpandedExpr diff = dep.first->expr - mem->expr;
            auto eval = diff.evaluate(loop);
            if (eval.first == Expr::INTEGER) {
                int64_t diffImm = eval.second;
                if (diffImm != 0 && loop->mainIterator &&
                    loop->mainIterator->stepKind == Iterator::CONSTANT_IMM &&
                    loop->mainIterator->kind == Iterator::INDUCTION_IMM) {
                    cout << "Diff: " << diffImm << endl;
                    cout << "Double stride: " << (stride * maxUnrollFactor * 2)
                         << endl;
                    bool readAfterWrite = false;
                    for (auto write : dep.first->writeFrom) {
                        for (auto mem : dep.second) {
                            for (auto read : mem->readBy) {
                                if (read->pc >= write->pc)
                                    readAfterWrite = true;
                            }
                        }
                    }
                    // Accept read before write dependencies.
                    // and accept other memory dependencies that are separated
                    // by at least 2 x maxVectorSize. (2 for safety)
                    if ((diffImm < 0 && !readAfterWrite) ||
                        abs(diffImm) >= (stride * maxUnrollFactor * 2)) {
                        continue;
                    }
                }
            }
            LOOPLOGLINE("loop " << dec << loop->id
                                << " memory dependence within vector size*2.");
            return false;
        }
    }
    return true;
}

static bool doesMemoryAccessOverlapNaive(Loop *loop, uint64_t stride)
{
    long maxOffset = (stride * loop->staticIterCount) - 1;
    long minRead, maxRead, minWrite, maxWrite;
    for (auto read : loop->memoryReads) {
        for (auto write : loop->memoryWrites) {
            if (read->type == LOOP_MEM_UNDECIDED) {
                maxRead = read->scev.expression.evaluateMax();
                minRead = maxRead - maxOffset - 1;
                if (write->type == LOOP_MEM_CONSTANT) {
                    maxWrite = write->scev.expression.evaluateMax();
                    minWrite = maxWrite;
                    if (maxWrite >= minRead && maxWrite <= maxRead) {
                        return true;
                    }
                }
            } else if (read->type == LOOP_MEM_CONSTANT) {
                maxRead = read->scev.expression.evaluateMax();
                minRead = maxRead;
                if (write->type == LOOP_MEM_UNDECIDED) {
                    maxWrite = write->scev.expression.evaluateMax();
                    minWrite = maxWrite - maxOffset - 1;
                    if (maxRead >= minWrite && maxRead <= maxWrite) {
                        return true;
                    }
                } else if (write->type == LOOP_MEM_CONSTANT) {
                    maxWrite = write->scev.expression.evaluateMax();
                    minWrite = maxWrite;
                    if (maxWrite == maxRead) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool checkMemory(Loop *loop, uint64_t stride)
{
    if (doesMemoryAccessOverlapNaive(loop, stride)) {
        LOOPLOGLINE("loop " << dec << loop->id << " memory overlap.");
        return false;
    }
    if (!checkMemoryDependencies(loop, stride))
        return false;
    return true;
}