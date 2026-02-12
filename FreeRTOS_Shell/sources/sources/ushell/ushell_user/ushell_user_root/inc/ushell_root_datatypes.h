#ifndef USHELL_ROOT_DATATYPES_H
#define USHELL_ROOT_DATATYPES_H

#include "ushell_core_settings.h"

#define uSHELL_COMMANDS_CONFIG_FILE              "ushell_root_commands.cfg"
#if (1 == uSHELL_IMPLEMENTS_USER_SHORTCUTS)
#define uSHELL_USER_SHORTCUTS_CONFIG_FILE        "ushell_root_shortcuts.cfg"
#endif /*(1 == uSHELL_IMPLEMENTS_USER_SHORTCUTS)*/

#include "ushell_core_datatypes_user.h"

#endif /* USHELL_ROOT_DATATYPES_H */