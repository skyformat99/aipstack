/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_MACRO_MAP_H
#define AIPSTACK_MACRO_MAP_H

#include <aipstack/misc/Preprocessor.h>

#define AIPSTACK_AS_NUM_MACRO_ARGS(...) AIPSTACK_EXPAND(AIPSTACK_AS_NUM_MACRO_ARGS_HELPER1(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define AIPSTACK_AS_NUM_MACRO_ARGS_HELPER1(...) AIPSTACK_EXPAND(AIPSTACK_AS_NUM_MACRO_ARGS_HELPER2(__VA_ARGS__))
#define AIPSTACK_AS_NUM_MACRO_ARGS_HELPER2(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N

#define AIPSTACK_NUM_TUPLE_ARGS(tuple) AIPSTACK_EXPAND(AIPSTACK_AS_NUM_MACRO_ARGS tuple)

#define  AIPSTACK_AS_GET_1(p1, ...) p1
#define  AIPSTACK_AS_GET_2(p1, p2, ...) p2
#define  AIPSTACK_AS_GET_3(p1, p2, p3, ...) p3
#define  AIPSTACK_AS_GET_4(p1, p2, p3, p4, ...) p4
#define  AIPSTACK_AS_GET_5(p1, p2, p3, p4, p5, ...) p5
#define  AIPSTACK_AS_GET_6(p1, p2, p3, p4, p5, p6, ...) p6
#define  AIPSTACK_AS_GET_7(p1, p2, p3, p4, p5, p6, p7, ...) p7
#define  AIPSTACK_AS_GET_8(p1, p2, p3, p4, p5, p6, p7, p8, ...) p8
#define  AIPSTACK_AS_GET_9(p1, p2, p3, p4, p5, p6, p7, p8, p9, ...) p9
#define AIPSTACK_AS_GET_10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, ...) p10
#define AIPSTACK_AS_GET_11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, ...) p11
#define AIPSTACK_AS_GET_12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, ...) p12
#define AIPSTACK_AS_GET_13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, ...) p13
#define AIPSTACK_AS_GET_14(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, ...) p14
#define AIPSTACK_AS_GET_15(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, ...) p15
#define AIPSTACK_AS_GET_16(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, ...) p16
#define AIPSTACK_AS_GET_17(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, ...) p17
#define AIPSTACK_AS_GET_18(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, ...) p18
#define AIPSTACK_AS_GET_19(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, ...) p19
#define AIPSTACK_AS_GET_20(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, ...) p20
#define AIPSTACK_AS_GET_21(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, ...) p21
#define AIPSTACK_AS_GET_22(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, ...) p22

#define  AIPSTACK_AS_MAP_1(f, del, arg, pars)                                                  f(arg,  AIPSTACK_AS_GET_1 pars)
#define  AIPSTACK_AS_MAP_2(f, del, arg, pars)  AIPSTACK_AS_MAP_1(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_2 pars)
#define  AIPSTACK_AS_MAP_3(f, del, arg, pars)  AIPSTACK_AS_MAP_2(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_3 pars)
#define  AIPSTACK_AS_MAP_4(f, del, arg, pars)  AIPSTACK_AS_MAP_3(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_4 pars)
#define  AIPSTACK_AS_MAP_5(f, del, arg, pars)  AIPSTACK_AS_MAP_4(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_5 pars)
#define  AIPSTACK_AS_MAP_6(f, del, arg, pars)  AIPSTACK_AS_MAP_5(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_6 pars)
#define  AIPSTACK_AS_MAP_7(f, del, arg, pars)  AIPSTACK_AS_MAP_6(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_7 pars)
#define  AIPSTACK_AS_MAP_8(f, del, arg, pars)  AIPSTACK_AS_MAP_7(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_8 pars)
#define  AIPSTACK_AS_MAP_9(f, del, arg, pars)  AIPSTACK_AS_MAP_8(f, del, arg, pars) del(dummy) f(arg,  AIPSTACK_AS_GET_9 pars)
#define AIPSTACK_AS_MAP_10(f, del, arg, pars)  AIPSTACK_AS_MAP_9(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_10 pars)
#define AIPSTACK_AS_MAP_11(f, del, arg, pars) AIPSTACK_AS_MAP_10(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_11 pars)
#define AIPSTACK_AS_MAP_12(f, del, arg, pars) AIPSTACK_AS_MAP_11(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_12 pars)
#define AIPSTACK_AS_MAP_13(f, del, arg, pars) AIPSTACK_AS_MAP_12(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_13 pars)
#define AIPSTACK_AS_MAP_14(f, del, arg, pars) AIPSTACK_AS_MAP_13(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_14 pars)
#define AIPSTACK_AS_MAP_15(f, del, arg, pars) AIPSTACK_AS_MAP_14(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_15 pars)
#define AIPSTACK_AS_MAP_16(f, del, arg, pars) AIPSTACK_AS_MAP_15(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_16 pars)
#define AIPSTACK_AS_MAP_17(f, del, arg, pars) AIPSTACK_AS_MAP_16(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_17 pars)
#define AIPSTACK_AS_MAP_18(f, del, arg, pars) AIPSTACK_AS_MAP_17(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_18 pars)
#define AIPSTACK_AS_MAP_19(f, del, arg, pars) AIPSTACK_AS_MAP_18(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_19 pars)
#define AIPSTACK_AS_MAP_20(f, del, arg, pars) AIPSTACK_AS_MAP_19(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_20 pars)
#define AIPSTACK_AS_MAP_21(f, del, arg, pars) AIPSTACK_AS_MAP_20(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_21 pars)
#define AIPSTACK_AS_MAP_22(f, del, arg, pars) AIPSTACK_AS_MAP_21(f, del, arg, pars) del(dummy) f(arg, AIPSTACK_AS_GET_22 pars)

// This injects a dummy parameter to the end of pars in order to not call AIPSTACK_AS_GET_n
// macros without any variadic argument, which is not allowed before C++20.
#define AIPSTACK_AS_ADD_DUMMY_TO_PARS(...) (__VA_ARGS__, aipstack_as_dummy_end)

#define AIPSTACK_AS_MAP(f, del, arg, pars) AIPSTACK_JOIN(AIPSTACK_AS_MAP_, AIPSTACK_NUM_TUPLE_ARGS(pars))(f, del, arg, AIPSTACK_AS_ADD_DUMMY_TO_PARS pars)

#define AIPSTACK_AS_MAP_DELIMITER_NONE(dummy)
#define AIPSTACK_AS_MAP_DELIMITER_COMMA(dummy) ,

#endif
