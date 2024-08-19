/*
	FUNCKy replacement routines
*/
#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <stdlib.h>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <functional>

#include "winsock2.h"
#include <Windows.h>

#include "stdio.h"
#include <stdint.h>
#include "io.h"
#include "fcntl.h"
#include "string.h"
#include "sys/locking.h"
#include "sys/stat.h"
#include "time.h"
#include "Utility/funcky.h"
#include "encodingTables.h"
#include "errno.h"
#include "buffer.h"

#include "util.h"

unsigned int sType[256] =
{
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  0
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  1
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  2
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  3
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  4
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  5
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  6
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  7
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  8
    0 |       0 |        0 |         0 |       0 |     0 | S_SPACE |     0 |    S_URL,  //  \t 9
    0 |       0 |        0 |         0 |       0 |     0 |       0 | S_EOL |    S_URL,  //  \n 10
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  11
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  12
    0 |       0 |        0 |         0 |       0 |     0 |       0 | S_EOL |    S_URL,  //  \r 13
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  14
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  15
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  16
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  17
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  18
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  19
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  20
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  21
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  22
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  23
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  24
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  25
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  26
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  27
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  28
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  29
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  30
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  31
    0 |       0 |        0 |         0 |       0 |     0 | S_SPACE |     0 |    S_URL,  //     32
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  !  33
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  "  34
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  #  35
    0 |       0 |        0 | S_SYMBOLB |       0 |     0 |       0 |     0 |    S_URL,  //  $  36
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  %  37
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  &  38
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  '  39
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  (  40
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  )  41
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  *  42
    0 |       0 |        0 |         0 |       0 | S_NUM |       0 |     0 |    S_URL,  //  +  43
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  ,  44
    0 |       0 |        0 |         0 |       0 | S_NUM |       0 |     0 |        0,  //  -  45
    0 |       0 |        0 |         0 | S_DIGIT | S_NUM |       0 |     0 |        0,  //  .  46
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  /  47
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  0  48
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  1  49
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  2  50
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  3  51
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  4  52
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  5  53
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  6  54
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  7  55
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  8  56
S_HEX |       0 |        0 | S_SYMBOLB | S_DIGIT | S_NUM |       0 |     0 |        0,  //  9  57
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  :  58
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //	;  59
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  <  60
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  =  61
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  >  62
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  ?  63
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |		0,  //  @  64
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  A  65
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  B  66
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  C  67
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  D  68
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  E  69
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  F  70
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  G  71
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  H  72
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  I  73
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  J  74
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  K  75
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  L  76
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  M  77
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  N  78
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  O  79
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  P  80
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  Q  81
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  R  82
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  S  83
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  T  84
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  U  85
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  V  86
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  W  87
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  X  88
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  Y  89
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  Z  90
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  [  91
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  \  92
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  ]  93
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  ^  94
    0 |       0 | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  _  95
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  `  96
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  a  97
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  b  98
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  c  99
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  d  100
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  e  101
S_HEX | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  f  102
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  g  103
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  h  104
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  i  105
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  j  106
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  k  107
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  l  108
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  m  109
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  n  110
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  o  111
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  p  112
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  q  113
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  r  114
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  s  115
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  t  116
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  u  117
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  v  118
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  w  119
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  x  120
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  y  121
    0 | S_ALPHA | S_SYMBOL | S_SYMBOLB |       0 |     0 |       0 |     0 |        0,  //  z  122
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  {  123
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  |  124
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  }  125
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  ~  126
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  .  127
    0 |       0 |        0 |         0 |       0 |     0 |       0 |     0 |    S_URL,  //  €  128
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //    129
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ‚  130
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ƒ  131
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  „  132
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  …  133
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  †  134
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ‡  135
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ˆ  136
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ‰  137
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Š  138
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ‹  139
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Œ  140
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //    141
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ž  142
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //    143
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //    144
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ‘  145
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ’  146
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  “  147
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ”  148
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  •  149
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  –  150
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  —  151
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ˜  152
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ™  153
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  š  154
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ›  155
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  œ  156
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //    157
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ž  158
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ÿ  159
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //     160
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¡  161
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¢  162
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  £  163
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¤  164
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¥  165
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¦  166
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  §  167
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¨  168
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ©  169
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ª  170
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  «  171
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¬  172
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ­  173
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ®  174
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¯  175
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  °  176
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ±  177
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ²  178
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ³  179
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ´  180
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  µ  181
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¶  182
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ·  183
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¸  184
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¹  185
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  º  186
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  »  187
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¼  188
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ½  189
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¾  190
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ¿  191
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  À  192
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Á  193
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Â  194
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ã  195
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ä  196
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Å  197
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Æ  198
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ç  199
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  È  200
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  É  201
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ê  202
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ë  203
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ì  204
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Í  205
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Î  206
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ï  207
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ð  208
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ñ  209
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ò  210
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ó  211
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ô  212
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Õ  213
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ö  214
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ×  215
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ø  216
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ù  217
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ú  218
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Û  219
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ü  220
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Ý  221
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  Þ  222
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ß  223
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  à  224
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  á  225
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  â  226
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ã  227
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ä  228
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  å  229
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  æ  230
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ç  231
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  è  232
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  é  233
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ê  234
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ë  235
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ì  236
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  í  237
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  î  238
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ï  239
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ð  240
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ñ  241
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ò  242
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ó  243
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ô  244
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  õ  245
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ö  246
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ÷  247
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ø  248
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ù  249
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ú  250
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  û  251
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ü  252
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ý  253
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  þ  254
    0 |       0 |        0 |         0 |       0 |     0 |       0 |	 0 |    S_URL,  //  ÿ  255
};

static struct {
	char const	*tzCode;
	int		 offset;
} aaShortTZ[] = { {	"UT",	 0	},
					{	"GMT",	0	},
					{	"EST",	-5	},
					{	"EDT",	-4	},
					{	"CST",	-6	},
					{	"CDT",	-5	},
					{	"MST",	-7	},
					{	"MDT",	-8	},
					{	"PST",	-8	},
					{	"PDT",	-9	},
					{	0,		0	},
};

char const *aShortMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char const *aShortDays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

char const *aMonths[] = { "January", "February", "March", "April", "May", "June",
					 "July", "August", "September", "October", "November", "December" };

char const *aDays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

char const *_arg ( int num )
{
	return (__argv[num]);
}
int _argc ( void )
{
	return (__argc - 1);
}

char *strnmerge ( char *dest, char const *src, int len )

{
	int	l1;
	int	l2;

	l1 = (int) strlen ( dest );
	l2 = (int) strlen ( src );

	if ( l1 + l2 + 1 >= len )
	{
		if ( l1 > len )
		{
			// result is already to big.. just return
			return (dest);
		}

		memcpy ( dest + l1, src, static_cast<size_t>(len) - l1 - 1 );
		dest[len - 1] = 0;
	} else
	{
		memcpy ( dest + l1, src, l2 );
		dest[l1 + l2] = 0;
	}
	return dest;
}

uint64_t _flen ( int handle )

{
	int64_t oldPtr;
	int64_t pos;

	oldPtr = _telli64 ( handle );
	pos = _lseeki64 ( handle, 0, SEEK_END );
	_lseeki64 ( handle, oldPtr, SEEK_SET );

	return pos;
}

char const *_firstchar ( char const *str )
{
	while ( (*str == ' ') || (*str == '\t') )
	{
		str++;
	}
	return str;
}
char *_firstchar ( char *str )
{
	while ( (*str == ' ') || (*str == '\t') )
	{
		str++;
	}
	return str;
}

char *_alltrim ( char const *inPtr, char *dstPtr )
{
	char	*endPtr;
	char	*ret;

	ret = dstPtr;

	endPtr = dstPtr;

	/* skip leading spaces */
	while ( (*inPtr == ' ') || (*inPtr == '\t') )
	{
		inPtr++;
	}

	/* copy all... noting last non-space character */
	while ( *inPtr )
	{
		if ( *inPtr && ((*inPtr == ' ') || (*inPtr == '\t')) )
		{
			if ( !endPtr )
			{
				endPtr = dstPtr;
			}
		} else
		{
			endPtr = 0;
		}
		*dstPtr++ = *inPtr++;
	}

	/* terminate after last non-space character */
	if ( endPtr )
	{
		*endPtr = 0;
	} else
	{
		*dstPtr = 0;
	}

	return (ret);
}

int _chrany ( char const *tokenList, char const *str )

{
	char const *tokenPtr;

	if ( !*str )
	{
		return (0);
	}

	while ( *str )
	{
		tokenPtr = tokenList;
		while ( *tokenPtr )
		{
			if ( *tokenPtr == *str )
			{
				return (1);
			}
			tokenPtr++;
		}
		str++;
	}
	return (0);
}

int _at ( char const *substr, char const *str )

{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;

	if ( !*str )
	{
		return (-1);
	}
	if ( !*substr )
	{
		return (-1);
	}


	subStrLen = (int) strlen ( substr );
	strLen = (int) strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( (*(str + matchPos) == *substr) && !memcmpi ( str + matchPos, substr, subStrLen ) )
		{
			return (matchPos);
		}
	}

	return (-1);
}

char *_token ( char const *str, char const *token, int num, char *buff )

{
	int		 count;
	const char	*tokenPtr;
	char	*returnBuff;

	returnBuff = buff;

	count = 0;

	if ( !*str )
	{
		*buff = 0;
		return (buff);
	}

	if ( !token )
	{
		token = "\x09\x0C\x1A\x20\x8A\x8D";
	}

	/* special case for very first token */
	if ( num == 1 )
	{
		while ( *str )
		{
			tokenPtr = token;
			while ( *tokenPtr )
			{
				if ( *str == *tokenPtr )
				{
					*buff = 0;
					return (returnBuff);
				}
				tokenPtr++;
			}
			*buff++ = *str++;
		}
	}
	num--;

	/* search the string for tokens */
	while ( *str )
	{
		/* check this character */
		tokenPtr = token;
		while ( *tokenPtr )
		{
			if ( *str == *tokenPtr )
			{
				/* found a token */
				count++;

				/* we're on a token...go to next chr */
				if ( count == num )
				{
					str++;
					/* found our token... now copy it */
#if 0
					/* skip until next non-token */
					while ( *str )
					{
						tokenPtr = token;
						while ( *tokenPtr )
						{
							if ( *tokenPtr == *str )
							{
								break;
							}
							tokenPtr++;
						}
						if ( !*tokenPtr )
						{
							break;
						}
						str++;
					}
#endif				
					/* copy it off */
					while ( *str )
					{
						tokenPtr = token;
						while ( *tokenPtr )
						{
							if ( *str == *tokenPtr )
							{
								*buff = 0;
								return (returnBuff);
							}
							tokenPtr++;
						}
						*buff++ = *str++;
					}
					*buff = 0;
					return (returnBuff);
				}
#if 0
				/* skip until next non-token */
				while ( *str )
				{
					tokenPtr = token;
					while ( *tokenPtr )
					{
						if ( *tokenPtr == *str )
						{
							break;
						}
						tokenPtr++;
					}
					if ( !*tokenPtr )
					{
						break;
					}
					str++;
				}
				/* we're on the right chr...but we'll increment it
					so we decrement it here so it all works out
				*/
				str--;
#endif
				break;
			}
			tokenPtr++;
		}
		str++;
	}

	*buff = 0;
	return (returnBuff);
}

char *_tokenn ( char const *str, char const *token, int num, char *buff, int buffMaxLen )

{
	int		 count;
	char const	*tokenPtr;
	char	*returnBuff;
	int		 len;

	returnBuff = buff;

	count = 0;

	if ( !*str )
	{
		*buff = 0;
		return (buff);
	}

	if ( !token )
	{
		token = "\x09\x0C\x1A\x20\x8A\x8D";
	}

	buffMaxLen--;	/* for null */
	len = 0;

	/* special case for very first token */
	if ( num == 1 )
	{
		while ( *str && (len < buffMaxLen) )
		{
			tokenPtr = token;
			while ( *tokenPtr )
			{
				if ( *str == *tokenPtr )
				{
					*buff = 0;
					return (returnBuff);
				}
				tokenPtr++;
			}
			*buff++ = *str++;
			len++;
		}
	}
	num--;

	/* search the string for tokens */
	while ( *str )
	{
		/* check this character */
		tokenPtr = token;
		while ( *tokenPtr )
		{
			if ( *str == *tokenPtr )
			{
				/* found a token */
				count++;

				/* we're on a token...go to next chr */
				str++;

				if ( count == num )
				{
					/* found our token... now copy it */

					/* skip until next non-token */
					while ( *str )
					{
						tokenPtr = token;
						while ( *tokenPtr )
						{
							if ( *tokenPtr == *str )
							{
								break;
							}
							tokenPtr++;
						}
						if ( !*tokenPtr )
						{
							break;
						}
						str++;
					}

					/* copy it off */
					while ( *str && (len < buffMaxLen) )
					{
						tokenPtr = token;
						while ( *tokenPtr )
						{
							if ( *str == *tokenPtr )
							{
								*buff = 0;
								return (returnBuff);
							}
							tokenPtr++;
						}
						*buff++ = *str++;
						len++;
					}
					*buff = 0;
					return (returnBuff);
				}
#if 0
				/* skip until next non-token */
				while ( *str )
				{
					tokenPtr = token;
					while ( *tokenPtr )
					{
						if ( *tokenPtr == *str )
						{
							break;
						}
						tokenPtr++;
					}
					if ( !*tokenPtr )
					{
						break;
					}
					str++;
				}
				/* we're on the right chr...but we'll increment it
					so we decrement it here so it all works out
				*/
				str--;
#endif
				break;
			}
			tokenPtr++;
		}
		str++;
	}

	*buff = 0;
	return (returnBuff);
}

int _isalpha ( char const *str, int num )

{
	if ( !*str )
	{
		return (0);
	}

	while ( *str && num )
	{
		if ( !(((*str >= 'A') && (*str <= 'Z')) ||
			((*str >= 'a') && (*str <= 'z')) ||
				((*str == ' ') || (*str == '\t')))
			 )
		{
			return (0);
		}
		str++;
		num--;
	}
	return (1);
}

int _islower ( char const *str, int num )

{
	if ( !*str )
	{
		return (0);
	}

	while ( *str && num )
	{
		if ( _isalphac ( str ) )
		{
			if ( !((*str >= 'a') && (*str <= 'z')) )
			{
				return (0);
			}
		}
		str++;
		num--;
	}
	return (1);
}

int _isupper ( char const *str, int num )

{
	if ( !*str )
	{
		return (0);
	}

	while ( *str && num )
	{
		if ( _isalphac ( str ) )
		{
			if ( !((*str >= 'A') && (*str <= 'Z')) )
			{
				return (0);
			}
		}
		str++;
		num--;
	}
	return (1);
}

char *_fwebname ( char const *src, char *dest )

{
	char *ret;
	int	  ctr;

	ret = dest;

	ctr = 0;
	while ( *src )
	{
		if ( ctr >= (MAX_NAME_SZ - 1) )
		{
			break;
		}
		if ( *src == '\\' )
		{
			*(dest++) = '/';
			src++;
		} else
		{
			*(dest++) = *(src++);
		}
		ctr++;
	}
	*dest = 0;
	return (ret);

}

char *_fmerge ( char const *temp, char const *inName, char *dest, size_t destSize )
{
	std::filesystem::path pat ( temp );
	std::filesystem::path in ( inName );
	std::filesystem::path d;

	if ( !pat.root_path().generic_string ().c_str ()[0] || pat.root_path().generic_string ().c_str ()[0] == '*' )
	{
		d = in.root_path ();
	} else
	{
		d = pat.root_path ();
	}

	if ( !std::filesystem::path( pat ).remove_filename().generic_string ().c_str()[0] || std::filesystem::path( pat ).remove_filename().generic_string ().c_str()[0] == '*' )
	{
		d /= std::filesystem::path ( in ).remove_filename ();
	} else
	{
		d /= std::filesystem::path ( pat ).remove_filename ();
	}

	if ( !pat.stem().generic_string ().c_str ()[0] || pat.stem().generic_string ().c_str ()[0] == '*' )
	{
		d /= in.stem ();
	} else
	{
		d /= pat.stem ();
	}

	if ( !pat.extension ().generic_string ().c_str ()[0] || pat.extension ().generic_string ().c_str ()[0] == '*' )
	{
		d += in.extension();
	} else
	{
		d += pat.extension();
	}

	strcpy_s ( dest, destSize, d.generic_string ().c_str () );

	return dest;
}

char *_fmake ( char const *drive, char const *path, char const *name, char const *ext, char *dest, size_t destSize )
{
	std::vector<char const *> components = { drive, path, name, ext };

	auto ret = std::accumulate ( std::next ( components.begin () ), components.end (), std::filesystem::path {}, std::divides {} ).generic_string ();

	strcpy_s ( dest, destSize, ret.c_str () );

	return dest;
}

char *_parseUserid ( char const *url, char *userId )

{
	char	*ret;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;
	}

	if ( !userId )
	{
		userId = _strdup ( url );
	}

	ret = userId;

	while ( *url )
	{
		if ( (*url == '\\') || (*url == '/') )
		{
			*ret = 0;
			return (ret);
		}
		if ( *url == ':' )
		{
			break;
		}
		*(userId++) = *(url++);
	}

	*userId = 0;
	return (ret);
}

char *_parsePassword ( char const *url, char *password )

{
	char	*ret;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;
	}

	if ( !password )
	{
		password = _strdup ( url );
	}

	ret = password;

	// skip over userId
	while ( *url )
	{
		if ( *url == ':' )
		{
			url++;
			break;
		}
		if ( (*url == '\\') || (*url == '/') || (*url == '@') )
		{
			// no password
			*ret = 0;
			return (ret);
		}
		url++;
	}
	while ( *url )
	{
		if ( (*url == '\\') || (*url == '/') || (*url == '@') )
		{
			break;
		}
		*(password++) = *(url++);
	}

	*password = 0;
	return (ret);
}

char *_parseDomain ( char const *url, char *domain )

{
	char	*ret;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;
	}

	if ( !domain )
	{
		domain = _strdup ( url );
	}

	ret = domain;

	while ( *url )
	{
		if ( *url == '@' )
		{
			// skip over userId/Password
			domain = ret;
			url++;
			continue;
		}
		if ( (*url == '\\') || (*url == '/') )
		{
			break;
		}
		*(domain++) = *(url++);
	}

	*domain = 0;
	return (ret);
}

char *_parseURI ( char const *url, char *uri )

{
	char	*ret;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;
	}

	if ( !uri )
	{
		uri = _strdup ( url );
	}

	ret = uri;

	while ( *url )
	{
		if ( (*url == '\\') || (*url == '/') )
		{
			url++;
			break;
		}
		url++;
	}

	while ( *url )
	{
		*(uri++) = *(url++);
	}

	*uri = 0;
	return (ret);
}

unsigned long _isfile ( char const *fName )
{
	WIN32_FILE_ATTRIBUTE_DATA	attrData{};

	return (unsigned long) (GetFileAttributesEx ( fName, GetFileExInfoStandard, &attrData ));
}

unsigned long _isDirectory ( char const *fName )
{
	WIN32_FILE_ATTRIBUTE_DATA	attrData{};

	if ( GetFileAttributesEx ( fName, GetFileExInfoStandard, &attrData ) )
	{
		return (unsigned long)(attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	}
	return false;
}

char *_getenv ( char const *varName, char *dest )

{
	GetEnvironmentVariable ( varName, dest, 129 );
	return (dest);
}

char * _upper ( char const *src, char *dest )
{
	char	 chr1;
	char	*ret;

	ret = dest;

	while ( *src )
	{
		chr1 = *(src++);
		if ( (chr1 >= 'a') && (chr1 <= 'z') )
		{
			chr1 = (char)((chr1 - 'a') + 'A');
		}
		*(dest++) = chr1;
	}
	*(dest++) = 0;

	return (ret);
}

char *_uppern ( char *dest, char const *src, int len )
{
	char	 chr1;
	char	*ret;

	ret = dest;

	len--;			// for trailing 0
	while ( *src && len-- )
	{
		chr1 = *(src++);
		if ( (chr1 >= 'a') && (chr1 <= 'z') )
		{
			chr1 = (char)((chr1 - (char)'a') + (char)'A');
		}
		*(dest++) = chr1;
	}
	*(dest++) = 0;

	return (ret);
}

int _ati ( char const *substr, char const *str )

{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;

	if ( !*str )
	{
		return (-1);
	}
	if ( !*substr )
	{
		return (-1);
	}

	subStrLen = (int) strlen ( substr );
	strLen = (int) strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( !memcmpi ( str + matchPos, substr, subStrLen ) )
		{
			return (matchPos);
		}
	}
	return (-1);
}

int _numtoken ( char const *str, char const *token )
{
	int			 count;
	char const	*tokenPtr;

	count = 0;

	if ( !str )
	{
		return (0);
	}

	if ( !*str )
	{
		return (0);
	}

	if ( !token )
	{
		token = "\x09\x0C\x1A\x20\x8A\x8D";
	}

	while ( *str )
	{
		tokenPtr = token;
		while ( *tokenPtr )
		{
			if ( *str == *tokenPtr )
			{
				/* found a token */
				count++;
				str++;
			}
			tokenPtr++;
		}
		if ( *str ) str++;
	}

	/* +1 for trailiing token */
	return (count + 1);
}

int _attoken ( char const *str, char const *token, int num )

{
	int		 count;
	char const *tokenPtr;
	int		 pos;

	count = 0;

	if ( !*str )
	{
		return (-1);
	}

	if ( !token )
	{
		token = "\x09\x0C\x1A\x20\x8A\x8D";
	}
	pos = 0;

	/* special case for very first token */
	if ( num == 1 )
	{
		while ( *str )
		{
			tokenPtr = token;
			while ( *tokenPtr )
			{
				if ( *str == *tokenPtr )
				{
					return (pos);
				}
				tokenPtr++;
			}
			str++;
			pos++;
		}
	}
	num--;

	/* search the string for tokens */
	while ( *str )
	{
		/* check this character */
		tokenPtr = token;
		while ( *tokenPtr )
		{
			if ( *str == *tokenPtr )
			{
				/* found a token */
				count++;

				/* we're on a token...go to next chr */
				str++;
				pos++;

				if ( count == num )
				{
					/* found our token... now copy it */

					/* skip until next non-token */
					while ( *str )
					{
						tokenPtr = token;
						while ( *tokenPtr )
						{
							if ( *tokenPtr == *str )
							{
								break;
							}
							tokenPtr++;
						}
						if ( !*tokenPtr )
						{
							break;
						}
						str++;
						pos++;
					}

					return (pos);
				}

				/* skip until next non-token */
				while ( *str )
				{
					tokenPtr = token;
					while ( *tokenPtr )
					{
						if ( *tokenPtr == *str )
						{
							break;
						}
						tokenPtr++;
					}
					if ( !*tokenPtr )
					{
						break;
					}
					str++;
					pos++;
				}
				/* we're on the right chr...but we'll increment it
					so we decrement it here so it all works out
				*/
				str--;
				pos--;
				break;
			}
			tokenPtr++;
		}
		if ( *str ) str++;
		pos++;
	}
	return (-1);
}



char *memcpyset ( char *dest, char const *src, size_t totalLen, size_t srcLen, char setChr )
{
	if ( srcLen > totalLen )
	{
		srcLen = totalLen;
	}
	memcpy ( dest, src, srcLen );
	if ( srcLen < totalLen )
	{
		memset ( dest + srcLen, setChr, totalLen - srcLen );
	}
	return (dest);

}

char *trimpunct ( char const *src, char *dst )
{
	char *end;
	char *ret;

	ret = dst;

	/* skip leading punctuation */
	while ( !(((*src >= 'A') && (*src <= 'Z')) ||
		((*src >= 'a') && (*src <= 'z')) ||
			   ((*src >= '0') && (*src <= '9')) ||
			   (*src == '\t')
			   ) && *src
			)
	{
		src++;
	}

	end = 0;

	while ( *src )
	{
		/* are we on punctuation? */
		if ( !(((*src >= 'A') && (*src <= 'Z')) ||
			((*src >= 'a') && (*src <= 'z')) ||
				((*src >= '0') && (*src <= '9')) ||
				(*src == '\t')
				)
			 )
		{
			/* mark potential end punctuation */
			end = dst;

			/* skip punctuation */
			while ( !(((*src >= 'A') && (*src <= 'Z')) ||
				((*src >= 'a') && (*src <= 'z')) ||
					   ((*src >= '0') && (*src <= '9')) ||
					   (*src == '\t')
					   ) && *src
					)
			{
				*(dst++) = *(src++);
			}
			if ( !*src )
			{
				/* we finished the string.. so end is true end */
				*end = 0;
				return (ret);
			}
			end = 0;
		}
		*(dst++) = *(src++);
	}

	/* add a trailing null */
	if ( end )
	{
		*end = 0;
	} else
	{
		*dst = 0;
	}

	return (ret);
}

unsigned int itoj ( int y, int m, int d )
{
	if ( m > 2 )
	{
		m -= 3;
	} else
	{
		m += 9;
		--y;
	}
	return ((y / 100) * 146097 / 4 + (y % 100) * 1461 / 4 + (m * 153 + 2) / 5 + d + 1721119);
}


/* this is a bit bs...it won't handle dates BCE or even before 1582... calender got changed... */
unsigned int jtoi ( int j, int *year, int*month, int*day )
{
	int d, m, y;

	if ( !j )
	{
		j = _jdate ( );
	}

	j -= 1721119;

	y = (4 * j - 1) / 146097;
	j = (4 * j - 1) % 146097;
	d = j / 4;

	j = (4 * d + 3) / 1461;
	d = (4 * d + 3) % 1461;
	d = (d + 4) / 4;

	m = (5 * d - 3) / 153;
	d = (5 * d - 3) % 153;
	d = (d + 5) / 5;

	y = 100 * y + j;
	if ( m < 10 )
	{
		m += 3;
	} else
	{
		m -= 9;
		++y;
	}

	*year = y;
	*month = m;
	*day = d;

	return (1);
}

static char hexValues[] = "0123456789ABCDEF";


unsigned long encHexExtendedX ( char const *src, int len, char *dest )

{
	unsigned long	 i;

	i = 0;

	while ( len-- )
	{
		if ( *src == ' ' )
		{
			dest[i++] = '+';
		} else if ( _isurl ( src ) )
		{
			dest[i++] = '%';
			if ( *src < 0x10 )
			{
				dest[i++] = '0';
				_ltoa ( *src, dest + i, 16 );
				i++;
			} else
			{
				_ltoa ( *src, dest + i, 16 );
				i += 2;
			}
		} else
		{
			dest[i++] = *src;
		}
		src++;
	}

	dest[i++] = 0;

	return (i);
}


long _jdate ( void )
{
	struct tm	utcTime;
	time_t		lTime;

	lTime = time ( 0 );
	localtime_s( &utcTime, &lTime );

	return (long) (itoj ( utcTime.tm_year + 1900, utcTime.tm_mon + 1, utcTime.tm_mday ));
}

long webGMTToTime ( char const *str )

{
	char		 days[4];
	char		 months[4];
	char		 tz[4]{};
	int			 day;
	int			 month;
	int			 year;
	int			 hour;
	int			 min;
	int			 sec;
	struct tm	 utcTime {};
	int			 time;
	int			 loop;

	if ( strlen ( str ) < 29 )
	{
		return (0);
	}

	*tz = 0;

	if ( str[3] == ',' )
	{
		if ( str[7] == '-' )
		{
			sscanf ( str, "%3s, %02u-%3s-%04u %02u:%02u:%02u %3s", days, &day, months, &year, &hour, &min, &sec, tz );
		} else
		{
			sscanf ( str, "%3s, %02u %3s %04u %02u:%02u:%02u %3s", days, &day, months, &year, &hour, &min, &sec, tz );
		}
	} else
	{
		char *tmpP;

		tmpP = _strdup ( str );

		_token ( str, ",", 2, tmpP );

		_alltrim ( tmpP, tmpP );

		if ( tmpP[2] == '-' )
		{
			sscanf ( tmpP, "%02u-%3s-%04u %02u:%02u:%02u %3s", &day, months, &year, &hour, &min, &sec, tz );
		} else
		{
			sscanf ( tmpP, "%02u %3s %04u %02u:%02u:%02u %3s", &day, months, &year, &hour, &min, &sec, tz );
		}

		free ( tmpP );
	}

	month = 1;
	for ( loop = 0; loop < 12; loop++ )
	{
		if ( !memcmpi ( aShortMonths[loop], months, 3 ) )
		{
			month = loop;
			break;
		}
	}

	// adjust for time-zone
	for ( loop = 0; aaShortTZ[loop].tzCode; loop++ )
	{
		if ( !fglstrccmp ( aaShortTZ[loop].tzCode, tz ) )
		{
			hour -= aaShortTZ[loop].offset;
			break;
		}
	}

	year -= 1900;

	utcTime.tm_hour = hour;
	utcTime.tm_min = min;
	utcTime.tm_sec = sec;
	utcTime.tm_mday = day;
	utcTime.tm_mon = month;
	utcTime.tm_year = year;

	time = (long) mktime ( &utcTime );

	return (time);
}

char *_webGMT ( time_t lTime, char *buffer )

{
	struct tm	 utcTime;
	char		*dst;

	if ( buffer )
	{
		dst = buffer;
	} else
	{
		dst = (char *) malloc ( sizeof ( char ) * 30 );
		if ( !dst ) throw errorNum::scMEMORY;
	}

	gmtime_s ( &utcTime, &lTime );

	sprintf ( dst, "%s, %02u %s %04u %02u:%02u:%02u GMT",
			  aShortDays[utcTime.tm_wday],
			  utcTime.tm_mday,
			  aShortMonths[utcTime.tm_mon],
			  utcTime.tm_year + 1900,
			  utcTime.tm_hour,
			  utcTime.tm_min,
			  utcTime.tm_sec
	);

	return (dst);
}

char *_mailGMT ( time_t lTime, char *buffer )
{
	struct tm	 utcTime;
	char		*dst;

	if ( buffer )
	{
		dst = buffer;
	} else
	{
		dst = (char *) malloc ( sizeof ( char ) * 40 );
		if ( !dst ) throw errorNum::scMEMORY;
	}

	gmtime_s ( &utcTime, &lTime );

	_snprintf_s ( dst, 40, _TRUNCATE, "%s, %02u %s %04u %02u:%02u:%02u %+05i",
				  aShortDays[utcTime.tm_wday],
				  utcTime.tm_mday,
				  aShortMonths[utcTime.tm_mon],
				  utcTime.tm_year + 1900,
				  utcTime.tm_hour,
				  utcTime.tm_min,
				  utcTime.tm_sec,
				  (-(_timezone * 100) / 3600)
	);
	return (dst);
}

char *_webLocal ( time_t lTime, char *buffer )
{
	struct tm	 utcTime;
	char		*dst;

	if ( buffer )
	{
		dst = buffer;
	} else
	{
		dst = (char *) malloc ( sizeof ( char ) * 26 );
		if ( !dst ) throw errorNum::scMEMORY;
	}

	localtime_s ( &utcTime, &lTime );

	sprintf ( dst, "%s, %02u %s %04u %02u:%02u:%02u",
			  aShortDays[utcTime.tm_wday],
			  utcTime.tm_mday,
			  aShortMonths[utcTime.tm_mon],
			  utcTime.tm_year + 1900,
			  utcTime.tm_hour,
			  utcTime.tm_min,
			  utcTime.tm_sec
	);

	return (dst);
}

char *_xmlTime ( time_t lTime, char *buffer )
{
	struct tm	 utcTime;
	char		*dst;

	if ( buffer )
	{
		dst = buffer;
	} else
	{
		dst = (char *) malloc ( sizeof ( char ) * 26 );
	}

	gmtime_s ( &utcTime, &lTime );

	_snprintf_s ( dst, 26, _TRUNCATE, "%04u-%02u-%02uT%02u:%02u:%02u.000Z",
				  utcTime.tm_year + 1900,
				  utcTime.tm_mon + 1,
				  utcTime.tm_mday,
				  utcTime.tm_hour,
				  utcTime.tm_min,
				  utcTime.tm_sec
	);
	return (dst);
}

char *_ipToA ( unsigned long ip, char *buff )
{
	if ( !buff )
	{
		if ( !(buff = (char *) malloc ( sizeof ( char ) * 16 )) )
		{
			return (0);
		}
	}

	sprintf ( buff, "%lu.%lu.%lu.%lu", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF );

	return (buff);
}

unsigned long _AToIp ( char const *buff )
{
	unsigned long a, b, c, d;

	sscanf ( buff, "%lu.%lu.%lu.%lu", &a, &b, &c, &d );

	return ((a << 24) | (b << 16) | (c << 8) | d);
}

long datej ( char const *date )
{
//	struct tm *tm;
//	time_t	t;
	char dayC[4], monthC[4], yearC[6];
	int	day, month, year;

	if ( strlen ( date ) == 6 )
	{
		memcpy ( dayC, date, 2 );
		dayC[2] = 0;
		memcpy ( monthC, date + 2, 2 );
		monthC[2] = 0;
		memcpy ( yearC, date + 4, 2 );
		yearC[2] = 0;
	} else if ( (strlen ( date ) == 8) && !strchr ( "/-.", date[2] ) )
	{
		memcpy ( monthC, date, 2 );
		monthC[2] = 0;
		memcpy ( dayC, date + 2, 2 );
		dayC[2] = 0;
		memcpy ( yearC, date + 4, 4 );
		yearC[4] = 0;
	} else
	{
		_tokenn ( date, "/-.", 1, monthC, 3 );
		_tokenn ( date, "/-.", 2, dayC, 3 );
		_tokenn ( date, "/-.", 3, yearC, 5 );
	}

	day = atoi ( dayC );
	month = atoi ( monthC );
	year = atoi ( yearC );

	if ( year > 99 )
	{
	} else if ( year < 100 )
	{
		if ( year <= 10 )
		{
			year += 2000;	/* 2 DIGIT CONVERT!!! */
		} else
		{
			year += 1900;
		}
	} else
	{
		return (0);
	}

	return (long)(itoj ( year, month, day ));
}

char *dtoc ( long lDate, char *szDest )
{
	int  iDay;
	int  iMonth;
	int  iYear;
	char cTmp[12];

	jtoi ( lDate, &iYear, &iMonth, &iDay );

	_itoa ( iMonth, cTmp, 10 );
	if ( iMonth > 9 )
	{
		szDest[0] = cTmp[0];
		szDest[1] = cTmp[1];
	} else
	{
		szDest[0] = '0';
		szDest[1] = cTmp[0];
	}
	szDest[2] = '-';

	_itoa ( iDay, cTmp, 10 );

	if ( iDay > 9 )
	{
		szDest[3] = cTmp[0];
		szDest[4] = cTmp[1];
	} else
	{
		szDest[3] = '0';
		szDest[4] = cTmp[0];
	}
	szDest[5] = '-';

	_itoa ( iYear, cTmp, 10 );

	szDest[6] = cTmp[2];
	szDest[7] = cTmp[3];
	szDest[8] = 0;

	return(szDest);
}

char *dtos ( long lDate, char *szDest )
{
	int  iDay;
	int  iMonth;
	int  iYear;
	char cTmp[9];

	jtoi ( lDate, &iYear, &iMonth, &iDay );

	_itoa ( iYear, cTmp, 10 );
	szDest[0] = cTmp[0];
	szDest[1] = cTmp[1];
	szDest[2] = cTmp[2];
	szDest[3] = cTmp[3];

	_itoa ( iMonth, cTmp, 10 );
	if ( iMonth > 9 )
	{
		szDest[4] = cTmp[0];
		szDest[5] = cTmp[1];
	} else
	{
		szDest[4] = '0';
		szDest[5] = cTmp[0];
	}

	_itoa ( iDay, cTmp, 10 );
	if ( iDay > 9 )
	{
		szDest[6] = cTmp[0];
		szDest[7] = cTmp[1];
	} else
	{
		szDest[6] = '0';
		szDest[7] = cTmp[0];
	}
	szDest[8] = 0;

	return(szDest);
}

bool wildcardMatch ( char const *str, char const *match )
{
	while ( *match )
	{
		if ( *match == '*' )
		{
			if ( wildcardMatch ( str, match + 1 ) ) return true;
			if ( *str && wildcardMatch ( str + 1, match ) ) return true;
			return false;
		} else if ( *match == '?' )
		{
			if ( !*str ) return false;
			++str;
			++match;
		} else
		{
			if ( caseInsenstiveTable[(size_t)(uint8_t)*(str++)] != caseInsenstiveTable[(size_t)(uint8_t)*(match++)] ) return false;
		}
	}
	return !*str;
}

char *_strcapfirst ( char const *str, char *dst )
{
	const char	*spaceString = " \x09\x0A\x0D\x27\x8A\x8D.,-?/;:#+*";
	char		*ret;

	ret = dst;

	/* very first character */
	if ( *str )
	{
		if ( *str >= 'a' && *str <= 'z' )
		{
			/* in range so convert to upercase */
			*(dst++) = (char)((int)*(str++) - 'a' + 'A');
		} else
		{
			/* not in range so just copy */
			*(dst++) = *(str++);
		}
	}

	while ( *str )
	{
		if ( strchr ( spaceString, *str ) )
		{
			/* copy over space character */
			*(dst++) = *(str++);

			if ( *str )
			{
				if ( *str >= 'a' && *str <= 'z' )
				{
					/* in range so convert to upercase */
					*(dst++) = (char)((int)*(str++) - 'a' + 'A');
				} else
				{
					/* not in range so just copy */
					*(dst++) = *(str++);
				}
			}
		} else
		{
			/* just copy */
			if ( *str >= 'A' && *str <= 'Z' )
			{
				*(dst++) = (char)((int)*(str++) - 'A' + 'a');
			} else
			{
				*(dst++) = *(str++);
			}
		}
	}
	*dst = 0;

	return (ret);


}

#define QUOTIENT 0x04c11db7
static unsigned int crctab[256];

static bool crc32_init ( void )
{
	int i, j;

	unsigned int crc;

	for ( i = 0; i < 256; i++ )
	{
		crc = i << 24;
		for ( j = 0; j < 8; j++ )
		{
			if ( crc & 0x80000000 )
				crc = (crc << 1) ^ QUOTIENT;
			else
				crc = crc << 1;
		}
		crctab[i] = htonl ( crc );
	}
	return true;
}

static bool crcInitComplete = crc32_init ( );

unsigned int _crc32 ( unsigned char *data, unsigned int result, int len )
{
	unsigned int	*p = (unsigned int *) data;
	unsigned int	*e = (unsigned int *) ((ptrdiff_t) (data + len) & ~0x03); //  0xFFFFFFFC);		// NOLINT (performance-no-int-to-ptr)
	unsigned int	 tmp = 0;

	if ( p < e )
	{
		result = ~*p++;

		while ( p < e )
		{
			result = crctab[result & 0xff] ^ result >> 8;
			result = crctab[result & 0xff] ^ result >> 8;
			result = crctab[result & 0xff] ^ result >> 8;
			result = crctab[result & 0xff] ^ result >> 8;
			result ^= *p++;
		}
	}

	e = (unsigned int *) (data + len);	/* true end */

	if ( p < e )
	{
		switch ( (int) (e - p) )
		{
			case 3:
				tmp = *((char *) p + 2) << 24;
				tmp |= *((char *) p + 1) << 16;
				tmp |= *((char *) p + 0) << 8;
				break;
			case 2:
				tmp = *((char *) p + 1) << 24;
				tmp |= *((char *) p + 0) << 16;
				break;
			case 1:
				tmp = *((char *) p) << 24;
				break;
		}
		result ^= tmp;
	}

	return (~result);
}

static int				oldRand;
static CRITICAL_SECTION *randCriticalSection = 0;


int _rand ( void )
{
	if ( !randCriticalSection )
	{
		randCriticalSection = (CRITICAL_SECTION *) malloc ( sizeof ( CRITICAL_SECTION ) );
		if ( !randCriticalSection ) throw errorNum::scMEMORY;

		InitializeCriticalSectionAndSpinCount ( randCriticalSection, 4000 );
	}

	EnterCriticalSection ( randCriticalSection );

	oldRand = oldRand * 214013L + 2531011L;

	LeaveCriticalSection ( randCriticalSection );

	return ((oldRand >> 16) & 0x7fff);

}

int freadline ( int iHandle, char *buff, int buffLen )
{
	char	*tmpC;
	int		 len;
	int		 lineLen;

	len = _read ( iHandle, buff, buffLen );

	tmpC = buff;
	lineLen = 0;

	while ( (*tmpC != '\r') && (*tmpC != '\n') )
	{
		tmpC++;
		lineLen++;
		if ( (lineLen >= (buffLen - 1)) || (lineLen >= len) )
		{
			break;
		}
	}

	*(tmpC++) = 0;	/* null terminate the string */

	if ( *tmpC == '\n' )
	{
		_lseek ( iHandle, -len + lineLen + 2, SEEK_CUR );	/* seek to the real next position + 1 for \n */
	} else
	{
		_lseek ( iHandle, -len + lineLen + 1, SEEK_CUR );	/* seek to the real next position */
	}

	return lineLen;
}

char *_unQuote ( char *buff, char *dest )
{
	int		len;
	char	endC;

	if ( !dest )
	{
		dest = buff;
	}

	switch ( *buff )
	{
		case '\"':
			endC = '\"';
			break;
		case '\'':
			endC = '\'';
			break;
		case '[':
			endC = ']';
			break;
		case '(':
			endC = ')';
			break;
		case '{':
			endC = '}';
			break;
		case '<':
			endC = '>';
			break;
		default:
			/* not a quoted string */
			if ( buff != dest )
			{
				memcpy ( dest, buff, strlen ( buff ) + 1 );
			}
			return (dest);
	}

	if ( (len = (int) strlen ( buff + 1 )) )
	{
		if ( buff[len] == endC )
		{
			/* valid quoted string... */
			memmove ( dest, buff + 1, static_cast<size_t>(len) - 1 );
			dest[len - 1] = 0;
		}
	}

	return (dest);
}

char *_unDecimate ( char *str, char *dest )

{
	char *tmpC;
	char *ret;

	tmpC = str;

	if ( !dest )
	{
		dest = str;
	}

	ret = dest;

	while ( *tmpC )
	{
		if ( *tmpC == ',' )
		{
			tmpC++;
		} else
		{
			*(dest++) = *(tmpC++);
		}
	}

	*dest = 0;

	return (ret);

}

unsigned int webAddrFromHost ( char const *host )
{
	struct hostent		*hostEntry;
	unsigned char		*addr;
	unsigned long		val;

#if _WIN32
	if ( !(hostEntry = gethostbyname ( host )) )	// NOLINT(concurrency-mt-unsafe)	this is SAFE under windows... should not emit a diagnostic
	{
		return (0);
	}
#else
	struct hostent		 linuxHostEntry;
	char buff[4092]{};
	if ( !(hostEntry = gethostbyname_r ( host, linuxHostEntry, buff, sizeof ( buff ), &h_errnop )) )
	{
		return (0);
	}
#endif

	addr = (unsigned char *) (hostEntry->h_addr_list[0]);

	val = (addr[0] << 24) + (addr[1] << 16) + (addr[2] << 8) + addr[3];
	return (val);

}

unsigned char hexToChar ( char const *src )

{
	unsigned char	 val;
	unsigned char	 chr;

	chr = *(src++);
	if ( chr )
	{
		val = (int) (strchr ( hexValues, chr ) - hexValues) << 4;

		chr = *(src++);
		if ( chr )
		{
			if ( chr >= 'a' && chr <= 'z' )
			{
				chr = chr - 'a' + 'A';
			}

			val |= (strchr ( hexValues, chr ) - hexValues);

			return (val);
		}
	}
	return (0);
}


int checkScript ( char const *value, size_t len )

{
	int		 state = 0;
	char const *tagLwr = "<script>";
	char const *tagUpr = "<SCRIPT>";

	if ( !len ) len = strlen ( value );

	char const *endPtr = value + len;

	while ( value < endPtr )
	{
		switch ( state )
		{
			case 0:
				if ( *value == '<' )
				{
					state = 1;
				} else	if ( (*value == '%') && (*(value + 1) == '3') && ((*(value + 2) == 'c') || (*(value + 2) == 'C')) )
				{
					value += 2;
					state = 1;
				}
				break;
			case 1:
				if ( (*value == 's') || (*value == 'S') )
				{
					state = 2;
				} else if ( (*value == '%') && (*(value + 1) == '5') && (*(value + 2) == '3') )
				{
					state = 2;
					value += 2;
				} else if ( (*value == '%') && (*(value + 1) == '7') && (*(value + 2) == '3') )
				{
					value += 2;
					state = 2;
				} else if ( ((*value == '%') && *(value + 1) && *(value + 2)) && (
					(hexToChar ( value + 1 ) == ' ') || (hexToChar ( value + 1 ) == '\t') ||
					(hexToChar ( value + 1 ) == '\r') || (hexToChar ( value + 1 ) == '\n'))
					)
				{
					value += 2;
				} else if ( !_isspacex ( value ) && (*value != '+') )
				{
					state = 0;
				}
				break;
			case 2: //c
			case 3: //r
			case 4: //i
			case 5: //p
			case 6: //t
				if ( *value == '%' && *(value + 1) && *(value + 2) )
				{
					if ( (hexToChar ( value + 1 ) == (uint8_t)tagLwr[state]) || (hexToChar ( value + 1 ) == (uint8_t)tagUpr[state]) )
					{
						state++;
						value += 2;
					}
				} else if ( (*value == tagLwr[state]) || (*value == tagUpr[state]) )
				{
					state++;
				} else
				{
					state = 0;
				}
				break;
			case 7:
				if	(	( *value == '>' ) ||
						( (*value == '%') && (*(value + 1) == '3') && ((*(value + 2) == 'e') || (*(value + 2) == 'E')) )
					)
				{
					return (1);
				} else if ( (*value == '%' && *(value + 1) && *(value + 2)) && (
					(hexToChar ( value + 1 ) == ' ') || (hexToChar ( value + 1 ) == '\t') ||
					(hexToChar ( value + 1 ) == '\r') || (hexToChar ( value + 1 ) == '\n'))
					)
				{
					value += 2;
				} else if ( !_isspacex ( value ) && (*value != '+') )
				{
					state = 0;
				}
				break;
		}
		value++;
	}

	return (0);
}


uint64_t _hashKey ( char const *ptr )
{
	uint64_t	hash;
	int			chr;

	chr = (int)(unsigned char)*(ptr++);
	if ( (chr >= 'a') && (chr <= 'z') )
	{
		chr = chr - 'a' + 'A';
	}

	hash = chr;

	while ( chr )
	{
		hash += hash * 53 + (unsigned char)chr;

		chr = (unsigned char)*(ptr++);
		if ( (chr >= 'a') && (chr <= 'z') )
		{
			chr = chr - 'a' + 'A';
		}
	}
	return hash;
}

std::string base64Encode ( std::string const &inp )
{
	size_t		 loop;
	size_t		 outLen;
	uint8_t		*in;
	uint8_t		*out;

	outLen = ((inp.size ( ) - 1) / 3 + 1) * 4; /* round len to greatest 3rd and multiply by 4 */

	uint8_t *rsp = new uint8_t[outLen];
	out = rsp;

	in = (uint8_t *) inp.c_str ( );

	for ( loop = 0; loop < inp.size ( ); loop += 3 )
	{
		uint8_t in2[3]{ in[0], loop + 1 < inp.size () ? in[1] : (uint8_t)0,  loop + 2 < inp.size () ? in[2] : (uint8_t)0 };

		out[0] = webBase64EncodingTable[(in2[0] >> 2)];
		out[1] = webBase64EncodingTable[((in2[0] & 0x03) << 4) | (in2[1] >> 4)];
		out[2] = webBase64EncodingTable[((in2[1] & 0x0F) << 2) | (in2[2] >> 6)];
		out[3] = webBase64EncodingTable[((in2[2] & 0x3F))];
		/* increment our pointers appropriately */
		out += 4;
		in += 3;
	}

	/* fill in "null" character... all 0's in important bits */
	switch ( inp.size ( ) % 3 )
	{
		case 1:
			out[-2] = '=';
			// fall through 
		case 2:
			out[-1] = '=';
			break;
		case 0:
		default:
			break;
	}

	std::string res ( (char *) rsp, outLen );
	delete[] rsp;

	return res;
}

std::string base64Decode ( std::string const &p1 )
{
	size_t		 decodedLen;
	size_t		 loop;
	uint8_t		*in;
	uint8_t		*out, *outP;

	in = (uint8_t *) p1.c_str ( );

	decodedLen = (p1.size ( ) + 3) / 4 * 3;

	if ( in[p1.size ( ) - 1] == '=' )
	{
		decodedLen--;
	}
	if ( in[p1.size ( ) - 2] == '=' )
	{
		decodedLen--;
	}

	out = outP = new uint8_t[decodedLen + 2 + 1];

	for ( loop = 0; loop < p1.size ( ); loop += 4 )
	{
		uint8_t in2[3]{ in[0], loop + 1 < p1.size () ? in[1] : (uint8_t)0, loop + 2 < p1.size () ? in[2] : (uint8_t)0 };

		out[0] = webBase64EncodingTable[(in2[0] >> 2)];
		out[1] = webBase64EncodingTable[((in2[0] & 0x03) << 4) | (in2[1] >> 4)];
		out[2] = webBase64EncodingTable[((in2[1] & 0x0F) << 2) | (in2[2] >> 6)];
		out[3] = webBase64EncodingTable[((in2[2] & 0x3F))];
		/* increment our pointers appropriately */
		out += 4;
		in += 3;
	}

	std::string rsp ( (char *) outP, decodedLen );
	delete[] outP;
	return rsp;
}

bool isSameFile ( char const* file1, char const* file2 )
{
	std::filesystem::path p1 = file1;
	std::filesystem::path p2 = file2;
	std::error_code ec;
	return std::filesystem::equivalent ( p1, p2, ec );
}
