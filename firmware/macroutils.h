#ifndef MACROUTILS_H
#define MACROUTILS_H

#define MACROUTILS_CONCAT_(x, y) x ## y
#define MACROUTILS_CONCAT(x, y)  MACROUTILS_CONCAT_(x, y)

#define MACROUTILS_CONCAT3_(x, y, z) x ## y ## z
#define MACROUTILS_CONCAT3(x, y, z)  MACROUTILS_CONCAT3_(x, y, z)

#endif