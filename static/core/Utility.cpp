#include "Utility.h"
#include "Arch.h"
#include "IO.h"
#include "janus_arch.h"
#include <bitset>
#include <cstdlib>
#include <cstring>
#include <sstream>

using namespace janus;
using namespace std;

void RegSet::insert(uint32_t reg) { bits = bits | get_reg_bit_array(reg); }

void RegSet::remove(uint32_t reg) { bits = bits & (~get_reg_bit_array(reg)); }

void RegSet::complement() { bits = ~bits; }

bool RegSet::contains(uint32_t reg) { return (bits & get_reg_bit_array(reg)); }

void RegSet::merge(RegSet set) { bits |= set.bits; }

void RegSet::subtract(RegSet &set) { bits &= (~(set.bits)); }

void RegSet::intersect(RegSet &set) { bits &= set.bits; }

uint32_t RegSet::size()
{
    if (!bits)
        return 0;
    uint32_t size = 0;
    uint32_t mask = 1;
    for (int i = 0; i < 64; i++) {
        if (bits & mask)
            size++;
        mask = mask << 1;
    }
    return size;
}

uint32_t RegSet::getNextLowest(uint32_t minReg)
{
    return decodeNextReg(bits, minReg);
}

uint32_t RegSet::popNextLowest(uint32_t minReg)
{
    uint32_t reg = RegSet::getNextLowest(minReg);
    RegSet::remove(reg);
    return reg;
}

void RegSet::toSTLSet(std::set<uint32_t> &regSet)
{
    decodeRegSet(bits, regSet);
}

std::ostream &janus::operator<<(std::ostream &out, const RegSet &r)
{
    set<uint32_t> regs;
    RegSet a = r;
    a.toSTLSet(regs);
    for (auto reg : regs) {
        out << get_reg_name(reg) << " ";
    }
    return out;
}

RegSet janus::operator+(RegSet a, RegSet b) { return RegSet(a.bits | b.bits); }

RegSet janus::operator-(RegSet a, RegSet b)
{
    return RegSet(a.bits & (~b.bits));
}

RegSet janus::operator&(RegSet a, RegSet b) { return RegSet(a.bits & b.bits); }

bool janus::operator==(RegSet a, RegSet b) { return a.bits == b.bits; }

/* Methods for BitVector */
BitVector::BitVector() { leftover = 0; }

BitVector::BitVector(uint32_t width)
{
    leftover = width % 32;
    int numBlocks = width / 32 + 1;
    blocks.reserve(numBlocks);
    for (int i = 0; i < numBlocks; i++)
        blocks.push_back(0);
}

BitVector::BitVector(uint32_t width, uint32_t blockValue)
{
    leftover = width % 32;
    int numBlocks = width / 32 + 1;
    blocks.reserve(numBlocks);
    for (int i = 0; i < numBlocks; i++)
        blocks.push_back(blockValue);
}

// least significant bit of x
#define LSB(x) (x & (-x))

uint32_t BitVector::size()
{
    uint32_t x, ret(0);
    for (int i = 0; i < blocks.size() - 1; i++) {
        x = blocks[i];
        while (x) {
            x ^= LSB(x);
            ret++;
        }
    }

    x = blocks[blocks.size() - 1];
    while ((x) && (LSB(x) < (1 << leftover))) {
        x ^= LSB(x);
        ret++;
    }

    return ret;
}

void BitVector::insert(uint32_t bitPos)
{
    uint32_t block_id = bitPos / 32;
    GASSERT(block_id <= blocks.size(), "bit position out of range");
    blocks[block_id] = blocks[block_id] | (1 << (bitPos % 32));
}

void BitVector::remove(uint32_t bitPos)
{
    uint32_t block_id = bitPos / 32;
    GASSERT(block_id <= blocks.size(), "bit position out of range");
    blocks[block_id] = blocks[block_id] & (~(1 << (bitPos % 32)));
}

bool BitVector::contains(uint32_t bitPos) const
{
    uint32_t block_id = bitPos / 32;
    GASSERT(block_id <= blocks.size(), "bit position out of range");
    return blocks[block_id] & (1 << (bitPos % 32));
}

void BitVector::clear()
{
    memset(blocks.data(), 0, blocks.size() * sizeof(uint32_t));
}

void BitVector::setUniversal()
{
    memset(blocks.data(), uint32_t(-1), blocks.size() * sizeof(uint32_t));
}

void BitVector::merge(BitVector &bv)
{
    uint32_t blockSize = bv.blocks.size();
    // GASSERT(blockSize == blocks.size() &&
    //       bv.leftover == leftover, "Vector length not match during merge");
    for (int i = 0; i < blockSize; i++)
        blocks[i] = blocks[i] | bv.blocks[i];
}

void BitVector::intersect(BitVector &bv)
{
    uint32_t blockSize = bv.blocks.size();
    // GASSERT(blockSize == blocks.size() &&
    //       bv.leftover == leftover, "Vector length not match during merge");
    for (int i = 0; i < blockSize; i++)
        blocks[i] = blocks[i] & bv.blocks[i];
}

void BitVector::subtract(BitVector &bv)
{
    uint32_t blockSize = bv.blocks.size();
    // GASSERT(blockSize == blocks.size() &&
    //       bv.leftover == leftover, "Vector length not match during merge");
    for (int i = 0; i < blockSize; i++)
        blocks[i] = blocks[i] & (~bv.blocks[i]);
}

void BitVector::toSTLSet(set<uint32_t> &set)
{
    uint32_t index = 0;
    uint32_t blockSize = blocks.size();
    for (int i = 0; i < blockSize; i++) {
        uint32_t end = (i != (blockSize - 1)) ? 32 : leftover;
        for (int j = 0; j < end; j++) {
            if (blocks[i] & (1 << j)) {
                set.insert(index);
            }
            index++;
        }
    }
}

string BitVector::print()
{
    stringstream ss;
    uint32_t index = 0;
    uint32_t blockSize = blocks.size();
    for (int i = 0; i < blockSize; i++) {
        uint32_t end = (i != (blockSize - 1)) ? 32 : leftover;
        for (int j = 0; j < end; j++) {
            if (blocks[i] & (1 << j)) {
                ss << index << " ";
            }
            index++;
        }
    }
    return ss.str();
}

bool janus::operator==(BitVector &a, BitVector &b)
{
    if (a.blocks.size() == b.blocks.size() && a.leftover == b.leftover) {
        for (int i = 0; i < a.blocks.size() - 1; i++) {
            if (a.blocks[i] != b.blocks[i])
                return false;
        }
        if ((a.blocks.size()) &&
            ((a.blocks[a.blocks.size() - 1] ^ b.blocks[a.blocks.size() - 1]) &
             ((1 << a.leftover) - 1)))
            return false; // last blocks differ
        return true;
    }
    return false;
}

bool janus::operator!=(BitVector &a, BitVector &b)
{
    if (a.blocks.size() != b.blocks.size() || a.leftover != b.leftover)
        return true;
    for (int i = 0; i < a.blocks.size() - 1; i++)
        if (a.blocks[i] != b.blocks[i])
            return true;
    if ((a.blocks.size()) &&
        ((a.blocks[a.blocks.size() - 1] ^ b.blocks[a.blocks.size() - 1]) &
         ((1 << a.leftover) - 1)))
        return true; // last blocks differ
    return false;
}

void ScratchSet::insert(uint32_t reg)
{
    if (!(regs.reg0)) {
        regs.reg0 = reg;
        return;
    }
    if (regs.reg0 == reg)
        return;
    if (!(regs.reg1)) {
        regs.reg1 = reg;
        return;
    }
    if (regs.reg1 == reg)
        return;
    if (!(regs.reg2)) {
        regs.reg2 = reg;
        return;
    }
    if (regs.reg2 == reg)
        return;
    if (!(regs.reg3)) {
        regs.reg3 = reg;
        return;
    }
}

uint32_t ScratchSet::find(uint32_t reg)
{
    if (reg == regs.reg0) {
        return 0;
    }
    if (reg == regs.reg1) {
        return 1;
    }
    if (reg == regs.reg2) {
        return 2;
    }
    if (reg == regs.reg3) {
        return 3;
    }
    return 4;
}

uint32_t ScratchSet::access(uint32_t i)
{
    return (uint32_t)(((uint8_t *)(&data))[i]);
}

std::ostream &janus::operator<<(std::ostream &out, const ScratchSet &s)
{
    out << get_reg_name(s.regs.reg0) << " " << get_reg_name(s.regs.reg1) << " "
        << get_reg_name(s.regs.reg2) << " " << get_reg_name(s.regs.reg3);
    return out;
}

void toRGB(double frequency, int *r, int *g, int *b)
{
    double max = 2;
    double min = 0;
    double m;
    if (frequency >= 1 && frequency < 1000)
        m = 1 + (frequency - 1) / 1000;
    else if (frequency >= 1000)
        m = max;
    /* m must be between min and max */
    GASSERT(m >= min && m <= max, "Convertion to RGB failed");
    double ratio = 2 * (frequency - min) / (max - min);
    if (ratio < 1 && ratio)
        *r = int(255 * (1 - ratio));
    *g = 0xff;
    //*g = 255 - *b - *r;
    *b = 0xff;
}
