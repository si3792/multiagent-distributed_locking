rock_library(ricart_agrawala
    SOURCES Dummy.cpp
    HEADERS Dummy.hpp)

rock_executable(ricart_agrawala_bin Main.cpp
    DEPS ricart_agrawala)

