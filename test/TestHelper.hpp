#ifndef DISTRIBUTED_LOCKING_TEST_HELPER_HPP
#define DISTRIBUTED_LOCKING_TEST_HELPER_HPP

#include <distributed_locking/DLM.hpp>

/**
 * Get the protocol path
 */
std::string getProtocolPath();

/**
 * Forwards all messages that currently await. FIXME to a new file
 */
void forwardAllMessages(std::list<fipa::distributed_locking::DLM::Ptr> dlms);

#endif // DISTRIBUTED_LOCKING_TEST_HELPER_HPP
