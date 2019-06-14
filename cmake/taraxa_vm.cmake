set(taraxa_vm_GO_MODULE )
set(taraxa_vm_CGO_LIB_DEST ${CMAKE_CURRENT_BINARY_DIR}/taraxa_evm)
set(taraxa_vm_CGO_LIB_FILE ${taraxa_vm_CGO_LIB_DEST}/taraxa_evm_cgo/taraxa_evm_cgo.a)
add_library(taraxa_vm_cgo STATIC IMPORTED)
add_custom_target(
        taraxa_vm_cgo_build
        COMMAND go build -tags=lib_cpp -buildmode=c-archive -o ${taraxa_vm_CGO_LIB_FILE}
        COMMENT "Build CGO static lib from ${taraxa_vm_GO_MODULE} to ${taraxa_vm_CGO_LIB_FILE}"
)
set_target_properties(${PROJECT_NAME}_cgo_static_lib PROPERTIES IMPORTED_LOCATION ${CGO_LIB_FILE})
add_dependencies(taraxa_vm_cgo taraxa_vm_cgo)
