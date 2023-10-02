#ifndef _Janus_UTIL_
#define _Janus_UTIL_

#include "janus.h"

#include <set>
#include <string>
#include <vector>

namespace janus {

class RegSet {
public:
    uint64_t            bits;
    /** \brief Constructs an empty *RegSet* (with all bits set to 0). */
    RegSet()            {bits = 0ll;}
    /** \brief Constructs a *RegSet* with the given bits set. */
    RegSet(uint32_t bits):bits(bits){}

    void                insert(uint32_t reg);
    void                remove(uint32_t reg);
    void                complement();
    bool                contains(uint32_t reg);
    void                merge(RegSet set);
    void                subtract(RegSet &set);
    void                intersect(RegSet &set);
    /** \brief Returns the number of bits set. */
    uint32_t            size();
    uint32_t            getNextLowest(uint32_t minReg);
    uint32_t            popNextLowest(uint32_t minReg);
    void                clear(){bits = 0ll;}
    void                setAll(){bits = 0xFFFFFFFFFFFFFFFFll;}
    void                toSTLSet(std::set<uint32_t> &regSet);
    bool                hasSIMD();
    void                toSIMD();
    void                removeRestricted();
};

RegSet operator+(RegSet a, RegSet b);
RegSet operator-(RegSet a, RegSet b);
RegSet operator&(RegSet a, RegSet b);
bool operator==(RegSet a, RegSet b);
std::ostream& operator<<(std::ostream& out, const RegSet& r);

/** \brief An optimised bit vector for analysing binary.
 *
 * Note: *operator==* and *operator!=* are also implemented.
 */
class BitVector {
public:
    /** \brief Data. Each block contains a 32 bit bitpattern */
    std::vector<uint32_t> blocks;
    /** \brief The number of valid bits in the last block. */
    uint32_t              leftover;

    /** \brief Constructs an empty *BitVector* */
    BitVector();
    ~BitVector(){};
    /** \brief Constructs a *BitVector* of the given size, with all bits set to *false* */
    BitVector(uint32_t width);
    /** \brief Constructs a *BitVector* of the given size, with all blocks set to the value given
     *         by blockValue.
     */
    BitVector(uint32_t width, uint32_t blockValue);

    /** \brief Returns the number of bits set. */
    uint32_t            size(); //number of bits set
    /** \brief Sets the given bit */
    void                insert(uint32_t bitPos);
    /** \brief Resets the given bit */
    void                remove(uint32_t bitPos);
    bool                contains(uint32_t bitPos);
    void                clear();
    void                setUniversal();
    void                merge(BitVector &bv);
    void                intersect(BitVector &bv);
    void                subtract(BitVector &bv);
    void                toSTLSet(std::set<uint32_t> &set);
    std::string         print();
};

bool operator==(BitVector &a, BitVector &b);
bool operator!=(BitVector &a, BitVector &b);

class FlagSet {
public:
    uint64_t            bits;
    /** \brief Constructs an empty *FlagSet* (with all bits set to 0). */
    FlagSet()            {bits = 0;}
    /** \brief Constructs a *FlagSet* with the given bits set. */
    FlagSet(uint64_t bits):bits(bits){}

    void                insert(uint64_t flag);
    void                remove(uint64_t flag);
    void                complement();
    bool                contains(uint64_t flag);
    void                merge(FlagSet set);
    void                subtract(FlagSet &set);
    void                intersect(FlagSet &set);
    /** \brief Returns the number of bits set. */
    uint32_t            size();
    void                clear(){bits = 0x0;}
    void                setAll(){bits = 0xFF;}
};

FlagSet operator+(FlagSet a, FlagSet b);
FlagSet operator-(FlagSet a, FlagSet b);
FlagSet operator&(FlagSet a, FlagSet b);
bool operator==(FlagSet a, FlagSet b);
std::ostream& operator<<(std::ostream& out, const FlagSet& r);

/* A 32-bit structure which contains four scratch registers */
class ScratchSet {
public:
    union {
        uint32_t         data;
        ScratchRegSet    regs;
    };

    ScratchSet() {data=0;}
    void insert             (uint32_t reg);
    uint32_t access         (uint32_t i);
    uint32_t find           (uint32_t reg);
};

std::ostream& operator<<(std::ostream& out, const ScratchSet& s);

} /* END Janus NAMESPACE */

void toRGB(double frequency, int *r, int *g, int *b);
#endif
