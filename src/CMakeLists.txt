set(EM_CONFIG_PARAM "-Oz -s LEGACY_VM_SUPPORT=1 -s TOTAL_MEMORY=67108864 -s MEM_INIT_METHOD=0 -s ENVIRONMENT=\"web,worker\" -s SINGLE_FILE=1 -s WASM=0 -s FETCH=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ELIMINATE_DUPLICATE_FUNCTIONS=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s NO_EXIT_RUNTIME=1 -s NO_FILESYSTEM=1 -s FILESYSTEM=0")
set(EM_OPTIMIZE_PARAM "--closure 1 --memory-init-file 0 --llvm-lto 2 -lwebsocket.js")
set(EM_EXPORT_METHOD "-s EXTRA_EXPORTED_RUNTIME_METHODS='[\"ccall\", \"cwrap\"]' -s EXPORTED_FUNCTIONS='[\"_security_worker_onmessage\", \"_security_worker_new\", \"_security_worker_exit\"]'")

add_library(ext console.cpp timer.cpp helper.cpp error.cpp request.cpp websocket.cpp self.cpp)
add_executable(core core.cpp map.hpp string.hpp)
set_target_properties(core PROPERTIES LINK_FLAGS "${EM_CONFIG_PARAM} ${EM_OPTIMIZE_PARAM} ${EM_EXPORT_METHOD}")
target_link_libraries(core ext jerry-core jerry-port-default b64 aes)

add_executable(loader loader.c)
target_link_libraries(loader gzip aes)