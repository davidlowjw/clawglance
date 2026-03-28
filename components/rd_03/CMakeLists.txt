idf_component_register(
                    INCLUDE_DIRS "include"
                    REQUIRES 
                    driver
                    )
target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/librd_03.a")