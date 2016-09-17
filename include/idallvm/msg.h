
#ifndef _IDALLVM_MSG_H
#define _IDALLVM_MSG_H

#include <kernwin.hpp>

#include <llvm/Support/raw_ostream.h>

#include "idallvm/plugin.h"

#define MSG_INFO(text, ...) msg(PLUGIN_NAME " info: " text "\n", ##__VA_ARGS__)
#define MSG_WARN(text, ...) msg(PLUGIN_NAME " warning: " text "\n", ##__VA_ARGS__)
#define MSG_ERROR(text, ...) msg(PLUGIN_NAME " error: " text "\n", ##__VA_ARGS__)

llvm::raw_ostream& outs(void);

#endif /* _IDALLVM_MSG_H */
