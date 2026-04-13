#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <string.h>
#include <math.h>

#include "vars.h"

#define VAR_FLAG_USED 0x01
#define VAR_FLAG_SYS 0x02
#define VAR_FLAG_VALID 0x04
#define VAR_ANS_INDEX 0

#define ERR_SEND(dst, lit) do {                         \
    _Static_assert(sizeof(lit) <= CALC_ERR_MSG_LEN + 1, \
                "error message too long");              \
    strcpy(dst, lit);                                   \
} while(0)

// VARIABLE TYPES:

typedef struct {
    double value;
    char name[VAR_NAME_MAX+1];
    uint8_t flags;
} VarEntry;

typedef struct {
    VarEntry entries[VAR_MAX];
    uint32_t count;
} VarTable;

// FUNCTION TYPES:

typedef struct {
    FuncEntry entries[FUNC_MAX];
    uint32_t count;
} FuncTable;

typedef struct {
    VarTable vt;
    FuncTable ft;
    char func_err_msg[CALC_ERR_MSG_LEN+1];
} Vars_Memory;


static Vars_Memory *vmem;
static const double fact_table[];

static const VarEntry sys_vars[] = {
    {.name = "ans", .value = 0.0, .flags = VAR_FLAG_SYS | VAR_FLAG_USED | VAR_FLAG_VALID},
    {.name = "pi", .value = M_PI, .flags = VAR_FLAG_SYS | VAR_FLAG_USED | VAR_FLAG_VALID},
    {.name = "e", .value = M_E, .flags = VAR_FLAG_SYS | VAR_FLAG_USED | VAR_FLAG_VALID},
};

static bool double_to_int64(double val, int64_t *out);
static double ln_safe(double val);
static double sqrt_safe(double val);
static double acos_safe(double val);
static double asin_safe(double val);
static double log10_safe(double val);
static double log2_safe(double val);
static double fact_safe(double val);
static double gcd(double a, double b);
static double lcm(double a, double b);

// Warning: if you change the placement or number of trig or inv trig functions
//  you MUST change the #define's in vars.h!
static const FuncEntry sys_funcs[] = {
    {.name = "sin", .arg_count = 1, .func.unary = sin},
    {.name = "cos", .arg_count = 1, .func.unary = cos},
    {.name = "tan", .arg_count = 1, .func.unary = tan},
    {.name = "asin", .arg_count = 1, .func.unary = asin_safe},
    {.name = "acos", .arg_count = 1, .func.unary = acos_safe},
    {.name = "atan", .arg_count = 1, .func.unary = atan},
    {.name = "atan2", .arg_count = 2, .func.binary = atan2},
    {.name = "ln", .arg_count = 1, .func.unary = ln_safe},
    {.name = "log10", .arg_count = 1, .func.unary = log10_safe},
    {.name = "log2", .arg_count = 1, .func.unary = log2_safe},
    {.name = "sqrt", .arg_count = 1, .func.unary = sqrt_safe},
    {.name = "round", .arg_count = 1, .func.unary = round},
    {.name = "exp", .arg_count = 1, .func.unary = exp},
    {.name = "abs", .arg_count = 1, .func.unary = fabs},
    {.name = "fact", .arg_count = 1, .func.unary = fact_safe},
    {.name = "tgamma", .arg_count = 1, .func.unary = tgamma},
    {.name = "gcd", .arg_count = 2, .func.binary = gcd},
    {.name = "lcm", .arg_count = 2, .func.binary = lcm},



};


size_t vars_mem_size(void) {
    return sizeof(Vars_Memory);
}

void vars_init(void *mem) {
    vmem = (Vars_Memory *)mem;
    memset(vmem, 0x00, sizeof(Vars_Memory));

    // fill in vars and func tables
    memcpy(&vmem->vt.entries, sys_vars, sizeof(sys_vars));
    vmem->vt.count = sizeof(sys_vars) / sizeof(sys_vars[0]);

    // fill in func tables
    memcpy(&vmem->ft.entries, sys_funcs, sizeof(sys_funcs));
    vmem->ft.count = sizeof(sys_funcs) / sizeof(sys_funcs[0]);

}

// returns false if variable does not exist or if cell tries to use an invalid ans variable
//      this is kind of common and is fine, user should be notified
bool vars_get(const char *name, double *value) {
    // uint8_t var_flag = VAR_FLAG_USED;
    uint8_t var_idx = 0;

    while (var_idx < vmem->vt.count) {
        if (strncmp(name, vmem->vt.entries[var_idx].name, VAR_NAME_MAX) == 0) {
            if (vmem->vt.entries[var_idx].flags | VAR_FLAG_VALID) {
                *value = vmem->vt.entries[var_idx].value;
                return true;
            } else { // variable is invalid. mostly reserved for "ans" usage
                return false;
            } 
        }
        var_idx += 1;
    }

    return false;
}

// can set non-system variables
bool vars_set(const char *name, double value) {
    uint8_t var_idx = 0;

    while (var_idx < vmem->vt.count) {
        if (strncmp(name, vmem->vt.entries[var_idx].name, VAR_NAME_MAX) == 0) {

            // if the variable is a system variable (pi, ans, etc)
            if (vmem->vt.entries[var_idx].flags & VAR_FLAG_SYS) return false;
            
            vmem->vt.entries[var_idx].value = value;
            return true;
        }
        var_idx += 1;
    }

    strncpy(vmem->vt.entries[var_idx].name, name, VAR_NAME_MAX);
    vmem->vt.entries[var_idx].value = value;
    vmem->vt.entries[var_idx].flags = VAR_FLAG_USED | VAR_FLAG_VALID;

    vmem->vt.count += 1;
    return true;
}

void var_set_ans(double value) {
    vmem->vt.entries[VAR_ANS_INDEX].value = value;
}

// because ans is cascading, the next cell needs to know if the prev ans was valid
void var_set_ans_valid(bool valid) {
    if (valid) {
        vmem->vt.entries[VAR_ANS_INDEX].flags |= VAR_FLAG_VALID;
    } else {
        vmem->vt.entries[VAR_ANS_INDEX].flags &= ~VAR_FLAG_VALID;
    }
}

// NOT USED
// because ans is cascading, the next cell needs to know if the prev ans was valid
// bool var_is_ans_valid(void) {
//     return vmem->vt.entries[VAR_ANS_INDEX].flags & VAR_FLAG_VALID;
// }

bool vars_clear_user(void) {
    for (uint8_t i = 0; i < vmem->vt.count; i++) {

        if (!(vmem->vt.entries[i].flags & VAR_FLAG_SYS)) {
            memset(&vmem->vt.entries[i], 0x00, sizeof(VarEntry));
        }
    }

    return true;
}

// name must be string!
bool funcs_get_idx(const char *name, uint8_t *idx) {
    uint8_t func_idx = 0;

    while (func_idx < vmem->ft.count) {
        if (strncmp(name, vmem->ft.entries[func_idx].name, VAR_NAME_MAX) == 0) {
            *idx = func_idx;
            return true;
        }
        func_idx += 1;
    }
    return false;
}

// caller must verify this didn't return NULL
FuncEntry* funcs_get_func(uint8_t index) {
    if (index >= vmem->ft.count) return NULL;

    return &vmem->ft.entries[index];
}

// empties func error message before using functions
const char *funcs_get_error(void) {
    return vmem->func_err_msg[0] ? vmem->func_err_msg : NULL;
}

void funcs_clear_error(void) {
    memset(vmem->func_err_msg, 0x00, CALC_ERR_MSG_LEN+1);
}

// CUSTOM FUNCTIONS for safety
static double ln_safe(double val) {
    if (val <= 0.0) {
        ERR_SEND(vmem->func_err_msg, "LN DOMAIN ERROR");
        return NAN;
    }
    return log(val);
}

static double sqrt_safe(double val) {
    if (val < 0.0) {
        ERR_SEND(vmem->func_err_msg, "SQRT DOMAIN ERROR");
        return NAN;
    }
    return sqrt(val);
}

static double acos_safe(double val) {
    if (val < -1.0 || val > 1.0) {
        ERR_SEND(vmem->func_err_msg, "ACOS DOMAIN ERROR");
        return NAN;
    }
    return acos(val);
}

static double asin_safe(double val) {
    if (val < -1.0 || val > 1.0) {
        ERR_SEND(vmem->func_err_msg, "ASIN DOMAIN ERROR");
        return NAN;
    }
    return asin(val);
}

static double log10_safe(double val) {
    if (val <= 0.0) {
        ERR_SEND(vmem->func_err_msg, "LOG10 DOMAIN ERROR");
        return NAN;
    }
    return log10(val);
}

static double log2_safe(double val) {
    if (val <= 0.0) {
        ERR_SEND(vmem->func_err_msg, "LOG2 DOMAIN ERROR");
        return NAN;
    }
    return log2(val);
}

static bool double_to_int64(double val, int64_t *out) {
    double mag = fabs(val);

    double epsilon = (mag > 1.0 ? mag : 1.0) * 1e-9;
    // not an integer:
    if (fabs(val - round(val)) > epsilon) return false;

    // too big to put in int64
    if (mag > (double)(1LL << 53)) return false;

    *out = (int64_t)round(val);
    return true;
}



#define FACT_TABLE_SIZE 170 // see fact_table[] below

static double fact_safe(double val) {
    int64_t n;
    if (!double_to_int64(val, &n) || n < 0) {
        ERR_SEND(vmem->func_err_msg, "NON INTEGER ERROR");
        return NAN;
    }

    if (n > FACT_TABLE_SIZE) return (double)INFINITY;

    return fact_table[n];
}

static double gcd(double a, double b) {
    int64_t x, y;

    if (!double_to_int64(a, &x) || !double_to_int64(b, &y)) {
        ERR_SEND(vmem->func_err_msg, "NON INTEGER ERROR");
        return NAN;
    }

    x = llabs(x);
    y = llabs(y);

    while (y) {
        int64_t t = y;
        y = x % y;
        x = t;
    }
    return (double)x;    
}

static double lcm(double a, double b) {
    int64_t x, y;

    if (!double_to_int64(a, &x) || !double_to_int64(b, &y)) {
        ERR_SEND(vmem->func_err_msg, "NON INTEGER ERROR");
        return NAN;
    }

    x = llabs(x);
    y = llabs(y);
    int64_t abs_a = x;
    int64_t abs_b = y;

    while (y) {
        int64_t t = y;
        y = x % y;
        x = t;
    }
    
    return (double)(abs_a / x) * (double)abs_b;
}

static const double fact_table[] = {
    1.0, // 0!
    1.0, // 1!
    2.0, // 2!
    6.0, // 3!
    24.0, // 4!
    120.0, // 5!
    720.0, // 6!
    5040.0, // 7!
    40320.0, // 8!
    362880.0, // 9!
    3628800.0, // 10!
    39916800.0, // 11!
    479001600.0, // 12!
    6227020800.0, // 13!
    87178291200.0, // 14!
    1307674368000.0, // 15!
    20922789888000.0, // 16!
    355687428096000.0, // 17!
    6402373705728000.0, // 18!
    1.21645100408832e+17, // 19!
    2.43290200817664e+18, // 20!
    5.109094217170944e+19, // 21!
    1.1240007277776077e+21, // 22!
    2.585201673888498e+22, // 23!
    6.204484017332394e+23, // 24!
    1.5511210043330986e+25, // 25!
    4.0329146112660565e+26, // 26!
    1.0888869450418352e+28, // 27!
    3.0488834461171387e+29, // 28!
    8.841761993739702e+30, // 29!
    2.6525285981219107e+32, // 30!
    8.222838654177922e+33, // 31!
    2.631308369336935e+35, // 32!
    8.683317618811886e+36, // 33!
    2.9523279903960416e+38, // 34!
    1.0333147966386145e+40, // 35!
    3.7199332678990125e+41, // 36!
    1.3763753091226346e+43, // 37!
    5.230226174666011e+44, // 38!
    2.0397882081197444e+46, // 39!
    8.159152832478977e+47, // 40!
    3.345252661316381e+49, // 41!
    1.40500611775288e+51, // 42!
    6.041526306337383e+52, // 43!
    2.658271574788449e+54, // 44!
    1.1962222086548019e+56, // 45!
    5.502622159812089e+57, // 46!
    2.5862324151116818e+59, // 47!
    1.2413915592536073e+61, // 48!
    6.082818640342675e+62, // 49!
    3.0414093201713376e+64, // 50!
    1.5511187532873822e+66, // 51!
    8.065817517094388e+67, // 52!
    4.2748832840600255e+69, // 53!
    2.308436973392414e+71, // 54!
    1.2696403353658276e+73, // 55!
    7.109985878048635e+74, // 56!
    4.0526919504877214e+76, // 57!
    2.3505613312828785e+78, // 58!
    1.3868311854568984e+80, // 59!
    8.32098711274139e+81, // 60!
    5.075802138772248e+83, // 61!
    3.146997326038794e+85, // 62!
    1.98260831540444e+87, // 63!
    1.2688693218588417e+89, // 64!
    8.247650592082472e+90, // 65!
    5.443449390774431e+92, // 66!
    3.647111091818868e+94, // 67!
    2.4800355424368305e+96, // 68!
    1.711224524281413e+98, // 69!
    1.1978571669969892e+100, // 70!
    8.504785885678623e+101, // 71!
    6.1234458376886085e+103, // 72!
    4.4701154615126844e+105, // 73!
    3.307885441519386e+107, // 74!
    2.48091408113954e+109, // 75!
    1.8854947016660504e+111, // 76!
    1.4518309202828587e+113, // 77!
    1.1324281178206297e+115, // 78!
    8.946182130782976e+116, // 79!
    7.156945704626381e+118, // 80!
    5.797126020747368e+120, // 81!
    4.753643337012842e+122, // 82!
    3.945523969720659e+124, // 83!
    3.314240134565353e+126, // 84!
    2.81710411438055e+128, // 85!
    2.4227095383672734e+130, // 86!
    2.107757298379528e+132, // 87!
    1.8548264225739844e+134, // 88!
    1.650795516090846e+136, // 89!
    1.4857159644817615e+138, // 90!
    1.352001527678403e+140, // 91!
    1.2438414054641308e+142, // 92!
    1.1567725070816416e+144, // 93!
    1.087366156656743e+146, // 94!
    1.032997848823906e+148, // 95!
    9.916779348709496e+149, // 96!
    9.619275968248212e+151, // 97!
    9.426890448883248e+153, // 98!
    9.332621544394415e+155, // 99!
    9.332621544394415e+157, // 100!
    9.42594775983836e+159, // 101!
    9.614466715035127e+161, // 102!
    9.90290071648618e+163, // 103!
    1.0299016745145628e+166, // 104!
    1.081396758240291e+168, // 105!
    1.1462805637347084e+170, // 106!
    1.226520203196138e+172, // 107!
    1.324641819451829e+174, // 108!
    1.4438595832024937e+176, // 109!
    1.588245541522743e+178, // 110!
    1.7629525510902446e+180, // 111!
    1.974506857221074e+182, // 112!
    2.2311927486598138e+184, // 113!
    2.5435597334721877e+186, // 114!
    2.925093693493016e+188, // 115!
    3.393108684451898e+190, // 116!
    3.969937160808721e+192, // 117!
    4.684525849754291e+194, // 118!
    5.574585761207606e+196, // 119!
    6.689502913449127e+198, // 120!
    8.094298525273444e+200, // 121!
    9.875044200833601e+202, // 122!
    1.214630436702533e+205, // 123!
    1.506141741511141e+207, // 124!
    1.882677176888926e+209, // 125!
    2.372173242880047e+211, // 126!
    3.0126600184576594e+213, // 127!
    3.856204823625804e+215, // 128!
    4.974504222477287e+217, // 129!
    6.466855489220474e+219, // 130!
    8.47158069087882e+221, // 131!
    1.1182486511960043e+224, // 132!
    1.4872707060906857e+226, // 133!
    1.9929427461615188e+228, // 134!
    2.6904727073180504e+230, // 135!
    3.659042881952549e+232, // 136!
    5.012888748274992e+234, // 137!
    6.917786472619489e+236, // 138!
    9.615723196941089e+238, // 139!
    1.3462012475717526e+241, // 140!
    1.898143759076171e+243, // 141!
    2.695364137888163e+245, // 142!
    3.854370717180073e+247, // 143!
    5.5502938327393044e+249, // 144!
    8.047926057471992e+251, // 145!
    1.1749972043909107e+254, // 146!
    1.727245890454639e+256, // 147!
    2.5563239178728654e+258, // 148!
    3.80892263763057e+260, // 149!
    5.713383956445855e+262, // 150!
    8.62720977423324e+264, // 151!
    1.3113358856834524e+267, // 152!
    2.0063439050956823e+269, // 153!
    3.0897696138473508e+271, // 154!
    4.789142901463394e+273, // 155!
    7.471062926282894e+275, // 156!
    1.1729568794264145e+278, // 157!
    1.853271869493735e+280, // 158!
    2.9467022724950384e+282, // 159!
    4.7147236359920616e+284, // 160!
    7.590705053947219e+286, // 161!
    1.2296942187394494e+289, // 162!
    2.0044015765453026e+291, // 163!
    3.287218585534296e+293, // 164!
    5.423910666131589e+295, // 165!
    9.003691705778438e+297, // 166!
    1.503616514864999e+300, // 167!
    2.5260757449731984e+302, // 168!
    4.269068009004705e+304, // 169!
    7.257415615307999e+306, // 170!
};