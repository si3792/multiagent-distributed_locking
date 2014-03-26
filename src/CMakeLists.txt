find_package(Boost COMPONENTS serialization system REQUIRED)

rock_library(distributed_locking
    SOURCES DLM.cpp RicartAgrawala.cpp RicartAgrawalaExtended.cpp SuzukiKasami.cpp
    HEADERS Agent.hpp DLM.hpp RicartAgrawala.hpp RicartAgrawalaExtended.hpp SuzukiKasami.hpp
    DEPS_PKGCONFIG base-types fipa_acl
    LIBS ${Boost_SERIALIZATION_LIBRARY} ${Boost_SYSTEM_LIBRARY}
    )