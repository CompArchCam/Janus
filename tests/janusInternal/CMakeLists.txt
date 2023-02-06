set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

include_directories(../include)

#use c11 features
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

## a modified castone disassembler engine
add_definitions(-DCAPSTONE_USE_SYS_DYN_MEM)
add_definitions(-D_STATIC_ANALYSIS_COMPILED)

set (MIN_UNIT
        minunit/
)

## ripped from capstone project
set(CS_ENGINE
    ${PROJECT_SOURCE_DIR}/static/capstone/cs.c
    ${PROJECT_SOURCE_DIR}/static/capstone/MCInst.c
    ${PROJECT_SOURCE_DIR}/static/capstone/MCInstrDesc.c
    ${PROJECT_SOURCE_DIR}/static/capstone/MCRegisterInfo.c
    ${PROJECT_SOURCE_DIR}/static/capstone/SStream.c
    ${PROJECT_SOURCE_DIR}/static/capstone/utils.c
)

set(CS_INT_INCLUDE
    ${PROJECT_SOURCE_DIR}/static/capstone/cs_priv.h
    ${PROJECT_SOURCE_DIR}/static/capstone/LEB128.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MathExtras.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MCDisassembler.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MCFixedLenDisassembler.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MCInst.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MCInstrDesc.h
    ${PROJECT_SOURCE_DIR}/static/capstone/MCRegisterInfo.h
    ${PROJECT_SOURCE_DIR}/static/capstone/myinttypes.h
    ${PROJECT_SOURCE_DIR}/static/capstone/osxkernel_inttypes.h
    ${PROJECT_SOURCE_DIR}/static/capstone/SStream.h
    ${PROJECT_SOURCE_DIR}/static/capstone/utils.h
)

set(CS_INCLUDE
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/arm64.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/arm.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/capstone.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/mips.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/ppc.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/x86.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/sparc.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/systemz.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/xcore.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/m68k.h
    ${PROJECT_SOURCE_DIR}/static/capstone/include/capstone/platform.h
 )

##current build x64, ARM, ARM64 disassembler
## architecture support
if (JANUS_ARM_SUPPORT)
    set(CS_SOURCES_ARM
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMDisassembler.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMInstPrinter.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMMapping.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMModule.c
        )
    set(CS_HEADERS_ARM
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMAddressingModes.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMBaseInfo.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMDisassembler.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMGenAsmWriter.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMGenDisassemblerTables.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMGenInstrInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMGenRegisterInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMGenSubtargetInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMInstPrinter.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMMapping.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMMappingInsn.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/ARM/ARMMappingInsnOp.inc
        )
endif ()

if (JANUS_ARM64_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_ARM64)
    set(CS_SOURCES_ARM64
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64BaseInfo.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64Disassembler.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64InstPrinter.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64Mapping.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64Module.c
        )
    set(CS_HEADERS_ARM64
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64AddressingModes.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64BaseInfo.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64Disassembler.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64GenAsmWriter.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64GenDisassemblerTables.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64GenInstrInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64GenRegisterInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64GenSubtargetInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64InstPrinter.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64Mapping.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/AArch64/AArch64MappingInsn.inc
        )
    set(JANUS_ARCH_SOURCES
        ${PROJECT_SOURCE_DIR}/static/arch/aarch64/Operand.cpp
        ${PROJECT_SOURCE_DIR}/static/arch/aarch64/MachineInstruction.cpp
        ${PROJECT_SOURCE_DIR}/static/arch/aarch64/Disassemble.cpp
        ${PROJECT_SOURCE_DIR}/static/arch/aarch64/Arch.cpp
    )
    set (JANUS_ARCH_INCLUDE
        ${PROJECT_SOURCE_DIR}/static/arch/aarch64/include
    )
endif ()

if (JANUS_X86_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_X86)
    set(CS_SOURCES_X86
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86Disassembler.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86DisassemblerDecoder.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86IntelInstPrinter.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86Mapping.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86Module.c
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86ATTInstPrinter.c
        )
    set(CS_HEADERS_X86
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86BaseInfo.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86Disassembler.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86DisassemblerDecoder.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86DisassemblerDecoderCommon.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenAsmWriter.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenAsmWriter1.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenAsmWriter1_reduce.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenAsmWriter_reduce.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenDisassemblerTables.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenDisassemblerTables_reduce.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenInstrInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenInstrInfo_reduce.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86GenRegisterInfo.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86InstPrinter.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86Mapping.h
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86MappingInsn.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86MappingInsnOp.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86MappingInsnOp_reduce.inc
        ${PROJECT_SOURCE_DIR}/static/capstone/arch/X86/X86MappingInsn_reduce.inc
        )
	set(JANUS_ARCH_SOURCES
		${PROJECT_SOURCE_DIR}/static/arch/x86/Operand.cpp
		${PROJECT_SOURCE_DIR}/static/arch/x86/MachineInstruction.cpp
        ${PROJECT_SOURCE_DIR}/static/arch/x86/Disassemble.cpp
        ${PROJECT_SOURCE_DIR}/static/arch/x86/Arch.cpp
	)
    set (JANUS_ARCH_INCLUDE
        ${PROJECT_SOURCE_DIR}/static/arch/x86/include
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
    ${PROJECT_SOURCE_DIR}/static/core/JanusContext.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Utility.cpp
    # Added by jahic
    ${PROJECT_SOURCE_DIR}/static/core/ProgramDependenceAnalysis.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Function.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Loop.cpp
    ${PROJECT_SOURCE_DIR}/static/core/BasicBlock.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Variable.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Instruction.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Expression.cpp
    ${PROJECT_SOURCE_DIR}/static/core/Slice.cpp
    ${PROJECT_SOURCE_DIR}/static/core/MemoryLocation.cpp
	# Added by jahic
    ${PROJECT_SOURCE_DIR}/static/core/SourceCodeStructure.cpp
)

set(ANALYSIS_SOURCES
    ${PROJECT_SOURCE_DIR}/static/analysis/ControlFlow.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/IO.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Analysis.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Profile.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/SSA.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Affine.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/AST.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Dependence.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Iterator.cpp
    ${PROJECT_SOURCE_DIR}/static/analysis/Alias.cpp
    # Added by jahic
    ${PROJECT_SOURCE_DIR}/static/analysis/LoopAnalysisReport.cpp
)

set(LOADER_SOURCES
    #loader/Executable.cpp
    # Added by jahic
    ${PROJECT_SOURCE_DIR}/static/loader/ExecutableBinaryStructure.cpp
    ${PROJECT_SOURCE_DIR}/static/loader/ELF.cpp
)

set(SCHEDGEN_SOURCES
    ${PROJECT_SOURCE_DIR}/static/schedgen/SchedGen.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/parallel/LoopSelect.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/parallel/ParaRule.cpp
    #vectorule
    ${PROJECT_SOURCE_DIR}/static/schedgen/vector/VectRule.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/vector/VectLoopSelect.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/plan/PlanRule.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/coverage/CoverageRule.cpp
    #prefetch
    ${PROJECT_SOURCE_DIR}/static/schedgen/prefetch/PrefetchRule.cpp
    #cinnamon dsl extension
    ${PROJECT_SOURCE_DIR}/static/schedgen/dsl/DSLGen.cpp
    ${PROJECT_SOURCE_DIR}/static/schedgen/dsl/StaticGenCode.cpp
    #dll -
    ${PROJECT_SOURCE_DIR}/static/schedgen/dll/dllRule.cpp
    #schedgen/vector/FindInduction.cpp
    #schedgen/vector/MemoryCheck.cpp
    #schedgen/vector/FindReduction.cpp
    #schedgen/vector/Extend.cpp
    #schedgen/vector/Broadcast.cpp
    #schedgen/vector/VectUtils.cpp
    #schedgen/timer/TimerRule.cpp
)

set(SCHEDGEN_INCLUDE
    ${PROJECT_SOURCE_DIR}/static/schedgen/
    ${PROJECT_SOURCE_DIR}/static/schedgen/parallel/
    ${PROJECT_SOURCE_DIR}/static/schedgen/coverage/
    ${PROJECT_SOURCE_DIR}/static/schedgen/vector/
    ${PROJECT_SOURCE_DIR}/static/schedgen/prefetch/
    ${PROJECT_SOURCE_DIR}/static/schedgen/plan/
    ${PROJECT_SOURCE_DIR}/static/schedgen/security
    ${PROJECT_SOURCE_DIR}/static/schedgen/dsl/
    ${PROJECT_SOURCE_DIR}/static/schedgen/dll/
)

set(SCHEDUMP_SOURCES
    tools/schedump/schedump.cpp
    ${REWRITE_RULE_SOURCES}
    ${REWRITE_RULE_ARCH_SOURCES}
)

set(INCLUDE_DIR
    ${PROJECT_SOURCE_DIR}/static/capstone/
    ${PROJECT_SOURCE_DIR}/static/capstone/include/
    ${PROJECT_SOURCE_DIR}/static/loader/include/
    ${PROJECT_SOURCE_DIR}/static/loader/format/
    ${PROJECT_SOURCE_DIR}/static/core/include/
    ${PROJECT_SOURCE_DIR}/static/translate/include/
    ${PROJECT_SOURCE_DIR}/static/analysis/include/
    ${SCHEDGEN_INCLUDE}
    ${JANUS_ARCH_INCLUDE}
	${REWRITE_RULE_INCLUDE}
    ${REWRITE_RULE_ARCH_INCLUDE}
    ${MIN_UNIT}
)

include_directories(${INCLUDE_DIR})

add_executable(test_OpenAndParseBinary unitTests/OpenAndParseBinary.cpp
                       ${LOADER_SOURCES}
                       ${CS_SOURCES}
                       ${CORE_SOURCES}
                       ${JANUS_ARCH_SOURCES}
                       ${ANALYSIS_SOURCES}
                       ${SCHEDGEN_SOURCES}
                       ${REWRITE_RULE_SOURCES}
                       ${REWRITE_RULE_SOURCES_CPP}
                       ${REWRITE_RULE_ARCH_SOURCES}
                       ${MIN_UNIT}
                       )