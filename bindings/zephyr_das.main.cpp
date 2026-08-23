#include "daScript/misc/platform.h"
#include "daScript/ast/ast.h"
#include "daScript/ast/ast_interop.h"
#include "daScript/ast/ast_handle.h"
#include "daScript/ast/ast_typefactory_bind.h"
#include "daScript/simulate/bind_enum.h"
#include "zephyr_das.h"
#include "need_zephyr_das.h"

namespace das {

void Module_zephyr_das::initMain() {
#include "zephyr_das.const.inc"
}

ModuleAotType Module_zephyr_das::aotRequire(TextWriter & tw) const {
	tw << "#include \"zephyr_api.h\"\n";
	tw << "#include \"daScript/simulate/bind_enum.h\"\n";
	tw << "#include \"zephyr_das.enum.decl.cast.inc\"\n";
	return ModuleAotType::cpp;
}

}
