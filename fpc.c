// Dustin DeWeese (deweese@nestlabs.com)
// 2016

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

typedef __int128_t int128_t;

struct parameters {
  /* these are the inputs */
  long double
    min,
    max,
    precision;

  /* these can use more than fixed_encoding_width since there can be a _really_ large offset */
  int128_t
    lower_bound,
    upper_bound,
    offset;

  int
    fractional_bits,
    integer_bits,
    fixed_encoding_width;

  bool
    use_signed,
    large_offset;

  const char *error;
};

#define ROUND(t, x, s) ((x) < 0 ? ((((t)(x)) - (((t)1) << (s-1))) >> s) : ((((t)(x)) + (((t)1) << (s-1))) >> s))

int ceil_log2l(long double x) {
  int exp;
  long double n = frexpl(x, &exp);
  return n == 0.5L ? exp - 1 : exp;
}

int floor_log2l(long double x) {
  int exp;
  frexpl(x, &exp);
  return exp - 1;
}

unsigned int int_log2(unsigned int x) {
  return x <= 1 ? 0 : (8 * sizeof(x)) - __builtin_clz(x - 1);
}

int clz128(int128_t x) {
  unsigned int y;
  unsigned int shift = sizeof(y) * 8;
  int i, n = sizeof(x) / sizeof(y);
  for(i = 0; i < n; i++) {
    y = x >> (shift * (n - 1 - i));
    if(y) return __builtin_clz(y) + shift * i;
  }
  return shift * n;
}

unsigned int int128_log2(int128_t x) {
  return x <= 1 ? 0 : 128 - clz128(x - 1);
}

bool calculate(struct parameters *param) {
  if(param->max < param->min + param->precision) {
    param->error = "max < min + precision";
    return false;
  }
  if(param->precision <= 0.0L) {
    param->error = "zero or negative precision";
    return false;
  }
  param->fractional_bits = -floor_log2l(param->precision);
  param->lower_bound = floorl(ldexpl(param->min, param->fractional_bits));
  param->upper_bound = ceill(ldexpl(param->max, param->fractional_bits));

  // push out bounds slightly if needed due to rounding
  if(ROUND(int128_t, param->lower_bound, param->fractional_bits) / param->precision > param->min / param->precision) param->lower_bound--;
  if(ROUND(int128_t, param->upper_bound, param->fractional_bits) / param->precision <  param->max / param->precision) param->upper_bound++;

  param->fixed_encoding_width = int128_log2(param->upper_bound - param->lower_bound + 1);
  param->integer_bits = param->fixed_encoding_width - param->fractional_bits;

  if(param->fixed_encoding_width > 64) {
    param->error = "fixed_encoding_width > 64";
    return false;
  }
  if(param->fixed_encoding_width < 8) { // could disable this lower bound to support packed formats
    param->fixed_encoding_width = 8;
  } else {
    param->fixed_encoding_width = 1 << int_log2(param->fixed_encoding_width);
  }
  param->offset = param->lower_bound;
  param->large_offset = false;
  if(param->offset > (((int128_t)1) << 63) - 1 &&
     param->offset < -(((int128_t)1) << 63)) {
    param->large_offset = true;
  }
  param->use_signed = false;
  if(param->min < 0.0L) {
    if(param->upper_bound <= (((int128_t)1) << (param->fixed_encoding_width - 1)) - 1 &&
       param->lower_bound >= (((int128_t)-1) << (param->fixed_encoding_width - 1))) {
      param->offset = 0;
      param->use_signed = true;
    }
  } else {
    if(param->upper_bound <= (((int128_t)1) << param->fixed_encoding_width) - 1) {
      param->offset = 0;
    }
  }
  return true;
}

#define max(x, y) ((y) > (x) ? (y) : (x))

void convert_to_double(struct parameters *param, FILE *f) {
#define printf(...) fprintf(f, __VA_ARGS__)
  int128_t
    lb = param->lower_bound - param->offset,
    ub = param->upper_bound - param->offset,
    min_int = param->use_signed ? -(1 << (param->fixed_encoding_width - 1)) : 0,
    max_int = (1 << (param->fixed_encoding_width - (param->use_signed ? 1 : 0))) - 1;
  printf("#include <math.h>\n"
         "#include <stdint.h>\n"
         "// can lose precision, for display only\n"
         "double convert_to_double(%s%d_t x) {\n",
         param->use_signed ? "int" : "uint", param->fixed_encoding_width);

  // Check bounds
  if(lb != min_int || ub != max_int) {
    printf("  if(");
    if(lb != min_int) {
      printf("x < %s%d_C(%lld)",
             param->use_signed ? "INT" : "UINT",
             param->fixed_encoding_width,
             (long long int)lb);
      if(ub != max_int) printf(" &&\n     ");
    }
    if(ub != max_int) {
      printf("x > %s%d_C(%lld)",
             param->use_signed ? "INT" : "UINT",
             param->fixed_encoding_width,
             (long long int)ub);
    }
    printf(") {\n"
      "    return NAN;\n"
      "  }\n");
  }

  int paren = 0;
  printf("  return ");
  if(param->min > ldexpl(param->lower_bound, -param->fractional_bits)) {
    paren++;
    printf("fmax(%.19Lg, ", param->min);
  }
  if(param->max < ldexpl(param->upper_bound, -param->fractional_bits)) {
    paren++;
    printf("fmin(%.19Lg, ", param->max);
  }
  if(param->precision != 1.0l) {
    printf("round(");
  }
  if(param->offset) printf("(");
  if(param->fractional_bits) {
    printf("ldexp(x, %d)", -param->fractional_bits);
  } else {
    printf("x");
  }
  if(param->offset) {
    printf(" + %.19Lg)", ldexpl(param->offset, -param->fractional_bits));
  }
  if(param->precision != 1.0L) {
    printf(" * %.19Lg) * %.19Lg",
           1.0L / param->precision,
           param->precision);
  }
  while(paren--) printf(")");
  printf(";\n");

  printf("}\n");
#undef printf
}

void print_params(struct parameters *param) {
  printf("[PARAMETERS]\n");
  printf("  min: %.19Lg (%.19Lg requested)\n",
         ldexpl(param->lower_bound, -param->fractional_bits),
         param->min);
  printf("  max: %.19Lg (%.19Lg requested)\n",
         ldexpl(param->upper_bound, -param->fractional_bits),
         param->max);
  long double actual_precision = ldexpl(1.0L, -param->fractional_bits);
  printf("  precision: %.19Lg (%.19Lg requested)\n",
         actual_precision,
         param->precision);
  printf("\n[CODE]\n");
  printf("  code density: %.1Lf%%\n", 100L * actual_precision / param->precision);
  if(param->large_offset) {
    printf("  offset: about %.19Lg\n", (long double)param->offset);
  } else {
    printf("  offset: %" PRId64 "\n", (int64_t)param->offset);
  }
  printf("  code range: [%" PRId64 ", %" PRId64 "]\n",
         (int64_t)(param->lower_bound - param->offset),
         (int64_t)(param->upper_bound - param->offset));
  printf("\n[ENCODING]\n");
  printf("  machine bit width: %d (%d used)\n", param->fixed_encoding_width, param->integer_bits + param->fractional_bits);
  printf("    fractional bits: %d\n", param->fractional_bits);
  printf("    integer bits: %d\n", param->integer_bits);
  printf("  use signed: %s\n", param->use_signed ? "yes" : "no");
  printf("  machine integer type: %s%d_t\n", param->use_signed ? "int" : "uint", param->fixed_encoding_width);
  printf("  Q notation: Q%c%d.%d\n", param->use_signed ? 's' : 'u', param->fixed_encoding_width - param->fractional_bits - (param->use_signed ? 1 : 0), param->fractional_bits); 
  printf("\n[CONVERSION]\n");
  convert_to_double(param, stdout);
}

void gen_converter(struct parameters *param) {
  FILE *f = fopen("convert.c", "w");
  fprintf(f,
          "#include <stdio.h>\n"
          "#include <stdlib.h>\n\n");
  convert_to_double(param, f);
  fprintf(f,
          "\n"
          "int main(int argc, char **argv) {\n"
          "  int i;\n"
          "  argv++; argc--; // skip first arg\n"
          "  for(i = 0; i < argc; i++) {\n"
          "    long int x = strtol(argv[i], NULL, 10);\n"
          "    printf(\"%%ld -> %%.15g\\n\", x, convert_to_double(x));\n"
          "  }\n"
          "  return 0;\n"
          "}\n");
  fclose(f);
}


long double eval_expr(char **pstr) {
  char *p = *pstr;
  long double a = strtold(p, &p), b = 0L;
  do {
    while(*p == ' ') p++;
    char op = *p++;
    if(!op || op == ')') break;
    while(*p == ' ') p++;
    if(!*p) break;
    if(*p == '(') {
      p++;
      b = eval_expr(&p);
    } else {
      b = strtold(p, &p);
    }
    switch(op) {
    case '+':
      a += b;
      break;
    case '-':
      a -= b;
      break;
    case '*':
      a *= b;
      break;
    case '/':
      a /= b;
      break;
    case '^':
      a = powl(a, b);
      break;
    default:
      a = NAN;
      goto end;
    }
  } while(!isnan(a));
end:
  *pstr = p;
  return a;
}

int main(int argc, char **argv) {
  struct parameters param;
  memset(&param, 0, sizeof(param));
  bool gen = false;
  if(argc <= 3) {
    printf("fpc [min] [max] [precision]\n");
    return -1;
  }
  if(strcmp(argv[1], "-g") == 0) {
    gen = true;
    argv++;
  }
  param.min = eval_expr(&argv[1]);
  param.max = eval_expr(&argv[2]);
  param.precision = eval_expr(&argv[3]);

  if(calculate(&param)) {
    print_params(&param);
    if(gen) gen_converter(&param);
    return 0;
  } else {
    fprintf(stderr, "ERROR: %s\n", param.error);
    return -1;
  }
}
