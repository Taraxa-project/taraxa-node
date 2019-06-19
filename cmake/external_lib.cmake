include_guard()

macro(external_lib name type file)
    add_library(${name} ${type} IMPORTED)
    add_custom_target(${name}_build ${ARGN})
    set_target_properties(${name} PROPERTIES IMPORTED_LOCATION ${file})
    add_dependencies(${name} ${name}_build)
endmacro()