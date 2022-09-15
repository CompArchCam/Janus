
include_directories(../include)

#use c20 features
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -g -fsanitize=undefined")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

## a modified castone disassembler engine
add_definitions(-DCAPSTONE_USE_SYS_DYN_MEM)
add_definitions(-D_STATIC_ANALYSIS_COMPILED)

## ripped from capstone project
set(CS_ENGINE
    capstone/cs.c
    capstone/MCInst.c
    capstone/MCInstrDesc.c
    capstone/MCRegisterInfo.c
    capstone/SStream.c
    capstone/utils.c
)

set(CS_INT_INCLUDE
    capstone/cs_priv.h
    capstone/LEB128.h
    capstone/MathExtras.h
    capstone/MCDisassembler.h
    capstone/MCFixedLenDisassembler.h
    capstone/MCInst.h
    capstone/MCInstrDesc.h
    capstone/MCRegisterInfo.h
    capstone/myinttypes.h
    capstone/osxkernel_inttypes.h
    capstone/SStream.h
    capstone/utils.h
)

set(CS_INCLUDE
    capstone/include/capstone/arm64.h
    capstone/include/capstone/arm.h
    capstone/include/capstone/capstone.h
    capstone/include/capstone/mips.h
    capstone/include/capstone/ppc.h
    capstone/include/capstone/x86.h
    capstone/include/capstone/sparc.h
    capstone/include/capstone/systemz.h
    capstone/include/capstone/xcore.h
    capstone/include/capstone/m68k.h
    capstone/include/capstone/platform.h
 )

##current build x64, ARM, ARM64 disassembler
## architecture support
if (JANUS_ARM_SUPPORT)
    set(CS_SOURCES_ARM
        capstone/arch/ARM/ARMDisassembler.c
        capstone/arch/ARM/ARMInstPrinter.c
        capstone/arch/ARM/ARMMapping.c
        capstone/arch/ARM/ARMModule.c
        )
    set(CS_HEADERS_ARM
        capstone/arch/ARM/ARMAddressingModes.h
        capstone/arch/ARM/ARMBaseInfo.h
        capstone/arch/ARM/ARMDisassembler.h
        capstone/arch/ARM/ARMGenAsmWriter.inc
        capstone/arch/ARM/ARMGenDisassemblerTables.inc
        capstone/arch/ARM/ARMGenInstrInfo.inc
        capstone/arch/ARM/ARMGenRegisterInfo.inc
        capstone/arch/ARM/ARMGenSubtargetInfo.inc
        capstone/arch/ARM/ARMInstPrinter.h
        capstone/arch/ARM/ARMMapping.h
        capstone/arch/ARM/ARMMappingInsn.inc
        capstone/arch/ARM/ARMMappingInsnOp.inc
        )
endif ()

if (JANUS_ARM64_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_ARM64)
    set(CS_SOURCES_ARM64
        capstone/arch/AArch64/AArch64BaseInfo.c
        capstone/arch/AArch64/AArch64Disassembler.c
        capstone/arch/AArch64/AArch64InstPrinter.c
        capstone/arch/AArch64/AArch64Mapping.c
        capstone/arch/AArch64/AArch64Module.c
        )
    set(CS_HEADERS_ARM64
        capstone/arch/AArch64/AArch64AddressingModes.h
        capstone/arch/AArch64/AArch64BaseInfo.h
        capstone/arch/AArch64/AArch64Disassembler.h
        capstone/arch/AArch64/AArch64GenAsmWriter.inc
        capstone/arch/AArch64/AArch64GenDisassemblerTables.inc
        capstone/arch/AArch64/AArch64GenInstrInfo.inc
        capstone/arch/AArch64/AArch64GenRegisterInfo.inc
        capstone/arch/AArch64/AArch64GenSubtargetInfo.inc
        capstone/arch/AArch64/AArch64InstPrinter.h
        capstone/arch/AArch64/AArch64Mapping.h
        capstone/arch/AArch64/AArch64MappingInsn.inc
        )
    set(JANUS_ARCH_SOURCES
        arch/aarch64/Operand.cpp
        arch/aarch64/MachineInstruction.cpp
        arch/aarch64/Disassemble.cpp
        arch/aarch64/Arch.cpp
    )
    set (JANUS_ARCH_INCLUDE
        arch/aarch64/include
    )
endif ()

if (JANUS_X86_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_X86)
    set(CS_SOURCES_X86
        capstone/arch/X86/X86Disassembler.c
        capstone/arch/X86/X86DisassemblerDecoder.c
        capstone/arch/X86/X86IntelInstPrinter.c
        capstone/arch/X86/X86Mapping.c
        capstone/arch/X86/X86Module.c
        capstone/arch/X86/X86ATTInstPrinter.c
        )
    set(CS_HEADERS_X86
        capstone/arch/X86/X86BaseInfo.h
        capstone/arch/X86/X86Disassembler.h
        capstone/arch/X86/X86DisassemblerDecoder.h
        capstone/arch/X86/X86DisassemblerDecoderCommon.h
        capstone/arch/X86/X86GenAsmWriter.inc
        capstone/arch/X86/X86GenAsmWriter1.inc
        capstone/arch/X86/X86GenAsmWriter1_reduce.inc
        capstone/arch/X86/X86GenAsmWriter_reduce.inc
        capstone/arch/X86/X86GenDisassemblerTables.inc
        capstone/arch/X86/X86GenDisassemblerTables_reduce.inc
        capstone/arch/X86/X86GenInstrInfo.inc
        capstone/arch/X86/X86GenInstrInfo_reduce.inc
        capstone/arch/X86/X86GenRegisterInfo.inc
        capstone/arch/X86/X86InstPrinter.h
        capstone/arch/X86/X86Mapping.h
        capstone/arch/X86/X86MappingInsn.inc
        capstone/arch/X86/X86MappingInsnOp.inc
        capstone/arch/X86/X86MappingInsnOp_reduce.inc
        capstone/arch/X86/X86MappingInsn_reduce.inc
        )
	set(JANUS_ARCH_SOURCES
		arch/x86/Operand.cpp
		arch/x86/MachineInstruction.cpp
        arch/x86/Disassemble.cpp
        arch/x86/Arch.cpp
	)
    set (JANUS_ARCH_INCLUDE
        arch/x86/include
    )
endif ()

set(CS_SOURCES
    ${CS_ENGINE}
    ${CS_SOURCES_ARM}
    ${CS_SOURCES_ARM64}
    ${CS_SOURCES_X86}
)

set(CS_HEADERS
    ${CS_INCLUDE}
    ${CS_INT_INCLUDE}
    ${CS_HEADERS_X86}
    ${CS_HEADERS_ARM}
    ${CS_HEADERS_ARM64}
)

set(CORE_SOURCES
    core/JanusContext.cpp
    core/Utility.cpp
    core/Function.cpp
    core/Loop.cpp
    core/BasicBlock.cpp
    core/Variable.cpp
    core/Instruction.cpp
    core/Expression.cpp
    core/Slice.cpp
    core/MemoryLocation.cpp
)

set(ANALYSIS_SOURCES
    analysis/ControlFlow.cpp
    analysis/IO.cpp
    analysis/Analysis.cpp
    analysis/Profile.cpp
    analysis/Affine.cpp
    analysis/AST.cpp
    analysis/Dependence.cpp
    analysis/Iterator.cpp
    analysis/Alias.cpp
)

set(LOADER_SOURCES
    loader/Executable.cpp
    loader/ELF.cpp
)

set(SCHEDGEN_SOURCES
    schedgen/SchedGen.cpp
    schedgen/parallel/LoopSelect.cpp
    schedgen/parallel/ParaRule.cpp
    #vectorule
    schedgen/vector/VectRule.cpp
    schedgen/vector/VectLoopSelect.cpp
    schedgen/plan/PlanRule.cpp
    schedgen/coverage/CoverageRule.cpp
    #prefetch
    schedgen/prefetch/PrefetchRule.cpp
    #cinnamon dsl extension
    schedgen/dsl/DSLGen.cpp
    schedgen/dsl/StaticGenCode.cpp
    #dll -
    schedgen/dll/dllRule.cpp
    #schedgen/vector/FindInduction.cpp
    #schedgen/vector/MemoryCheck.cpp
    #schedgen/vector/FindReduction.cpp
    #schedgen/vector/Extend.cpp
    #schedgen/vector/Broadcast.cpp
    #schedgen/vector/VectUtils.cpp
    #schedgen/timer/TimerRule.cpp
)

set(SCHEDGEN_INCLUDE
    schedgen/
    schedgen/parallel/
    schedgen/coverage/
    schedgen/vector/
    schedgen/prefetch/
    schedgen/plan/
    schedgen/security
    schedgen/dsl/
    schedgen/dll/
)

set(SCHEDUMP_SOURCES
    tools/schedump/schedump.cpp
    ${REWRITE_RULE_SOURCES}
    ${REWRITE_RULE_ARCH_SOURCES}
)

set(INCLUDE_DIR
    capstone/
    capstone/include/
    loader/include/
    loader/format/
    core/include/
    translate/include/
    analysis/include/
    ${SCHEDGEN_INCLUDE}
    ${JANUS_ARCH_INCLUDE}
	${REWRITE_RULE_INCLUDE}
    ${REWRITE_RULE_ARCH_INCLUDE}
)

include_directories(${INCLUDE_DIR})

add_executable(analyze main.cpp
                       ${LOADER_SOURCES}
                       ${CS_SOURCES}
                       ${CORE_SOURCES}
                       ${JANUS_ARCH_SOURCES}
                       ${ANALYSIS_SOURCES}
                       ${SCHEDGEN_SOURCES}
                       ${REWRITE_RULE_SOURCES}
                       ${REWRITE_RULE_SOURCES_CPP}
                       ${REWRITE_RULE_ARCH_SOURCES})

# compile rule dump tool
add_executable(schedump ${SCHEDUMP_SOURCES})
