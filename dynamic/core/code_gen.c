#include "code_gen.h"

//Convert Janus variable size to operand size
static opnd_size_t opsz(uint8_t s);

opnd_t create_opnd(const JVar v) {
    switch (v.type) {
        case JVAR_REGISTER:
            return opnd_create_reg(v.value);
        case JVAR_STACK:
            return opnd_create_base_disp(DR_REG_XSP,DR_REG_NULL,0,(int)v.value, opsz(v.size));
        case JVAR_STACKFRAME:
            return opnd_create_base_disp(v.base, DR_REG_NULL, 0, v.value, opsz(v.size));
        case JVAR_MEMORY:
            return opnd_create_base_disp(v.base, v.index, v.scale, v.value, opsz(v.size));
        case JVAR_ABSOLUTE:
            return opnd_create_rel_addr((void *)(v.value), opsz(v.size));
        case JVAR_CONSTANT:
            return opnd_create_immed_int((int32_t)v.value, OPSZ_4);
        case JVAR_POLYNOMIAL:
            return opnd_create_base_disp(v.base, v.index, v.scale, v.value, OPSZ_lea);
        default:
            return opnd_create_null();
    }
}

opnd_t create_shared_stack_opnd(const JVar v, reg_id_t shared_stk_ptr)
{
    switch (v.type) {
        case JVAR_STACKFRAME:
        case JVAR_STACK:
            return opnd_create_base_disp(shared_stk_ptr,DR_REG_NULL,0,(int)v.value, opsz(v.size));
        default:
            return opnd_create_null();
    }
}

static opnd_size_t opsz(uint8_t s) {
    switch (s) {
        case 0: return OPSZ_0;
        case 1: return OPSZ_1;
        case 2: return OPSZ_2;
        case 4: return OPSZ_4;
        case 8: return OPSZ_8;
        case 16: return OPSZ_16;
        default: return OPSZ_NA;
    }
}

reg_id_t
reg_with_same_width(reg_id_t reg, opnd_t opnd)
{
    reg_id_t result = reg;
    if(reg_get_size(reg) != opnd_get_size(opnd)) { 
        if (opnd_get_size(opnd) == OPSZ_4)
            result = reg_64_to_32(reg);
        else
            result = reg_32_to_64(reg);
    }

    return result;
}

app_pc
generate_runtime_code(void *drcontext, uint32_t max_size, instrlist_t *bb)
{
    byte *end;
    app_pc code_cache = dr_nonheap_alloc(max_size,
                                         DR_MEMPROT_READ  |
                                         DR_MEMPROT_WRITE |
                                         DR_MEMPROT_EXEC);

    /* Encodes the instructions into memory and then cleans up. */
    end = instrlist_encode(drcontext, bb, code_cache, true);
    DR_ASSERT((end - code_cache) < max_size);

    instrlist_clear_and_destroy(drcontext, bb);

    /* Set memory as just +rx. */
    dr_memory_protect(code_cache, max_size,
                    DR_MEMPROT_READ | DR_MEMPROT_EXEC);

    return code_cache;
}

JVar
reg_to_JVar(reg_id_t reg)
{
    JVar var;
    var.type = JVAR_REGISTER;
    var.value = reg;
    return var;
}
