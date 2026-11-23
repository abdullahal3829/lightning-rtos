/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_STDLIB           LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN
#define LV_USE_MEM_POOL_BUILTIN 1
#define LV_MEM_SIZE             (128U * 1024U)
#define LV_MEM_ADR              0x00200000U
#define LV_USE_OS               LV_OS_NONE
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_XRGB8888
#define LV_USE_LOG              0
#define LV_USE_THEME_DEFAULT    0
#define LV_USE_THEME_SIMPLE     0
#define LV_USE_THEME_MONO       0
#define LV_USE_FLEX             1
#define LV_USE_GRID             0
#define LV_USE_ARC              0
#define LV_USE_ARCLABEL         0
#define LV_USE_BAR              0
#define LV_USE_BUTTON           1
#define LV_USE_BUTTONMATRIX     1
#define LV_USE_CALENDAR         0
#define LV_USE_CANVAS           0
#define LV_USE_CHART            0
#define LV_USE_CHECKBOX         0
#define LV_USE_DROPDOWN         0
#define LV_USE_IMAGE            1
#define LV_USE_IMAGEBUTTON      1
#define LV_USE_KEYBOARD         1
#define LV_USE_LABEL            1
#define LV_USE_LED              0
#define LV_USE_LINE             0
#define LV_USE_LIST             0
#define LV_USE_MENU             1
#define LV_USE_MSGBOX           0
#define LV_USE_ROLLER           0
#define LV_USE_SCALE            0
#define LV_USE_SLIDER           0
#define LV_USE_SPAN             0
#define LV_USE_SPINBOX          0
#define LV_USE_SPINNER          0
#define LV_USE_SWITCH           0
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_DEFAULT         (&lv_font_montserrat_14)

#endif /* LV_CONF_H */