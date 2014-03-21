find_package(Boost COMPONENTS system REQUIRED)

rock_testsuite(test_suite suite.cpp
  test_RicartAgrawala.cpp test_SuzukiKasami.cpp test_DLM.cpp TestHelper.cpp
  DEPS distributed_locking
  LIBS ${Boost_SYSTEM_LIBRARY})
