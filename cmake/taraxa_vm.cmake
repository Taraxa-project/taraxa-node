set(taraxa_vm_GO_MODULE ${PROJECT_SOURCE_DIR}/submodules/taraxa-evm/main)
set(taraxa_vm_CGO_LIB_FILE ${PROJECT_SOURCE_DIR}/vm/cgo_generated/taraxa_vm_cgo.a)
add_library(taraxa_vm_cgo STATIC IMPORTED)
add_custom_target(
        taraxa_vm_cgo_build
        COMMAND go build -tags=secp256k1_no_cgo -buildmode=c-archive -o ${taraxa_vm_CGO_LIB_FILE}
        WORKING_DIRECTORY ${taraxa_vm_GO_MODULE}
        COMMENT "Build CGO static lib from ${taraxa_vm_GO_MODULE} to ${taraxa_vm_CGO_LIB_FILE}"
)
set_target_properties(taraxa_vm_cgo PROPERTIES IMPORTED_LOCATION ${taraxa_vm_CGO_LIB_FILE})
add_dependencies(taraxa_vm_cgo taraxa_vm_cgo_build)
