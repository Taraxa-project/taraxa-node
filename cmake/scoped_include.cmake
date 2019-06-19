include_guard()

# A helper that provides a nested scope to variables inside included files
function(scoped_include module)
    include(${module})
endfunction()