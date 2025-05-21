file(GLOB sources "*.cpp")
file(GLOB headers "*.h")

add_library(devcore ${sources} ${headers})

find_package(Boost REQUIRED)
find_package(JsonCpp REQUIRED)

target_link_libraries(devcore 
    PUBLIC 
        boost::boost
        JsonCpp::JsonCpp 
        ethash
        spdlogger
)
target_include_directories(devcore 
    SYSTEM 
    PUBLIC 
        ../ 
        ${Boost_INCLUDE_DIRS}
        ${JsonCpp_INCLUDE_DIRS}
)
