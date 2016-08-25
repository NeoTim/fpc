# Fixed-point Calculator

This program takes a range [min, max] and precision and calculates constants and information to be used in code performing calculations in this range using fixed-point integer math.

# Try it!

    $ git clone ssh://git@stash.nestlabs.com:7999/user/fpc.git
    $ make
    $ ./fpc -g 30 1800 0.1
    min: 30 (30)
    max: 1800 (1800)
    precision: 0.0625 (0.1)
    code density: 62.5%
    offset: 0
    code range: [480, 28800]
    machine bit width: 16
    fractional bits: 4
    integer bits: 11
    use signed: no
    machine integer type: uint16_t
    
    #include <math.h>
    #include <stdint.h>
    // can lose precision, for display only
    double convert_to_double(uint16_t x) {
      if(x < UINT16_C(480) &&
         x > UINT16_C(28800)) {
        return NAN;
      }
      return round(ldexp(x, -4) * 10) * 0.1;
    }
    $ make convert
    $ ./convert 480 600 10000 28800
    480 -> 30
    600 -> 37.5
    10000 -> 625
    28800 -> 1800

-   Also, check this out:

    $ ./fpc '-(2^7)' '2^7-1' '2^-7'
    [PARAMETERS]
      min: -128 (-128 requested)
      max: 127 (127 requested)
      precision: 0.0078125 (0.0078125 requested)
    ...

`fpc` has an expression evaluator built in to allow simple expressions!

-   evaluated left-to-right
-   no operator precedence
-   parenthesis for grouping