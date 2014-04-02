find_package(Boost 1.48 COMPONENTS serialization system REQUIRED)

rock_library(distributed_locking
    SOURCES DLM.cpp RicartAgrawala.cpp RicartAgrawalaExtended.cpp SuzukiKasami.cpp SuzukiKasamiExtended.cpp
    HEADERS Agent.hpp DLM.hpp RicartAgrawala.hpp RicartAgrawalaExtended.hpp SuzukiKasami.hpp SuzukiKasamiExtended.hpp
    DEPS_PKGCONFIG base-types fipa_acl
    LIBS ${Boost_SERIALIZATION_LIBRARY} ${Boost_SYSTEM_LIBRARY}
    )