#find_package(Boost COMPONENTS serialization REQUIRED)
#find_package(Boost 1.55 REQUIRED system serialization)

rock_library(distributed_locking
    SOURCES DLM.cpp RicartAgrawala.cpp SuzukiKasami.cpp
    HEADERS Agent.hpp DLM.hpp RicartAgrawala.hpp SuzukiKasami.hpp
    DEPS_PKGCONFIG base-types fipa_acl
    LIBS boost_serialization#${BOOST_SERIALIZATION_LIBRARY}
    )
    #target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
    #SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lboost_serialization" )

