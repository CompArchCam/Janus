#include <stdio.h>
#include "janus_var.h"
#include "janus_arch.h"

void print_var(JVar var)
{
    switch (var.type) {
        case JVAR_REGISTER:
            printf("%s",get_reg_name((int)var.value));
            break;
        case JVAR_STACK: {
            printf("[RSP");
            int value = (int)var.value;
            if (value > 0) 
                printf("+0x%x]",(uint32_t)value);
            else if (value==0) printf("]");
            else printf("-0x%x]",(uint32_t)-value);
            break;
        }
        case JVAR_MEMORY:
        case JVAR_STACKFRAME:
        case JVAR_POLYNOMIAL: {
            printf("[");
            int base = (int)var.base;
            int index = (int)var.index;
            int value = (int)var.value;
            int scale = (int)var.scale;
            printf("%s",get_reg_name(base));
            if (index && base) printf("+");
            if (index) {
                printf("%s",get_reg_name(index));
                if (scale > 1) printf("*%d",scale);
            }
            if (value > 0) 
                printf("+0x%x]",value);
            else if (value==0) printf("]");
            else printf("-0x%x]",-value);
            break;
        }
        case JVAR_ABSOLUTE: {
            printf("[");
            int64_t value = (int)var.value;
            if (value > 0) 
                printf("+0x%lx]",(uint64_t)value);
            else if (value==0) printf("]");
            else printf("-0x%lx]",(uint64_t)-value);
            break;
        }
        case JVAR_CONSTANT: {
            int64_t value = (int)var.value;
            if (value>0) printf("0x%lx",(uint64_t)value);
            else if (value<0) printf("-0x%lx",(uint64_t)-value);
            else printf("0");
            break;
        }
        case JVAR_CONTROLFLAG:
            printf("CtrlFlag_%d",(int)var.value);
            break;
#ifdef JANUS_AARCH64
        case JVAR_SHIFTEDCONST: {
            int64_t value = (int)var.value;
            if (value>0) printf("0x%lx",(uint64_t)value);
            else if (value<0) printf("-0x%lx",(uint64_t)-value);
            else printf("0");
            printf(" %s #%d", get_shift_name(var.shift_type), var.shift_value);
            break;
        }
        case JVAR_SHIFTEDREG:
            printf("%s",get_reg_name((int)var.value));
            printf(" %s #%d", get_shift_name(var.shift_type), var.shift_value);
            break;
#endif
        case JVAR_UNKOWN:
        default:
            printf("UndefinedVar");
        break;
    }
    if(var.shift_type != JSHFT_INVALID)
    {
    }
}

void print_profile(JVarProfile *profile)
{
    if (profile->type == INDUCTION_PROFILE) {
        printf("Induction ");
        print_var(profile->var);
        if (profile->induction.op == UPDATE_ADD) printf(" add");
        else printf(" unknown");
        printf(" ");
        print_var(profile->induction.stride);
        if (profile->induction.check.type == JVAR_UNKOWN)
            printf(" no check");
        else {
            printf(" check ");
            print_var(profile->induction.check);
        }
        if (profile->induction.init.type == JVAR_UNKOWN)
            printf(" no init");
        else {
            printf(" init ");
            print_var(profile->induction.init);
        }
        printf("\n");
    } else if (profile->type == COPY_PROFILE) {
        printf("ThreadCopy ");
        print_var(profile->var);
        printf("\n");
    } else if (profile->type == ARRAY_PROFILE) {
        printf("Array base ");
        print_var(profile->array.base);
        printf(" range ");
        print_var(profile->array.max_range);
        printf("\n");
    }
}
