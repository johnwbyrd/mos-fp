#pragma once

// C API prefix configuration
// Change OPINE_C_PREFIX to rebrand (e.g., __llvm_, __mylib_, etc.)

#ifndef OPINE_C_PREFIX
#define OPINE_C_PREFIX __opine_
#endif

// Token pasting
#define OPINE_CONCAT_IMPL(a, b) a##b
#define OPINE_CONCAT(a, b) OPINE_CONCAT_IMPL(a, b)
#define OPINE_C_NAME(name) OPINE_CONCAT(OPINE_C_PREFIX, name)
