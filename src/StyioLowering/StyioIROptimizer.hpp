#pragma once
#ifndef STYIO_IR_OPTIMIZER_H_
#define STYIO_IR_OPTIMIZER_H_

#include "../StyioIR/StyioIR.hpp"

namespace styio::lowering {

StyioIR*
optimize_styio_ir(StyioIR* root);

}  // namespace styio::lowering

#endif  // STYIO_IR_OPTIMIZER_H_
