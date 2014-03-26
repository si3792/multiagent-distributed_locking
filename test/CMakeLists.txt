find_package(Boost COMPONENTS system thread REQUIRED)

rock_testsuite(test_suite suite.cpp
  test_RicartAgrawala.cpp test_SuzukiKasami.cpp TestHelper.cpp
  DEPS distributed_locking
  LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY}
  )
