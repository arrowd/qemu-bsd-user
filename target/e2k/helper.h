DEF_HELPER_2(raise_exception, noreturn, env, int)
DEF_HELPER_3(call, void, env, i64, i64)
DEF_HELPER_2(sxt, i64, i64, i64)
DEF_HELPER_1(debug_i32, void, i32)
DEF_HELPER_1(debug_i64, void, i64)
