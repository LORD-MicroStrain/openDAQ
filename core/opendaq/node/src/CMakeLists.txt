set(BASE_NAME node)

function(rtgen_component_${BASE_NAME})

endfunction()

function(create_component_source_groups_${BASE_NAME})
    set(SDK_HEADERS_DIR "node/include/opendaq")
    set(SDK_SRC_DIR "node/src")
    
    source_group("node//sample_type" FILES 
        ${SDK_HEADERS_DIR}/sample_type.h
    )
    
    source_group("node//node" FILES 
        #${SDK_HEADERS_DIR}/ids_parser.h
        ${SDK_HEADERS_DIR}/node.h #added
        #${SDK_SRC_DIR}/ids_parser.cpp
        ${SDK_SRC_DIR}/node.cpp #added
    )
    
    source_group("node" FILES 
        #${SDK_HEADERS_DIR}/utility_errors.h
        #${SDK_HEADERS_DIR}/utility_exceptions.h
        #${SDK_SRC_DIR}/utility.natvis
        ${SDK_SRC_DIR}/node.natvis #added

    )
endfunction()

set(SRC_Cpp_Component 
    #ids_parser.cpp
    node.cpp #added
    #utility.natvis
    node.natvis #added
    PARENT_SCOPE
)

set(SRC_PublicHeaders_Component 
    #utility_errors.h
    #utility_exceptions.h
    #ids_parser.h
    node.h #added
    PARENT_SCOPE
)

set(SRC_PrivateLinkLibraries_Component
    $<BUILD_INTERFACE:Boost::algorithm>
    PARENT_SCOPE
)

# No bindings are generated in utility. Workaround for install.
get_filename_component(CURR_BINDINGS_DIR
                       "${CMAKE_CURRENT_BINARY_DIR}/../bindings"
                       ABSOLUTE)
opendaq_create_dir(${CURR_BINDINGS_DIR})
