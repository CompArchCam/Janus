/*! \file loop_format.h
 *  \brief Defines the format of loop meta information in the rewrite schedule
 */
#ifndef _JANUS_RS_LOOP_FORMAT_
#define _JANUS_RS_LOOP_FORMAT_

/** \brief Approach to parallelise the given loop */
typedef enum _para_schedule
{
    ///Each thread split the loop iterations into equal chunks
    PARA_DOALL_BLOCK,
    ///Each thread steals a chunk of the iterations
    PARA_DOALL_CYCLIC_CHUNK,
    ///Each thread execute a chunk of iterations in speculative mode
    PARA_SPEC_CYCLIC_CHUNK,
} SchedulePolicy;


/** \brief  loop header
 *
 * Meta information per loop used for dynamic parallelisation under Janus */
typedef struct loop_header {
    /** \brief Dynamic loop id: for accessing dynamic loop structures */
    uint32_t        id;
    /** \brief Static loop id in the static analyser, reference only */
    uint32_t        staticID;
    /** \brief Loop Parallelisation Schedule Policy */
    SchedulePolicy  schedule;
    /** \brief Rule instruction offset relative to the start of file */
    uint32_t        ruleInstOffset;
    uint32_t        ruleInstSize;
    /** \brief Rule data offset relative to the start of file */
    uint32_t        ruleDataOffset;
    uint32_t        ruleDataSize;
    /** \brief PC address of the loop start block
     *
     *  After loop initialisation, the main thread will jump to this address */
    uint64_t        loopStartAddr;
    /** \brief The set of registers that need to copy to other thread prior to parallelisation */
    uint64_t        registerToCopy;
    /** \brief The set of registers that need to merge from all threads to the main thread after parallelisation */
    uint64_t        registerToMerge;
    /** \brief Scratch register 0 (dr_reg_id_t) 
     *
     * s0 is stolen to hold the shared stack pointer value. If the loop does not use stack, it is free. */
    uint8_t         scratchReg0;
    /** \brief Scratch register 1 (dr_reg_id_t)
     *
     * s1 is stolen to hold the pointer to the thread local storage. */
    uint8_t         scratchReg1;
    /** \brief Scratch register 2 (dr_reg_id_t)
     *
     * s2 register is not usable for non-speculative loops during loop execution (additional spill required) */
    uint8_t         scratchReg2;
    /** \brief Scratch register 3 (dr_reg_id_t) 
     *
     * s3 register is not usable for non-speculative loops during loop execution (additional spill required)*/
    uint8_t         scratchReg3;
    /** \brief true if the loop have stack accesses */
    uint32_t        useStack;
    /** \brief The size of stack frame that each thread needs to allocate before jump to the loop */
    uint32_t        stackFrameSize;

    /** \brief Need to save EFLAG registers */
    uint8_t         saveflag;
    /** \brief If set true, it means this loop is an inner loop, please use lightweight threading support */
    uint32_t        isInnerLoop;
    /** \brief original SIMD word size */
    uint32_t        vectorWordSize;
    /* The following is required by dynamic code generation */
    uint32_t        validateGPRMask;
    uint32_t        validateSIMDMask;
    uint32_t        reductionMask;
    uint32_t        numSyncChannel;
    
    uint32_t        loop_finish_code;
    uint32_t        pre_allocate;
    uint32_t        hasLockArray;
} RSLoopHeader;

#endif
