find_package(Boost 1.48 COMPONENTS system thread REQUIRED)

rock_testsuite(test_suite suite.cpp
  test_RicartAgrawala.cpp test_RicartAgrawalaExtended.cpp test_SuzukiKasami.cpp test_SuzukiKasamiExtended.cpp TestHelper.cpp
  DEPS distributed_locking
  LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY}
  )
