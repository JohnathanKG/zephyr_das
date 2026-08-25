options gen2

module zephyr_hooks public

require daslib/ast
require daslib/ast_boost
require strings

// Named [on_input] because Zephyr's struct input_callback is already a bound type annotation.

[function_macro(name="on_input")]
class InputCallbackMacro : AstFunctionAnnotation {
    def override apply(var func : FunctionPtr; var group : ModuleGroup;
                       args : AnnotationArgumentList; var errors : das_string) : bool {
        func.flags |= FunctionFlags.exports
        return true
    }

    def override finish(var func : FunctionPtr; var group : ModuleGroup;
                        args, progArgs : AnnotationArgumentList; var errors : das_string) : bool {
        if (length(func.arguments) != 2 || func.result == null || func.result.baseType != Type.tVoid) {
            errors := "[on_input] expects def (evt : input_event; arg : void?)"
            return false
        }
        let t0 = describe(func.arguments[0]._type, false, false, false)
        let t1 = func.arguments[1]._type
        if (find(t0, "input_event") < 0 || t1 == null || t1.baseType != Type.tPointer) {
            errors := "[on_input] expects def (evt : input_event; arg : void?)"
            return false
        }
        return true
    }
}
