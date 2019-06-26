include_guard()
include(${PROJECT_SOURCE_DIR}/cmake/external_lib.cmake)

set(taraxa_vm_GO_MODULE ${PROJECT_SOURCE_DIR}/submodules/taraxa-evm/main)
set(taraxa_vm_CGO_LIB_FILE ${PROJECT_SOURCE_DIR}/vm/cgo_generated/taraxa_vm_cgo.a)
external_lib(taraxa_vm_cgo STATIC ${taraxa_vm_CGO_LIB_FILE}
        COMMAND
        test ! -e ${taraxa_vm_CGO_LIB_FILE}
        && go build -tags=secp256k1_no_cgo -buildmode=c-archive -o ${taraxa_vm_CGO_LIB_FILE}
        || true
        WORKING_DIRECTORY ${taraxa_vm_GO_MODULE}
        COMMENT "Building CGO static lib from ${taraxa_vm_GO_MODULE} to ${taraxa_vm_CGO_LIB_FILE}... ")