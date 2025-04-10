IS_MACOS=$(uname | grep -q Darwin && echo true || echo false)

conan profile show --profile:host=clang --profile:build=clang > /dev/null 2>&1
# go inside if on error
if [ $? -ne 0 ]; then
    conan profile detect --name clang
    PROFILE_PATH=$(conan profile path clang)
    echo "PROFILE_PATH: $PROFILE_PATH"

    if [ "$IS_MACOS" = true ]; then
        # macOS
        SED_IN_PLACE="-i ''"
    else
        # Linux/Ubuntu  
        SED_IN_PLACE="-i"
    fi

    sed "${SED_IN_PLACE}" "s|cppstd=.*|cppstd=20|" "$PROFILE_PATH"
    sed "${SED_IN_PLACE}" "s|compiler=.*|compiler=clang|" "$PROFILE_PATH"
    sed "${SED_IN_PLACE}" "s|compiler.version=.*|compiler.version=18|" "$PROFILE_PATH"
else 
    PROFILE_PATH=$(conan profile path clang)
fi

cat $PROFILE_PATH

if [ -z "$CMAKE_BUILD_TYPE" ]; then
    echo 'CMAKE_BUILD_TYPE is not specified. Defaulting to "Release"'
    export CMAKE_BUILD_TYPE=Release
fi
sed -i "s/build_type=.*/build_type=${CMAKE_BUILD_TYPE}/" "$PROFILE_PATH"
echo "CMAKE_BUILD_TYPE: $CMAKE_BUILD_TYPE"

# stop on ctrl+c
trap "exit 1" INT

if [ -z "$LLVM_VERSION" ]; then
    echo "LLVM_VERSION is not specified. Defaulting to 18"
    export LLVM_VERSION=18
fi

if [ "$(uname)" == "Darwin" ]; then
    export CC=/opt/homebrew/opt/llvm@${LLVM_VERSION}/bin/clang
    export CXX=/opt/homebrew/opt/llvm@${LLVM_VERSION}/bin/clang++
else
    export CC=clang-${LLVM_VERSION}
    export CXX=clang++-${LLVM_VERSION}
fi