#ifndef DISTRIBUTED_LOCKING_TEST_HELPER_HPP
#define DISTRIBUTED_LOCKING_TEST_HELPER_HPP

#include <distributed_locking/DLM.hpp>

/**
 * Forwards all messages that currently await. FIXME to a new file
 */
void forwardAllMessages(std::list<fipa::distributed_locking::DLM*> dlms);

#endif // DISTRIBUTED_LOCKING_TEST_HELPER_HPP