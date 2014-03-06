rock_library(distributed_locking
    SOURCES DLM.cpp RicartAgrawala.cpp
    HEADERS Agent.hpp DLM.hpp RicartAgrawala.hpp
    DEPS_PKGCONFIG base-types fipa_acl)

